# CURSORE — AI Voice Assistant

## Repository condiviso — leggere prima di lavorare (29 settembre 2026)

Questo repository è ora un monorepo usato da due sessioni/computer.
Leggere integralmente `README.md`, `docs/collaboration.md` e `docs/handoff.md`
prima di riprendere il lavoro: la chat precedente può descrivere uno stato superato.

- `server/`: backend DietServer. `docker-compose.yml` nella root avvia solo il backend.
- `firmware/`: progetto ESP-IDF prima collocato nella root del repo GitHub.
  Sul Mac eseguire `idf.py` da `firmware/`; vedere `firmware/README.md`.
- `docs/`: documentazione condivisa e GitHub Pages.
- `.env` e `firmware/main/secrets.h` sono locali, ignorati da Git; si pubblicano solo template.
- Prima di iniziare: controllare worktree, fare fetch, leggere gli ultimi commit e l'handoff.
  Usare un branch per attività; coordinare le modifiche a protocollo, compose, CI e documenti comuni.
- Non sovrascrivere modifiche dell'altra sessione, non fare force push, non usare reset distruttivi.
- Aggiornare l'handoff con modifiche, test realmente eseguiti e limiti prima di pubblicare.
- Build/flash hardware sul Mac; un aggiornamento Git non autorizza il riavvio dei servizi sul server.

Stato corrente: STT Groq, LLM Groq GPT-OSS e Piper persistente nel backend.
GeminiTTS esiste ma non è chiamato dal percorso attivo; `TTS_PROVIDER` non lo seleziona.
Il firmware contiene VAD/upload/playback, ma la wake word locale manca e il codice
attuale apre lo stream al rilevamento VAD. Non dichiarare quindi soddisfatto il vincolo
"nessun audio prima della wake word". Le sezioni seguenti restano requisiti del progetto,
non una dichiarazione di funzionalità tutte completate.

## Obiettivo

Realizzare un assistente vocale denominato "Cursore" utilizzando:

- M5Stack Atom Echo originale ESP32-PICO-D4
- DietServer Debian 13 x86_64
- Docker
- AI provider intercambiabile
- Node-RED disponibile per future automazioni
- nessun Home Assistant

Il sistema deve essere modulare e non deve dipendere da uno specifico provider AI.

---

## Hardware server

Host: FUTRO S720
OS: Debian GNU/Linux 13 (trixie)
CPU: AMD GX-217GA dual-core 1.65 GHz
RAM: circa 3.5 GiB
Server LAN: 192.168.123.5

Le risorse hardware sono limitate.

NON installare LLM pesanti localmente sul server.

Il server deve principalmente funzionare come orchestratore.

---

## Hardware client

M5Stack Atom Echo originale.

ESP32-PICO-D4.

Il dispositivo dispone di:

- Wi-Fi
- microfono
- speaker
- pulsante
- LED RGB

Il flash del dispositivo NON deve essere eseguito dal server.

Il firmware verrà compilato/flashed dal Mac collegando fisicamente Atom Echo via USB.

I sorgenti firmware devono comunque essere mantenuti in:

/root/DOCKER/cursore/firmware

---

# WAKE WORD

La parola di attivazione è:

CURSORE

Il riconoscimento della wake word deve avvenire LOCALMENTE sull'Atom Echo.

Preferire microWakeWord o soluzione equivalente compatibile con ESP32 originale.

NON inviare continuamente il microfono al server.

L'audio può lasciare il dispositivo solamente dopo il rilevamento della wake word.

I file relativi al modello personalizzato devono essere mantenuti sotto:

wakeword/

Il modello finale dovrà possibilmente comprendere:

cursore.tflite
cursore.json

---

# PULSANTE

Il pulsante fisico dell'Atom deve funzionare esclusivamente come toggle:

MICROFONO ATTIVO
        ↓
pressione
        ↓
MICROFONO DISABILITATO
        ↓
pressione
        ↓
MICROFONO ATTIVO

Lo stato deve rimanere valido durante il normale funzionamento.

Quando il microfono è disabilitato:

- non effettuare wake-word detection
- non acquisire audio destinato all'assistente
- non inviare audio
- LED rosso fisso

---

# LED RGB

Implementare esattamente questi stati.

## ROSSO

Microfono disabilitato.

LED rosso fisso.

## VERDE

Microfono attivo.

Sistema disponibile.

In ascolto esclusivamente della wake word "Cursore".

LED verde fisso.

## GIALLO

Wake word rilevata.

Il dispositivo sta ascoltando/registrando la richiesta dell'utente.

LED giallo fisso.

## BLU

Registrazione conclusa.

Audio inviato al server.

Trascrizione/elaborazione AI in corso.

LED blu fisso.

## VIOLA

Risposta ricevuta.

Riproduzione vocale della risposta tramite speaker Atom.

LED viola durante tutta la riproduzione.

Terminata la risposta:

ritornare automaticamente a VERDE.

---

# MACCHINA A STATI

MUTED
  LED ROSSO

READY
  LED VERDE

WAKEWORD DETECTED
  LED GIALLO

LISTENING
  LED GIALLO

PROCESSING
  LED BLU

SPEAKING
  LED VIOLA

READY
  LED VERDE

Gestire appropriatamente timeout, errori di rete e mancata risposta.

In caso di errore recuperabile, ritornare in READY.

---

# AUDIO

Obiettivo:

"Cursore"
   ↓
wake word locale
   ↓
registrazione richiesta
   ↓
VAD / rilevamento fine frase
   ↓
invio audio
   ↓
server
   ↓
STT / AI / TTS
   ↓
audio risposta
   ↓
speaker Atom Echo

Audio mono.

Preferire PCM/WAV 16 kHz quando appropriato.

La fine della richiesta deve essere individuata automaticamente tramite VAD/silenzio.

L'utente NON deve premere pulsanti per terminare la registrazione.

---

# WIFI

Sono disponibili due reti.

Priorità:

1. Pepis
2. PepisRoofTop

Entrambe utilizzano una password definita nei secrets locali.

NON inserire la password Wi-Fi:

- nei sorgenti
- in AGENTS.md
- in README.md
- in Git
- nell'immagine Docker

Implementare failover automatico.

Tentare prima Pepis.

Se indisponibile, tentare PepisRoofTop.

Quando Pepis ritorna disponibile non è necessario interrompere una sessione funzionante soltanto per cambiare AP.

---

# SERVER CURSORE

Percorso:

/root/DOCKER/cursore

Creare un container dedicato.

Nome preferito:

cursore-server

Il container deve essere definito esclusivamente dal:

/root/DOCKER/cursore/docker-compose.yml

NON aggiungerlo al docker-compose.yml principale.

Esporre inizialmente il servizio soltanto sulla LAN.

Preferire una API REST semplice.

Endpoint iniziali suggeriti:

GET /health

POST /api/ask

L'implementazione può introdurre endpoint aggiuntivi quando tecnicamente giustificato.

---

# AI

Il backend AI deve utilizzare una abstraction layer.

NON accoppiare firmware o API a Gemini, OpenAI o altro specifico provider.

Interfaccia concettuale:

Audio
  ↓
STT
  ↓
LLM
  ↓
TTS
  ↓
Audio

Implementare provider intercambiabili.

Priorità iniziale:

1. soluzioni gratuite / free tier
2. nessun abbonamento obbligatorio
3. buona comprensione dell'italiano
4. buona latenza
5. TTS italiano naturale

Le API key devono essere secrets/configurazione runtime.

Mai inserirle nei sorgenti.

---

# NODE-RED

Node-RED è già presente sul server.

NON modificarlo nella prima fase.

Prevedere architetturalmente una futura integrazione.

In futuro Cursore potrà distinguere tra:

DOMANDA
    → AI

COMANDO DOMOTICO
    → Node-RED/API/MQTT

L'integrazione sarà implementata solo su esplicita richiesta.

---

# HOME ASSISTANT

HOME ASSISTANT È ESPRESSAMENTE ESCLUSO.

Non installarlo.

Non proporlo come dipendenza.

Non utilizzarlo indirettamente.

---

# DOCKER ESISTENTE

ATTENZIONE.

Il server contiene già servizi Docker in produzione.

NON:

- arrestare container esistenti
- riavviare container esistenti
- rimuovere container esistenti
- modificare immagini esistenti
- effettuare docker system prune
- modificare network Docker esistenti senza necessità
- modificare /root/DOCKER/docker-compose.yml
- modificare Caddy
- modificare Node-RED
- modificare Grafana
- modificare Prometheus
- modificare Portainer
- modificare Squid
- modificare altri progetti presenti sotto /root/DOCKER

Tutto ciò che riguarda Cursore deve rimanere sotto:

/root/DOCKER/cursore

Prima di modificare qualsiasi elemento esterno a tale directory chiedere esplicita autorizzazione.

---

# SICUREZZA

Non committare:

.env
secrets.yaml
API key
password
token
certificati
credenziali

Creare .gitignore appropriato.

Il container deve girare con il minimo dei privilegi necessari.

Non utilizzare --privileged salvo necessità tecnica documentata e autorizzata.

---

# LOG

Implementare log leggibili almeno per:

wake request ricevuta
dimensione audio
STT iniziato/completato
provider AI utilizzato
tempo AI
TTS iniziato/completato
tempo totale richiesta
errori

NON loggare credenziali.

Evitare di loggare integralmente conversazioni se non esplicitamente abilitato.

---

# OBIETTIVO PRIMA VERSIONE

La prima milestone deve dimostrare:

1. Atom Echo si collega al Wi-Fi
2. failover Pepis -> PepisRoofTop
3. pulsante abilita/disabilita microfono
4. rosso = OFF
5. verde = READY
6. wake word "Cursore"
7. giallo = registrazione
8. VAD individua fine frase
9. audio raggiunge DietServer
10. blu = processing
11. backend restituisce risposta audio
12. viola = playback
13. Atom riproduce risposta
14. ritorno automatico al verde

---

# METODO DI SVILUPPO

Procedere incrementalmente.

NON implementare tutto in un unico passaggio non verificato.

Ordine:

PHASE 1
Audit ambiente e struttura progetto.

PHASE 2
Server minimale /health.

PHASE 3
Pipeline audio server di test.

PHASE 4
Firmware Atom: Wi-Fi + LED + pulsante.

PHASE 5
Microfono e speaker.

PHASE 6
Wake word Cursore.

PHASE 7
VAD + upload audio.

PHASE 8
STT.

PHASE 9
LLM.

PHASE 10
TTS.

PHASE 11
Pipeline completa.

PHASE 12
Ottimizzazione latenza e robustezza.

---

# REGOLA FONDAMENTALE

Prima di ogni modifica significativa:

- verificare lo stato corrente
- non assumere la presenza di software/librerie
- controllare compatibilità ESP32-PICO-D4
- non compromettere i servizi esistenti

Quando una scelta tecnica è incerta, privilegiare:

semplicità
stabilità
basso consumo di RAM
bassa latenza
manutenibilità

rispetto alla complessità.
