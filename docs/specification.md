# Specifiche tecniche

## Target

- Board: M5Stack Atom Echo originale, SKU C008-C.
- SoC: ESP32-PICO-D4, Xtensa dual-core, 240 MHz.
- Flash: 4 MB.
- PSRAM: assente.
- Framework: ESP-IDF v6.0.3.
- Target ESP-IDF: `esp32`.

## Pin verificati

| Risorsa | GPIO |
|---|---:|
| Pulsante | 39 |
| SK6812 data | 27 |
| PDM microphone clock | 33 |
| PDM microphone data | 23 |
| I²S speaker data out | 22 |
| I²S BCLK | 19 |
| I²S LRCK / PDM clock condiviso | 33 |

GPIO19, GPIO22, GPIO23 e GPIO33 sono riservati all'audio interno dell'Atom Echo.

## Audio

- ingresso: PCM signed 16-bit little-endian, mono, 16 kHz;
- VAD: RMS/energia locale adattiva;
- pre-roll: buffer circolare limitato;
- upload: progressivo via LAN;
- risposta: WAV progressivo;
- uscita: I²S stereo con duplicazione del canale mono per compatibilità NS4168.

## Rete

- SSID primario e fallback: configurabili localmente in `firmware/main/secrets.h`;
- backend: `192.168.123.5:8766`;
- health: `GET /health`;
- verifica: HTTP 200 e JSON con `status: "ok"`, `service: "cursore-server"`.

## Limitazioni note

- nessuna PSRAM;
- wake word locale non ancora integrata;
- latenza e qualità dipendono dal Wi-Fi LAN e dall'acustica del contenitore;
- la condivisione GPIO33 richiede pausa del microfono durante la riproduzione.
