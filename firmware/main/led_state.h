#pragma once
typedef enum { CURSORE_STATE_MUTED, CURSORE_STATE_READY, CURSORE_STATE_LISTENING, CURSORE_STATE_PROCESSING, CURSORE_STATE_SPEAKING, CURSORE_STATE_ERROR } cursore_state_t;
void led_state_init(void);
void led_state_set(cursore_state_t state);
cursore_state_t led_state_get(void);
