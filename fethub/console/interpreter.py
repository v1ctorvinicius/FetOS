# fethub/console/interpreter.py

import json
from fethub.console.syntax import parse_tokens


class HubInterpreter:
    """
    Processa comandos digitados no console:
      - ui.text oled1 10 20 "Hello"
      - ui.clear oled1
      - sys.ping fetos:uno
      - tvs
      - nodes
      - send {json}
    """

    def __init__(self, hub):
        self.hub = hub  # FetHubCore
        self.link = hub.link
        self.tv = hub.tv_manager
        self.nodes = hub.node_manager

    # =====================================================================
    def execute(self, line):
        tokens = parse_tokens(line)

        if not tokens:
            return

        cmd = tokens[0]

        # -----------------------------------------------------------------
        # HELP
        # -----------------------------------------------------------------
        if cmd == "help":
            self._cmd_help()
            return

        if cmd == "exit":
            self.hub.running = False
            return

        # -----------------------------------------------------------------
        # LISTAGENS
        # -----------------------------------------------------------------
        if cmd == "tvs":
            self._cmd_tvs()
            return

        if cmd == "nodes":
            self._cmd_nodes()
            return

        # -----------------------------------------------------------------
        # ENVIAR JSON RAW
        # -----------------------------------------------------------------
        if cmd == "send":
            self._cmd_send(tokens[1:])
            return

        # -----------------------------------------------------------------
        # SYS.*
        # -----------------------------------------------------------------
        if cmd.startswith("sys."):
            self._cmd_sys(tokens)
            return

        # -----------------------------------------------------------------
        # UI.*
        # -----------------------------------------------------------------
        if cmd.startswith("ui."):
            self._cmd_ui(tokens)
            return

        print(f"[CONSOLE] Comando desconhecido: {cmd}")

    # =====================================================================
    # HELP
    # =====================================================================
    def _cmd_help(self):
        print("""
==== COMANDOS DISPONÍVEIS =====

UI:
  ui.text <tv> <x> <y> "<texto>"
  ui.clear <tv>
  ui.pixel <tv> <x> <y>
  ui.rect  <tv> <x> <y> <w> <h> [fill=0|1]

SYS:
  sys.ping <node_id>

GERAL:
  nodes           - lista nodes
  tvs             - lista TVs
  send {json}     - envia pacote cru
  exit            - encerra hub
""")

    # =====================================================================
    # LISTAR TVs
    # =====================================================================
    def _cmd_tvs(self):
        print("=== TVs registradas ===")
        for tv_id, tv_obj in self.tv.tvs.items():
            print(f" - {tv_id} ({tv_obj.__class__.__name__})")

    # =====================================================================
    # LISTAR NODES
    # =====================================================================
    def _cmd_nodes(self):
        print("=== Nodes FetOS ===")
        for nid, node in self.nodes.nodes.items():
            print(f" - {nid}  [{node.state}]")

    # =====================================================================
    # ENVIAR JSON RAW
    # =====================================================================
    def _cmd_send(self, args):
        try:
            data = json.loads(" ".join(args))
            self.link.send(data)
            print("[CONSOLE] enviado ✔")
        except Exception as e:
            print(f"[ERRO] JSON inválido: {e}")

    # =====================================================================
    # SYS.*
    # =====================================================================
    def _cmd_sys(self, tokens):
        cmd = tokens[0]  # ex: sys.ping
        if cmd == "sys.ping":
            if len(tokens) < 2:
                print("uso: sys.ping <node>")
                return
            self.nodes.ping(tokens[1])
            return

        print(f"[CONSOLE] sys.* desconhecido: {cmd}")

    # =====================================================================
    # UI.*
    # =====================================================================
    def _cmd_ui(self, tokens):
        cmd = tokens[0][3:]  # remove "ui."

        if cmd == "clear":
            tv_id = tokens[1]
            self.link.send({"cmd": "ui.clear", "dst": tv_id})
            return

        if cmd == "text":
            tv_id = tokens[1]
            x = int(tokens[2])
            y = int(tokens[3])
            text = tokens[4]
            text = text.strip('"')
            self.link.send({
                "cmd": "ui.text",
                "dst": tv_id,
                "payload": {"x": x, "y": y, "text": text}
            })
            return

        if cmd == "pixel":
            tv_id = tokens[1]
            x = int(tokens[2])
            y = int(tokens[3])
            self.link.send({
                "cmd": "ui.pixel",
                "dst": tv_id,
                "payload": {"x": x, "y": y}
            })
            return

        if cmd == "rect":
            tv_id = tokens[1]
            x = int(tokens[2])
            y = int(tokens[3])
            w = int(tokens[4])
            h = int(tokens[5])
            fill = 1
            for arg in tokens[6:]:
                if arg.startswith("fill="):
                    fill = int(arg.split("=")[1])
            self.link.send({
                "cmd": "ui.rect",
                "dst": tv_id,
                "payload": {"x": x, "y": y, "w": w, "h": h, "fill": fill}
            })
            return

        print(f"[CONSOLE] ui.* desconhecido: {cmd}")
