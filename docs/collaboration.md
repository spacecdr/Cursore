# Lavorare in due su Cursore

Il repository canonico è https://github.com/spacecdr/Cursore. Dal 29 settembre 2026
riunisce backend DietServer e firmware Mac, prima inizializzati come due repository
con cronologie indipendenti. L'integrazione conserva entrambe le cronologie.

## Prima di ogni sessione

1. Leggere AGENTS.md, README.md e docs/handoff.md, anche se si dispone di una chat precedente.
2. Eseguire `git status --short --branch` e `git fetch origin`.
3. Se ci sono modifiche locali, conservarle su un branch proprio prima di aggiornare.
   Non usare `reset --hard`, force push o checkout che le cancellino.
4. Su main pulito usare `git pull --ff-only origin main`; se fallisce, esaminare la
   divergenza e integrarla su un branch. Non forzare l'allineamento.
5. Aprire un branch dall'ultimo main: `git switch -c server/<attivita>` oppure
   `git switch -c firmware/<attivita>`.

## Ripartizione del lavoro

| Sessione | Ambito principale | Ambiente di verifica |
|---|---|---|
| DietServer | server/, compose root, provider e API | Docker/backend LAN |
| Mac collegato ad Atom | firmware/, wake word, GPIO/audio | ESP-IDF, USB e hardware |
| Entrambe | docs/, AGENTS, API, CI, template | Revisione coordinata |

La ripartizione serve al coordinamento e non limita dove si può leggere/modificare
il codice. Una modifica al protocollo va descritta nell'handoff e accompagnata dalle
modifiche compatibili sull'altro lato. Non presumere che l'altra sessione legga la chat.

## Pubblicare il lavoro

- Prima del commit: controllare `git diff` e `git diff --cached`; aggiungere file
  selezionati, mai credenziali o artefatti audio/firmware.
- Aggiornare docs/handoff.md: commit/branch di partenza, modifiche, verifiche
  effettive, verifiche mancanti e prossimo passo. Evitare risultati ipotetici.
- Push del branch e pull request verso main; verificare CI e conflitti con gli
  aggiornamenti dell'altra sessione prima di integrare.
- Su entrambi i computer aggiornare main dopo l'integrazione. Il pull non fa
  deploy né flash: queste operazioni sono separate e dipendono dall'incarico.

Non usare `git pull` sul server per sostituire alla cieca il compose operativo.
Non riavviare servizi di altri progetti; non fare pruning Docker globale.

## Migrare il vecchio clone firmware sul Mac

I sorgenti prima nella root sono ora in firmware/. Se `git status` mostra modifiche,
salvarle su un branch e integrarle esplicitamente. Conservare fuori dal checkout una
copia privata di `main/secrets.h` PRIMA di aggiornare, senza aggiungerla a Git.
Dopo il pull, spostare le credenziali in `firmware/main/secrets.h` e controllare che
`git check-ignore firmware/main/secrets.h` le segnali come ignorate.

I vecchi `build/`, `managed_components/` e `sdkconfig` nella root sono output locali:
non copiarli nel repository condiviso. Ricompilare da firmware/ con ESP-IDF v6.0.3.
Non fare flash automatico come parte della migrazione.

## Credenziali

- Backend: .env locale da .env.example. Non sovrascrivere un .env esistente.
- Wi-Fi: firmware/main/secrets.h da secrets.example.h. Nessuna password nei template.
- GitHub: ogni computer si autentica autonomamente; non copiare il token del server
  nel progetto o nella chat. L'autenticazione CLI non pubblica i file locali.
- Le chiavi API non servono alle build CI. Non caricare artefatti compilati con
  credenziali Wi-Fi reali su GitHub Actions, release o Pages.

I workflow controllano i nomi dei file sensibili versionati; questo non sostituisce
una revisione dei contenuti per segreti inseriti per errore in altri file.
