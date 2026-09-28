# Replicare Cursore sulla propria rete

Repository unico: https://github.com/spacecdr/Cursore.
Questa guida installa l'attuale prototipo server + firmware. Non è ancora una release
con wake word: il firmware apre lo stream quando il VAD rileva voce. Usare un ambiente
di test consapevole di questo comportamento; il requisito “Cursore prima dell'audio” è aperto.

## Occorrente

- M5Stack Atom Echo **originale** ESP32-PICO-D4 (C008-C), cavo USB dati e Wi-Fi 2.4 GHz.
  AtomS3/S3R e altri pinout non sono supportati da questo firmware.
- Server Linux con Docker Engine e Compose v2, spazio per immagine/modelli e indirizzo
  IPv4 LAN stabile (prenotazione DHCP consigliata). Il backend è verificato su Debian
  x86_64 con circa 3.5 GiB RAM totale; altre architetture non sono ancora validate.
- Accesso Internet dal server per download e API Groq; chiave Groq personale.
  La pipeline attiva è STT/LLM cloud + TTS Piper locale, quindi non interamente offline.
  Quote e disponibilità del provider possono variare; non è promesso costo zero illimitato.
- Computer USB con ESP-IDF v6.0.3 per compilare/flashare; il flusso di sviluppo attuale
  usa un Mac. Docker da solo non sostituisce configurazione/flash del dispositivo.

Non servono Home Assistant, ESPHome, Node-RED o una chiave Gemini per il percorso attivo.

## 1. Clonare e configurare il backend

Sul server Docker, in una cartella dedicata:

```sh
git clone https://github.com/spacecdr/Cursore.git
cd Cursore
test -e .env || cp .env.example .env
chmod 600 .env
```

Modificare localmente `.env`:

- CURSORE_LAN_IP: IPv4 effettivamente assegnato al server, ad esempio 192.168.1.10.
  Non usare 127.0.0.1, 0.0.0.0 o l'IP di un container.
- CURSORE_PORT: porta libera, default 8766. Verificarla con gli strumenti del server.
- GROQ_API_KEY: la propria chiave, mai copiata nei template o nei log.
- GROQ_LLM_MODEL e PIPER_MODEL possono restare ai default del template.

Il compose pubblica su loopback e sull'IP LAN scelto. Non aprire porte sul router
e non esporre l'API su Internet; il protocollo attuale è HTTP senza autenticazione dispositivo.
Se si usa un firewall, autorizzare la porta soltanto dalla propria rete fidata.

## 2. Avviare e verificare

```sh
docker compose config --quiet
docker compose up -d --build cursore-server
docker compose ps
curl --fail http://127.0.0.1:8766/health
```

Se si cambia porta, aggiornare anche il comando curl. Il primo build scarica Piper
e i modelli; l'avvio carica il modello TTS prima di rendere disponibile HTTP.
La risposta health attesa contiene `status: "ok"` e `service: "cursore-server"`.
Dal computer USB verificare anche `http://<IP-LAN-server>:<porta>/health`.
Un health positivo verifica il server HTTP, non la validità della chiave o tutta la pipeline AI.

## 3. Configurare l'Atom dal computer USB

Clonare lo stesso repository sul Mac; attivare ESP-IDF v6.0.3 tramite la propria
installazione locale, poi:

```sh
cd Cursore/firmware
test -e main/secrets.h || cp main/secrets.example.h main/secrets.h
```

Modificare solo `main/secrets.h`:

- CURSORE_WIFI_PRIMARY: SSID principale.
- CURSORE_WIFI_FALLBACK: secondo SSID, oppure lo stesso principale se si ha una sola rete.
- CURSORE_WIFI_PASSWORD: password Wi-Fi. Le due reti devono usare la stessa password
  con l'implementazione attuale.
- CURSORE_SERVER_HOST: stesso IP di CURSORE_LAN_IP del backend, senza http:// e senza porta.
- CURSORE_SERVER_PORT: stessa porta CURSORE_PORT del backend.

Le chiavi Groq/Gemini non vanno mai nel firmware.
La configurazione locale sovrascrive i default di compatibilità del progetto.

## 4. Compilare e flashare

```sh
idf.py set-target esp32
idf.py build
```

Solo sul computer fisicamente collegato all'Atom, dopo aver identificato la porta
seriale corretta, eseguire volontariamente:

```sh
idf.py -p <porta-seriale> flash monitor
```

Questo comando sostituisce il firmware del dispositivo. Non viene eseguito dalla CI
né automaticamente da queste istruzioni. Il binario contiene la configurazione Wi-Fi:
non pubblicarlo su GitHub o come artefatto pubblico.

## 5. Verifica funzionale

Verificare Wi-Fi e health; il tasto deve portare in MUTED con LED rosso. Prima di
abilitare il microfono ricordare che l'attuale trigger è VAD, non wake word. In un
test vocale controllato controllare upload, risposta JSON, playback e ritorno READY.
Dettagli di stati e limiti in [handoff](handoff.md) e [contratto API](api.md).

## Aggiornamenti e problemi comuni

- Docker non riesce a pubblicare: controllare IP realmente assegnato e porta libera.
- Atom non raggiunge il server: confrontare i valori nei due file locali, evitare
  reti guest isolate e verificare health dal Mac.
- API provider rifiutata: controllare chiave/quota sul proprio account, senza
  pubblicare output con credenziali. Le risposte vocali non sono garantite senza Internet.
- Primo avvio lento: attendere caricamento Piper e controllare i log del solo servizio.
- `idf.py` non trovato: attivare ESP-IDF v6.0.3 nel terminale del Mac.
- Per aggiornare: worktree pulito, `git fetch origin`, `git pull --ff-only origin main`;
  build/deploy backend e flash firmware sono operazioni successive distinte.

Non sovrascrivere `.env` o `secrets.h` quando cambiano i template: confrontare e
aggiungere soltanto i nuovi campi. Le due sessioni di sviluppo usano esclusivamente
GitHub per condividere codice, decisioni e verifiche: [procedura](collaboration.md).
