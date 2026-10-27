# Dataset e criteri di lettura

I dataset sono generati sulla ESP dalle formule in `src/scenarios.h`. Non devi inviare migliaia di righe dal PC: il comando RUN sceglie il generatore. I campioni attraversano Kalman e macchina a stati come gli ingressi dei sensori. Nei replay non si collaudano collegamenti, I2C o orientamento fisico.

Il riferimento nominale ha apogeo a circa **7,854 s**, quota **171,359 m**. Tutti i vecchi scenari condividono la stessa traiettoria di riferimento, anche TIMEOUT. Il suo valore stimato di circa 181 m al deploy era un errore della stima, non un apogeo più alto.

| Dataset | Ingresso particolare | Obiettivo |
|---|---|---|
| NOMINALE | Traiettoria originale: salita, parabola, arresto istantaneo al suolo | Confronto con v2 |
| RUMORE | Disturbi deterministici su accelerazione e quota | Ripetibilità e timing flash |
| PICCO_BARO | +150 m intorno a 5 s; un campione barometrico fresco interessato | Reiezione di un picco isolato |
| BUCO_IMU | IMU assente 4,10–4,40 s | Vecchio caso a accelerazione costante |
| BUCO_BARO | Barometro assente 4,00–5,00 s | Propagazione senza quota |
| TIMEOUT | Forza IMU bloccata a +g dopo 3 s | Riserva temporale e inaccuratezza con IMU congelata |
| FALSO_DECOLLO | Due campioni oltre la soglia, scheda ferma | Nessun decollo/apertura; ora il log comprende lo stimolo |
| BUCO_IMU_BURNOUT | IMU assente 2,85–3,35 s, attraversa lo spegnimento motore | Limite del mantenimento dell'ultima accelerazione |
| BUCO_BARO_APOGEO | Barometro assente 7,40–8,50 s | Disponibilità della conferma proprio all'apogeo |
| COAST_ZERO | Dopo 3 s accelerazione −g, forza specifica zero | Limite della vecchia condizione `forza < 0` |
| PICCO_BARO_LUNGO | +150 m per 1 s, 4,80–5,80 s | Limite dell'accettazione dopo cinque scarti |
| SENSORI_ASSENTI | Entrambi assenti da 4 s | Riserva, assenza di conferme, impossibilità di validare LANDED |
| PAD_RUMOROSO | Disturbi e impulsi di quattro campioni ogni 4 s | Nessun falso decollo su una permanenza lunga in PAD |
| JITTER | Un passo matematico di 30 ms ogni 125 campioni | Uso di dt variabile; non crea blocchi flash artificiali |
| PARACADUTE | Discesa prescritta verso −5 m/s, terreno a 0 m | Atterraggio senza il violentissimo arresto del vecchio profilo |
| QUOTA_NEGATIVA | Come PARACADUTE, terreno a −25 m dalla rampa | Quota negativa fisicamente possibile, LANDED indipendente dallo zero di rampa |
| FALSO_LANDED | Come PARACADUTE, pausa artificiale di 0,3 s a 5 m | Una breve quiete in aria non deve chiudere il log |
| BARO_CONGELATO | Valore di quota fermo a quello di 4 s, ancora marcato fresco | Guasto plausibile che non si rileva controllando solo l'età dei dati |

Il tratto PARACADUTE comincia **per costruzione a 8,1 s**, con transizione di 0,5 s verso −5 m/s. È uguale per BASE e CANDIDATO e non è attivato dal loro comando: serve come ingresso ripetibile, non come previsione fisica dell'effetto dell'apertura. Nel profilo FALSO_LANDED la pausa è uno stress artificiale, con cambiamenti di velocità istantanei.

Le prove durano 35 s di modello, oppure 75 s nei tre scenari con discesa lenta. I valori sintetici raw sono ancora espressi su Z per compatibilità con v2; `forza` è già l'ingresso verticale. L'asse X/segno −1 viene usato dai sensori reali.

## Significato dei risultati

| Campo | Criterio nel codice |
|---|---|
| `algorithm` | Nessun decollo nei due casi PAD; negli altri un solo decollo e deploy. TIMEOUT deve aprire per riserva a 9,00–9,30 s. Negli altri casi il criterio desiderato è apertura per apogeo fra apogeo fisico e apogeo +0,5 s. |
| `accuracy` | Fino al deploy: errore massimo di quota ≤5 m, di velocità ≤5 m/s, nessun errore numerico. È indipendente dal motivo del deploy. |
| `landing` | LANDED dopo il contatto prescritto, entro 5 s; SKIP per i casi PAD. Non certifica l'accuratezza della quota Kalman al suolo. |
| `log` | PASS se nessuna perdita/errore rilevata dal logger; SKIP in OFF. Il client verifica anche CRC, sequenza e numero record scaricati. |
| `timing` | CHECK se un intervallo reale supera 20.000 us. Soglia diagnostica provvisoria, non requisito del veicolo. |
| `predeploy_max_dt_us` | Peggiore intervallo reale prima dell'apertura, incluso il campione che comanda l'apertura. |
| `deploy_ms` | Istante nel tempo del modello (MODEL) o tempo trascorso del replay (WALL). |
| `deploy_wall_us` | Istante realmente trascorso dall'inizio, acquisito dopo il comando all'uscita. |
| `land_h_error` | Errore della quota Kalman nel momento di LANDED; non viene azzerato per nascondere l'errore. |
| `max_imu_gap_us`, `max_baro_gap_us` | Massima assenza di dati validi/freschi nel tempo del modello. Un valore congelato ma fresco NON conta come assenza. |

Un `CHECK` in una prova di guasto è un risultato da analizzare. Il runner non si ferma su CHECK e non lo trasforma in PASS. Non è previsto che tutti i guasti gravi superino tutti i criteri.
