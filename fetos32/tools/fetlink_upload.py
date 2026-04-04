import serial
import sys
import os
import time
import json
import argparse

class FetLink:
    def __init__(self, port, baud=115200):
        self.port = port
        self.baud = baud
        self.ser = None

    def connect(self):
        try:
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
            print(f"❌ Erro na porta {self.port}: {e}")
            print("   Dica: Verifique se o Monitor Serial está aberto em outro programa.")
            return False

    def send_file(self, local_path, remote_name):
        if not os.path.exists(local_path):
            print(f"❌ Arquivo {local_path} não encontrado.")
            return False

        filesize = os.path.getsize(local_path)
        print(f"📦 Enviando: {remote_name} ({filesize} bytes)")

        cmd = f"FVM_UPLOAD {remote_name} {filesize}\n"
        self.ser.write(cmd.encode('utf-8'))
        print("⏳ Solicitando acesso ao LittleFS...")

        start = time.time()
        acesso_liberado = False
        while time.time() - start < 3:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if "OK" in line:
                    acesso_liberado = True
                    break
                elif line != "":
                    print(f"  [Log OS] {line}")
        
        if not acesso_liberado:
            print("❌ ESP32 não deu OK para o upload (Timeout).")
            return False

        print("🚀 Acesso liberado! Despejando bytecode...")
        with open(local_path, 'rb') as f:
            data = f.read()
            chunk_size = 64
            
            for i in range(0, len(data), chunk_size):
                chunk = data[i:i+chunk_size]
                self.ser.write(chunk)
                self.ser.flush()
                
                enviado = min(i + chunk_size, filesize)
                porcentagem = int((enviado / filesize) * 100)
                print(f"  Progresso: [{porcentagem:3d}%] {enviado}/{filesize} bytes", end='\r')
                
                time.sleep(0.06) 

        print("\n✅ Aguardando confirmação (DONE)...")
        start = time.time()
        while time.time() - start < 5:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if "DONE" in line:
                    print("✨ SUCESSO! Arquivo gravado no LittleFS.")
                    return True
                elif line != "":
                    print(f"  [Log OS] {line}")
        
        print("⚠️ ESP32 não confirmou o fechamento (DONE).")
        return False

    def monitor(self):
        print("🖥️ Entrando em modo MONITOR (Ctrl+C para sair)...")
        try:
            while True:
                if self.ser.in_waiting > 0:
                    line = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    print(line, end='')
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\nSaindo do monitor...")

    def close(self):
        if self.ser: 
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.close()

def hardware_wizard():
    print("\n--- 🛠️ FetOS Hardware Config Wizard ---")
    profile = input("Nome do Perfil (ex: ESP32_PROTOPACK): ")
    devices = []
    
    while True:
        d_type = input("\nTipo (DISPLAY_SSD1306, DISPLAY_LCD1602_PAR, BUTTON_GPIO, BUZZER_GPIO) [Vazio p/ sair]: ")
        if not d_type: break
        d_id = int(input("ID do Dispositivo (ex: 2): "))
        dev = {"id": d_id, "type": d_type}

        if d_type == "DISPLAY_SSD1306":
            dev["address"] = input("Endereço I2C (ex: 0x3C): ")
        elif d_type == "DISPLAY_LCD1602_PAR":
            dev["pins"] = {
                "rs": int(input("  RS: ")), "en": int(input("  EN: ")),
                "d4": int(input("  D4: ")), "d5": int(input("  D5: ")),
                "d6": int(input("  D6: ")), "d7": int(input("  D7: "))
            }
        else:
            dev["pin"] = int(input("Pino GPIO: "))
        devices.append(dev)

    config = {"profile": profile, "devices": devices}
    with open("hardware.json", "w") as f:
        json.dump(config, f, indent=2)
    print("\n✅ hardware.json gerado!")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FetLink Tool - Gerenciador do FetOS")
    parser.add_argument("port", help="Porta Serial (COMx)")
    subparsers = parser.add_subparsers(dest="command")

    up_parser = subparsers.add_parser("upload", help="Sobe um arquivo")
    up_parser.add_argument("file", help="Caminho do arquivo local")
    up_parser.add_argument("--name", help="Nome destino no LittleFS")

    cfg_parser = subparsers.add_parser("config", help="Gera e sobe o hardware.json")

    args = parser.parse_args()

    if args.command == "config":
        hardware_wizard()
        link = FetLink(args.port)
        if link.connect():
            if link.send_file("hardware.json", "hardware.json"):
                link.monitor()
            link.close()

    elif args.command == "upload":
        link = FetLink(args.port)
        if link.connect():
            remote = args.name if args.name else os.path.basename(args.file)
            if link.send_file(args.file, remote):
                link.monitor()
            link.close()
    else:
        parser.print_help()