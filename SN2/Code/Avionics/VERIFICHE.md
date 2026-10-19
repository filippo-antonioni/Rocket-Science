# Verifiche eseguite sul PC di sviluppo

Data: 18 ottobre 2026. Versione firmware: 4; formato log: 3.

- Verificati i 36 log SCENARI ricevuti: 149.892 record, CRC, sequenze, numero righe e corrispondenza del comando deploy con RESULT. Nessun record perso o flag di errore numerico.
- Nuovo logger RAM con filesystem e task simulati: allocazione PSRAM obbligatoria; assenza di scritture e flush durante PAD/volo/replay; ordine del prebuffer; nessun doppione al decollo; salvataggio a fine acquisizione; overflow e limite 180 s senza scritture premature; mantenimento della RAM dopo errore di scrittura; blocco dei nuovi test; recupero seriale; nuovo SAVE; corruzione RAM rilevata tramite CRC.

- Compilazione con g++ C++11, `-Wall -Wextra -Werror`, e replay di tutti i 18 scenari con entrambe le varianti: 36 esecuzioni. Nessun doppio comando di apertura, nessuna stima non finita. I CHECK di comportamento rimangono nella tabella, non sono trattati come test superati.
- Ripetizione dei sette scenari originali in BASE: tempi di apertura originali mantenuti.
- Confronto delle equazioni C++ BASE con una trascrizione NumPy delle equazioni MATLAB: 6.000 campioni a 10 ms e 6.000 con dt variabile. Errore massimo su stato/covarianza rispettivamente circa 6,54e-13 e 5,68e-13; fasi ed eventi identici. Non è un'esecuzione di MATLAB.
- Test mirati su quiete breve, barometro mancante, quota negativa, discesa continua, rollover dei tempi e barometro congelato.
- Logger con task/coda/file simulati: PAD reale senza scrittura dei record, prebuffer in ordine, chiusura, acquisizione completa del replay, scrittura a blocchi anche parziali, OFF senza file, overflow rilevato.
- Compatibilità del decoder con v2 e v3; CRC errato, file troncato e sequenza controllati; nessuna cancellazione a fronte di un download con numero di record errato.
- Sintassi del `main.cpp` controllata in BANCO e VOLO, con e senza `BOARD_HAS_PSRAM`, con interfacce hardware simulate.

**Non ho eseguito qui una build PlatformIO per ESP32 né caricato il firmware su una scheda fisica.** Le verifiche con mock non controllano linker ESP32, consumo reale di RAM, SDK, flash, USB o I2C. La prima verifica richiesta al tuo PC è Build; poi Upload, INFO, RAMTEST e le sei prove RAM. Il test della memoria fisica e le prestazioni v4 saranno risultati della tua scheda, non sono già misurati qui.

`risultati_host.csv` contiene le previsioni dei 36 replay, non dati acquisiti sulla tua ESP. Non contiene una misura reale dei tempi di scrittura.

Per ripetere i controlli su un PC di sviluppo con g++, Python e NumPy:

```text
python tests/run_host.py
```

Non serve eseguire questo comando per usare il menu banco.
