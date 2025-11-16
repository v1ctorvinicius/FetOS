import socket
import sys
import threading
import os

# Config para TCP fallback
HOST = "127.0.0.1"
PORT = 5678


class HubSocket:
    def __init__(self, hub):
        self.hub = hub
        self.running = True
        self.server = None
        self.thread = None
        self.unix_path = "/tmp/fethub.sock"

        # Detecta sistema operacional
        self.use_unix = hasattr(socket, "AF_UNIX") and os.name != "nt"

    def start(self):
        if self.use_unix:
            # --- Linux/macOS: AF_UNIX ---
            try:
                if os.path.exists(self.unix_path):
                    os.remove(self.unix_path)

                self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.server.bind(self.unix_path)
                self.server.listen(1)
                print(f"[IPC] UNIX socket ativo em {self.unix_path}")

            except Exception as e:
                print(f"[IPC][WARN] Falhou abrir UNIX socket, caindo para TCP: {e}")
                self.use_unix = False

        if not self.use_unix:
            # --- Windows: TCP ---
            self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server.bind((HOST, PORT))
            self.server.listen(1)
            print(f"[IPC] TCP socket ativo em {HOST}:{PORT}")

        self.thread = threading.Thread(target=self._accept_loop, daemon=True)
        self.thread.start()

    def _accept_loop(self):
        while self.running:
            try:
                conn, _ = self.server.accept()
            except OSError:
                break
            threading.Thread(target=self._handle_client, args=(conn,), daemon=True).start()

    def _handle_client(self, conn):
        try:
            buf = b""
            while True:
                data = conn.recv(4096)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    cmd = line.decode().strip()
                    if not cmd:
                        continue
                    resp = self.hub.console_exec(cmd)
                    conn.send((resp + "\n").encode())
        finally:
            conn.close()

    def stop(self):
        self.running = False
        if self.server:
            self.server.close()

        if self.use_unix and os.path.exists(self.unix_path):
            os.remove(self.unix_path)

        print("[IPC] Servidor IPC parado")
