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
