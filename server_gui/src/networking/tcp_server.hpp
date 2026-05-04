#include <iostream>
#include <exception>

#include <asio.hpp>
#include <unordered_map>

#include "tcp_connection.hpp"
#include "tcp_common.hpp"

#include "registry.hpp"

namespace ip = asio::ip;

class tcp_server_t {
public:
    tcp_server_t() : m_acceptor(m_io_context) {}

    void start_listening(const ip::tcp& protocol, uint16_t port) {
        try {
            m_acceptor.open(protocol);
            m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
            m_acceptor.bind(ip::tcp::endpoint(protocol, port));
            m_acceptor.listen();
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }

        start_accept();

        m_io_context.run();
    }

    void stop_listening() {
        try {
            m_acceptor.close();
        }
        catch(const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }

    template<typename T>
    void send_to_client(esp_id_t client_id, const T& data) {
        std::cout << "Sending to client: " << client_id << "\n";

        if (m_connections.contains(client_id)) {
            m_registry.send<T>([&](payload_t&& buffer) {
                m_connections.at(client_id)->send(std::move(buffer));
            });
        }
        else {
            std::cerr << "Failed to send to client as client: " << client_id << " was not found!\n";
        }
    }

    void toggle_read_from_client(bool enable) {
        //LOG_INFO("Listening to clients has {}.", enable ? "started" : "stopped");

        // tell every client that the server doesn't/does want to receive 
        for (auto [_, connection] : m_connections) {
            connection->can_receive_from(enable);
        }
    }

    void disconnect_client(esp_id_t client_id) {
        if (m_connections.contains(client_id)) {
            m_connections.at(client_id)->close_connection();
            m_connections.erase(client_id);
        }
        else {
            //LOG_ERROR("Failed to disconnect client as client {} was not found!", client_id);
        }
    }

    void shutdown() {
        stop_listening();
        m_io_context.stop();

        std::cout << "Shutting down\n";
    }

private:
    void start_accept() {
        tcp_connection_ptr_t new_connection = tcp_connection_t::create(m_io_context);

        std::cout << "Listening for new clients connections...\n";

        m_acceptor.async_accept(
            new_connection->get_socket(), 
            std::bind(
                &tcp_server_t::handle_accept, 
                this, 
                new_connection,
                asio::placeholders::error
            )
        );
    }

    void handle_accept(tcp_connection_ptr_t new_connection, const std::error_code& error) {
        if (error) {
            std::cout << "Accepting incoming client failed: " << error.message() << "\n";
            return; 
        }

        // setup new valid connection
        new_connection->set_connection_id(m_connection_count);
        new_connection->can_receive_from(true);
        m_connections.emplace(m_connection_count, new_connection);

        // move and check for other connections
        m_connection_count++;
        start_accept();
    }

private:
    asio::io_context m_io_context;
    ip::tcp::acceptor m_acceptor;

    registry_t m_registry;

    std::unordered_map<esp_id_t, tcp_connection_ptr_t> m_connections;
    esp_id_t m_connection_count = 0;
};