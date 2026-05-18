#pragma once

namespace common {
    struct service_config_t {
        static constexpr const char* name = "esp_server";
        static constexpr const char* hostname = "esp_server.local";
        static constexpr const char* port = "8080";
    };
}