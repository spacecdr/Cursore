#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#endif
// Compatibilità con i vecchi secrets.h del DietServer: i nuovi template
// definiscono host e porta localmente, senza modificare questo sorgente.
#ifndef CURSORE_SERVER_HOST
#define CURSORE_SERVER_HOST "192.168.123.5"
#endif
#ifndef CURSORE_SERVER_PORT
#define CURSORE_SERVER_PORT 8766
#endif
#define CURSORE_HEALTH_PATH "/health"
#define ATOM_ECHO_LED_GPIO 27
#define ATOM_ECHO_BUTTON_GPIO 39
// Reserved for future audio phases; do not initialize in MAC-1.
#define ATOM_ECHO_I2S_DOUT_GPIO 22
#define ATOM_ECHO_I2S_BCLK_GPIO 19
#define ATOM_ECHO_I2S_LRCK_GPIO 33
#define ATOM_ECHO_PDM_DATA_GPIO 23
#define ATOM_ECHO_PDM_CLOCK_GPIO 33
