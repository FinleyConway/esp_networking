#include <iostream> // remove later

#include <mutex>
#include <atomic>
#include <thread>
#include <utility>
#include <system_error>
#include <unordered_map>

#include <asio.hpp>

#include "tcp_connection.hpp"
#include "tcp_macros.hpp"
#include "tcp_common.hpp"
#include "registry.hpp"

namespace ip = asio::ip;

class tcp_server_t {
public:
    tcp_server_t() : m_acceptor(m_io_context) {}

    void start() {
        m_io_thread = std::thread([this] {
            m_io_context.run();
        });
    }

    void shutdown() {
        toggle_accepting(false);

        m_io_context.stop();

        if (m_io_thread.joinable()) {
            m_io_thread.join();
        }

        std::cout << "Shutting down\n";
    }

    void configure(const ip::tcp& protocol, uint16_t port) {
        m_endpoint = ip::tcp::endpoint(protocol, port);
        m_configured = true;
    }

    void toggle_accepting(bool enable) {
        if (!m_configured) {
            std::cerr << "Server not configured\n";
            return;
        }

        std::error_code ec;

        if (enable) {
            // prevent accepting when its already open
            if (m_acceptor.is_open()) return;

            m_accepting = true;

            // try setup acceptor for esps to connect 
            EC_TRY_OR_RTN(m_acceptor.open(m_endpoint.protocol()), ec);
            EC_TRY_OR_RTN(m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true)), ec);
            EC_TRY_OR_RTN(m_acceptor.bind(ip::tcp::endpoint(m_endpoint)), ec);
            EC_TRY_OR_RTN(m_acceptor.listen(), ec);

            // start listening
            wait_for_connection();
        }
        else {
            m_accepting = false;
            m_acceptor.close();
        }
    }

    template<typename T>
    bool send_to_client(esp_id_t client_id, const T& data) {
        std::lock_guard lock(m_connection_mutex);

        auto it = m_connections.find(client_id);
        if (it == m_connections.end()) {
            std::cerr << "send_to_client: client " << client_id << " not found\n";
            return false;
        }

        std::cout << "Sending to client " << client_id << '\n';

        m_registry.send<T>([connection = it->second](common::payload_t&& buffer) {
            connection->send(std::move(buffer));
        });

        return true;
    }

    void receive_from_client(bool enable) {
        std::lock_guard lock(m_connection_mutex);

        //LOG_INFO("Listening to clients has {}.", enable ? "started" : "stopped");

        // tell every client that the server doesn't/does want to receive 
        for (auto& [_, connection] : m_connections) {
            connection->can_receive_from(enable);
        }
    }

    bool disconnect_client(esp_id_t client_id) {
        std::lock_guard lock(m_connection_mutex);

        auto it = m_connections.find(client_id);
        if (it == m_connections.end()) {
            // LOG_ERROR("Client {} not found", client_id);
            return false;
        }

        it->second->close_connection();
        m_connections.erase(it);

        return true;
    }

private:
    void wait_for_connection() {
        if (!m_accepting) return;

        tcp_connection_ptr_t new_connection = tcp_connection_t::create(m_io_context);

        m_acceptor.async_accept(
            new_connection->get_socket(), 
            std::bind(
                &tcp_server_t::create_new_connection, 
                this, 
                new_connection,
                asio::placeholders::error
            )
        );
    }

    void create_new_connection(tcp_connection_ptr_t new_connection, const std::error_code& error) {
        if (!m_accepting) return;

        if (error) {
            std::cout << "Accepting incoming client failed: " << error.message() << "\n";
            return; 
        }

        // setup new valid connection
        new_connection->set_connection_id(m_connection_count);
        m_connections.emplace(m_connection_count, new_connection);

        // move and check for other connections
        m_connection_count++;
        wait_for_connection();
    }

private:
    asio::io_context m_io_context;
    ip::tcp::acceptor m_acceptor;
    std::thread m_io_thread;

    ip::tcp::endpoint m_endpoint;
    bool m_configured = false;
    std::atomic<bool> m_accepting = false;

    std::unordered_map<esp_id_t, tcp_connection_ptr_t> m_connections;
    std::mutex m_connection_mutex;
    esp_id_t m_connection_count = 0;

    common::registry_t m_registry;
};