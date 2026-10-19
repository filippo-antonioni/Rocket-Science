# Risultati misurati sulla scheda

CHECK richiede esame; SKIP significa misura non prevista. Le soglie sono provvisorie per il banco.

| scenario | mode | core | clock | algorithm | accuracy | landing | log | timing | deploy_ms | deploy_wall_us | predeploy_max_dt_us |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RUMORE | OFF | BASE | MODEL | PASS | PASS | PASS | SKIP | PASS | 7910 | 7920406 | 10207 |
| RUMORE | LEGACY | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 8308362 | 31769 |
| RUMORE | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 8262961 | 32248 |
| RUMORE | LEGACY | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 8281464 | 31239 |
| RUMORE | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | PASS | 7910 | 7935871 | 11000 |
| RUMORE | OFF | BASE | MODEL | PASS | PASS | PASS | SKIP | PASS | 7910 | 7920433 | 10233 |
| RUMORE | BATCH | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 7935280 | 11001 |
| RUMORE | OFF | BASE | MODEL | PASS | PASS | PASS | SKIP | PASS | 7910 | 7920430 | 10231 |
| RUMORE | LEGACY | BASE | MODEL | PASS | PASS | PASS | PASS | CHECK | 7910 | 7941603 | 11004 |

I dettagli e i file HTML sono nella stessa cartella. Il test MODEL separa il tempo del dataset dal tempo reale. WALL usa il tempo trascorso sulla scheda.
