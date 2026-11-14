#!/usr/bin/env python3
"""psxrecomp: static recompiler for PlayStation 1 (MIPS R3000A) code, producing C.

    psxrecomp.py EXE --out DIR [--prefix NAME] [--hooks hooks.py]

Design
  * A "function" is a set of basic blocks reachable from an entry without following `jal`.
  * Every block leader gets a C label; branches are gotos, delay slots are emitted inline.
  * `jr $ra` returns. A `jr rX` that is not a return dispatches with a switch over the labels of
    the function (so jump tables need no pattern recovery), and anything unknown falls out to the
    runtime's dynamic dispatcher, which runs recompiled code if it knows the address and otherwise
    interprets it. That keeps code the tool cannot see (overlays, self-modified code) working.
  * Calls to other functions are direct C calls; `jalr` and unknown targets go through the dispatcher.
"""
import argparse
import os
import struct
import sys

# ----------------------------------------------------------------------------------------- decoding

REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
       's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']

SPECIAL = {0x00: 'sll', 0x02: 'srl', 0x03: 'sra', 0x04: 'sllv', 0x06: 'srlv', 0x07: 'srav', 0x08: 'jr', 0x09: 'jalr',
           0x0C: 'syscall', 0x0D: 'break', 0x10: 'mfhi', 0x11: 'mthi', 0x12: 'mflo', 0x13: 'mtlo', 0x18: 'mult',
           0x19: 'multu', 0x1A: 'div', 0x1B: 'divu', 0x20: 'add', 0x21: 'addu', 0x22: 'sub', 0x23: 'subu',
           0x24: 'and', 0x25: 'or', 0x26: 'xor', 0x27: 'nor', 0x2A: 'slt', 0x2B: 'sltu'}
OPS = {0x02: 'j', 0x03: 'jal', 0x04: 'beq', 0x05: 'bne', 0x06: 'blez', 0x07: 'bgtz', 0x08: 'addi', 0x09: 'addiu',
       0x0A: 'slti', 0x0B: 'sltiu', 0x0C: 'andi', 0x0D: 'ori', 0x0E: 'xori', 0x0F: 'lui', 0x20: 'lb', 0x21: 'lh',
       0x22: 'lwl', 0x23: 'lw', 0x24: 'lbu', 0x25: 'lhu', 0x26: 'lwr', 0x28: 'sb', 0x29: 'sh', 0x2A: 'swl',
       0x2B: 'sw', 0x2E: 'swr', 0x32: 'lwc2', 0x3A: 'swc2'}

BRANCHES = {'beq', 'bne', 'blez', 'bgtz', 'bltz', 'bgez', 'bltzal', 'bgezal'}
LOADS = {'lb', 'lh', 'lw', 'lbu', 'lhu'}


class Insn:
    __slots__ = ('addr', 'w', 'op', 'rs', 'rt', 'rd', 'sh', 'imm', 'simm', 'target', 'cop')

    def __init__(self, addr, w):
        self.addr, self.w = addr, w
        op = w >> 26
        self.rs, self.rt, self.rd = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
        self.sh = (w >> 6) & 31
        self.imm = w & 0xFFFF
        self.simm = self.imm - 0x10000 if self.imm & 0x8000 else self.imm
        self.target = ((addr + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        self.cop = w & 0x1FFFFFF
        if op == 0:
            self.op = SPECIAL.get(w & 63, '?')
        elif op == 1:
            self.op = {0: 'bltz', 1: 'bgez', 0x10: 'bltzal', 0x11: 'bgezal'}.get(self.rt, '?')
        elif op == 0x10:
            self.op = {0: 'mfc0', 4: 'mtc0'}.get(self.rs, 'rfe' if (w & 63) == 0x10 and self.rs == 0x10 else '?')
        elif op == 0x12:
            self.op = 'cop2' if w & (1 << 25) else {0: 'mfc2', 2: 'cfc2', 4: 'mtc2', 6: 'ctc2'}.get(self.rs, '?')
        else:
            self.op = OPS.get(op, '?')

    def branch_target(self):
        return (self.addr + 4 + (self.simm << 2)) & 0xFFFFFFFF

    @property
    def is_branch(self):
        return self.op in BRANCHES

    @property
    def is_jump(self):
        return self.op in ('j', 'jal', 'jr', 'jalr')

    @property
    def has_delay(self):
        return self.is_branch or self.is_jump

    def reads(self):
        o = self.op
        if o in ('sll', 'srl', 'sra'):
            return {self.rt}
        if o in ('sllv', 'srlv', 'srav', 'add', 'addu', 'sub', 'subu', 'and', 'or', 'xor', 'nor', 'slt', 'sltu',
                 'mult', 'multu', 'div', 'divu', 'beq', 'bne'):
            return {self.rs, self.rt}
        if o in ('jr', 'jalr', 'mthi', 'mtlo', 'blez', 'bgtz', 'bltz', 'bgez', 'bltzal', 'bgezal', 'addi', 'addiu',
                 'slti', 'sltiu', 'andi', 'ori', 'xori', 'lb', 'lh', 'lw', 'lbu', 'lhu', 'lwc2', 'swc2'):
            return {self.rs}
        if o in ('sb', 'sh', 'sw'):
            return {self.rs, self.rt}
        if o in ('lwl', 'lwr', 'swl', 'swr'):
            return {self.rs, self.rt}
        if o in ('mtc0', 'mtc2', 'ctc2'):
            return {self.rt}
        return set()

    def writes(self):
        o = self.op
        if o in ('sll', 'srl', 'sra', 'sllv', 'srlv', 'srav', 'add', 'addu', 'sub', 'subu', 'and', 'or', 'xor', 'nor',
                 'slt', 'sltu', 'mfhi', 'mflo', 'jalr'):
            return self.rd
        if o in ('addi', 'addiu', 'slti', 'sltiu', 'andi', 'ori', 'xori', 'lui', 'lb', 'lh', 'lw', 'lbu', 'lhu',
                 'lwl', 'lwr', 'mfc0', 'mfc2', 'cfc2'):
            return self.rt
        if o == 'jal' or o in ('bltzal', 'bgezal'):
            return 31
        return None


class Image:
    def __init__(self, base, data):
        self.base, self.data = base, data
        self.end = base + len(data)

    def has(self, a):
        return self.base <= a < self.end and not a & 3

    def word(self, a):
        return struct.unpack_from('<I', self.data, a - self.base)[0]

    def insn(self, a):
        return Insn(a, self.word(a))


def load_exe(path):
    d = open(path, 'rb').read()
    if d[:8] != b'PS-X EXE':
        raise SystemExit('not a PS-X EXE: ' + path)
    pc, gp, addr, size = struct.unpack_from('<IIII', d, 0x10)
    return pc, gp, Image(addr & 0xFFFFFFFF, d[0x800:0x800 + size])


# --------------------------------------------------------------------------------------- discovery

class Function:
    def __init__(self, entry):
        self.entry = entry
        self.insns = {}          # addr -> Insn (every instruction belonging to the function, delay slots included)
        self.leaders = {entry}
        self.delay = set()       # addresses that are some branch's delay slot
        self.jump_tables = {}    # addr of jr -> [targets]
        self.calls = set()
        self.tail = {}           # addr of j -> target (tail calls)


class Analyzer:
    def __init__(self, img, seeds):
        self.img = img
        self.funcs = {}
        self.seeds = set(seeds)
        self.pending = sorted(self.seeds)

    def run(self):
        while self.pending:
            e = self.pending.pop()
            if e in self.funcs or not self.img.has(e):
                continue
            self.funcs[e] = self.explore(e)
        return self.funcs

    def add_seed(self, a):
        if self.img.has(a) and a not in self.seeds:
            self.seeds.add(a)
            self.pending.append(a)

    def table_targets(self, f, jr_addr, reg):
        """Find a jump table feeding `jr reg` by scanning back for lui/addiu/lw constants."""
        img = self.img
        base = None
        lui = {}
        his = []
        pc = jr_addr - 4
        for _ in range(24):
            if pc < f.entry or pc not in f.insns:
                break
            i = f.insns[pc]
            if i.op == 'lui':
                lui.setdefault(i.rt, i.imm << 16)  # scanning backwards: keep the nearest definition of each register
                his.append(i.imm << 16)
            if i.op == 'lw' and i.rt == reg and base is None:
                base = ('lw', i.rs, i.simm)
            pc -= 4
        if not base:
            return []
        t = self.table_from_base_register(f, jr_addr, reg)
        if t:
            return t
        # find the register holding the table address: look for addiu/lui constants, a few forms
        _, breg, off = base
        pc = jr_addr - 4
        hi = None
        add_off = 0
        for _ in range(24):
            if pc not in f.insns:
                break
            i = f.insns[pc]
            if i.op == 'lui' and i.rt in lui and hi is None:
                hi = (i.rt, i.imm << 16)
            if i.op == 'addiu' and i.rt == i.rs and add_off == 0:
                pass
            pc -= 4
        cands = []
        for h in his:  # any recent lui constant: the table is validated below
            cands.append(h + off)
        for r, h in lui.items():
            cands.append(h + off)
            # lui + addiu (offset in a separate instruction)
            for a, i in f.insns.items():
                if i.op == 'addiu' and i.rs == r and a < jr_addr and jr_addr - a < 100:
                    cands.append(h + i.simm + off)
        for tbl in cands:
            tbl &= 0xFFFFFFFF
            if not img.has(tbl):
                continue
            out = []
            for k in range(256):
                a = tbl + 4 * k
                if not img.has(a):
                    break
                t = img.word(a)
                if not img.has(t) or abs(t - f.entry) > 0x10000:
                    break
                out.append(t)
            if len(out) >= 2:
                return out
        return []

    def table_from_base_register(self, f, jr_addr, reg):
        """`addu r, idx, base; lw r, 0(r); jr r` where `base` was loaded with lui/addiu earlier in the function
        (the compiler keeps the table address in a saved register). The entry count comes from the sltiu bound."""
        img = self.img
        lw = f.insns.get(jr_addr - 8)
        add = f.insns.get(jr_addr - 12)
        if not lw or not add or lw.op != 'lw' or lw.rt != reg or lw.rs != reg or add.op != 'addu' or add.rd != reg:
            return []
        count = 0
        for k in range(3, 12):
            i = f.insns.get(jr_addr - 4 * k)
            if i and i.op == 'sltiu':
                count = i.imm
                break
        # constants held in registers, tracked over the function's instructions in address order (a heuristic:
        # the table found this way is validated against the image below)
        consts = {}
        for a in sorted(f.insns):
            if a >= jr_addr:
                break
            i = f.insns[a]
            w = i.writes()
            if i.op == 'lui':
                consts[i.rt] = (i.imm << 16) & 0xFFFFFFFF
            elif i.op in ('addiu', 'ori') and i.rs in consts:
                consts[i.rt] = (consts[i.rs] + (i.simm if i.op == 'addiu' else i.imm)) & 0xFFFFFFFF
            elif w is not None:
                consts.pop(w, None)
        for breg in (add.rs, add.rt):
            best = (0, consts[breg]) if breg in consts else None
            if not best:
                continue
            tbl = (best[1] + lw.simm) & 0xFFFFFFFF
            if not img.has(tbl):
                continue
            out = []
            for k in range(count or 256):
                a = tbl + 4 * k
                if not img.has(a):
                    break
                t = img.word(a)
                if not img.has(t) or abs(t - f.entry) > 0x10000:
                    break
                out.append(t)
            if len(out) >= 2:
                return out
        return []

    def explore(self, entry):
        img = self.img
        f = Function(entry)
        work = [entry]
        seen = set()
        while work:
            pc = work.pop()
            while True:
                if pc in seen or not img.has(pc):
                    break
                seen.add(pc)
                i = img.insn(pc)
                f.insns[pc] = i
                if not i.has_delay:
                    pc += 4
                    continue
                d = img.insn(pc + 4) if img.has(pc + 4) else None
                if d is None:
                    break
                f.insns[pc + 4] = d
                f.delay.add(pc + 4)
                seen.add(pc + 4)
                op = i.op
                if op in BRANCHES:
                    t = i.branch_target()
                    f.leaders.add(t)
                    f.leaders.add(pc + 8)
                    work.append(t)
                    pc += 8
                    continue
                if op == 'jal':
                    self.add_seed(i.target)
                    f.calls.add(i.target)
                    f.leaders.add(pc + 8)
                    pc += 8
                    continue
                if op == 'jalr':
                    f.leaders.add(pc + 8)
                    pc += 8
                    continue
                if op == 'j':
                    t = i.target
                    if t in self.seeds and t != entry:
                        f.tail[pc] = t
                    else:
                        f.leaders.add(t)
                        work.append(t)
                    break
                if op == 'jr':
                    if i.rs != 31:
                        tg = self.table_targets(f, pc, i.rs)
                        if tg:
                            f.jump_tables[pc] = tg
                            for t in tg:
                                f.leaders.add(t)
                                work.append(t)
                    break
        return f

    def pointer_seeds(self, data_words):
        """Words in the image that look like function pointers (they point right after a return)."""
        img = self.img
        n = 0
        for k in range(0, len(img.data) - 3, 4):
            t = struct.unpack_from('<I', img.data, k)[0]
            if not img.has(t) or t in self.seeds:
                continue
            prev = img.word(t - 8) if img.has(t - 8) else 0
            first = img.word(t)
            if prev == 0x03E00008 or (first >> 16) == 0x27BD and (first & 0x8000):  # jr ra / addiu sp,sp,-N
                self.add_seed(t)
                n += 1
        return n

    def gap_seeds(self):
        """Code the flow analysis did not reach: a stack frame setup, or whatever follows a return."""
        img = self.img
        covered = set()
        ends = []
        for f in self.funcs.values():
            covered.update(f.insns)
            for a, i in f.insns.items():
                if i.op == 'jr' and i.rs == 31:
                    ends.append(a + 8)
        n = 0
        for a in range(img.base, img.end, 4):
            if a in covered:
                continue
            w = img.word(a)
            if (w >> 16) == 0x27BD and (w & 0x8000):  # addiu sp, sp, -N
                self.add_seed(a)
                n += 1
        for a in ends:
            if a not in covered and img.has(a) and img.word(a) != 0:
                self.add_seed(a)
                n += 1
        return n


# ------------------------------------------------------------------------------------------ emitter

def R(n):
    return '0u' if n == 0 else 'c->r[%d]' % n


class Emitter:
    def __init__(self, f, known, hooks):
        self.f, self.known, self.hooks = f, known, hooks
        self.lines = []
        self.hazards = 0

    def o(self, s):
        self.lines.append('    ' + s)

    def label(self, a):
        return 'L_%08X' % a

    def setreg(self, n, expr):
        return '' if n == 0 else 'c->r[%d] = %s;' % (n, expr)

    def alu(self, i):
        o, rs, rt, rd, sh = i.op, R(i.rs), R(i.rt), i.rd, i.sh
        s = self.setreg
        m = {
            'sll': lambda: s(rd, '%s << %d' % (rt, sh)),
            'srl': lambda: s(rd, '%s >> %d' % (rt, sh)),
            'sra': lambda: s(rd, '(uint32_t)((int32_t)%s >> %d)' % (rt, sh)),
            'sllv': lambda: s(rd, '%s << (%s & 31)' % (rt, rs)),
            'srlv': lambda: s(rd, '%s >> (%s & 31)' % (rt, rs)),
            'srav': lambda: s(rd, '(uint32_t)((int32_t)%s >> (%s & 31))' % (rt, rs)),
            'add': lambda: s(rd, '%s + %s' % (rs, rt)), 'addu': lambda: s(rd, '%s + %s' % (rs, rt)),
            'sub': lambda: s(rd, '%s - %s' % (rs, rt)), 'subu': lambda: s(rd, '%s - %s' % (rs, rt)),
            'and': lambda: s(rd, '%s & %s' % (rs, rt)), 'or': lambda: s(rd, '%s | %s' % (rs, rt)),
            'xor': lambda: s(rd, '%s ^ %s' % (rs, rt)), 'nor': lambda: s(rd, '~(%s | %s)' % (rs, rt)),
            'slt': lambda: s(rd, '(int32_t)%s < (int32_t)%s' % (rs, rt)),
            'sltu': lambda: s(rd, '%s < %s' % (rs, rt)),
            'addi': lambda: s(i.rt, '%s + (uint32_t)%d' % (rs, i.simm)),
            'addiu': lambda: s(i.rt, '%s + (uint32_t)%d' % (rs, i.simm)),
            'slti': lambda: s(i.rt, '(int32_t)%s < %d' % (rs, i.simm)),
            'sltiu': lambda: s(i.rt, '%s < (uint32_t)%d' % (rs, i.simm)),
            'andi': lambda: s(i.rt, '%s & 0x%Xu' % (rs, i.imm)),
            'ori': lambda: s(i.rt, '%s | 0x%Xu' % (rs, i.imm)),
            'xori': lambda: s(i.rt, '%s ^ 0x%Xu' % (rs, i.imm)),
            'lui': lambda: s(i.rt, '0x%Xu' % (i.imm << 16)),
            'mfhi': lambda: s(rd, 'c->hi'), 'mflo': lambda: s(rd, 'c->lo'),
            'mthi': lambda: 'c->hi = %s;' % rs, 'mtlo': lambda: 'c->lo = %s;' % rs,
            'mult': lambda: '{ int64_t p_ = (int64_t)(int32_t)%s * (int32_t)%s; c->lo = (uint32_t)p_; c->hi = (uint32_t)(p_ >> 32); }' % (rs, rt),
            'multu': lambda: '{ uint64_t p_ = (uint64_t)%s * %s; c->lo = (uint32_t)p_; c->hi = (uint32_t)(p_ >> 32); }' % (rs, rt),
            'div': lambda: 'psx_div(c, %s, %s);' % (rs, rt),
            'divu': lambda: 'psx_divu(c, %s, %s);' % (rs, rt),
        }
        return m[o]()

    def memop(self, i):
        o = i.op
        a = '%s + (uint32_t)%d' % (R(i.rs), i.simm)
        rt = i.rt
        if o == 'lw':
            return self.setreg(rt, 'rd32(c, %s)' % a) if rt else '(void)rd32(c, %s);' % a
        if o == 'lh':
            return self.setreg(rt, '(uint32_t)(int32_t)(int16_t)rd16(c, %s)' % a) if rt else '(void)rd16(c, %s);' % a
        if o == 'lhu':
            return self.setreg(rt, 'rd16(c, %s)' % a) if rt else '(void)rd16(c, %s);' % a
        if o == 'lb':
            return self.setreg(rt, '(uint32_t)(int32_t)(int8_t)rd8(c, %s)' % a) if rt else '(void)rd8(c, %s);' % a
        if o == 'lbu':
            return self.setreg(rt, 'rd8(c, %s)' % a) if rt else '(void)rd8(c, %s);' % a
        if o == 'sw':
            return 'wr32(c, %s, %s);' % (a, R(rt))
        if o == 'sh':
            return 'wr16(c, %s, %s);' % (a, R(rt))
        if o == 'sb':
            return 'wr8(c, %s, %s);' % (a, R(rt))
        if o == 'lwl':
            return self.setreg(rt, 'psx_lwl(c, %s, %s)' % (a, R(rt)))
        if o == 'lwr':
            return self.setreg(rt, 'psx_lwr(c, %s, %s)' % (a, R(rt)))
        if o == 'swl':
            return 'psx_swl(c, %s, %s);' % (a, R(rt))
        if o == 'swr':
            return 'psx_swr(c, %s, %s);' % (a, R(rt))
        if o == 'lwc2':
            return 'gte_write_data(c, %d, rd32(c, %s));' % (rt, a)
        if o == 'swc2':
            return 'wr32(c, %s, gte_read_data(c, %d));' % (a, rt)
        raise ValueError(o)

    def plain(self, i):
        """C for one non-control instruction (no delay slot handling)."""
        o = i.op
        if o in ('lw', 'lh', 'lhu', 'lb', 'lbu', 'sw', 'sh', 'sb', 'lwl', 'lwr', 'swl', 'swr', 'lwc2', 'swc2'):
            return self.memop(i)
        if o == 'mfc0':
            return self.setreg(i.rt, 'cop0_read(c, %d)' % i.rd)
        if o == 'mtc0':
            return 'cop0_write(c, %d, %s);' % (i.rd, R(i.rt))
        if o == 'rfe':
            return 'cop0_rfe(c);'
        if o == 'mfc2':
            return self.setreg(i.rt, 'gte_read_data(c, %d)' % i.rd)
        if o == 'cfc2':
            return self.setreg(i.rt, 'gte_read_ctrl(c, %d)' % i.rd)
        if o == 'mtc2':
            return 'gte_write_data(c, %d, %s);' % (i.rd, R(i.rt))
        if o == 'ctc2':
            return 'gte_write_ctrl(c, %d, %s);' % (i.rd, R(i.rt))
        if o == 'cop2':
            return 'gte_command(c, 0x%X);' % i.cop
        if o == 'syscall':
            return 'psx_syscall(c, 0x%08Xu);' % i.addr
        if o == 'break':
            return 'psx_break(c, 0x%08Xu, %d);' % (i.addr, (i.w >> 6) & 0xFFFFF)
        if o == '?':
            return 'psx_illegal(c, 0x%08Xu, 0x%08Xu);' % (i.addr, i.w)
        return self.alu(i)

    def delay_slot(self, d):
        if d.has_delay:
            return 'psx_illegal(c, 0x%08Xu, 0x%08Xu);' % (d.addr, d.w)
        return self.plain(d)

    def cond(self, i):
        rs, rt = R(i.rs), R(i.rt)
        return {'beq': '%s == %s' % (rs, rt), 'bne': '%s != %s' % (rs, rt), 'blez': '(int32_t)%s <= 0' % rs,
                'bgtz': '(int32_t)%s > 0' % rs, 'bltz': '(int32_t)%s < 0' % rs, 'bgez': '(int32_t)%s >= 0' % rs,
                'bltzal': '(int32_t)%s < 0' % rs, 'bgezal': '(int32_t)%s >= 0' % rs}[i.op]

    def call(self, target):
        if target in self.known:
            return 'f_%08X(c);' % target
        return 'psx_call(c, 0x%08Xu);' % target

    def run(self):
        f = self.f
        addrs = sorted(f.insns)
        self.o('PSX_TRACE(c, 0x%08XU);' % f.entry)
        for hk in self.hooks.get(f.entry, []):
            self.o(hk)
        nxt = {a: (addrs[k + 1] if k + 1 < len(addrs) else None) for k, a in enumerate(addrs)}
        for a in addrs:
            i = f.insns[a]
            standalone = a not in f.delay or a in f.leaders
            if a in f.leaders and a != f.entry or a == f.entry:
                self.lines.append('%s:;' % self.label(a))
                self.o('PSX_TICK(c, %d);' % self.block_len(a))
            if not standalone:
                continue
            self.o('// %08X %s' % (a, i.op))
            for hk in self.hooks.get(('insn', a), []):
                self.o(hk)
            self.insn(a, i, addrs, nxt)
        # a function that runs off its recovered flow: let the dispatcher continue
        last = addrs[-1]
        self.o('psx_fell_off(c, 0x%08Xu);' % (last + 4))
        return self.lines

    def block_len(self, a):
        n = 0
        f = self.f
        while a in f.insns:
            n += 1
            i = f.insns[a]
            if i.has_delay:
                n += 1
                break
            a += 4
            if a in f.leaders:
                break
        return n

    def fallthrough(self, a, nxt_addr):
        """After a conditional or call, continue at a (the next executed instruction)."""
        if a in self.f.insns:
            if nxt_addr != a or a in self.f.delay and a not in self.f.leaders:
                self.o('goto %s;' % self.label(a))
        else:
            self.o('psx_fell_off(c, 0x%08Xu);' % a)
            self.o('return;')

    def insn(self, a, i, addrs, nxt):
        f = self.f
        op = i.op
        if not i.has_delay:
            code = self.plain(i)
            # load delay slot: the next instruction still sees the old value of the loaded register
            if op in LOADS and i.rt and (a + 4) in f.insns and (a + 4) not in f.delay:
                n = f.insns[a + 4]
                if i.rt in n.reads() and not n.has_delay and n.op not in ('lwl', 'lwr'):
                    self.hazards += 1
                    self.o('{ uint32_t ld_ = %s;' % code.split('= ', 1)[1].rstrip(';'))
                    self.o('  ' + self.plain(n))
                    self.o('  c->r[%d] = ld_; }' % i.rt)
                    self.o('goto %s;' % self.label(a + 8) if (a + 8) in f.insns else 'psx_fell_off(c, 0x%08Xu);' % (a + 8))
                    return
            if code:
                self.o(code)
            return
        d = f.insns[a + 4]
        dly = self.delay_slot(d)
        after = a + 8
        if op in BRANCHES:
            self.o('{ const int take_ = %s;' % self.cond(i))
            if op in ('bltzal', 'bgezal'):
                self.o('  c->r[31] = 0x%08Xu;' % after)
            if dly:
                self.o('  ' + dly)
            t = i.branch_target()
            self.o('  if (take_) { %s } }' % ('goto %s;' % self.label(t) if t in f.insns else 'psx_jump(c, 0x%08Xu); return;' % t))
            self.fallthrough(after, nxt.get(a + 4) if (a + 4) in nxt else None)
            return
        if op == 'jal':
            self.o('c->r[31] = 0x%08Xu;' % after)
            if dly:
                self.o(dly)
            self.o(self.call(i.target))
            self.fallthrough(after, nxt.get(a + 4))
            return
        if op == 'jalr':
            self.o('{ const uint32_t tgt_ = %s;' % R(i.rs))
            self.o('  %s' % self.setreg(i.rd, '0x%08Xu' % after))
            if dly:
                self.o('  ' + dly)
            self.o('  psx_call(c, tgt_); }')
            self.fallthrough(after, nxt.get(a + 4))
            return
        if op == 'j':
            if dly:
                self.o(dly)
            if a in f.tail:
                self.o('f_%08X(c); return;' % f.tail[a] if f.tail[a] in self.known else 'psx_jump(c, 0x%08Xu); return;' % f.tail[a])
            else:
                self.o('goto %s;' % self.label(i.target) if i.target in f.insns else 'psx_jump(c, 0x%08Xu); return;' % i.target)
            return
        if op == 'jr':
            self.o('{ const uint32_t tgt_ = %s;' % R(i.rs))
            if dly:
                self.o('  ' + dly)
            if i.rs == 31:
                self.o('  (void)tgt_; PSX_RETURN(c, tgt_); }')
                return
            if a in f.jump_tables:
                self.o('  switch (tgt_) {')
                for t in sorted(set(f.jump_tables[a])):
                    if t in f.insns:
                        self.o('  case 0x%08Xu: goto %s;' % (t, self.label(t)))
                self.o('  default: psx_jump(c, tgt_); return; } }')
            else:
                self.o('  psx_jump(c, tgt_); return; }')
            return
        raise ValueError(op)


def emit_all(funcs, img, out, prefix, hooks):
    os.makedirs(out, exist_ok=True)
    known = set(funcs)
    names = sorted(funcs)
    hazards = 0
    header = ['// generated by psxrecomp. Do not edit.', '#pragma once', '#include "psxrt.h"', '']
    header += ['void f_%08X(Cpu* c);' % e for e in names]
    open(os.path.join(out, prefix + '_funcs.h'), 'w').write('\n'.join(header) + '\n')
    per = 120
    files = 0
    for start in range(0, len(names), per):
        parts = ['// generated by psxrecomp. Do not edit.', '#include "%s_funcs.h"' % prefix, '']
        for e in names[start:start + per]:
            em = Emitter(funcs[e], known, hooks)
            body = em.run()
            hazards += em.hazards
            parts.append('void f_%08X(Cpu* c) {' % e)
            parts += body
            parts.append('}')
            parts.append('')
        open(os.path.join(out, '%s_%03d.c' % (prefix, files)), 'w').write('\n'.join(parts))
        files += 1
    t = ['// generated by psxrecomp. Do not edit.', '#include "%s_funcs.h"' % prefix, '',
         'const PsxFunc %s_table[] = {' % prefix]
    t += ['    {0x%08Xu, f_%08X},' % (e, e) for e in names]
    t += ['};', 'const unsigned %s_count = %d;' % (prefix, len(names)),
          'const unsigned %s_base = 0x%08Xu;' % (prefix, img.base),
          'const unsigned %s_size = 0x%X;' % (prefix, len(img.data))]
    # a checksum of the code the table was built from, so the runtime can refuse different code
    h = 2166136261
    for b in img.data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    t.append('const unsigned %s_hash = 0x%08X;' % (prefix, h))
    open(os.path.join(out, prefix + '_table.c'), 'w').write('\n'.join(t) + '\n')
    for p in os.listdir(out):
        if p.startswith(prefix + '_') and p.endswith('.c') and p[len(prefix) + 1:-2].isdigit() and int(p[len(prefix) + 1:-2]) >= files:
            os.remove(os.path.join(out, p))
    return files, hazards


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--out', default='generated')
    ap.add_argument('--prefix', default='main')
    ap.add_argument('--hooks', help='python file defining HOOKS = {addr or ("insn", addr): [c statements]}')
    args = ap.parse_args()
    pc, gp, img = load_exe(args.exe)
    hooks = {}
    if args.hooks:
        ns = {}
        exec(open(args.hooks).read(), ns)
        hooks = ns.get('HOOKS', {})
    an = Analyzer(img, [pc])
    an.run()
    for _ in range(6):
        n = an.pointer_seeds(None) + an.gap_seeds()
        if not n:
            break
        an.run()
    files, hazards = emit_all(an.funcs, img, args.out, args.prefix, hooks)
    covered = set()
    for f in an.funcs.values():
        covered.update(f.insns)
    tables = sum(len(f.jump_tables) for f in an.funcs.values())
    print('%d functions, %d jump tables, %d/%d instructions covered, %d load-delay hazards handled, %d files' % (
        len(an.funcs), tables, len(covered), len(img.data) // 4, hazards, files))
    # indirect jumps with no table: the runtime has to guess where they go, which is only safe for the BIOS call
    # stubs (`li t2, 0xA0; jr t2`) and tail calls through a function pointer. Anything else listed here is probably a jump table the analysis missed.
    stray = []
    for f in an.funcs.values():
        for addr, i in f.insns.items():
            if i.op == 'jr' and i.rs != 31 and addr not in f.jump_tables:
                prev = f.insns.get(addr - 4) or f.insns.get(addr - 8)
                stub = any((f.insns.get(addr - k) is not None and f.insns[addr - k].op in ('addiu', 'ori')
                            and f.insns[addr - k].rt == i.rs and f.insns[addr - k].imm in (0xA0, 0xB0, 0xC0)) for k in (4, 8))
                fnptr = False  # `lui t, hi; lw t, lo(t); jr t`: a call through a pointer variable
                for k in (4, 8):
                    lw = f.insns.get(addr - k)
                    lu = f.insns.get(addr - k - 4)
                    if lw is not None and lw.op == 'lw' and lw.rt == i.rs and lw.rs == lw.rt and lu is not None \
                            and lu.op == 'lui' and lu.rt == lw.rt:
                        fnptr = True
                if not stub and not fnptr:
                    stray.append(addr)
    if stray:
        print('warning: %d indirect jumps without a table (jump tables missed by the analysis?): %s' % (
            len(stray), ' '.join('%08X' % a for a in sorted(stray)[:20])))
    open(os.path.join(args.out, args.prefix + '_info.txt'), 'w').write('entry %08x gp %08x base %08x size %x\n' % (pc, gp, img.base, len(img.data)))


if __name__ == '__main__':
    main()
