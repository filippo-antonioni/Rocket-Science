# Avionica SN2

Questa è la cartella da aprire in PlatformIO. Il progetto principale contiene
tre ambienti; le prove hardware dedicate sono progetti separati, raggiungibili
anche dallo stesso menu.

**Avvio:** doppio clic su `AVVIA_MODALITA.cmd`, oppure su
`AVVIA_AVIONICA.cmd` nella cartella principale della repository.

| Ambiente | Dati | Uscita servo |
|---|---|---|
| `banco_psram` | Replay sintetici; comando `ORIENT` per leggere i sensori reali | Disabilitata |
| `banco_servo_psram` | Replay sintetici e comandi manuali | GPIO17; chiusura 130°, apertura 90° |
| `sensori_psram` | Acquisizione reale per durata prefissata | Disabilitata |

| Progetto dedicato | Scopo |
|---|---|
| `DiagnosiI2C/` | Rilevamento indirizzi e identificativi dei sensori |
| `DiagnosiServo/` | Prova del segnale PWM; collegamenti di loopback nella sua guida |
| `TestAlimentazione/` | Sensori e registrazione in PSRAM mentre il servo riceve comandi temporizzati |

Ogni progetto ha il proprio `platformio.ini`. Il firmware eseguito dalla ESP è
quello caricato per ultimo. Il progetto principale non definisce gli ambienti
storici `banco`, `volo` o `volo_psram`.

## Documentazione attuale

- [Uso del menu, comandi e prove](docs/USO.md)
- [Configurazione hardware](docs/HARDWARE.md)
- [Guida di montaggio in PDF](docs/hardware/Guida_montaggio_avionica.pdf)
- [Archivio delle guide precedenti](docs/storico/README.md)
- [Risultati e log](logs/README.md)
- [Codice precedente conservato](archive/README.md)

## Dove mettere mano

| File / cartella | Responsabilità |
|---|---|
| `platformio.ini` | Ambienti, dipendenze e memoria |
| `src/main.cpp` | Seleziona il programma da compilare; è l'unico ingresso sotto `src/` |
| `firmware/banco/` | Replay, filtro, stati, logger e comando del servo di banco |
| `firmware/sensori/` | Acquisizione reale e relativo logger |
| `tools/` | Programmi eseguiti sul PC |
| `tests/` | Verifiche del software sul PC con hardware simulato |
| `libera_spazio_power.py` | Backup e pulizia dei soli file POWER sulla ESP |

I file `.h` sono intestazioni C++ incluse dai sorgenti. L'estensione `.h` non
significa che siano programmi C né che vadano eseguiti separatamente.

Le implementazioni nei progetti dedicati restano separate: non sono state
accorpate durante il riordino, così ciascun progetto conserva il codice con
cui è stato provato. Le copie precedenti in `archive/` non sono compilate
dall'ambiente principale.
