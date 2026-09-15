#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void server_health_task_start(void);

bool server_audio_begin(void);

bool server_audio_write(
    const int16_t *samples,
    size_t sample_count
);

bool server_audio_end(void);

void server_audio_abort(void);

const char *server_audio_get_url(void);
