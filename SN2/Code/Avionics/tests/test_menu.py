"""Verifica l'instradamento dei menu senza aprire seriali o compilare firmware."""
from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import avionics
import banco


class MenuTests(unittest.TestCase):
    def test_diagnostics_preserve_port_and_use_existing_scripts(self):
        calls = []
        with patch('builtins.input', side_effect=['P', 'COM12', '14', '15', '16', '17', '18', '0']), \
                patch.object(avionics, 'execute', side_effect=lambda args: calls.append(args)), \
                redirect_stdout(io.StringIO()):
            avionics.main()
        expected = [
            ('TestAlimentazione/tools/prova.py', []),
            ('DiagnosiI2C/prova.py', []),
            ('DiagnosiServo/prova.py', []),
            ('libera_spazio_power.py', []),
            ('DiagnosiI2C/prova.py', ['--solo-lettura']),
        ]
        self.assertEqual(len(calls), len(expected))
        for args, (name, extra) in zip(calls, expected):
            self.assertEqual(args, [sys.executable, avionics.ROOT / name, '--port', 'COM12'] + extra)
            self.assertTrue((avionics.ROOT / name).is_file())

    def test_old_unavailable_environment_never_uploads(self):
        with patch('builtins.input', side_effect=['10', '0']), \
                patch.object(banco, 'execute') as execute, redirect_stdout(io.StringIO()):
            banco.main()
        execute.assert_not_called()

    def test_menu_open_and_exit_has_no_device_actions(self):
        with patch('builtins.input', return_value='0'), \
                patch.object(avionics, 'execute') as execute, redirect_stdout(io.StringIO()):
            avionics.main()
        execute.assert_not_called()


if __name__ == '__main__':
    unittest.main()
