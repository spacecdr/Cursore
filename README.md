# Cursore

Firmware standalone per M5Stack Atom Echo originale: un piccolo terminale vocale locale per un assistente AI ispirato ad Automan.

[Pagina del progetto](https://spacecdr.github.io/Cursore/) · [Specifiche](docs/specification.md) · [Architettura](docs/architecture.md) · [Fattibilità wake word](docs/wake-word-feasibility.md)

![Cursore Atom Echo](assets/cursore-atom-hero.png)

> Il nome **Cursore** richiama l'assistente di Automan: un'interfaccia discreta, sempre pronta, tra persona, macchina e rete locale.

## Scopo

Cursore trasforma l'Atom Echo originale in un terminale vocale LAN per `cursore-server`, già disponibile sulla rete locale all'indirizzo `192.168.123.5:8766`.

Il firmware è progettato per lavorare senza Home Assistant, ESPHome, PlatformIO o servizi cloud obbligatori. L'Atom gestisce localmente il pulsante, il LED, l'acquisizione audio, il VAD e il controllo della sessione; il server esegue le funzioni AI più pesanti.

## Stato attuale

Il codice corrente contiene:

- Wi-Fi con rete primaria e fallback configurabili localmente, più riconnessione;
- stati LED `MUTED`, `READY`, `LISTENING`, `PROCESSING`, `SPEAKING`, `ERROR`;
- toggle globale tramite pressione breve del pulsante;
- verifica `GET /health` del backend;
- acquisizione microfono PDM a 16 kHz;
- VAD energetico locale adattivo con pre-roll;
- invio progressivo di PCM signed 16-bit little-endian, mono, 16 kHz;
- ricezione progressiva di WAV dal server e riproduzione I²S sullo speaker;
- gestione della condivisione GPIO33 tra clock PDM e LRCK I²S.

La wake word locale **“Cursore”** non è ancora inclusa. Prima della sua implementazione il firmware non deve trasmettere audio in rete.

## Flusso operativo

```text
Atom Echo
  │
  ├─ pulsante → READY / MUTED
  ├─ microfono PDM → VAD locale
  ├─ LED → stato della sessione
  │
  └─ Wi-Fi LAN
       │
       ▼
  cursore-server:8766
       │
       ├─ POST audio PCM progressivo
       ├─ STT → LLM → TTS
       └─ GET WAV progressivo
```

Il dettaglio dei moduli e dei protocolli è in [docs/architecture.md](docs/architecture.md). Le specifiche hardware sono in [docs/specification.md](docs/specification.md).

## Hardware supportato

Solo M5Stack Atom Echo originale, SKU C008-C:

| Funzione | Pin / componente |
|---|---|
| MCU | ESP32-PICO-D4, 240 MHz |
| Flash | 4 MB |
| PSRAM | assente |
| Pulsante | GPIO39 |
| LED | SK6812, GPIO27 |
| Microfono | SPM1423 PDM, CLK GPIO33, DATA GPIO23 |
| Speaker | NS4168 I²S, DATA GPIO22, BCLK GPIO19, LRCK GPIO33 |

Non usare questo progetto con AtomS3, AtomS3R o altri modelli con pinout diverso.

## Build locale

Prerequisiti:

- ESP-IDF `v6.0.3`;
- target `esp32`;
- Docker opzionale.

Configurazione credenziali:

```sh
cp main/secrets.example.h main/secrets.h
# inserire localmente SSID e password; main/secrets.h è ignorato da Git
```

Build nativa:

```sh
source /Users/iceman/.espressif/tools/activate_idf_v6.0.3.sh
idf.py set-target esp32
idf.py build
```

Build containerizzata:

```sh
docker build -t cursore-firmware .
docker run --rm -v "$PWD":/project -w /project cursore-firmware idf.py build
```

Il container usa `main/secrets.example.h` se `main/secrets.h` non è presente. Non inserire mai credenziali nell'immagine Docker o nel repository.

## Macchina a stati LED

| Stato | Colore |
|---|---|
| MUTED | rosso fisso |
| READY | verde fisso |
| LISTENING | giallo fisso |
| PROCESSING | blu fisso |
| SPEAKING | viola fisso |
| ERROR | rosso temporaneo, poi stato precedente |

## Installazione e licenze

Il firmware applicativo è pubblicato come progetto open source con licenza MIT. ESP-IDF e i componenti Espressif mantengono le rispettive licenze upstream. Il nome Cursore e il riferimento narrativo ad Automan descrivono l'identità del progetto e non implicano affiliazione ufficiale.

## Sicurezza

- nessuna password è versionata;
- `main/secrets.h` è escluso da Git;
- nessun audio deve precedere la futura wake word locale;
- il firmware comunica solo con il backend LAN configurato;
- non sono incluse procedure di flash automatico.

## Riferimenti

- [M5Stack Atom Echo](https://docs.m5stack.com/en/atom/atomecho)
- [ESP-IDF v6.0.3](https://github.com/espressif/esp-idf/releases/tag/v6.0.3)
- [Fattibilità wake word locale](docs/wake-word-feasibility.md)
- [Architettura](docs/architecture.md)
- [Specifiche](docs/specification.md)
