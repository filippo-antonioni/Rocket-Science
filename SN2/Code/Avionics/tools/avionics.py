"""Un solo menu per tre ambienti dello stesso progetto PlatformIO."""
import datetime
import pathlib
import subprocess
import sys
import zipfile
import rocket

ROOT=pathlib.Path(__file__).resolve().parents[1]

def execute(arguments):
    print('\nEseguo:', ' '.join(str(x) for x in arguments),flush=True)
    return subprocess.run([str(x) for x in arguments],cwd=ROOT).returncode

def upload(environment,port):
    return execute([sys.executable,'-m','platformio','run','--project-dir',ROOT,'-e',environment,'-t','upload','--upload-port',port])

def led_campaign(port):
    # Evita di avviare per errore sei replay fisici con il firmware servo.
    device=rocket.Device(port)
    try:
        device.send('INFO');identity=rocket.fields(device.until('INFO '));device.until('STATUS ')
        if identity.get('firmware')!='BANCO' or identity.get('servo_enabled')!='0':
            print('Caricare prima la modalita LED, menu 1.');return
    finally:device.close()
    stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f');folder=ROOT/'logs'/f'{stamp}_ram_led'
    code=execute([sys.executable,ROOT/'tools/rocket.py','--port',port,'campaign','ram','--out',folder,'--remove-downloaded'])
    if folder.exists():
        archive=folder.with_suffix('.zip')
        with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
            for path in sorted(folder.rglob('*')):
                if path.is_file():out.write(path,path.relative_to(folder.parent))
        print('ZIP DA ALLEGARE:',archive)
    print('Campagna completata.' if code==0 else 'Leggere errore e conservare risultati.')

def main():
    port='COM6'
    while True:
        print(f'''
AVIONICS - progetto unico, porta {port}
Chiudi il Monitor seriale di VSCode. Upload compila e carica la modalita scelta.
1  Carica SIMULAZIONI LED       (banco_psram)
2  Carica SIMULAZIONI SERVO     (banco_servo_psram)
3  Carica SENSORI REALI         (sensori_psram)
4  INFO del firmware attuale
5  SERVO CHIUDI 130 gradi
6  SERVO APRI 90 gradi (prima CHIUDI)
7  SERVO OFF: stacca il segnale, alimentazione ancora presente
8  SERVO: tre replay NOMINALE / FALSO_DECOLLO / BUCO_BARO
9  SENSORI: 60 secondi da fermo
10 SENSORI: 180 secondi per scale
11 LED: le sei prove RAM/OFF gia note
12 SERVO: scegli un singolo dataset
13 SERVO: tutti i 18 dataset, BASE, circa 20-30 minuti
P  Cambia porta
0  Esci
''')
        choice=input('Numero: ').strip().upper()
        if choice=='0':return
        if choice=='P':
            value=input('Porta (es. COM6): ').strip()
            if value:port=value
            continue
        if choice in ['1','2','3']:
            upload({'1':'banco_psram','2':'banco_servo_psram','3':'sensori_psram'}[choice],port);continue
        if choice=='4':execute([sys.executable,ROOT/'tools/rocket.py','--port',port,'info']);continue
        if choice in ['5','6','7']:
            execute([sys.executable,ROOT/'tools/servo.py','--port',port,{'5':'close','6':'open','7':'off'}[choice]]);continue
        if choice=='8':execute([sys.executable,ROOT/'tools/servo.py','--port',port,'test']);continue
        if choice in ['9','10']:
            execute([sys.executable,ROOT/'tools/sensori.py','--port',port,'run','--seconds','60' if choice=='9' else '180',
                     '--label','fermo' if choice=='9' else 'scale']);continue
        if choice=='11':led_campaign(port);continue
        if choice=='12':
            for index,scenario in enumerate(rocket.SCENARIOS,1):print(index,scenario)
            value=input('Dataset: ').strip()
            if not value.isdigit() or not 1<=int(value)<=len(rocket.SCENARIOS):print('Numero non valido.');continue
            execute([sys.executable,ROOT/'tools/servo.py','--port',port,'test','--scenario',rocket.SCENARIOS[int(value)-1]]);continue
        if choice=='13':execute([sys.executable,ROOT/'tools/servo.py','--port',port,'test','--all','--clock','MODEL']);continue
        print('Scegli un numero della lista.')

if __name__=='__main__':
    try:main()
    except (OSError,RuntimeError,TimeoutError,ImportError) as error:print('ERRORE:',error,file=sys.stderr)
    except KeyboardInterrupt:print('\nMenu chiuso; una prova sulla scheda puo essere ancora attiva.')
