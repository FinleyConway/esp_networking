#pragma once

#include <esp_err.h>
#include <esp_log.h>

#include "networking/wifi/wifi.hpp"
#include "networking/wifi/wifi_provisioning_http.hpp"

namespace client {
    class wifi_provisioning_t {
    public:
        explicit wifi_provisioning_t(wifi_t& wifi) : m_wifi(wifi) {}

        esp_err_t start(const char* ap_ssid, const char* ap_password, uint8_t max_connections) {
            esp_err_t ret = start_softap(ap_ssid, ap_password, max_connections);
            if (ret != ESP_OK) {
                ESP_LOGE(c_tag, "Failed to start softap!");
                
                return ret;
            }

            return m_http.try_start();
        }

        template<typename Fn>
        void try_connect(Fn&& fn) {
            while (true) {
                if (m_http.has_credentials()) {
                    ESP_ERROR_CHECK(try_station_connect(m_http.get_ssid(), m_http.get_password()));

                    if (wait_connection()) {
                        ESP_ERROR_CHECK(m_http.try_stop());
                        ESP_ERROR_CHECK(stop());

                        ESP_LOGI(c_tag, "Successfully connected!");

                        std::forward<Fn>(fn)(true);
                    } 
                    else {
                        std::forward<Fn>(fn)(false);

                        m_http.reset_credentials_check();
                    }
                }
                else {
                    m_http.reset_credentials_check();
                }
            }
        }

    private:
        esp_err_t start_softap(const char* ssid, const char* password, uint8_t max_connections) {
            ESP_LOGI("WIFI_PROV", "Starting provisioning");

            ESP_ERROR_CHECK(m_wifi.set_mode(wifi_mode_t::sta_ap));
            ESP_ERROR_CHECK(m_wifi.set_softap_config(ssid, password, max_connections));

            return m_wifi.start();
        }

        esp_err_t try_station_connect(const char* ssid, const char* password) {
            if (m_wifi.get_mode() != wifi_mode_t::sta_ap) return ESP_FAIL;

            ESP_LOGI("WIFI_PROV", "Starting station provisioning");

            ESP_ERROR_CHECK(m_wifi.set_sta_config(ssid, password));
            m_wifi.reset_retry();
            
            return esp_wifi_connect();
        }

        bool wait_connection() {
            ESP_LOGI("WIFI_PROV", "Trying to connect");

            return m_wifi.wait_connection();
        }

        esp_err_t stop() {
            ESP_LOGI("WIFI_PROV", "Stopping provisioning");

            m_wifi.disconnect();
            m_wifi.reset_retry();

            return m_wifi.set_mode(wifi_mode_t::none);
        }

    private:
        static constexpr const char* c_tag = "wifi_provisioning";

        wifi_t& m_wifi;
        wifi_provisioning_http_t m_http;
    };
}