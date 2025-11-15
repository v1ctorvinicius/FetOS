# fethub/transport/tcp_server.py
import socket, threading, json
from queue import Queue
from datetime import datetime

STX = 0x02
ETX = 0x03

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class TcpServerLink:
    """
    Cada conexão TCP cria um link isolado.
    Frame igual ao serial (STX/ETX + JSON).
    Servidor simples para testes / simulação.
    """

    def __init__(self, conn, addr, verbose=True):
        self.conn = conn
        self.addr = addr
        self.verbose = verbose
        self.queue = Queue()
        self.running = True

        threading.Thread(target=self._rx_loop, daemon=True).start()

    def _rx_loop(self):
        pkt = bytearray()
        in_frame = False

        while self.running:
            try:
                b = self.conn.recv(1)
                if not b:
                    break

                byte = b[0]

                if byte == STX:
                    pkt.clear()
                    in_frame = True
                    continue

                if byte == ETX:
                    in_frame = False
                    try:
                        obj = json.loads(pkt.decode(errors="ignore"))
                        if self.verbose:
                            print(f"[TCP RX] {obj}")
                        self.queue.put(obj)
                    except:
                        print("[TCP RX] JSON inválido")
                    continue

                if in_frame:
                    pkt.append(byte)

            except:
                break

        self.running = False
        self.conn.close()

    def send(self, obj):
        raw = json.dumps(obj)
        frame = bytes([STX]) + raw.encode() + bytes([ETX])
        try:
            self.conn.sendall(frame)
            if self.verbose:
                print(f"[TCP TX] {obj}")
        except:
            pass

    def poll(self):
        if self.queue.empty():
            return None
        return self.queue.get()
