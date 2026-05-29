#pragma once

#include <freertos/FreeRTOS.h>

#include "task_events/tcp_send_event.hpp"
#include "networking/tcp_client.hpp"

namespace client {
    class tcp_task_t {
    public:
        static void init(UBaseType_t priority_offset) {
            xTaskCreate(
                run,
                "tcp_task_t",
                s_stack_size,
                nullptr,
                tskIDLE_PRIORITY + priority_offset,
                &s_handle
            );
        }
        
        static TaskHandle_t get_handle() {
            return s_handle;
        }

    private:
        static void run(void* parameters) {
            tcp_client_t client;

            register_messages(client);

            while (true) {
                try_connect(client);

                while (client.is_connected()) {
                    if (!handle_send(client)) {
                        break;
                    }

                    tcp_status_t status = client.listen_to_server();

                    if (status == tcp_status_t::unknown_packet) {
                        // assert, message wasnt registerd
                        // I should also really add logs for more context with errno.....
                        ESP_LOGI("TCP_TASK", "Received an unknown messsage?");
                        continue;
                    }
                    
                    if (status == tcp_status_t::failure || status == tcp_status_t::connection_closed) {
                        break;
                    }
                }

                ESP_LOGI("TCP_TASK", "Disconnected");

                client.disconnect();
            }

            vTaskDelete(nullptr);
        }

        static void register_messages(tcp_client_t& client) {
            client.register_receieve_callback<common::esp_init_request_t, &on_init_request>();
        }

        static void try_connect(tcp_client_t& client) {
            // TODO: Add retry attempts

            constexpr TickType_t retry_delay = pdMS_TO_TICKS(5000);
            constexpr int32_t tcp_timeout_sec = 5;
            
            ESP_LOGI("TCP_TASK", "Connecting...");

            while (client.try_connect(tcp_timeout_sec) != tcp_status_t::success) {
                vTaskDelay(retry_delay);
            }

            ESP_LOGI("TCP_TASK", "Connected");
        }

        static bool handle_send(tcp_client_t& client) {
            tcp_status_t status{};
            tcp_event_data_t event_data;

            if (tcp_send_event_t::receive(event_data, 0)) {
                switch (event_data.type) {
                    case tcp_event_data_t::type_t::init_respond:
                        status = client.send_to_server(event_data.init_respond);
                        break;
                }

                if (status != tcp_status_t::success) {
                    return false;
                }
            }

            return true;
        }

    private:
        static void on_init_request(const common::esp_init_request_t& init_request) {
            ESP_LOGI("TCP_TASK", "Received server response, sending ack...");

            tcp_send_event_t::send(tcp_event_data_t {
                .type = tcp_event_data_t::type_t::init_respond,
                .init_respond = {
                    .id = init_request.id
                }
            });

            // store id somewhere
        }

    private:
        inline static TaskHandle_t s_handle = nullptr;
        inline static uint32_t s_stack_size = 8192;
    };        
}