# Contratto Atom → cursore-server

Endpoint LAN attuale: http://192.168.123.5:8766.
Nessun nome provider o chiave API deve essere conosciuto dal firmware.

## Upload ufficiale

HTTP/1.1 POST /api/v1/requests con Transfer-Encoding: chunked.
Body: PCM signed 16-bit little-endian, mono, 16000 Hz; niente header WAV.
Il client deve aprire lo stream solo dopo la futura wake word locale e inviare
blocchi man mano che acquisisce. Al termine VAD invia `0\r\n\r\n`, quindi attende JSON.
Il server scrive su file temporaneo, completa il WAV e avvia STT dopo la fine del body.
Content-Length è supportato per file di test con dimensione nota.

Header del firmware:

| Header | Valore |
|---|---|
| Content-Type | application/octet-stream |
| X-Request-Id | ID esadecimale con trattini, 8–64 caratteri |
| X-Device-Id | atom-echo |
| X-Audio-Sample-Rate | 16000 |
| X-Audio-Channels | 1 |
| X-Audio-Encoding | pcm_s16le |

Limite attuale: 30 secondi / 960000 byte PCM; una richiesta concorrente, le altre
ricevono 429. Il request ID collega log e audio ma non implementa idempotenza.
Il backend oggi genera un ID se assente e non impone X-Device-Id: questi sono
limiti dell'implementazione, non garanzie di validazione completa.

## Risposta

JSON con request_id, status, transcript, response_text, audio.url,
timings_ms e providers. Il GET su audio.url restituisce audio/wav con Content-Length.
Il WAV viene letto progressivamente dall'Atom; disponibilità attuale 300 secondi.
Il firmware deve leggere il formato WAV effettivo. La voce Riccardo predefinita
produce PCM mono 16 kHz; cambiare voce richiede verificarne i metadati.

GET /health restituisce status=ok e service=cursore-server.
Errori attuali: 400 input, 415 header audio, 429 occupato, 502 pipeline, 404 risorsa.
Non esistono ancora endpoint di cancellazione, autenticazione dispositivo o retry
idempotenti. Non esporre questo servizio su Internet.

## Lavori aperti

Il parser chunked corrente necessita di ulteriori limiti/timeout per input incompleto
o malformato. Il firmware contiene un buffer risposta da 2048 byte: mantenere risposte
compatibili e coordinare l'eventuale estensione. La wake word rimane il prerequisito
per rispettare il vincolo di invio audio; non è implementata dall'integrazione repo.
