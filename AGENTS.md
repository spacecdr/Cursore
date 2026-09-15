# CURSORE standalone firmware

Target: original M5Stack Atom Echo (SKU C008-C), ESP32-PICO-D4, 4 MB flash, no PSRAM, not AtomS3/S3R.

- Standalone ESP-IDF firmware; direct LAN communication with `cursore-server` at `192.168.123.5:8766`.
- MAC-1 implements only Wi-Fi primary/fallback, reconnection, button toggle, RGB state indication, and `GET /health`.
- Primary SSID `Pepis`; fallback `PepisRoofTop`; password is local-only in `main/secrets.h`.
- Never log credentials. Never commit `main/secrets.h`. Do not add Home Assistant or ESPHome dependencies.
- Startup is enabled/READY. Short debounced presses toggle READY/MUTED. MUTED red; READY green.
- Future states: LISTENING yellow, PROCESSING blue, SPEAKING violet, temporary ERROR red.
- Future wake word `Cursore` must be local; no network audio before detection. Future PCM is signed 16-bit LE, mono, 16 kHz and streamed progressively.
- Do not implement microphone, speaker, wake word, VAD, STT, AI, TTS, or audio streaming in MAC-1.

Verified original pins from M5Stack: button GPIO39; SK6812 GPIO27; PDM mic clock GPIO33/data GPIO23; NS4168 I2S speaker data GPIO22/BCLK GPIO19/LRCK GPIO33. GPIO19/22/23/33 are reserved.

Do not flash or erase hardware without explicit authorization.
