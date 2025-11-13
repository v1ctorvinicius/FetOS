# fethub/core/tv_manager.py
class TVManager:
    def __init__(self):
        self.tvs = {}

    def dispatch(self, tv_id, payload):
        """Roteia comandos pro device correto."""
        if tv_id not in self.tvs:
            print(f"⚠️  TV desconhecida: {tv_id}")
            return

        tv = self.tvs[tv_id]
        cmd = payload.get("cmd")
        tv.execute(cmd, payload)

    def tick(self):
        for tv in self.tvs.values():
            tv.update()
