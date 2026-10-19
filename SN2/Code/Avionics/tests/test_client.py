import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
import servo
servo.verify_servo({'count':'1','servo_command':'1'},[{'flags':16},{'flags':16|4|8}])
servo.verify_servo({'count':'0','servo_command':'0'},[{'flags':16}])
for result,rows in [({'count':'1','servo_command':'0'},[{'flags':16|4}]),
                    ({'count':'1','servo_command':'1'},[{'flags':16|8}]),
                    ({'count':'2','servo_command':'1'},[{'flags':16|4|8}])]:
    try:servo.verify_servo(result,rows)
    except ValueError:pass
    else:raise AssertionError('Comando incoerente accettato')
class Device(servo.Device):
    def __init__(self,identity):self.identity=identity;self.sent=[]
    def send(self,line):self.sent.append(line)
    def until(self,prefix,timeout=10):return self.identity if prefix=='INFO ' else 'STATUS armed=0 log_active=0'
bad=Device('INFO firmware=BANCO servo_enabled=0')
try:bad.require_idle()
except RuntimeError:pass
else:raise AssertionError('Modalita LED accettata per servo')
assert bad.sent==['INFO']
good=Device('INFO firmware=BANCO_SERVO servo_enabled=1');good.require_idle()
print('PASS: rifiuto modalita errata, flag servo/deploy, caso senza apertura e doppio deploy')
