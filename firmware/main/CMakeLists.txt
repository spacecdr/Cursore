idf_component_register(
    SRCS "main.c" "wifi.c" "led_state.c" "button.c" "server_client.c" "microphone.c"
        "speaker.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif esp_http_client cjson esp_driver_gpio esp_driver_i2s nvs_flash
)
