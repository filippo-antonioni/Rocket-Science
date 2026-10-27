# Configurazione hardware del progetto

Questa pagina descrive i collegamenti e le impostazioni già usati nel progetto.
La riorganizzazione della repository non modifica il firmware sulla scheda.

| Collegamento | Configurazione attuale |
|---|---|
| Scheda | ESP32-S3 N16R8; target PlatformIO `esp32-s3-devkitc-1` |
| Bus sensori | SDA GPIO8, SCL GPIO9, massa comune |
| IMU | ICM42670P; AD0 a GND, CS a 3V3; indirizzo `0x68` |
| Barometro | BMP280; SDO a GND, CSB a 3V3; indirizzo `0x76` |
| Alimentazione sensori | 3V3 dell'ESP; non il ramo 5 V del servo |
| Servo nei programmi di banco e POWER_TEST | Segnale GPIO17; rosso al ramo 5 V, marrone a GND |
| Posizioni comandate | 130° chiuso, 90° aperto; da verificare sul montaggio meccanico |
| LED RGB | GPIO48 |
| LED esterno di deploy | GPIO4, con resistenza in serie |
| Asse del filtro monodimensionale | X con segno −1; forza specifica = −ax |
| Comunicazione PC | USB-UART, 115200 baud; COM6 è solo il valore iniziale dei menu |

Il pin `servoPin=18` rimane nel vecchio sketch fairing, nell'archivio e nella
configurazione dei soli sensori, che non aziona il servo. Per il servo attivo
valgono `firmware/banco/config.h` e il progetto dedicato POWER_TEST: GPIO17.

## Alimentazione e montaggio

La guida dettagliata è [Guida_montaggio_avionica.pdf](hardware/Guida_montaggio_avionica.pdf).
Il montaggio previsto usa una batteria 1S, lo step-up regolato a 5 V, un ramo
per il servo e un ramo verso il pin 5VIN dell'ESP; tutti condividono GND.

Per il collegamento al PC con USB, scollegare la batteria e il collegamento
step-up OUT+ → ESP 5VIN. Per la prova autonoma ripristinare il cablaggio a
circuito spento e scollegare USB prima di alimentare dalla batteria.

## Memoria

La scheda fisica è N16R8, ma i progetti attivi mantengono la mappa flash da
**8 MB** già usata nelle prove (`partitions_8mb.csv`); la PSRAM è abilitata
negli ambienti principali e nel test di alimentazione. Non selezionare la
mappa da 16 MB solo per riordinare la repository: cambierebbe le partizioni.

`partitions_16mb.csv` è conservato, ma non è la mappa selezionata dagli
ambienti principali. I valori realmente rilevati vanno letti con `INFO`.

## Letture e limiti

Con l'asse −X verticale, una scheda ferma dovrebbe misurare una forza specifica
vicina a +g. Se la scheda è appoggiata orizzontalmente, la gravità può essere
prevalentemente su Z: i dati grezzi restano utili, ma la stima monodimensionale
non rappresenta quella configurazione come una rampa correttamente orientata.

I test POWER registrano i tempi e i comandi, non la tensione di alimentazione.
Nessuna modifica elettrica o validazione aggiuntiva è implicita nel riordino.
