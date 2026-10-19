import pathlib
import subprocess
import sys
import tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    exe=pathlib.Path(tmp)/'test'
    for sensors,servo in [(0,0),(0,1),(1,0)]:
        subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsyntax-only',
                        '-DBOARD_HAS_PSRAM','-DFIRMWARE_VOLO=0','-DMODE_SENSORI='+str(sensors),'-DBENCH_SERVO='+str(servo),
                        '-I',str(root/'tests/hardware_mocks'),str(root/'src/main.cpp')],check=True)
    print('PASS: tre ambienti, un solo setup/loop; sintassi con mock, non build ESP32')
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root/'tests/hardware_mocks'),
                    str(root/'tests/test_servo.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    subprocess.run([sys.executable,str(root/'tests/test_client.py')],check=True)
    for name in ['test_core','test_scenarios','test_evidence','replay_v3']:
        subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(root/f'tests/{name}.cpp'),'-o',str(exe)],check=True)
        result=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
        print('PASS: '+name)
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-pthread','-I',str(root/'tests/hardware_mocks'),
                    str(root/'tests/hardware_mocks/check_logger.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
