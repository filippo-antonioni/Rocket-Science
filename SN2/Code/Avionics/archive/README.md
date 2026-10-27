# Archivio dei sorgenti precedenti

Nessun codice del progetto è stato eliminato durante il riordino.

- `legacy/src/`: intestazioni rimaste nel vecchio `Avionics/src`. Quasi tutte
  coincidono con `firmware/banco`, ma `config.h` differisce: è quindi conservata
  una copia completa, senza deduplicare arbitrariamente.
- `legacy/tests/`: vecchia cartella `test/`, con i sorgenti originali. La
  disposizione relativa `tests/` e `src/` conserva i percorsi degli include.
- `datalogger/`: testo originale del programma di acquisizione precedente.
- `platformio_templates/`: README generici del modello PlatformIO per
  `include/` e `lib/`.

Questa cartella non viene compilata dagli ambienti principali. I test correnti
sono in `../tests` e usano `../firmware/banco`. L'archivio legacy conserva solo
i file effettivamente presenti nel vecchio albero: non è un firmware completo
alternativo da caricare sulla ESP.
