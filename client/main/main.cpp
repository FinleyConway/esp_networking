#include <esp_err.h>
#include <esp_log.h>

#include "networking/wifi/wifi.hpp"
#include "networking/wifi/provisioning/provisioning.hpp"
#include "esp_https_server.h"

#include "task_events/tcp_send_event.hpp"
#include "tasks/tcp_task.hpp"

void connection_handle(bool has_connected) {
    if (has_connected) {
        ESP_LOGI("main", "Connected!");

        // restart the esp to take advantage of nvs
        esp_restart();
    }
    else {
        ESP_LOGI("main", "Given creds werent correct, try again!");
    }
}

extern "C" void app_main() {
    static client::wifi_t wifi;

    ESP_ERROR_CHECK(wifi.connect_from_nvs());

    if (!wifi.wait_connection()) {
        client::provisioning_t wifi_prov(wifi);

        wifi_prov.start("esp_device", "", 1);
        wifi_prov.wait_connection(connection_handle);
    }

    ESP_LOGI("main", "connected");

    client::tcp_send_event_t::create(10);
    client::tcp_task_t::init(2);
}