#include "networking/wifi/provisioning/provisioning_http.hpp"

#include <memory>
#include <cstring>

namespace client {
    esp_err_t provisioning_http_t::try_start() {
        m_event_group = xEventGroupCreate();
        if (m_event_group == nullptr) {
            return ESP_FAIL;
        }
        reset_credentials_check();

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        esp_err_t ret = httpd_start(&m_http_handle, &config);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Failed to create http server");

            return ret;
        }

        m_wifi_cred_post_uri = {
            .uri       = "/wifi_cred/connect",               
            .method    = HTTP_POST,        
            .handler   = wifi_cred_post_handler, 
            .user_ctx  = this            
        };

        ret = httpd_register_uri_handler(m_http_handle, &m_wifi_cred_post_uri);
            if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Failed to register POST URI handler");
            ESP_ERROR_CHECK(try_stop());

            return ret;
        }

        m_wifi_cred_get_uri = {
            .uri       = "/wifi_cred/status",               
            .method    = HTTP_GET,        
            .handler   = wifi_cred_get_handler, 
            .user_ctx  = this            
        };

        ret = httpd_register_uri_handler(m_http_handle, &m_wifi_cred_get_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Failed to register GET URI handler");
            ESP_ERROR_CHECK(try_stop());

            return ret;
        }

        m_ping_get_uri = {
            .uri       = "/ping",               
            .method    = HTTP_GET,        
            .handler   = esp_ping_handler, 
            .user_ctx  = nullptr
        };

        ret = httpd_register_uri_handler(m_http_handle, &m_ping_get_uri);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Failed to register ping URI handler");
            ESP_ERROR_CHECK(try_stop());

            return ret;
        }

        return ESP_OK;
    }

    bool provisioning_http_t::has_credentials() {
        EventBits_t bits = xEventGroupWaitBits(
            m_event_group,
            c_received_bit | c_fail_bit,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        if (bits & c_received_bit) {
            return true;
        }

        reset_credentials_check();

        return false;
    }

    void provisioning_http_t::set_status(provisioning_status_t::value state) {
        m_status = state;
    }

    bool provisioning_http_t::has_ack_connection() {
        EventBits_t bits = xEventGroupWaitBits(
            m_event_group,
            c_ack_bit,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(m_ack_timeout_ms) // prevent hanging if client doesnt respond
        );

        if (bits & c_ack_bit) {
            return true;
        }

        reset_credentials_check();

        return false;
    }

    void provisioning_http_t::reset_credentials_check() {
        xEventGroupClearBits(m_event_group, c_received_bit | c_fail_bit | c_ack_bit);
    }

    esp_err_t provisioning_http_t::try_stop() {
        if (m_http_handle == nullptr) return ESP_FAIL;

        vEventGroupDelete(m_event_group);
        m_event_group = nullptr;

        esp_err_t ret = httpd_stop(m_http_handle);
        m_http_handle = nullptr;

        return ret;
    }

    const char* provisioning_http_t::get_ssid() const {
        return m_ssid;
    }

    const char* provisioning_http_t::get_password() const {
        return m_password;
    }

    esp_err_t provisioning_http_t::wifi_cred_post_handler(httpd_req_t* req) {
        ESP_LOGI(c_tag, "Receiving wifi creds from http");

        auto* self = static_cast<provisioning_http_t*>(req->user_ctx);

        size_t content_length = req->content_len;
        if (content_length == 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");

            return ESP_FAIL;
        }

        auto buffer = std::make_unique<char[]>(content_length + 1);
        if (buffer == nullptr) {
            ESP_LOGE("HTTP", "Failed to allocate memory of %d bytes", req->content_len + 1);

            httpd_resp_send_500(req);
            xEventGroupSetBits(self->m_event_group, c_fail_bit);
            self->fail();

            return ESP_FAIL;
        }

        esp_err_t ret = self->get_body(buffer.get(), req);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Fail to read incoming body request");
            self->fail();

            return ret;
        }

        ret = self->parse_body(buffer.get(), req);
        if (ret != ESP_OK) {
            ESP_LOGE(c_tag, "Fail to parse incoming body");
            self->fail();

            return ret;
        }

        return httpd_resp_sendstr(req, "OK");
    }

    esp_err_t provisioning_http_t::wifi_cred_get_handler(httpd_req_t* req) {
        ESP_LOGI(c_tag, "Receiving wifi status from http");

        auto* self = static_cast<provisioning_http_t*>(req->user_ctx);

        httpd_resp_set_type(req, "application/json");

        const char* status_str = provisioning_status_t::to_str(self->m_status);
        
        // put status inside response
        char response[128];
        snprintf(
            response, 
            sizeof(response),
            R"({"status":"%s"})",
            status_str
        );

        if (self->m_status == provisioning_status_t::connected) {
            xEventGroupSetBits(self->m_event_group, c_ack_bit);
        }

        return httpd_resp_sendstr(req, response);
    }

    esp_err_t provisioning_http_t::esp_ping_handler(httpd_req_t* req) {
        ESP_LOGI(c_tag, "Receiving ping from http");

        return httpd_resp_sendstr(req, "OK");
    }

    esp_err_t provisioning_http_t::get_body(char* buffer, httpd_req_t* req) {
        size_t offset = 0;

        while (offset < req->content_len) {
            int bytes_recv = httpd_req_recv(req, buffer + offset, req->content_len - offset);

            if (bytes_recv <= 0) {
                if (bytes_recv == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                }

                xEventGroupSetBits(m_event_group, c_fail_bit);

                return ESP_FAIL;
            }

            offset += bytes_recv;
        }
        buffer[offset] = '\0';

        return ESP_OK;
    }

    esp_err_t provisioning_http_t::parse_body(const char* buffer, httpd_req_t* req) {
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
            cJSON_Parse(buffer),
            cJSON_Delete
        );

        if (root == nullptr) {
            ESP_LOGE("HTTP", "Invalid JSON");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

            return ESP_FAIL;
        }

        cJSON* ssid = cJSON_GetObjectItem(root.get(), "ssid");
        cJSON* password = cJSON_GetObjectItem(root.get(), "password");

        if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
            ESP_LOGE(c_tag, "Missing fields");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");

            return ESP_FAIL;
        }

        return store_credentials(ssid->valuestring, password->valuestring, req);
    }

    esp_err_t provisioning_http_t::store_credentials(const char* ssid, const char* password,  httpd_req_t* req) {
        size_t ssid_len = strlen(ssid);
        size_t pass_len = strlen(password);

        if (ssid_len >= sizeof(m_ssid)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID too long");

            return ESP_FAIL;
        }

        if (pass_len >= sizeof(m_password)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password too long");

            return ESP_FAIL;
        }

        strlcpy(m_ssid, ssid, sizeof(m_ssid));
        strlcpy(m_password, password, sizeof(m_password));

        xEventGroupSetBits(m_event_group, c_received_bit);

        return ESP_OK;
    }

    void provisioning_http_t::fail() { 
        xEventGroupSetBits(m_event_group, c_fail_bit);
    }
}