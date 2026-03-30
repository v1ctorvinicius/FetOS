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
        ser = serial.Serial(port, 115200, timeout=1) 
        
        # Tenta evitar que o ESP32 reinicie cortando o sinal de DTR/RTS
        ser.setDTR(False)
        ser.setRTS(False)
        
        print("🔌 Porta aberta! Aguardando o FetOS acordar...")
        # Dá 2 segundos pro ESP32 terminar o boot caso o auto-reset seja inevitável
        time.sleep(2) 
        
        # Limpa todo o lixo/logs de boot que acumularam nesse tempo
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
    except Exception as e:
        print(f"❌ Erro ao abrir porta {port}. Está sendo usada pelo Monitor Serial?")
        return

    # Limpa o buffer de qualquer log antigo que o ESP32 tenha mandado
    ser.reset_input_buffer()

    # 1. Manda o comando de cabeçalho
    cmd = f"FVM_UPLOAD {filename} {filesize}\n"
    ser.write(cmd.encode('utf-8'))
    print("⏳ Solicitando acesso ao LittleFS do ESP32...")

    # 2. Espera o "OK" filtrando logs do sistema
    start_time = time.time()
    acesso_liberado = False
    
    while time.time() - start_time < 3.0: # 3 segundos de timeout para o handshake
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
                # Imprime o lixo/log que chegou no meio do caminho para você ver
                print(f"  [Log do OS] {resp}")

    if not acesso_liberado:
        print("❌ Timeout: O ESP32 não respondeu 'OK' a tempo.")
        ser.close()
        return

    print("🚀 Acesso liberado! Despejando o bytecode...")

    # 3. Manda o binário
    with open(filepath, 'rb') as f:
        data = f.read()
        ser.write(data)

    # 4. Confirmação de recebimento
    start_time = time.time()
    sucesso = False
    
    while time.time() - start_time < 3.0:
        if ser.in_waiting > 0:
            resp = ser.readline().decode('utf-8', errors='ignore').strip()
            if resp == "DONE":
                sucesso = True
                break
            elif resp != "":
                print(f"  [Log do OS] {resp}")
                
    if sucesso:
        print(f"✅ SUCESSO! {filename} gravado e pronto para rodar.")
    else:
        print("⚠️ Aviso: Arquivo enviado, mas não recebi a confirmação 'DONE' final.")

    ser.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python fetlink_upload.py <PORTA_COM> <arquivo.fvm>")
        print("Ex: python fetlink_upload.py COM3 app_buzzer.fvm")
        sys.exit(1)
        
    upload_fvm(sys.argv[1], sys.argv[2])