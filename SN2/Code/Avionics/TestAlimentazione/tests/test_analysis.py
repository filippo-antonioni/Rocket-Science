import json
import pathlib
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
import analisi

H=struct.Struct('<8sHHIIIIiIIII')
R=struct.Struct('<III6f6h4BIIIII2fII')


def fixture():
    data=bytearray(H.pack(b'RKTLOG3\0',3,88,10000,300,101,0,-1,3,2,1,180000))
    for i in range(18000):
        us=(i+1)*10000;ms=us//1000
        reason=0 if ms<60000 else 1 if ms<90000 else 2 if ms<120000 else 3
        flags=3|(8 if reason in (1,3) else 0)
        row=R.pack(ms,10000,i,9.80665,0,0,0,0,101325,-2048,0,0,0,0,0,
                   0,flags,reason,2,0,0,us,10000,100,float('nan'),float('nan'),0,0)
        data.extend(row[:-4]+struct.pack('<I',zlib.crc32(row[:-4])))
    return bytes(data)


class AnalysisTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.binary=fixture()

    def run_analysis(self,data,result=None):
        with tempfile.TemporaryDirectory() as temp:
            folder=pathlib.Path(temp);path=folder/'input.bin';path.write_bytes(data)
            return analisi.analyse(path,folder,result)

    def test_complete_and_event_windows(self):
        r=self.run_analysis(self.binary,dict(rows=18000,complete=1,log='PASS',ram_pending=0,
                                           flash_ops_capture=0,servo='PASS'))
        self.assertTrue(r['complete']);self.assertFalse(r['voltage_measured'])
        self.assertEqual([e['time_s'] for e in r['events']],[60,90,120])
        self.assertEqual(len(r['servo_windows']),6)
        self.assertTrue(all(w['max_dt_us']==10000 for w in r['servo_windows']))

    def test_no_summary_never_claims_complete(self):
        self.assertFalse(self.run_analysis(self.binary)['complete'])

    def test_crc_corruption_rejected(self):
        data=bytearray(self.binary);data[48+42]^=1
        with self.assertRaisesRegex(ValueError,'CRC'): self.run_analysis(data)

    def test_truncated_record_rejected(self):
        with self.assertRaisesRegex(ValueError,'incompleto'): self.run_analysis(self.binary[:-1])

    def test_sequence_gap_rejected(self):
        with self.assertRaisesRegex(ValueError,'sequenza'):
            self.run_analysis(self.binary[:48+88]+self.binary[48+176:])

    def test_header_only_rejected(self):
        with self.assertRaisesRegex(ValueError,'senza campioni'): self.run_analysis(self.binary[:48])

    def test_wrong_firmware_rejected(self):
        data=bytearray(self.binary);struct.pack_into('<I',data,20,100)
        with self.assertRaisesRegex(ValueError,'diverso'): self.run_analysis(data)

    def test_summary_counter_mismatch_rejected(self):
        with self.assertRaisesRegex(ValueError,'Numero record'):
            self.run_analysis(self.binary,dict(rows=1))

    def test_servo_flag_inconsistent_rejected(self):
        data=bytearray(self.binary);offset=48+88*6000
        fields=list(R.unpack_from(data,offset));fields[16]&=~8
        raw=R.pack(*fields);data[offset:offset+88]=raw[:-4]+struct.pack('<I',zlib.crc32(raw[:-4]))
        with self.assertRaisesRegex(ValueError,'Flag PWM'): self.run_analysis(data)

    def test_clock_inconsistent_rejected(self):
        data=bytearray(self.binary);offset=48+88*4000
        fields=list(R.unpack_from(data,offset));fields[21]+=1
        raw=R.pack(*fields);data[offset:offset+88]=raw[:-4]+struct.pack('<I',zlib.crc32(raw[:-4]))
        with self.assertRaisesRegex(ValueError,'Tempi non continui'): self.run_analysis(data)


if __name__=='__main__': unittest.main()
