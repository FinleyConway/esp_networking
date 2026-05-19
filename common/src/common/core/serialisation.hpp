    #pragma once

    #include <type_traits>
    #include <concepts>
    #include <cstdint>
    #include <cstring>
    #include <array>
    #include <bit>

    #include "common/core/net_types.hpp"

    namespace common {
        template<typename T>
        concept primitive_type = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

        template<typename T>
        T swap_bytes(T value) {
            constexpr size_t size = sizeof(T);
            constexpr bool needs_swap = std::endian::native == std::endian::big;    

            // return regular value if same endian or is a byte
            if constexpr (!needs_swap || size == 1) {
                return value;
            }
            else if constexpr (std::is_integral_v<T>) {
                return std::byteswap(value);
            }
            else if constexpr (std::is_floating_point_v<T>) {
                // temp convert decimal to a int
                using temp_int_t = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;

                // convert the decimal
                temp_int_t temp{};
                std::memcpy(&temp, &value, sizeof(T));

                // swap and convert bytes back to decimal
                temp = std::byteswap(temp);
                std::memcpy(&value, &temp, sizeof(T));

                return value;
           }

           return value;
        }

        template<primitive_type T>
        void write(common::payload_view_t& payload, T primitive) {
            assert(payload.size() >= sizeof(T));

            primitive = swap_bytes(primitive);

            std::memcpy(payload.data(), &primitive, sizeof(T));

            payload = payload.subspan(sizeof(T)); // offset payload
        }

        template<typename T, size_t N>
        void write(payload_view_t& payload, const std::array<T, N>& arr) {
            for (auto v : arr) {
                write(payload, v);
            }
        }

        template<primitive_type T>
        T read(common::payload_view_t& payload) {
            assert(payload.size() >= sizeof(T));

            T primitive{};

            std::memcpy(&primitive, payload.data(), sizeof(T));

            primitive = swap_bytes(primitive);

            payload = payload.subspan(sizeof(T)); // offset payload

            return primitive;
        }

        template<typename T, size_t N>
        std::array<T, N> read(common::payload_view_t& payload) {
            std::array<T, N> arr;

            for (auto& v : arr) {
                v = read<T>(payload);
            }

            return arr;
        }
    }