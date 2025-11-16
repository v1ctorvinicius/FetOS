# fethub/console_client.py
import socket
import sys
import os

HOST = "127.0.0.1"
PORT = 5678
UNIX_PATH = "/tmp/fethub.sock"

def main():
    # Detecta se AF_UNIX existe (Linux/macOS)
    use_unix = hasattr(socket, "AF_UNIX") and os.name != "nt"

    if use_unix:
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(UNIX_PATH)
        except Exception as e:
            print(f"[WARN] Não foi possível conectar via UNIX socket: {e}")
            print("[INFO] Tentando TCP fallback...")
            use_unix = False

    if not use_unix:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((HOST, PORT))
        except Exception as e:
            print(f"Não foi possível conectar ao FetHub: {e}")
            sys.exit(1)

    print("Console conectado. Digite comandos (CTRL+C para sair)")
    try:
        while True:
            cmd = input("> ").strip()
            if not cmd:
                continue
            sock.send((cmd + "\n").encode())
            resp = sock.recv(8192).decode()
            print(resp.rstrip())
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()

if __name__ == "__main__":
    main()
