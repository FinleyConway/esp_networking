#pragma once

#include <cstdint>

namespace common {
    struct init_esp_t {
        uint16_t id;

        #pragma pack(push, 1)
        struct net_t {
            uint16_t id;
        };
        #pragma pack(pop)

        static net_t to_net(init_esp_t data) {
            return {
                .id = data.id
            };
        }

        static init_esp_t from_net(net_t net) {
            return {
                .id = net.id
            };
        }
    };
}