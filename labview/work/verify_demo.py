import csv
import math
import pathlib
import subprocess
import sys

root = pathlib.Path('outputs/LabVIEW_Ultrasonic_Scanner_Demo')
sim = root / 'tools/stm32_serial_simulator.py'
result = subprocess.run([sys.executable, str(sim), '--mode', 'stdout', '--count', '5', '--interval', '20'],
                        capture_output=True, check=True)
assert result.stdout.count(b'\r\n') == 5, result.stdout
lines = result.stdout.decode('ascii').splitlines()
assert all(len(line.split(',')) == 4 and line.startswith('$SCAN,') for line in lines)
assert lines == (root / 'examples/sample_serial_log.txt').read_text(encoding='utf-8').splitlines()
with (root / 'examples/sample_scan_data.csv').open(newline='', encoding='utf-8') as file:
    rows = list(csv.DictReader(file))
assert len(rows) == 5
for row, line in zip(rows, lines):
    _, angle, distance, temperature = line.split(',')
    assert float(row['angle_deg']) == float(angle)
    assert float(row['distance_cm']) == float(distance)
    assert float(row['temperature_c']) == float(temperature)
    x = float(distance) * math.cos(math.radians(float(angle)))
    y = float(distance) * math.sin(math.radians(float(angle)))
    assert abs(float(row['x_cm']) - x) < .011
    assert abs(float(row['y_cm']) - y) < .011
print('5 CRLF frames, 5 CSV rows, and XY values verified')
