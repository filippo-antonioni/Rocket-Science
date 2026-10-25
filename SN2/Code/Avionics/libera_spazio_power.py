"""Scarica, verifica e rimuove tutti e soli i log /pwrNNNN.bin e .json.

Versione 2, riutilizzabile dopo ogni prova POWER_TEST. Nessuna formattazione,
nessun upload firmware. Anche i tentativi vuoti/incompleti vengono conservati
nel backup prima di essere rimossi dalla ESP. Salvare dentro Avionics.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import time
import zipfile
import zlib

NAME=re.compile(r'/pwr[0-9]{4}\.(?:bin|json)')
HEADER=struct.Struct('<8sHHIIIIiIIII')
RECORD=struct.Struct('<III6f6h4BIIIII2fII')


def fields(line): return dict(x.split('=',1) for x in line.split() if '=' in x)


def inspect_file(name,data):
    """Invalid content is retained too, but needs a second identical transfer."""
    if name.endswith('.json'):
        try:
            result=json.loads(data)
            expected=name[:-5]+'.bin'
            if not isinstance(result,dict) or result.get('firmware')!='POWER_TEST' or result.get('file')!=expected:
                return dict(status='JSON_UNEXPECTED',confirm=True)
            return dict(status='JSON_OK',confirm=False)
        except (ValueError,UnicodeError): return dict(status='JSON_INCOMPLETE',confirm=True)
    if len(data)<HEADER.size: return dict(status='HEADER_INCOMPLETE',confirm=True,records=0)
    head=HEADER.unpack_from(data)
    if head[:3]!=(b'RKTLOG3\0',3,88) or head[5]!=101 or head[8:11]!=(3,2,1):
        return dict(status='HEADER_UNEXPECTED',confirm=True,records=0)
    count=(len(data)-HEADER.size)//RECORD.size
    previous=None
    for i in range(count):
        offset=HEADER.size+i*RECORD.size;raw=data[offset:offset+RECORD.size]
        if zlib.crc32(raw[:-4])!=struct.unpack_from('<I',raw,84)[0]:
            return dict(status='RECORD_CRC_ERROR',confirm=True,records=count,bad_record=i)
        values=RECORD.unpack(raw)
        if values[2]!=i or (previous is not None and values[21]-previous!=values[22]):
            return dict(status='SEQUENCE_OR_TIME_ERROR',confirm=True,records=count,bad_record=i)
        if values[1]!=values[22] or (i==0 and values[21]!=values[1]):
            return dict(status='TIME_ERROR',confirm=True,records=count,bad_record=i)
        previous=values[21]
    if (len(data)-HEADER.size)%RECORD.size:
        return dict(status='RECORD_INCOMPLETE',confirm=True,records=count)
    return dict(status='CRC_SEQUENCE_OK' if count else 'HEADER_ONLY',confirm=False,records=count)


class Link:
    def __init__(self,port):
        import serial
        self.serial=serial.Serial(port=None,baudrate=115200,timeout=.25,write_timeout=5)
        self.serial.dtr=False;self.serial.rts=False;self.serial.port=port;self.serial.open()
        time.sleep(.3);self.serial.reset_input_buffer()
        self.transcript=None

    def close(self): self.serial.close()

    def record(self,line):
        if self.transcript: self.transcript.write(line+'\n');self.transcript.flush()

    def send(self,command):
        self.record('> '+command)
        self.serial.write((command+'\n').encode('ascii'));self.serial.flush()

    def line(self,deadline):
        data=bytearray()
        while time.monotonic()<deadline:
            chunk=self.serial.read(1)
            if chunk==b'\n':
                text=data.rstrip(b'\r').decode('utf8',errors='replace');self.record(text);return text
            data.extend(chunk)
        raise TimeoutError('Nessuna risposta completa dalla ESP')

    def until(self,prefix,seconds=10):
        deadline=time.monotonic()+seconds
        while True:
            line=self.line(deadline)
            if line.startswith('ERR'): raise RuntimeError(line)
            if line.startswith(prefix): return line

    def hold(self):
        self.send('INFO');identity=fields(self.until('INFO '));status=fields(self.until('STATUS '))
        if identity.get('firmware')!='POWER_TEST' or identity.get('revision')!='1':
            raise RuntimeError('Firmware diverso da POWER_TEST: nessuna cancellazione')
        if status.get('stage') in ['PREPARE','CAPTURE','SAVING'] or any(
                status.get(k)=='1' for k in ['armed','log_active','ram_pending']):
            raise RuntimeError('Prova/salvataggio attivo o RAM non salvata: attendere. Se e una nuova prova partita dalla USB, RESET e rilancia subito il comando.')
        self.send('HOLD');self.until('OK HOLD')

    def listing(self):
        self.send('LIST');deadline=time.monotonic()+15;files={}
        while True:
            line=self.line(deadline)
            if line=='END LIST': return files
            if line.startswith('ERR'): raise RuntimeError(line)
            if line.startswith('FILE '):
                _,name,size=line.split()
                if not NAME.fullmatch(name) or name in files or not 0<=int(size)<=8*1024*1024:
                    raise RuntimeError('Elenco inatteso: nessuna cancellazione')
                files[name]=int(size)

    def fetch(self,name,path,expected):
        if not NAME.fullmatch(name): raise ValueError('Nome fuori ambito')
        self.send('GET '+name)
        _,remote,size=self.until('BEGIN ').split();size=int(size)
        if remote!=name or size!=expected: raise RuntimeError('Dimensione/nome remoto cambiati: nessuna cancellazione')
        start=last_data=last_progress=time.monotonic();remaining=size;digest=hashlib.sha256()
        deadline=start+max(60,size/3000)
        with path.open('xb') as out:
            while remaining:
                now=time.monotonic()
                if now>deadline or now-last_data>10:
                    raise TimeoutError('Download interrotto: nessun file di questa operazione e stato cancellato')
                data=self.serial.read(min(4096,remaining))
                if data:
                    out.write(data);digest.update(data);remaining-=len(data);last_data=time.monotonic()
                if time.monotonic()-last_progress>=1:
                    done=size-remaining;elapsed=time.monotonic()-start;rate=done/max(elapsed,.01)
                    eta=int(remaining/rate) if rate else 0
                    print(f'\r  {name}: {100*done/max(size,1):5.1f}%  {done}/{size} byte  circa {eta}s rimanenti   ',end='',flush=True)
                    last_progress=time.monotonic()
            out.flush();os.fsync(out.fileno())
        self.until('END')
        print(f'\r  {name}: 100% - {size} byte scaricati.                                  ',flush=True)
        return digest.hexdigest()

    def delete(self,name):
        if not NAME.fullmatch(name): raise ValueError('Nome fuori ambito')
        self.send('DELETE '+name);self.until('OK DELETE')


def write_json(path,value):
    with path.open('w',encoding='utf8') as out:
        json.dump(value,out,indent=2,ensure_ascii=True);out.flush();os.fsync(out.fileno())


def local_name(name):
    stem=name[1:8]
    return Path(stem)/('alimentazione.bin' if name.endswith('.bin') else 'firmware.json')


def add_analysis(folder,entries,project):
    tools=project/'TestAlimentazione'/'tools';source=tools/'analisi.py'
    if not source.is_file():
        print('Modulo grafici non trovato: backup binario comunque completo e verificato.');return
    sys.path.insert(0,str(tools))
    spec=importlib.util.spec_from_file_location('power_cleanup_analysis',source)
    if spec is None or spec.loader is None: return
    module=importlib.util.module_from_spec(spec)
    try: spec.loader.exec_module(module)
    except (ImportError,OSError,SyntaxError) as error:
        print('Grafici non disponibili:',error);return
    for entry in entries:
        if entry['name'].endswith('.bin') and entry['integrity']['status']=='CRC_SEQUENCE_OK':
            binary=folder/entry['local'];meta=binary.parent/'firmware.json';result=None
            if meta.is_file():
                try: result=json.loads(meta.read_bytes())
                except (ValueError,UnicodeError): pass
            try: module.analyse(binary,binary.parent,result)
            except (ValueError,OSError,KeyError,TypeError) as error:
                print('Analisi da esaminare per',entry['name'],':',error)


def archive_verified(folder,entries):
    archive=folder.with_suffix('.zip')
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
        for path in sorted(folder.rglob('*')):
            if path.is_file(): out.write(path,path.relative_to(folder.parent))
    with archive.open('rb+') as out: out.flush();os.fsync(out.fileno())
    with zipfile.ZipFile(archive) as check:
        bad=check.testzip()
        if bad is not None: raise RuntimeError('Backup ZIP corrotto: nessuna cancellazione')
        for entry in entries:
            path=folder.name+'/'+entry['local']
            if hashlib.sha256(check.read(path)).hexdigest()!=entry['sha256']:
                raise RuntimeError('Backup ZIP diverso dai byte scaricati: nessuna cancellazione')
    return archive


def clean(link,project,output=None,analyse=True):
    link.hold();snapshot=link.listing()
    if not snapshot:
        print('FATTO: nessun log POWER sulla ESP. Non c e altro da pulire.');return None
    base=output or project/'logs'
    folder=base/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f_pulizia_power')
    folder.mkdir(parents=True,exist_ok=False)
    print(f'Trovati {len(snapshot)} file POWER, {sum(snapshot.values())/1000000:.2f} MB.')
    print('Backup in:',folder.resolve(),flush=True)
    entries=[];deleted=[];archive=None
    transcript=(folder/'terminale.txt').open('x',encoding='utf8');link.transcript=transcript
    try:
        for index,(name,size) in enumerate(sorted(snapshot.items()),1):
            print(f'File {index}/{len(snapshot)}',flush=True)
            relative=local_name(name);path=folder/relative;path.parent.mkdir(exist_ok=True)
            digest=link.fetch(name,path,size)
            content=path.read_bytes();integrity=inspect_file(name,content)
            if integrity['confirm']:
                print('  Contenuto incompleto/non valido: lo conservo e confermo con una seconda lettura.')
                second=path.with_name('conferma_'+path.name)
                if link.fetch(name,second,size)!=digest:
                    raise RuntimeError('Due letture diverse dello stesso file: nessuna cancellazione')
            print('  Verifica:',integrity['status'],flush=True)
            entries.append(dict(name=name,size=size,local=relative.as_posix(),sha256=digest,integrity=integrity))
        if analyse: add_analysis(folder,entries,project)
        write_json(folder/'manifest.json',dict(firmware='POWER_TEST',files=entries,
            note='Backup verificato PRIMA della rimozione. Anche i tentativi incompleti sono conservati.'))
        link.transcript=None;transcript.close()
        archive=archive_verified(folder,entries)
        print('ZIP DA ALLEGARE:',archive.resolve(),flush=True)
        print('Backup ZIP verificato. Ora rimuovo soltanto i file POWER appena copiati.',flush=True)
        link.hold()
        if link.listing()!=snapshot:
            raise RuntimeError('Elenco cambiato durante il backup: nessuna cancellazione')
        for name in sorted(snapshot):
            link.delete(name);deleted.append(name)
            print('Rimosso dalla ESP:',name,flush=True)
            write_json(folder/'rimozione.json',dict(removed=deleted,backup_zip=str(archive.resolve())))
        if link.listing(): raise RuntimeError('Restano file POWER: non dichiaro completata la pulizia')
        write_json(folder/'rimozione.json',dict(complete=True,removed=deleted,backup_zip=str(archive.resolve())))
        print(f'FATTO: rimossi {len(deleted)} file POWER, circa {sum(snapshot.values())/1000000:.2f} MB liberati.')
        print('Gli altri log e il firmware non sono stati toccati. Nessuna formattazione.')
        print('Scollega USB prima di ripristinare step-up -> 5VIN e batteria. Il prossimo avvio partira da solo.')
        return archive
    except BaseException as error:
        print('\nOperazione interrotta. File gia rimossi:',len(deleted),flush=True)
        print('Backup locale conservato:',archive.resolve() if archive else folder.resolve(),flush=True)
        try: write_json(folder/'rimozione.json',dict(complete=False,removed=deleted,error=str(error),
            backup_zip=str(archive.resolve()) if archive else None))
        except OSError: pass
        raise
    finally:
        link.transcript=None
        if not transcript.closed: transcript.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    parser.add_argument('--out',type=Path,help='Cartella backup alternativa; predefinita Avionics/logs')
    parser.add_argument('--senza-grafici',action='store_true',help='Salva comunque tutti i file binari e i riepiloghi')
    args=parser.parse_args()
    print('PULIZIA POWER v2: backup verificato, poi rimozione di tutti e soli /pwrNNNN.bin e .json.')
    print('Batteria scollegata, step-up -> 5VIN staccato, ESP via USB. Chiudere CMD e Monitor seriale.')
    link=Link(args.port)
    try: clean(link,Path(__file__).resolve().parent,args.out,not args.senza_grafici)
    finally: link.close()


if __name__=='__main__':
    try: main()
    except KeyboardInterrupt: print('\nInterrotto. Le copie gia scaricate rimangono sul PC.');sys.exit(130)
    except (OSError,ValueError,RuntimeError,TimeoutError,ImportError,zipfile.BadZipFile) as error:
        print('ERRORE:',error,file=sys.stderr);sys.exit(1)
