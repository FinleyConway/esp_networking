#pragma once

#include <utility>

#include <esp_err.h>
#include <esp_log.h>

#include "networking/wifi/wifi.hpp"
#include "networking/wifi/provisioning/provisioning_http.hpp"
#include "networking/wifi/provisioning/provisioning_status.hpp"

namespace client {
    class provisioning_t {
    public:
        explicit provisioning_t(wifi_t& wifi);

        esp_err_t start(const char* ap_ssid, const char* ap_password, uint8_t max_connections);

        template<typename Fn>
        void wait_connection(Fn&& fn) {
            while (true) {
                // handle when no cred is provided and retry
                if (!m_http.has_credentials()) {
                    m_http.reset_credentials_check();
                    m_http.set_status(provisioning_status_t::bad_credentials);

                    continue;
                }

                ESP_ERROR_CHECK(try_station_connect(m_http.get_ssid(), m_http.get_password()));

                m_http.set_status(provisioning_status_t::connecting);

                // handle when cred are wrong and retry
                if (!m_wifi.wait_connection()) {
                    m_http.reset_credentials_check();
                    m_http.set_status(provisioning_status_t::bad_credentials);

                    // provide callback
                    std::forward<Fn>(fn)(false);

                    continue;
                }

                // connected successfully
                m_http.set_status(provisioning_status_t::connected);

                if (m_http.has_ack_connection()) {
                    ESP_ERROR_CHECK(m_http.try_stop());
                    ESP_ERROR_CHECK(stop());

                    ESP_LOGI(c_tag, "Successfully connected!");

                    // provide callback
                    std::forward<Fn>(fn)(true);

                    break;
                }
            }
        }

    private:
        esp_err_t start_softap(const char* ssid, const char* password, uint8_t max_connections);

        esp_err_t try_station_connect(const char* ssid, const char* password);

        esp_err_t stop();

    private:
        static constexpr const char* c_tag = "wifi_provisioning";

        wifi_t& m_wifi;
        provisioning_http_t m_http;
    };
}