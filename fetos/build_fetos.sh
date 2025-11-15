#!/usr/bin/env bash
set -euo pipefail

# ===========================================
# ⚙️ CONFIGURAÇÕES DO PROJETO
# ===========================================
MCU="atmega328p"
PROGRAMMER="arduino"
F_CPU=16000000UL
TARGET="FetOS"

ROOT_DIR="$(dirname "$(realpath "$0")")"
BUILD_DIR="$ROOT_DIR/build"
ELF="$BUILD_DIR/$TARGET.elf"
HEX="$BUILD_DIR/$TARGET.hex"

# ===========================================
# 🎨 Helpers
# ===========================================
msg() { echo -e "\033[1;34m$1\033[0m"; }
ok()  { echo -e "\033[1;32m$1\033[0m"; }
err() { echo -e "\033[1;31m$1\033[0m" >&2; }

# ===========================================
# 🔍 Utilitários
# ===========================================
check_tool() {
    if ! command -v "$1" &>/dev/null; then
        err "❌ '$1' não encontrado."
        exit 1
    fi
}

detect_port() {
    local port
    port=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n1 || true)
    [[ -z "$port" ]] && { err "⚠️ Nenhuma porta serial detectada!"; exit 1; }
    echo "$port"
}

# ===========================================
# 🔧 Compilação
# ===========================================
msg "\n==========================================="
msg "  🔧 COMPILANDO $TARGET"
msg "==========================================="

for tool in avr-gcc avr-objcopy avr-size avrdude; do check_tool "$tool"; done
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
# 🚀 Gravação
# ===========================================
if [[ "${1:-}" == "--flash" || "${1:-}" == "--serial" ]]; then
    msg "\n==========================================="
    msg "  🚀 GRAVANDO NO ARDUINO"
    msg "==========================================="

    PORT=$(detect_port)
    ok "📡 Porta detectada: $PORT"

    avrdude -c $PROGRAMMER -p $MCU -P "$PORT" -b 115200 -U flash:w:"$HEX":i
    ok "✅ Gravação concluída!"
    sleep 1

    # =======================================
    # 🔎 AUTO-DETECÇÃO DE BAUD
    # =======================================
    if [[ "${1:-}" == "--serial" ]]; then
        msg "\n==========================================="
        msg "  🧠 AUTO-DETECÇÃO DE BAUD"
        msg "==========================================="

        for baud in 115200 57600 38400 19200 9600; do
            stty -F "$PORT" $baud raw -echo
            echo > "$PORT" || true
            line=$(timeout 1 cat "$PORT" | grep -m1 "\[INFO\]" || true)
            if [[ "$line" == *"BAUD="* ]]; then
                ok "✅ Baud detectado automaticamente: $baud"
                DETECTED_BAUD=$baud
                break
            fi
        done

        DETECTED_BAUD="${DETECTED_BAUD:-115200}"
        msg "\n==========================================="
        msg "  🖥️ MONITOR SERIAL  (${DETECTED_BAUD} baud)"
        msg "==========================================="
        sleep 1

        stty -F "$PORT" $DETECTED_BAUD raw -echo
        cat "$PORT" | awk '
            /\[FetLink\]/ {print "\033[1;33m" $0 "\033[0m"; next}
            /\{.*\}/     {print "\033[1;36m" $0 "\033[0m"; next}
            {print $0}'
    fi
fi
