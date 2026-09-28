#include "wifi.h"
#include "config.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
static const char *TAG = "wifi";
static EventGroupHandle_t s_events;
static bool s_connected;
static bool s_using_fallback;
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { s_connected = false; xEventGroupClearBits(s_events, BIT0); ESP_LOGW(TAG, "disconnected; retrying current network"); esp_wifi_connect(); }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) { s_connected = true; xEventGroupSetBits(s_events, BIT0); ESP_LOGI(TAG, "connected (%s)", s_using_fallback ? "fallback" : "primary"); }
}
static void select_network(const char *ssid)
{
    wifi_config_t cfg = {0};
    snprintf((char *)cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid);
    snprintf((char *)cfg.sta.password, sizeof(cfg.sta.password), "%s", CURSORE_WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    s_using_fallback = strcmp(ssid, CURSORE_WIFI_FALLBACK) == 0;
    ESP_LOGI(TAG, "trying configured network: %s", ssid);
    esp_wifi_connect();
}
static void fallback_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(15000));
    if (!s_connected) { esp_wifi_disconnect(); select_network(CURSORE_WIFI_FALLBACK); }
    vTaskDelete(NULL);
}
void wifi_init(void)
{
	esp_err_t ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	    ESP_ERROR_CHECK(nvs_flash_erase());
	    ret = nvs_flash_init();
	}

	ESP_ERROR_CHECK(ret);
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default()); esp_netif_create_default_wifi_sta();
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL)); ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_start()); select_network(CURSORE_WIFI_PRIMARY);
    xTaskCreate(fallback_task, "wifi_fallback", 2048, NULL, 4, NULL);
}
bool wifi_is_connected(void) { return s_connected; }
