# Avionica v4 — prova della PSRAM

Questa versione raccoglie i dati nella PSRAM e li salva sulla flash alla fine. Il prossimo passo è misurare il risultato sulla tua scheda: non devi modificare C++.

## Installazione

1. Chiudi il Monitor seriale di VSCode e fai una copia di sicurezza della cartella Avionics.
2. Estrai lo ZIP. Apri la cartella **Avionics_v4**: contiene `platformio.ini`, `src`, `tools`, `AVVIA_BANCO.cmd`.
3. Copia **il contenuto** nella tua cartella `C:\Filippo\Rocket\Rocket-Science\SN2\Code\Avionics`, sostituendo i file omonimi. La cartella `logs` con i risultati precedenti resta al suo posto. Copia anche il nuovo `src/ram_capture.h`.
4. Collega la USB-UART già funzionante, normalmente **COM6**.
5. Fai doppio clic su **AVVIA_BANCO.cmd**. Se necessario puoi avviarlo da PowerShell nella cartella Avionics:

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/banco.py
   ```

## Nel menu: 1 → 2 → 3 → 4 → 5

| Numero | Operazione | Risultato atteso |
|---|---|---|
| **1** | Build `banco_psram` | `SUCCESS` |
| **2** | Upload `banco_psram` | `SUCCESS` |
| **3** | INFO | `revision=4`, `firmware=BANCO`, `fs_mounted=1`. Su N16R8 con PSRAM riconosciuta: `psram_bytes=8388608`. |
| **4** | Test memoria | `OK RAMTEST bytes=2097152 patterns=4` |
| **5** | Sei prove automatiche | `FINE 6/6`, poi **ZIP DA ALLEGARE** |

**Non formattare la scheda.** Le partizioni rimangono quelle già utilizzate: abilitare PSRAM non richiede di cancellare i log. Se INFO non risponde o la PSRAM resta a zero, conserva l'output. Il menu **10** compila e carica la configurazione `banco` senza PSRAM, per tornare alla configurazione di avvio precedente; non cancella l'intera flash. Non usarlo se devi recuperare dati ancora in RAM: un upload riavvia la scheda.

Il test verifica i **2 MiB destinati ai log**, non certifica tutti gli 8 MiB fisici. Se non riesce ad allocare o verificare questo buffer, la modalità RAM non parte. Non sostituisce silenziosamente la PSRAM con SRAM interna o con flash.

## Quali sono le sei prove

| Prova | Dataset | Registrazione | Tempo |
|---|---|---|---|
| 1 | RUMORE | OFF | MODEL |
| 2 | RUMORE | RAM | MODEL |
| 3 | RUMORE | OFF | WALL |
| 4 | RUMORE | RAM | WALL |
| 5 | BUCO_BARO | RAM | WALL |
| 6 | PARACADUTE | RAM | WALL |

Tutte usano **BASE**, così confrontiamo la registrazione senza cambiare contemporaneamente l'algoritmo. I dataset e le equazioni sono quelli della v3. MODEL mantiene riproducibili i campioni; WALL tiene conto del tempo realmente trascorso sulla ESP. La prova PARACADUTE dura 75 secondi; le altre 35. Con salvataggi e download, considera circa **7–10 minuti**.

La campagna esegue RAMTEST anche automaticamente. Durante `SAVING RAM` il test è finito ma i dati vengono trasferiti sulla flash: aspetta. Non premere RESET. Il LED di deploy può restare acceso durante salvataggio e download; non indica che il programma sia bloccato.

Alla fine manda lo **ZIP indicato dal menu** e l'output INFO. Trovi tutto nella repo, sotto `logs/banco_v4/`. Non serve ripetere le 36 prove SCENARI per questo primo confronto.

## Come riconoscere un buon risultato

- `log=PASS`, `dropped=0`, `ram_pending=0`: nessuna perdita rilevata; il PC verifica anche CRC, sequenza e numero di record del file scaricato.
- `flash_ops_capture=0`: il logger non ha eseguito scritture o flush durante la raccolta RAM. Il file vuoto con intestazione è creato prima del test; i dati sono scritti dopo.
- `max_dt_us` e `predeploy_max_dt_us`: confrontiamo i valori RAM con OFF, puntando a intervalli vicini ai 10 ms. La soglia diagnostica resta 20 ms; non è stata alzata.
- `save_ms`, `max_write_us`, `max_flush_us`: in modalità RAM riguardano il salvataggio finale, fuori dall'acquisizione. Possono essere grandi senza rallentare il test già concluso.

`FINE 6/6` significa campagna completata, non automaticamente tutti i criteri superati. Un `CHECK` rimane nel riepilogo. Un errore log/numerico interrompe la campagna e conserva i risultati disponibili.

## Se il salvataggio fallisce

Se compare `ram_pending=1`, lascia la scheda alimentata e non fare upload/reset. La campagna prova già a scaricare la copia RAM. Puoi riprovare con **menu 6** (recupero USB) o **menu 11** (nuovo tentativo di salvataggio flash). I nuovi test e RAMTEST sono bloccati finché il salvataggio resta pendente.

Il menu scarica e verifica ogni nuovo file riuscito, poi elimina **soltanto quel file** dalla scheda per liberare spazio. I vecchi file restano. In caso di errore non cancella il file fallito. Un reset perde la copia RAM, mentre i file già salvati sulla flash restano.

Per il funzionamento reale PAD/BOOST/LANDED e i limiti: **GUIDA.md**. Per i risultati delle 36 prove appena ricevute: **ANALISI_SCENARI_V3.md**. Per le verifiche del codice: **VERIFICHE.md**.
