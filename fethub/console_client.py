# fethub/console_client.py
import socket
import sys

SOCK_PATH = "/tmp/fethub.sock"

def main():
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(SOCK_PATH)
    except FileNotFoundError:
        print("Hub não está rodando. Execute: fethub.")
        sys.exit(1)

    print("Console conectado. Digite comandos (CTRL+C para sair)")

    try:
        while True:
            cmd = input("> ")

            if cmd.strip() in ("exit", "quit"):
                break

            # ENVIA APENAS LF
            sock.sendall((cmd + "\n").encode("utf-8"))

            resp = sock.recv(4096).decode("utf-8")
            print(resp, end="")

    except KeyboardInterrupt:
        pass
    finally:
        sock.close()

if __name__ == "__main__":
    main()
