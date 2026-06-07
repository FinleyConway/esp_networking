#pragma once

#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <freertos/event_groups.h>

#include <cJSON.h>

#include "networking/wifi/provisioning/provisioning_status.hpp"

namespace client {
    class provisioning_http_t {
    public:
        esp_err_t try_start();

        bool has_credentials();

        void set_status(provisioning_status_t::value state);

        bool has_ack_connection();

        void reset_credentials_check();

        esp_err_t try_stop();

        const char* get_ssid() const;

        const char* get_password() const;

    private:
        static esp_err_t wifi_cred_post_handler(httpd_req_t* req);

        static esp_err_t wifi_cred_get_handler(httpd_req_t* req);

        static esp_err_t esp_ping_handler(httpd_req_t* req);

    private:
        esp_err_t get_body(char* buffer, httpd_req_t* req);

        esp_err_t parse_body(const char* buffer, httpd_req_t* req);

        esp_err_t store_credentials(const char* ssid, const char* password,  httpd_req_t* req);

        void fail();

    private:
        static constexpr const char* c_tag = "http";

        httpd_handle_t m_http_handle = nullptr;
        httpd_uri_t m_wifi_cred_post_uri;
        httpd_uri_t m_wifi_cred_get_uri;
        httpd_uri_t m_ping_get_uri;

        EventGroupHandle_t m_event_group = nullptr;
        static constexpr EventBits_t c_received_bit = BIT0;
        static constexpr EventBits_t c_fail_bit = BIT1;
        static constexpr EventBits_t c_ack_bit = BIT2;

        char m_ssid[32]{};
        char m_password[64]{};

        provisioning_status_t::value m_status = provisioning_status_t::none;
        TickType_t m_ack_timeout_ms = 10000; 
    };
}