# fethub/core/console.py
import json
try:
    import readline
except ImportError:
    readline = None


class HubConsole:
    """
    Console interativo dentro do FetHub.
    Comandos disponíveis:
      ui.text <tv> <x> <y> <texto>
      ui.clear <tv>
      ui.pixel <tv> <x> <y>
      ui.rect <tv> <x> <y> <w> <h>
      send <json>
      nodes
      help
      quit
    """

    def __init__(self, hub):
        self.hub = hub
        self.running = True
        self._setup_autocomplete()

    # -----------------------------
    # Autocomplete simples
    # -----------------------------
    def _setup_autocomplete(self):
        cmds = ["ui.text", "ui.clear", "ui.pixel", "ui.rect", "send", "nodes", "help", "quit"]

        def completer(text, state):
            options = [c for c in cmds if c.startswith(text)]
            if state < len(options):
                return options[state]
            return None

        readline.parse_and_bind("tab: complete")
        readline.set_completer(completer)

    # -----------------------------
    # Loop de entrada
    # -----------------------------
    def loop(self):
        print("> Console iniciado. Digite `help`.\n")
        while self.running:
            try:
                line = input("> ").strip()
                if not line:
                    continue
                self.handle(line)
            except KeyboardInterrupt:
                print("\n> CTRL+C no console")
                self.running = False

    # -----------------------------
    # Interpretador de comandos
    # -----------------------------
    def handle(self, line):
        parts = line.split()
        cmd = parts[0]

        if cmd == "help":
            self._help()
            return

        if cmd == "quit":
            self.running = False
            return

        if cmd == "nodes":
            self._nodes()
            return

        if cmd == "send":
            raw = line[len("send"):].strip()
            try:
                obj = json.loads(raw)
                self.hub.link.send(obj)
            except Exception as e:
                print("[ERR] JSON inválido:", e)
            return

        #
        # UI COMMANDS
        #
        if cmd.startswith("ui."):
            self._handle_ui(cmd, parts)
            return

        print("[ERR] Comando desconhecido:", cmd)

    # -----------------------------
    # UI commands
    # -----------------------------
    def _handle_ui(self, cmd, parts):
        if cmd == "ui.clear":
            if len(parts) < 2:
                print("Uso: ui.clear <tv>")
                return
            tv = parts[1]
            self.hub.link.send({"cmd": "ui.clear", "dst": tv})
            return

        if cmd == "ui.text":
            if len(parts) < 5:
                print("Uso: ui.text <tv> <x> <y> <texto>")
                return
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            text = " ".join(parts[4:])
            self.hub.link.send({"cmd": "ui.text", "dst": tv,
                                "payload": {"x": x, "y": y, "text": text}})
            return

        if cmd == "ui.pixel":
            if len(parts) < 4:
                print("Uso: ui.pixel <tv> <x> <y>")
                return
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            self.hub.link.send({"cmd": "ui.pixel", "dst": tv,
                                "payload": {"x": x, "y": y}})
            return

        if cmd == "ui.rect":
            if len(parts) < 6:
                print("Uso: ui.rect <tv> <x> <y> <w> <h>")
                return
            tv = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            w = int(parts[4])
            h = int(parts[5])
            self.hub.link.send({"cmd": "ui.rect", "dst": tv,
                                "payload": {"x": x, "y": y, "w": w, "h": h}})
            return

    # -----------------------------
    # Helpers
    # -----------------------------
    def _nodes(self):
        for node_id, node in self.hub.node_manager.nodes.items():
            print(f"- {node_id} [{node.state}] last={node.last_seen}")

    def _help(self):
        print("""
Comandos disponíveis:
  ui.text <tv> <x> <y> <texto>
  ui.clear <tv>
  ui.pixel <tv> <x> <y>
  ui.rect <tv> <x> <y> <w> <h>
  send <json>
  nodes
  help
  quit
""")
