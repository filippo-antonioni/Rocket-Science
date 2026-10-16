# Avionica ESP32-S3: prove con LED, sensori e log

Questa versione integra i file allegati. Il filtro e le soglie della macchina a stati sono quelli del tuo `flight_core.h`. Sono nuovi il sistema di test, i LED, il logger e i comandi seriali. Non occorre rifare la misura dei rumori che hai gia' eseguito.

## 1. Perche' flight_core.h e' C++

`.h` significa **header**, non significa C. Il compilatore lo legge attraverso `#include "flight_core.h"` dentro `main.cpp`, quindi in questo progetto lo interpreta come C++.

Il tuo header usa `namespace volo`, `enum class`, metodi dentro `struct`, inizializzazioni dei membri e riferimenti `const Parametri&`: sono costrutti C++. I cicli, gli array e le operazioni numeriche assomigliano al C perche' la sintassi e' condivisa.

```cpp
volo::Kalman kalman;  // una variabile che contiene stato e funzioni del filtro
kalman.step(acc, quota, fresh, dt, parametri);
```

Nel C penseresti a una `struct Kalman` e a una funzione `kalman_step(&kalman, ...)`. Qui `step` e' una funzione associata a quella struttura e modifica i suoi membri. `const Parametri&` passa la configurazione per riferimento, senza copiarla e senza permettere alla funzione di modificarla. Non devi imparare ereditarieta' o template per usare il filtro.

`#pragma once` impedisce inclusioni duplicate. La definizione dei metodi nell'header e' valida in C++; non serve trasformarlo in un file `.c`. Puoi anche chiamarlo `.hpp`, ma devi aggiornare l'include. Nel pacchetto e' rimasto `.h`.

## 2. Quali file usare

Apri **questa cartella come progetto PlatformIO completo**. Non copiare soltanto il nuovo main nel vecchio progetto: ora include gli altri moduli.

| File | Funzione |
|---|---|
| `src/main.cpp` | Esegue il ciclo, interpreta comandi, aggiorna LED e comanda il servo |
| `src/flight_core.h` | Kalman e macchina a stati; invariato rispetto all'allegato |
| `src/config.h` | Pin, asse, segno, durata buffer e abilitazione servo |
| `src/scenarios.h` | Genera i sette profili di test |
| `src/sensors.h` | Acquisizione IMU/BMP280 e calibrazione |
| `src/logger.h` | Buffer RAM, coda e task scrittore LittleFS |
| `src/log_format.h` | Formato binario e CRC dei record |
| `tools/rocket.py` | Test da PC, download, cancellazione e conversione CSV |
| `platformio.ini` | Ambienti BANCO e VOLO, dipendenze e partizioni |

Non mettere `Gestione_fairing.ino`, `test_core.cpp` o `replay.cpp` dentro `src`: definirebbero altri punti di ingresso. `compile_commands.json` e' un indice generato dagli strumenti; non serve copiarlo dal vecchio computer e non prova che questa nuova versione sia compilata.

## 3. Scheda, memoria e USB

Il `.ini` allegato indica **ESP32-S3-DevKitC-1**, configurata a 8 MB. La sigla N16R8, se confermata sulla schermatura del modulo, indica 16 MB di flash e 8 MB di PSRAM. Sono due memorie diverse: la flash conserva i log dopo lo spegnimento; la PSRAM no.

La configurazione di base mantiene **8 MB**, sufficienti a questi test e ai log. Non abilita la PSRAM, quindi `INFO psram_bytes=0` non prova che il chip sia fisicamente assente. L'ambiente `volo_16mb` e' disponibile solo dopo aver confermato il modulo e `INFO flash_bytes=16777216`.

Usa la presa **USB-UART** della scheda (spesso indicata UART/COM), con un cavo dati. Sulle schede con due USB, l'altra presa puo' essere la USB nativa del chip. Non posso identificare destra/sinistra dalla sola descrizione: una foto o la sigla completa risolve l'ambiguita'.

Il pacchetto usa `ARDUINO_USB_CDC_ON_BOOT=0` per la USB-UART. Se vuoi usare la presa USB nativa, sostituisci quella riga nella sezione comune del `.ini` con:

```ini
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Il `compile_commands.json` allegato riportava CDC attivo, mentre il `.ini` allegato aveva le relative righe commentate: potevano provenire da compilazioni diverse. Usa una configurazione coerente con la porta scelta.

## 4. Collegare il LED esterno, con scheda scollegata

Serve un normale LED (rosso e' adatto), una resistenza **330 ohm** e due/tre cavetti.

1. Collega il pin della scheda marcato **4 / GPIO4** a un capo della resistenza.
2. Collega l'altro capo della resistenza all'**anodo** del LED: generalmente la gamba lunga.
3. Collega il **catodo** del LED a un pin **GND** della scheda: generalmente gamba corta e lato piatto del corpo.

La resistenza non ha verso. Anche 470 ohm va bene, con luce piu' debole. Non mettere le due gambe del LED nella stessa fila elettricamente collegata della breadboard. Non collegare il LED direttamente al GPIO senza resistenza. Il collegamento non richiede il pin 5 V.

| Segnale | Pin scelto |
|---|---|
| LED esterno di deploy, tramite resistenza | GPIO4 |
| Sensori SDA | GPIO8 |
| Sensori SCL | GPIO9 |
| GND comune | GND |
| Futuro segnale servo | GPIO18 |

Questi numeri sono GPIO, non la posizione del piedino lungo la scheda. Non cambiare i collegamenti dei sensori gia' funzionanti. Non usare i pin 19/20 per il LED: sono coinvolti nella USB nativa dell'S3.

Il LED esterno resta acceso dopo il comando di deploy e si spegne all'inizio di un nuovo test. Il test BANCO non collega mai il PWM al servo.

## 5. Primo avvio: caricare BANCO

In VS Code apri la cartella di questo pacchetto. In PlatformIO → Project Tasks → **banco** esegui **Build**, poi **Upload**, poi **Monitor**.

Oppure, dal terminale PlatformIO nella cartella del progetto:

```sh
pio run -e banco
pio run -e banco -t upload
pio device monitor -b 115200
```

Le librerie hanno versioni esplicite per evitare aggiornamenti involontari; la piattaforma 6.10.0 usa Arduino-ESP32 2.x. Non sostituire ESP32Servo con una release per core 3.x senza ricompilare e verificare.

Se i comandi del monitor non arrivano fino a quando premi Invio, e' normale: il parser aspetta una riga completa terminata da newline. Scrivi i comandi **in maiuscolo**. Al boot il sistema e' disarmato e il servo non riceve PWM.

```text
INFO
LED ON
LED OFF
```

`INFO` mostra flash rilevata, spazio LittleFS, modalita', stato della registrazione e ritardi. LED ON/OFF verifica il cablaggio, non il Kalman.

### Inizializzazione LittleFS

Se `fs_mounted=0`, inizializza una volta il filesystem con:

```text
FORMAT CONFIRM
INFO
```

**FORMAT cancella tutto il filesystem.** Non formattare se contiene log da recuperare. La nuova tabella partizioni e' diversa dal vecchio progetto: prima di caricarla salva eventuali vecchi log usando il firmware precedente. Cambiare gli offset delle partizioni puo' rendere i vecchi file inaccessibili. Il nuovo decoder legge solo il nuovo formato `.bin`.

Non usare Upload Filesystem Image o Erase Flash per scaricare i dati: sono operazioni di scrittura/cancellazione. Il firmware non formatta mai automaticamente al mount fallito.

## 6. LED RGB integrato

La prima revisione ufficiale usa GPIO48, la v1.1 GPIO38. Le schede clone possono differire; non e' il LED fisso di alimentazione.

Il default e' 48. Da disarmato puoi provare:

```text
RGB 48
RGB 38
```

Ogni comando cambia pin; il LED torna poi al colore dello stato. Guarda quale comando fa accendere il LED RGB. Salva il valore corretto in `-DRGB_PIN=48` oppure `38` nel `.ini` e ricarica il firmware. Il comando seriale non memorizza il pin dopo un reset.

| Colore | Significato |
|---|---|
| Blu tenue | Disarmato |
| Verde lampeggiante | Armato, PAD |
| Arancione | BOOST |
| Giallo | COAST |
| Viola | DESCENT, deploy comandato |
| Ciano | LANDED, oppure modalita' ORIENT |
| Rosso lampeggiante | Errore di preparazione o del logger |

Il LED esterno di GPIO4 e' indipendente e indica soltanto il comando di apertura. Sul banco lo vedi facilmente. Nel razzo chiuso, il LED integrato si vede solo attraverso una finestra/foro o una guida di luce: **il software non puo' renderlo visibile attraverso il tubo**. Per osservare lo stato dalla rampa servira' accesso ottico, un LED di stato esterno o un segnalatore aggiuntivo. GPIO4 non indica che il razzo e' pronto: indica deploy.

## 7. Test del sistema con dati artificiali

Per queste prove bastano ESP e LED; i sensori possono restare collegati ma non vengono letti. La scheda genera i dati con `syntheticSample` in `scenarios.h`. Non devi caricare migliaia di numeri a mano: il comando sceglie il profilo.

Il percorso e': **forza specifica + quota artificiale → stesso Kalman → stessa macchina a stati → LED e stesso logger**. Nessuna fase viene imposta a mano.

Il profilo nominale ha 1 s di PAD, 2 s di accelerazione verticale a 25 m/s² e poi una fase balistica con accelerazione -10.3 m/s². All'IMU simulata viene passato `a+g`, al barometro la quota calcolata. Il test dura 35 s e include il ritorno a terra. E' un profilo didattico, non la traiettoria del tuo motore e non un modello completo del paracadute/urto.

Esegui un comando, aspetta `RESULT`, poi passa al successivo:

| Comando | Perturbazione | Risultato atteso sul PC |
|---|---|---|
| `RUN NOMINALE` | Nessuna | Apertura a 7.91 s, una volta |
| `RUN RUMORE` | Bias e oscillazioni deterministiche su IMU/baro | Apertura a 7.91 s, una volta |
| `RUN PICCO_BARO` | Picco di 150 m | Picco scartato; apertura a 7.91 s |
| `RUN BUCO_IMU` | IMU NaN per 0.30 s durante COAST | Apertura a 7.91 s |
| `RUN BUCO_BARO` | Barometro mancante per 1 s | Apertura a 7.91 s |
| `RUN TIMEOUT` | IMU bloccata a +g dopo burnout, impedisce COAST | Apertura di riserva a 9.05 s |
| `RUN FALSO_DECOLLO` | Solo due campioni oltre soglia | Rimane PAD, LED spento |

Questi tempi sono dall'inizio del dataset: nel timeout gli 8 s decorrono dal decollo rilevato, intorno a 1.04 s. Nel test artificiale l'orologio del modello avanza esattamente di 10 ms per campione. Il tempo reale a parete puo' allungarsi se la scheda rallenta: vengono misurati separatamente i ritardi reali.

Il monitor stampa gli eventi e alla fine, per esempio:

```text
RESULT NOMINALE algorithm=PASS log=PASS timing=PASS deploy_ms=7910 count=1 phase=LANDED file=/log0000.bin
```

- `algorithm=PASS`: eventi, numero di aperture e finestre temporali rispettate; nei casi normali sono attraversate tutte le fasi.
- `log=PASS`: nessuna perdita rilevata dalla coda o da scritture parziali. Scarica comunque il file per verificare i CRC.
- `timing=CHECK`: almeno un intervallo del loop ha superato 20 ms. Il limite e' diagnostico, non una qualifica di sicurezza per il volo.

Il campo `phase` puo' arrivare a LANDED prima della fine dei 35 s, quindi il file viene gia' chiuso. Il report aspetta la fine della prova e il completamento della chiusura. I test generano file distinti. `FALSO_DECOLLO` salva alla fine solo gli ultimi 300 campioni PAD, per documentare lo stato senza registrare tutti i 35 s.

`STOP` interrompe un test. Aspetta `INFO` con `log_active=0` prima di lanciare un altro test o scaricare. Con dati reali, STOP e' accettato in PAD/LANDED, ma rifiutato dopo il decollo rilevato.

Per cambiare i dati artificiali modifica `syntheticSample` e ricompila BANCO. Dopo aver cambiato il profilo devi aggiornare anche le finestre attese in `testReport` e nei test host: un LED che si accende da solo non basta a dimostrare correttezza.

## 8. Orientamento: la prova che ti manca

Collega i tuoi sensori e invia:

```text
ORIENT
```

Mantieni ferma la scheda per la calibrazione. Ogni 0.2 s vedrai, in m/s²:

```text
ORIENT ax=... ay=... az=... forza=... h=... valid=1
```

Metti la scheda come sara' nel razzo sulla rampa. L'asse allineato alla lunghezza del razzo deve dare circa +9.81 oppure -9.81; gli altri circa zero se il razzo e' verticale. Quindi imposta in `src/config.h`:

```cpp
constexpr int asse = 2;   // X=0, Y=1, Z=2
constexpr int segno = +1; // -1 se l'asse scelto legge -g sulla rampa
```

La colonna `forza`, dopo questa scelta, deve essere circa **+9.81**. Capovolgendo la scheda diventa circa -9.81. Non usare il modulo del vettore: perderesti il segno necessario al COAST. I test artificiali passano gia' un'accelerazione verticale, quindi non verificano il montaggio reale dell'asse.

Termina con `STOP`. Non devi rifare l'analisi dei rumori. L'ARM reale ricontrolla la media della forza specifica e la presenza del barometro; non e' una stima 3D dell'assetto.

## 9. RAM, flash e chiusura dei file

Durante PAD si conservano gli ultimi **300 record in RAM interna**, 19,200 byte. A 100 Hz sono circa 3 s. Al decollo vengono messi in ordine cronologico nella coda, poi si aggiungono i record nuovi. La coda contiene 512 record (32,768 byte) e viene consumata da un task scrittore. Il filtro non attende una scrittura flash sincrona per ogni riga.

Prima di armare si crea il file e si scrive soltanto un header di 32 byte. Durante PAD non crescono i dati del file. STOP in PAD salva gli ultimi 300 campioni e chiude.

Al LANDED la coda viene svuotata, poi si eseguono **flush e close**. Si esegue anche un flush periodico ogni secondo dopo l'inizio della registrazione: aspettare esclusivamente LANDED rischierebbe di lasciare non sincronizzati molti dati. In caso di spegnimento brusco puoi comunque perdere record recenti, la coda RAM e una scrittura in corso.

Ogni record e' di 64 byte, comprende misure, stime, fase, contatori e CRC32:

| Durata dopo decollo | Dati circa, esclusi 3 s di prebuffer |
|---|---|
| 60 s | 384,000 byte |
| 120 s | 768,000 byte |
| 180 s | 1,152,000 byte |

Il filesystem base e' 0x4F0000 = 5,177,344 byte, circa 4.94 MiB, con parte dello spazio usata dal filesystem. La variante 16 MB offre circa 12.94 MiB di filesystem. Non c'e' OTA in queste partizioni: si aggiorna via USB.

ARM/RUN richiede almeno 1,300,000 byte liberi. La registrazione viene chiusa dopo massimo 180 s dal decollo se LANDED non arriva; l'algoritmo e il comando servo continuano. Questa riserva copre il log massimo previsto con margine, ma non sostituisce le prove sul filesystem reale.

`INFO` riporta `max_dt_us`, `max_write_us`, `max_flush_us` e `dropped`. Il task separato riduce i blocchi diretti del loop, ma flash e cache sono risorse condivise: **non garantisce da solo una cadenza perfetta**. Un overflow della coda o una scrittura parziale segnala errore, accende il rosso e non ferma Kalman/macchina a stati. Se `log=FAIL` o `dropped>0`, indagare prima del volo.

## 10. Scaricare i file sul PC Windows

Installa Python se necessario. Nel terminale, nella cartella del progetto:

```sh
python -m pip install -r tools/requirements.txt
python tools/rocket.py ports
```

Su alcuni PC il comando e' `py` al posto di `python`. Sostituisci COM5 nei comandi seguenti con la porta mostrata. **Chiudi il monitor seriale PlatformIO** prima di usare lo script: due programmi non possono occupare contemporaneamente la stessa porta.

```sh
python tools/rocket.py --port COM5 info
python tools/rocket.py --port COM5 list
python tools/rocket.py --port COM5 get /log0000.bin --out log0000.bin
python tools/rocket.py decode log0000.bin --out log0000.csv
```

Il download conserva il file sull'ESP e crea una copia sul PC. Il decoder verifica il CRC di ogni record e segnala eventuali salti di sequenza. Rifiuta file di output gia' presenti per non sovrascriverli. Il CSV puo' essere aperto con MATLAB (`readtable`) o Excel. La colonna flags contiene i bit: 1=IMU valida, 2=baro nuovo, 4=deploy, 8=PWM servo comandato, 16=dati artificiali, 32=errore numerico Kalman. `motivo`: 0=nessuno, 1=apogeo, 2=timeout.

Se un file e' troncato per spegnimento, conserva l'originale e prova:

```sh
python tools/rocket.py decode log0000.bin --out recuperato.csv --recover
```

Recupera solo il prefisso di record con CRC valido; non inventa dati mancanti. Un header danneggiato non viene riparato automaticamente.

### Svuotare i log, dopo averli scaricati

```sh
python tools/rocket.py --port COM5 erase --confirm
```

Cancella soltanto i file `/logNNNN.bin` del nuovo firmware. Non cancella il programma. Dal monitor puoi ottenere lo stesso con `ERASE CONFIRM`.

`FORMAT CONFIRM`/`format --confirm` e' diverso: cancella **tutto il filesystem** e lo inizializza. Usalo per il primo avvio o una reinizializzazione deliberata.

I comandi di download, cancellazione e formattazione sono rifiutati durante acquisizione/test/volo. Se il programma e' in ORIENT invia prima STOP. Con flight attivo non riavviare per sbloccare i comandi.

### Lanciare automaticamente tutti i test

```sh
python tools/rocket.py --port COM5 run NOMINALE
python tools/rocket.py --port COM5 suite
```

La suite dura almeno 7×35 s e si ferma se rileva FAIL o timing CHECK. Non formatta, non cancella i file e non arma i sensori reali. Prima deve esserci un filesystem inizializzato e spazio sufficiente.

## 11. Dati reali, ambiente VOLO e servo futuro

In BANCO, dopo la prova ORIENT, `ARM` attiva sensori reali → Kalman → macchina a stati → LED e logger, con servo sempre escluso. Il file viene creato ma resta senza record fino a BOOST oppure STOP. Non scuotere la scheda per simulare un intero lancio: per quello ci sono i profili controllati.

Per compilare il firmware di volo:

```sh
pio run -e volo
pio run -e volo -t upload
```

VOLO rifiuta tutti i comandi RUN artificiali. Anche qui si avvia **disarmato**. Per armare puoi inviare `ARM`, oppure, solo nel firmware VOLO, **tenere premuto BOOT per 2 secondi dopo l'accensione normale**. Il programma calibra e verifica l'orientamento, controlla lo spazio e passa al verde lampeggiante. Non tenere BOOT premuto mentre accendi/resetti: entreresti nel bootloader hardware. Sul banco non premere/reset per interrompere un test: usa STOP.

Il pulsante BOOT permette di armare usando la batteria, senza collegare un PC. Deve essere accessibile prima di montare/chiudere il vano. Dopo l'armamento non togliere alimentazione e non cambiare l'orientamento di montaggio. Un riavvio torna disarmato e perde lo stato di volo.

Per ora `cfg::abilitaServo=false`, perche' non hai il servo. L'ambiente VOLO e' quindi utilizzabile con il LED, **ma non apre fisicamente il paracadute**. Quando avrai il servo:

1. Verifica il suo modello, alimentazione e corsa meccanica con lo sketch fairing separato.
2. Conferma GPIO18 e angoli 130°/90°.
3. Imposta `abilitaServo=true` in `config.h` e ricompila VOLO.

Il PWM viene collegato solo durante ARM, non all'accensione. Con il servo abilitato, ARM comanda 130°, deploy comanda 90°. In VOLO non c'e' comando manuale di richiusura. Al reset non viene comandata automaticamente la chiusura, ma non e' implementato il recupero dello stato di volo: non e' una soluzione completa ai brownout. Il flag del PWM indica un comando, non la conferma meccanica di apertura.

## 12. Stato delle verifiche

Eseguiti sul PC:

- test originali del core e confronto numerico su 12,000 campioni; massimo scarto stato/covarianza 6.54e-13;
- i sette profili completi, 24,500 campioni; eventi e finestre temporali attesi verificati;
- buffer circolare: ordine e sovrascrittura dei campioni vecchi;
- logger con filesystem e coda simulati: nessun record flash in PAD, prebuffer senza duplicati, ordine cronologico, chiusura LANDED, snapshot PAD e rilevamento overflow;
- file binari emessi in C++ e letti da Python: campi, NaN, CRC, errore di integrita' e recupero di una coda troncata;
- controllo sintattico dei rami BANCO/VOLO con interfacce Arduino simulate.

**Non e' stata eseguita una compilazione con il toolchain ESP32**, perche' PlatformIO non era disponibile e la sua installazione nel runtime non e' riuscita. Il controllo sintattico con interfacce simulate non dimostra la compatibilita' delle versioni reali delle librerie. Il prossimo controllo sul tuo PC e' `pio run -e banco`: se compare un errore serve il testo completo a partire dalla prima riga di errore.

Non sono verificati su hardware il pin RGB del tuo clone, il filesystem reale, i picchi di latenza, l'orientamento, il servo o l'alimentazione. Il filtro resta 1D, il gating forza la riaccettazione dopo cinque scarti, e la condizione di COAST resta quella originale. I test riproducibili aiutano a vedere il comportamento; non qualificano da soli il sistema per un lancio.

I controlli host sono ripetibili, facoltativamente, con g++ e Python/NumPy (Linux o WSL), dalla cartella del progetto:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_core.cpp -o test_core
./test_core
g++ -std=c++11 -Wall -Wextra -Werror tests/test_scenarios.cpp -o test_scenarios
./test_scenarios
python tests/check_equivalence.py
python tests/test_decode.py
g++ -std=c++11 -Wall -Wextra -Werror -pthread -I tests/hardware_mocks tests/hardware_mocks/check_logger.cpp -o test_logger
./test_logger
```

`tests/hardware_mocks` contiene interfacce finte esclusivamente per questi controlli sul PC: non aggiungerle alle include directory del firmware PlatformIO. Per la prima prova pratica ti bastano Build/Upload BANCO e i comandi RUN.

## Riferimenti hardware/software

- ESP32-S3 DevKitC, revisioni e pin RGB: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html
- Board PlatformIO: https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html
- IMU e maschera di validita': https://github.com/tdk-invn-oss/motion.arduino.ICM42670P
- Partizioni Arduino-ESP32: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html
