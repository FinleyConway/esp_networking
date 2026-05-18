#include "host/networking/tcp_io_state.hpp"

namespace host {
    tcp_io_state_t::tcp_io_state_t(ip::tcp::socket& io_context) : m_socket(io_context) {}

    void tcp_io_state_t::set_spec(common::registry_t& registry, on_disconnect_fn&& callback) {
        m_registry = &registry;
        m_disconnect_callback = std::move(callback);
    }

    bool tcp_io_state_t::send(common::payload_t&& payload, size_t bytes) {
        if (!m_socket.is_open()) return false;

        m_write_state_queue.emplace_back(
            std::move(payload),
            bytes
        );
        
        if (m_io_write_state == io_state_t::idle) {
            write();
        }

        return true;
    }

    void tcp_io_state_t::start_receiving() {
        m_can_receive = true;

        read_id();
    }

    void tcp_io_state_t::stop_receiving() {
        m_can_receive = false;
        m_io_read_state == io_state_t::idle;
    }

    void tcp_io_state_t::stop_io() {
        m_can_receive = false;
        m_io_read_state == io_state_t::idle;
        m_io_write_state = io_state_t::idle;
        m_write_state_queue.clear();
    }

    void tcp_io_state_t::write() {
        if (m_write_state_queue.empty()) return;
        if (m_io_write_state == io_state_t::writing_payload) return;

        m_io_write_state = io_state_t::writing_payload;
        auto& buffer = m_write_state_queue.front();

        asio::async_write(
            m_socket, 
            asio::buffer(buffer.payload.data(), buffer.bytes),
            [this](const std::error_code& ec, size_t) {
                m_io_write_state = io_state_t::idle;

                if (!has_or_handle_io_error(ec)) {
                    // remove currently sent state
                    m_write_state_queue.pop_front();

                    // start the next write cycle
                    write();
                }
            }
        );
    }

    void tcp_io_state_t::read_id() {    
        if (!m_can_receive) return;
        if (m_io_read_state == io_state_t::reading_payload) return;

        m_io_read_state = io_state_t::reading_payload;

        asio::async_read(
            m_socket,
            asio::buffer(&m_read_state.id, sizeof(common::esp_id_t)),
            [this](const std::error_code& ec, size_t bytes_transferred) {
                read_payload(ec, bytes_transferred);
            }
        );
    }

    void tcp_io_state_t::read_payload(const std::error_code& id_ec, size_t) {
        if (has_or_handle_io_error(id_ec)) return;

        LOG_ASSERT(m_registry != nullptr, "m_registry is nullptr!");

        size_t size = m_registry->get_packet_bytes(m_read_state.id);

        LOG_ASSERT(size != 0, "Packet size is not registered!");

        asio::async_read(
            m_socket,
            asio::buffer(m_read_state.payload.data(), size),
            [this](const std::error_code& ec, size_t bytes_transferred) {
                if (!has_or_handle_io_error(ec)) {
                    LOG_ASSERT(m_registry != nullptr, "m_registry is nullptr!");

                    // notify server
                    m_registry->dispatch(
                        m_read_state.id,
                        std::move(m_read_state.payload), 
                        bytes_transferred
                    );

                    // start the next read cycle
                    m_io_read_state = io_state_t::idle;
                    read_id();
                }
            }
        );
    }

    bool tcp_io_state_t::has_or_handle_io_error(const std::error_code& ec) {
        // all errors seem to be suitable to just disconnect client
        if (ec) {
            LOG_ERROR("Failed to read/write: {}", ec.message());

            stop_io();

            if (m_disconnect_callback) {
                m_disconnect_callback(0); // id is pointless here, kind of a code smell?
            }

            return true;
        }

        return false;
    }
}