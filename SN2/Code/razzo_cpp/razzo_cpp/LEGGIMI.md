# Conversione MATLAB → C++ per l'avionica

## File da usare

Mettere **entrambi** `src/main.cpp` e `src/flight_core.h` nella cartella `src` del progetto PlatformIO, sostituendo il precedente `main.cpp`. Non compilare anche `Gestione_fairing.ino`: avrebbe un secondo `setup()` e un secondo `loop()`.

`main.cpp` collega sensori, algoritmo, log e servo; `flight_core.h` contiene il Kalman e la macchina a stati. Il file header separato permette di testarli sul PC senza Arduino; non richiede librerie matriciali.

Mantenere il proprio `platformio.ini`. Aggiungere ESP32Servo alle dipendenze, se non c'e' gia', come indicato in `platformio_aggiunta.ini`. Senza `.ini`, modello preciso di scheda e versioni installate non e' stata eseguita una compilazione ESP32. I test eseguiti riguardano il C++ degli algoritmi, non certificano il funzionamento del dispositivo.

## Cosa viene eseguito sulla scheda

1. Lettura accelerometro a cadenza nominale 100 Hz e conversione LSB → m/s².
2. Acquisizione pressione a cadenza obiettivo ~38 Hz, conversione in quota relativa alla rampa.
3. Predizione del Kalman a ogni ciclo; correzione solo con nuova quota valida.
4. Macchina `PAD → BOOST → COAST → DESCENT → LANDED`.
5. All'evento di apertura, comando al servo e mantenimento della posizione aperta.
6. Log dei dati e delle decisioni.

`flight_sim.m`, `calcola_acc.m`, `sensors_sim.m`, `montecarlo_test.m`, `noise_analysis.m` e i grafici restano sul PC: descrivono o analizzano il volo, non servono all'elaborazione delle misure vere. Dal file `parameters.m` vengono copiati solo i parametri usati da filtro e logica. Non occorre portare su ESP32 massa, curva di spinta, RK4 o modelli del drag.

## Impostazioni iniziali

In `main.cpp`:

```cpp
constexpr Modalita MODALITA = BANCO;
constexpr int SERVO_PIN = 18;
constexpr int SERVO_CHIUSO = 130;
constexpr int SERVO_APERTO = 90;
constexpr int ASSE_VERTICALE = 2;  // Z
constexpr float SEGNO_ASSE = +1.0f;
```

| Modalita' | Comportamento |
|---|---|
| `BANCO` | Mantiene il CSV seriale a 8 colonne. Non collega il servo al PWM e non esegue la logica di volo. |
| `TEST_SERVO` | Inizializza solo il servo; `A` apre e `C` chiude. Non richiede IMU o BMP280. |
| `VOLO` | Esegue Kalman e macchina a stati, comanda il servo automaticamente e salva un CSV esteso su flash. Non accetta comandi manuali A/C. |

La modalita' e' una costante: dopo averla modificata bisogna ricompilare e caricare il firmware. All'avvio di VOLO, completata la calibrazione, la macchina e' attiva in PAD; non c'e' un interruttore di armamento fisico aggiunto da questa conversione.

## Servo: segnale e alimentazione

| Collegamento | Destinazione |
|---|---|
| Segnale servo | GPIO 18, configurabile in `SERVO_PIN` |
| Positivo servo | Alimentazione adatta alla tensione e corrente del modello di servo |
| GND servo | Negativo della sua alimentazione e GND della scheda, in comune |

GPIO 18 riprende il file fairing, ma va verificato sulla **scheda precisa**: il numero e' quello GPIO, non il diciottesimo piedino del connettore. Non alimentare il motore del servo dal GPIO; tensione, corrente di spunto e compatibilita' del segnale a 3.3 V dipendono dal servo. Non posso ricavarle dal solo codice.

La configurazione riprende 50 Hz, impulsi 500–2400 µs e angoli 130°/90° dal file fornito:

```cpp
servoParacadute.setPeriodHertz(50);
servoParacadute.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
servoParacadute.write(SERVO_CHIUSO);
```

`attach` associa il pin al segnale PWM: non serve fare `digitalWrite(HIGH)` per aprire un servo. Il comando di apertura e':

```cpp
if (eventi.apertura) apriParacadute();
```

La funzione scrive 90° una sola volta, ma la libreria continua a generare il PWM. Non c'e' un `delay` durante l'apertura, non si scollega il servo e in VOLO non si torna a 130°. I valori angolari sono comandi, non misure della posizione reale.

Il flag `servoComandatoAperto` significa **comando inviato**. Per sapere se lo sportello si e' aperto davvero servirebbe un sensore di posizione/finecorsa, assente dagli allegati. Verificare la corsa meccanica in TEST_SERVO senza precaricare il meccanismo.

Come nello sketch originale, l'avvio comanda la chiusura: un riavvio in volo perde lo stato e puo' richiudere il servo. Non e' implementato il recupero dopo brownout/reset. Neppure il transitorio elettrico di accensione/attach e' verificabile senza banco e oscilloscopio.

## Traduzione del Kalman

Lo stato e' sempre `x = [h; v; bias]`. Nel MATLAB l'indice parte da 1, nel C++ da 0:

| MATLAB | C++ | Significato |
|---|---|---|
| `stato.x(1)` | `kalman.x[0]` | quota relativa |
| `stato.x(2)` | `kalman.x[1]` | velocita' verticale |
| `stato.x(3)` | `kalman.x[2]` | bias accelerometro |
| `stato.P` | `kalman.P[3][3]` | covarianza 3×3 |
| `stato.acc_prec` | `kalman.accPrecedente` | ultimo dato accelerometrico valido |
| `stato.n_scarti` | `kalman.scarti` | scarti consecutivi del barometro |

Non ci sono conti impossibili da convertire. Il C++ standard non assegna ai normali array gli operatori matriciali del MATLAB: occorre espanderli oppure usare una libreria. Per queste dimensioni sono sufficienti cicli di tre elementi.

**`F*x + B*u`** diventa:

```cpp
const double a = acc - p.g - x[2];
x[0] += dt * x[1] + 0.5 * dt * dt * a;
x[1] += dt * a;
// x[2] non cambia durante la predizione
```

Aggiornare h prima di v usa correttamente la velocita' del campione precedente.

**`F*P*F' + Q`** usa due moltiplicazioni con cicli `for`. La trasposta non richiede una copia: si usa `F[j][k]` invece di `F[k][j]`. Il termine `B*B'` e' `B[i]*B[j]`; `diag([0 0 1e-4])` aggiunge solo alla casella `[2][2]`. Si usano matrici temporanee per non sovrascrivere gli elementi che servono ancora al calcolo.

**`H=[1 0 0]`** semplifica molto la correzione:

```cpp
y = baro - x[0];
S = P[0][0] + R;
K[i] = P[i][0] / S;
x[i] += K[i] * y;
```

`S` e' uno scalare: la divisione MATLAB `/S` e' una divisione ordinaria, non l'inversa di una matrice.

**Covarianza dopo la correzione:** il codice usa la forma di Joseph `A*P*A' + K*R*K'`, con `A=I-KH`, invece della forma breve `(I-KH)*P`. Sono equivalenti in aritmetica esatta; Joseph aiuta a conservare una covarianza numericamente valida. I test confrontano deliberatamente le due forme.

Altri adattamenti: `isfinite` → `std::isfinite`, `abs` → `std::fabs` per reali, `sqrt` → `std::sqrt`, `~deploy` → `!deploy`, stringhe delle fasi → `enum class`. In C++, `^` **non** e' la potenza: `dt^2` va scritto `dt*dt`. Il parametro `const Parametri&` evita copie della configurazione.

## Macchina a stati: condizioni conservate

| Transizione | Condizione |
|---|---|
| PAD → BOOST | forza specifica >30 m/s² per 5 campioni, oppure quota Kalman >18 m per 5 campioni con contatore separato |
| BOOST → COAST | velocita' in diminuzione, forza specifica <0, quota >1.5 m, per 5 campioni |
| COAST → DESCENT | velocita' <0, quota >30 m, piu' di 1.8 s dal decollo rilevato, per 5 campioni |
| BOOST/COAST → DESCENT | piu' di 8 s dal decollo rilevato, senza altre conferme; apertura di riserva |
| DESCENT → LANDED | velocita' in modulo <0.4 m/s e quota <10 m per 5 campioni |

`flag_t` diventa `eventi.decollo`; `flag` diventa `eventi.apertura`; `deploy` rimane memorizzato nella macchina. I due eventi sono impulsi di un ciclo. I contatori usano `>=5`, equivalente a `==5` nel normale incremento unitario.

A 100 Hz, 5 campioni corrispondono a circa 50 ms di campionamento, non a 2.5 s. Se il loop rallenta, la durata delle conferme si allunga: il criterio rimane basato sui campioni come nel MATLAB.

## Differenze necessarie rispetto al main originale

- **Quota relativa:** viene sottratta la media iniziale; il vecchio placeholder calcolava quota ISA senza usare `altitudineZero`.
- **Tempo reale:** `dt` arriva dal tempo trascorso con `micros()`, non si assume che logging e I2C impieghino sempre meno di 10 ms. Le sottrazioni unsigned gestiscono il rollover. Non vengono simulate piu' letture IMU uguali per recuperare cicli persi.
- **Rumore del bias:** `1e-4` e' moltiplicato per `dt/0.01`, quindi resta identico al MATLAB al passo nominale e scala col tempo se il loop ritarda. Il termine accelerometrico rimane quello del tuo modello a input costante sul passo.
- **Nuovo barometro:** si passa da NORMAL a FORCED asincrono. Ogni conversione e' avviata esplicitamente; dopo almeno 23 ms si controlla `measuring` e si legge una volta sola. L'obiettivo e' 38 Hz, ma I2C e flash possono ridurlo. Oversampling x1/x8 e IIR x4 restano. Non si decide se una misura e' nuova confrontando i valori: due misure nuove possono essere identiche.
- **Niente attesa attiva della conversione in volo:** `takeForcedMeasurement()` della libreria attende il completamento; qui viene sostituito da avvio e controllo separati. Le singole transazioni I2C e le scritture flash restano sincrone e possono comunque ritardare il loop.
- **Zero iniziale:** campioni distinti, esclusione dei primi 40 per assestamento del filtro e timeout se la calibrazione non riesce.
- **IMU:** viene controllato il risultato della lettura, con valori iniziali sentinel per non usare memoria non inizializzata. Un dato invalido entra come NaN nel Kalman e nella logica, come nel MATLAB.
- **Log:** nomi progressivi `/volo_0000.csv`, ecc.; niente sovrascrittura automatica del volo precedente, niente formattazione automatica al fallimento di mount. Il comando al servo viene prima del log. La perdita del log non arresta l'algoritmo.
- **Avvio:** se manca un sensore, il programma segnala l'errore e non entra nel loop operativo. Non rimane nel vecchio test infinito per sistemare i cavi a caldo.

## Log e prove

In BANCO il formato a 8 colonne resta compatibile con `noise_analysis.m`. La pressione e' mantenuta fra due acquisizioni: i valori uguali NON permettono da soli di misurare la frequenza del BMP280. Le letture IMU fallite hanno sentinel -32768 e sono escluse dal criterio di validita' del tuo script.

In VOLO il CSV aggiunge validita' IMU, `baro_fresh`, `dt`, quota barometrica, stato Kalman, fase e comando servo. Usare `readtable` per questo formato esteso: il vecchio parser `sscanf` a 8 valori ignora le colonne aggiuntive. `motivo` vale 0=nessuno, 1=apogeo, 2=timeout. `correzione` vale 0=assente, 1=scartata, 2=accettata, 3=errore numerico. `cicli_ritardo` conta intervalli oltre 15 ms.

Test riproducibili su PC, dalla cartella del pacchetto:

```sh
g++ -std=c++11 -O2 -Wall -Wextra -Werror tests/test_core.cpp -o test_core
./test_core
python tests/check_equivalence.py
```

Il secondo test richiede NumPy. Risultati ottenuti:

- casi mirati: rampa ferma, NaN, 5 scarti e riaccettazione della sesta misura, conferme interrotte, COAST, limiti di tempo/quota, singolo impulso di apertura, atterraggio, fallback barometrico, timeout con sensori invalidi e rollover;
- 6000 campioni a dt=0.01 s: massimo scarto assoluto su stato/covarianza 6.54e-13;
- 6000 campioni a dt variabile: massimo scarto 5.68e-13;
- contatori, fasi ed eventi identici al riferimento in entrambi i confronti.

Il riferimento Python esegue le equazioni MATLAB con NumPy e la logica originale. **Non e' stata eseguita una simulazione MATLAB del tuo razzo**: negli allegati manca `dati_motore.eng`, richiesto da `parameters.m`. Non sono stati testati servo reale, alimentazione, GPIO, latenze LittleFS o compilazione con le tue dipendenze PlatformIO.

## Limiti del modello che la traduzione non risolve

Il filtro e' 1D: sottrae g dall'accelerazione lungo l'asse scelto. Funziona come modello verticale; se il razzo si inclina, il singolo asse non e' piu' la verticale terrestre. Il giroscopio viene loggato, non usato per stimare l'assetto. Usare il modulo `sqrt(ax²+ay²+az²)` non risolve: perde il segno necessario alla condizione `forza < 0`.

La condizione BOOST → COAST usa la forza specifica grezza: bias e rumore possono ritardare il passaggio. E' stata conservata. Con IMU guasta a lungo, riusare l'ultima accelerazione puo' produrre errori molto grandi. La riserva di decollo e' basata sulla **quota del Kalman**, quindi non e' completamente indipendente dall'IMU.

Il gating accetta la sesta misura anche se e' ancora aberrante, come nel MATLAB. Il ritardo IIR del barometro non viene compensato. Non sono state aggiunte fusioni 3D o politiche nuove di gestione guasti.

L'apertura di riserva parte dal **decollo rilevato**, non dall'istante fisico di accensione del motore; non parte se il decollo non viene mai rilevato. In caso di volo basso, la soglia di 30 m puo' impedire l'apertura normale. Il timeout ignora la quota come nell'originale: 8 s non sono una garanzia di apertura ancora in aria.

Nel simulatore `flight_sim.m` il paracadute si apre automaticamente all'apogeo vero; il comando della macchina a stati non retroagisce sulla traiettoria simulata. Il Monte Carlo allegato misura quindi il ritardo della decisione su quella traiettoria, non gli effetti di un'apertura tardiva/reale sul volo. Inoltre non include il tempo di movimento del servo e il tempo di estrazione/gonfiaggio del paracadute.

La flash puo' riempirsi dopo molte prove o lunghe attese in PAD, e `flush()` puo' introdurre ritardi: controllare `dt_s` e il contatore ritardi nelle prove reali. Se LittleFS non e' inizializzata, va predisposta una volta tramite il progetto: non si formatta in automatico ad ogni mount fallito.

## Riferimenti delle interfacce verificate

- TDK ICM42670P: https://github.com/tdk-invn-oss/motion.arduino.ICM42670P
- Adafruit BMP280, implementazione di `readPressure` e `takeForcedMeasurement`: https://github.com/adafruit/Adafruit_BMP280_Library/blob/master/Adafruit_BMP280.cpp
- Bosch BMP280, registri `ctrl_meas` e `status`: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
- ESP32Servo: https://github.com/madhephaestus/ESP32Servo
