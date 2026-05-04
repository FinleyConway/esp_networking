#pragma once

namespace common {
    struct restart_esp_t { 
        #pragma pack(push, 1)
        struct net_t {};
        #pragma pack(pop)

        static net_t to_net(restart_esp_t data) {
            return {};
        }

        static restart_esp_t from_net(net_t net) {
            return {};
        }
    };
}