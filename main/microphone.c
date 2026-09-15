#include "microphone.h"
#include "config.h"
#include "led_state.h"
#include "server_client.h"
#include "speaker.h"

#include "driver/i2s_pdm.h"
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const char *TAG = "microphone";

static i2s_chan_handle_t s_rx_handle = NULL;

#define MIC_SAMPLE_RATE         16000
#define MIC_SAMPLES             512

/* 512 samples @ 16 kHz = 32 ms per frame */
#define VAD_ATTACK_FRAMES       2
#define VAD_RELEASE_FRAMES      25      /* ~800 ms */
#define VAD_CALIBRATION_FRAMES  50
#define AUDIO_PREROLL_FRAMES     10      /* ~320 ms */

/*
 * Static storage: ~10 KB in BSS instead of the 4 KB task stack.
 */
static int16_t s_preroll[AUDIO_PREROLL_FRAMES][MIC_SAMPLES];
      /* ~1.6 s */

/*
 * Safety threshold:
 * measured silence on this Atom was ~35-50 avg_abs.
 * This prevents an unrealistically low adaptive threshold.
 */
#define VAD_MIN_THRESHOLD       180

/*
 * Voice threshold = noise floor * 4.
 * Measured speech was ~800-1700, so there is ample margin.
 */
#define VAD_NOISE_MULTIPLIER    4

static uint32_t calculate_avg_abs(
    const int16_t *samples,
    size_t count,
    int32_t *peak_out
)
{
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

    if (peak_out != NULL) {
        *peak_out = peak;
    }

    return count > 0 ? (uint32_t)(sum_abs / count) : 0;
}

static uint32_t vad_threshold(uint32_t noise_floor)
{
    uint32_t threshold = noise_floor * VAD_NOISE_MULTIPLIER;

    if (threshold < VAD_MIN_THRESHOLD) {
        threshold = VAD_MIN_THRESHOLD;
    }

    return threshold;
}

static void microphone_task(void *arg)
{
    int16_t samples[MIC_SAMPLES];

    size_t preroll_write = 0;
    size_t preroll_count = 0;

    bool stream_active = false;

    uint32_t noise_floor = 40;
    uint32_t calibration_sum = 0;
    uint32_t calibration_frames = 0;

    uint32_t speech_frames = 0;
    uint32_t silence_frames = 0;

    uint32_t log_counter = 0;

    bool calibrated = false;
    bool voice_active = false;

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
            ESP_LOGE(
                TAG,
                "I2S read failed: %s",
                esp_err_to_name(err)
            );
            continue;
        }

        size_t count = bytes_read / sizeof(int16_t);

        if (count == 0) {
            continue;
        }

        int32_t peak = 0;

        uint32_t level = calculate_avg_abs(
            samples,
            count,
            &peak
        );

        /*
         * The button is a global abort.
         * If the assistant becomes muted while streaming,
         * terminate the network request immediately.
         */
        if (led_state_get() == CURSORE_STATE_MUTED &&
            stream_active) {

            ESP_LOGI(TAG, "MUTED: aborting active audio stream");

            server_audio_abort();
            stream_active = false;
            voice_active = false;
            speech_frames = 0;
            silence_frames = 0;
        }

        /*
         * Initial ambient-noise calibration.
         */
        if (!calibrated) {
            calibration_sum += level;
            calibration_frames++;

            if (calibration_frames >= VAD_CALIBRATION_FRAMES) {
                noise_floor =
                    calibration_sum / calibration_frames;

                if (noise_floor < 10) {
                    noise_floor = 10;
                }

                calibrated = true;

                ESP_LOGI(
                    TAG,
                    "VAD calibrated: noise_floor=%lu threshold=%lu",
                    (unsigned long)noise_floor,
                    (unsigned long)vad_threshold(noise_floor)
                );
            }

            continue;
        }

        uint32_t threshold = vad_threshold(noise_floor);
        bool above_threshold = level >= threshold;

        /*
         * Slowly adapt the noise floor only while we're confident
         * that the current frame is background noise.
         *
         * EMA approximately:
         * new_floor = 98% old + 2% current.
         */
        if (!voice_active && !above_threshold) {
            noise_floor =
                (noise_floor * 49 + level) / 50;

            if (noise_floor < 10) {
                noise_floor = 10;
            }

            threshold = vad_threshold(noise_floor);
        }

        /*
         * Keep the latest ~320 ms while idle.
         * This prevents the beginning of the first word from
         * being lost while VAD confirms speech.
         */
        if (!voice_active) {
            memcpy(
                s_preroll[preroll_write],
                samples,
                count * sizeof(int16_t)
            );

            preroll_write =
                (preroll_write + 1) % AUDIO_PREROLL_FRAMES;

            if (preroll_count < AUDIO_PREROLL_FRAMES) {
                preroll_count++;
            }
        }

        if (!voice_active) {
            if (above_threshold) {
                speech_frames++;

                if (speech_frames >= VAD_ATTACK_FRAMES) {
                    voice_active = true;
                    silence_frames = 0;
                    speech_frames = 0;

                    ESP_LOGI(
                        TAG,
                        "VOICE START level=%lu peak=%ld floor=%lu threshold=%lu",
                        (unsigned long)level,
                        (long)peak,
                        (unsigned long)noise_floor,
                        (unsigned long)threshold
                    );

                    if (led_state_get() != CURSORE_STATE_MUTED) {
                        led_state_set(CURSORE_STATE_LISTENING);

                        if (server_audio_begin()) {
                            stream_active = true;

                            /*
                             * Send pre-roll in chronological order.
                             */
                            size_t start =
                                (preroll_write +
                                 AUDIO_PREROLL_FRAMES -
                                 preroll_count) %
                                AUDIO_PREROLL_FRAMES;

                            bool preroll_ok = true;

                            for (size_t i = 0;
                                 i < preroll_count;
                                 i++) {

                                size_t index =
                                    (start + i) %
                                    AUDIO_PREROLL_FRAMES;

                                if (!server_audio_write(
                                        s_preroll[index],
                                        MIC_SAMPLES)) {

                                    preroll_ok = false;
                                    break;
                                }
                            }

                            if (!preroll_ok) {
                                ESP_LOGE(
                                    TAG,
                                    "failed sending audio pre-roll"
                                );

                                server_audio_abort();
                                stream_active = false;
                            } else {
                                ESP_LOGI(
                                    TAG,
                                    "audio stream started with %u pre-roll frames",
                                    (unsigned)preroll_count
                                );
                            }
                        } else {
                            ESP_LOGE(
                                TAG,
                                "unable to open audio stream"
                            );
                        }

                        preroll_count = 0;
                    }
                }
            } else {
                speech_frames = 0;
            }
        } else {
            /*
             * After VOICE START, every frame is streamed,
             * including the silence used to determine VOICE END.
             */
            if (stream_active) {
                if (!server_audio_write(samples, count)) {
                    ESP_LOGE(TAG, "audio streaming failed");
                    server_audio_abort();
                    stream_active = false;
                }
            }

            if (above_threshold) {
                silence_frames = 0;
            } else {
                silence_frames++;

                if (silence_frames >= VAD_RELEASE_FRAMES) {
                    voice_active = false;
                    silence_frames = 0;

                    ESP_LOGI(
                        TAG,
                        "VOICE END level=%lu peak=%ld floor=%lu threshold=%lu",
                        (unsigned long)level,
                        (long)peak,
                        (unsigned long)noise_floor,
                        (unsigned long)threshold
                    );

                    if (stream_active) {
                        if (led_state_get() != CURSORE_STATE_MUTED) {
                            led_state_set(CURSORE_STATE_PROCESSING);
                        }

                        ESP_LOGI(
                            TAG,
                            "audio stream complete; waiting for server"
                        );

                        bool request_ok = server_audio_end();
                        stream_active = false;

                        ESP_LOGI(
                            TAG,
                            "server processing: %s",
                            request_ok ? "OK" : "FAILED"
                        );

                        if (request_ok &&
                            led_state_get() != CURSORE_STATE_MUTED) {

                            const char *audio_url =
                                server_audio_get_url();

                            if (audio_url != NULL) {
                                led_state_set(
                                    CURSORE_STATE_SPEAKING
                                );

                                bool playback_ok =
                                    speaker_play_url(audio_url);

                                ESP_LOGI(
                                    TAG,
                                    "speaker playback: %s",
                                    playback_ok ? "OK" : "FAILED"
                                );
                            } else {
                                ESP_LOGE(
                                    TAG,
                                    "server response has no audio URL"
                                );
                            }
                        }

                        if (led_state_get() != CURSORE_STATE_MUTED) {
                            if (request_ok) {
                                led_state_set(CURSORE_STATE_READY);
                            } else {
                                led_state_set(CURSORE_STATE_ERROR);
                                vTaskDelay(pdMS_TO_TICKS(1000));

                                if (led_state_get() ==
                                    CURSORE_STATE_ERROR) {
                                    led_state_set(
                                        CURSORE_STATE_READY
                                    );
                                }
                            }
                        }
                    } else if (
                        led_state_get() != CURSORE_STATE_MUTED) {

                        led_state_set(CURSORE_STATE_READY);
                    }
                }
            }
        }

        /*
         * Diagnostic line roughly every second.
         * Avoid flooding the serial monitor.
         */
        log_counter++;

        if (log_counter >= 31) {
            log_counter = 0;

            ESP_LOGI(
                TAG,
                "VAD level=%lu peak=%ld floor=%lu threshold=%lu active=%d",
                (unsigned long)level,
                (long)peak,
                (unsigned long)noise_floor,
                (unsigned long)threshold,
                voice_active ? 1 : 0
            );
        }
    }
}


bool microphone_audio_pause(void)
{
    if (s_rx_handle == NULL) {
        return true;
    }

    esp_err_t err = i2s_channel_disable(s_rx_handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "unable to disable microphone: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    err = i2s_del_channel(s_rx_handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "unable to release microphone I2S: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    s_rx_handle = NULL;

    ESP_LOGI(TAG, "microphone I2S released for speaker");
    return true;
}

bool microphone_audio_resume(void)
{
    if (s_rx_handle != NULL) {
        return true;
    }

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    esp_err_t err =
        i2s_new_channel(
            &chan_cfg,
            NULL,
            &s_rx_handle
        );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "unable to recreate microphone channel: %s",
            esp_err_to_name(err)
        );
        s_rx_handle = NULL;
        return false;
    }

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg =
            I2S_PDM_RX_CLK_DEFAULT_CONFIG(
                MIC_SAMPLE_RATE
            ),

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

    err = i2s_channel_init_pdm_rx_mode(
        s_rx_handle,
        &pdm_cfg
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PDM resume init failed: %s",
                 esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return false;
    }

    err = i2s_channel_enable(s_rx_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PDM resume enable failed: %s",
                 esp_err_to_name(err));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return false;
    }

    ESP_LOGI(TAG, "microphone PDM resumed");
    return true;
}

void microphone_init(void)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    ESP_ERROR_CHECK(
        i2s_new_channel(
            &chan_cfg,
            NULL,
            &s_rx_handle
        )
    );

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg =
            I2S_PDM_RX_CLK_DEFAULT_CONFIG(
                MIC_SAMPLE_RATE
            ),

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
        i2s_channel_enable(
            s_rx_handle
        )
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
        8192,
        NULL,
        5,
        NULL
    );
}
