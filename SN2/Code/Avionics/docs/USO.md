# Uso degli strumenti

Eseguire `AVVIA_MODALITA.cmd` dalla cartella Avionics oppure
`AVVIA_AVIONICA.cmd` dalla radice. Il menu non collega la porta finché non
si sceglie un'operazione. Chiudere Monitor seriale e altri client concorrenti.

## Menu principale

| Voce | Operazione |
|---|---|
| 1 | Compila e carica simulazioni LED, `banco_psram` |
| 2 | Compila e carica simulazioni servo, `banco_servo_psram` |
| 3 | Compila e carica sensori reali, `sensori_psram` |
| 4 | Legge INFO del firmware presente |
| 5 / 6 / 7 | Servo: chiudi / apri / disattiva PWM; richiede BANCO_SERVO |
| 8 | Tre replay servo: NOMINALE, FALSO_DECOLLO, BUCO_BARO |
| 9 / 10 | Sensori: 60 s da fermo / 180 s in movimento; richiede SENSORI |
| 11 | Sei prove LED RAM/OFF; richiede BANCO senza servo |
| 12 / 13 | Un dataset servo / tutti i 18 dataset |
| 14 | Apre il menu del test di alimentazione; da lì si sceglie upload o download |
| 15 | Compila e carica DIAGNOSI I2C, poi legge le scansioni |
| 16 | Compila e carica DIAGNOSI SERVO, che avvia la sua sequenza automatica |
| 17 | Scarica, verifica e poi rimuove dalla ESP i soli log POWER |
| 18 | Legge DIAGNOSI I2C senza upload; premere RESET dopo l'apertura della lettura |
| P / 0 | Cambia porta / esci |

Le voci 1–13 mantengono la numerazione precedente. Le voci 14–18 richiamano
gli strumenti già presenti; non riscrivono i loro protocolli.
Una modifica della porta nel sottomenu POWER vale per quel sottomenu.

## Terminale di VS Code

Aprire il terminale nella cartella `SN2/Code/Avionics`. I comandi seguenti sono
per PowerShell; sostituire `COM6` se la porta è diversa.

```powershell
# Menu
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\tools\avionics.py

# Sola compilazione: non serve collegare la ESP
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio run -e banco_psram

# Compilazione e caricamento della modalità sensori
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio run -e sensori_psram -t upload --upload-port COM6

# Monitor seriale: Ctrl+C per chiuderlo
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m platformio device monitor --port COM6 --baud 115200
```

Se Python manca nel percorso previsto, aprire PlatformIO in VS Code e
completarne l'installazione. I client seriali usano `pyserial`, elencato in
`tools/requirements.txt`.

## Controllo dei sensori dopo il montaggio

1. Voce **15**. Il programma richiede, dopo l'upload, un ciclo di alimentazione
   dei sensori: seguire il messaggio a video. La guida del progetto è in
   [DiagnosiI2C/LEGGIMI.txt](../DiagnosiI2C/LEGGIMI.txt).
2. Controllare tutte e cinque le scansioni: IMU `0x68`, ID `0x67`; BMP280
   normalmente `0x76`, ID `0x58`; `bus_errors=0`. Lo scanner controlla anche
   gli indirizzi alternativi `0x69` e `0x77`.
3. Per verificare le misure, ricaricare **1**, aprire il Monitor e inviare
   `ORIENT` seguito da Invio. Attendere la calibrazione con la scheda ferma.
   `STOP` termina la stampa. Questo comando non richiede un nuovo log.

Lo scanner conferma la comunicazione e gli identificativi, non la precisione
delle misure. Il suo firmware non aggiorna il LED RGB: leggere il terminale.

## Sensori, servo e alimentazione

Per la prova dei soli sensori scegliere **3**, poi **9** o **10**. Il nome
“scale” della voce 10 è l'etichetta del file: l'acquisizione dura 180 secondi
anche se si usa un ascensore. Tenere ferma la scheda durante la calibrazione.

Per i replay con servo scegliere **2**, poi prima **5 → 6 → 5**, quindi **8**
o un singolo dataset con **12**. Un contatore di comandi non misura il movimento
fisico del servo: quest'ultimo va osservato.

La voce **14** apre [TestAlimentazione](../TestAlimentazione/LEGGIMI.md).
Quella prova usa sensori reali e comandi servo temporizzati: non rileva un
apogeo e non misura elettricamente i 5 V. Dopo l'avvio automatico e la
preparazione, acquisisce per 180 s e comanda apri/chiudi/apri a 60/90/120 s.
Attendere il salvataggio finale prima di togliere alimentazione.

## Dati e memoria

I risultati PC vengono creati sotto `Avionics/logs/`. I dati in PSRAM sono
volatili e diventano persistenti solo dopo il salvataggio sulla flash.

La voce **17** richiede il firmware POWER_TEST inattivo. Crea e verifica un
backup ZIP sul PC prima di rimuovere i file `/pwrNNNN.bin` e `/pwrNNNN.json`
dalla ESP. Non formatta la memoria e non pulisce i log di altre modalità.
Il comando diretto rimane:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\libera_spazio_power.py --port COM6
```

I download a 115200 baud possono richiedere alcuni minuti. Non caricare altro
firmware per risolvere un download lento: prima controllare l'avanzamento e
lo stato della prova.

Il vecchio `AVVIA_BANCO.cmd` resta disponibile per la campagna PSRAM e il
recupero RAM; la sua voce 10 ora spiega che il vecchio ambiente `banco` non è
definito, anziché tentare un upload destinato a fallire.
