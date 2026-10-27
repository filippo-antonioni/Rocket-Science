# Portare il riordino nella repository sul PC

Il pacchetto contiene una cartella `Rocket-Science` completa di sorgenti,
documenti e tutti i log ricevuti. **Non contiene `.git`**: la cronologia, i
branch e il collegamento al remoto devono rimanere quelli della tua repository.

Non limitarti a incollare sopra la vecchia cartella: Windows aggiungerebbe i
file nuovi senza rimuovere le copie che qui sono state spostate.

## Procedura consigliata, con copia di sicurezza

1. Chiudi VS Code, menu e Monitor seriale.
2. In `C:\Filippo\Rocket`, rinomina l'attuale cartella `Rocket-Science` in
   `Rocket-Science_backup_prima_riordino`. Se esiste già, scegli un altro nome.
   Mantienila intera, inclusa la cartella nascosta `.git`.
3. Estrai lo ZIP nuovo in `C:\Filippo\Rocket`. Il risultato deve essere
   `C:\Filippo\Rocket\Rocket-Science\README.md`, senza un secondo livello
   `Rocket-Science\Rocket-Science`.
4. In Esplora file attiva **Visualizza → Mostra → Elementi nascosti**. Copia
   **soltanto la cartella `.git`** dal backup alla nuova `Rocket-Science`.
   Non copiarci di nuovo `SN2`, altrimenti reintrodurresti il vecchio disordine.
5. Se hai prodotto nuovi log o modificato codice **dopo aver creato lo ZIP
   inviato**, recuperali dal backup e confrontali con i nuovi percorsi prima
   del commit. La consegna contiene solo ciò che era nello ZIP ricevuto.
6. Apri la nuova `Rocket-Science` in VS Code e controlla dal terminale:

   ```powershell
   git status --short
   git remote -v
   git diff --stat
   ```

   Il remoto deve essere quello già usato. Se Git dice che non è una repository,
   controlla la posizione di `.git`: **non eseguire `git init` per rimediare**.

7. Per il menu fai doppio clic su `AVVIA_AVIONICA.cmd`. Per i comandi PlatformIO
   apri `SN2/Code/Avionics`; per verificare solo la compilazione, senza upload:

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio run -e banco_psram
   ```

   La prima build ricrea `.pio` e scarica le dipendenze: quelle cartelle sono
   intenzionalmente assenti dalla consegna.

8. Quando hai controllato le modifiche, dalla radice della repository:

   ```powershell
   git add -A
   git diff --cached --stat
   git diff --cached --name-status
   ```

   Verifica l'elenco prima di proseguire. `git add -A` registra anche le rimozioni
   dai vecchi percorsi, necessarie per gli spostamenti. Può includere modifiche
   locali già presenti prima del riordino: non è un commit automatico.

9. Esegui commit e push con il tuo flusso abituale, ad esempio:

   ```powershell
   git commit -m "Organize SN2 repository and document avionics workflows"
   git push
   ```

Il backup rimane fuori dalla repository nuova. Non eliminarlo finché non hai
verificato sorgenti, log, funzionamento e commit. Per tornare indietro, chiudi
VS Code, rinomina la nuova cartella e ripristina il nome della cartella backup.

## Cosa aspettarsi da Git

- Alcuni file compariranno inizialmente come eliminati e altri come nuovi;
  Git può riconoscerli come rinominati dopo lo staging.
- `compile_commands.json` e due cache Python erano già tracciati nello ZIP:
  la loro rimozione dal prossimo commit è intenzionale.
- `.gitignore` evita nuove cache, ma non riduce il peso dei commit storici.
- Tutti i log sono ancora presenti e versionabili. Questa consegna non riscrive
  la cronologia e non esegue commit, push, formattazione ESP o upload firmware.
