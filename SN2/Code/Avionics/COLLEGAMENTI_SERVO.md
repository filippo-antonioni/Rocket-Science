# Servo, ESP e step-up

Nella foto precedente è leggibile SG90 e si vedono tre fili: arancione, rosso e marrone. Il documento TowerPro dell'SG90 indica alimentazione nominale 4,8 V. Il marchio/versione del tuo esemplare e il convertitore vanno confermati: non dedurre da SG90 che qualunque tensione fino a 6 V sia ammessa.

La ESP usa segnali GPIO a 3,3 V. Il servo richiede due connessioni di alimentazione e un segnale: sono funzioni diverse. L'uscita GPIO18 genera gli impulsi di controllo, non alimenta il motore.

## Collegamento di principio

La tabella vale per un convertitore compatibile con la batteria, regolato e misurato alla tensione richiesta dal servo. È uno schema per funzioni, non l'identificazione dei morsetti sulla tua scheda step-up ancora sconosciuta.

| Collegamento | Destinazione |
|---|---|
| Positivo della sorgente adatta al convertitore | IN+ dello step-up |
| Negativo della stessa sorgente | IN− dello step-up |
| OUT+ regolato per il servo | Filo rosso del servo |
| OUT− | Filo marrone del servo |
| GND della ESP | Stesso nodo del marrone/OUT− |
| GPIO18 della ESP | Filo arancione del servo |

Per la prima prova la ESP resta alimentata dalla USB del PC. La sorgente del servo alimenta soltanto il servo; colleghi in comune il negativo, non i due positivi. Non collegare OUT+ dello step-up al pin 3V3 della ESP né alla barra 3,3 V dei sensori. Non collegarlo neppure al pin 5V della ESP alimentata contemporaneamente dalla USB durante questa procedura.

La massa comune è necessaria perché il servo interpreti il segnale rispetto allo stesso riferimento della ESP. Molti servo RC accettano segnali intorno a 3,3 V pur avendo un'alimentazione più alta; la compatibilità del tuo esemplare non si deduce soltanto dal suo nome. Se non risponde con alimentazione corretta e stabile, controlliamo anche la soglia del segnale prima di cambiare cablaggio.

## Cosa fa lo step-up

Il convertitore alza una tensione; non crea potenza o corrente aggiuntiva. Aumentando la tensione, per una certa corrente in uscita richiede più corrente dalla batteria, più le perdite. La capacità dichiarata sul modulo può riferirsi alla corrente interna/di ingresso, non a quella erogabile al servo alla tua tensione.

Un boost puro non è la scelta automatica se la batteria supera già la tensione del servo. Per esempio, una sorgente sopra 5 V richiede una valutazione diversa da una cella litio 1S. Non collegare lo step-up al 3V3 della ESP per ottenere la potenza del servo: caricheresti il regolatore della scheda e potresti causare cali di tensione/reset.

Dimensioniamo batteria e convertitore per i picchi del servo sotto carico, compresa la corrente a motore bloccato; non per il consumo della ESP o per il solo consumo del servo a riposo. La potenza effettiva va verificata sul tuo convertitore. Un reset durante la registrazione perde i dati ancora in PSRAM.

Se il convertitore è regolabile, misura OUT+ rispetto a OUT− col multimetro prima di collegare il servo. Non ruotare il trimmer alla cieca con il servo collegato. Poi osserva la tensione durante il movimento; un multimetro può non catturare tutti i picchi brevi, ma una caduta evidente è già un problema.

## MOSFET

Per questi comandi di posizione non serve inserire un MOSFET: il servo riceve alimentazione continua e impulsi su GPIO18. Il MOSFET può essere utile in un progetto separato per togliere alimentazione, ma non converte automaticamente 3,3 V in 5 V e non aumenta la corrente disponibile. Non inseriamo ora un MOSFET sconosciuto nel ritorno di massa, che è anche il riferimento del segnale.

`SERVO OFF` nel firmware stacca gli impulsi, non l'alimentazione. Non equivale ad azionare un MOSFET di potenza.

## Breadboard e cavetti

Mantieni separati il positivo dei sensori a 3,3 V e il positivo del servo alla tensione prevista. Possono avere lo stesso negativo comune. Le barre laterali della breadboard possono essere interrotte a metà: la continuità va verificata, non dedotta dal colore della linea.

Per il servo femmina servono contatti maschi: tre jumper possono portare i fili a tre nodi separati della breadboard. Per partire da un header maschio della ESP verso la breadboard serve invece un jumper femmina-maschio. Non collegare in una stessa fila rosso, marrone e arancione. La corrente del servo deve tornare verso l'alimentatore con un percorso breve, senza attraversare il pin/regolatore 3V3 della ESP; i collegamenti di potenza definitivi vanno scelti dopo aver identificato il modulo e misurato il carico.

## Informazioni ancora necessarie

Servono una foto leggibile o la sigla dello **step-up**, il tipo di **batteria**, numero di celle/tensione nominale e tensione a piena carica, e la sigla dei **MOSFET** se vorrai usarli per interrompere alimentazione. La foto del convertitore deve mostrare le etichette IN/OUT. Queste informazioni servono per indicare i morsetti esatti e verificare la corrente disponibile, non per completare il firmware già fornito.

## Riferimenti

- [TowerPro, SG90 Analog](https://towerpro.com.tw/product/sg90-analog/): nominale 4,8 V del prodotto originale, non identificazione garantita del servo fotografato.
- [Pololu, caratteristiche elettriche dei servo](https://www.pololu.com/blog/16/electrical-characteristics-of-servos-and-introduction-to-the-servo-control-interface): alimentazione, picchi/corrente di stallo, segnale riferito alla massa.
- [Pololu, esempio di regolatore boost](https://www.pololu.com/product/5581/): relazione tra tensioni, rendimento e correnti. Non è il modulo dell'utente.
- [Espressif, ESP32-S3 DevKitC-1](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html): alimentazione della scheda e opzioni USB/pin.
