#include "host/networking/tcp_connection.hpp"

namespace host {
    tcp_connection_t::pointer_t tcp_connection_t::create(asio::io_context& io_context) {
        return pointer_t(new tcp_connection_t(io_context));
    }

    ip::tcp::socket& tcp_connection_t::get_socket() {
        return m_socket;
    }

    void tcp_connection_t::set_spec(common::esp_id_t id, common::registry_t& registry, on_disconnect_fn&& callback) {
        m_io_state.set_spec(id, registry, std::move(callback));
    }

    bool tcp_connection_t::send(common::payload_t&& payload, size_t bytes) {
        return m_io_state.send(std::move(payload), bytes);
    }

    void tcp_connection_t::set_receiving_state(bool enable) {
        if (enable) {
            m_io_state.start_receiving();
        }   
        else {
            m_io_state.stop_receiving();
        }
    }

    bool tcp_connection_t::disconnect() {
        if (!m_socket.is_open()) return false;

        m_io_state.stop_io();
        m_socket.close();

        return true;
    }

    tcp_connection_t::tcp_connection_t(asio::io_context& io_context) : m_socket(io_context), m_io_state(m_socket) {
    }
}