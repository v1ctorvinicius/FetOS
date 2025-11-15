# fethub/transport/serial_link.py
import serial, threading, time, json
from queue import Queue
from datetime import datetime

STX = 0x02
ETX = 0x03

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class SerialLink:
    """
    Low-level serial framing (STX/ETX + JSON).
    Não sabe nada sobre TVs, nodes ou UI.
    Só empacota e desempacota.
    """

    def __init__(self, port="/dev/ttyACM0", baud=115200, verbose=True):
        self.port = port
        self.baud = baud
        self.verbose = verbose

        self.ser = None
        self.running = False
        self.queue = Queue()

    # ==========================================================
    # OPEN
    # ==========================================================
    def open(self):
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

            self.running = True
            threading.Thread(target=self._rx_loop, daemon=True).start()

            if self.verbose:
                print(f"[SYS][{now_ts()}] SerialLink aberto {self.port}@{self.baud}")

            return True

        except Exception as e:
            print(f"[ERR][{now_ts()}] Serial error: {e}")
            return False

    # ==========================================================
    # RX LOOP (STX/ETX)
    # ==========================================================
    def _rx_loop(self):
        packet = bytearray()
        in_frame = False

        while self.running:
            try:
                b = self.ser.read(1)
                if not b:
                    time.sleep(0.0005)
                    continue

                byte = b[0]

                if byte == STX:
                    in_frame = True
                    packet.clear()
                    continue

                if byte == ETX:
                    in_frame = False
                    try:
                        line = packet.decode(errors="ignore").strip()
                        pkt = json.loads(line)

                        if self.verbose:
                            print(f"[RX][{now_ts()}] {pkt}")

                        self.queue.put(pkt)

                    except json.JSONDecodeError:
                        print(f"[RX][{now_ts()}] JSON inválido:", packet)

                    packet.clear()
                    continue

                if in_frame:
                    packet.append(byte)

            except:
                time.sleep(0.001)

    # ==========================================================
    # TX (STX/ETX)
    # ==========================================================
    def send(self, pkt: dict):
        if not self.ser:
            return False

        raw = json.dumps(pkt)
        frame = bytes([STX]) + raw.encode() + bytes([ETX]) + b"\n"

        try:
            self.ser.write(frame)
            if self.verbose:
                print(f"[TX][{now_ts()}] {pkt}")
            return True

        except Exception as e:
            print(f"[ERR][{now_ts()}] TX error: {e}")
            return False

    # ==========================================================
    # Poll
    # ==========================================================
    def poll(self):
        if self.queue.empty():
            return None
        return self.queue.get()
