#include "server_client.h"
#include "config.h"
#include "wifi.h"
#include "led_state.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
static const char *TAG = "server";
static esp_err_t health_event(esp_http_client_event_t *evt) { return ESP_OK; }
static bool health_check(void)
{
    char url[96]; snprintf(url, sizeof(url), "http://%s:%d%s", CURSORE_SERVER_HOST, CURSORE_SERVER_PORT, CURSORE_HEALTH_PATH);
    esp_http_client_config_t cfg = {.url = url, .method = HTTP_METHOD_GET, .timeout_ms = 4000, .event_handler = health_event};
    esp_http_client_handle_t client = esp_http_client_init(&cfg); if (!client) return false;
    esp_err_t err = esp_http_client_perform(client); int status = esp_http_client_get_status_code(client); int length = esp_http_client_get_content_length(client);
    char body[192] = {0}; bool valid = false;
    if (err == ESP_OK && status == 200 && length > 0 && length < (int)sizeof(body)) {
        int read = esp_http_client_read_response(client, body, sizeof(body) - 1); cJSON *json = cJSON_ParseWithLength(body, read);
        cJSON *status_item = json ? cJSON_GetObjectItem(json, "status") : NULL;
        valid = status_item && cJSON_IsString(status_item) && strcmp(status_item->valuestring, "ok") == 0; cJSON_Delete(json);
    }
    esp_http_client_cleanup(client); return valid;
}
static void health_task(void *arg)
{
    for (;;) {
        if (wifi_is_connected()) { bool ok = health_check(); ESP_LOGI(TAG, "GET /health: %s", ok ? "HTTP 200 JSON ok" : "error"); if (led_state_get() != CURSORE_STATE_MUTED) led_state_set(ok ? CURSORE_STATE_READY : CURSORE_STATE_ERROR); if (!ok) { vTaskDelay(pdMS_TO_TICKS(1000)); if (led_state_get() == CURSORE_STATE_ERROR) led_state_set(CURSORE_STATE_READY); } }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
void server_health_task_start(void) { xTaskCreate(health_task, "health", 4096, NULL, 4, NULL); }
