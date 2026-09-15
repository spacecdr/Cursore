#include "button.h"
#include "microphone.h"
#include "led_state.h"
#include "server_client.h"
#include "wifi.h"

#include "esp_log.h"

static const char *TAG = "cursore";

void app_main(void)
{
    ESP_LOGI(TAG, "Cursore standalone boot; original Atom Echo / ESP32-PICO-D4");

    led_state_init();
    led_state_set(CURSORE_STATE_READY);
    button_init();
    wifi_init();
    server_health_task_start();
    microphone_init();
}
