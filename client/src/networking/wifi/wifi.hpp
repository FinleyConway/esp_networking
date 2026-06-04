#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>

#include "networking/wifi/wifi_config.hpp"

namespace client {
    enum class wifi_mode_t {
        none,
        sta,
        sta_ap
    };

    class wifi_t {
    public:
        explicit wifi_t(const uint8_t retry_attempts = 3) : c_retry_attempts(retry_attempts) {
            // TODO: This could be in its own section/file?
            // initialize nvs
            esp_err_t ret = nvs_flash_init();
            // erase and reinit if full/corrupted or is nvs new format
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ret = nvs_flash_init();
            }
            ESP_ERROR_CHECK(ret);

            m_event_group = xEventGroupCreate();
            if (m_event_group == nullptr) {
                ESP_LOGE("WIFI", "Can't create event group!");
                abort();
            }

            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());

            ESP_ERROR_CHECK(esp_event_handler_instance_register(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &wifi_event,
                this,
                &m_wifi_event_handler
            ));
            ESP_ERROR_CHECK(esp_event_handler_instance_register(
                IP_EVENT,
                ESP_EVENT_ANY_ID,
                &ip_event,
                this,
                &m_ip_event_handler
            ));

            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        }

        ~wifi_t() {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_deinit());

            if (m_ap_netif != nullptr) {
                esp_netif_destroy(m_ap_netif);
                m_ap_netif = nullptr;
            }

            if (m_sta_netif != nullptr) {
                esp_netif_destroy(m_sta_netif);
                m_sta_netif = nullptr;
            }

            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_unregister(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                m_wifi_event_handler
            ));
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_unregister(
                IP_EVENT,
                ESP_EVENT_ANY_ID,
                m_ip_event_handler
            ));

            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_delete_default());

            vEventGroupDelete(m_event_group);
        }

        esp_err_t start() {
            return esp_wifi_start();
        }

        esp_err_t stop() {
            return esp_wifi_stop();
        }

        esp_err_t set_mode(wifi_mode_t mode) {
            if (m_mode == mode) {
                return ESP_OK;
            }

            // must stop the wifi before switching modes
            if (m_mode != wifi_mode_t::none) {
                ESP_ERROR_CHECK(esp_wifi_stop());
            }

            handle_create_netif(mode);
            m_mode = mode;

            switch (mode) {
                case (wifi_mode_t::none):   return esp_wifi_set_mode(WIFI_MODE_NULL);
                case (wifi_mode_t::sta):    return esp_wifi_set_mode(WIFI_MODE_STA);
                case (wifi_mode_t::sta_ap): return esp_wifi_set_mode(WIFI_MODE_APSTA);
            }

            ESP_LOGE(c_tag, "Can't set wifi mode!");

            return ESP_FAIL;
        }

        wifi_mode_t get_mode() const {
            return m_mode;
        }

        esp_err_t set_sta_config(const char* ssid, const char* password) {
            return wifi_config_t::init_station(ssid, password);
        }

        esp_err_t set_softap_config(const char* ssid, const char* password, uint8_t max_connections) {
            return wifi_config_t::init_softap(ssid, password, max_connections);
        }

        esp_err_t connect_from_nvs() {
            ::wifi_config_t cfg;

            ESP_ERROR_CHECK(set_mode(wifi_mode_t::sta));
            ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &cfg));

            reset_retry();

            return esp_wifi_start();
        }

        esp_err_t disconnect() {
            return esp_wifi_disconnect();
        }

        bool wait_connection() {
            ESP_LOGI(c_tag, "Trying to connect");

            EventBits_t bits = xEventGroupWaitBits(
                m_event_group,
                c_connected_bit | c_fail_bit,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY
            );

            return (bits & c_connected_bit);
        }

        void reset_retry() {
            m_retry_count = 0;
            xEventGroupClearBits(m_event_group, c_connected_bit | c_fail_bit);
        }

    private:
        void handle_create_netif(wifi_mode_t mode) {
            if (mode == wifi_mode_t::sta || mode == wifi_mode_t::sta_ap) {
                if (!m_sta_netif) m_sta_netif = esp_netif_create_default_wifi_sta();
            }

            if (mode == wifi_mode_t::sta_ap) {
                if (!m_ap_netif) m_ap_netif = esp_netif_create_default_wifi_ap();
            }
        }

        static void wifi_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
            auto* self = static_cast<wifi_t*>(arg);

            if (event_id == WIFI_EVENT_STA_START) {
                if (esp_wifi_connect() != ESP_OK) {
                    ESP_LOGE(c_tag, "Failed to connect!");
                    xEventGroupSetBits(self->m_event_group, c_fail_bit);
                }
            }

            if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                if (self->m_retry_count < self->c_retry_attempts) {
                    ESP_LOGW(c_tag, "Failed to connect, retrying...");

                    self->m_retry_count++;
                    esp_wifi_connect();

                    return;
                }

                ESP_LOGE(c_tag, "Failed to connect!");
                xEventGroupSetBits(self->m_event_group, c_fail_bit);
            }
        }

        static void ip_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
            auto* self = static_cast<wifi_t*>(arg);

            if (event_id == IP_EVENT_STA_GOT_IP) {
                self->m_retry_count = 0;
                xEventGroupSetBits(self->m_event_group, c_connected_bit);
            }
        }

    private:
        static constexpr const char* c_tag = "wifi";

        wifi_mode_t m_mode = wifi_mode_t::none;

        EventGroupHandle_t m_event_group = nullptr;
        esp_event_handler_instance_t m_wifi_event_handler = nullptr;
        esp_event_handler_instance_t m_ip_event_handler = nullptr;
        static constexpr EventBits_t c_connected_bit = BIT0;
        static constexpr EventBits_t c_fail_bit = BIT1;

        esp_netif_t* m_ap_netif = nullptr;
        esp_netif_t* m_sta_netif = nullptr;

        uint8_t m_retry_count = 0;
        const uint8_t c_retry_attempts = 3;
    };
}