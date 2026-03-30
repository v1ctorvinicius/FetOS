import serial
import sys
import os
import time

def upload_fvm(port, filepath):
    if not os.path.exists(filepath):
        print(f"❌ Erro: Arquivo {filepath} não encontrado.")
        return

    filename = os.path.basename(filepath)
    filesize = os.path.getsize(filepath)

    print(f"📦 Preparando envio: {filename} ({filesize} bytes)")

    try:
        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.dtr = False  # define ANTES de abrir — evita pulso de reset
        ser.rts = False  # define ANTES de abrir — evita pulso de reset
        ser.open()
        time.sleep(0.5)
        ser.reset_input_buffer()
    except Exception as e:
        print(f"❌ Erro ao abrir porta {port}. Está sendo usada pelo Monitor Serial?")
        print(f"   Detalhe: {e}")
        return

    # 1. Manda o comando de cabeçalho
    cmd = f"FVM_UPLOAD {filename} {filesize}\n"
    ser.write(cmd.encode('utf-8'))
    print("⏳ Solicitando acesso ao LittleFS do ESP32...")

    # 2. Espera o "OK" filtrando logs do sistema
    start_time = time.time()
    acesso_liberado = False

    while time.time() - start_time < 3.0:
        if ser.in_waiting > 0:
            resp = ser.readline().decode('utf-8', errors='ignore').strip()

            if resp == "OK":
                acesso_liberado = True
                break
            elif resp == "ERR_FS":
                print("❌ Falha: O ESP32 não conseguiu abrir o arquivo no LittleFS.")
                ser.close()
                return
            elif resp != "":
                print(f"  [Log do OS] {resp}")

    if not acesso_liberado:
        print("❌ Timeout: O ESP32 não respondeu 'OK' a tempo.")
        ser.close()
        return

    print("🚀 Acesso liberado! Despejando o bytecode...")

    # 3. Manda o binário em blocos pequenos
    chunk_size = 64
    with open(filepath, 'rb') as f:
        data = f.read()
        total_sent = 0
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i + chunk_size]
            ser.write(chunk)
            ser.flush()
            total_sent += len(chunk)
            time.sleep(0.01)
            print(f"  Enviando... {total_sent}/{filesize} bytes", end='\r')

    print("\n✅ Envio concluído. Aguardando confirmação...")

    # 4. Confirmação de recebimento
    start_time = time.time()
    sucesso = False

    while time.time() - start_time < 5.0:
        if ser.in_waiting > 0:
            resp = ser.readline().decode('utf-8', errors='ignore').strip()
            if resp == "DONE":
                sucesso = True
                break
            elif resp != "":
                print(f"  [Log do OS] {resp}")

    if sucesso:
        print(f"✅ SUCESSO! Gravado. Entrando em modo MONITOR (Ctrl+C para sair)...")
        try:
            while True:
                if ser.in_waiting > 0:
                    line = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    print(line, end='')
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\nSaindo...")
    else:
        print("⚠️ Falha: ESP32 não confirmou o recebimento.")

    ser.dtr = False
    ser.rts = False
    ser.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python fetlink_upload.py <PORTA_COM> <arquivo.fvm>")
        print("Ex:  python fetlink_upload.py COM3 app_buzzer.fvm")
        sys.exit(1)

    upload_fvm(sys.argv[1], sys.argv[2])