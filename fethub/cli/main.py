import sys
from fethub.core.tickloop import FetHubCore
from fethub.console_client import main as console_main

def main():
    # fethub console → abre console
    if len(sys.argv) > 1:
        if sys.argv[1] == "console":
            return console_main()
        if sys.argv[1] == "run":
            hub = FetHubCore()
            return hub.run()

    # fethub → inicia o Hub
    hub = FetHubCore()
    return hub.run()
