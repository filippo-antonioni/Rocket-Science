"""Carica lo scanner I2C e visualizza automaticamente cinque scansioni."""
import argparse
import datetime
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    parser.add_argument('--solo-lettura',action='store_true',help='Nessun upload; premere RESET quando la lettura e aperta')
    args=parser.parse_args()
    project=Path(__file__).resolve().parent
    print('Batteria scollegata, step-up -> 5VIN staccato. Sensori alimentati dal 3V3 ESP, USB-UART collegata.',flush=True)
    if not args.solo_lettura:
        subprocess.run([sys.executable,'-m','platformio','run','--project-dir',str(project),
            '-e','diagnosi_i2c','-t','upload','--upload-port',args.port],check=True)
        print('\nCARICATO SCANNER revision=2. Ora serve riavviare anche i sensori.',flush=True)
        print('Con batteria scollegata: stacca la USB per almeno 5 secondi, poi ricollegala.',flush=True)
        input('Aspetta che Windows riconosca la porta e premi INVIO per leggere: ')
    import serial
    connection=serial.Serial(port=None,baudrate=115200,timeout=.5)
    connection.dtr=False;connection.rts=False;connection.port=args.port;connection.open()
    folder=project.parent/'logs';folder.mkdir(exist_ok=True)
    path=folder/('diagnosi_i2c_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.txt')
    print('Leggo la diagnosi. Se non compare nulla, premi RESET sulla ESP.',flush=True)
    deadline=time.monotonic()+45
    try:
        with path.open('x',encoding='utf8') as out:
            while time.monotonic()<deadline:
                line=connection.readline().decode('utf8',errors='replace').strip()
                if not line: continue
                print(line,flush=True);out.write(line+'\n');out.flush()
                if line.startswith('END_DIAGNOSI'):
                    print('\nFINE. Output salvato in:',path.resolve())
                    return 0
        print('Tempo terminato; output parziale in:',path.resolve());return 1
    finally: connection.close()


if __name__=='__main__':
    try: raise SystemExit(main())
    except KeyboardInterrupt: print('\nLettura chiusa.');raise SystemExit(130)
    except (OSError,ImportError,EOFError,subprocess.CalledProcessError) as error:
        print('ERRORE:',error,file=sys.stderr);raise SystemExit(1)
