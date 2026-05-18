#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <utility>
#include <exception>
#include <system_error>
#include <unordered_map>

#include <asio.hpp>

#include "registry.hpp"
#include "service_config.hpp"
#include "logging/logger.hpp"
#include "networking/mdns_service.hpp"
#include "networking/tcp_callbacks.hpp"
#include "networking/tcp_connection.hpp"

namespace host {
    namespace ip = asio::ip;

    enum class tcp_status_t {
        success,
        already_accepting,
        fail_to_accept,
        unknown_client,
        no_client_connection
    };

    class tcp_server_t {
    public:
        tcp_server_t(const ip::tcp& protocol) : 
            m_acceptor(m_io_context),
            m_endpoint(ip::tcp::endpoint(protocol, std::atoi(common::service_config_t::port))),
            m_work_guard(asio::make_work_guard(m_io_context))
        {
        }

        ~tcp_server_t() {
            shutdown();
        }

        void start() {
            m_io_thread = std::thread([this] {
                m_mdns_service.start(
                    common::service_config_t::name, 
                    common::service_config_t::port
                );

                m_io_context.run();
            });
        }

        bool is_running() const {
            return m_io_thread.joinable();
        }

        void shutdown() {
            toggle_accepting(false);

            // loop through each client and disconnect them
            for (auto& [_, connection] : m_connections) {
                connection->disconnect();
            }

            m_work_guard.reset();
            m_mdns_service.stop();
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

            return tcp_status_t::success;
        }

        template<typename T>
        tcp_status_t send_to_client(common::esp_id_t client_id, const T& data) {
            std::lock_guard lock(m_connection_mutex);

            auto it = m_connections.find(client_id);
            if (it == m_connections.end()) {
                return tcp_status_t::unknown_client;
            }

            const bool success = it->second->send(
                m_registry.create_payload(data),
                m_registry.get_packet_bytes<T>() + sizeof(common::esp_id_t)
            );

            if (!success) return tcp_status_t::no_client_connection;

            return tcp_status_t::success;
        }

        void receive_from_client(bool enable) {
            std::lock_guard lock(m_connection_mutex);

            // tell every client that the server doesn't/does want to receive 
            for (auto& [_, connection] : m_connections) {
                connection->set_receiving_state(enable);
            }
        }

        tcp_status_t disconnect_client(common::esp_id_t client_id) {
            std::lock_guard lock(m_connection_mutex);

            auto it = m_connections.find(client_id);
            if (it == m_connections.end()) {
                return tcp_status_t::unknown_client;
            }

            if (it->second->disconnect()) {
                m_connections.erase(it);

                return tcp_status_t::success;
            }

            return tcp_status_t::no_client_connection;
        }

        template<typename T, auto Fn>
        void register_receive_callback() {
            m_registry.register_callback<T, Fn>();
        }

        void register_on_connect(on_connect_fn&& callback) {
            m_on_connect_callback = std::move(callback);
        }

        void register_on_disconnect(on_connect_fn&& callback) {
            m_on_disconnect_callback = std::move(callback);
        }

    private:
        void on_connect(common::esp_id_t id) {
            if (m_on_connect_callback) {
                m_on_connect_callback(id);
            }
        }

        void on_disconnect(common::esp_id_t id) {
            if (m_on_disconnect_callback) {
                m_on_disconnect_callback(id);
            }

            if (auto it = m_connections.find(id); it != m_connections.end()) {
                m_connections.erase(it);
            }
        }
        
        void wait_for_connection() {
            if (!m_acceptor.is_open()) return;

            tcp_connection_ptr_t new_connection = tcp_connection_t::create(m_io_context);

            m_acceptor.async_accept(
                new_connection->get_socket(), 
                [this, new_connection](const std::error_code& ec) {
                    this->create_new_connection(new_connection, ec);
                }
            );
        }

        void create_new_connection(tcp_connection_ptr_t new_connection, const std::error_code& ec) {
            if (!m_acceptor.is_open()) return;

            if (ec) {
                LOG_ERROR("Accepting incoming client failed: {}", ec.message());

                wait_for_connection(); // TODO: Fix this
                return; 
            }

            // setup new valid connection
            new_connection->set_spec(m_connection_count, m_registry, [this](common::esp_id_t id) {
                on_disconnect(id);
            });
            m_connections.emplace(m_connection_count, new_connection);

            // call callback
            on_connect(m_connection_count);

            // move and check for other connections
            m_connection_count++;
            wait_for_connection();
        }

    private:
        asio::io_context m_io_context;
        ip::tcp::acceptor m_acceptor;
        ip::tcp::endpoint m_endpoint;
        host::mdns_service_t m_mdns_service;

        std::thread m_io_thread;
        std::mutex m_connection_mutex;
        asio::executor_work_guard<asio::io_context::executor_type> m_work_guard;

        std::unordered_map<common::esp_id_t, tcp_connection_ptr_t> m_connections;
        common::esp_id_t m_connection_count = 0;

        on_connect_fn m_on_connect_callback;
        on_disconnect_fn m_on_disconnect_callback;

        common::registry_t m_registry;
    };
}