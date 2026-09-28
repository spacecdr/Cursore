# Firmware standalone Atom Echo

Questa cartella contiene il progetto prima pubblicato nella root di spacecdr/Cursore.
Leggere [handoff](../docs/handoff.md) e [collaborazione](../docs/collaboration.md).
Licenza applicativa: [MIT](LICENSE).

Target: Atom Echo originale ESP32-PICO-D4, 4 MB flash, nessuna PSRAM.
Ambiente: ESP-IDF v6.0.3; pin e hardware in [specifiche](../docs/specification.md).
Non è ancora implementata la wake word: il VAD attualmente avvia direttamente l'upload.

## Build nativa sul Mac

Attivare l'installazione ESP-IDF v6.0.3 del proprio computer. Da questa cartella,
solo se main/secrets.h non esiste, copiarlo da main/secrets.example.h e inserire
localmente le credenziali Wi-Fi e CURSORE_SERVER_HOST / CURSORE_SERVER_PORT.
main/config.h usa questi valori locali; mantiene i default storici del DietServer
solo per compatibilità con i vecchi secrets.h privi dei due campi.

```sh
idf.py set-target esp32
idf.py build
```

Il flash avviene solo su richiesta esplicita sul Mac con Atom collegato via USB.
Non è eseguito da CI, Docker Compose o DietServer.

## Build Docker senza credenziali

Dalla root del monorepo:

```sh
docker compose -f firmware/docker-compose.yml build firmware-build
docker compose -f firmware/docker-compose.yml run --rm firmware-build
```

L'immagine include solo sorgenti e template; secrets.h è escluso dal contesto.
Questa build verifica la compilazione con credenziali fittizie. Non monta il checkout
locale e non produce automaticamente un firmware configurato per la propria rete.
Per build/flash con credenziali reali usare l'ambiente nativo Mac; non pubblicare il binario.

Il workflow GitHub build.yml esegue la stessa selezione target/build da firmware/.
