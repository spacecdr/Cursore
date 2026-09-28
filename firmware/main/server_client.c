#include "server_client.h"
#include "config.h"
#include "wifi.h"
#include "led_state.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include <errno.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "server";

static int s_audio_socket = -1;
static uint32_t s_request_counter = 0;
static char s_audio_response[2048];
static char s_audio_url[192];

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

            /*
             * Health checks must never overwrite LISTENING,
             * PROCESSING or SPEAKING states.
             */
            cursore_state_t state = led_state_get();

            if (!ok && state == CURSORE_STATE_READY) {
                led_state_set(CURSORE_STATE_ERROR);

                vTaskDelay(pdMS_TO_TICKS(1000));

                if (led_state_get() == CURSORE_STATE_ERROR) {
                    led_state_set(CURSORE_STATE_READY);
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


static bool socket_send_all(int sock, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t sent_total = 0;

    while (sent_total < len) {
        int sent = send(
            sock,
            p + sent_total,
            len - sent_total,
            0
        );

        if (sent <= 0) {
            ESP_LOGE(
                TAG,
                "socket send failed: errno=%d",
                errno
            );
            return false;
        }

        sent_total += (size_t)sent;
    }

    return true;
}

void server_audio_abort(void)
{
    if (s_audio_socket >= 0) {
        shutdown(s_audio_socket, SHUT_RDWR);
        close(s_audio_socket);
        s_audio_socket = -1;
    }

    ESP_LOGW(TAG, "audio POST aborted");
}

bool server_audio_begin(void)
{
    if (s_audio_socket >= 0) {
        ESP_LOGW(TAG, "audio request already active");
        return false;
    }

    if (!wifi_is_connected()) {
        ESP_LOGW(
            TAG,
            "cannot start audio request: Wi-Fi offline"
        );
        return false;
    }

    char port[8];

    snprintf(
        port,
        sizeof(port),
        "%d",
        CURSORE_SERVER_PORT
    );

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res = NULL;

    int err = getaddrinfo(
        CURSORE_SERVER_HOST,
        port,
        &hints,
        &res
    );

    if (err != 0 || res == NULL) {
        ESP_LOGE(
            TAG,
            "getaddrinfo failed: %d",
            err
        );
        return false;
    }

    int sock = socket(
        res->ai_family,
        res->ai_socktype,
        res->ai_protocol
    );

    if (sock < 0) {
        ESP_LOGE(
            TAG,
            "socket creation failed: errno=%d",
            errno
        );
        freeaddrinfo(res);
        return false;
    }

    if (connect(
            sock,
            res->ai_addr,
            res->ai_addrlen) != 0) {

        ESP_LOGE(
            TAG,
            "socket connect failed: errno=%d",
            errno
        );

        close(sock);
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);

    s_audio_socket = sock;

    char request_id[48];

    snprintf(
        request_id,
        sizeof(request_id),
        "%08lx-%08lx",
        (unsigned long)xTaskGetTickCount(),
        (unsigned long)++s_request_counter
    );

    char headers[768];

    int header_len = snprintf(
        headers,
        sizeof(headers),
        "POST /api/v1/requests HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: application/octet-stream\r\n"
        "X-Request-Id: %s\r\n"
        "X-Device-Id: atom-echo\r\n"
        "X-Audio-Sample-Rate: 16000\r\n"
        "X-Audio-Channels: 1\r\n"
        "X-Audio-Encoding: pcm_s16le\r\n"
        "\r\n",
        CURSORE_SERVER_HOST,
        CURSORE_SERVER_PORT,
        request_id
    );

    if (header_len <= 0 ||
        header_len >= (int)sizeof(headers) ||
        !socket_send_all(
            s_audio_socket,
            headers,
            (size_t)header_len)) {

        ESP_LOGE(TAG, "failed sending HTTP headers");
        server_audio_abort();
        return false;
    }

    ESP_LOGI(
        TAG,
        "audio POST opened request_id=%s",
        request_id
    );

    return true;
}

bool server_audio_write(
    const int16_t *samples,
    size_t sample_count
)
{
    if (s_audio_socket < 0 ||
        samples == NULL ||
        sample_count == 0) {
        return false;
    }

    size_t bytes =
        sample_count * sizeof(int16_t);

    char chunk_header[16];

    int header_len = snprintf(
        chunk_header,
        sizeof(chunk_header),
        "%x\r\n",
        (unsigned)bytes
    );

    if (header_len <= 0 ||
        header_len >= (int)sizeof(chunk_header)) {
        return false;
    }

    if (!socket_send_all(
            s_audio_socket,
            chunk_header,
            (size_t)header_len) ||
        !socket_send_all(
            s_audio_socket,
            samples,
            bytes) ||
        !socket_send_all(
            s_audio_socket,
            "\r\n",
            2)) {

        ESP_LOGE(TAG, "failed sending audio chunk");
        server_audio_abort();
        return false;
    }

    return true;
}

bool server_audio_end(void)
{
    if (s_audio_socket < 0) {
        return false;
    }

    if (!socket_send_all(
            s_audio_socket,
            "0\r\n\r\n",
            5)) {

        ESP_LOGE(
            TAG,
            "failed terminating audio stream"
        );

        server_audio_abort();
        return false;
    }

    size_t total = 0;

    while (total < sizeof(s_audio_response) - 1) {
        int received = recv(
            s_audio_socket,
            s_audio_response + total,
            sizeof(s_audio_response) - 1 - total,
            0
        );

        if (received < 0) {
            ESP_LOGE(
                TAG,
                "response recv failed: errno=%d",
                errno
            );

            server_audio_abort();
            return false;
        }

        if (received == 0) {
            break;
        }

        total += (size_t)received;
    }

    s_audio_response[total] = '\0';

    /*
     * Extract audio.url from the JSON body returned by Cursore.
     */
    s_audio_url[0] = '\0';

    char *body = strstr(s_audio_response, "\r\n\r\n");

    if (body != NULL) {
        body += 4;

        cJSON *json = cJSON_Parse(body);

        if (json != NULL) {
            cJSON *audio = cJSON_GetObjectItem(json, "audio");
            cJSON *url = audio ?
                cJSON_GetObjectItem(audio, "url") : NULL;

            if (cJSON_IsString(url) &&
                url->valuestring != NULL) {

                snprintf(
                    s_audio_url,
                    sizeof(s_audio_url),
                    "%s",
                    url->valuestring
                );
            }

            cJSON_Delete(json);
        }
    }

    bool http_ok =
        total >= 12 &&
        (
            strncmp(
                s_audio_response,
                "HTTP/1.1 200",
                12
            ) == 0 ||
            strncmp(
                s_audio_response,
                "HTTP/1.0 200",
                12
            ) == 0
        );

    ESP_LOGI(
        TAG,
        "audio POST response: %s bytes=%u",
        http_ok ? "HTTP 200" : "HTTP error",
        (unsigned)total
    );

    if (total > 0) {
        ESP_LOGI(TAG, "%s", s_audio_response);
    }

    shutdown(s_audio_socket, SHUT_RDWR);
    close(s_audio_socket);
    s_audio_socket = -1;

    return http_ok;
}


const char *server_audio_get_url(void)
{
    return s_audio_url[0] ? s_audio_url : NULL;
}
