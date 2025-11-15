# fethub/ipc/hub_socket.py
import os
import socket
import threading

SOCK_PATH = "/tmp/fethub.sock"

class HubSocket:
    def __init__(self, core):
        self.core = core
        self.server = None
        self.running = False

    def start(self):
        try: os.unlink(SOCK_PATH)
        except FileNotFoundError: pass

        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(SOCK_PATH)
        self.server.listen(5)

        self.running = True

        t = threading.Thread(target=self._accept_loop, daemon=True)
        t.start()

        print("[IPC] Servidor IPC ativo em", SOCK_PATH)

    def _accept_loop(self):
        while self.running:
            try:
                conn, _ = self.server.accept()
                threading.Thread(
                    target=self._client_thread, args=(conn,), daemon=True
                ).start()
            except Exception:
                break

    def _client_thread(self, conn):
        try:
            while True:
                data = conn.recv(1024)
                if not data:
                    return

                cmd = data.decode().rstrip("\r\n")

                resp = self.core.console_exec(cmd)

                # resposta ao cliente
                conn.sendall((resp + "\n").encode())

        except Exception as e:
            print("[IPC] erro:", e)

        finally:
            conn.close()

    def stop(self):
        self.running = False
        try: self.server.close()
        except: pass
        try: os.unlink(SOCK_PATH)
        except: pass
        print("[IPC] Servidor IPC parado")
