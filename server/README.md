# Backend DietServer

Il compose operativo è `../docker-compose.yml`; il contesto build è questa cartella.
`index.py` serve HTTP e gestisce i file temporanei; `providers.py` contiene gli
adattatori STT/LLM/TTS. Python 3.11 è necessario anche per audioop, usato dal codice.

Percorso attivo: Groq Whisper → Groq GPT-OSS → PiperVoice persistente.
GeminiTTS è presente ma non selezionato nel percorso attivo. La sola variabile
TTS_PROVIDER non cambia il routing. Il compose richiede GROQ_API_KEY e l'indirizzo
CURSORE_LAN_IP; GEMINI_API_KEY è opzionale. Creare `.env` nella root dal template
senza sovrascrivere le credenziali esistenti. Mai passare chiavi come build arg
o inserirle nel Dockerfile. La porta host CURSORE_PORT ha default 8766.

Il servizio gira non-root, filesystem read-only, tmpfs 64 MiB, capability eliminate.
Le dipendenze e le voci Piper sono nell'immagine. Modificare provider o voce richiede
verificare che il WAV e i metadati API concordino. Non usare i benchmark vecchi della
CLI Piper come misure del runtime persistente.

Vedere [API](../docs/api.md), [handoff](../docs/handoff.md) e
[collaborazione](../docs/collaboration.md). Il download dal repository non aggiorna
automaticamente il container in esecuzione.
