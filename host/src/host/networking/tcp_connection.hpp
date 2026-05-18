#pragma once

#include <memory>

#include <asio.hpp>

#include "host/networking/tcp_callbacks.hpp"
#include "host/networking/tcp_io_state.hpp"

#include "common/registry.hpp"

namespace ip = asio::ip;

namespace host {
    class tcp_connection_t : public std::enable_shared_from_this<tcp_connection_t> {
    public:
        using pointer_t = std::shared_ptr<tcp_connection_t>;

        static pointer_t create(asio::io_context& io_context);

        ip::tcp::socket& get_socket();

        void set_spec(common::esp_id_t id, common::registry_t& registry, on_disconnect_fn&& callback);

        bool send(common::payload_t&& payload, size_t bytes);

        void set_receiving_state(bool enable);

        bool disconnect();

    private:
        explicit tcp_connection_t(asio::io_context& io_context);

    private:
        ip::tcp::socket m_socket;
        tcp_io_state_t m_io_state;

        common::esp_id_t m_id = 0;
        on_disconnect_fn m_disconnect_callback;
    };

    using tcp_connection_ptr_t = tcp_connection_t::pointer_t;
}