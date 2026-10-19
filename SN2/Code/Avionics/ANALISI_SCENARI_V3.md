# Analisi delle 36 prove ricevute

Archivio: `20261018_113248_863441_scenari.zip`, ricevuto il 18 ottobre 2026.

Verificati tutti i 36 file binari: **149.892 campioni**, CRC e sequenze integri, conteggi coerenti con RESULT, nessun record perso e nessun flag di errore numerico. Il numero di aperture e il tempo del campione di deploy coincidono con i riepiloghi. Tutti i replay usano BATCH e MODEL.

## Due problemi distinti

**Temporizzazione:** tutte le prove hanno `timing=CHECK`. Gli intervalli massimi di campionamento arrivano a **43.631 µs, cioè 43,631 ms**, contro il passo desiderato di 10 ms. L'assenza di record persi significa che i campioni effettivamente elaborati sono arrivati nel log; non significa che la CPU abbia rispettato la frequenza desiderata.

Il confronto precedente TIMING mostrava intervalli molto più regolari con OFF. È una motivazione concreta per provare la raccolta RAM e spostare le scritture a fine acquisizione. BATCH da solo non ha eliminato le pause. Il risultato hardware della nuova modalità RAM resta da misurare.

**Algoritmo e guasti dei sensori:** la PSRAM non corregge errori di stima né condizioni della macchina a stati. In MODEL i rallentamenti non cambiano il tempo matematico fornito al filtro, perciò un diverso `deploy_ms` fra BASE e CANDIDATO rivela una differenza dell'algoritmo, non il costo del salvataggio.

## Risultati che orientano le prossime decisioni

I tempi sotto sono tempi del modello. Per il profilo nominale l'apogeo vero è a circa 7,854 s. COAST_ZERO usa un profilo diverso, con apogeo a circa 8,099 s.

| Scenario | Deploy BASE | Deploy CANDIDATO | Cosa emerge |
|---|---:|---:|---|
| NOMINALE | 7,910 s | 8,280 s | La conferma barometrica aggiunge 370 ms. |
| RUMORE | 7,910 s | 8,310 s | La conferma aggiunge 400 ms. Non è un ritardo causato dalla flash. |
| BUCO_BARO | 7,910 s | 8,280 s | Il buco previsto da questo scenario non impedisce il deploy; resta il costo della conferma. |
| BUCO_IMU_BURNOUT | 7,430 s | 8,280 s | BASE apre circa 424 ms prima dell'apogeo. CANDIDATO evita l'anticipo ma l'accuratezza rimane CHECK. |
| BUCO_BARO_APOGEO | 7,910 s | 9,050 s | CANDIDATO attende il timeout: la conferma dipende dal barometro proprio quando manca. |
| COAST_ZERO | 9,050 s | 8,520 s | BASE non riconosce COAST con forza specifica zero; la variante lo gestisce. |
| BARO_CONGELATO | 6,210 s | 9,050 s | BASE apre circa 1,644 s in anticipo; la variante evita quell'apertura ma arriva al timeout con stima molto errata. |
| PICCO_BARO_LUNGO | 8,000 s | 8,280 s | Entrambe hanno errori di quota fino a circa 56,38 m prima del deploy. |
| SENSORI_ASSENTI | 7,840 s | 9,050 s | Entrambe algorithm=CHECK, nessuna arriva a LANDED. accuracy=PASS in questo dataset non prova una gestione valida dei guasti. |
| TIMEOUT | 9,050 s | 9,050 s | Il timer interviene, ma la quota al deploy è sovrastimata di circa 17,36 m. |
| FALSO_DECOLLO / PAD_RUMOROSO | Nessuna | Nessuna | Corretta assenza di apertura nei due dataset. |

PARACADUTE, QUOTA_NEGATIVA e FALSO_LANDED superano i criteri di algoritmo/accuratezza/atterraggio previsti con entrambe le varianti, mantenendo però `timing=CHECK`. Sono profili prescritti: non simulano la risposta meccanica al comando del servo.

## Perché la quota del timeout può sembrare più alta

La quota stampata in EVENT è una **stima del Kalman**, non la quota vera del dataset. TIMEOUT forza una condizione anomala dell'accelerometro per verificare il timer: una stima errata può risultare più alta anche mentre la traiettoria vera scende. Non bisogna dedurre la direzione del moto dal confronto fra quote stimate in due scenari diversi. Nei CSV/HTML v3 la quota vera è disponibile separatamente.

## LANDED e salvataggio RAM

Nel vecchio profilo nominale l'arresto al suolo è istantaneo; il filtro può restare molto sotto la quota vera prima di recuperare. Un `landing=PASS` verifica il riconoscimento rispetto al contatto prescritto e non garantisce una quota stimata corretta. La variante CANDIDATO migliora alcuni riconoscimenti, ma un barometro congelato può risultare falsamente rassicurante per il suo controllo di stabilità.

Con i sensori reali la v4 avvia il salvataggio RAM quando viene riconosciuto LANDED. Quindi questo stato non è trascurabile: falso LANDED significa scrittura anticipata; LANDED mancato significa dati ancora volatili. Nei replay il logger aspetta invece la fine dell'intero dataset, per poter osservare tutto il comportamento.

## Passo deciso

La v4 cambia soltanto il logger e la configurazione PSRAM. Le sei nuove prove usano BASE per misurare il beneficio senza introdurre contemporaneamente modifiche del filtro. La scelta definitiva dell'algoritmo e la gestione dei sensori guasti rimangono aperte: né BASE né CANDIDATO hanno superato tutti gli scenari critici.
