# Riordino della repository — 27 ottobre 2026

## Conservazione dei file

L'archivio ricevuto conteneva **420 file di progetto**, oltre a metadati Git,
dipendenze scaricate, build e cache. Tutti i 420 file hanno una destinazione
nella consegna:

- **411** sono conservati byte per byte; **33** di questi cambiano percorso.
- **9** sono modificati per documentazione, percorsi, menu o test. Le copie
  originali sono conservate in `riordino/originali_modificati.zip`.
- Tutti i **281 file già presenti in `Avionics/logs`** rimangono nello stesso
  percorso e con lo stesso contenuto. Anche `risultati_host.csv` è conservato,
  sotto `logs/host/`.

La mappa completa con SHA-256 è in [inventario.json](riordino/inventario.json).
Per una tabella più semplice da consultare usare
[mappa_file.csv](riordino/mappa_file.csv).

## Spostamenti

| Prima | Adesso |
|---|---|
| `SN2/SN2.ork` | `SN2/Design/SN2.ork` |
| `SN2/Lista della spesa.txt` | `SN2/docs/lista_acquisti_originale.txt` |
| Screenshot nella cartella `SN2/Code` | `SN2/docs/images/screenshot_2026-04-21.png` |
| PNG nella cartella `Sim` | `Sim/figures/` |
| Vecchie guide sparse in `Avionics` | `Avionics/docs/storico/` |
| PDF di montaggio in `Avionics` | `Avionics/docs/hardware/` |
| Vecchie intestazioni in `Avionics/src` | `Avionics/archive/legacy/src/` |
| Vecchia cartella `Avionics/test` | `Avionics/archive/legacy/tests/` |
| `datalogger_backup.txt` | `Avionics/archive/datalogger/` |
| README generici di `include` e `lib` | `Avionics/archive/platformio_templates/` |
| `Avionics/risultati_host.csv` | `Avionics/logs/host/risultati_host.csv` |

I percorsi del progetto principale, dei client, dei log e dei tre progetti
diagnostici restano stabili. `Avionics/src` contiene ora solo il vero ingresso
`main.cpp`; i sorgenti attivi sono in `firmware/`.

## Modifiche al codice e agli strumenti

1. `tests/replay.cpp` e `tests/log_fixture.cpp`: include rivolti al firmware
   attuale, invece che alle vecchie copie archiviate.
2. `tests/test_servo.cpp`: il test usa il pin configurato anziché imporre il
   vecchio GPIO18. Prima del riordino il test falliva contro GPIO17.
3. `tests/check_equivalence.py`: aggiornata soltanto la descrizione del percorso.
4. `tools/banco.py`: la voce 10 spiega l'assenza dell'ambiente storico `banco`
   e non tenta più un upload di un ambiente inesistente.
5. `tools/avionics.py`: aggiunte voci 14–18 per richiamare i programmi esistenti
   di alimentazione, I2C, PWM e pulizia POWER; voci 1–13 conservate.
6. `Sim/noise_analysis.m`: sostituito il percorso assoluto del PC con un percorso
   relativo al file MATLAB. Nessuna equazione modificata.

**Firmware C++, parametri attivi, dataset sintetici, algoritmi, partizioni e
dipendenze PlatformIO rimangono byte per byte quelli ricevuti.** Non è stata
fatta una nuova conversione MATLAB/C++ o una fusione dei logger.

Sono nuovi il README generale, gli indici di cartella, le guide attuali,
`AVVIA_AVIONICA.cmd`, il comando `tools/verifica_repo.py` e i test dei nuovi
collegamenti del menu. `.editorconfig` definisce convenzioni per modifiche
future senza riformattare in massa i sorgenti esistenti.

## File esclusi dalla consegna

- **2.073 file** di build, dipendenze gestite da PlatformIO e cache, nelle
  cartelle `.pio`, `.cache` e `__pycache__`.
- **3 file** generati dall'editor: database di compilazione e configurazioni
  locali C/C++ e debug.
- **1.719 file interni a `.git`**: non distribuiti per non sovrascrivere la
  cronologia del PC. Copiare la propria `.git` seguendo la guida di trasferimento.

Le dipendenze sotto `.pio/libdeps` vengono riscaricate da PlatformIO in base
ai `platformio.ini` conservati. Le copie esatte ricevute restano recuperabili
dallo ZIP originale e dal backup sul PC; non vengono scambiate per sorgenti
del progetto. La lista dei file generati esclusi è in
[esclusi_generati.csv](riordino/esclusi_generati.csv).

`.gitignore` è aggiornato per evitare le nuove cache; non ignora i dati
sperimentali. La rimozione dei file generati da un prossimo commit non riduce
automaticamente la cronologia Git precedente.

## Applicazione sul PC

Seguire [AGGIORNAMENTO_PC.md](AGGIORNAMENTO_PC.md). Nessun commit o push è stato
eseguito e nessun firmware è stato caricato sulla ESP durante questo lavoro.
