#pragma once

#include <deque>
#include <cstdint>
#include <functional>
#include <system_error>

#include <asio.hpp>

#include "host/networking/tcp_callbacks.hpp"

#include "common/api/registry.hpp"
#include "common/api/types.hpp"

namespace host {
    namespace ip = asio::ip;

    class tcp_io_state_t {
    public:
        tcp_io_state_t(ip::tcp::socket& io_context);

        void set_spec(common::registry_t& registry, on_disconnect_fn&& callback);

        bool send(common::payload_t&& payload, size_t bytes);

        void start_receiving();

        void stop_receiving();

        void stop_io();

    private:
        enum class io_state_t {
            idle,
            reading_payload,
            writing_payload
        };

    private:
        void write();

        void read_id();

        void read_payload(const std::error_code& id_ec, size_t);

        bool has_or_handle_io_error(const std::error_code& ec);

    private:
        struct write_state_t {
            const common::payload_t payload;
            const size_t bytes = 0;
        };

        struct read_state_t {
            common::packet_id_t id = 0;
            common::payload_t payload;
        };

    private:
        ip::tcp::socket& m_socket;
        common::registry_t* m_registry = nullptr;

        bool m_can_receive = false;
        read_state_t m_read_state;
        std::deque<write_state_t> m_write_state_queue;
        io_state_t m_io_read_state = io_state_t::idle;
        io_state_t m_io_write_state = io_state_t::idle;

        on_disconnect_fn m_disconnect_callback;
    };
}