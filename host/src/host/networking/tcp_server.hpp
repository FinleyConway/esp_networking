#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <utility>
#include <exception>
#include <system_error>
#include <unordered_map>

#include <asio.hpp>

#include "host/networking/mdns_service.hpp"
#include "host/networking/tcp_callbacks.hpp"
#include "host/networking/tcp_connection.hpp"

#include "common/api/types.hpp"
#include "common/api/registry.hpp"

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
        tcp_server_t();
        ~tcp_server_t();

        void start();

        bool is_running() const;

        void shutdown();

        tcp_status_t toggle_accepting(bool enable);

        template<typename T>
        tcp_status_t send_to_client(common::esp_id_t client_id, const T& data) {
            std::lock_guard lock(m_connection_mutex);

            auto it = m_connections.find(client_id);
            if (it == m_connections.end()) {
                return tcp_status_t::unknown_client;
            }

            const bool success = it->second->send(
                m_registry.create(data),
                m_registry.packet_size<T>() + sizeof(common::esp_id_t)
            );

            if (!success) return tcp_status_t::no_client_connection;

            return tcp_status_t::success;
        }

        void receive_from_client(bool enable);

        tcp_status_t disconnect_client(common::esp_id_t client_id);

        template<typename T, auto Fn>
        void register_receive_callback() {
            m_registry.register_callback<T, Fn>();
        }

        void register_on_connect(on_connect_fn&& callback);

        void register_on_disconnect(on_connect_fn&& callback);

    private:
        void on_connect(common::esp_id_t id);

        void on_disconnect(common::esp_id_t id);
        
        void wait_for_connection();

        void create_new_connection(tcp_connection_ptr_t new_connection, const std::error_code& ec);

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