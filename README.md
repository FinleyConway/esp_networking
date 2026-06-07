# esp_networking
A server-client system for communication between a PC server and multiple ESP32 clients simultaneously. It uses a compile-time TCP stream handler design, using [ASIO](https://github.com/chriskohlhoff/asio) for server networking and Unix socket communication and FreeRTOS for the ESP32s.

```c++
// Server
host::tcp_server_t server;
host::handshake_manager_t handshake(server, std::chrono::seconds(1)); // 1 sec timeout

server.register_on_connect([&](common::esp_id_t id) {
    handshake.on_connect(id);
});

server.register_on_disconnect([&](common::esp_id_t id) {
    LOG_INFO("ESP: {} disconnected", id);
});

server.register_receive_callback<common::esp_init_response_t>([&](const common::esp_init_response_t& res) {
    handshake.on_response_received(res);
});

// -------------------------------------------------------------------------------------------------------------

// Client
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

client::tcp_client_t client;
client.register_receieve_callback<common::esp_init_request_t, &on_init_request>();
client.listen_to_server();
```

## Building and Running
### Client
Uses esp-idf to build, flash and monitor esp32s.

Follow the [Get Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) provided by Espressif to get the tools needed to use this project.

```bash
# building project
idf.py build flash
idf.py monitor # to monitor logs being printed to console
```

#### Flashing WiFi credentials
During start if no WiFi credentials are not stored inside the nvs parition, it will start up a access point that a device e.g. mobile phone can connect to.

It exposes a http endpoint which expects a POST response:
```
# typically 192.168.4.1
192.168.4.1/wifi_cred
```

```
# body example
{
    "ssid": "network_name",
    "password": "super_secret_password"
}
```
It also exposes a GET response to know when the wifi has been connected (this must be called to let esp acknowledge of this connection):
```
{
    "connected": true
}
```

---
### Server:

### Windows (Not Support): See: [mdns_service.hpp](host/src/host/networking/mdns_service.hpp)

### Linux and Mac:
```bash
# Required for automatic network discovery using mdns.
pip install zeroconf
```

```bash
# Cloning and building project
git clone https://github.com/FinleyConway/esp_networking.git --recursive
cd esp_networking
cmake -S . -B build && cmake --build build

# Run server
./build/esp_networking/host
```

## Using Project
