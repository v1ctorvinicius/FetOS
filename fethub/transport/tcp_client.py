# fethub/transport/tcp_client.py
import socket, json, threading
from queue import Queue
from datetime import datetime

STX = 0x02
ETX = 0x03

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class TcpClientLink:
    """
    Cliente TCP → conecta ao hub.
    API igual ao SerialLink.
    """

    def __init__(self, host="127.0.0.1", port=9001, verbose=True):
        self.host = host
        self.port = port
        self.verbose = verbose
        self.queue = Queue()
        self.sock = None
        self.running = False

    def open(self):
        try:
            self.sock = socket.create_connection((self.host, self.port))
            self.running = True
            threading.Thread(target=self._rx_loop, daemon=True).start()
            print(f"[TCP][{now_ts()}] Conectado {self.host}:{self.port}")
            return True
        except Exception as e:
            print("[TCP] Erro:", e)
            return False

    def _rx_loop(self):
        buf = bytearray()
        in_frame = False

        while self.running:
            try:
                b = self.sock.recv(1)
                if not b:
                    break

                byte = b[0]

                if byte == STX:
                    buf.clear()
                    in_frame = True
                    continue

                if byte == ETX:
                    in_frame = False
                    try:
                        pkt = json.loads(buf.decode(errors="ignore"))
                        self.queue.put(pkt)
                    except:
                        print("[TCP] JSON inválido")
                    continue

                if in_frame:
                    buf.append(byte)

            except:
                break

        self.running = False
        self.sock.close()

    def send(self, pkt):
        if not self.sock:
            return False
        raw = json.dumps(pkt)
        frame = bytes([STX]) + raw.encode() + bytes([ETX])
        self.sock.sendall(frame)
        return True

    def poll(self):
        if self.queue.empty():
            return None
        return self.queue.get()
