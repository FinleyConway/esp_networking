#include <iostream>

#include "networking/tcp_server.hpp"
#include "logging/logger.hpp"
#include "types/restart_esp.hpp"

void on_esp_init(common::init_esp_t init_esp) {
    LOG_INFO("Received from esp: {}", init_esp.id);
}

void on_connect(common::esp_id_t id) {
    LOG_INFO("ESP: {} connected", id);
}

void on_disconnect(common::esp_id_t id) {
    LOG_INFO("ESP: {} disconnected", id);
}

int main() {
    host::logger_t::init();
    host::tcp_server_t server(ip::tcp::v4());

    server.register_receive_callback<common::init_esp_t, &on_esp_init>();
    server.register_on_connect(on_connect);
    server.register_on_disconnect(on_disconnect);

    server.start();
    if (server.toggle_accepting(true) != host::tcp_status_t::success) {
        return -1;
    }

    while(server.is_running()) {
        std::string x;

        std::cout << "Input:\n";
        std::cin >> x;

        auto status = server.send_to_client(0, common::init_esp_t(1));

        if (status == host::tcp_status_t::success) {
            LOG_INFO("init_esp_t was sent to esp!");
        }

        if (status == host::tcp_status_t::unknown_client) {
            LOG_INFO("Client could not be found!");
        }

        if (status == host::tcp_status_t::no_client_connection) {
            LOG_INFO("Client lost connection?");
        }
    }
}