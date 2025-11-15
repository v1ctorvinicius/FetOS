# fethub/core/tickloop.py
import time
from datetime import datetime
from threading import Thread

from fethub.transport.link_manager import LinkManager
from fethub.core.tv_manager import TVManager
from fethub.core.node_manager import NodeManager
from fethub.core.packet_router import PacketRouter
from fethub.ui.console import HubConsole
from fethub.drivers.tv_registry import register_all_tv_types
from fethub.ipc.hub_socket import HubSocket
from fethub.tv.tv_spawn import spawn_tv_process


def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

class FetHubCore:
    def __init__(self, port="/dev/ttyACM0", baud=115200, verbose=True):
        print(f"[SYS][{now_ts()}] FetHub iniciado")

        self.verbose = verbose

        self.ipc = HubSocket(self)
        # Serial
        self.link = LinkManager(port=port, baud=baud, verbose=verbose)
        self.tv_manager = TVManager()
        register_all_tv_types(self.tv_manager)
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

        if op == "help":
            return "Comandos: tv.list, ui.text <tv> <x> <y> <msg>, ui.clear <tv>"

        if op == "tv.list":
            out = []
            for tv_id in self.tv_manager.proc:
                out.append(f"- {tv_id} (PID={self.tv_manager.proc[tv_id].pid})")
            return "\n".join(out)

        # delega pro router
        try:
            self.router.console_command(parts)
            return "ok"
        except Exception as e:
            return f"ERR: {e}"

        return "?"

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

