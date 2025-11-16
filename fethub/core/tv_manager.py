# fethub/core/tv_manager.py
import threading
import time

class TVManager:
    def __init__(self):
        self.instances = {}   # inst_name -> { "type": ..., "alias": ..., "kwargs": ... }
        self.queues = {}      # inst_name -> Queue
        self.proc = {}        # inst_name -> Process
        self.alias = {}       # "tv:OLED_SIM" -> "oled#1"
        self.lock = threading.Lock()

    # -----------------------------
    # INSTANCIAMENTO
    # -----------------------------
    def create_instance(self, tv_type: str, tv_id: str, **kwargs):
        """
        Cria registro da instância de TV antes do spawn real.
        Retorna nome interno: ex: oled#1
        """
        with self.lock:
            # descobrir próximo número
            count = sum(1 for i in self.instances.values() if i["type"] == tv_type)
            inst_name = f"{tv_type}#{count+1}"

            self.instances[inst_name] = {
                "type": tv_type,
                "alias": tv_id,
                "kwargs": kwargs,
            }

            # registrar alias global
            self.alias[tv_id] = inst_name

            return inst_name

    # chamado por tv_spawn depois que o processo é criado
    def register_process(self, inst_name, queue, process):
        with self.lock:
            self.queues[inst_name] = queue
            self.proc[inst_name] = process

    # -----------------------------
    # RESOLVER ALIAS
    # -----------------------------
    def resolve(self, dst: str):
        """
        Aceita:
            - inst_name ("oled#1")
            - alias ("tv:OLED_SIM")
        Retorna inst_name normalizado ou None
        """
        if dst in self.instances:
            return dst
        if dst in self.alias:
            return self.alias[dst]
        return None

    # -----------------------------
    # DESPACHO (comando interno)
    # -----------------------------
    def dispatch(self, inst_name, subcmd, payload, pkt=None):
        """
        Envia comando para processo TV já resolvido.
        """
        with self.lock:
            if inst_name not in self.queues:
                print(f"[TV] WARN: Sem queue para {inst_name}")
                return
            q = self.queues[inst_name]

        msg = {"op": subcmd}
        msg.update(payload)
        q.put(msg)

    # -----------------------------
    # DESPACHO VIA CONSOLE
    # -----------------------------
    def dispatch_from_console(self, tv_id, subcmd, payload):
        inst = self.resolve(tv_id)
        if not inst:
            raise ValueError(f"TV '{tv_id}' não encontrada")

        self.dispatch(inst, subcmd, payload)

    # -----------------------------
    # ENCERRAMENTO
    # -----------------------------
    def stop_all(self):
        with self.lock:
            for inst_name, q in self.queues.items():
                try:
                    q.put({"op": "__quit__"})
                except:
                    pass

            # matar processos
            for inst_name, proc in self.proc.items():
                try:
                    proc.join(timeout=1)
                    if proc.is_alive():
                        proc.terminate()
                except:
                    pass
