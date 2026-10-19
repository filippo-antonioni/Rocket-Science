# Cosa cambia e cosa resta da decidere

## Aggiornamento v4 — 18 ottobre 2026

Questa consegna aggiunge registrazione RAM, test PSRAM e salvataggio finale. Non modifica Kalman, macchine a stati, dataset o soglie di verifica. Le analisi algoritmiche v3 riportate sotto restano valide; i dati della tua scheda sono ora riassunti in **ANALISI_SCENARI_V3.md**. La procedura corrente è **INIZIA_QUI.md**, non le campagne diagnostiche iniziali citate nello storico.

Nuovi file/comandi: `ram_capture.h`, `RAMTEST`, modalità `RAM`, `GET RAM`, `SAVE`; ambiente predefinito `banco_psram`. Il buffer da 2 MiB è allocato solo in PSRAM, verificato prima dell'uso e non sostituito automaticamente con flash. Il filesystem mantiene la mappa precedente. Le prove del menu confrontano OFF/RAM usando BASE.

Da verificare sulla scheda: riconoscimento degli 8 MiB, controllo del buffer, intervalli reali con RAM e integrità dopo salvataggio. Non si può ancora affermare che il rallentamento sia eliminato. I controlli con task e filesystem simulati provano il comportamento del codice, non le prestazioni della ESP.

## 1. Misure e registrazione

- Log v3 con tempo del modello, tempo realmente trascorso, dt realmente misurato, tempo del nucleo di calcolo, profondità della coda e riferimento sintetico.
- RESULT include i massimi e i ritardi prima del deploy; non è più necessario dedurre a quale test appartiene un vecchio STATUS.
- Diagnostica OFF/LEGACY/BATCH a scenario e algoritmo uguali. BATCH è implementato, ma la sua efficacia sulla tua flash sarà un risultato della prova hardware.
- Tutto il replay viene conservato, anche dopo LANDED. In FALSO_DECOLLO non si salvano più soltanto gli ultimi tre secondi privi dello stimolo iniziale.
- Decoder compatibile con log v2. Download verificati prima dell'eventuale cancellazione del singolo file.

## 2. BASE e CANDIDATO

BASE conserva le equazioni Kalman e la logica MATLAB. CANDIDATO usa lo stesso Kalman e introduce tre modifiche isolate:

1. **BOOST → COAST:** tollera forza specifica inferiore a 2 m/s², includendo lo zero. Restano cinque conferme, velocità decrescente e quota oltre la rampa. La soglia è da validare con i dati fisici del motore e della scheda.
2. **Apertura per apogeo:** richiede anche una discesa barometrica confermata. Regressione su 24 letture fresche, intervallo 0,4–0,9 s, età massima 150 ms, pendenza + due errori standard inferiore a −0,3 m/s. Restano tutte le condizioni precedenti e il timeout originale. Le ipotesi statistiche non sono una garanzia contro qualunque rumore o guasto.
3. **LANDED:** dopo il deploy richiede almeno due secondi di forza specifica vicino a +g e quota barometrica stabile entro 1 m, almeno 20 letture fresche, nessuna pausa barometrica oltre 150 ms. Non usa il valore assoluto della quota né la velocità Kalman per dichiarare quiete. Questo evita di affidarsi alla stima gravemente errata dopo l'urto sintetico; non corregge quella stima.

Un oggetto fermo in aria e uno fermo al suolo possono produrre misure simili: il criterio LANDED non prova il contatto con il terreno. L'orientamento fisso usato per la forza specifica può cambiare dopo il recupero; una rotazione può impedire la dichiarazione di quiete. Queste sono limitazioni da collaudare, non motivi per modificare ora il montaggio verificato.

## 3. Risultati locali che richiedono attenzione

Tempi nel **modello**, non misure della tua ESP:

| Caso | BASE | CANDIDATO | Interpretazione |
|---|---|---|---|
| NOMINALE | Deploy 7,910 s | 8,280 s | La conferma aggiunge 0,370 s; il ritardo rispetto all'apogeo è circa 0,426 s. |
| RUMORE | 7,910 s | 8,310 s | Circa 0,456 s dopo l'apogeo; da giudicare rispetto ai requisiti meccanici. |
| COAST_ZERO | Timeout 9,050 s | Apogeo 8,520 s | Il vecchio confronto `<0` manca la fine della spinta. Apogeo fisico in questo caso circa 8,099 s. |
| BUCO_IMU_BURNOUT | 7,430 s, prima dell'apogeo | 8,280 s | La conferma evita l'anticipo in questo dataset, ma l'accuratezza Kalman resta CHECK. |
| BUCO_BARO_APOGEO | 7,910 s | Timeout 9,050 s | Peggioramento della disponibilità: mancano le letture per la conferma. Questo caso impedisce di adottare automaticamente CANDIDATO. |
| BARO_CONGELATO | 6,210 s, prima dell'apogeo | Timeout 9,050 s | La conferma impedisce il falso apogeo; la quota stimata resta molto errata. |
| PICCO_BARO_LUNGO | Errori di quota fino a circa 56 m prima del deploy | Stesso limite | Il filtro dopo cinque scarti accetta il disturbo persistente. Non basta aver superato il test del picco isolato. |
| TIMEOUT | Errore quota al deploy circa +17,36 m | Stesso errore | Il timer funziona nel test; il Kalman non riconosce l'IMU bloccata. |

Non ho cambiato rumori/covarianze, forzato la quota a zero, né inventato un accelerometro sostitutivo quando manca quello reale. Il vecchio mantenimento dell'ultima forza specifica è rimasto visibile e verificabile. Le correzioni di questi problemi richiedono una scelta esplicita del comportamento in guasto e ulteriori dati; i dataset e le misure per confrontare le scelte sono ora pronti.

L'arresto istantaneo nel vecchio NOMINALE porta il Kalman molto sotto il terreno anche se il barometro indica zero. CANDIDATO può riconoscere la quiete mentre la stima è ancora errata (circa −58 m al suo LANDED). Questo valore viene riportato in `land_h_error`; `landing=PASS` significa corretto riconoscimento temporale del contatto prescritto, non quota corretta. I nuovi profili con discesa lenta rendono distinguibile l'effetto dell'urto artificiale.

## 4. Percorso diagnostico v3 (già eseguito sulla scheda)

1. Confrontare i nove risultati TIMING, soprattutto `predeploy_max_dt_us`, numero di ritardi e code. Un solo massimo non basta a dimostrare la causa; OFF elimina le scritture durante il replay e fornisce un confronto utile.
2. Valutare se BATCH migliora la situazione e il costo della minore frequenza di sincronizzazione. Non alzare il limite di 20 ms per nascondere CHECK.
3. Esaminare SCENARI: aperture anticipate, casi che passano al timer, accuratezza e LANDED.
4. Esaminare CLOCK: se il tempo reale cambia il risultato del filtro, sarà visibile senza confonderlo con il tempo sintetico.
5. Definire un comportamento in guasto che tenga conto del ritardo massimo accettabile del paracadute, del profilo di spinta e della quota prevista. Il timeout fisso di 8 s dal decollo è ancora quello originario: non è stato validato per ogni volo.

Il passo corrente è il confronto RAM/OFF della v4. Restano da decidere gestione dei sensori guasti, compromesso fra conferma barometrica e ritardo e riconoscimento affidabile di LANDED. Quest'ultimo governa il salvataggio finale con sensori reali: un falso LANDED può attivare la flash in anticipo, mentre un LANDED mancato lascia i dati in RAM.

Non sono stati collaudati qui alimentazione reale, comando meccanico, vibrazioni, rotazione in volo o perdita di alimentazione. La simulazione e i controlli di codice non sostituiscono queste prove fisiche.
