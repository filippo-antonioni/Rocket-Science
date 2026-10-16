import csv
import importlib.util
import pathlib
import subprocess
import tempfile
root=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('rocket',root/'tools/rocket.py')
rocket=importlib.util.module_from_spec(spec);spec.loader.exec_module(rocket)
with tempfile.TemporaryDirectory() as temp:
    temp=pathlib.Path(temp);exe=temp/'fixture';binary=temp/'log.bin'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(root/'tests/log_fixture.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(binary)],check=True)
    rocket.decode(binary,temp/'out.csv')
    with (temp/'out.csv').open() as f: rows=list(csv.DictReader(f))
    assert len(rows)==3 and rows[0]['fase']=='DESCENT' and rows[1]['forza_ms2']=='nan'
    assert rows[2]['sequenza']=='102' and rows[0]['ax_raw']=='0' and rows[0]['az_raw']=='2048'
    data=bytearray(binary.read_bytes());data[32+64+8]^=1
    (temp/'bad.bin').write_bytes(data)
    try: rocket.decode(temp/'bad.bin',temp/'bad.csv')
    except ValueError: pass
    else: raise AssertionError('CRC corrotto non rilevato')
    (temp/'partial.bin').write_bytes(binary.read_bytes()[:-10])
    rocket.decode(temp/'partial.bin',temp/'recover.csv',recover=True)
    with (temp/'recover.csv').open() as f: assert len(list(csv.DictReader(f)))==2
print('PASS: file C++ decodificato in Python; CRC errato e coda troncata rilevati')
