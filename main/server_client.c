#include "server_client.h"
#include "config.h"
#include "wifi.h"
#include "led_state.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "server";

typedef struct {
    char body[256];
    size_t len;
} health_response_t;

static esp_err_t health_event(esp_http_client_event_t *evt)
{
    health_response_t *response = (health_response_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA &&
        response != NULL &&
        evt->data != NULL &&
        evt->data_len > 0) {

        size_t available = sizeof(response->body) - 1 - response->len;
        size_t copy_len = (size_t)evt->data_len;

        if (copy_len > available) {
            copy_len = available;
        }

        if (copy_len > 0) {
            memcpy(response->body + response->len, evt->data, copy_len);
            response->len += copy_len;
            response->body[response->len] = '\0';
        }
    }

    return ESP_OK;
}

static bool health_check(void)
{
    char url[96];

    snprintf(
        url,
        sizeof(url),
        "http://%s:%d%s",
        CURSORE_SERVER_HOST,
        CURSORE_SERVER_PORT,
        CURSORE_HEALTH_PATH
    );

    health_response_t response = {0};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 4000,
        .event_handler = health_event,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    if (client == NULL) {
        ESP_LOGE(TAG, "unable to initialize HTTP client");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    bool valid = false;

    if (err == ESP_OK && status == 200 && response.len > 0) {

        cJSON *json = cJSON_Parse(response.body);

        if (json != NULL) {
            cJSON *status_item = cJSON_GetObjectItem(json, "status");

            cJSON *service_item = cJSON_GetObjectItem(json, "service");

            valid =
                cJSON_IsString(status_item) &&
                strcmp(status_item->valuestring, "ok") == 0 &&
                cJSON_IsString(service_item) &&
                strcmp(service_item->valuestring, "cursore-server") == 0;

            cJSON_Delete(json);
        }
    }

    if (!valid) {
        ESP_LOGW(
            TAG,
            "health failed: esp_err=%s http=%d body_bytes=%u",
            esp_err_to_name(err),
            status,
            (unsigned)response.len
        );
    }

    esp_http_client_cleanup(client);

    return valid;
}

static void health_task(void *arg)
{
    for (;;) {

        if (wifi_is_connected()) {

            bool ok = health_check();

            ESP_LOGI(
                TAG,
                "GET /health: %s",
                ok ? "HTTP 200 JSON ok" : "error"
            );

            if (led_state_get() != CURSORE_STATE_MUTED) {

                led_state_set(
                    ok ? CURSORE_STATE_READY : CURSORE_STATE_ERROR
                );

                if (!ok) {
                    vTaskDelay(pdMS_TO_TICKS(1000));

                    if (led_state_get() == CURSORE_STATE_ERROR) {
                        led_state_set(CURSORE_STATE_READY);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void server_health_task_start(void)
{
    xTaskCreate(
        health_task,
        "health",
        4096,
        NULL,
        4,
        NULL
    );
}
