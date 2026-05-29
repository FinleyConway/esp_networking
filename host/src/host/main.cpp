#include <iostream>

#include "host/networking/tcp_server.hpp"
#include "host/logging/logger.hpp"

#include "common/messages/handshake.hpp"

int main() {
    host::logger_t::init();
    host::tcp_server_t server;

    server.register_on_connect([&](common::esp_id_t id) {
    });

    server.register_on_disconnect([&](common::esp_id_t id) {
        LOG_INFO("ESP: {} disconnected", id);
    });

    server.register_receive_callback<common::esp_init_response_t>([&](const common::esp_init_response_t& res) {
    });

    server.start();
    
    if (server.toggle_accepting(true) != host::tcp_status_t::success) {
        LOG_INFO("tcp_server not able to accept");
        return -1;
    }

    while(server.is_running()) {
    }
}