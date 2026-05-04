#pragma once

#include <array>
#include <deque>
#include <memory>
#include <optional>

#include <asio.hpp>
#include <sys/types.h>

#include "tcp_common.hpp"

#include "registry.hpp"

namespace ip = asio::ip;

class tcp_connection_t : public std::enable_shared_from_this<tcp_connection_t> {
public:
    typedef std::shared_ptr<tcp_connection_t> pointer_t;

    static pointer_t create(asio::io_context& io_context) {
        return pointer_t(new tcp_connection_t(io_context));
    }

    ip::tcp::socket& get_socket() {
        return m_socket;
    }

    void set_connection_id(esp_id_t id) {
        m_connection_id = std::make_optional(id);
    }

    void can_receive_from(bool flag) {
        m_can_send = flag;

        if (flag) {
            //read_from();
        }
    }

    void send(common::payload_t&& buffer) {
        if (!m_socket.is_open()) {
            //LOG_WARN("Client {} is disconnecting, write prevented", m_connection_id);
            return;
        }

        // queue buffer data
        m_write_queue.emplace_back(std::move(buffer));

        // prevent over writing by doing 1 operation at a time
        if (!m_is_writing) {
            write_to();
        }
    }

    void close_connection() {
        if (!m_socket.is_open()) return;

        // make sure everything stops
        m_is_reading = false;
        m_is_writing = false;
        m_write_queue.clear();  

        if (std::error_code ec; m_socket.close(ec)) {
            //LOG_ERROR("Client {}, socket failed to close: {}", m_connection_id, ec.message());
        }
    }

private:
    explicit tcp_connection_t(asio::io_context& io_context) : m_socket(io_context) {
    }

    void write_to() {
        // prevent multiple requests from happening
        if (m_is_writing || m_write_queue.empty()) {
            return;
        }

        m_is_writing = true;
        auto& bytes = m_write_queue.front();

        asio::async_write(
            m_socket, 
            asio::buffer(bytes.data(), bytes.size()),
            std::bind(
                &tcp_connection_t::handle_write, 
                shared_from_this(),
                asio::placeholders::error
            )
        );
    }

    void handle_write(const std::error_code& error) {
        m_is_writing = false;

        if (error) {
            handle_io_errors(error);
            return;
        }

        m_write_queue.pop_front();
        write_to();
    }

    void read_from() {    
        // prevent clients from senting information to server
        if (m_is_reading || !m_can_send) return;

        m_is_reading = true;

        asio::async_read(
            m_socket,
            asio::buffer(m_read_data.data(), m_read_data.size()),
            std::bind(
                &tcp_connection_t::handle_read, 
                shared_from_this(),
                asio::placeholders::error
            )
        );
    }

    void handle_read(const std::error_code& error) {
        m_is_reading = false;

        if (error) {
            handle_io_errors(error);
            return;
        }

        // notify server
        // ...

        read_from();
    }

    void handle_io_errors(const std::error_code& error) {
        if (!m_socket.is_open()) return;

        if (error == asio::error::operation_aborted) return;

        if (error == asio::error::eof || error == asio::error::connection_reset) {
            //close_connection();
            return;
        }

        //LOG_WARN("Client {} I/O error: {}", m_connection_id, error.message());

        //close_connection();
    }

private:
    ip::tcp::socket m_socket;
    std::optional<esp_id_t> m_connection_id;
    std::deque<common::payload_t> m_write_queue;
    common::payload_t m_read_data;
    
    bool m_can_send = true;
    bool m_is_writing = false;
    bool m_is_reading = false;
};

using tcp_connection_ptr_t = tcp_connection_t::pointer_t;