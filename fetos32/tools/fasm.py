#!/usr/bin/env python3
# fasm.py — FetOS Assembler v2 (Extreme Verbose Edition)
import sys
import struct
import shlex
import argparse
import os

sys.stdout.reconfigure(encoding='utf-8')

OPCODES = {
    "PUSH_INT":  (0x01, 'h'),
    "PUSH_STR":  (0x02, 's'),
    "PUSH_BOOL": (0x03, 'B'),
    "PUSH_NIL":  (0x04, ''),
    "POP":       (0x05, ''),
    "DUP":       (0x06, ''),
    "STORE_H":   (0x10, 'B'),
    "LOAD_H":    (0x11, 'B'),
    "ADD":       (0x20, ''),
    "SUB":       (0x21, ''),
    "MUL":       (0x22, ''),
    "DIV":       (0x23, ''),
    "EQ":        (0x30, ''),
    "LT":        (0x31, ''),
    "GT":        (0x32, ''),
    "AND":       (0x33, ''),
    "OR":        (0x34, ''),
    "NOT":       (0x35, ''),
    "JMP":       (0x40, 'L'),
    "JIF":       (0x41, 'L'),
    "JNIF":      (0x42, 'L'),
    "CALL":      (0x43, 'L'),
    "RET":       (0x44, ''),
    "HALT":      (0x45, ''),
    "SYS_REQ":   (0x50, 'sB'),
    "SYS_EVT":   (0x51, 'B'),
    "STORE_P":   (0x60, 's'),
    "LOAD_P":    (0x61, 's')
}

def compile_fasm(input_file, output_file, def_stack=16, def_heap=16, def_ram=0, force_v1=False, verbose=False):
    def vprint(*args, **kwargs):
        if verbose: print(*args, **kwargs)

    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    string_table = []
    labels = {}
    instructions = []
    
    hdr_stack = def_stack
    hdr_heap = def_heap
    hdr_ram = def_ram
    is_v2 = False

    vprint(f"\n[FASM] --- INICIANDO MONTAGEM: {input_file} ---")
    vprint("\n[1/4] 📂 LEITURA E DIRETIVAS (Pass 1) " + "="*31)

    # PASSAGEM 1: Diretivas, Strings e Labels
    current_address = 0
    for line_num, line in enumerate(lines, 1):
        original_line = line.strip()
        line = line.split(';')[0].split('//')[0].strip()
        if not line: continue
        
        # Diretivas
        if line.startswith('#'):
            is_v2 = True
            tokens = line.lower().split()
            if tokens[0] == '#stack': hdr_stack = int(tokens[1])
            elif tokens[0] == '#heap': hdr_heap = int(tokens[1])
            elif tokens[0] == '#ram': hdr_ram = int(tokens[1])
            vprint(f"  ↳ [Header] Aplicado: {original_line}")
            continue

        # Labels
        if line.endswith(':'):
            labels[line[:-1]] = current_address
            continue
            
        tokens = shlex.split(line)
        op_name = tokens[0].upper()
        if op_name not in OPCODES:
            print(f"❌ Erro na linha {line_num}: Instrução desconhecida '{op_name}'")
            sys.exit(1)
            
        opcode_hex, arg_types = OPCODES[op_name]
        
        # Coleta strings
        for i, arg_type in enumerate(arg_types):
            if arg_type == 's' and tokens[i+1] not in string_table:
                string_table.append(tokens[i+1])
        
        instructions.append((current_address, op_name, arg_types, tokens[1:], line_num))
        
        # Calcula tamanho para os endereços
        size = 1
        for arg in arg_types:
            if arg in ['h', 'L']: size += 2
            elif arg in ['B', 's']: size += 1
        current_address += size

    vprint("\n[2/4] 🗂️  TABELAS DE SÍMBOLOS " + "="*37)
    vprint(f"  ↳ Tabela de Strings ({len(string_table)}):")
    if not string_table: vprint("      (Nenhuma string definida)")
    for idx, s in enumerate(string_table): 
        vprint(f"      [0x{idx:02X}] '{s}'")
        
    vprint(f"  ↳ Tabela de Labels ({len(labels)}):")
    if not labels: vprint("      (Nenhuma label definida)")
    for k, v in labels.items(): 
        vprint(f"      {k+':':<20} -> 0x{v:04X}")

    vprint("\n[3/4] ⚙️  RESOLUÇÃO DE BYTECODE E HEX DUMP " + "="*24)
    vprint("  ADDR   | HEX DUMP            | INSTRUÇÃO")
    vprint(" --------+---------------------+-------------------------")
    
    # PASSAGEM 2: Bytecode e Hex Dump
    bytecode = bytearray()
    for addr, op_name, arg_types, args, lnum in instructions:
        inst_bytes = bytearray([OPCODES[op_name][0]])
        for i, arg_type in enumerate(arg_types):
            val = args[i]
            if arg_type == 'h': 
                inst_bytes.extend(struct.pack('>h', int(val)))
            elif arg_type == 'B':
                inst_bytes.extend(struct.pack('B', int(val)))
            elif arg_type == 's':
                inst_bytes.append(string_table.index(val))
            elif arg_type == 'L': 
                if val not in labels:
                    print(f"\n❌ Erro de Linkagem na linha {lnum}: Label '{val}' não encontrado!")
                    sys.exit(1)
                inst_bytes.extend(struct.pack('>H', labels[val]))
        
        bytecode.extend(inst_bytes)
        
        if verbose:
            # Formata os bytes em Hex (ex: "50 00 01")
            hex_str = " ".join(f"{b:02X}" for b in inst_bytes)
            inst_str = f"{op_name} {' '.join(args)}"
            vprint(f"  0x{addr:04X} | {hex_str:<19} | {inst_str}")

    if force_v1:
        is_v2 = False

    # Montagem Final
    header_bytes = bytearray()
    if is_v2 or def_ram > 0:
        header_bytes.extend(b'FVM2')
        header_bytes.append(0x00) 
        header_bytes.extend(struct.pack('>H', hdr_ram))
        header_bytes.append(hdr_stack)
        header_bytes.append(hdr_heap)
    else:
        header_bytes.extend(b'FVM1')

    str_table_bytes = bytearray([len(string_table)])
    for s in string_table:
        str_table_bytes.extend(s.encode('utf-8') + b'\x00')
    
    bytecode_header_bytes = struct.pack('>H', len(bytecode))
    
    output = bytearray()
    output.extend(header_bytes)
    output.extend(str_table_bytes)
    output.extend(bytecode_header_bytes)
    output.extend(bytecode)

    with open(output_file, 'wb') as f:
        f.write(output)
    
    if verbose:
        vprint("\n[4/4] 📦 MONTAGEM DO BINÁRIO (.FVM) " + "="*31)
        vprint(f"  ↳ Assinatura:  {'FVM2' if (is_v2 or def_ram > 0) else 'FVM1'}")
        if is_v2:
            vprint(f"  ↳ VM Specs:    Stack={hdr_stack}, Heap={hdr_heap}, RAM={hdr_ram}B")
        
        vprint("\n  [Layout do Arquivo]")
        vprint(f"  ├─ Header:        {len(header_bytes):4d} bytes")
        vprint(f"  ├─ String Table:  {len(str_table_bytes):4d} bytes")
        vprint(f"  └─ Bytecode:      {len(bytecode_header_bytes) + len(bytecode):4d} bytes (2 de len + {len(bytecode)} de instrucoes)")
        vprint(f"     Total:         {len(output):4d} bytes")
        vprint("="*67 + "\n")

    fmt = "FVM2" if (is_v2 or def_ram > 0) else "FVM1"
    print(f"✅ {fmt} compilado: {output_file} ({len(output)} bytes)")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FetOS Assembler v2")
    parser.add_argument("input",  help="arquivo .fasm de entrada")
    parser.add_argument("output", help="arquivo .fvm de saída")
    parser.add_argument("--stack", type=int, default=16, help="tamanho da stack (padrao 16)")
    parser.add_argument("--heap",  type=int, default=16, help="slots do heap (padrao 16)")
    parser.add_argument("--ram",   type=int, default=0, help="RAM minima em bytes")
    parser.add_argument("--v1", action="store_true", help="forca formato FVM1 legado")
    parser.add_argument("-v", "--verbose", action="store_true", help="Mostra log detalhado da compilacao (Hex Dump)")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"❌ Erro: Arquivo '{args.input}' nao encontrado.")
        sys.exit(1)
        
    compile_fasm(args.input, args.output, args.stack, args.heap, args.ram, args.v1, args.verbose)