#include "host/networking/tcp_server.hpp"

#include "host/logging/logger.hpp"

#include "common/api/service_config.hpp"

namespace host {
    tcp_server_t::tcp_server_t() : 
        m_acceptor(m_io_context),
        m_endpoint(ip::tcp::endpoint(ip::tcp::v4(), std::atoi(common::service_config_t::port))),
        m_work_guard(asio::make_work_guard(m_io_context))
    {
    }

    tcp_server_t::~tcp_server_t() {
        shutdown();
    }

    void tcp_server_t::start() {
        m_io_thread = std::thread([this] {
            m_mdns_service.start(
                common::service_config_t::hostname, 
                common::service_config_t::port
            );

            m_io_context.run();
        });
    }

    bool tcp_server_t::is_running() const {
        return m_io_thread.joinable();
    }

    void tcp_server_t::shutdown() {
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

    tcp_status_t tcp_server_t::toggle_accepting(bool enable) {
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

    void tcp_server_t::receive_from_client(bool enable) {
        std::lock_guard lock(m_connection_mutex);

        // tell every client that the server doesn't/does want to receive 
        for (auto& [_, connection] : m_connections) {
            connection->set_receiving_state(enable);
        }

        m_receiving = enable;
    }

    tcp_status_t tcp_server_t::disconnect_client(common::esp_id_t client_id) {
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

    void tcp_server_t::register_on_connect(on_connect_fn&& callback) {
        m_on_connect_callback = std::move(callback);
    }

    void tcp_server_t::register_on_disconnect(on_disconnect_fn&& callback) {
        m_on_disconnect_callback = std::move(callback);
    }

    void tcp_server_t::on_connect(common::esp_id_t id) {
        if (m_on_connect_callback) {
            m_on_connect_callback(id);
        }
    }

    void tcp_server_t::on_disconnect(common::esp_id_t id) {
        if (m_on_disconnect_callback) {
            m_on_disconnect_callback(id);
        }

        if (auto it = m_connections.find(id); it != m_connections.end()) {
            m_connections.erase(it);
        }
    }
        
    void tcp_server_t::wait_for_connection() {
        if (!m_acceptor.is_open()) return;

        tcp_connection_ptr_t new_connection = tcp_connection_t::create(m_io_context);

        m_acceptor.async_accept(
            new_connection->get_socket(), 
            [this, new_connection](const std::error_code& ec) {
                this->create_new_connection(new_connection, ec);
            }
        );
    }

    void tcp_server_t::create_new_connection(tcp_connection_ptr_t new_connection, const std::error_code& ec) {
        if (!m_acceptor.is_open()) return;

        if (ec) {
            LOG_ERROR("Accepting incoming client failed: {}", ec.message());

            wait_for_connection();
            return; 
        }

        // setup new valid connection
        new_connection->set_spec(m_connection_count, m_registry, [this](common::esp_id_t id) {
            on_disconnect(id);
        });
        new_connection->set_receiving_state(m_receiving);
        m_connections.emplace(m_connection_count, new_connection);

        // call callback
        on_connect(m_connection_count);

        // move and check for other connections
        m_connection_count++;
        wait_for_connection();
    }
}