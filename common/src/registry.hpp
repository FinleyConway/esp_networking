#pragma once

#include "packet_registry.hpp"

#include "types/init_esp.hpp"
#include "types/restart_esp.hpp"

namespace common {
    using registry_t = packet_registry_impl_t<
        init_esp_t,
        restart_esp_t
    >;

    using payload_t = std::array<uint8_t, registry_t::get_max_payload()>;
}