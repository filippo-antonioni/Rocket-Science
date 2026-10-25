# Test alimentazione: step-up + ESP + sensori + servo

Questa cartella e un progetto separato. Copiala INTERA dentro Avionics: deve
risultare Avionics/TestAlimentazione/platformio.ini. Non copiare il suo contenuto
sopra i file del progetto principale. Il menu e gli ambienti precedenti restano
disponibili. Questo firmware e solo una prova a banco/in ascensore: il servo e
comandato da un timer, senza identificazione del decollo o dell'apogeo.

## 1. Caricamento, una volta prima della prova

- Batteria scollegata. Stacca il filo che unisce lo step-up a 5VIN dell'ESP.
- ESP collegato al PC via USB-UART, solitamente COM6.
- Chiudi Monitor seriale e altri menu che usano COM6.
- Doppio clic su TestAlimentazione/AVVIA_TEST_ALIMENTAZIONE.cmd.
- Menu 1: compila e carica. Il menu invia poi HOLD per sospendere l'avvio su
  questa accensione; aspettare CARICATO. Non occorre un comando di avvio in volo.
- Chiudi il menu e scollega entrambe le USB dell'ESP.

Alternativa dal terminale, aperto nella cartella Avionics:

    & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\TestAlimentazione\tools\prova.py --port COM6 upload

Il progetto usa la stessa mappa partizioni dei precedenti: non esegue erase,
format o uploadfs. Non abilita un filesystem diverso su una partizione nuova.
Le librerie vengono gestite da PlatformIO automaticamente.

## 2. Collegamenti sulla breadboard (batteria e USB scollegate)

Prima verifica 5,0 V stabili all'uscita dello step-up. Non regolare la vitina
mentre l'ESP e connesso. Serve un puntale del multimetro affidabile.

| Da | A |
|---|---|
| Batteria positiva | Step-up IN+ |
| Batteria negativa | Step-up IN- |
| Step-up OUT+ 5,0 V | Linea breadboard dedicata ai 5 V |
| Step-up OUT- | Linea GND comune |
| ESP 5VIN | Linea 5 V |
| ESP GND | Linea GND |
| Servo rosso | Linea 5 V direttamente dallo step-up |
| Servo marrone | Linea GND |
| Servo arancione | GPIO17 |
| ESP 3V3 | Linea SEPARATA a 3,3 V per entrambi i sensori |
| IMU e BMP GND | Linea GND comune |
| IMU e BMP SDA/SDI | GPIO8 |
| IMU e BMP SCL/SCK | GPIO9 |
| IMU CS e BMP CSB/CS | 3,3 V |
| IMU AD0 e BMP SDO | GND |

Il GPIO16 non serve. Non unire la linea 3,3 V a quella 5 V. Verifica gli
eventuali tagli a meta delle linee della breadboard: una linea puo non essere
continua per tutta la lunghezza. Tieni il servo e la batteria fissati, evitando
che i fili tirino sui sensori durante il trasporto.

Durante questa prova l'ESP deve ricevere energia SOLO da 5VIN: entrambe le sue
USB e la USB-C dello step-up rimangono scollegate. Il servo puo muoversi gia
nella fase di preparazione, quando viene portato a 130 gradi.

## 3. Prova autonoma

Collega la batteria e tieni ferma la scheda nella posizione prevista sul razzo:
la forza configurata e -X e a riposo deve essere circa +9,8 m/s2. La scheda
appoggiata orizzontale puo essere usata per carico/tempi, ma rende inaffidabile
la stima Kalman in questo firmware a singolo asse.

| LED RGB | Significato |
|---|---|
| Giallo | Attesa iniziale di 30 secondi dall'accensione |
| Viola | Controllo PSRAM, chiusura iniziale e calibrazione dei sensori: tenere fermo |
| Blu | Acquisizione iniziata: questo e t = 0 del test |
| Azzurro/ciano | Acquisizione con servo comandato aperto |
| Arancione | Acquisizione finita; salvataggio in flash in corso |
| Verde fisso | Prova completa e file/riepilogo salvati |
| Rosso | Errore o prova incompleta: scaricare i dati; non significa automaticamente calo di tensione |
| Bianco tenue | HOLD ricevuto dal PC: attesa annullata fino al prossimo reset |

Dall'accensione al blu passano 30 secondi PIU controllo RAM e calibrazione.
Il conto alla rovescia del servo parte dal BLU, non dal collegamento batteria.
Da quel momento:

| Tempo acquisizione | Azione |
|---|---|
| 0..60 s | Sensori registrati, servo comandato chiuso a 130 gradi |
| 60 s | APRI a 90 gradi |
| 90 s | CHIUDI a 130 gradi |
| 120 s | APRI a 90 gradi |
| 180 s | Fine acquisizione; resta il comando aperto durante il salvataggio |
| Dopo il salvataggio | LED verde; PWM ancora attivo nell'ultima posizione |

Puoi salire/scendere in ascensore dopo la calibrazione: il movimento del servo
e indipendente dall'altezza raggiunta. Per confrontare il comportamento a riposo
e in movimento, rimani fermo per i primi 20-30 secondi dal blu, poi usa
l'ascensore. Non servono due piani o tempi di corsa precisi. La sequenza non si
ripete da sola: riparte al prossimo reset/nuova alimentazione.

## 4. Misurazione dei 5 V

Multimetro su tensione CONTINUA (V DC), nero nella presa COM e rosso nella presa
V/ohm, non in quella 10 A. Puntale nero su GND/OUT-, puntale rosso su OUT+,
oppure sulla linea 5 V vicino a ESP e servo. Lascia i collegamenti di misura
fermi e isolati: non far toccare + e -.

Annota tensione a riposo, durante APRI/CHIUDI e durante il salvataggio finale.
Se disponibile, usa la funzione MIN/MAX. Un multimetro normale puo non cogliere
abbassamenti molto brevi; anche un log perfetto non dimostra che non ce ne siano.
Il programma NON usa l'ADC e NON misura la tensione: non collegare OUT+ a un GPIO.

I record vengono scritti in PSRAM durante tutti i 180 secondi. La flash riceve
i dati solo alla fine: questa prova verifica il comportamento con la strategia
RAM scelta, non una scrittura flash continua durante il movimento del servo.
Il servo resta alimentato e comandato durante il salvataggio finale. Non
spegnere finche il LED e arancione. Confrontando con la prova precedente,
attenditi circa 13 secondi per salvare tre minuti, ma il tempo puo variare.

Se la scheda torna al giallo e poi al viola durante la prova, puo essersi
riavviata. Un reset prima del salvataggio perde i record ancora in RAM: il file
puo contenere solo l'header. Il codice registra il motivo di avvio nel riepilogo
dei test che arrivano alla fine, incluso BROWNOUT se segnalato dal chip. Un calo
profondo puo risultare POWERON, quindi quel campo da solo non esclude un guasto
di alimentazione. Il cambio di porta/suono USB non serve per questa verifica.

## 5. Scaricamento dopo la prova

- Aspetta LED verde. Se rosso senza piu salvataggio, conserva anche quel risultato.
- Scollega la batteria; stacca il filo fra step-up OUT+ e ESP 5VIN.
- Collega USB-UART dell'ESP al PC. Non fare upload: i log sono gia sulla flash.
- Apri lo stesso CMD e scegli 3, entro i 30 secondi di attesa iniziale. Il client
  invia HOLD, impedendo l'avvio di una nuova prova su questa accensione.
- Scarica i soli /pwrNNNN.bin e .json, verifica CRC/sequenze, genera CSV, grafici
  HTML e uno ZIP dentro Avionics/logs. Allega lo ZIP e le letture del multimetro.

Se dopo il ricollegamento USB sono gia passati 30 secondi, il test potrebbe
essere iniziato: nessun comando lo interrompe automaticamente. Chiudi il menu,
premi RESET e scegli 3 durante la nuova attesa iniziale. Cosi interrompi SOLO
la nuova prova non richiesta, mantenendo i file gia salvati della prova prima.
Puoi evitare la fretta tenendo gia il menu aperto prima di collegare la USB.

Alternativa dal terminale Avionics:

    & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\TestAlimentazione\tools\prova.py --port COM6 download

Il client conserva tutti gli originali sulla ESP. I vecchi log banco e sensori
non vengono toccati. Prima o poi la flash puo riempirsi: LED rosso e FLASH_SPACE
significano che serve liberare file gia scaricati, non formattare o cambiare
partizioni. Il logger lascia una riserva e non sovrascrive file esistenti.

In caso di errore salvataggio, conserva l'alimentazione e l'output disponibile.
I comandi `ram` e `save` consentono recupero/riprova solo se esiste gia un
collegamento seriale che non interrompa e non raddoppi l'alimentazione dell'ESP.
Non collegare entrambe le alimentazioni e non improvvisare un cambio sorgente
per tentare il recupero. Spegnendo l'ESP perdi la RAM; i file gia in flash,
anche parziali, rimangono e il client li conserva per l'analisi.

## Cosa ci diranno i risultati

- Tempi massimi e letture IMU/barometro prima/dopo ciascun comando al servo.
- Tre comandi PWM realmente richiesti dal firmware, con tempi registrati:
  nessun sensore di posizione conferma il movimento meccanico.
- Integrita della registrazione e operazioni flash durante la cattura.
- Conteggi separati: ritorno driver IMU non zero oppure dati asse ancora
  mancanti/sentinel con ritorno zero; non bastano da soli a diagnosticare i cavi.
- Voltmetro esterno per i 5 V; senza un oscilloscopio non possiamo escludere
  transitori brevissimi. Non e una validazione del firmware di volo.

## Verifica del pacchetto

Sono inclusi test host per sequenza temporale, PWM e analisi dei log. Questi
controlli non equivalgono a compilazione PlatformIO per ESP32 o a prova fisica:
la compilazione hardware viene eseguita sul tuo PC con menu 1.
