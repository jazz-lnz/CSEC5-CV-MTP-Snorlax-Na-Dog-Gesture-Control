"""
Laptop -> Arduino link (src/robot/controller.py).

Keyboard-drive the robot to test the wiring and the link:
    python src/robot/controller.py                              (lists ports)
    python src/robot/controller.py --port /dev/cu.usbmodem1101  (Mac)
    python src/robot/controller.py --port COM3                  (Windows)

Keys:
    W forward   S backward   A left   D right   SPACE stop
    0-9 speed   Q quit (robot stops)

Hold a key = keeps driving (the key repeat refreshes the command).
Release it = robot stops by itself after ~0.3 s.

The same RobotLink class is used later by the gesture and line-follow programs:
    link = RobotLink("COM3"); link.start(); link.set("F"); ... link.close()

USB cable now, Bluetooth later: only the --port value changes
(the HC-05 shows up as a new COM port after pairing).

Requires: pip install pyserial
"""
import argparse
import threading
import time

import serial
import serial.tools.list_ports

SEND_HZ = 10          # commands per second (keeps the Arduino watchdog fed)
VALID = set("FBLRS")


class RobotLink:
    def __init__(self, port, baud=9600):
        self.ser = serial.Serial(port, baud, timeout=0.05)
        time.sleep(2.0)                 # Uno resets when the USB port opens
        self.ser.reset_input_buffer()
        self.cmd = "S"
        self.running = False
        self.lock = threading.Lock()

    def set(self, cmd):
        """Set the command the robot should be doing right now (F/B/L/R/S)."""
        if cmd in VALID:
            with self.lock:
                self.cmd = cmd

    def speed(self, level):
        """0..9"""
        self.ser.write(str(int(level))[0].encode())

    def start(self):
        self.running = True
        threading.Thread(target=self._loop, daemon=True).start()
        threading.Thread(target=self._reader, daemon=True).start()

    def _loop(self):
        while self.running:
            with self.lock:
                c = self.cmd
            # 'S' is also sent repeatedly; harmless and keeps things simple
            self.ser.write(c.encode())
            time.sleep(1.0 / SEND_HZ)

    def _reader(self):
        while self.running:
            try:
                line = self.ser.readline().decode(errors="ignore").strip()
            except serial.SerialException:
                break
            if line:
                print(f"  [arduino] {line}")

    def close(self):
        self.set("S")
        time.sleep(0.15)
        self.running = False
        try:
            self.ser.write(b"S")
            self.ser.close()
        except Exception:
            pass


def list_ports():
    print("Available ports:")
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device:32s} {p.description}")
    print("Mac: use the one starting with /dev/cu.usbmodem or /dev/cu.usbserial")


class _Keys:
    """Read single key presses without Enter, on Windows and on macOS/Linux."""

    def __enter__(self):
        try:
            import msvcrt
            self.win = msvcrt
        except ImportError:
            import termios, tty, sys
            self.win = None
            self.fd = sys.stdin.fileno()
            self.old = termios.tcgetattr(self.fd)
            tty.setcbreak(self.fd)
        return self

    def get(self):
        """Return a pressed key, or None if no key is waiting."""
        if self.win:
            return self.win.getwch() if self.win.kbhit() else None
        import select, sys
        if select.select([sys.stdin], [], [], 0)[0]:
            return sys.stdin.read(1)
        return None

    def __exit__(self, *a):
        if not self.win:
            import termios
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old)


def keyboard_drive(link):
    keymap = {"w": "F", "s": "B", "a": "L", "d": "R", " ": "S"}
    print(__doc__.split("Keys")[1].split("The same")[0])

    FIRST_GAP = 0.8        # seconds allowed between the first press and the first repeat
    REPEAT_GAP = 0.25      # seconds allowed between repeats before we decide the key was released
    last_key, held_key, repeating = 0.0, None, False
    link.start()
    
    try:
      with _Keys() as keys:
        while True:
            k = keys.get()
            now = time.time()
            if k:
                k = k.lower()
                if k == "q":
                    break
                if k.isdigit():
                    link.speed(k)
                elif k in keymap:
                    repeating = (k == held_key and now - last_key < FIRST_GAP)
                    held_key, last_key = k, now
                    link.set(keymap[k])
            # key released -> stop
            gap = REPEAT_GAP if repeating else FIRST_GAP
            if link.cmd != "S" and now - last_key > gap:
                link.set("S")
                held_key, repeating = None, False
            time.sleep(0.01)
    finally:
        link.close()
        print("Stopped.")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="Windows: COM3   Mac: /dev/cu.usbmodem1101 (run without it to list ports)")
    args = ap.parse_args()
    if not args.port:
        list_ports()
    else:
        keyboard_drive(RobotLink(args.port))