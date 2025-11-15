# fethub/tv/tv_spawn.py
import multiprocessing
from multiprocessing import Queue
from fethub.tv.tv_process import tv_process_main


def spawn_tv_process(tv_manager, tv_type: str, tv_id: str, **kwargs):

    inst_name = tv_manager.create_instance(tv_type, tv_id, **kwargs)
    alias = f"tv:{tv_id}"

    queue = Queue()

    proc = multiprocessing.Process(
        target=tv_process_main,
        args=(tv_type, alias, kwargs, queue),
        daemon=False   # ❗ NUNCA DEIXAR DAEMON AQUI
    )

    proc.start()

    tv_manager.register_process(inst_name, proc, queue)

    print(f"[TVPROC] Processo iniciado para {tv_type}:{tv_id} (pid={proc.pid})")
    return inst_name
