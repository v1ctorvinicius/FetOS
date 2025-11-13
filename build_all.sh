#!/bin/bash
set -e
echo "🔧 Compilando FetOS..."
(cd fetos && ./build_fetos.sh)

echo "🧠 Iniciando FetHub..."
(cd fethub && python3 fethub.py)
