# fethub/core/fetlink_client.py
import serial, json, threading, time
from queue import Queue
from datetime import datetime

STX = 0x02
ETX = 0x03

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

class FetLinkClient:
    def __init__(self, port="/dev/ttyACM0", baud=115200, verbose=True):
        self.port = port
        self.baud = baud
        self.verbose = verbose
        self.running = True
        self.queue = Queue()
        self.ser = None

        try:
            self.ser = serial.Serial(
                self.port,
                self.baud,
                timeout=0.1,
                dsrdtr=False,
                rtscts=False
            )
            self.ser.dtr = False
            self.ser.rts = False

            if self.verbose:
                print(f"[SYS][{now_ts()}] Port opened: {self.port} @ {self.baud}")

            threading.Thread(target=self.read_loop, daemon=True).start()

        except Exception as e:
            print(f"[ERR][{now_ts()}] Serial error: {e}")

    # ===================================================
    # RX — leitura contínua da serial, respeitando STX/ETX
    # ===================================================
    def read_loop(self):
        if not self.ser:
            return

        packet = bytearray()
        in_frame = False

        while self.running:
            try:
                b = self.ser.read(1)
                if not b:
                    time.sleep(0.001)
                    continue

                byte = b[0]

                if byte == STX:
                    in_frame = True
                    packet.clear()
                    continue

                if byte == ETX:
                    in_frame = False
                    line = packet.decode(errors="ignore").strip()

                    if self.verbose:
                        print(f"[RX][{now_ts()}] {line}")

                    self.queue.put(line)
                    packet.clear()
                    continue

                if in_frame:
                    packet.append(byte)

            except Exception:
                time.sleep(0.005)

    # ===================================================
    # TX — envio de JSON com enquadramento STX/ETX
    # ===================================================
    def send(self, obj):
        if not self.ser:
            return False

        try:
            raw = json.dumps(obj)
            framed = bytes([STX]) + raw.encode() + bytes([ETX]) + b"\n"
            self.ser.write(framed)

            if self.verbose:
                print(f"[TX][{now_ts()}] {raw}")

            return True

        except Exception as e:
            print(f"[ERR][{now_ts()}] TX error: {e}")
            return False

    # ===================================================
    # API para tickloop processar pacotes pendentes
    # ===================================================
    def poll(self):
        """Retorna 1 pacote se tiver, senão None."""
        if self.queue.empty():
            return None
        try:
            return json.loads(self.queue.get())
        except:
            return None
