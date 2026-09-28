#include "speaker.h"

#include "config.h"
#include "microphone.h"

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "speaker";

#define SPEAKER_SAMPLE_RATE 16000
#define NET_BUFFER_SIZE     2048

static uint8_t s_net_buffer[NET_BUFFER_SIZE];

/*
 * Input WAV is mono PCM16.
 * Duplicate each mono sample into L/R because Atom Echo playback
 * is more reliable using a stereo I2S frame.
 */
static int16_t s_stereo_buffer[NET_BUFFER_SIZE];

static bool send_all(
    int sock,
    const void *data,
    size_t len
)
{
    const uint8_t *p = data;
    size_t total = 0;

    while (total < len) {
        int n = send(
            sock,
            p + total,
            len - total,
            0
        );

        if (n <= 0) {
            ESP_LOGE(
                TAG,
                "HTTP send failed errno=%d",
                errno
            );
            return false;
        }

        total += (size_t)n;
    }

    return true;
}

static bool speaker_write_pcm(
    i2s_chan_handle_t tx,
    const uint8_t *data,
    size_t len
)
{
    /*
     * PCM16 requires complete samples.
     */
    size_t samples = len / 2;

    if (samples > NET_BUFFER_SIZE / 2) {
        return false;
    }

    const int16_t *mono = (const int16_t *)data;

    for (size_t i = 0; i < samples; i++) {
        s_stereo_buffer[i * 2] = mono[i];
        s_stereo_buffer[i * 2 + 1] = mono[i];
    }

    size_t bytes_to_write =
        samples * 2 * sizeof(int16_t);

    size_t written = 0;

    esp_err_t err = i2s_channel_write(
        tx,
        s_stereo_buffer,
        bytes_to_write,
        &written,
        portMAX_DELAY
    );

    return err == ESP_OK &&
           written == bytes_to_write;
}

bool speaker_play_url(const char *path)
{
    if (path == NULL || path[0] != '/') {
        ESP_LOGE(TAG, "invalid audio path");
        return false;
    }

    /*
     * Release I2S0 PDM RX before creating standard TX.
     * GPIO33 is shared by microphone clock and speaker LRCK.
     */
    if (!microphone_audio_pause()) {
        return false;
    }

    bool result = false;
    int sock = -1;
    i2s_chan_handle_t tx = NULL;

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    esp_err_t err =
        i2s_new_channel(
            &chan_cfg,
            &tx,
            NULL
        );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "TX channel creation failed: %s",
            esp_err_to_name(err)
        );
        goto cleanup;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            I2S_STD_CLK_DEFAULT_CONFIG(
                SPEAKER_SAMPLE_RATE
            ),

        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_STEREO
            ),

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = ATOM_ECHO_I2S_BCLK_GPIO,
            .ws = ATOM_ECHO_I2S_LRCK_GPIO,
            .dout = ATOM_ECHO_I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,

            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(
        tx,
        &std_cfg
    );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "TX init failed: %s",
            esp_err_to_name(err)
        );
        goto cleanup;
    }

    err = i2s_channel_enable(tx);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "TX enable failed: %s",
            esp_err_to_name(err)
        );
        goto cleanup;
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

    if (getaddrinfo(
            CURSORE_SERVER_HOST,
            port,
            &hints,
            &res) != 0 ||
        res == NULL) {

        ESP_LOGE(TAG, "audio GET DNS/address failure");
        goto cleanup;
    }

    sock = socket(
        res->ai_family,
        res->ai_socktype,
        res->ai_protocol
    );

    if (sock < 0) {
        freeaddrinfo(res);
        ESP_LOGE(
            TAG,
            "audio GET socket failed errno=%d",
            errno
        );
        goto cleanup;
    }

    if (connect(
            sock,
            res->ai_addr,
            res->ai_addrlen) != 0) {

        freeaddrinfo(res);
        ESP_LOGE(
            TAG,
            "audio GET connect failed errno=%d",
            errno
        );
        goto cleanup;
    }

    freeaddrinfo(res);

    char request[512];

    int request_len = snprintf(
        request,
        sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path,
        CURSORE_SERVER_HOST,
        CURSORE_SERVER_PORT
    );

    if (request_len <= 0 ||
        request_len >= (int)sizeof(request) ||
        !send_all(
            sock,
            request,
            (size_t)request_len)) {

        goto cleanup;
    }

    ESP_LOGI(TAG, "streaming %s", path);

    /*
     * First collect the HTTP headers.
     */
    char http_header[1024];
    size_t header_len = 0;
    bool headers_done = false;

    uint8_t wav_header[44];
    size_t wav_header_len = 0;

    /*
     * Odd-byte carry protects PCM16 alignment across recv().
     */
    bool have_carry = false;
    uint8_t carry = 0;

    while (1) {
        int n = recv(
            sock,
            s_net_buffer,
            sizeof(s_net_buffer),
            0
        );

        if (n < 0) {
            ESP_LOGE(
                TAG,
                "audio GET recv failed errno=%d",
                errno
            );
            goto cleanup;
        }

        if (n == 0) {
            break;
        }

        size_t pos = 0;

        if (!headers_done) {
            while (pos < (size_t)n &&
                   !headers_done) {

                if (header_len >=
                    sizeof(http_header) - 1) {

                    ESP_LOGE(
                        TAG,
                        "HTTP response header too large"
                    );
                    goto cleanup;
                }

                http_header[header_len++] =
                    (char)s_net_buffer[pos++];

                http_header[header_len] = '\0';

                if (header_len >= 4 &&
                    memcmp(
                        http_header +
                        header_len - 4,
                        "\r\n\r\n",
                        4
                    ) == 0) {

                    headers_done = true;

                    if (strncmp(
                            http_header,
                            "HTTP/1.0 200",
                            12) != 0 &&
                        strncmp(
                            http_header,
                            "HTTP/1.1 200",
                            12) != 0) {

                        ESP_LOGE(
                            TAG,
                            "audio GET returned non-200"
                        );
                        goto cleanup;
                    }
                }
            }

            if (!headers_done) {
                continue;
            }
        }

        /*
         * Skip the standard 44-byte PCM WAV header.
         */
        while (pos < (size_t)n &&
               wav_header_len < sizeof(wav_header)) {

            wav_header[wav_header_len++] =
                s_net_buffer[pos++];
        }

        if (wav_header_len < sizeof(wav_header)) {
            continue;
        }

        if (wav_header[0] != 'R' ||
            wav_header[1] != 'I' ||
            wav_header[2] != 'F' ||
            wav_header[3] != 'F' ||
            wav_header[8] != 'W' ||
            wav_header[9] != 'A' ||
            wav_header[10] != 'V' ||
            wav_header[11] != 'E') {

            ESP_LOGE(TAG, "invalid WAV response");
            goto cleanup;
        }

        if (pos >= (size_t)n) {
            continue;
        }

        /*
         * Keep 16-bit sample alignment.
         */
        if (have_carry) {
            uint8_t pair[2] = {
                carry,
                s_net_buffer[pos++]
            };

            if (!speaker_write_pcm(
                    tx,
                    pair,
                    2)) {
                goto cleanup;
            }

            have_carry = false;
        }

        size_t remaining =
            (size_t)n - pos;

        size_t even_len =
            remaining & ~(size_t)1;

        if (even_len > 0) {
            if (!speaker_write_pcm(
                    tx,
                    s_net_buffer + pos,
                    even_len)) {

                ESP_LOGE(
                    TAG,
                    "I2S speaker write failed"
                );
                goto cleanup;
            }

            pos += even_len;
        }

        if (pos < (size_t)n) {
            carry = s_net_buffer[pos];
            have_carry = true;
        }
    }

    ESP_LOGI(TAG, "audio playback completed");
    result = true;

cleanup:

    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    if (tx != NULL) {
        i2s_channel_disable(tx);
        i2s_del_channel(tx);
    }

    if (!microphone_audio_resume()) {
        ESP_LOGE(
            TAG,
            "CRITICAL: microphone resume failed"
        );
        result = false;
    }

    return result;
}
