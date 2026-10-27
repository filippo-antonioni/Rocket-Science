"""Esegue i test host esistenti senza scheda, upload o accesso seriale."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
AVIONICS = ROOT / 'SN2/Code/Avionics'


def main():
    if not shutil.which('g++'):
        raise RuntimeError('Manca g++ nel PATH: serve per i test C++ sul PC.')
    try:
        import numpy  # noqa: F401
    except ImportError as exc:
        raise RuntimeError('Installare requirements-dev.txt con questo Python.') from exc
    scripts = [
        'tests/run_host.py',
        'tests/check_equivalence.py',
        'tests/test_decode.py',
        'tests/test_menu.py',
        'TestAlimentazione/tests/test_analysis.py',
    ]
    for script in scripts:
        print('\nVERIFICA:', script, flush=True)
        subprocess.run([sys.executable, str(AVIONICS / script)], cwd=AVIONICS, check=True)
    with tempfile.TemporaryDirectory() as folder:
        executable = Path(folder) / ('power-test.exe' if sys.platform == 'win32' else 'power-test')
        subprocess.run([
            'g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
            '-I', str(AVIONICS / 'TestAlimentazione/tests/mocks'),
            str(AVIONICS / 'TestAlimentazione/tests/check_sequence.cpp'),
            '-o', str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)
    print('\nVERIFICHE HOST COMPLETATE. Nessuna build ESP32 o prova fisica eseguita.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print('ERRORE:', error, file=sys.stderr)
        sys.exit(1)
