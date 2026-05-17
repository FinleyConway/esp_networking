#pragma once

#include "packet_registry.hpp"

#include "types/init_esp.hpp"
#include "types/restart_esp.hpp"

namespace common {
    using registry_t = packet_registry_impl_t<
        init_esp_t,
        restart_esp_t
    >;

    using esp_id_t = registry_t::packet_id_t;
    using payload_t = registry_t::payload_t;
    using payload_view_t = registry_t::payload_view_t;
}