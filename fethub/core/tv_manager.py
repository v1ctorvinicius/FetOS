from datetime import datetime
import os

def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


class TVManager:
    def __init__(self, verbose=True):
        self.verbose = verbose

        # tipos → ctor
        self.factories = {}

        # contadores
        self.count = {}

        # metadados para subprocessos
        self.kwargs = {}          # inst_name → kwargs usados na criação real

        # processos
        self.proc = {}
        self.queues = {}

        # alias usados pelo router
        self.alias = {}

    # -------------------------------------------------------------
    def register_type(self, tv_type, ctor):
        self.factories[tv_type] = ctor
        self.count[tv_type] = 0
        if self.verbose:
            print(f"[TV] Tipo registrado: {tv_type}")

    # -------------------------------------------------------------
    def create_instance(self, tv_type, tv_id, **kwargs):
        if tv_type not in self.factories:
            raise KeyError(f"TV type '{tv_type}' não registrado")

        self.count[tv_type] += 1
        inst_name = f"{tv_type}#{self.count[tv_type]}"

        # salva kwargs pro processo criar a TV real
        self.kwargs[inst_name] = kwargs

        # registra alias
        self.alias[f"tv:{tv_id}"] = inst_name

        if self.verbose:
            print(f"[TV][{now_ts()}] Instância registrada: {inst_name} (alias tv:{tv_id})")

        return inst_name

    # -------------------------------------------------------------
    def register_process(self, inst_name, process, queue):
        self.proc[inst_name] = process
        self.queues[inst_name] = queue
        if self.verbose:
            print(f"[TV][{now_ts()}] Processo registrado para {inst_name}")

    # -------------------------------------------------------------
    def resolve(self, dst_alias):
        return self.alias.get(dst_alias)

    # -------------------------------------------------------------
    def dispatch(self, dst_alias, subcmd, payload, pkt):
        inst_name = self.resolve(dst_alias)
        if not inst_name:
            print(f"[TV] Alias desconhecido '{dst_alias}'")
            return

        q = self.queues.get(inst_name)
        if not q:
            print(f"[TV] Sem queue para {inst_name}")
            return

        q.put({
            "op": subcmd,
            **payload
        })

    def stop_all(self):
        for inst, proc in self.proc.items():
            q = self.queues.get(inst)
            if q:
                q.put({"op": "__quit__"})  # notifica processo

            if proc.is_alive():
                proc.join(timeout=1)

            if proc.is_alive():
                proc.terminate()

