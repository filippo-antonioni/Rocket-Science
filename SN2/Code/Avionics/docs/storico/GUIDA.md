# Guida v4: registrazione in PSRAM

## Cosa cambia

La PSRAM viene usata come memoria temporanea per i dati. Il Kalman non diventa più potente o più veloce per questo: togliamo dal periodo di acquisizione le scritture flash che possono interrompere il lavoro della CPU.

Il buffer occupa **2.097.152 byte**, con record da 88 byte. Offre 23.831 slot. Per i sensori reali manteniamo un limite esplicito di **180 secondi dal decollo**, più gli ultimi 300 campioni di PAD. A 100 Hz bastano circa 1,61 MB. Non serve occupare subito tutti gli 8 MB disponibili. All'avvio di un test RAM occorrono anche almeno 2.162.688 byte liberi sul filesystem, per il salvataggio finale e un margine.

Le impostazioni N16R8 usano flash quad e PSRAM octal (`qio_opi`, `BOARD_HAS_PSRAM`). La dimensione fisica e il test di lettura/scrittura vanno verificati con INFO e RAMTEST sulla scheda. La mappa flash rimane quella da 8 MB già usata, anche quando il chip fisico ne ha 16. Non è necessario modificarla per usare la PSRAM.

## Con sensori reali

1. **Prima di ARM:** verifica PSRAM (automatica se non già fatta), calibrazione, verifica spazio flash, creazione del file con sola intestazione e flush.
2. **PAD:** ring circolare negli slot iniziali della PSRAM, ultimi 300 campioni. Attendere sulla rampa non riempie il buffer.
3. **BOOST:** congela la posizione del ring e inizia a riempire la parte successiva. Non copia in blocco i 300 campioni al decollo.
4. **COAST/DESCENT:** aggiunge campioni, senza scritture o flush del logger sulla flash. Nessuna allocazione dinamica del buffer durante il volo.
5. **LANDED rilevato dal software:** termina l'acquisizione e salva in ordine cronologico. Copia piccoli blocchi dalla PSRAM alla SRAM interna prima di scriverli. Verifica CRC dei record e dimensione del file, chiude il file e pubblica `log_active=0`.

`ARM` usa RAM negli ambienti `banco_psram`/`volo_psram`, LEGACY negli ambienti senza PSRAM. `ARM RAM` richiede esplicitamente RAM anche nel firmware di fallback; se manca la memoria, fallisce. BASE rimane la variante usata con sensori reali. Questa scelta serve a isolare la modifica del logger, non approva BASE per il volo.

**Il riconoscimento LANDED conta più di prima:** se viene dichiarato troppo presto, può avviare la scrittura mentre il razzo si muove ancora; se non viene riconosciuto, i dati rimangono in RAM. I problemi dell'algoritmo individuati negli scenari v3 non sono risolti da questa modifica.

Se si raggiunge il limite di 180 s, la raccolta si interrompe e aumenta `dropped`; l'algoritmo continua e il logger aspetta LANDED. Se il buffer si riempie in un replay, conserva il prefisso e segnala la perdita. **Nessun fallback automatico alla flash durante l'acquisizione.** Il successivo salvataggio conserva il prefisso, con `log=FAIL` per rendere visibile la perdita.

STOP salva il ring se il sistema reale è ancora in PAD. Come nella v3, STOP è bloccato durante un volo rilevato con sensori reali; è consentito durante un replay. Se LANDED non arriva, non esiste in questa versione un comando che forzi l'arresto di un volo reale: serve definire e collaudare la procedura di recupero prima del volo. Non spegnere confidando che il contenuto RAM sia già sulla flash.

## Nei replay

I replay conservano l'intero dataset, compresi PAD e campioni successivi a LANDED, fino al termine prescritto (35 o 75 s). Questo permette di valutare gli errori di riconoscimento degli stati. Il salvataggio RAM parte alla fine del replay o con STOP. È una differenza intenzionale rispetto all'acquisizione reale.

MODEL usa i tempi del dataset, WALL i tempi realmente misurati. In MODEL una pausa della CPU allunga il replay: `deploy_wall_us/1000 - deploy_ms` non coincide automaticamente con il ritardo fisico di un paracadute. WALL consente il confronto usando il tempo reale, ma rimane una simulazione: la discesa prescritta dal dataset non risponde al comando di apertura.

`core_us` misura filtro, evidenze, macchina a stati e comando uscita, escludendo sensori, copia record e seriale. `dt_reale_us` misura l'intervallo fra gli inizi di due campioni, includendo gli effetti complessivi. I tempi di salvataggio finale non fanno parte di `max_dt_us` del replay RAM.

## Memoria volatile e recupero

Una batteria saldata può ridurre i falsi contatti, ma non impedisce un reset software, watchdog o un calo di tensione. Un reset perde i dati ancora in PSRAM. La nuova strategia accetta questo compromesso per evitare le scritture durante la raccolta.

Se il salvataggio fallisce, `ram_pending=1`: niente riuso del buffer da parte di RUN/ARM/RAMTEST. `GET RAM` può esportarlo via USB e `SAVE` riprova su un nuovo nome di file, conservando quello parziale. Il download verifica CRC e sequenza. Un CRC errato nella RAM può rendere impossibile recuperare un file integro; non viene mascherato come successo.

La copia RAM di un log già salvato rimane disponibile fino al successivo RUN/RAMTEST/reset. Il menu 6 la scarica in un nuovo file locale. La campagna non cancella file remoti falliti. `flush()` Arduino non restituisce un esito: il controllo finale sul PC è necessario per verificare anche il contenuto scritto.

## Menu e comandi equivalenti

Usa normalmente `AVVIA_BANCO.cmd`. I seguenti comandi sono per PowerShell aperto nella cartella Avionics, con Monitor seriale chiuso:

```powershell
# Configurazione e memoria
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/rocket.py --port COM6 info
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/rocket.py --port COM6 ramtest

# Scarica un file flash senza cancellarlo (scegli un nome locale non esistente)
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/rocket.py --port COM6 get /log0000.bin --out logs/copia_log0000.bin

# Recupero RAM e nuovo tentativo di salvataggio
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/rocket.py --port COM6 get RAM --out logs/recupero_RAM.bin
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/rocket.py --port COM6 save
```

I precedenti comandi `campaign timing`, `campaign scenari`, `campaign clock` restano disponibili. La nuova campagna si chiama `campaign ram`. Tutte richiedono `--out` con una cartella nuova. Non devi avviarle manualmente per la procedura del menu.

`erase --confirm` cancella tutti i log flash e `format --confirm` formatta il filesystem: **non sono passi della migrazione v4**. Usali soltanto dopo aver scaricato ciò che vuoi conservare. Il menu non li esegue.

## File e configurazione

| File | Ruolo |
|---|---|
| `platformio.ini` | Configurazione build; ambiente predefinito `banco_psram`. |
| `src/main.cpp` | Programma della ESP: acquisizione, comandi, stati, uscite. |
| `src/logger.h` | Modalità OFF/LEGACY/BATCH/RAM, test PSRAM, salvataggio e recupero. |
| `src/ram_capture.h` | Ring PAD e memoria del volo, senza copia al decollo. |
| `src/flight_core.h` | Kalman e stati BASE/CANDIDATO, invariati dalla v3. |
| `src/scenarios.h` | I 18 dataset, invariati. |
| `src/config.h` | Pin, asse, segno, dimensioni e limiti. |
| `tools/banco.py`, `tools/rocket.py` | Menu e comunicazione sul PC. |
| `tests/` | Verifiche per lo sviluppo, non da caricare sulla ESP. |

Gli header `.h` fanno parte del programma C++ incluso da `main.cpp`; non si eseguono separatamente. Sono conservati SDA8, SCL9, LED4, RGB48, servo18, asse X e segno −1, e la lettura IMU già corretta. I nuovi file binari mantengono il formato v3 (88 byte/record), con codice modalità RAM=3; il decoder legge anche i vecchi v2.

Colori RGB: blu disarmato; verde lampeggiante PAD armato; arancione BOOST; giallo COAST; viola DESCENT; ciano LANDED/ORIENT; rosso lampeggiante errore durante acquisizione. Il LED esterno indica il comando di deploy, non il completamento del salvataggio. Per il salvataggio usa l'output seriale.

## Riferimenti tecnici

- [Configurazione ufficiale PlatformIO di una scheda ESP32-S3 con PSRAM octal](https://github.com/platformio/platform-espressif32/blob/develop/boards/freenove_esp32_s3_wroom.json): riferimento per `qio_opi`, non identificazione della tua scheda.
- [Espressif, RAM esterna su ESP32-S3, IDF 4.4.7](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/external-ram.html): restrizioni della memoria esterna durante operazioni flash.
- [Espressif, allocazione per capacità](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/mem_alloc.html): allocazione esplicita con `MALLOC_CAP_SPIRAM`.

La consegna è una versione di banco da compilare e misurare sulla tua ESP. Gli ambienti `volo` sono presenti per continuità del progetto, ma non fanno parte della procedura di prova richiesta ora.
