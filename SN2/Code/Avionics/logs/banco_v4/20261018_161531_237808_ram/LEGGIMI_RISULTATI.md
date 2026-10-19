# Risultati misurati sulla scheda

CHECK richiede esame; SKIP significa misura non prevista. Le soglie sono provvisorie per il banco.

| scenario | mode | core | clock | algorithm | accuracy | landing | log | timing | deploy_ms | deploy_wall_us | predeploy_max_dt_us | flash_ops_capture | save_ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RUMORE | OFF | BASE | MODEL | PASS | PASS | PASS | SKIP | PASS | 7910 | 7920335 | 10158 | 0 | 0 |
| RUMORE | RAM | BASE | MODEL | PASS | PASS | PASS | PASS | PASS | 7910 | 7920778 | 10589 | 0 | 2631 |
| RUMORE | OFF | BASE | WALL | PASS | PASS | PASS | SKIP | PASS | 7902 | 7902572 | 10999 | 0 | 0 |
| RUMORE | RAM | BASE | WALL | PASS | PASS | PASS | PASS | PASS | 7910 | 7910508 | 10317 | 0 | 2662 |
| BUCO_BARO | RAM | BASE | WALL | PASS | PASS | PASS | PASS | PASS | 7910 | 7910780 | 10641 | 0 | 2612 |
| PARACADUTE | RAM | BASE | WALL | PASS | PASS | PASS | PASS | PASS | 7910 | 7910572 | 10431 | 0 | 5660 |

I dettagli e i file HTML sono nella stessa cartella. Il test MODEL separa il tempo del dataset dal tempo reale. WALL usa il tempo trascorso sulla scheda.
