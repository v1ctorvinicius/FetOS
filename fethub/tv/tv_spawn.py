# fethub/tv/tv_spawn.py

from multiprocessing import Process, Queue
from fethub.tv.tv_process import tv_process_main

def spawn_tv_process(tv_manager, tv_type, tv_id, **kwargs):
    inst_name = tv_manager.create_instance(tv_type, tv_id, **kwargs)

    q = Queue()
    p = Process(
        target=tv_process_main,
        args=(tv_type, tv_id, kwargs, q),
        daemon=False,
    )
    p.start()

    tv_manager.register_process(inst_name, q, p)
    print(f"[TVPROC] Processo iniciado para {tv_type}:{tv_id} (pid={p.pid})")
