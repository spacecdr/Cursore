#include "button.h"
#include "config.h"
#include "led_state.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
static const char *TAG = "button";
static void button_task(void *arg)
{
    bool last = true; TickType_t pressed_at = 0;
    for (;;) {
        bool now = gpio_get_level(ATOM_ECHO_BUTTON_GPIO) != 0;
        if (last && !now) pressed_at = xTaskGetTickCount();
        TickType_t held = xTaskGetTickCount() - pressed_at;
        if (!last && now && held >= pdMS_TO_TICKS(30) && held <= pdMS_TO_TICKS(1000)) {
            cursore_state_t state = led_state_get();
            led_state_set(state == CURSORE_STATE_MUTED ? CURSORE_STATE_READY : CURSORE_STATE_MUTED);
            ESP_LOGI(TAG, "assistant %s", led_state_get() == CURSORE_STATE_MUTED ? "MUTED" : "ENABLED");
        }
        last = now; vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void button_init(void)
{
    gpio_config_t cfg = {.pin_bit_mask = 1ULL << ATOM_ECHO_BUTTON_GPIO, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&cfg));
    xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
}
