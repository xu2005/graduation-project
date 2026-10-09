"""STM32 ultrasonic scanner protocol simulator.

No third-party package is needed for stdout mode. Serial mode needs pyserial
and a real or paired virtual COM port.
"""

from __future__ import annotations

import argparse
import math
import sys
import time
from dataclasses import dataclass


@dataclass
class Settings:
    start: int = 30
    end: int = 150
    step: int = 2
    interval_ms: int = 100

    def valid(self) -> bool:
        return (0 <= self.start < self.end <= 180
                and 1 <= self.step <= 20
                and 20 <= self.interval_ms <= 2000)


class Scanner:
    def __init__(self, settings: Settings | None = None, running: bool = True):
        self.settings = settings or Settings()
        self.running = running
        self.angle = self.settings.start
        self.direction = 1
        self.sample = 0

    def command(self, raw: str) -> str | None:
        line = raw.strip()
        if line == "$PING":
            return "$PONG\r\n"
        if line == "$START":
            self.running = True
            return None
        if line == "$STOP":
            self.running = False
            return None
        parts = line.split(",")
        if len(parts) == 5 and parts[0] == "$SET":
            try:
                values = [int(p) for p in parts[1:]]
            except ValueError:
                return None
            candidate = Settings(*values)
            if candidate.valid():
                self.settings = candidate
                self.angle = candidate.start
                self.direction = 1
            return None
        return None

    def next_frame(self) -> str | None:
        if not self.running:
            return None
        angle = self.angle
        radians = math.radians(angle)
        # Smooth background plus a narrower, nearer obstacle around 82 degrees.
        background = 145 + 18 * math.sin(3 * radians)
        obstacle = 63 * math.exp(-((angle - 82) / 15) ** 2)
        ripple = 1.2 * math.sin(self.sample * 0.37)
        distance = max(2.0, background - obstacle + ripple)
        temperature = 26.0 + 0.3 * math.sin(self.sample * 0.04)
        frame = f"$SCAN,{angle:.1f},{distance:.1f},{temperature:.1f}\r\n"
        self.sample += 1
        if self.direction > 0:
            if angle >= self.settings.end:
                self.direction = -1
                self.angle = max(self.settings.start, angle - self.settings.step)
            else:
                self.angle = min(self.settings.end, angle + self.settings.step)
        elif angle <= self.settings.start:
            self.direction = 1
            self.angle = min(self.settings.end, angle + self.settings.step)
        else:
            self.angle = max(self.settings.start, angle - self.settings.step)
        return frame


def run_stdout(scanner: Scanner, count: int) -> None:
    sent = 0
    try:
        while count == 0 or sent < count:
            frame = scanner.next_frame()
            if frame:
                sys.stdout.buffer.write(frame.encode("ascii"))
                sys.stdout.buffer.flush()
                sent += 1
            time.sleep(scanner.settings.interval_ms / 1000)
    except KeyboardInterrupt:
        pass


def run_serial(scanner: Scanner, port: str, baud: int) -> None:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("串口模式需要 pyserial：py -m pip install -r tools/requirements.txt") from exc
    try:
        connection = serial.Serial(port, baudrate=baud, bytesize=8,
                                   parity="N", stopbits=1, timeout=0)
    except serial.SerialException as exc:
        raise SystemExit(f"无法打开 {port}: {exc}") from exc
    print(f"模拟器已打开 {port} @ {baud}。等待 $START；Ctrl+C 退出。", file=sys.stderr)
    rx = bytearray()
    next_send = time.monotonic()
    try:
        while True:
            incoming = connection.read(min(connection.in_waiting, 4096) or 1)
            for byte in incoming:
                if byte == 10:
                    if len(rx) <= 128:
                        answer = scanner.command(rx.decode("ascii", errors="replace"))
                        if answer:
                            connection.write(answer.encode("ascii"))
                    rx.clear()
                elif len(rx) < 129:
                    rx.append(byte)
                else:
                    # Resynchronize on LF after a long or corrupt command.
                    pass
            now = time.monotonic()
            if scanner.running and now >= next_send:
                frame = scanner.next_frame()
                if frame:
                    connection.write(frame.encode("ascii"))
                next_send = now + scanner.settings.interval_ms / 1000
            elif not scanner.running:
                next_send = now
            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="STM32 扫描串口模拟器")
    parser.add_argument("--mode", choices=("stdout", "serial"), default="stdout")
    parser.add_argument("--port", help="串口模式下 Python 使用的端口，例如 COM8")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--count", type=int, default=0, help="stdout 模式输出帧数；0 表示持续输出")
    parser.add_argument("--start", type=int, default=30)
    parser.add_argument("--end", type=int, default=150)
    parser.add_argument("--step", type=int, default=2)
    parser.add_argument("--interval", type=int, default=100, help="毫秒")
    args = parser.parse_args()
    settings = Settings(args.start, args.end, args.step, args.interval)
    if not settings.valid() or args.baud <= 0 or args.count < 0:
        parser.error("参数范围：0≤start<end≤180，step 1–20，interval 20–2000，baud>0，count≥0")
    scanner = Scanner(settings, running=args.mode == "stdout")
    if args.mode == "serial":
        if not args.port:
            parser.error("串口模式必须提供 --port")
        run_serial(scanner, args.port, args.baud)
    else:
        run_stdout(scanner, args.count)


if __name__ == "__main__":
    main()
