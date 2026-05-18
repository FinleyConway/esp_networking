#include <driver/gpio.h>
#include <nvs_flash.h>

#include "networking/wifi.hpp"
#include "networking/tcp_client.hpp"
#include "service_config.hpp"

void init_gpio() {
    gpio_reset_pin(GPIO_NUM_27);
    gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
}

void on_esp_init(common::init_esp_t) {
    static bool state = false;

    state = !state;

    gpio_set_level(GPIO_NUM_27, state);
}

extern "C" void app_main() {
    nvs_flash_init();
    init_gpio();

    client::wifi_status_t status = client::wifi_boot_t::connect();

    if (status == client::wifi_status_t::failure) {
        ESP_LOGI("ESP_MAIN", "Failed to associate to AP, dying...");

        return;
    }

    client::tcp_client_t c;
    
    c.register_receieve_callback<common::init_esp_t, &on_esp_init>();

    if (c.try_connect() == client::tcp_status_t::success) { 

        ESP_LOGI("ESP_MAIN", "Connected");

        while (c.is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(10));

            auto status = c.listen_to_server();

            if (status == client::tcp_status_t::success) {
                ESP_LOGI("ESP_MAIN", "Received!");
            }

            if (status == client::tcp_status_t::unknown_packet) {
                ESP_LOGI("ESP_MAIN", "Should probably assert this?");
            }

            if (status == client::tcp_status_t::failure) {
                ESP_LOGI("ESP_MAIN", "Fail");
            }
        }
    }
}