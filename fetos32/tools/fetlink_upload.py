#!/usr/bin/env python3
import serial
import sys
import os
import time
import argparse

# O jeito CERTO de corrigir UTF-8 no Windows sem causar Buffer Lock!
if sys.platform == "win32":
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')

class FetLink:
    def __init__(self, port, baud=115200, verbose=False):
        self.port = port
        self.baud = baud
        self.ser = None
        self.verbose = verbose

    def vprint(self, *args, **kwargs):
        if self.verbose:
            print(f"[DEBUG][FetLink] ", *args, **kwargs)

    def connect(self):
        try:
            self.vprint(f"Conectando na porta {self.port}...")
            self.ser = serial.Serial()
            self.ser.port = self.port
            self.ser.baudrate = self.baud
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.timeout = 1
            self.ser.open()
            time.sleep(0.5)
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            print(f"[ERRO] Porta {self.port}: {e}")
            sys.exit(1)

    def delete_file(self, remote_name):
        print(f"RM: {remote_name}")
        if not remote_name.startswith('/'):
            remote_name = f"/{remote_name}"
            
        cmd = f"FVM_DELETE {remote_name}\n"
        self.ser.write(cmd.encode('utf-8'))
        
        start = time.time()
        while time.time() - start < 3:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                self.vprint(f"ESP32: {line}")
                if "DONE_DELETE" in line:
                    print("OK: Arquivo removido.")
                    return True
                elif "ERR_NOT_FOUND" in line:
                    print("AVISO: Arquivo nao encontrado.")
                    return True
        return False

    def send_file(self, local_path, remote_name):
        if not os.path.exists(local_path):
            print(f"ERRO: Arquivo local {local_path} nao existe.")
            return False
        
        filesize = os.path.getsize(local_path)
        print(f"UPLOAD: {remote_name} ({filesize} bytes)")
        
        self.vprint(f"Solicitando upload...")
        cmd = f"FVM_UPLOAD {remote_name} {filesize}\n"
        self.ser.write(cmd.encode('utf-8'))
        
        start = time.time()
        acesso_liberado = False
        while time.time() - start < 3:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                self.vprint(f"ESP32 diz: {line}")
                if "OK" in line:
                    acesso_liberado = True
                    break
        
        if not acesso_liberado:
            print("ERRO: ESP32 recusou o upload.")
            return False

        self.vprint("Transmitindo...")
        with open(local_path, 'rb') as f:
            data = f.read()
            for i in range(0, len(data), 64):
                self.ser.write(data[i:i+64])
                time.sleep(0.06)
                if self.verbose:
                    prog = min(100, int((i + 64) / len(data) * 100))
                    # Flush forçado no terminal funciona perfeitamente agora!
                    print(f"\r   ↳ {prog}%", end="", flush=True)

        if self.verbose: print() # Pula a linha pós-upload
        print("OK: Transmissao concluida. Aguardando DONE...")
        
        start = time.time()
        while time.time() - start < 5:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                self.vprint(f"ESP32: {line}")
                if "DONE" in line:
                    print("SUCESSO!")
                    return True
        
        print("ERRO: Timeout no salvamento.")
        return False

    def monitor(self):
        print("MONITOR: (Ctrl+C para sair)...")
        try:
            while True:
                if self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting)
                    # flush=True garante que o monitor não tenha "delay"
                    print(data.decode('utf-8', errors='replace'), end='', flush=True)
                time.sleep(0.01)
        except KeyboardInterrupt:
            # O except pega o Ctrl+C, fecha silenciamente.
            pass

    def close(self):
        if self.ser: self.ser.close()

if __name__ == "__main__":
    base_parser = argparse.ArgumentParser(add_help=False)
    base_parser.add_argument("-v", "--verbose", action="store_true")
    base_parser.add_argument("--monitor", action="store_true")

    parser = argparse.ArgumentParser(parents=[base_parser], description="FetLink Tool")
    parser.add_argument("port", help="Porta Serial")
    
    subparsers = parser.add_subparsers(dest="command")

    up_parser = subparsers.add_parser("upload", parents=[base_parser])
    up_parser.add_argument("file")
    up_parser.add_argument("--name")

    rm_parser = subparsers.add_parser("rm", parents=[base_parser])
    rm_parser.add_argument("name")

    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)

    link = FetLink(args.port, verbose=args.verbose)

    if args.command == "rm":
        if link.connect():
            res = link.delete_file(args.name)
            link.close()
            if not res: sys.exit(1)
            
    elif args.command == "upload":
        if link.connect():
            # Pegando o remote name exato, que agora o automator passa forçadamente
            remote = args.name if args.name else os.path.basename(args.file)
            ok = link.send_file(args.file, remote)

            if ok and args.monitor:
                link.monitor()

            link.close()

            if not ok:
                sys.exit(1)