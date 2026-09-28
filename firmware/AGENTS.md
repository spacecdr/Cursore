# Firmware CURSORE nel repository condiviso

Leggere `../AGENTS.md`, `../docs/collaboration.md` e `../docs/handoff.md` prima di lavorare.
Questa cartella era la root del vecchio repository firmware; tutti i percorsi ESP-IDF
sono ora relativi a `firmware/`.

- Solo Atom Echo originale ESP32-PICO-D4, 4 MB flash, senza PSRAM; ESP-IDF v6.0.3.
- Compilare/flashare sul Mac collegato fisicamente al dispositivo; niente flash dal DietServer.
- Credenziali in `main/secrets.h` locale, template `main/secrets.example.h`.
- Pulsante GPIO39; LED GPIO27; microfono PDM CLK33/DATA23; speaker DOUT22/BCLK19/LRCK33.
- GPIO33 è condiviso: sospendere il microfono durante il playback.
- Upload ufficiale HTTP/1.1 chunked, PCM s16le mono 16 kHz, VAD locale.
- Wake word locale Cursore ancora assente: il VAD oggi apre lo stream. È un limite
  noto da risolvere, non una realizzazione del requisito privacy.
- Nessun Home Assistant/ESPHome; non accumulare intere registrazioni in RAM.
- Modifiche al contratto API vanno coordinate con la sessione backend; aggiornare l'handoff.
- Non presumere accessibili i percorsi `/Users/...` sul DietServer o viceversa.
