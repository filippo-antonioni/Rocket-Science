"""Menu Windows: utilizza il Python di PlatformIO che lo ha avviato."""
import datetime
import pathlib
import subprocess
import sys
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]

def execute(arguments):
    print('\nEseguo:', ' '.join(str(x) for x in arguments),flush=True)
    return subprocess.run([str(x) for x in arguments],cwd=ROOT).returncode

def main():
    port='COM6'
    while True:
        print(f'''
BANCO V4 PSRAM - porta {port}
Chiudere il Monitor seriale di VSCode prima di usare la porta.
1  Build banco_psram (compila, non serve USB)
2  Upload banco_psram (carica sulla scheda, USB-UART)
3  Info
4  RAMTEST: verifica 2 MiB in PSRAM, pochi secondi
5  RAM: 6 prove e download, circa 7-10 minuti (RAMTEST incluso)
6  Recupera ultimo log direttamente dalla RAM via USB
7  Stop del replay / ORIENT
8  Elenco file sulla scheda
9  Cambia porta
10 Informazioni sul vecchio ripristino senza PSRAM
11 Riprova SAVE dopo un errore di salvataggio flash
0  Esci
''')
        choice=input('Numero: ').strip()
        if choice=='0': return
        if choice=='9':
            value=input('Porta (es. COM6): ').strip()
            if value: port=value
            continue
        if choice=='1': execute([sys.executable,'-m','platformio','run','-e','banco_psram']);continue
        if choice=='10':
            print('Il progetto attuale non definisce un ambiente banco senza PSRAM. Nessun upload eseguito.');continue
        if choice=='2':
            execute([sys.executable,'-m','platformio','run','-e','banco_psram','-t','upload','--upload-port',port]);continue
        if choice in ['3','4','7','8','11']:
            execute([sys.executable,ROOT/'tools/rocket.py','--port',port,{'3':'info','4':'ramtest','7':'stop','8':'list','11':'save'}[choice]])
            continue
        if choice=='6':
            stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
            target=ROOT/'logs'/'banco_v4'/f'{stamp}_recupero_RAM.bin'
            execute([sys.executable,ROOT/'tools/rocket.py','--port',port,'get','RAM','--out',target]);continue
        if choice!='5': print('Scegli un numero della lista.');continue
        kind='ram'
        stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        folder=ROOT/'logs'/'banco_v4'/f'{stamp}_{kind}'
        print('Risultati in:',folder)
        print('Ogni nuovo log viene copiato e verificato sul PC, poi cancellato dalla scheda.')
        print('I file presenti sulla scheda prima di questa campagna restano al loro posto.')
        code=execute([sys.executable,ROOT/'tools/rocket.py','--port',port,'campaign',kind,
                      '--out',folder,'--remove-downloaded'])
        if folder.exists():
            archive=folder.with_suffix('.zip')
            with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
                for path in sorted(folder.rglob('*')):
                    if path.is_file(): out.write(path,path.relative_to(folder.parent))
            print('\nZIP DA ALLEGARE:',archive)
        print('Campagna terminata.' if code==0 else 'Campagna interrotta: leggere ERRORE sopra; conservati i risultati disponibili.')

if __name__=='__main__':
    try: main()
    except KeyboardInterrupt: print('\nMenu chiuso. Un replay gia avviato sulla scheda puo essere ancora attivo.')
