#pragma once

#include <deque>
#include <memory>
#include <functional>
#include <system_error>

#include <asio.hpp>

#include "tcp_io_state.hpp"
#include "registry.hpp"

namespace ip = asio::ip;

namespace host {
    using on_receive_fn = std::function<void(common::esp_id_t, common::payload_t&&, size_t)>;

    struct tcp_connection_info_t {
        common::esp_id_t id = 0;
        common::registry_t* registry = nullptr;

        on_receive_fn callback;
    };

    class tcp_connection_t : public std::enable_shared_from_this<tcp_connection_t> {
    public:
        using pointer_t = std::shared_ptr<tcp_connection_t>;

        static pointer_t create(asio::io_context& io_context) {
            return pointer_t(new tcp_connection_t(io_context));
        }

        ip::tcp::socket& get_socket() {
            return m_socket;
        }

        void set_info(const tcp_connection_info_t& info) {
            m_connection_id = info.id;
            m_receive_callback = std::move(info.callback);
        }

        void set_registry(common::registry_t* registry) {
        }

        bool send(common::payload_t&& payload, size_t bytes) {
            return m_io_state.send(std::move(payload), bytes);
        }

        void set_receiving_state(bool enable) {
            if (enable) {
                m_io_state.start_receiving();
            }   
            else {
                m_io_state.stop_receiving();
            }
        }

    private:
        explicit tcp_connection_t(asio::io_context& io_context) : m_socket(io_context) {
        }

    private:
        ip::tcp::socket m_socket;
        common::esp_id_t m_connection_id;

        tcp_io_state_t m_io_state;

        on_receive_fn m_receive_callback;
    };

    using tcp_connection_ptr_t = tcp_connection_t::pointer_t;
}