#fasm.py
import sys
import struct
import shlex

# Mapeamento de Opcodes (Mantendo o seu padrão)
OPCODES = {
    "PUSH_INT":  (0x01, 'h'),
    "PUSH_STR":  (0x02, 's'),
    "PUSH_BOOL": (0x03, 'B'),
    "PUSH_NIL":  (0x04, ''),
    "POP":       (0x05, ''),
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

def compile_fasm(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()

    string_table = []
    labels = {}
    instructions = []
    
    # PASSAGEM 1: Strings e Labels
    current_address = 0
    for line in lines:
        line = line.split(';')[0].split('//')[0].strip()
        if not line: continue
        if line.endswith(':'):
            labels[line[:-1]] = current_address
            continue
        tokens = shlex.split(line)
        op_name = tokens[0].upper()
        opcode_hex, arg_types = OPCODES[op_name]
        for i, arg_type in enumerate(arg_types):
            if arg_type == 's' and tokens[i+1] not in string_table:
                string_table.append(tokens[i+1])
        instructions.append((op_name, arg_types, tokens[1:]))
        size = 1 
        for arg in arg_types:
            if arg in ['h', 'L']: size += 2
            elif arg in ['B', 's']: size += 1
        current_address += size

    # PASSAGEM 2: Bytecode (AGORA EM BIG ENDIAN '>')
    bytecode = bytearray()
    for op_name, arg_types, args in instructions:
        bytecode.append(OPCODES[op_name][0])
        for i, arg_type in enumerate(arg_types):
            val = args[i]
            if arg_type == 'h': # int16 Big Endian
                bytecode.extend(struct.pack('>h', int(val)))
            elif arg_type == 'B':
                bytecode.extend(struct.pack('B', int(val)))
            elif arg_type == 's':
                bytecode.append(string_table.index(val))
            elif arg_type == 'L': # uint16 Big Endian
                bytecode.extend(struct.pack('>H', labels[val]))

    # Pack final .fvm
    output = bytearray(b'FVM1')
    output.append(len(string_table))
    for s in string_table:
        output.extend(s.encode('utf-8') + b'\x00')
    output.extend(struct.pack('>H', len(bytecode))) # Tamanho em Big Endian
    output.extend(bytecode)

    with open(output_file, 'wb') as f:
        f.write(output)
    print(f"✅ Compilado em Big Endian: {output_file}")

if __name__ == "__main__":
    compile_fasm(sys.argv[1], sys.argv[2])