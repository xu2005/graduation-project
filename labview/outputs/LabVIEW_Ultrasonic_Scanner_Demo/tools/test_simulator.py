"""Basic protocol and scan tests; run with `python -m unittest tools/test_simulator.py`."""

import unittest

from stm32_serial_simulator import Scanner, Settings


class SimulatorTests(unittest.TestCase):
    def test_default_frame_and_turnaround(self):
        scanner = Scanner()
        angles = [float(scanner.next_frame().split(",")[1]) for _ in range(123)]
        self.assertEqual(angles[0], 30)
        self.assertIn(150, angles)
        self.assertEqual(angles[61], 148)
        self.assertEqual(angles[120], 30)
        self.assertEqual(angles[121], 32)

    def test_commands_and_reconfigure(self):
        scanner = Scanner(running=False)
        self.assertIsNone(scanner.next_frame())
        self.assertEqual(scanner.command("$PING\r\n"), "$PONG\r\n")
        scanner.command("$SET,30,150,7,20\r\n")
        self.assertEqual(scanner.settings, Settings(30, 150, 7, 20))
        scanner.command("$START\r\n")
        angles = [float(scanner.next_frame().split(",")[1]) for _ in range(20)]
        self.assertEqual(angles[-3:], [149, 150, 143])
        scanner.command("$STOP\r\n")
        self.assertIsNone(scanner.next_frame())

    def test_invalid_set_does_not_mutate(self):
        scanner = Scanner()
        original = scanner.settings
        for line in ("$SET,150,30,2,100", "$SET,x,150,2,100", "$SET,30,150,0,100",
                     "$SET,30,150,2,5", "$SET,30,150,2,100,extra"):
            scanner.command(line)
            self.assertEqual(scanner.settings, original)


if __name__ == "__main__":
    unittest.main()
