# Risultati misurati sulla scheda

CHECK richiede esame; SKIP significa misura non prevista. Le soglie sono provvisorie per il banco.

| scenario | mode | core | clock | algorithm | accuracy | landing | log | timing | deploy_ms | deploy_wall_us | predeploy_max_dt_us |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| NOMINALE | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 7928686 | 10998 |
| NOMINALE | BATCH | BASE | WALL | PASS | PASS | PASS | PASS | CHECK | 7900 | 7900668 | 11003 |
| RUMORE | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | PASS | 7910 | 7934770 | 11000 |
| RUMORE | BATCH | BASE | WALL | PASS | PASS | PASS | PASS | CHECK | 7896 | 7897435 | 11002 |
| BUCO_BARO | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 8290673 | 34839 |
| BUCO_BARO | BATCH | BASE | WALL | PASS | PASS | PASS | PASS | CHECK | 7896 | 7897347 | 39002 |

I dettagli e i file HTML sono nella stessa cartella. Il test MODEL separa il tempo del dataset dal tempo reale. WALL usa il tempo trascorso sulla scheda.
