#include "microphone.h"
#include "config.h"

#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stddef.h>

static const char *TAG = "microphone";
static i2s_chan_handle_t s_rx_handle = NULL;

#define MIC_SAMPLE_RATE 16000
#define MIC_SAMPLES     512

static void microphone_task(void *arg)
{
    int16_t samples[MIC_SAMPLES];

    while (1) {
        size_t bytes_read = 0;

        esp_err_t err = i2s_channel_read(
            s_rx_handle,
            samples,
            sizeof(samples),
            &bytes_read,
            pdMS_TO_TICKS(1000)
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(err));
            continue;
        }

        size_t count = bytes_read / sizeof(int16_t);

        if (count == 0) {
            continue;
        }

        uint64_t sum_abs = 0;
        int32_t peak = 0;

        for (size_t i = 0; i < count; i++) {
            int32_t v = samples[i];

            if (v < 0) {
                v = -v;
            }

            sum_abs += (uint32_t)v;

            if (v > peak) {
                peak = v;
            }
        }

        uint32_t avg_abs = (uint32_t)(sum_abs / count);

        ESP_LOGI(
            TAG,
            "samples=%u avg_abs=%lu peak=%ld",
            (unsigned)count,
            (unsigned long)avg_abs,
            (long)peak
        );

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void microphone_init(void)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(
        i2s_new_channel(&chan_cfg, NULL, &s_rx_handle)
    );

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg =
            I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),

        .slot_cfg =
            I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT,
                I2S_SLOT_MODE_MONO
            ),

        .gpio_cfg = {
            .clk = ATOM_ECHO_PDM_CLOCK_GPIO,
            .din = ATOM_ECHO_PDM_DATA_GPIO,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(
        i2s_channel_init_pdm_rx_mode(
            s_rx_handle,
            &pdm_cfg
        )
    );

    ESP_ERROR_CHECK(
        i2s_channel_enable(s_rx_handle)
    );

    ESP_LOGI(
        TAG,
        "PDM microphone initialized: %d Hz, CLK GPIO%d, DATA GPIO%d",
        MIC_SAMPLE_RATE,
        ATOM_ECHO_PDM_CLOCK_GPIO,
        ATOM_ECHO_PDM_DATA_GPIO
    );

    xTaskCreate(
        microphone_task,
        "microphone",
        4096,
        NULL,
        5,
        NULL
    );
}
