#include <mutex>
#include <atomic>
#include <thread>
#include <utility>
#include <exception>
#include <system_error>
#include <unordered_map>

#include <asio.hpp>

#include "tcp_connection.hpp"
#include "registry.hpp"

namespace ip = asio::ip;

namespace host {
    enum class tcp_status_t {
        success,
        already_accepting,
        fail_to_accept,
        unknown_client,
        no_client_connection
    };

    class tcp_server_t {
    public:
        tcp_server_t(const ip::tcp& protocol, uint16_t port) : 
            m_acceptor(m_io_context),
            m_endpoint(ip::tcp::endpoint(protocol, port))
        {
        }

        ~tcp_server_t() {
            shutdown();
        }

        void start() {
            m_io_thread = std::thread([this] {
                m_io_context.run();
            });
        }

        bool is_running() const {
            return m_io_thread.joinable();
        }

        void shutdown() {
            toggle_accepting(false);

            m_io_context.stop();

            if (m_io_thread.joinable()) {
                m_io_thread.join();
            }
        }

        tcp_status_t toggle_accepting(bool enable) {
            if (enable) {
                // prevent accepting when its already open
                if (m_acceptor.is_open()) return tcp_status_t::already_accepting;

                // try setup acceptor for esps to connect 
                try {
                    m_acceptor.open(m_endpoint.protocol());
                    m_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
                    m_acceptor.bind(ip::tcp::endpoint(m_endpoint));
                    m_acceptor.listen();
                }
                catch(const std::exception& e) {
                    return tcp_status_t::fail_to_accept;
                }

                // start listening
                wait_for_connection();
            }
            else {
                m_acceptor.close();
            }
        }

        template<typename T>
        tcp_status_t send_to_client(common::esp_id_t client_id, const T& data) {
            std::lock_guard lock(m_connection_mutex);

            auto it = m_connections.find(client_id);
            if (it == m_connections.end()) {
                return tcp_status_t::unknown_client;
            }

            bool success = it->second->send(
                m_registry.create_payload(data),
                m_registry.get_packet_bytes<T>()
            );

            if (!success) return tcp_status_t::no_client_connection;

            return tcp_status_t::success;
        }

        template<typename T, auto Fn>
        void register_receive_callback() {
            m_registry.register_callback<T, Fn>();
        }

        void receive_from_client(bool enable) {
            std::lock_guard lock(m_connection_mutex);

            // tell every client that the server doesn't/does want to receive 
            for (auto& [_, connection] : m_connections) {
                connection->set_receiving(enable);
            }
        }

        tcp_status_t disconnect_client(common::esp_id_t client_id) {
            std::lock_guard lock(m_connection_mutex);

            auto it = m_connections.find(client_id);
            if (it == m_connections.end()) {
                return tcp_status_t::unknown_client;
            }

            if (it->second->close()) {
                m_connections.erase(it);

                return tcp_status_t::success;
            }

            return tcp_status_t::no_client_connection;
        }

    private:
        void handle_client_data(common::esp_id_t id, common::payload_t&& buffer, size_t bytes_received) {
            m_registry.dispatch(id, std::move(buffer), bytes_received);
        }

        void wait_for_connection() {
            if (!m_acceptor.is_open()) return;

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
            if (!m_acceptor.is_open()) return;

            if (error) {
                //std::cout << "Accepting incoming client failed: " << error.message() << "\n";
                return; 
            }

            // setup new valid connection
            new_connection->set_connection_id(m_connection_count);
            new_connection->set_receive_callback([&](common::esp_id_t id, common::payload_t&& buffer, size_t bytes) {
                handle_client_data(id, std::move(buffer), bytes);
            });
            new_connection->set_registry(&m_registry);

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

        std::unordered_map<common::esp_id_t, tcp_connection_ptr_t> m_connections;
        std::mutex m_connection_mutex;
        common::esp_id_t m_connection_count = 0;

        common::registry_t m_registry;
    };
}