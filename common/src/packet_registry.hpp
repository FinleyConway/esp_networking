#pragma once

#include <span>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <algorithm> 

namespace common {
    enum class packet_error_t {
        invalid_packet,
        invalid_packet_id,
        invalid_packet_bytes
    };

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
    private:
        using packet_id_t = uint16_t;
        using packet_bytes_view = std::span<uint8_t>;
        using callback_fn = void(*)(packet_bytes_view);

    public:
        template<typename T, typename Fn>
        void send(const T& data, Fn&& fn) {
            static_assert(has_type<T>(), "T must be in Ts...");

            constexpr packet_id_t packet_id = get_type_id<T>();
            constexpr size_t packet_id_size = sizeof(packet_id_t);
            constexpr size_t packet_size = get_max_bytes() + packet_id_size;

            // set up buffer and data 
            typename T::net_t net_data = T::to_net(data);
            std::array<uint8_t, packet_size> buffer;

            // copy data into the buffer
            std::memcpy(buffer.data(), &packet_id, packet_id_size); 
            std::memcpy(buffer.data() + packet_id_size, &net_data, sizeof(net_data)); 

            // give the network ownership of the buffer for async use
            std::forward<Fn>(fn)(std::move(buffer));
        }

        template<typename Fn>
        packet_error_t listen(Fn&& fn) {
            constexpr size_t packet_id_size = sizeof(packet_id_t);
            constexpr size_t packet_size = get_max_bytes() + packet_id_size;
            
            // set up buffer and receive bytes from network
            std::array<uint8_t, packet_size> buffer;
            size_t bytes_received = std::forward<Fn>(fn)(buffer);

            // have we gotten the minimum bytes needed?
            if (bytes_received < packet_id_size) {
                return packet_error_t::invalid_packet;
            }

            // get the id and perform a lookup to callback network for type that was received
            packet_id_t packet_id = 0;
            std::memcpy(&packet_id, buffer.data(), packet_id_size);

            // have we gotten a valid id?
            if (packet_id >= m_callback.size()) {
                return packet_error_t::invalid_packet_id;
            }

            // call the callback if one has been registered
            auto& callback = m_callback[packet_id];

            if (callback.invoker != nullptr) {
                // have we received the correct size of bytes?
                if (bytes_received != packet_id_size + callback.bytes) {
                    return packet_error_t::invalid_packet_bytes;
                }

                callback.invoker(packet_bytes_view(buffer.data() + packet_id_size, callback.bytes));
            }
        }

        template<typename T, auto Fn>
        void register_callback() {
            static_assert(has_type<T>(), "T must be in Ts...");

            auto& callback = m_callback[get_type_id<T>()];
            
            callback.bytes = sizeof(typename T::net_t);
            callback.invoker = [](packet_bytes_view bytes) {
                typename T::net_t net_data{};
                std::memcpy(&net_data, bytes.data(), sizeof(net_data));

                T obj = T::from_net(net_data);
                Fn(obj);
            };
        }

        static consteval size_t get_max_payload() {
            return get_max_bytes() + sizeof(packet_id_t);
        }

    private:
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

        template<typename T>
        static consteval size_t get_type_id() {
            return type_index_impl<T, Ts...>();
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
    };
}