#include "networking/tcp_server.hpp"
#include "types/restart_esp.hpp"

void on_esp_restart(common::restart_esp_t) {

}

int main() {
    tcp_server_t server;
    server.configure(ip::tcp::v4(), 8080);
    server.register_receive_callback<common::restart_esp_t, &on_esp_restart>();
    server.start();
    server.toggle_accepting(true);

    while(1);
}