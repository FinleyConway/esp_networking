#pragma once

#include <span>
#include <array>
#include <utility>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <type_traits>

namespace common {
    template<typename T>
    concept packet_type =
    requires(std::span<uint8_t>& payload, const T& obj) {
        { T::serialise(payload, obj) } -> std::same_as<void>;
        { T::deserialise(payload) } -> std::same_as<T>;
        { T::payload_size() } -> std::same_as<size_t>;
    };

    template<typename... Ts>
    class packet_registry_impl_t {
    private:
        static consteval size_t max_payload() {
            std::size_t max = 0;
            ((max = std::max(max, Ts::payload_size())), ...);
            return max + sizeof(packet_id_t);
        }

        template<typename T>
        static consteval bool contains() {
            return (std::is_same_v<T, Ts> || ...);
        }

        template<typename T, typename First, typename... Rest>
        static consteval size_t type_index_impl(size_t idx = 0) {
            if constexpr (std::is_same_v<T, First>)
                return idx;
            else {
                static_assert(sizeof...(Rest) > 0, "type not found");
                return type_index_impl<T, Rest...>(idx + 1);
            }
        }

        template<typename T>
        static consteval size_t get_type_of() {
            return type_index_impl<T, Ts...>();
        }

    public:
        using packet_id_t = uint16_t;
        using payload_t = std::array<uint8_t, max_payload()>;

        struct callback_info_t {
            void (*invoke)(std::span<uint8_t>) = nullptr;
            size_t size = 0;
        };

    public:
        template<typename T>
        payload_t create(const T& data) const {
            static_assert(contains<T>(), "T must be in Ts...");

            // set up payload
            payload_t payload{};
            std::span<uint8_t> view(payload.data(), payload.size());

            // write packet id to front
            constexpr packet_id_t id = get_type_of<T>();
            std::memcpy(view.data(), &id, sizeof(id)); 

            // write packet to payload 
            auto payload_view = view.subspan(sizeof(id));
            T::serialise(payload_view, data);

            return payload;
        }

        bool dispatch(packet_id_t id, payload_t&& payload, size_t bytes) const {
            if (id >= m_callback.size()) return false;

            const auto& callback = m_callback[id];

            if (callback.invoke == nullptr) return false; // nothing to call
            if (bytes < sizeof(packet_id_t)) return false; // prevent underflow
            if (bytes - sizeof(packet_id_t) != callback.size) return false; // byte mismatch

            callback.invoke(std::span<uint8_t> {
                payload.data() + sizeof(packet_id_t),
                callback.size
            });

            return true;
        }

        template<typename T, auto Fn>
        void register_callback() {
            static_assert(contains<T>(), "T must be registered in common!");

            auto& callback = m_callback[get_type_of<T>()];
            
            callback.size = T::payload_size();
            callback.invoke = [](std::span<uint8_t> payload) {
                T obj = T::deserialise(payload);
                Fn(obj);
            };
        }

        std::optional<size_t> expected_payload_size(packet_id_t id) const {
            if (id >= m_callback.size()) return std::nullopt;
            return std::make_optional(m_callback[id].size);
        }
        
        template<typename T>
        constexpr size_t packet_size() {
            return sizeof(packet_id_t) + T::payload_size();
        }

    private:
        std::array<callback_info_t, sizeof...(Ts)> m_callback;
    };
}