# Passaggio di consegne tra DietServer e Mac

Aggiornato: 29 settembre 2026 — integrazione repository.

## Provenienza

- Backend locale di partenza: 06485f0, `Use Groq GPT-OSS and persistent Piper TTS`.
- Firmware/GitHub di partenza: 4b65d9c, `Fix project page hero image path`.
- Le cronologie erano indipendenti. Il branch integration/unified-cursore le riunisce.
- Da questa integrazione tutti lavorano su spacecdr/Cursore. Leggere collaboration.md.

## Cosa cambia per la sessione Mac

Il progetto ESP-IDF ora è in firmware/, non nella root. Compilare da quella cartella.
Il backend è in server/; il compose root appartiene al DietServer. Il compose
firmware/docker-compose.yml serve solo a una build separata con template Wi-Fi.
Portare il vecchio secrets.h locale in firmware/main/secrets.h senza versionarlo.
Le istruzioni riferite a percorsi assoluti di un altro computer non sono trasferibili.

## Stato backend rilevato dal codice

- GET /health, POST /api/v1/requests, GET /api/v1/requests/<id>/audio.
- PCM s16le mono 16 kHz; upload chunked ufficiale; Content-Length anche accettato.
- STT Groq whisper-large-v3-turbo, language=it, User-Agent esplicito.
- LLM Groq openai/gpt-oss-20b configurabile via GROQ_LLM_MODEL.
- TTS PiperVoice persistente, precaricato all'avvio; default it_IT-riccardo-x_low.
- GeminiTTS esiste ma non è chiamato dal percorso attivo. Non c'è fallback attivo.
  La variabile TTS_PROVIDER nel compose è legacy e attualmente ignorata dal routing.
- Una richiesta attiva; file di input eliminati e risposta temporanea per 300 secondi.
- Non reinterpretare come attuali i benchmark della CLI Piper che ricaricava il
  modello a ogni richiesta: il caricamento persistente è una modifica successiva.

## Stato firmware rilevato dal codice

- ESP32 originale, ESP-IDF v6.0.3, no PSRAM; C compilato dalla CI da firmware/.
- Wi-Fi primaria/fallback, tasto mute, LED, health, PDM, VAD adattivo con pre-roll.
- POST chunked; download WAV e playback I²S; GPIO33 condiviso gestito dai moduli audio.
- Wake word NON implementata. Il VAD invoca server_audio_begin: al momento può
  trasmettere senza “Cursore”. Il requisito privacy rimane aperto.
- L'integrazione non cambia sorgenti C o comportamento hardware.

## Verifiche e limiti dell'integrazione

Questa attività integra percorsi, template, documentazione e CI. Non costituisce
un nuovo test vocale end-to-end e non certifica comportamento sul dispositivo.
La build firmware in CI usa credenziali fittizie; il test hardware resta al Mac.
Il servizio già in esecuzione non viene ricreato per questa riorganizzazione.

## Prossimi lavori da coordinare

1. Mac/firmware: realizzare e misurare il gate wake word locale prima dell'upload.
2. API condivisa: validazione chunked più rigida, timeout upload, limite risposte
   compatibile con il buffer JSON firmware da 2048 byte, ID/cancellazione/cleanup errori.
3. Backend: allineare configurazione provider e routing effettivo; misurare Piper
   persistente prima di nuove decisioni prestazionali. Nessun cambio provider implicito.
4. Audio: verificare sample rate reale del WAV se si cambia voce Piper: il JSON
   attuale dichiara 16000 Hz, ma Piper scrive il sample rate nativo del modello.
5. Logging: il firmware può loggare il JSON completo; verificare la policy privacy.
6. Riproducibilità/licenze: fissare dipendenze transitive, checksum e condizioni
   delle voci prima di distribuire immagini/modelli.

Quando si riprende, aggiornare questo documento con attività e test realmente
completati. Il contenuto del repository prevale sulle supposizioni da chat passate.
