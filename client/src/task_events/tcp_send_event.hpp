#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "common/messages/handshake.hpp"

namespace client {
    struct tcp_event_data_t {
        enum class type_t {
            init_respond
        } type;

        common::esp_init_response_t init_respond;
    };

    class tcp_send_event_t {
    public:
        static void create(UBaseType_t queue_size) {
            if (s_event != nullptr) {
                ESP_LOGI("TCP_EVENT", "Event is already created!");
                return;
            }

            s_event = xQueueCreate(queue_size, sizeof(tcp_event_data_t));

            if (s_event == nullptr) {
                ESP_LOGI("TCP_EVENT", "Failed to create event!");
            }
        }

        static void send(const tcp_event_data_t& data, TickType_t block_tick = pdMS_TO_TICKS(10)) {
            if (s_event == nullptr) {
                ESP_LOGI("TCP_EVENT", "Event not created to send");
                return;
            } 

            xQueueSend(s_event, &data, block_tick);
        }

        static bool receive(tcp_event_data_t& data_ref, TickType_t timeout_tick = portMAX_DELAY) {
            if (s_event == nullptr) {
                ESP_LOGI("TCP_EVENT", "Event not created to receive");
                return;
            } 

            return xQueueReceive(s_event, &data_ref, timeout_tick) == pdPASS;
        }

    private:
        inline static QueueHandle_t s_event = nullptr;
    };
}