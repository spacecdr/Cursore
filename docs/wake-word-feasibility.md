# Fattibilità futura wake word locale “Cursore”

## Vincoli verificati

L’Atom Echo originale ha ESP32-PICO-D4, 4 MB flash e nessuna PSRAM. Il microfono SPM1423 è PDM: GPIO33 clock e GPIO23 data. La futura acquisizione deve quindi usare il periferico I²S/PDM dell’ESP32 e produrre mono a 16 kHz.

## Valutazione

- **microWakeWord standalone:** tecnicamente possibile come porting del modello e del frontend streaming, ma non è un componente ESP-IDF ufficiale “drop-in”; il runtime va integrato senza ESPHome.
- **TensorFlow Lite Micro:** disponibile per ESP32 tramite il componente Espressif `esp-tflite-micro`. Un modello INT8 custom “Cursore” è il percorso più coerente con il requisito di riconoscimento locale e senza servizio a pagamento.
- **RAM:** l’ESP32 originale ha poca RAM libera rispetto a ESP32-S3. Un modello compatto e una tensor arena statica dovranno essere dimensionati da benchmark; come ordine di grandezza iniziale è prudente restare nell’area 20–40 KB di arena, più frontend/audio ring buffer, lasciando margine per Wi-Fi.
- **Flash:** 4 MB sono sufficienti per un firmware MAC-1 e possono contenere un modello INT8 piccolo, ma la partizione OTA dual-slot adottata qui lascia 1.5 MB per ciascuna app. Il modello dovrà essere misurato prima di congelare le partizioni.
- **CPU:** 240 MHz è sufficiente per inferenza streaming leggera, ma il costo reale dipende da architettura, frontend e kernel; va misurato sul target ESP32 originale, non trasferito da benchmark ESP32-S3.
- **Rischi:** falsi positivi/negativi in ambiente rumoroso, memoria Wi-Fi concorrente, latenza del frontend PDM, frammentazione heap e possibile necessità di ridurre il modello o rinunciare a OTA dual-slot.

## Decisione per MAC-1

Nessun microfono, modello, TFLite Micro, wake word o audio viene incluso ora. La pipeline futura dovrà mantenere l’audio locale fino alla rilevazione di “Cursore” e poi inviare PCM progressivamente, senza accumulare l’intera frase.

Fonti tecniche: [M5Stack Atom Echo](https://docs.m5stack.com/en/atom/atomecho), [ESP-IDF release v6.0.3](https://github.com/espressif/esp-idf/releases/tag/v6.0.3), [Espressif TFLite Micro](https://github.com/espressif/esp-tflite-micro), [microWakeWord](https://github.com/kahrendt/microWakeWord), [ESP-SR WakeNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/wake_word_engine/index.html).
