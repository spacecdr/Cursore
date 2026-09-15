# Cursore

Assistente vocale basato su M5Stack Atom Echo + DietServer.

Wake word: **Cursore**

## Server

DietServer: 192.168.123.5

Il backend è containerizzato e indipendente dai container Docker già presenti.

## Firmware

Il firmware viene mantenuto nel repository ma compilato e flashato localmente dal Mac.

## Stati LED

- Rosso: microfono disabilitato
- Verde: pronto / wake-word detection
- Giallo: ascolto
- Blu: elaborazione
- Viola: risposta vocale

Consultare AGENTS.md per specifiche e vincoli completi.

## Stato PHASE 1 / PHASE 2

Implementato e verificato:

- audit iniziale dell’ambiente Debian 13, Docker/Compose, container esistenti e porte;
- container autonomo `cursore-server` definito da questo `docker-compose.yml`;
- endpoint `GET /health` sulla porta TCP `8766`, con risposta JSON `{ "status": "ok", "service": "cursore-server" }`;
- verifica locale e tramite `192.168.123.5:8766`;
- controllo dello stato e dell’uso RAM del container.

STT, AI, TTS, firmware, wake word, Node-RED e Home Assistant non sono stati implementati.
