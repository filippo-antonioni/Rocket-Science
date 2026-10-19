"""Prova autonoma dei sensori reali. Eseguire con il Python di PlatformIO."""
import argparse
import csv
import datetime
import html
import json
import math
import pathlib
import re
import statistics
import sys
import threading
import time
import zipfile
import log_io_v4 as logio

ROOT=pathlib.Path(__file__).resolve().parents[1]

def sensor_name(name): return re.fullmatch(r'/sens[0-9]{4}\.bin',name) is not None

class Device(logio.Device):
    def status(self):
        self.send('INFO')
        first=logio.fields(self.until('INFO '))
        if first.get('firmware')!='SENSORI' or first.get('revision')!='1':
            raise RuntimeError('Firmware diverso: caricare ProvaSensori, senza cambiare i file della v4')
        return self.until('STATUS ')
    def require_idle(self):
        status=logio.fields(self.status())
        if any(status.get(key)=='1' for key in ['armed','log_active','pending_report']):
            raise RuntimeError('Test/salvataggio attivo: attendere oppure usare stop e poi attendere')
    def delete_verified(self,name):
        if not sensor_name(name): raise ValueError('Cancellazione consentita solo per /sensNNNN.bin')
        super().delete_verified(name)

def validate(meta,rows,result=None):
    if (meta.get('version'),meta.get('scenario'),meta.get('log_mode'),meta.get('core_mode'),meta.get('clock_mode'))!=(3,100,3,2,1):
        raise ValueError('Il file non appartiene alla prova sensori reali')
    if not rows: raise ValueError('File senza campioni: originale conservato')
    if rows[0]['sequenza']!=0: raise ValueError('Manca l\'inizio del test: originale conservato')
    if any(r['fase']!='PAD' or r['flags']&(4|8|16) for r in rows):
        raise ValueError('File con stati/comandi/simulazione inattesi')
    if any(r['dt_us']!=r['dt_reale_us'] for r in rows): raise ValueError('Clock inatteso')
    for a,b in zip(rows,rows[1:]):
        if b['wall_us']-a['wall_us']!=b['dt_reale_us']: raise ValueError('Tempi non coerenti')
    if result:
        if len(rows)!=int(result['rows']) or len(rows)!=int(result['ram_records']):
            raise ValueError('Numero record diverso da RESULT: originale conservato')
        if result.get('flash_ops_capture')!='0' or result.get('ram_pending')!='0':
            raise ValueError('Salvataggio non confermato: originale conservato')

def enrich(rows):
    for r in rows:
        r['fase']='TEST_SENSORI'
        for axis in ['ax','ay','az']:
            raw=r[axis+'_raw'];r[axis+'_ms2']=raw/2048*9.80665 if raw!=-32768 else math.nan
        r['acc_norm_ms2']=math.sqrt(sum(r[a+'_ms2']**2 for a in ['ax','ay','az']))
        r['imu_valida']=int(bool(r['flags']&1));r['baro_nuovo']=int(bool(r['flags']&2))
    return rows

def describe(values):
    values=[v for v in values if math.isfinite(v)]
    if not values: return None
    return dict(n=len(values),media=statistics.mean(values),std=statistics.pstdev(values),min=min(values),max=max(values))

def reports(binary,folder,result=None):
    meta,rows=logio.read_log(binary);validate(meta,rows,result);enrich(rows)
    # Quota e statistiche baro: conta solo conversioni nuove, non valori ripetuti.
    fresh=[r for r in rows if r['baro_nuovo'] and math.isfinite(r['quota_baro_m'])]
    duration=(int(result['elapsed_ms'])/1000 if result else rows[-1]['wall_us']/1e6)
    summary=dict(campioni=len(rows),durata_s=duration,
        frequenza_ciclo_hz=len(rows)/duration if duration else 0,
        letture_imu_invalide=sum(not r['imu_valida'] for r in rows),
        campioni_baro_nuovi=len(fresh),frequenza_baro_hz=len(fresh)/duration if duration else 0,
        intervallo_us=describe([r['dt_reale_us'] for r in rows]),
        forza_ms2=describe([r['forza_ms2'] for r in rows]),
        norma_acc_ms2=describe([r['acc_norm_ms2'] for r in rows]),
        quota_baro_m=describe([r['quota_baro_m'] for r in fresh]))
    if fresh:
        first=fresh[0]['wall_us'];last=fresh[-1]['wall_us']
        initial=describe([r['quota_baro_m'] for r in fresh if r['wall_us']<=first+5000000])
        final=describe([r['quota_baro_m'] for r in fresh if r['wall_us']>=last-5000000])
        summary['quota_primi_5s']=initial;summary['quota_ultimi_5s']=final
        summary['differenza_finale_iniziale_m']=final['media']-initial['media']
    with (folder/'sensori.csv').open('x',newline='',encoding='utf8') as out:
        writer=csv.DictWriter(out,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    (folder/'statistiche.json').write_text(json.dumps(summary,indent=2,allow_nan=False),encoding='utf8')
    body='<h1>Prova sensori reali</h1><p>CRC e sequenze verificati. Nessuna macchina a stati o uscita di apertura attiva.</p>'
    body+=f'<p>{len(rows)} campioni; durata {duration:.2f} s; frequenza media del ciclo {summary["frequenza_ciclo_hz"]:.2f} Hz; '
    body+=f'letture IMU invalide {summary["letture_imu_invalide"]}; nuove letture baro {summary["frequenza_baro_hz"]:.2f} Hz.</p>'
    body+='<p>La quota barometrica è relativa al punto di calibrazione. Kalman usa un solo asse fisso: durante passi o inclinazioni può essere poco attendibile. La curva Kalman non è una misura vera di riferimento.</p>'
    body+=logio.svg_chart(rows,[('quota_baro_m','#2166ac','Barometro'),('h_m','#b2182b','Kalman indicativo')],'Quota relativa (m)',True)
    body+=logio.svg_chart(rows,[('ax_ms2','#b2182b','X'),('ay_ms2','#238b45','Y'),('az_ms2','#2166ac','Z')],'Accelerometro, assi della scheda (m/s²)',True)
    body+=logio.svg_chart(rows,[('forza_ms2','#7a0177','Forza asse scelto'),('acc_norm_ms2','#238b45','Norma accelerazione')],'Forza specifica e norma (m/s²)',True)
    body+=logio.svg_chart(rows,[('dt_reale_us','#7a0177','Intervallo reale')],'Intervallo fra campioni (µs)',True,20000)
    body+=logio.svg_chart(rows,[('imu_valida','#238b45','IMU valida'),('baro_nuovo','#2166ac','Baro nuovo')],'Disponibilità delle letture',True)
    if result: body+='<h2>Esito firmware</h2><pre>'+html.escape(json.dumps(result,indent=2))+'</pre>'
    page='<!doctype html><html lang="it"><meta charset="utf-8"><title>Prova sensori</title><style>body{font:16px system-ui;max-width:1000px;margin:25px auto;padding:15px;color:#223}svg{width:100%;background:#fafafa}p{line-height:1.5}pre{white-space:pre-wrap}</style>'+body+'</html>'
    (folder/'grafici.html').write_text(page,encoding='utf8')
    print(f'Log verificato: {len(rows)} campioni, IMU invalide {summary["letture_imu_invalide"]}, baro {summary["frequenza_baro_hz"]:.2f} Hz.')
    print('Grafici:',folder/'grafici.html')
    return summary

def progress(stop,seconds):
    elapsed=0
    while not stop.wait(10):
        elapsed+=10
        print(f'  Acquisizione: circa {min(elapsed,seconds)}/{seconds} s; attendere il salvataggio finale.',flush=True)

def run(device,seconds,folder,keep):
    device.require_idle();device.send(f'TEST {seconds}')
    device.until('OK TEST ',timeout=45)
    print('REGISTRAZIONE PARTITA. Per la prova scale: primi 30 s fermo, poi salita/discesa, ultimi 30 s fermo.',flush=True)
    stop=threading.Event();worker=threading.Thread(target=progress,args=(stop,seconds),daemon=True);worker.start()
    try: result=logio.fields(device.until('RESULT SENSORI ',timeout=seconds+120))
    finally: stop.set();worker.join(timeout=1)
    (folder/'risultato.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    if result.get('log')!='PASS':
        if result.get('ram_pending')=='1':
            print('Salvataggio fallito: provo a scaricare la RAM, senza cancellare file sulla scheda.')
            device.get('RAM',folder/'recupero_RAM.bin')
        raise RuntimeError('Log non riuscito: conserva alimentazione e risultati; non fare upload/reset')
    name=result.get('file','')
    if not sensor_name(name): raise ValueError('Nome remoto inatteso: nessuna cancellazione')
    binary=folder/'sensori.bin';meta,rows=device.get(name,binary);validate(meta,rows,result)
    reports(binary,folder,result)
    if not keep:
        device.delete_verified(name)
        print('Rimossa dalla ESP soltanto la copia appena scaricata e verificata:',name)
    if result.get('numerical')!='0': raise RuntimeError('Errore numerico del Kalman: log conservato sul PC, da esaminare')
    print('FINE: prova, salvataggio e download completati.')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    commands=parser.add_subparsers(dest='action',required=True)
    for name in ['info','list','stop','save']: commands.add_parser(name)
    test=commands.add_parser('run');test.add_argument('--seconds',type=int,default=60)
    test.add_argument('--label',default='fermo');test.add_argument('--out')
    test.add_argument('--keep-on-board',action='store_true')
    get=commands.add_parser('get');get.add_argument('name',help='/sens0000.bin oppure RAM');get.add_argument('--out')
    args=parser.parse_args()
    if args.action=='run' and (not 5<=args.seconds<=180 or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}',args.label)):
        parser.error('--seconds tra 5 e 180; --label solo lettere, numeri, _ e -')
    if args.action=='get' and args.name!='RAM' and not sensor_name(args.name): parser.error('Nome: /sensNNNN.bin oppure RAM')
    folder=transcript=device=None
    if args.action in ['run','get']:
        label=args.label if args.action=='run' else 'recupero'
        stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        folder=pathlib.Path(args.out) if args.out else ROOT/'logs'/f'{stamp}_{label}'
        folder.mkdir(parents=True,exist_ok=False);transcript=(folder/'terminale.txt').open('x',encoding='utf8')
        print('Risultati in:',folder.resolve(),flush=True)
    try:
        device=Device(args.port,transcript)
        if args.action=='info': device.status()
        elif args.action=='run': run(device,args.seconds,folder,args.keep_on_board)
        elif args.action=='get':
            device.require_idle();device.get(args.name,folder/'sensori.bin');reports(folder/'sensori.bin',folder)
        elif args.action=='list': device.require_idle();device.send('LIST');device.until('END LIST')
        elif args.action=='stop': device.status();device.send('STOP');device.until('OK STOP')
        elif args.action=='save':
            device.require_idle();device.send('SAVE');device.until('OK SAVE')
            deadline=time.monotonic()+300
            while True:
                time.sleep(1);status=logio.fields(device.status())
                if status.get('log_active')=='0':
                    if status.get('ram_pending')!='0': raise RuntimeError('SAVE fallito: recuperare con get RAM')
                    print('Salvataggio terminato.');break
                if time.monotonic()>deadline: raise TimeoutError('Salvataggio ancora attivo: attendere, usare info')
    finally:
        if device: device.close()
        if transcript: transcript.close()
        if folder:
            archive=folder.with_suffix('.zip')
            with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
                for path in sorted(folder.rglob('*')):
                    if path.is_file(): out.write(path,path.relative_to(folder.parent))
            print('ZIP DA ALLEGARE:',archive.resolve())

if __name__=='__main__':
    try: main()
    except (ValueError,RuntimeError,OSError,TimeoutError,ImportError) as error:
        print('ERRORE:',error,file=sys.stderr);sys.exit(1)
    except KeyboardInterrupt:
        print('\nClient interrotto: la ESP continua fino alla durata prevista. Non scollegare; poi usa list e get.',file=sys.stderr);sys.exit(130)
