# Registrazioni delle prove

Tutti i file presenti nello ZIP originale sono conservati byte per byte,
compresi i tentativi incompleti e gli archivi ZIP già prodotti. L'unico
spostamento di un risultato è `risultati_host.csv`, ora in
[host/risultati_host.csv](host/risultati_host.csv).

| Percorso / suffisso | Origine |
|---|---|
| `banco_v3/` | Campagne di timing, clock e scenari |
| `banco_v4/` | Confronti RAM/OFF |
| `*_fermo/`, `*_scale/` | Acquisizioni dei sensori reali |
| `*_alimentazione/` | Download dei log POWER_TEST |
| `*_pulizia_power/` | Backup verificati prima della pulizia dei file POWER |
| `diagnosi_i2c_*.txt` | Trascrizioni delle scansioni del bus |
| `logNNNN.bin` | Prime registrazioni binarie conservate |
| `device-monitor-*.log` | Acquisizione seriale usata dall'analisi MATLAB |

Le sottocartelle contengono dati binari, CSV, grafici HTML, riepiloghi JSON e
trascrizioni. Il file `pwr0000` può ricomparire in sessioni diverse dopo una
pulizia della ESP: confrontare anche la cartella con data e ora.

Un file vuoto o un errore sperimentale non è una cache da eliminare. I log non
sono stati selezionati in base a PASS/FAIL. I file ZIP possono duplicare dati
già estratti, ma sono stati conservati come backup originali.

I nuovi risultati rimangono visibili a Git: controllare `git status` e scegliere
quali sessioni versionare. Nessuna regola globale ignora `.bin`, `.csv` o
`.json`, perché qui sono dati sperimentali.
