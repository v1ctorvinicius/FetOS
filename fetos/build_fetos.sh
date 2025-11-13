#!/usr/bin/env bash
# build_fetos.sh
set -euo pipefail

# ===========================================
#  ⚙️  CONFIGURAÇÕES DO PROJETO
# ===========================================
PORT="/dev/ttyACM0"
MCU="atmega328p"
F_CPU=16000000UL
PROGRAMMER="arduino"
BAUD="115200"
TARGET="FetOS"

ROOT_DIR="$(dirname "$(realpath "$0")")"
BUILD_DIR="$ROOT_DIR/build"
ELF="$BUILD_DIR/$TARGET.elf"
HEX="$BUILD_DIR/$TARGET.hex"

# ===========================================
#  🧩 FUNÇÕES AUXILIARES
# ===========================================
msg() { echo -e "\033[1;36m$1\033[0m"; }
ok()  { echo -e "\033[1;32m$1\033[0m"; }
err() { echo -e "\033[1;31m$1\033[0m" >&2; }

check_tool() {
    if ! command -v "$1" &>/dev/null; then
        err "❌ Ferramenta '$1' não encontrada. Instale antes de continuar."
        case "$(grep -oP '^ID=\K\w+' /etc/os-release)" in
            arch|garuda|manjaro) echo "→ sudo pacman -S avr-gcc avr-libc avrdude" ;;
            ubuntu|debian)       echo "→ sudo apt install gcc-avr avr-libc avrdude" ;;
            *)                   echo "→ Instale manualmente o pacote AVR toolchain." ;;
        esac
        exit 1
    fi
}

detect_port() {
    local port
    port=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n1 || true)
    [[ -z "$port" ]] && { err "⚠️ Nenhuma porta serial detectada!"; exit 1; }
    echo "$port"
}

ensure_serial_access() {
    local port="$1"
    if [[ ! -r "$port" || ! -w "$port" ]]; then
        sudo chmod a+rw "$port" && ok "✅ Permissão concedida temporariamente."
    fi
}

# ===========================================
#  🔧 COMPILAÇÃO
# ===========================================
msg "\n==========================================="
msg "  🔧 COMPILANDO $TARGET"
msg "==========================================="

check_tool avr-gcc
check_tool avr-objcopy
check_tool avr-size
check_tool avrdude

mkdir -p "$BUILD_DIR"

echo
echo "[1/4] Compilando fontes..."
SRC_LIST=("$ROOT_DIR"/*.c "$ROOT_DIR/src/"*.c "$ROOT_DIR/apps/"*.c)
for f in "${SRC_LIST[@]}"; do
    [[ -f "$f" ]] && echo "  - $(basename "$f")" \
        && avr-gcc -std=gnu99 -mmcu=$MCU -DF_CPU=$F_CPU -Os -Wall \
           -ffunction-sections -fdata-sections \
           -I"$ROOT_DIR/include" -I"$ROOT_DIR/apps" \
           -c "$f" -o "$BUILD_DIR/$(basename "${f%.*}").o"
done

echo
echo "[2/4] Ligando objetos..."
avr-gcc -mmcu=$MCU -Os -flto -Wl,--gc-sections -o "$ELF" "$BUILD_DIR"/*.o

echo
echo "[3/4] Gerando HEX..."
avr-objcopy -O ihex -R .eeprom "$ELF" "$HEX"

echo
echo "[4/4] Informações do binário:"
avr-size -A "$ELF"
ok "\n✅ Compilação concluída."
msg "==========================================="

# ===========================================
#  🚀 GRAVAÇÃO / SERIAL / SIMULADOR
# ===========================================
if [[ "${1:-}" == "--flash" || "${1:-}" == "--serial" ]]; then
    msg "\n==========================================="
    msg "  🚀 GRAVANDO NO ARDUINO"
    msg "==========================================="
    PORT=$(detect_port)
    ensure_serial_access "$PORT"
    ok "📡 Porta detectada: $PORT"
    avrdude -c $PROGRAMMER -p $MCU -P "$PORT" -b $BAUD -U flash:w:"$HEX":i
    ok "✅ Gravação concluída!"

    if [[ "${1:-}" == "--serial" ]]; then
        msg "\n==========================================="
        msg "  🖥️  MONITOR SERIAL (Ctrl+C pra sair)"
        msg "==========================================="
        sleep 2
        sudo stty -F "$PORT" $BAUD raw -echo
        sudo cat "$PORT"
        exit 0
    fi
fi

msg "\n==========================================="
msg "  🧠 Iniciando FetOS Hub (simulador)"
msg "==========================================="
if [[ -f "$ROOT_DIR/../fethub/fethub.py" ]]; then
    python3 -m fethub.fethub "$PORT"
else
    err "⚠️  Arquivo fethub.py não encontrado — pulando etapa de simulação."
fi

ok "\n✅ Processo completo!"
msg "==========================================="
