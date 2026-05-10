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
        using on_receive_t = std::function<void(common::payload_t&&, size_t)>;
        typedef std::shared_ptr<tcp_connection_t> pointer_t;

        static pointer_t create(asio::io_context& io_context) {
            return pointer_t(new tcp_connection_t(io_context));
        }

        ip::tcp::socket& get_socket() {
            return m_socket;
        }

        void set_connection_id(common::esp_id_t id) {
            m_connection_id = id;
        }

        void set_receive_callback(on_receive_t&& callback) {
            m_receive_callback = std::move(callback);
        }

        void set_receiving(bool enable) {
            m_receiving = enable;

            if (enable) {
                start_read();
            }
        }

        void send(common::payload_t&& buffer) {
            if (!m_socket.is_open()) return;

            m_write_queue.emplace_back(std::move(buffer));

            if (!m_writing) {
                start_write();
            }
        }

        void close() {
            if (!m_socket.is_open()) return;

            m_receiving = false;
            m_writing = false;
            m_write_queue.clear();  

            m_socket.close();
        }

    private:
        explicit tcp_connection_t(asio::io_context& io_context) : m_socket(io_context) {
        }

        void start_write() {
            if (m_writing || m_write_queue.empty()) return;

            m_writing = true;

            auto& buffer = m_write_queue.front();

            asio::async_write(
                m_socket, 
                asio::buffer(buffer.data(), buffer.size()),
                std::bind(
                    &tcp_connection_t::handle_write, 
                    shared_from_this(),
                    asio::placeholders::error
                )
            );
        }

        void handle_write(const std::error_code& ec) {
            m_writing = false;

            if (ec) {
                handle_errors(ec);
                return;
            }

            m_write_queue.pop_front();
            start_write();
        }

        void start_read() {    
            if (m_reading || !m_receiving) return;

            m_reading = true;

            asio::async_read(
                m_socket,
                asio::buffer(m_read_data.data(), m_read_data.size()),
                std::bind(
                    &tcp_connection_t::handle_read, 
                    shared_from_this(),
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred
                )
            );
        }

        void handle_read(const std::error_code& ec, size_t bytes_transferred) {
            m_reading = false;

            if (ec) {
                handle_errors(ec);
                return;
            }

            if (m_receive_callback != nullptr) {
                m_receive_callback(std::move(m_read_data), bytes_transferred);
            }

            start_read();
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
        ip::tcp::socket m_socket;
        common::esp_id_t m_connection_id;

        on_receive_t m_receive_callback;

        std::deque<common::payload_t> m_write_queue;
        common::payload_t m_read_data;
        
        bool m_receiving = true;
        bool m_writing = false;
        bool m_reading = false;
    };

    using tcp_connection_ptr_t = tcp_connection_t::pointer_t;
}