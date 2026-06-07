#pragma once

namespace client {
    struct provisioning_status_t {
        enum value {
            none,
            connecting,
            connected,
            bad_credentials,
        };

        static const char* to_str(provisioning_status_t::value status) {
            switch (status) {

                case provisioning_status_t::value::connected:
                    return "connected";

                case provisioning_status_t::bad_credentials:
                    return "bad_credentials";

                case provisioning_status_t::connecting:
                    return "connecting";

                default:
                    return "idle";
                    
            }
        }
    };
}