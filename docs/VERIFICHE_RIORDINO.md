# Verifiche del riordino

Le verifiche software sono state eseguite su un PC Linux con Python e `g++`.
Il comando riproducibile dalla radice è `python tools/verifica_repo.py`.

| Verifica | Esito |
|---|---|
| Sintassi dei tre ambienti principali con interfacce hardware simulate | PASS |
| Comandi manuali e 18 replay collegati al servo simulato | PASS |
| Client: rifiuto del firmware errato e coerenza dei flag | PASS |
| Filtro, scenari, evidenza di discesa e atterraggio | PASS |
| Logger: ring PAD, PSRAM, CRC, overflow e recupero dopo errore | PASS |
| Confronto numerico su 6.000 campioni a dt fisso | PASS; errore massimo x/P circa 6,54 × 10⁻¹³ |
| Confronto numerico su 6.000 campioni a dt variabile | PASS; errore massimo x/P circa 5,68 × 10⁻¹³ |
| Decodifica C++ → Python, CRC errato e record troncato | PASS |
| Analisi POWER: 10 casi, inclusi corruzione e dati incompleti | PASS |
| Sequenza POWER e PWM: timer 60/90/120 s, errori e GPIO17 | PASS |
| Nuovi collegamenti del menu: 3 test senza seriale | PASS |

Il confronto numerico usa una trascrizione Python delle equazioni MATLAB:
non è un'esecuzione del simulatore MATLAB originale. Il successo dei test
degli scenari non significa che tutte le condizioni di guasto producano un
deploy adeguato: restano i limiti algoritmici documentati nelle analisi storiche.

Sono stati verificati anche la sintassi Python, gli include locali C++, i link
dei nuovi documenti, la presenza dei file richiamati dai menu e la conservazione
SHA-256 dei file originari. Lo ZIP finale viene controllato per integrità e
confrontato con i file della consegna.

Non sono stati eseguiti una compilazione PlatformIO per ESP32, un caricamento
sulla scheda, MATLAB, OpenRocket o una prova fisica. I file `.cmd` sono stati
controllati nei percorsi e nella sintassi leggibile; non eseguiti su Windows.
La prima build sul PC dell'utente deve ricreare le dipendenze escluse dal pacchetto.
