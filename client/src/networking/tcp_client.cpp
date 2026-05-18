#include "networking/tcp_client.hpp"

#include <lwip/netdb.h>

#include "service_config.hpp"

namespace client {
    int tcp_client_t::get_socket() const {
        return m_socket;
    }

    tcp_status_t tcp_client_t::try_connect() {
        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM
        };
        struct addrinfo* res;

        constexpr const char* host = common::service_config_t::hostname;
        constexpr const char* port = common::service_config_t::port;

        // try to get the address from mdns name
        if (getaddrinfo(host, port, &hints, &res) == 0) {
            const int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

            if (socket_fd < 0) {
                return tcp_status_t::socket_failure;
            }

            if (connect(socket_fd, res->ai_addr, res->ai_addrlen) < 0) {
                disconnect();

                return tcp_status_t::failed_to_connect;
            }

            m_socket = socket_fd;

            freeaddrinfo(res);

            return tcp_status_t::success;
        }

        freeaddrinfo(res);

        return tcp_status_t::unknown_mds_address;
    }

    tcp_status_t tcp_client_t::disconnect() {
        if (close(m_socket) < 0) {
            return tcp_status_t::socket_failure;
        }

        m_socket = -1;

        return tcp_status_t::success;
    }

    bool tcp_client_t::is_connected() const {
        return m_socket >= 0;
    }

    tcp_status_t tcp_client_t::listen_to_server() {
        tcp_status_t status;
        common::payload_t payload;

        const common::esp_id_t received_id = recv_payload_id(payload, status);

        if (status != tcp_status_t::success) return status;

        const size_t expected_payload_bytes = m_registry.get_packet_bytes(received_id);
        const bool has_payload = expected_payload_bytes != 0;

        if (!has_payload) return tcp_status_t::unknown_packet;

        const size_t payload_bytes = recv_payload(expected_payload_bytes, payload, status);

        if (status != tcp_status_t::success) return status;

        const size_t received_bytes = payload_bytes + sizeof(received_id);

        if (!m_registry.dispatch(received_id, std::move(payload), received_bytes)) {
            return tcp_status_t::unknown_packet;
        }

        return tcp_status_t::success;
    }

    tcp_status_t tcp_client_t::recv_exact(uint8_t* dst, size_t bytes_to_read, size_t& bytes_read) const {
        bytes_read = 0;

        while (bytes_read < bytes_to_read) {
            const ssize_t result = recv(
                m_socket,
                dst + bytes_read,
                bytes_to_read - bytes_read,
                0
            );

            if (result == -1) return tcp_status_t::failure;
            if (result == 0) return tcp_status_t::connection_closed;

            bytes_read += result;
        }

        return tcp_status_t::success;
    }

    common::esp_id_t tcp_client_t::recv_payload_id(common::payload_t& payload, tcp_status_t& status) const {
        constexpr size_t esp_id_size = sizeof(common::esp_id_t);
        size_t id_bytes_read = 0;

        status = recv_exact(payload.data(), esp_id_size, id_bytes_read);

        // get the payload size from the received id
        common::esp_id_t received_id = 0;
        std::memcpy(&received_id, payload.data(), esp_id_size);

        return received_id;
    }

    size_t tcp_client_t::recv_payload(size_t expected_payload_bytes, common::payload_t& payload, tcp_status_t& status) {
        size_t payload_bytes = 0;

        status = recv_exact(
            payload.data() + sizeof(common::esp_id_t), 
            expected_payload_bytes,
            payload_bytes
        );

        return payload_bytes;
    }
}