#!/usr/bin/env python3
"""
FetScript Transpiler v3.4 (Capability Sync Edition)
"""
import sys, re, argparse
# Garante que o terminal Windows não engasgue com os logs
if sys.platform == "win32":
    import codecs
    sys.stdout.reconfigure(encoding='utf-8')
VERBOSE = False
def vprint(*args, **kwargs):
    if VERBOSE: print(*args, **kwargs)
class Token:
    def __init__(self, typ, val, line):
        self.type = typ; self.value = val; self.line = line
def tokenize(code):
    vprint("\n[1/4] 🔍 ANÁLISE LÉXICA (Scanner) " + "="*30)
    specs = [
        ('COMMENT', r'//.*|/\*[\s\S]*?\*/'),
        ('STR', r'"([^"\\]|\\.)*"'),
        ('NUM', r'\d+'),
        ('DIR', r'@\w+'),
        ('ID', r'[a-zA-Z_]\w*'),
        ('OP', r'==|!=|<=|>=|&&|\|\||[+\-*/%<>=!]'),
        ('LPAREN', r'\('),
        ('RPAREN', r'\)'),
        ('LBRACE', r'\{'),
        ('RBRACE', r'\}'),
        ('SEMI', r';'),
        ('COMMA', r','),
        ('WS', r'\s+'),
    ]
    tok_regex = '|'.join(f'(?P<{n}>{p})' for n, p in specs)
    tokens = []; line = 1
    for m in re.finditer(tok_regex, code):
        kind = m.lastgroup; val = m.group()
        if kind in ('WS', 'COMMENT'):
            line += val.count('\n'); continue
        if kind == 'STR': val = val[1:-1]
        tokens.append(Token(kind, val, line))
    tokens.append(Token("EOF", "", line))
   
    vprint(f" ↳ {len(tokens)} tokens gerados.")
    return tokens
# ─── AST NODES ────────────────────────────────────────────────
class Directive: pass
class VarDecl: pass
class ConstDecl: pass
class Assign: pass
class Return: pass
class Call: pass
class SysCall: pass
class FunctionDecl: pass
class IfStmt: pass
class WhileStmt: pass
class Ident: pass
class NumLit: pass
class StrLit: pass
class BoolLit: pass
class BinOp: pass
class UnaryOp: pass
def make(cls, **kw):
    n = cls(); n.__dict__.update(kw); return n
class Parser:
    def __init__(self, tokens):
        self.tokens = tokens; self.pos = 0
    def peek(self): return self.tokens[self.pos]
    def peek2(self): return self.tokens[min(self.pos+1, len(self.tokens)-1)]
    def consume(self, t, v=None):
        tok = self.peek()
        if tok.type == t and (v is None or tok.value == v):
            self.pos += 1; return tok
        raise SyntaxError(f"L{tok.line}: esperado {t!r}({v!r}), got {tok.type!r}({tok.value!r})")
    def parse(self):
        vprint("\n[2/4] 🌳 ANÁLISE SINTÁTICA (Parser/AST) " + "="*25)
        out = []
        while self.peek().type != "EOF":
            out.append(self.parse_stmt())
        return out
    def parse_block(self):
        self.consume("LBRACE")
        stmts = []
        while self.peek().type != "RBRACE":
            stmts.append(self.parse_stmt())
        self.consume("RBRACE")
        return stmts
    def parse_stmt(self):
        tok = self.peek()
       
        if tok.type == "DIR":
            name = self.consume("DIR").value[1:]
            val = int(self.consume("NUM").value)
            if self.peek().type == "SEMI": self.consume("SEMI")
            return make(Directive, name=name, val=val)
        if tok.type != "ID": raise SyntaxError(f"L{tok.line}: esperado statement, got {tok.type!r}")
        # --- KEYWORDS ---
        if tok.value == "var":
            self.consume("ID"); name = self.consume("ID").value
            self.consume("OP", "="); expr = self.parse_expr(); self.consume("SEMI")
            return make(VarDecl, name=name, expr=expr)
       
        if tok.value == "const":
            self.consume("ID"); name = self.consume("ID").value
            self.consume("OP", "="); expr = self.parse_expr(); self.consume("SEMI")
            return make(ConstDecl, name=name, expr=expr)
           
        if tok.value == "fn":
            self.consume("ID"); name = self.consume("ID").value
            self.consume("LPAREN")
            params = []
            if self.peek().type == "ID":
                params.append(self.consume("ID").value)
                while self.peek().type == "COMMA":
                    self.consume("COMMA"); params.append(self.consume("ID").value)
            self.consume("RPAREN"); body = self.parse_block()
            return make(FunctionDecl, name=name, params=params, body=body)
        if tok.value == "return":
            self.consume("ID")
            if self.peek().type == "SEMI":
                self.consume("SEMI"); return make(Return, expr=None)
            expr = self.parse_expr(); self.consume("SEMI")
            return make(Return, expr=expr)
        if tok.value == "if":
            self.consume("ID"); cond = self.parse_expr(); then = self.parse_block()
            else_ = None
            if self.peek().type == "ID" and self.peek().value == "else":
                self.consume("ID")
                if self.peek().type == "ID" and self.peek().value == "if":
                    else_ = [self.parse_stmt()]
                else:
                    else_ = self.parse_block()
            return make(IfStmt, cond=cond, then=then, else_=else_)
        if tok.value == "while":
            self.consume("ID"); cond = self.parse_expr(); body = self.parse_block()
            return make(WhileStmt, cond=cond, body=body)
        # MUDANÇA AQUI: Reconhece 'sys' como comando (Statement)
        if tok.value == "sys" and self.peek2().type == "LPAREN":
            self.consume("ID"); self.consume("LPAREN")
            cap = self.parse_expr(); params = []
            while self.peek().type == "COMMA":
                self.consume("COMMA"); k = self.parse_expr(); self.consume("COMMA"); v = self.parse_expr()
                params.append((k, v))
            self.consume("RPAREN"); self.consume("SEMI")
            return make(SysCall, cap=cap, params=params, pop=True)
        # Atribuição ou Chamada de Função
        name = self.consume("ID").value
        if self.peek().type == "LPAREN":
            args = []; self.consume("LPAREN")
            if self.peek().type != "RPAREN":
                args.append(self.parse_expr())
                while self.peek().type == "COMMA":
                    self.consume("COMMA"); args.append(self.parse_expr())
            self.consume("RPAREN"); self.consume("SEMI")
            return make(Call, name=name, args=args, pop=True)
           
        self.consume("OP", "="); expr = self.parse_expr(); self.consume("SEMI")
        return make(Assign, name=name, expr=expr)
    def parse_expr(self): return self.parse_or()
    def parse_or(self):
        l = self.parse_and()
        while self.peek().type == "OP" and self.peek().value == "||":
            self.consume("OP"); r = self.parse_and(); l = make(BinOp, op="||", left=l, right=r)
        return l
    def parse_and(self):
        l = self.parse_cmp()
        while self.peek().type == "OP" and self.peek().value == "&&":
            self.consume("OP"); r = self.parse_cmp(); l = make(BinOp, op="&&", left=l, right=r)
        return l
    def parse_cmp(self):
        l = self.parse_add()
        while self.peek().type == "OP" and self.peek().value in ("==","!=","<",">","<=",">="):
            op = self.consume("OP").value; r = self.parse_add(); l = make(BinOp, op=op, left=l, right=r)
        return l
    def parse_add(self):
        l = self.parse_mul()
        while self.peek().value in ('+','-') and self.peek().type == "OP":
            op = self.consume("OP").value; r = self.parse_mul(); l = make(BinOp, op=op, left=l, right=r)
        return l
    def parse_mul(self):
        l = self.parse_unary()
        while self.peek().value in ('*','/') and self.peek().type == "OP":
            op = self.consume("OP").value; r = self.parse_unary(); l = make(BinOp, op=op, left=l, right=r)
        return l
    def parse_unary(self):
        if self.peek().type == "OP" and self.peek().value == "!":
            self.consume("OP"); e = self.parse_primary(); return make(UnaryOp, op="!", expr=e)
        return self.parse_primary()
       
    def parse_primary(self):
        tok = self.peek()
        if tok.type == "NUM": return make(NumLit, val=int(self.consume("NUM").value))
        if tok.type == "STR": return make(StrLit, val=self.consume("STR").value)
        if tok.type == "ID" and tok.value in ("true","false"):
            v = tok.value == "true"; self.consume("ID"); return make(BoolLit, val=v)
           
        if tok.type == "ID":
            name = self.consume("ID").value
            if self.peek().type == "LPAREN":
                self.consume("LPAREN")
               
                # MUDANÇA AQUI: Reconhece 'sys_result' como expressão (não dá POP)
                if name == "sys_result":
                    cap = self.parse_expr(); params = []
                    while self.peek().type == "COMMA":
                        self.consume("COMMA"); k = self.parse_expr(); self.consume("COMMA"); v = self.parse_expr()
                        params.append((k, v))
                    self.consume("RPAREN")
                    return make(SysCall, cap=cap, params=params, pop=False)
               
                # Chamada de função normal
                args = []
                if self.peek().type != "RPAREN":
                    args.append(self.parse_expr())
                    while self.peek().type == "COMMA":
                        self.consume("COMMA"); args.append(self.parse_expr())
                self.consume("RPAREN")
                return make(Call, name=name, args=args, pop=False)
            return make(Ident, name=name)
           
        if tok.type == "LPAREN":
            self.consume("LPAREN"); e = self.parse_expr(); self.consume("RPAREN"); return e
        raise SyntaxError(f"L{tok.line}: expressão inesperada {tok.type!r}({tok.value!r})")
class Compiler:
    def __init__(self):
        self.out = []; self.vars = {}; self.consts = set()
        self.next_heap = 0; self._label_id = 0; self._in_main = False
    def emit(self, x): self.out.append(x)
    def label(self, prefix="L"): self._label_id += 1; return f"{prefix}_{self._label_id}"
    def compile(self, ast):
        vprint("\n[3/4] ⚙️ CODE GEN (FASM Emitter) " + "="*31)
       
        # 1. Filtra diretivas para o header
        dirs = [x for x in ast if isinstance(x, Directive)]
        for d in dirs: self.emit(f"#{d.name} {d.val}")
        fns = [x for x in ast if isinstance(x, FunctionDecl)]
        main = [x for x in ast if not isinstance(x, (FunctionDecl, Directive))]
        # 2. Aloca globais
        for m in main:
            if type(m).__name__ in ("VarDecl", "ConstDecl"):
                self.alloc(m.name)
                if type(m).__name__ == "ConstDecl": self.consts.add(m.name)
        if fns: self.emit("JMP __main")
        for f in fns: self.visit_FunctionDecl(f)
       
        self.emit("__main:")
        self._in_main = True
        for m in main: self.visit(m)
        self.emit("HALT")
       
        return "\n".join(self.out)
    def visit(self, n): getattr(self, f"visit_{type(n).__name__}")(n)
    def alloc(self, name):
        slot = self.next_heap; self.vars[name] = slot; self.next_heap += 1
        return slot
    def visit_VarDecl(self, n):
        slot = self.vars[n.name] if n.name in self.vars else self.alloc(n.name)
        self.visit(n.expr); self.emit(f"STORE_H {slot}")
    def visit_ConstDecl(self, n):
        slot = self.vars[n.name] if n.name in self.vars else self.alloc(n.name)
        self.consts.add(n.name)
        self.visit(n.expr); self.emit(f"STORE_H {slot}")
    def visit_Assign(self, n):
        if n.name in self.consts: raise Exception(f"const {n.name} é readonly")
        self.visit(n.expr); self.emit(f"STORE_H {self.vars[n.name]}")
    def visit_Return(self, n):
        if n.expr: self.visit(n.expr)
        else: self.emit("PUSH_INT 0")
        self.emit("RET")
    def visit_Call(self, n):
        for a in n.args: self.visit(a)
        self.emit(f"CALL fn_{n.name}")
        if getattr(n, 'pop', False): self.emit("POP")
    def visit_SysCall(self, n):
        # MUDANÇA: Inverte parâmetros para a FVM (K1, V1, K2, V2...)
        for k, v in reversed(n.params):
            self.visit(k); self.visit(v)
       
        # Pega a string da capacidade
        cap_str = n.cap.val if isinstance(n.cap, StrLit) else "unknown"
        self.emit(f'SYS_REQ "{cap_str}" {len(n.params)}')
       
        # Se for statement isolado, limpa o resultado da pilha
        if getattr(n, 'pop', True): self.emit("POP")
    def visit_FunctionDecl(self, n):
        old_vars = self.vars.copy(); old_heap = self.next_heap; old_consts = self.consts.copy()
        self.emit(f"fn_{n.name}:")
        for p in reversed(n.params):
            slot = self.alloc(p); self.emit(f"STORE_H {slot}")
        for s in n.body: self.visit(s)
        self.emit("PUSH_INT 0"); self.emit("RET")
        self.vars = old_vars; self.next_heap = old_heap; self.consts = old_consts
    def visit_IfStmt(self, n):
        end_lbl = self.label("if_end")
        else_lbl = self.label("if_else") if n.else_ else end_lbl
        self.visit(n.cond); self.emit(f"JNIF {else_lbl}")
        for s in n.then: self.visit(s)
        if n.else_:
            self.emit(f"JMP {end_lbl}"); self.emit(f"{else_lbl}:")
            for s in n.else_: self.visit(s)
        self.emit(f"{end_lbl}:")
    def visit_WhileStmt(self, n):
        loop_lbl = self.label("wh_loop"); end_lbl = self.label("wh_end")
        self.emit(f"{loop_lbl}:"); self.visit(n.cond); self.emit(f"JNIF {end_lbl}")
        for s in n.body: self.visit(s)
        self.emit(f"JMP {loop_lbl}"); self.emit(f"{end_lbl}:")
    def visit_Ident(self, n): self.emit(f"LOAD_H {self.vars[n.name]}")
    def visit_NumLit(self, n): self.emit(f"PUSH_INT {n.val}")
    def visit_StrLit(self, n): self.emit(f'PUSH_STR "{n.val}"')
    def visit_BoolLit(self, n): self.emit(f"PUSH_BOOL {1 if n.val else 0}")
    def visit_UnaryOp(self, n):
        self.visit(n.expr)
        if n.op == "!": self.emit("NOT")
    def visit_BinOp(self, n):
        self.visit(n.left); self.visit(n.right)
        ops = {'+':'ADD','-':'SUB','*':'MUL','/':'DIV','==':'EQ','<':'LT','>':'GT','&&':'AND','||':'OR'}
        if n.op == "!=": self.emit("EQ"); self.emit("NOT")
        elif n.op == "<=": self.emit("GT"); self.emit("NOT")
        elif n.op == ">=": self.emit("LT"); self.emit("NOT")
        else: self.emit(ops[n.op])
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input"); parser.add_argument("output")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(); VERBOSE = args.verbose
   
    code = open(args.input, encoding='utf-8').read()
    tokens = tokenize(code)
    ast = Parser(tokens).parse()
    fasm = Compiler().compile(ast)
    open(args.output, "w", encoding='utf-8').write(fasm)
    print("✨ OK")