"""Banco v4: PSRAM, test ripetibili, download verificati, log v2/v3. Python >=3.9."""
import argparse
import csv
import datetime
import html
import json
import math
import pathlib
import struct
import sys
import time
import zlib

HEADER=struct.Struct('<8sHHIIIIi')
EXTRA_HEADER=struct.Struct('<IIII')
RECORD_V2=struct.Struct('<III6f6h4BIII')
RECORD=struct.Struct('<III6f6h4BIIIII2fII')
SCENARIOS=['NOMINALE','RUMORE','PICCO_BARO','BUCO_IMU','BUCO_BARO','TIMEOUT','FALSO_DECOLLO',
           'BUCO_IMU_BURNOUT','BUCO_BARO_APOGEO','COAST_ZERO','PICCO_BARO_LUNGO','SENSORI_ASSENTI',
           'PAD_RUMOROSO','JITTER','PARACADUTE','QUOTA_NEGATIVA','FALSO_LANDED','BARO_CONGELATO']
PHASES=['PAD','BOOST','COAST','DESCENT','LANDED']
COMMON=['tempo_ms','dt_us','sequenza','forza_ms2','quota_baro_m','h_m','v_ms','bias_ms2',
        'pressione_pa','ax_raw','ay_raw','az_raw','gx_raw','gy_raw','gz_raw',
        'fase','flags','motivo','correzione','cicli_ritardo','record_persi']
COLUMNS=COMMON+['wall_us','dt_reale_us','core_us','quota_vera_m','velocita_vera_ms','coda','crc32']

def read_log(source,recover=False):
    rows=[]
    with open(source,'rb') as inp:
        raw=inp.read(HEADER.size)
        if len(raw)!=HEADER.size: raise ValueError('Header incompleto')
        magic,version,size,period,pre,scenario,axis,sign=HEADER.unpack(raw)
        meta=dict(version=version,period=period,pre=pre,scenario=scenario,axis=axis,sign=sign)
        if version==2 and magic==b'RKTLOG2\0' and size==RECORD_V2.size:
            layout=RECORD_V2;columns=COMMON+['crc32']
        elif version==3 and magic==b'RKTLOG3\0' and size==RECORD.size:
            raw=inp.read(EXTRA_HEADER.size)
            if len(raw)!=EXTRA_HEADER.size: raise ValueError('Header v3 incompleto')
            meta.update(zip(['log_mode','core_mode','clock_mode','duration_ms'],EXTRA_HEADER.unpack(raw)))
            layout=RECORD;columns=COLUMNS
        else: raise ValueError('Formato non supportato')
        previous=None
        while True:
            raw=inp.read(size)
            if not raw: break
            if len(raw)!=size or zlib.crc32(raw[:-4])!=struct.unpack('<I',raw[-4:])[0]:
                if recover:
                    print(f'ATTENZIONE: recuperato solo il prefisso valido, {len(rows)} record',file=sys.stderr);break
                raise ValueError(f'Record {len(rows)} incompleto o CRC errato')
            values=list(layout.unpack(raw))
            if previous is not None and values[2]!=previous+1:
                raise ValueError(f'Salto sequenza {previous} -> {values[2]}')
            previous=values[2]
            if values[15]>=len(PHASES): raise ValueError('Fase non valida')
            values[15]=PHASES[values[15]]
            rows.append(dict(zip(columns,values)))
    return meta,rows

def decode(source,destination,recover=False):
    meta,rows=read_log(source,recover)
    path=pathlib.Path(destination);path.parent.mkdir(parents=True,exist_ok=True)
    columns=COLUMNS if meta['version']==3 else COMMON+['crc32']
    with path.open('x',newline='',encoding='utf8') as out:
        writer=csv.DictWriter(out,fieldnames=columns);writer.writeheader();writer.writerows(rows)
    print(f'CSV: {path}; {len(rows)} record; v{meta["version"]}; asse={meta["axis"]}, segno={meta["sign"]}')
    return meta,rows

def svg_chart(rows,fields,title,wall=False,threshold=None):
    # Grafico SVG autonomo; nessuna libreria esterna, nessun accesso Internet.
    width,height=920,270;left,right,top,bottom=65,20,30,35
    points=[]
    for key,color,label in fields:
        series=[((r.get('wall_us',r['tempo_ms']*1000)/1e6 if wall else r['tempo_ms']/1000),r.get(key,math.nan)) for r in rows]
        points.append((series,color,label))
    finite=[v for series,_,_ in points for _,v in series if math.isfinite(v)]
    if not finite: return ''
    low,high=min(finite),max(finite)
    if threshold is not None: low=min(low,threshold);high=max(high,threshold)
    margin=max((high-low)*.06,.1);low-=margin;high+=margin
    xmax=max((x for series,_,_ in points for x,_ in series),default=1) or 1
    def xy(x,y): return left+x/xmax*(width-left-right),height-bottom-(y-low)/(high-low)*(height-top-bottom)
    out=[f'<h3>{html.escape(title)}</h3><svg viewBox="0 0 {width} {height}" role="img" aria-label="{html.escape(title)}">']
    for step in range(5):
        y=low+(high-low)*step/4;_,py=xy(0,y)
        out.append(f'<path d="M {left} {py} H {width-right}" stroke="#ddd"/><text x="3" y="{py+4}" font-size="11">{y:.2f}</text>')
    for step in range(6):
        t=xmax*step/5;px,py=xy(t,low)
        out.append(f'<text x="{px-8}" y="{py+20}" font-size="11">{t:.1f}s</text>')
    for series,color,label in points:
        # Conserva ogni campione (anche un ritardo isolato); NaN interrompe la linea.
        segments=[];segment=[]
        for x,y in series:
            if math.isfinite(y):
                px,py=xy(x,y);segment.append(f'{px:.2f},{py:.2f}')
            elif segment: segments.append(segment);segment=[]
        if segment: segments.append(segment)
        for segment in segments: out.append(f'<polyline points="{" ".join(segment)}" fill="none" stroke="{color}" stroke-width="1.3"/>')
    if threshold is not None:
        _,py=xy(0,threshold);out.append(f'<path d="M {left} {py} H {width-right}" stroke="red" stroke-dasharray="5 4"/>')
    out.append('</svg><p>'+ ' · '.join(f'<span style="color:{color}">{html.escape(label)}</span>' for _,color,label in points)+'</p>')
    return ''.join(out)

def analyze(source,destination):
    meta,rows=read_log(source)
    events=[];previous=None
    for r in rows:
        if r['fase']!=previous:
            events.append(r);previous=r['fase']
    body=f'<h1>{html.escape(pathlib.Path(source).name)}</h1><p>{len(rows)} record, CRC e sequenze verificati. Log v{meta["version"]}.</p>'
    if meta['version']==2: body+='<p>Nei replay v2 dt_us e il tempo matematico: il ritardo reale di ciascun campione non era registrato.</p>'
    else: body+='<p>Quota vera: riferimento del dataset, disponibile soltanto in simulazione. Il profilo del paracadute e prescritto: non risponde al comando del firmware.</p>'
    body+='<table><tr><th>Fase</th><th>Modello / sensori (s)</th><th>Reale (s)</th><th>h stimata</th><th>h vera</th></tr>'
    for r in events:
        wall=r.get('wall_us',math.nan)/1e6
        body+=f'<tr><td>{r["fase"]}</td><td>{r["tempo_ms"]/1000:.3f}</td><td>{wall:.3f}</td><td>{r["h_m"]:.2f}</td><td>{r.get("quota_vera_m",math.nan):.2f}</td></tr>'
    body+='</table>'
    body+=svg_chart(rows,[('h_m','#c0392b','Kalman'),('quota_baro_m','#2980b9','Barometro'),('quota_vera_m','#198754','Riferimento')],'Quota (m) / tempo del modello o dei sensori')
    body+=svg_chart(rows,[('v_ms','#c0392b','Kalman'),('velocita_vera_ms','#198754','Riferimento')],'Velocita verticale (m/s)')
    if meta['version']==3:
        body+=svg_chart(rows,[('dt_reale_us','#8e44ad','Intervallo reale')],'Intervallo reale (microsecondi) / tempo reale',True,20000)
    page='<!doctype html><html lang="it"><meta charset="utf-8"><title>Analisi banco</title><style>body{font:16px system-ui;max-width:1000px;margin:30px auto;padding:15px;color:#223}table{border-collapse:collapse}td,th{padding:8px;border:1px solid #ccc}svg{width:100%;background:#fafafa}p{line-height:1.5}</style>'+body+'</html>'
    with pathlib.Path(destination).open('x',encoding='utf8') as out: out.write(page)
    print('Grafici:',destination)

def fields(line): return dict(item.split('=',1) for item in line.split() if '=' in item)

class Device:
    def __init__(self,port,transcript=None):
        import serial
        self.transcript=transcript
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
            if line:
                print(line)
                if self.transcript: self.transcript.write(line+'\n');self.transcript.flush()
            if line.startswith('ERR'): raise RuntimeError(line)
            if line.startswith(prefix): return line
    def status(self):
        self.send('INFO');return self.until('STATUS')
    def require_idle(self):
        status=fields(self.status())
        if status.get('armed')=='1' or status.get('log_active')=='1':
            raise RuntimeError('Dispositivo attivo: attendere la fine, oppure STOP durante un replay')
    def run(self,name,mode='LEGACY',core='BASE',clock='MODEL'):
        self.require_idle();self.send(f'RUN {name} {mode} {core} {clock}')
        result=self.until('RESULT ',timeout=480)
        values=fields(result);values['scenario']=name
        # CHECK non interrompe una campagna diagnostica; resta nel riepilogo.
        return values
    def ramtest(self):
        self.require_idle();self.send('RAMTEST')
        answer=fields(self.until('OK RAMTEST',timeout=60))
        if answer.get('bytes')!='2097152' or answer.get('patterns')!='4':
            raise RuntimeError('RAMTEST inatteso: caricare firmware v4 e conservare output')
    def get(self,name,path):
        path=pathlib.Path(path)
        if path.exists(): raise FileExistsError(f'File gia presente: {path}')
        path.parent.mkdir(parents=True,exist_ok=True)
        self.require_idle();self.send('GET '+name)
        line=self.until('BEGIN ')
        _,remote,size=line.split();size=int(size)
        deadline=time.monotonic()+max(20,size/3000)
        with path.open('xb') as out:
            remaining=size
            while remaining:
                if time.monotonic()>deadline: raise TimeoutError('Download incompleto: file parziale conservato, originale non cancellato')
                data=self.serial.read(min(4096,remaining));out.write(data);remaining-=len(data)
        self.until('END',timeout=5)
        meta,rows=read_log(path) # CRC e continuita' PRIMA di offrire cancellazione
        print(f'Scaricato e verificato {remote}: {size} byte, {len(rows)} record in {path}')
        return meta,rows
    def delete_verified(self,name):
        self.require_idle();self.send('DELETE '+name);self.until('OK DELETE')

def campaign_jobs(kind,repeats):
    if kind=='ram':
        return [('RUMORE',mode,'BASE',clock) for clock in ['MODEL','WALL'] for mode in ['OFF','RAM']]+[
            ('BUCO_BARO','RAM','BASE','WALL'),('PARACADUTE','RAM','BASE','WALL')]
    if kind=='timing':
        jobs=[]
        for rep in range(repeats):
            modes=['OFF','LEGACY','BATCH'];modes=modes[rep%3:]+modes[:rep%3]
            jobs.extend(('RUMORE',mode,'BASE','MODEL') for mode in modes)
        return jobs
    if kind=='clock': return [(s,'BATCH','BASE',clock) for s in ['NOMINALE','RUMORE','BUCO_BARO'] for clock in ['MODEL','WALL']]
    return [(s,'BATCH',core,'MODEL') for s in SCENARIOS for core in ['BASE','CANDIDATO']]

def save_summary(folder,results):
    (folder/'risultati.json').write_text(json.dumps(results,indent=2),encoding='utf8')
    keys=list(dict.fromkeys(k for r in results for k in r))
    with (folder/'risultati.csv').open('w',newline='',encoding='utf8') as out:
        writer=csv.DictWriter(out,fieldnames=keys);writer.writeheader();writer.writerows(results)
    cols=['scenario','mode','core','clock','algorithm','accuracy','landing','log','timing','deploy_ms','deploy_wall_us','predeploy_max_dt_us','flash_ops_capture','save_ms']
    lines=['# Risultati misurati sulla scheda','', 'CHECK richiede esame; SKIP significa misura non prevista. Le soglie sono provvisorie per il banco.','',
           '| '+' | '.join(cols)+' |','| '+' | '.join(['---']*len(cols))+' |']
    lines.extend('| '+' | '.join(str(r.get(k,'')) for k in cols)+' |' for r in results)
    lines+=['','I dettagli e i file HTML sono nella stessa cartella. Il test MODEL separa il tempo del dataset dal tempo reale. WALL usa il tempo trascorso sulla scheda.']
    (folder/'LEGGIMI_RISULTATI.md').write_text('\n'.join(lines)+'\n',encoding='utf8')

def collect_run(device,job,folder,index,remove):
    name,mode,core,clock=job
    print(f'\nPROVA {index}: {name} / {mode} / {core} / {clock}')
    result=device.run(*job)
    result['test']=str(index)
    # Salva subito l'esito: anche un download fallito lascia la diagnostica.
    report=folder/f'{index:02d}_result.json'
    report.write_text(json.dumps(result,indent=2),encoding='utf8')
    for key,value in [('mode',mode),('core',core),('clock',clock)]:
        if result.get(key)!=value: raise RuntimeError('Opzioni firmware non corrispondenti: caricare il nuovo banco')
    if mode=='RAM' and result.get('flash_ops_capture')!='0':
        raise RuntimeError('Manca conferma zero scritture durante acquisizione RAM; esito conservato')
    if mode!='OFF':
        base=folder/f'{index:02d}_{name}_{mode}_{core}_{clock}'
        if result.get('log')=='FAIL':
            # Non cancellare mai un log fallito; prova il recupero RAM in un file distinto.
            if mode=='RAM' and result.get('ram_pending')=='1':
                try:
                    recovery=base.with_name(base.name+'_recupero_RAM').with_suffix('.bin')
                    _,rows=device.get('RAM',recovery)
                    if len(rows)!=int(result['ram_records']): raise ValueError('Numero record recuperati inatteso')
                    result['recovered_ram_bin']=recovery.name
                except (ValueError,RuntimeError,OSError,TimeoutError) as error:
                    result['recovery_error']=str(error)
            report.write_text(json.dumps(result,indent=2),encoding='utf8')
            return result
        binary=base.with_suffix('.bin');meta,rows=device.get(result['file'],binary)
        if len(rows)!=int(result['rows']): raise ValueError('Numero record scaricati diverso da RESULT: originale conservato')
        if mode=='RAM' and (meta.get('log_mode')!=3 or len(rows)!=int(result['ram_records']) or result.get('ram_pending')!='0'):
            raise ValueError('Log RAM incoerente: originale conservato')
        decode(binary,base.with_suffix('.csv'));analyze(binary,base.with_suffix('.html'))
        result['local_bin']=binary.name
        if remove: device.delete_verified(result['file']);result['removed_after_copy']='yes'
    report.write_text(json.dumps(result,indent=2),encoding='utf8')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',help='es. COM6; utilizzare USB-UART')
    sub=parser.add_subparsers(dest='action',required=True)
    for name in ['ports','info','list','stop','ramtest','save']: sub.add_parser(name)
    run=sub.add_parser('run');run.add_argument('scenario',choices=SCENARIOS)
    run.add_argument('--mode',choices=['OFF','LEGACY','BATCH','RAM'],default='LEGACY')
    run.add_argument('--core',choices=['BASE','CANDIDATO'],default='BASE')
    run.add_argument('--clock',choices=['MODEL','WALL'],default='MODEL')
    camp=sub.add_parser('campaign');camp.add_argument('kind',choices=['timing','scenari','clock','ram'])
    camp.add_argument('--repeats',type=int,default=3)
    for command in [run,camp]:
        command.add_argument('--out',help='Cartella nuova per risultati e log; obbligatoria per campaign')
        command.add_argument('--remove-downloaded',action='store_true',help='Cancella SOLO il log appena scaricato dopo verifica CRC, sequenze e numero record')
    get=sub.add_parser('get');get.add_argument('name');get.add_argument('--out',required=True)
    for name in ['erase','format']:
        cmd=sub.add_parser(name);cmd.add_argument('--confirm',action='store_true',required=True)
    dec=sub.add_parser('decode');dec.add_argument('source');dec.add_argument('--out',required=True);dec.add_argument('--recover',action='store_true')
    ana=sub.add_parser('analyze');ana.add_argument('source');ana.add_argument('--out',required=True)
    args=parser.parse_args()
    if args.action in ['decode','analyze']:
        if pathlib.Path(args.source).resolve()==pathlib.Path(args.out).resolve(): raise ValueError('Input e output devono differire')
        if pathlib.Path(args.out).exists(): raise FileExistsError(args.out)
        if args.action=='decode': decode(args.source,args.out,args.recover)
        else: analyze(args.source,args.out)
        return
    if args.action=='ports':
        from serial.tools.list_ports import comports
        for p in comports(): print(p.device,p.description)
        return
    if not args.port: parser.error('--port obbligatorio')
    if args.action=='campaign' and (not args.out or args.repeats<1): parser.error('Servono --out e repeats >=1')
    if args.action=='run' and args.remove_downloaded and not args.out: parser.error('--remove-downloaded richiede --out')
    folder=pathlib.Path(args.out) if args.action in ['run','campaign'] and args.out else None
    transcript=None
    if folder:
        folder.mkdir(parents=True,exist_ok=False)
        transcript=(folder/'terminale.txt').open('x',encoding='utf8')
    device=None
    try:
        device=Device(args.port,transcript)
        if args.action=='info': device.status()
        elif args.action=='ramtest': device.ramtest()
        elif args.action=='save':
            device.require_idle();device.send('SAVE');device.until('OK SAVE')
            deadline=time.monotonic()+300
            while True:
                time.sleep(1);status=fields(device.status())
                if status.get('log_active')=='0':
                    if status.get('ram_pending')!='0': raise RuntimeError('Salvataggio fallito: GET RAM prima di RESET')
                    print('Salvataggio terminato.');break
                if time.monotonic()>deadline: raise TimeoutError('Salvataggio ancora attivo: conservare alimentazione e usare INFO')
        elif args.action=='stop': device.send('STOP');device.until('OK STOP')
        elif args.action=='list': device.require_idle();device.send('LIST');device.until('END LIST')
        elif args.action=='get': device.get(args.name,args.out)
        elif args.action in ['run','campaign']:
            jobs=campaign_jobs(args.kind,args.repeats) if args.action=='campaign' else [(args.scenario,args.mode,args.core,args.clock)]
            if any(job[1]=='RAM' for job in jobs): device.ramtest()
            results=[]
            for index,job in enumerate(jobs,1):
                print(f'\nAvanzamento: {index}/{len(jobs)}',flush=True)
                if folder:
                    result=collect_run(device,job,folder,index,args.remove_downloaded);results.append(result);save_summary(folder,results)
                else: result=device.run(*job)
                if result.get('log')=='FAIL' or result.get('numerical','0')!='0':
                    raise RuntimeError('Errore log/numerico: campagna fermata, risultati conservati')
            if folder: print(f'\nFINE {len(jobs)}/{len(jobs)}. Allegare la cartella:',folder)
        elif args.action in ['erase','format']:
            device.require_idle();device.send(args.action.upper()+' CONFIRM');device.until('OK ',timeout=60)
    finally:
        if device: device.close()
        if transcript: transcript.close()

if __name__=='__main__':
    try: main()
    except (ValueError,RuntimeError,OSError,TimeoutError,ImportError) as error:
        print('ERRORE:',error,file=sys.stderr);sys.exit(1)
    except KeyboardInterrupt:
        print('\nInterrotto il client. Il test sulla scheda puo continuare: attendere o usare stop.',file=sys.stderr);sys.exit(130)
