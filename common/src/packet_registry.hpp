#pragma once

#include <span>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <algorithm> 

namespace common {
    template<typename T>
    concept packet_type =
    requires(T obj, typename T::net_t net) {
        typename T::net_t;

        { T::to_net(obj) } -> std::same_as<typename T::net_t>;
        { T::from_net(net) } -> std::same_as<T>;
    }   
    && std::is_trivially_copyable_v<typename T::net_t>
    && std::is_standard_layout_v<typename T::net_t>;

    template<packet_type... Ts>
    class packet_registry_impl_t {
    public:
        using packet_id_t = uint16_t;
        using payload_t = std::array<uint8_t, packet_registry_impl_t<...Ts>::c_max_payload>;
        using payload_view_t = std::span<uint8_t>;

    public:
        template<typename T>
        payload_t create_payload(const T& data) {
            static_assert(has_type<T>(), "T must be in Ts...");

            // set up buffer and data 
            payload_t buffer;
            typename T::net_t net_data = T::to_net(data);

            // copy data into the buffer
            std::memcpy(buffer.data(), &get_type_id<T>(), c_packet_id_size); 
            std::memcpy(buffer.data() + c_packet_id_size, &net_data, sizeof(net_data)); 

            return buffer;
        }

        size_t get_packet_bytes(packet_id_t packed_id) const {
            if (packed_id >= m_callback.size()) {
                return 0;
            }

            return m_callback.at(packed_id).bytes;
        }
        
        template<typename T>
        size_t get_packet_bytes() const {
            static_assert(has_type<T>(), "T must be in Ts...");

            return m_callback.at(get_type_id<T>()).bytes;
        }

        bool dispatch(packet_id_t packet_id, const payload_t& payload, size_t bytes_received) {
            // prevent invalid packet ids
            if (packet_id >= m_callback.size()) return false;

            // call the callback if one has been registered
            auto& callback = m_callback[packet_id];
            bool has_callback = callback.invoker != nullptr && callback.bytes != 0;
            bool do_bytes_match = callback.bytes == bytes_received - c_packet_id_size;

            if (has_callback) {
                if (!do_bytes_match) return false;

                callback.invoker(payload_view_t(
                    payload.data() + c_packet_id_size, // offset id bytes to just give payload
                    callback.bytes
                ));
            }

            return true;
        }

        template<typename T, auto Fn>
        void register_callback() {
            static_assert(has_type<T>(), "T must be in Ts...");

            auto& callback = m_callback[get_type_id<T>()];
            
            callback.bytes = sizeof(typename T::net_t);
            callback.invoker = [](payload_view_t bytes) {
                typename T::net_t net_data{};
                std::memcpy(&net_data, bytes.data(), sizeof(net_data));

                T obj = T::from_net(net_data);
                Fn(obj);
            };
        }

    private:
        using callback_fn = void(*)(payload_view_t);

        struct callback_info_t {
            callback_fn invoker = nullptr; 
            size_t bytes = 0;
        };

    private:
        template<typename T, typename First, typename... Rest>
        static consteval size_t type_index_impl(size_t idx = 0) {
            if constexpr (std::is_same_v<T, First>)
                return idx;
            else {
                static_assert(sizeof...(Rest) > 0, "type not found");
                return type_index_impl<T, Rest...>(idx + 1);
            }
        }

        static consteval size_t get_max_bytes() {
            size_t max = 0;
            ((max = std::max(max, sizeof(typename Ts::net_t))), ...);
            return max;
        }

        template<typename T>
        static consteval bool has_type() {
            return (std::is_same_v<T, Ts> || ...);
        }

    private:
        std::array<callback_info_t, sizeof...(Ts)> m_callback;

        static constexpr size_t c_packet_id_size = sizeof(packet_id_t);
        static constexpr size_t c_max_payload = get_max_bytes() + c_packet_id_size;
    };
}