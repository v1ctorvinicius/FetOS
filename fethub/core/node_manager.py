# fethub/core/node_manager.py
from datetime import datetime

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class NodeManager:
    def __init__(self, link, verbose=True):
        self.link = link
        self.verbose = verbose
        self.nodes = {}  # "fetos:uno" → { ver, last_seen }

    # ---------------------------------------------------------
    def handle_hello(self, pkt):
        src = pkt.get("src")
        payload = pkt.get("payload", {})
        ver = payload.get("ver", "unknown")

        self.nodes[src] = {
            "ver": ver,
            "last_seen": now_ts()
        }

        if self.verbose:
            print(f"[NODE][{now_ts()}] HELLO from {src} (ver={ver})")

        # Enviar READY de volta
        self.link.send({
            "cmd": "READY",
            "dst": src,
            "payload": {"hub_ver": "1.0.0"}
        })

    # ---------------------------------------------------------
    def list_nodes(self):
        return self.nodes
