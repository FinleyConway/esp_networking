#include <iostream>

#include "host/networking/tcp_server.hpp"
#include "host/logging/logger.hpp"

#include "common/messages/handshake.hpp"

int main() {
    host::logger_t::init();
    host::tcp_server_t server;

    server.start();
    
    if (server.toggle_accepting(true) != host::tcp_status_t::success) {
        return -1;
    }

    while(server.is_running()) {
    }
}