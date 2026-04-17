#!/usr/bin/env python3
"""
FetOS Automator - Toolchain Runner (Live Output Edition)
"""
import sys
import os
import subprocess
import argparse

sys.stdout.reconfigure(encoding='utf-8')

def run_cmd(cmd, step_name, verbose=False):
    # Injetamos o "-u" (Unbuffered) para forçar o Python a cuspir os prints em tempo real
    if cmd[0] == sys.executable and "-u" not in cmd:
        cmd.insert(1, "-u")

    if verbose:
        print(f"\n[⚙️  PASS {step_name}] Executando: {' '.join(cmd)}")
    else:
        print(f"\n[⚙️  PASS {step_name}] {' '.join(cmd)}")
    
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError:
        print(f"\n❌ ERRO FATAL: O passo {step_name} falhou.")
        sys.exit(1)
    except FileNotFoundError:
        print(f"\n❌ ERRO: Um dos scripts não foi encontrado. Verifique os caminhos.")
        sys.exit(1)
    except KeyboardInterrupt:
        # Quando você der Ctrl+C no monitor, o automador pega aqui e sai calado, sem traceback!
        print(f"\n🛑 [Automator] Processo encerrado pelo usuário.")
        sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description="FetOS Automator")
    parser.add_argument("input_fet", help="Arquivo .fets local (ou NOME DO APP se usar --rm)")
    parser.add_argument("port", help="Porta COM")
    parser.add_argument("-v", "--verbose", action="store_true", help="Ativa modo verboso")
    parser.add_argument("--monitor", action="store_true", help="Abre o monitor serial após o upload")
    parser.add_argument("--rm", action="store_true", help="Remove o app do ESP32 em vez de compilar")
    
    args = parser.parse_args()

    # A MÁGICA ESTÁ AQUI: Só checamos se o arquivo existe se NÃO for uma operação de remoção
    if not args.rm and not os.path.exists(args.input_fet):
        print(f"❌ Erro: Arquivo local '{args.input_fet}' não existe.")
        sys.exit(1)

    os.makedirs("build", exist_ok=True)
    
    # Extração inteligente de nome! Funciona quer você digite "app", "app.fets", ou "/app.fvm"
    base_name = os.path.splitext(os.path.basename(args.input_fet))[0]
    fasm_file = os.path.join("build", f"{base_name}.fasm")
    fvm_file = os.path.join("build", f"{base_name}.fvm")
    
    remote_name = f"/{base_name}.fvm" 

    v_flag = ["-v"] if args.verbose else []

    if args.rm:
        print(f"🚀 --- Iniciando remoção no FetOS: {remote_name} ---")
        rm_cmd = [sys.executable, "fetlink_upload.py", args.port, "rm", remote_name] + v_flag
        run_cmd(rm_cmd, "Remover App", args.verbose)
        print(f"\n✨ SUCESSO! O app '{remote_name}' virou poeira cósmica!")
        sys.exit(0)

    print(f"🚀 --- Iniciando FetOS Toolchain para: {base_name} ---")

    run_cmd([sys.executable, "fetscript.py", args.input_fet, fasm_file] + v_flag, "1 (Transpilação)", args.verbose)
    run_cmd([sys.executable, "fasm.py", fasm_file, fvm_file] + v_flag, "2 (Assembler)", args.verbose)
    
    upload_cmd = [sys.executable, "fetlink_upload.py", args.port, "upload", fvm_file, "--name", remote_name] + v_flag
    if args.monitor:
        upload_cmd.append("--monitor")
        
    run_cmd(upload_cmd, "3 (Upload)", args.verbose)

    print(f"\n✨ SUCESSO! O app '{base_name}' está rodando na fera!")

if __name__ == "__main__":
    main()