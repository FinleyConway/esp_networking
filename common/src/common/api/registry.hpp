#pragma once

#include "common/core/packet_registry.hpp"

#include "common/messages/handshake.hpp"

namespace common {
    using registry_t = packet_registry_impl_t<
        esp_init_request_t,
        esp_init_response_t
    >;

    // Globally provide a compile time size payload from registry.
    // std::array<uint8_t, {size_of_max_message_type}> 
    using payload_t = registry_t::payload_t;
    
    // The compile time id for each message.
    using packet_id_t = registry_t::packet_id_t;
}