# fethub/core/fetlink_client.py
import serial, json, threading, time
from queue import Queue
from fethub.core.tv_manager import TVManager

class FetLinkClient:
    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.port = port
        self.baud = baud
        self.manager = TVManager()
        self.running = True

        try:
            # 🚫 evita reset automático do Arduino
            self.ser = serial.Serial(
                self.port,
                self.baud,
                timeout=0.1,
                dsrdtr=False,
                rtscts=False
            )
            self.ser.dtr = False
            self.ser.rts = False
            print(f"✅ Porta {self.port} aberta (DTR desativado).")
            
            self.queue = Queue()
            if self.ser:
                threading.Thread(target=self.read_loop, daemon=True).start()

        except Exception as e:
            print(f"❌ Erro abrindo {self.port}: {e}")
            self.ser = None

        if self.ser:
            threading.Thread(target=self.read_loop, daemon=True).start()

    # ----------------------------
    # 🧠 Loop de leitura contínua
    # ----------------------------
    def read_loop(self):
        packet = b""
        in_packet = False
        while self.running:
            try:
                byte = self.ser.read(1)
                if not byte:
                    time.sleep(0.002)
                    continue

                if byte == b"\x02":  # STX
                    in_packet = True
                    packet = b""
                    print("📥 Início de pacote detectado")
                    continue
                elif byte == b"\x03":  # ETX
                    print("📤 Pacote completo recebido")
                    in_packet = False
                    line = packet.decode(errors="ignore").strip()
                    if line:
                        self.queue.put(line)
                    packet = b""
                    continue

                if in_packet:
                    packet += byte

            except serial.SerialException:
                # porta desconectada, dá um tempo e tenta de novo
                time.sleep(0.1)
            except Exception as e:
                # erros transitórios silenciosos
                time.sleep(0.01)




    def process_queue(self):
        while not self.queue.empty():
            line = self.queue.get()
            if not line:
                continue
            try:
                pkt = json.loads(line)
                dst = pkt.get("dst", "tv:OLED_SIM")
                cmd = pkt.get("cmd")
                payload = pkt.get("payload", {})
                self.manager.dispatch(dst, {"cmd": cmd, **payload})
            except json.JSONDecodeError:
                print("↩️ Raw (corrompido?):", line)

    # ----------------------------
    # 📦 Interpretação de mensagens
    # ----------------------------
    def handle_line(self, line: str):
        if not line:
            return
        if not line.startswith("{") or not line.endswith("}"):
            print("⚠️  Pacote parcial:", repr(line))
            return
        try:
            pkt = json.loads(line)
            cmd = pkt.get("cmd", "")
            payload = pkt.get("payload", {})
            dst = pkt.get("dst", "tv:OLED_SIM")

            self.manager.dispatch(dst, {"cmd": cmd, **payload})

        except json.JSONDecodeError:
            print("↩️ Raw (corrompido?):", line)

