# fethub/core/packet_router.py

from datetime import datetime

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class PacketRouter:
    """
    Roteia sys.* e ui.* vindo dos nodes.
    """
    def __init__(self, tv_manager, node_manager):
        self.tv = tv_manager
        self.nodes = node_manager

    # ---------------------------------------------------------
    def route(self, pkt):
        cmd = pkt.get("cmd")
        if not cmd:
            print("[ROUTER] Pacote sem cmd")
            return

        if cmd.startswith("sys."):
            self._route_sys(cmd, pkt)
        elif cmd.startswith("ui."):
            self._route_ui(cmd, pkt)
        else:
            print(f"[ROUTER][{now_ts()}] comando desconhecido '{cmd}'")

    # ---------------------------------------------------------
    def _route_sys(self, cmd, pkt):
        if cmd == "sys.hello":
            self.nodes.handle_hello(pkt)
        else:
            print(f"[ROUTER] sys desconhecido: {cmd}")

    def _route_ui(self, subcmd, pkt):
        dst = pkt.get("dst")
        payload = pkt.get("payload", {})
        self.tv_manager.dispatch(dst, subcmd, payload, pkt)

    def console_command(self, parts):
        op = parts[0]

        if op == "ui.text":
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            msg = " ".join(parts[4:]).strip('"')
            self.tv_manager.dispatch(tv, "text", {"x": x, "y": y, "text": msg}, {})

        elif op == "ui.clear":
            self.tv_manager.dispatch(parts[1], "clear", {}, {})

        elif op == "ui.pixel":
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            self.tv_manager.dispatch(tv, "pixel", {"x": x, "y": y}, {})

        elif op == "ui.rect":
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            w = int(parts[4])
            h = int(parts[5])
            self.tv_manager.dispatch(tv, "rect", {"x": x, "y": y, "w": w, "h": h}, {})

        else:
            raise Exception(f"Comando desconhecido: {op}")
