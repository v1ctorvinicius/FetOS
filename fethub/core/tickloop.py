# fethub/core/tickloop.py
import time
from datetime import datetime
from threading import Thread

from fethub.transport.link_manager import LinkManager
from fethub.core.tv_manager import TVManager
from fethub.core.node_manager import NodeManager
from fethub.core.packet_router import PacketRouter
from fethub.ui.console import HubConsole
from fethub.ipc.hub_socket import HubSocket
from fethub.tv.tv_spawn import spawn_tv_process
import os
from serial.tools import list_ports

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

class FetHubCore:
    def __init__(self, port=None, baud=115200, verbose=True):
        if port is None:
            if os.name == "nt":
                ports = [p.device for p in list_ports.comports()]
                if len(ports) == 0:
                    print("[WARN] Nenhuma porta COM encontrada. Usando COM3 por fallback.")
                    port = "COM3"
                else:
                    print(f"[SYS] Portas encontradas: {ports}")
                    port = ports[0]
            else:
                port = "/dev/ttyACM0"

        print(f"[SYS][{now_ts()}] FetHub iniciado")

        self.verbose = verbose

        self.ipc = HubSocket(self)
        # Serial
        self.link = LinkManager(port=port, baud=baud, verbose=verbose)
        self.tv_manager = TVManager()
        self.node_manager = NodeManager(self.link, verbose=verbose)
        self.router = PacketRouter(
            tv_manager=self.tv_manager,
            node_manager=self.node_manager
        )
        self.running = True

    # chamado pelo socket IPC
    def console_exec(self, cmd: str):
        parts = cmd.split()
        if not parts:
            return ""

        op = parts[0]

        # -----------------------------------------
        # HELP
        # -----------------------------------------
        if op == "help":
            return (
                "Comandos disponíveis:\n"
                "  tv.list\n"
                "  ui.text <tv> <x> <y> <msg>\n"
                "  ui.clear <tv>\n"
            )

        # -----------------------------------------
        # LISTAR TVs
        # -----------------------------------------
        if op == "tv.list":
            out = []
            for tv_id, proc in self.tv_manager.proc.items():
                out.append(f"- {tv_id} (PID={proc.pid})")
            return "\n".join(out)

        # -----------------------------------------
        # ui.clear <tv_id>
        # -----------------------------------------
        if op == "ui.clear":
            if len(parts) < 2:
                return "ERR: uso: ui.clear <tv>"
            tv = parts[1]
            self.tv_manager.dispatch_from_console(tv, "clear", {})
            return "ok"

        # -----------------------------------------
        # ui.text <tv_id> <x> <y> <msg...>
        # -----------------------------------------
        if op == "ui.text":
            if len(parts) < 5:
                return "ERR: uso: ui.text <tv> <x> <y> <msg>"

            tv = parts[1]
            try:
                x = int(parts[2])
                y = int(parts[3])
            except:
                return "ERR: x e y devem ser inteiros"

            msg = " ".join(parts[4:])

            self.tv_manager.dispatch_from_console(tv, "text", {"x": x, "y": y, "msg": msg})

            return "ok"

        return f"ERR: Comando desconhecido: {op}"


    def run(self):
        print(f"[SYS][{now_ts()}] Loop principal iniciado")
        self.ipc.start()

        # Criar TVs padrão em processos
        spawn_tv_process(self.tv_manager, "oled", "OLED_SIM", width=128, height=64, zoom=4, title="OLED 128x64")
        spawn_tv_process(self.tv_manager, "lcd", "LCD_SIM", cols=16, lines=2, zoom=4, title="LCD 16x2")


        try:
            while self.running:

                pkt = self.link.poll()
                if pkt:
                    self.router.route(pkt)

                time.sleep(0.002)

        except KeyboardInterrupt:
            print("\n[SYS] CTRL+C recebido")

        finally:
            print("[SYS] Encerrando...")
            self.running = False
            self.ipc.stop()
            self.link.running = False
            self.tv_manager.stop_all()
            time.sleep(0.1)
            print("[SYS] Finalizado ✔")

