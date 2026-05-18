#pragma once

#include <sys/socket.h>

#include "registry.hpp"

namespace client {
    enum class tcp_status_t {
        success,
        failure,
        connection_closed,
        socket_failure,
        failed_to_connect,
        unknown_mds_address,
        unknown_packet
    };

    class tcp_client_t {
    public:
        int get_socket() const;

        tcp_status_t try_connect();

        tcp_status_t disconnect();

        bool is_connected() const;

        template<typename T, auto Fn>
        void register_receieve_callback() {
            m_registry.register_callback<T, Fn>();
        }

        template<typename T>
        tcp_status_t send_to_server(common::esp_id_t client_id, const T& data) {
            const size_t payload_size = m_registry.get_packet_bytes<T>();
            common::payload_t payload = m_registry.create_payload<T>(data);
            size_t total_bytes_send = 0;

            while (total_bytes_send < payload_size) {
                const ssize_t bytes_sent = send(
                    m_socket,
                    payload.data() + total_bytes_send,
                    payload_size - total_bytes_send,
                    0
                );

                if (bytes_sent == -1) return tcp_status_t::failure;
                if (bytes_sent == 0) return tcp_status_t::connection_closed;

                total_bytes_send += bytes_sent;
            }

            return tcp_status_t::success;
        }

        tcp_status_t listen_to_server();

    private:
        tcp_status_t recv_exact(uint8_t* dst, size_t bytes_to_read, size_t& bytes_read) const;

        common::esp_id_t recv_payload_id(common::payload_t& payload, tcp_status_t& status) const;

        size_t recv_payload(size_t expected_payload_bytes, common::payload_t& payload, tcp_status_t& status);

    private:
        int m_socket = -1;
        common::registry_t m_registry;
    };
}