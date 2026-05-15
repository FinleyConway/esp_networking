#pragma once

#include <deque>
#include <memory>
#include <functional>
#include <system_error>

#include <asio.hpp>

#include "registry.hpp"

namespace ip = asio::ip;

namespace host {
    class tcp_connection_t : public std::enable_shared_from_this<tcp_connection_t> {
    public:
        using on_receive_fn = std::function<void(common::esp_id_t, common::payload_t&&, size_t)>;
        using pointer_t = std::shared_ptr<tcp_connection_t>;

        static pointer_t create(asio::io_context& io_context) {
            return pointer_t(new tcp_connection_t(io_context));
        }

        ip::tcp::socket& get_socket() {
            return m_socket;
        }

        void set_connection_id(common::esp_id_t id) {
            m_connection_id = id;
        }

        void set_receive_callback(on_receive_fn&& callback) {
            m_receive_callback = std::move(callback);
        }

        void set_registry(common::registry_t* registry) {
            m_registry = registry;
        }

        void set_receiving(bool enable) {
            m_receiving = enable;

            if (enable) {
                read_id();
            }
        }

        bool send(common::payload_t&& payload, size_t payload_bytes) {
            if (!m_socket.is_open()) return false;

            m_write_state_queue.emplace_back(
                std::move(payload),
                payload_bytes
            );

            if (!m_writing) {
                write();
            }

            return true;
        }

        bool close() {
            if (!m_socket.is_open()) return false;

            m_receiving = false;
            m_writing = false;
            m_write_state_queue.clear();  
            m_socket.close();

            return true;
        }

    private:
        explicit tcp_connection_t(asio::io_context& io_context) : m_socket(io_context) {
        }

        void write() {
            if (m_writing || m_write_state_queue.empty()) return;

            m_writing = true;

            auto& buffer = m_write_state_queue.front();
            auto self = shared_from_this();

            asio::async_write(
                m_socket, 
                asio::buffer(buffer.payload.data(), buffer.bytes),
                [self](const std::error_code& ec, size_t) {
                    self->m_writing = false;

                    if (ec) return self->handle_errors(ec);

                    self->m_write_state_queue.pop_front();
                    self->write();
                }
            );
        }

        void read_id() {    
            if (m_reading || !m_receiving) return;

            m_reading = true;

            auto self = shared_from_this();

            asio::async_read(
                m_socket,
                asio::buffer(&m_read_state.id, sizeof(common::esp_id_t)),
                [self](const std::error_code& ec, size_t) {
                    if (ec) return self->handle_errors(ec);

                    self->read_payload();
                }
            );
        }

        void read_payload() {
            // add assert for reg

            size_t size = m_registry->get_packet_bytes(m_read_state.id);

            if (size == 0) {
                return;
            }

            auto self = shared_from_this();

            asio::async_read(
                m_socket,
                asio::buffer(m_read_state.payload.data(), size),
                [self](const std::error_code& ec, size_t bytes_transferred) {
                    if (ec) return self->handle_errors(ec);

                    // add assert for fn

                    self->m_receive_callback(
                        self->m_read_state.id,
                        std::move(self->m_read_state.payload), 
                        bytes_transferred
                    );
                    self->m_reading = false;

                    self->read_id();
                }
            );
        }

        void handle_errors(const std::error_code& ec) {
            if (!m_socket.is_open()) return;

            if (ec == asio::error::operation_aborted) return;

            if (ec == asio::error::eof || ec == asio::error::connection_reset) {
                close();
                return;
            }

            //LOG_WARN("Client {} I/O error: {}", m_connection_id, error.message());

            close();
        }

    private:
        struct write_state_t {
            const common::payload_t payload;
            const size_t bytes = 0;
        };

        struct read_state_t {
            common::esp_id_t id = 0;
            common::payload_t payload;
        };

    private:
        ip::tcp::socket m_socket;
        common::esp_id_t m_connection_id;

        on_receive_fn m_receive_callback;

        std::deque<write_state_t> m_write_state_queue;
        read_state_t m_read_state;
        
        bool m_receiving = true;
        bool m_writing = false;
        bool m_reading = false;

        common::registry_t* m_registry = nullptr;
    };

    using tcp_connection_ptr_t = tcp_connection_t::pointer_t;
}