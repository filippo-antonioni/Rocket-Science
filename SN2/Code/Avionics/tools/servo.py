"""Comandi manuali e replay con servo, solo firmware BANCO_SERVO."""
import argparse
import datetime
import json
import pathlib
import sys
import zipfile
import rocket

ROOT=pathlib.Path(__file__).resolve().parents[1]

class Device(rocket.Device):
    def status(self):
        self.send('INFO');identity=rocket.fields(self.until('INFO '))
        if identity.get('firmware')!='BANCO_SERVO' or identity.get('servo_enabled')!='1':
            raise RuntimeError('Caricare prima la modalita banco_servo_psram (menu 2)')
        return self.until('STATUS ')

def verify_servo(result,rows):
    count=int(result['count'])
    commanded=result.get('servo_command')
    if commanded!=str(int(count>0)):
        raise ValueError('Comando servo incoerente con deploy: log conservato sulla scheda')
    if sum(bool(r['flags']&8) and not bool(r['flags']&4) for r in rows):
        raise ValueError('Servo comandato prima del deploy: log conservato')
    if any(bool(r['flags']&8)!=bool(r['flags']&4) for r in rows):
        raise ValueError('Flag servo/deploy diversi: log conservato')
    if count not in [0,1]: raise ValueError('Numero comandi deploy inatteso')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    sub=parser.add_subparsers(dest='action',required=True)
    for action in ['close','open','off']:sub.add_parser(action)
    suite=sub.add_parser('test')
    choice=suite.add_mutually_exclusive_group()
    choice.add_argument('--scenario',choices=rocket.SCENARIOS)
    choice.add_argument('--all',action='store_true')
    suite.add_argument('--clock',choices=['MODEL','WALL'],default='WALL')
    suite.add_argument('--pause-between',action='store_true')
    args=parser.parse_args()
    device=transcript=folder=None
    if args.action=='test':
        stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        folder=ROOT/'logs'/f'{stamp}_servo';folder.mkdir(parents=True,exist_ok=False)
        transcript=(folder/'terminale.txt').open('x',encoding='utf8')
    try:
        device=Device(args.port,transcript)
        if args.action!='test':
            device.require_idle();device.send('SERVO '+args.action.upper());device.until('OK SERVO');return
        device.require_idle();device.ramtest()
        scenarios=[args.scenario] if args.scenario else rocket.SCENARIOS if args.all else ['NOMINALE','FALSO_DECOLLO','BUCO_BARO']
        results=[]
        print('Ogni prova richiude a 130 gradi, poi usa il comando algoritmo per aprire a 90 gradi.')
        for index,scenario in enumerate(scenarios,1):
            if args.pause_between:input(f'Prepara il meccanismo per {scenario}; INVIO avvia la prova: ')
            print(f'PROVA SERVO {index}/{len(scenarios)}: {scenario}',flush=True)
            result=rocket.collect_run(device,(scenario,'RAM','BASE',args.clock),folder,index,False)
            results.append(result);rocket.save_summary(folder,results)
            if result.get('log')!='PASS' or result.get('numerical')!='0':
                raise RuntimeError('Errore log/numerico: risultati conservati, campagna fermata')
            _,rows=rocket.read_log(folder/result['local_bin']);verify_servo(result,rows)
            result['servo_check']='PASS_COMMAND_ONLY'
            device.delete_verified(result['file']);result['removed_after_copy']='yes'
            rocket.save_summary(folder,results)
            print('Comando software verificato. Il movimento fisico va osservato direttamente.')
        print(f'FINE SERVO {len(scenarios)}/{len(scenarios)}')
    finally:
        if device:device.close()
        if transcript:transcript.close()
        if folder:
            archive=folder.with_suffix('.zip')
            with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as out:
                for path in sorted(folder.rglob('*')):
                    if path.is_file():out.write(path,path.relative_to(folder.parent))
            print('ZIP DA ALLEGARE:',archive)

if __name__=='__main__':
    try:main()
    except (ValueError,RuntimeError,OSError,TimeoutError,ImportError) as error:
        print('ERRORE:',error,file=sys.stderr);sys.exit(1)
    except KeyboardInterrupt:
        print('Client interrotto: il replay gia avviato sulla ESP puo continuare.',file=sys.stderr);sys.exit(130)
