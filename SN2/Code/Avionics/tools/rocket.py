"""Comandi USB/seriale e conversione .bin -> .csv. Python >=3.9; pip install pyserial.
Esempio: python tools/rocket.py --port COM5 run NOMINALE
Il monitor PlatformIO deve essere chiuso quando questo programma usa la porta.
"""
import argparse
import csv
import pathlib
import struct
import sys
import time
import zlib

HEADER=struct.Struct('<8sHHIIIIi')
RECORD=struct.Struct('<III6f6h4BIII')
SCENARIOS=['NOMINALE','RUMORE','PICCO_BARO','BUCO_IMU','BUCO_BARO','TIMEOUT','FALSO_DECOLLO']
PHASES=['PAD','BOOST','COAST','DESCENT','LANDED']
COLUMNS=['tempo_ms','dt_us','sequenza','forza_ms2','quota_baro_m','h_m','v_ms','bias_ms2',
         'pressione_pa','ax_raw','ay_raw','az_raw','gx_raw','gy_raw','gz_raw',
         'fase','flags','motivo','correzione','cicli_ritardo','record_persi','crc32']

def decode(source, destination, recover=False):
    count=0
    with open(source,'rb') as inp, open(destination,'w',newline='',encoding='utf8') as out:
        header=inp.read(HEADER.size)
        if len(header)!=HEADER.size: raise ValueError('Header incompleto')
        magic,version,size,period,pre,scenario,axis,sign=HEADER.unpack(header)
        if magic!=b'RKTLOG2\0' or version!=2 or size!=RECORD.size:
            raise ValueError('Formato non supportato')
        writer=csv.writer(out);writer.writerow(COLUMNS)
        previous=None
        while True:
            raw=inp.read(size)
            if not raw: break
            if len(raw)!=size or zlib.crc32(raw[:-4])!=struct.unpack('<I',raw[-4:])[0]:
                if recover:
                    print(f'ATTENZIONE: recuperati {count} record; resto incompleto/corrotto scartato',file=sys.stderr)
                    break
                raise ValueError(f'Record {count} incompleto o CRC errato; usare --recover per salvare il prefisso valido')
            row=list(RECORD.unpack(raw))
            if previous is not None and row[2]!=previous+1:
                print(f'ATTENZIONE: salto sequenza {previous} -> {row[2]}',file=sys.stderr)
            previous=row[2]
            row[15]=PHASES[row[15]] if row[15]<len(PHASES) else 'INVALID'
            writer.writerow(row);count+=1
    print(f'CSV: {destination}; {count} record; source={scenario}, asse={axis}, segno={sign}, passo={period} us, pre={pre}')

class Device:
    def __init__(self,port):
        import serial
        self.serial=serial.Serial(port=None,baudrate=115200,timeout=.25,write_timeout=5)
        self.serial.dtr=False;self.serial.rts=False
        self.serial.port=port;self.serial.open()
        time.sleep(.4);self.serial.reset_input_buffer()
    def close(self): self.serial.close()
    def send(self,line): self.serial.write((line+'\n').encode('ascii'));self.serial.flush()
    def line(self,deadline):
        data=bytearray()
        while time.monotonic()<deadline:
            b=self.serial.read(1)
            if b==b'\n': return data.rstrip(b'\r').decode('utf8','replace')
            data.extend(b)
        raise TimeoutError('Nessuna risposta completa: porta/USB corretta?')
    def until(self,prefix,timeout=10):
        deadline=time.monotonic()+timeout
        while True:
            line=self.line(deadline)
            if line: print(line)
            if line.startswith('ERR'): raise RuntimeError(line)
            if line.startswith(prefix): return line
    def status(self):
        self.send('INFO');return self.until('STATUS')
    def require_idle(self):
        status=self.status()
        if 'armed=1' in status or 'log_active=1' in status:
            raise RuntimeError('Dispositivo attivo: STOP sul banco e attendere log_active=0; non interrompere un volo')
    def run(self,name):
        self.require_idle();self.send('RUN '+name)
        result=self.until('RESULT ',timeout=100)
        if 'algorithm=FAIL' in result or 'log=FAIL' in result or 'timing=CHECK' in result:
            raise RuntimeError('Test da esaminare: '+result)
    def get(self,name,path):
        self.require_idle();self.send('GET '+name)
        line=self.until('BEGIN ')
        _,remote,size=line.split();size=int(size)
        path=pathlib.Path(path);path.parent.mkdir(parents=True,exist_ok=True)
        if path.exists(): raise FileExistsError(f'File gia presente: {path}')
        deadline=time.monotonic()+max(20,size/3000)
        with path.open('xb') as out:
            remaining=size
            while remaining:
                if time.monotonic()>deadline: raise TimeoutError('Download incompleto: conservato file parziale')
                data=self.serial.read(min(4096,remaining))
                out.write(data);remaining-=len(data)
        self.until('END',timeout=5)
        print(f'Scaricato {remote}: {size} byte in {path}')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',help='es. COM5 su Windows, /dev/ttyUSB0 su Linux')
    sub=parser.add_subparsers(dest='action',required=True)
    sub.add_parser('ports');sub.add_parser('info');sub.add_parser('list');sub.add_parser('suite')
    run=sub.add_parser('run');run.add_argument('scenario',choices=SCENARIOS)
    get=sub.add_parser('get');get.add_argument('name');get.add_argument('--out',required=True)
    erase=sub.add_parser('erase');erase.add_argument('--confirm',action='store_true',required=True)
    fmt=sub.add_parser('format');fmt.add_argument('--confirm',action='store_true',required=True)
    dec=sub.add_parser('decode');dec.add_argument('source');dec.add_argument('--out',required=True);dec.add_argument('--recover',action='store_true')
    args=parser.parse_args()
    if args.action=='decode':
        if pathlib.Path(args.source).resolve()==pathlib.Path(args.out).resolve(): raise ValueError('Input e output devono differire')
        if pathlib.Path(args.out).exists(): raise FileExistsError(args.out)
        decode(args.source,args.out,args.recover);return
    if args.action=='ports':
        from serial.tools.list_ports import comports
        for p in comports(): print(p.device,p.description)
        return
    if not args.port: parser.error('--port e obbligatorio per questa azione')
    if args.action=='get' and pathlib.Path(args.out).exists(): raise FileExistsError(args.out)
    device=Device(args.port)
    try:
        if args.action=='info': device.status()
        elif args.action=='list': device.require_idle();device.send('LIST');device.until('END LIST')
        elif args.action=='run': device.run(args.scenario)
        elif args.action=='suite':
            for s in SCENARIOS: device.run(s)
        elif args.action=='get': device.get(args.name,args.out)
        elif args.action in ('erase','format'):
            device.require_idle();device.send(args.action.upper()+' CONFIRM');device.until('OK ',timeout=60)
    finally: device.close()

if __name__=='__main__':
    try: main()
    except (ValueError,RuntimeError,OSError,TimeoutError,ImportError) as e:
        print('ERRORE:',e,file=sys.stderr);sys.exit(1)
