#include "led_state.h"
#include "config.h"
#include "esp_log.h"
#include "led_strip.h"
#include <stdint.h>
static const char *TAG = "led";
static led_strip_handle_t s_strip;
static cursore_state_t s_state = CURSORE_STATE_MUTED;
static void color_for_state(cursore_state_t state, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = *g = *b = 0;
    switch (state) {
    case CURSORE_STATE_MUTED: *r = 32; break;
    case CURSORE_STATE_READY: *g = 32; break;
    case CURSORE_STATE_LISTENING: *r = 32; *g = 24; break;
    case CURSORE_STATE_PROCESSING: *b = 32; break;
    case CURSORE_STATE_SPEAKING: *r = 24; *b = 32; break;
    case CURSORE_STATE_ERROR: *r = 64; break;
    }
}
void led_state_init(void)
{
    led_strip_config_t strip_cfg = {.strip_gpio_num = ATOM_ECHO_LED_GPIO, .max_leds = 1, .led_model = LED_MODEL_SK6812, .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, .flags.invert_out = false};
    led_strip_rmt_config_t rmt_cfg = {.resolution_hz = 10 * 1000 * 1000, .flags.with_dma = false};
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip));
    led_state_set(CURSORE_STATE_MUTED);
    ESP_LOGI(TAG, "SK6812 initialized on GPIO %d", ATOM_ECHO_LED_GPIO);
}
void led_state_set(cursore_state_t state)
{
    uint8_t r, g, b; s_state = state; color_for_state(state, &r, &g, &b);
    if (s_strip) { ESP_ERROR_CHECK(led_strip_set_pixel(s_strip, 0, r, g, b)); ESP_ERROR_CHECK(led_strip_refresh(s_strip)); }
}
cursore_state_t led_state_get(void) { return s_state; }
