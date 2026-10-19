"""Compila/carica il solo diagnostico e legge automaticamente la sua uscita."""
import argparse
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM6')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    command = [sys.executable, '-m', 'platformio', 'run', '--project-dir', str(root),
               '-e', 'diagnosi_servo', '-t', 'upload', '--upload-port', args.port]
    print('Compilo e carico DIAGNOSI SERVO. Chiudere prima il menu e il Monitor seriale.', flush=True)
    subprocess.run(command, check=True)
    import serial
    connection = serial.Serial(port=None, baudrate=115200, timeout=1)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    connection.open()
    print('Attendo la prova automatica: dura circa un minuto. Servo libero dal meccanismo.', flush=True)
    deadline = time.monotonic() + 85
    try:
        while time.monotonic() < deadline:
            line = connection.readline().decode('utf-8', errors='replace').strip()
            if not line:
                continue
            print(line, flush=True)
            if line.startswith('ERR PWM_'):
                return 1
            if line.startswith('TEST_FINITO'):
                return 0
        print('Nessun TEST_FINITO ricevuto: non prova un guasto del servo. Copiare tutto l output.')
        return 1
    finally:
        connection.close()


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print('\nLettura interrotta. Il firmware termina comunque la prova dopo 60 s di movimento.')
        raise SystemExit(130)
    except Exception as error:
        print(f'ERRORE: {error}')
        raise SystemExit(1)

