# Mappa della repository

## Veicolo e simulazione

| Percorso | Ruolo |
|---|---|
| `SN2/Design/SN2.ork` | Modello OpenRocket |
| `SN2/Code/Sim/*.m` | Simulatore MATLAB e analisi dei sensori |
| `SN2/Code/Sim/dati_motore.eng` | Curva di spinta letta relativamente a `parameters.m` |
| `SN2/Code/Sim/figures/` | Grafici originali |
| `SN2/Code/Gestione_fairing/` | Sketch servo originale |
| `SN2/docs/` | Materiale di riferimento originale |

## Avionica

Tutti i percorsi sotto sono relativi a `SN2/Code/Avionics`.

| Percorso | Ruolo |
|---|---|
| `platformio.ini`, `partitions_*.csv` | Configurazione principale e mappe flash |
| `src/main.cpp` | Selezione del firmware con `MODE_SENSORI` |
| `firmware/banco/` | Firmware delle simulazioni LED/servo |
| `firmware/sensori/` | Firmware di acquisizione reale |
| `tools/` | Client Python, menu e decodifica |
| `tests/` | Test attuali sul PC e interfacce hardware simulate |
| `DiagnosiI2C/` | Progetto PlatformIO indipendente per il bus I2C |
| `DiagnosiServo/` | Progetto indipendente per PWM e loopback |
| `TestAlimentazione/` | Progetto indipendente per sensori, servo e PSRAM |
| `docs/` | Istruzioni attuali, PDF hardware e note storiche separate |
| `archive/` | Vecchie copie del codice e template conservati |
| `logs/` | Registrazioni, grafici, riepiloghi e backup |

I progetti diagnostici mantengono la posizione precedente per non rompere i
percorsi relativi verso `Avionics/logs` e gli script di pulizia. Le copie del
logger nei progetti indipendenti non sono state fuse durante questo intervento.

Non esiste un file C++ da “eseguire” direttamente: PlatformIO compila il progetto
scelto e carica il programma sulla scheda. I programmi `.py` e i menu `.cmd`
sono invece eseguiti dal PC.
