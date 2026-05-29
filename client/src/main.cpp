#include <nvs_flash.h>

#include "networking/wifi.hpp"

#include "task_events/tcp_send_event.hpp"
#include "tasks/tcp_task.hpp"

extern "C" void app_main() {
    nvs_flash_init();

    client::wifi_status_t status = client::wifi_boot_t::connect();

    if (status == client::wifi_status_t::failure) {
        ESP_LOGI("ESP_MAIN", "Failed to associate to AP, dying...");

        return;
    }

    client::tcp_send_event_t::create(10);
    client::tcp_task_t::init(2);
}