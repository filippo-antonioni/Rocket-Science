"""Menu indipendente: upload, scaricamento e analisi della prova autonoma."""
import argparse
import datetime
import json
import pathlib
import re
import subprocess
import sys
import time
import zipfile
import analisi
import log_io_v4 as logio

ROOT=pathlib.Path(__file__).resolve().parents[1]
PATH=re.compile(r'/pwr[0-9]{4}\.(?:bin|json)')


class Device(logio.Device):
    def status(self):
        self.send('INFO');identity=logio.fields(self.until('INFO '))
        if identity.get('firmware')!='POWER_TEST' or identity.get('revision')!='1':
            raise RuntimeError('Firmware diverso: caricare TestAlimentazione solo prima della prova, non per scaricare log vecchi')
        return self.until('STATUS ')

    def hold(self):
        self.status();self.send('HOLD');self.until('OK HOLD')

    def listing(self):
        self.require_idle();self.send('LIST');files=[]
        deadline=time.monotonic()+15
        while True:
            line=self.line(deadline)
            if line=='END LIST': return sorted(files)
            if line.startswith('ERR'): raise RuntimeError(line)
            if line.startswith('FILE '):
                _,name,size=line.split()
                if not PATH.fullmatch(name): raise ValueError('Nome inatteso')
                files.append((name,int(size)))

    def fetch(self,name,destination):
        if name!='RAM' and not PATH.fullmatch(name): raise ValueError('Nome non consentito')
        self.require_idle();self.send('GET '+name)
        _,remote,size=self.until('BEGIN ').split();size=int(size)
        if remote!=name or not 0<=size<=3*1024*1024: raise ValueError('Download inatteso')
        deadline=time.monotonic()+max(30,size/3000)
        with pathlib.Path(destination).open('xb') as out:
            remaining=size
            while remaining:
                if time.monotonic()>deadline: raise TimeoutError('Download parziale conservato; nessun file cancellato sulla ESP')
                data=self.serial.read(min(4096,remaining));out.write(data);remaining-=len(data)
        self.until('END',timeout=10)
        print('Scaricato:',name,size,'byte',flush=True)


def download(device):
    files=device.listing();binaries=[name for name,size in files if name.endswith('.bin')]
    if not binaries:
        print('Nessun file /pwrNNNN.bin. Se c e stato un reset durante la cattura, la PSRAM non conserva i dati.')
        return 1
    stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    folder=ROOT.parent/'logs'/f'{stamp}_alimentazione'
    folder.mkdir(parents=True,exist_ok=False)
    transcript=(folder/'terminale.txt').open('x',encoding='utf8');device.transcript=transcript
    results=[];names={name for name,size in files}
    try:
        device.status()
        for name in binaries:
            target=folder/pathlib.PurePosixPath(name).stem;target.mkdir()
            binary=target/'alimentazione.bin';device.fetch(name,binary)
            reportname=name[:-4]+'.json';result=None
            if reportname in names:
                path=target/'firmware.json';device.fetch(reportname,path)
                try: result=json.loads(path.read_text(encoding='utf8'))
                except (ValueError,UnicodeError) as error: print('Riepilogo non valido:',error)
            try:
                data=analisi.analyse(binary,target,result)
                results.append(dict(file=name,complete=data['complete'],max_dt_us=data['dt_us']['max'],imu_invalid=data['imu_invalid']))
                print('ANALISI',name,'completo=',data['complete'],'max_dt_us=',data['dt_us']['max'])
            except (ValueError,OSError) as error:
                results.append(dict(file=name,error=str(error)))
                print('DA ESAMINARE:',name,error)
    finally:
        device.transcript=None;transcript.close()
        # A failed/partial transfer is retained too; originals are never deleted.
        (folder/'risultati.json').write_text(json.dumps(results,indent=2),encoding='utf8')
        archive=folder.with_suffix('.zip')
        with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
            for path in sorted(folder.rglob('*')):
                if path.is_file(): out.write(path,path.relative_to(folder.parent))
        print('ZIP DA ALLEGARE:',archive.resolve(),flush=True)
        print('Originali conservati sulla ESP. Non e stata formattata o cancellata la memoria.')
    return 0 if results and all(r.get('complete') for r in results) else 1


def execute(action,port):
    if action=='upload':
        print('Batteria scollegata e filo step-up OUT+ -> ESP 5VIN staccato. Chiudere Monitor seriale e altri menu.')
        command=[sys.executable,'-m','platformio','run','--project-dir',str(ROOT),
                 '-e','alimentazione_psram','-t','upload','--upload-port',port]
        subprocess.run(command,check=True)
        with_device('hold',port)
        print('\nCARICATO. Il conto alla rovescia su questa accensione e sospeso.')
        print('Scollega USB. A batteria scollegata collega i cavi; poi alimenta dalla batteria: partira da solo.')
        return 0
    return with_device(action,port)


def with_device(action,port):
    device=Device(port)
    try:
        device.hold() # Prevent a fresh run when reconnecting USB to download.
        if action in ['info','hold']: return 0
        if action=='list':
            for name,size in device.listing(): print(name,size,'byte')
            return 0
        if action=='download': return download(device)
        if action=='ram':
            folder=ROOT.parent/'logs'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f_RAM_alimentazione')
            folder.mkdir(parents=True);device.fetch('RAM',folder/'alimentazione.bin')
            print('RAM salvata:',folder);return 0
        if action=='save':
            device.send('SAVE');device.until('OK SAVE');device.until('FINISHED POWER',timeout=120);return 0
        raise ValueError('Azione non prevista')
    finally: device.close()


def menu(port):
    while True:
        print(f'''\nTEST ALIMENTAZIONE - porta {port}
1  COMPILA E CARICA (solo prima della prova)
2  INFO e annulla avvio automatico su questa accensione
3  SCARICA E ANALIZZA tutti i log POWER (dopo la prova; nessun upload)
4  Elenca soltanto i log POWER
P  Cambia porta
0  Esci
''')
        choice=input('Numero: ').strip().upper()
        if choice=='0': return 0
        if choice=='P':
            value=input('Porta, es. COM6: ').strip()
            if value: port=value
            continue
        if choice not in ['1','2','3','4']: continue
        try: execute({'1':'upload','2':'info','3':'download','4':'list'}[choice],port)
        except (OSError,RuntimeError,ValueError,TimeoutError,subprocess.CalledProcessError) as error:
            print('ERRORE:',error,flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    parser.add_argument('action',nargs='?',default='menu',choices=['menu','upload','info','download','list','hold','ram','save'])
    args=parser.parse_args()
    return menu(args.port) if args.action=='menu' else execute(args.action,args.port)


if __name__=='__main__':
    try: raise SystemExit(main())
    except KeyboardInterrupt: print('\nMenu chiuso. Il firmware continua la prova se gia avviata.');raise SystemExit(130)
    except (OSError,RuntimeError,ValueError,TimeoutError,subprocess.CalledProcessError) as error:
        print('ERRORE:',error,file=sys.stderr);raise SystemExit(1)
