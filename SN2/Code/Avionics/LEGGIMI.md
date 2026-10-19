# Un progetto, tre modalità

Questo pacchetto riunisce simulazioni LED, simulazioni con servo e prova dei sensori reali sotto **un solo `platformio.ini` e un solo menu**. Copia l'intera cartella **ModalitaAvionics** dentro la tua cartella Avionics. Non copiare il suo contenuto sopra il vecchio `src`.

Percorso previsto:

`C:\Filippo\Rocket\Rocket-Science\SN2\Code\Avionics\ModalitaAvionics`

I vecchi file possono rimanere dove sono. In questo progetto il file `src/main.cpp` seleziona un solo programma durante la compilazione; gli altri sorgenti risiedono fuori da `src`. Non vengono quindi compilati insieme due `setup()`/`loop()`. Tutte le modalità usano la PSRAM e la stessa mappa flash da 8 MB già collaudata. Non occorrono FORMAT o Upload Filesystem.

Un ambiente PlatformIO è una configurazione di compilazione. La ESP esegue il firmware caricato per ultimo: scegliere una voce di caricamento nel menu compila e carica quella modalità. Non cambia modalità semplicemente chiudendo il menu.

## Avvio

Chiudi il Monitor seriale e apri **ModalitaAvionics\AVVIA_MODALITA.cmd**, oppure dal terminale PowerShell di VSCode nella tua cartella Avionics:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\avionics.py
```

Il menu usa COM6; con **P** cambi porta.

| Voce | Cosa fa |
|---|---|
| 1 | Compila e carica BANCO LED, ambiente `banco_psram`. |
| 2 | Compila e carica BANCO SERVO, ambiente `banco_servo_psram`. |
| 3 | Compila e carica SENSORI REALI, ambiente `sensori_psram`. |
| 4 | Legge INFO per sapere quale firmware è sulla scheda. |
| 5 / 6 / 7 | Servo: chiudi 130°, apri 90°, disattiva il segnale. Richiedono modalità SERVO. |
| 8 | Tre replay con servo: NOMINALE, FALSO_DECOLLO, BUCO_BARO; BASE, RAM, WALL. |
| 9 / 10 | Sensori reali: 60 s da fermo oppure 180 s per le scale. Richiedono modalità SENSORI. |
| 11 | Sei prove RAM/OFF precedenti, in modalità LED. |
| 12 | Scegli un singolo dataset e lo esegue con servo, BASE, RAM, WALL. |
| 13 | Tutti i 18 dataset con servo, BASE, RAM, MODEL. |

Le voci dei test controllano il firmware prima di inviare comandi. Se hai caricato la modalità sbagliata, il programma chiede di caricare quella prevista. Il menu non formatta la flash.

## Cosa era già presente e cosa è stato aggiunto

Lo sketch `Gestione_fairing(1).ino` è un test manuale: ESP32Servo, GPIO18, 50 Hz, impulsi 500–2400 µs, 130° chiuso, 90° aperto. Nel programma allegato `A` apre e `C` chiude; all'avvio si collega subito il servo e si comanda la chiusura.

La v4 aveva il comando del servo nel percorso VOLO, con `abilitaServo=false`; i replay BANCO non generavano PWM. Non bastava collegare il servo per farlo muovere nei dataset.

Il nuovo ambiente **BANCO SERVO** collega il PWM solo su comando manuale CHIUDI oppure all'avvio di un replay. Mantiene i valori del tuo sketch. Non attiva ARM con sensori reali in questa modalità. Il servo viene chiuso e attende 400 ms prima di avviare ogni replay; al comando di deploy dell'algoritmo viene aperto una sola volta.

Non devi programmare il servo internamente: la ESP gli invia periodicamente gli impulsi che indicano la posizione. Le scritte 130° e 90° sono posizioni comandate; verso di rotazione e corsa reale dipendono da servo, squadretta e montaggio. Non sono una calibrazione garantita del meccanismo.

## Ordine delle prove servo

Prima prepara e verifica l'alimentazione come descritto in **COLLEGAMENTI_SERVO.md**. La sigla SG90 è leggibile nella vecchia foto; manca la sigla dello step-up e la tensione della batteria per confermare quel circuito specifico.

1. Fai la prima prova con il servo libero dal meccanismo caricato a molla, osservando il movimento della squadretta.
2. Menu **2** carica il firmware SERVO; menu **4** deve mostrare `firmware=BANCO_SERVO`, `servo_enabled=1` e `servo_pin=18`.
3. Menu **5** comanda CHIUSO 130°; menu **6** APERTO 90°. Puoi ripetere 5 → 6. Se la squadretta spinge contro un finecorsa o il servo resta sotto sforzo, interrompi l'alimentazione del servo e correggi montaggio/corsa.
4. Menu **8** esegue tre replay. All'inizio di ciascuno il servo torna a 130°. In NOMINALE e BUCO_BARO deve andare a 90° al deploy; in FALSO_DECOLLO deve restare a 130°.
5. Osserva il movimento e indica se la ESP si riavvia o il servo vibra/smette di muoversi. Il terminale da solo non misura corrente, posizione o apertura fisica del vano.
6. Passa al meccanismo reale solo dopo aver verificato direzione e corsa. Con molla/fairing da riarmare, usa un dataset alla volta dal menu **12**. La richiusura automatica del servo non ripiega il paracadute né riarma da sola il meccanismo.

Il menu 8 dura circa 4–5 minuti, download compresi. I file vengono salvati in `ModalitaAvionics\logs\<data_ora>_servo`, con risultati JSON/CSV, grafici e un nuovo ZIP. Dopo aver verificato il file e i flag servo/deploy, il client cancella soltanto il nuovo file appena scaricato; i vecchi log restano.

`servo_command=1` e `servo_check=PASS_COMMAND_ONLY` significano comando inviato dal software. Non provano che il servo alimentato si sia mosso. I CHECK algoritmici già noti rimangono visibili: aggiungere il servo non modifica le equazioni né risolve le condizioni di guasto dei sensori.

`SERVO OFF` stacca il segnale PWM, non la corrente del servo. Per togliere alimentazione serve intervenire sull'alimentatore/circuito di potenza. STOP interrompe un replay e salva i dati, ma non richiude automaticamente un servo già aperto.

## Sensori reali

Menu **3**, poi **9**: scheda ferma nella posizione di rampa durante la calibrazione e i 60 secondi successivi a OK TEST. Aspetta FINE e ZIP DA ALLEGARE. Il firmware registra per durata fissa, senza decollo o LANDED, e salva in PSRAM prima del trasferimento finale sulla flash.

Menu **10** è la prova sulle scale: primi 30 s fermo, salita e ritorno entro 150 s, ultimi 30 s fermo. Mantieni la USB collegata a un portatile, evita la sospensione e fissa scheda/breadboard/cavi su un supporto. Aspetta anche il download, che può richiedere 2–3 minuti oltre ai 180 s di registrazione. La quota barometrica è la misura più utile sulle scale; passi e inclinazioni possono peggiorare il Kalman a un asse.

I risultati vengono creati sotto `ModalitaAvionics\logs`. Le copie sulla ESP usano `/sensNNNN.bin`; i replay usano `/logNNNN.bin`. Il client sensori può cancellare solo i propri nomi. I log rimangono volatili fino al salvataggio: non resettare o cambiare firmware quando il salvataggio è ancora pendente.

## Comandi diretti da VSCode

Eseguili dalla tua cartella Avionics, con Monitor seriale chiuso. Il menu è equivalente e non richiede di ricordarli.

Caricamento SERVO:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --project-dir .\ModalitaAvionics -e banco_servo_psram -t upload --upload-port COM6
```

Comandi manuali:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\servo.py --port COM6 close
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\servo.py --port COM6 open
```

Tre replay con servo:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\servo.py --port COM6 test
```

Un singolo dataset, oppure tutti i 18 con pausa per riarmare:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\servo.py --port COM6 test --scenario NOMINALE
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\ModalitaAvionics\tools\servo.py --port COM6 test --all --clock MODEL --pause-between
```

Per sensori e LED cambia soltanto l'ambiente del comando di caricamento: `sensori_psram` oppure `banco_psram`. Per una build senza upload ometti `-t upload --upload-port COM6`. In PlatformIO di VSCode puoi aprire direttamente la cartella ModalitaAvionics e usare Build/Upload nell'ambiente desiderato.

## File per eventuali regolazioni

Angoli e GPIO del servo sono in **firmware/banco/config.h**. La costante `abilitaServo` riguarda il precedente percorso VOLO; non occorre cambiarla per BANCO SERVO, abilitato dalla configurazione dedicata. Driver, Kalman e scenari conservano il comportamento della v4. Il programma dei sensori reali è in `firmware/sensori`.

Il vecchio ambiente di volo resta nel tuo progetto precedente; questo pacchetto riunisce le tre modalità di banco richieste. Non c'è una selezione VOLO nel nuovo menu.

## Verifiche eseguite

Controllata la sintassi C++ dei tre ambienti con interfacce hardware simulate, un solo punto di ingresso. Verificati con servo simulato: chiusura/apertura, mancato attach, disattivazione, assenza di doppi comandi e collegamento agli eventi di tutti i 18 dataset. Verificati anche il controllo del firmware nel client, la coerenza dei flag e i controlli di filtro, stati e logger RAM ereditati.

Non è stata eseguita qui una build PlatformIO per ESP32 né una prova fisica di alimentazione, corrente o meccanismo. L'upload dal tuo PC eseguirà la build reale; il menu 5 → 6 e poi 8 verificano l'hardware.
