# Simulazione MATLAB

Il simulatore descrive il moto verticale, genera misure sintetiche e applica
Kalman e macchina a stati. L'unica modifica al codice MATLAB durante il
riordino è il percorso del log in `noise_analysis.m`, ora relativo alla repository.

Aprire questa cartella come cartella corrente di MATLAB ed eseguire:

```matlab
main_test
```

Per la campagna Monte Carlo già definita nel codice:

```matlab
montecarlo_test
```

| File | Funzione |
|---|---|
| `parameters.m` | Parametri del veicolo, del motore, dei sensori e della logica |
| `dati_motore.eng` | Curva di spinta letta da `parameters.m` |
| `flight_sim.m`, `calcola_acc.m` | Integrazione del moto e accelerazione |
| `sensors_sim.m` | Misure sintetiche con rumore e disturbi |
| `kalman.m` | Stima di quota, velocità e bias |
| `macchina_stati.m` | Fasi e comando di apertura |
| `main_test.m` | Prova singola e grafici |
| `montecarlo_test.m` | Ripetizioni con variazioni dei parametri |
| `noise_analysis.m` | Analisi della registrazione reale inclusa in Avionics/logs |
| `figures/` | Grafici PNG preesistenti, conservati senza modificarli |

Il modello OpenRocket è ora in [SN2/Design/SN2.ork](../../Design/SN2.ork).
I suoi dati e i parametri MATLAB non vengono sincronizzati automaticamente.
MATLAB e OpenRocket non sono stati eseguiti durante la riorganizzazione.
