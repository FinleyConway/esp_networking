#include "networking/wifi/provisioning/provisioning.hpp"

namespace client {
    provisioning_t::provisioning_t(wifi_t& wifi) : m_wifi(wifi) {}

    esp_err_t provisioning_t::start(const char* ap_ssid, const char* ap_password, uint8_t max_connections) {
        ESP_LOGI(c_tag, "Starting provisioning");

        esp_err_t ret = start_softap(ap_ssid, ap_password, max_connections);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Failed to start softap!");
            
            return ret;
        }

        return m_http.try_start();
    }

    esp_err_t provisioning_t::start_softap(const char* ssid, const char* password, uint8_t max_connections) {
        ESP_ERROR_CHECK(m_wifi.set_mode(wifi_mode_t::sta_ap));
        ESP_ERROR_CHECK(m_wifi.set_softap_config(ssid, password, max_connections));

        return m_wifi.start();
    }

    esp_err_t provisioning_t::try_station_connect(const char* ssid, const char* password) {
        if (m_wifi.get_mode() != wifi_mode_t::sta_ap) return ESP_FAIL;

        ESP_LOGI(c_tag, "Starting station provisioning");

        ESP_ERROR_CHECK(m_wifi.set_sta_config(ssid, password));
        m_wifi.reset_retry();
        
        return esp_wifi_connect();
    }

    esp_err_t provisioning_t::stop() {
        ESP_LOGI(c_tag, "Stopping provisioning");

        m_wifi.disconnect();
        m_wifi.reset_retry();

        return m_wifi.set_mode(wifi_mode_t::none);
    }
}