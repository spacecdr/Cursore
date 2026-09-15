#pragma once

#include <stdbool.h>

void microphone_init(void);

bool microphone_audio_pause(void);
bool microphone_audio_resume(void);
