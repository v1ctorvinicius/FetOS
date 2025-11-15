# fethub/console/repl.py

import readline
import json
from datetime import datetime
from fethub.console.interpreter import HubInterpreter

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class HubConsole:
    """
    Console interativo do FetHub.
    Integra NodeManager, TVManager e o link de transporte.
    """

    def __init__(self, hub):
        self.hub = hub
        self.interp = HubInterpreter(hub)
        self.running = True

        print("> Console iniciado. Digite `help`.")

    # =====================================================================
    def loop(self):
        """
        Loop principal do console.
        """
        while self.running:
            try:
                line = input("> ").strip()
                if not line:
                    continue

                self.interp.execute(line)

            except KeyboardInterrupt:
                print("\n[CONSOLE] CTRL+C — use `exit` pra sair.")
            except EOFError:
                print("\n[CONSOLE] EOF — encerrando.")
                self.running = False
            except Exception as e:
                print(f"[CONSOLE][ERRO] {e}")
