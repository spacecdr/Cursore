# CURSORE standalone firmware

Target: original M5Stack Atom Echo (SKU C008-C), ESP32-PICO-D4, 4 MB flash, no PSRAM, not AtomS3/S3R.

- Standalone ESP-IDF firmware; direct LAN communication with `cursore-server` at `192.168.123.5:8766`.
- MAC-1 established Wi-Fi primary/fallback, reconnection, button toggle, RGB state indication, and `GET /health`; the current development branch also contains the subsequent local audio/VAD and progressive playback modules documented in `README.md`.
- Primary/fallback SSID and password are local-only in `main/secrets.h`; never publish them.
- Never log credentials. Never commit `main/secrets.h`. Do not add Home Assistant or ESPHome dependencies.
- Startup is enabled/READY. Short debounced presses toggle READY/MUTED. MUTED red; READY green.
- Future states: LISTENING yellow, PROCESSING blue, SPEAKING violet, temporary ERROR red.
- Future wake word `Cursore` must be local; no network audio before detection. Future PCM is signed 16-bit LE, mono, 16 kHz and streamed progressively.
- The local wake word `Cursore` is not implemented yet. Audio must remain local until wake-word detection is added; do not add cloud wake-word services or credentials.

Verified original pins from M5Stack: button GPIO39; SK6812 GPIO27; PDM mic clock GPIO33/data GPIO23; NS4168 I2S speaker data GPIO22/BCLK GPIO19/LRCK GPIO33. GPIO19/22/23/33 are reserved.

Do not flash or erase hardware without explicit authorization.

## Percorsi locali aggiornati (24 settembre 2026)

Prima di riprendere una sessione precedente, leggi `/Users/iceman/Documents/PROGETTI.md`:
contiene i percorsi correnti e la mappa delle cartelle spostate. I vecchi riferimenti
in chat a `/Users/iceman/IRReceiverTest`, `/Users/iceman/TelecomandoTV` e
`/Users/iceman/esp` vanno tradotti nei nuovi percorsi. Anche
`/Users/iceman/Projects/cursore-atom` è stato spostato in
`/Users/iceman/Documents/Arduino/cursore-atom`. I progetti sono stati spostati,
non cancellati: continua dai file, README e handoff esistenti senza ricrearli.
