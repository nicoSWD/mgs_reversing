// The machine core: guest memory, coprocessor 0, the scheduler, interrupt delivery, function dispatch and
// the interpreter that runs whatever the recompiler did not see.
#include <csetjmp>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "runtime.h"

uint8_t psx_ram[PSX_RAM_SIZE];
uint8_t psx_scratch[1024];
int psx_cache_isolated = 0;

bool g_log_bios, g_log_irq, g_log_cd, g_log_gpu, g_log_dma, g_log_pad;

void fatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "FATAL: ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
    std::fflush(stderr);
    std::abort();
}

void logf(const char* cat, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "[%s f%d] ", cat, host_frame_number());
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
}

// ------------------------------------------------------------------------------------------ cop0

extern "C" uint32_t cop0_read(Cpu* c, int n) {
    if (n == 13) {  // CAUSE: IP2 mirrors the interrupt line
        uint32_t v = c->cop0[13] & ~0x400u;
        if (irq_pending(c)) v |= 0x400;
        return v;
    }
    if (n == 15) return 2;  // PRID
    return c->cop0[n];
}

extern "C" void cop0_write(Cpu* c, int n, uint32_t v) {
    if (n == 12) {
        c->cop0[12] = v;
        psx_cache_isolated = (v >> 16) & 1;
        c->next_event = 0;  // interrupts may have become deliverable
        return;
    }
    if (n == 13) {
        c->cop0[13] = (c->cop0[13] & ~0x300u) | (v & 0x300u);
        return;
    }
    c->cop0[n] = v;
}

extern "C" void cop0_rfe(Cpu* c) {
    uint32_t sr = c->cop0[12];
    c->cop0[12] = (sr & ~0xFu) | ((sr >> 2) & 0xFu);
    c->next_event = 0;
}

// -------------------------------------------------------------------------------------- scheduler

static uint64_t g_ev_time[EV_COUNT];
static bool g_ev_on[EV_COUNT];

void sched_set(Cpu* c, Ev e, uint64_t when) {
    g_ev_time[e] = when;
    g_ev_on[e] = true;
    sched_recompute(c);
}
void sched_clear(Cpu* c, Ev e) {
    g_ev_on[e] = false;
    sched_recompute(c);
}
void sched_recompute(Cpu* c) {
    uint64_t best = UINT64_MAX;
    for (int i = 0; i < EV_COUNT; i++)
        if (g_ev_on[i] && g_ev_time[i] < best) best = g_ev_time[i];
    c->next_event = best;
}
uint64_t sched_now(Cpu* c) { return c->cycles; }

void hw_run_event(Cpu* c, int ev);

static int g_poll_depth = 0;

extern "C" void psx_poll(Cpu* c) {
    g_poll_depth++;
    for (;;) {
        int due = -1;
        uint64_t best = UINT64_MAX;
        for (int i = 0; i < EV_COUNT; i++)
            if (g_ev_on[i] && g_ev_time[i] <= c->cycles && g_ev_time[i] < best) { best = g_ev_time[i]; due = i; }
        if (due < 0) break;
        g_ev_on[due] = false;
        hw_run_event(c, due);
    }
    sched_recompute(c);
    if (g_poll_depth == 1 && irq_deliverable(c) && !bios_in_exception()) bios_take_interrupt(c);
    g_poll_depth--;
    sched_recompute(c);
}

int core_poll_depth() { return g_poll_depth; }
void core_set_poll_depth(int d) { g_poll_depth = d; }

// ------------------------------------------------------------------------------------ interrupts

static uint32_t g_istat = 0, g_imask = 0;
uint32_t irq_stat() { return g_istat; }
uint32_t irq_mask() { return g_imask; }
void irq_set_stat(uint32_t v) { g_istat &= v; }
void irq_set_mask(uint32_t v) { g_imask = v & 0x7FF; }
void irq_raise(Cpu* c, uint32_t bits) {
    g_istat |= bits;
    LOG(irq, "raise %03X stat=%03X mask=%03X", bits, g_istat, g_imask);
    c->next_event = 0;
}
bool irq_pending(Cpu*) { return (g_istat & g_imask) != 0; }
bool irq_deliverable(Cpu* c) { return (g_istat & g_imask) && (c->cop0[12] & 1) && (c->cop0[12] & 0x400); }

// ---------------------------------------------------------------------------------------- syscall

extern "C" void psx_syscall(Cpu* c, uint32_t pc) {
    uint32_t fn = c->r[4];
    uint32_t& sr = c->cop0[12];
    if (fn == 1) {  // EnterCriticalSection
        bool was = (sr & 0x401) == 0x401;
        sr &= ~0x401u;
        c->r[2] = was ? 1 : 0;
    } else if (fn == 2) {  // ExitCriticalSection
        sr |= 0x401;
        c->next_event = 0;
    } else {
        LOG(bios, "syscall(%u) at %08X ignored", fn, pc);
    }
}

extern "C" void psx_break(Cpu*, uint32_t pc, uint32_t code) { LOG(bios, "break %u at %08X", code, pc); }
extern "C" void psx_illegal(Cpu* c, uint32_t pc, uint32_t word) {
    fatal("illegal instruction %08X at %08X (ra=%08X)", word, pc, c->r[31]);
}

// ------------------------------------------------------------------------------------- dispatcher

static std::unordered_map<uint32_t, PsxFnPtr> g_funcs;
static std::unordered_map<uint32_t, void (*)(Cpu*)> g_overrides;

extern "C" void rt_register_table(const PsxFunc* t, unsigned n) {
    for (unsigned i = 0; i < n; i++) g_funcs[t[i].addr] = t[i].fn;
}
void rt_override(uint32_t addr, void (*fn)(Cpu*)) { g_overrides[addr] = fn; }

static inline uint32_t norm(uint32_t a) { return (a & 0x1FFFFFFFu) | 0x80000000u; }

static void interp_run(Cpu* c, uint32_t pc, uint32_t stop_ra);
uint32_t g_last_interp_pc = 0;
#ifdef PSX_TRACE_CALLS
extern "C" void psx_trace_enter(Cpu* c, uint32_t addr);
#endif

extern "C" void psx_call(Cpu* c, uint32_t addr) {
    uint32_t p = addr & 0x1FFFFFFFu;
    if (p == 0xA0 || p == 0xB0 || p == 0xC0) {
        bios_call(c, p, c->r[9]);
        return;
    }
    uint32_t a = norm(addr);
    if (g_log_irq && bios_in_exception()) logf("irq", "  call %08X ra=%08X a0=%08X", a, c->r[31], c->r[4]);
    if (!g_overrides.empty()) {
        auto o = g_overrides.find(a);
        if (o != g_overrides.end()) { o->second(c); return; }
    }
    auto it = g_funcs.find(a);
    if (it != g_funcs.end()) { it->second(c); return; }
    interp_run(c, a, c->r[31]);
}
extern "C" void psx_jump(Cpu* c, uint32_t addr) { psx_call(c, addr); }
extern "C" void psx_fell_off(Cpu* c, uint32_t addr) { psx_call(c, addr); }

extern "C" uint32_t rt_guest_call(Cpu* c, uint32_t addr, uint32_t a0) {
    c->r[4] = a0;
    c->r[31] = 0;
    psx_call(c, addr);
    return c->r[2];
}

// ------------------------------------------------------------------------------------ interpreter

static inline int32_t sx16(uint32_t v) { return (int16_t)v; }

static void interp_run(Cpu* c, uint32_t pc, uint32_t stop_ra) {
    uint32_t ld_reg = 0, ld_val = 0;
    for (;;) {
        // arrive at a boundary: calls into known code and BIOS vectors
        if (pc == stop_ra) return;
        uint32_t phys = pc & 0x1FFFFFFFu;
        if (phys == 0xA0 || phys == 0xB0 || phys == 0xC0) {
            bios_call(c, phys, c->r[9]);
            pc = c->r[31];
            continue;
        }
        PSX_TICK(c, 1);
        g_last_interp_pc = pc;
#ifdef PSX_TRACE_CALLS
        { extern bool trace_in_range(); if (trace_in_range()) fprintf(stderr, "ITRACE f%d %08X ra=%08X stop=%08X\n", host_frame_number(), pc, c->r[31], stop_ra); }
#endif
        uint32_t w = rd32(c, pc);
        uint32_t next = pc + 4;
        uint32_t op = w >> 26, rs = (w >> 21) & 31, rt = (w >> 16) & 31, rd = (w >> 11) & 31, sh = (w >> 6) & 31;
        uint32_t imm = w & 0xFFFF;
        int32_t simm = sx16(imm);
        uint32_t* r = c->r;
        // pending load from the previous instruction commits after this one executes
        uint32_t pl_reg = ld_reg, pl_val = ld_val;
        ld_reg = 0;
        bool is_branch = false;
        bool taken = false;
        uint32_t target = 0;
        bool link = false;
        uint32_t link_reg = 31;
        auto setr = [&](uint32_t n, uint32_t v) { if (n) { r[n] = v; if (n == pl_reg) pl_reg = 0; } };
        auto load = [&](uint32_t n, uint32_t v) { if (n) { ld_reg = n; ld_val = v; } };
        switch (op) {
            case 0:
                switch (w & 63) {
                    case 0x00: setr(rd, r[rt] << sh); break;
                    case 0x02: setr(rd, r[rt] >> sh); break;
                    case 0x03: setr(rd, (uint32_t)((int32_t)r[rt] >> sh)); break;
                    case 0x04: setr(rd, r[rt] << (r[rs] & 31)); break;
                    case 0x06: setr(rd, r[rt] >> (r[rs] & 31)); break;
                    case 0x07: setr(rd, (uint32_t)((int32_t)r[rt] >> (r[rs] & 31))); break;
                    case 0x08: is_branch = true; taken = true; target = r[rs]; break;
                    case 0x09: is_branch = true; taken = true; target = r[rs]; link = true; link_reg = rd; break;
                    case 0x0C: psx_syscall(c, pc); break;
                    case 0x0D: psx_break(c, pc, (w >> 6) & 0xFFFFF); break;
                    case 0x10: setr(rd, c->hi); break;
                    case 0x11: c->hi = r[rs]; break;
                    case 0x12: setr(rd, c->lo); break;
                    case 0x13: c->lo = r[rs]; break;
                    case 0x18: { int64_t p = (int64_t)(int32_t)r[rs] * (int32_t)r[rt]; c->lo = (uint32_t)p; c->hi = (uint32_t)(p >> 32); break; }
                    case 0x19: { uint64_t p = (uint64_t)r[rs] * r[rt]; c->lo = (uint32_t)p; c->hi = (uint32_t)(p >> 32); break; }
                    case 0x1A: psx_div(c, r[rs], r[rt]); break;
                    case 0x1B: psx_divu(c, r[rs], r[rt]); break;
                    case 0x20: case 0x21: setr(rd, r[rs] + r[rt]); break;
                    case 0x22: case 0x23: setr(rd, r[rs] - r[rt]); break;
                    case 0x24: setr(rd, r[rs] & r[rt]); break;
                    case 0x25: setr(rd, r[rs] | r[rt]); break;
                    case 0x26: setr(rd, r[rs] ^ r[rt]); break;
                    case 0x27: setr(rd, ~(r[rs] | r[rt])); break;
                    case 0x2A: setr(rd, (int32_t)r[rs] < (int32_t)r[rt]); break;
                    case 0x2B: setr(rd, r[rs] < r[rt]); break;
                    default: psx_illegal(c, pc, w);
                }
                break;
            case 1: {
                bool ge = rt & 1;
                bool neg = (int32_t)r[rs] < 0;
                is_branch = true;
                taken = ge ? !neg : neg;
                target = pc + 4 + (simm << 2);
                if (rt & 0x10) { link = true; link_reg = 31; }
                break;
            }
            case 2: is_branch = true; taken = true; target = ((pc + 4) & 0xF0000000u) | ((w & 0x3FFFFFF) << 2); break;
            case 3: is_branch = true; taken = true; target = ((pc + 4) & 0xF0000000u) | ((w & 0x3FFFFFF) << 2); link = true; break;
            case 4: is_branch = true; taken = r[rs] == r[rt]; target = pc + 4 + (simm << 2); break;
            case 5: is_branch = true; taken = r[rs] != r[rt]; target = pc + 4 + (simm << 2); break;
            case 6: is_branch = true; taken = (int32_t)r[rs] <= 0; target = pc + 4 + (simm << 2); break;
            case 7: is_branch = true; taken = (int32_t)r[rs] > 0; target = pc + 4 + (simm << 2); break;
            case 8: case 9: setr(rt, r[rs] + (uint32_t)simm); break;
            case 10: setr(rt, (int32_t)r[rs] < simm); break;
            case 11: setr(rt, r[rs] < (uint32_t)simm); break;
            case 12: setr(rt, r[rs] & imm); break;
            case 13: setr(rt, r[rs] | imm); break;
            case 14: setr(rt, r[rs] ^ imm); break;
            case 15: setr(rt, imm << 16); break;
            case 0x10:
                if (rs == 0) setr(rt, cop0_read(c, rd));
                else if (rs == 4) cop0_write(c, rd, r[rt]);
                else if ((w & 63) == 0x10) cop0_rfe(c);
                break;
            case 0x12:
                if (w & (1u << 25)) gte_command(c, w & 0x1FFFFFF);
                else if (rs == 0) setr(rt, gte_read_data(c, rd));
                else if (rs == 2) setr(rt, gte_read_ctrl(c, rd));
                else if (rs == 4) gte_write_data(c, rd, r[rt]);
                else if (rs == 6) gte_write_ctrl(c, rd, r[rt]);
                break;
            case 0x20: load(rt, (uint32_t)(int32_t)(int8_t)rd8(c, r[rs] + simm)); break;
            case 0x21: load(rt, (uint32_t)(int32_t)(int16_t)rd16(c, r[rs] + simm)); break;
            case 0x22: { uint32_t cur = (pl_reg == rt) ? pl_val : r[rt]; load(rt, psx_lwl(c, r[rs] + simm, cur)); break; }
            case 0x23: load(rt, rd32(c, r[rs] + simm)); break;
            case 0x24: load(rt, rd8(c, r[rs] + simm)); break;
            case 0x25: load(rt, rd16(c, r[rs] + simm)); break;
            case 0x26: { uint32_t cur = (pl_reg == rt) ? pl_val : r[rt]; load(rt, psx_lwr(c, r[rs] + simm, cur)); break; }
            case 0x28: wr8(c, r[rs] + simm, r[rt]); break;
            case 0x29: wr16(c, r[rs] + simm, r[rt]); break;
            case 0x2A: psx_swl(c, r[rs] + simm, r[rt]); break;
            case 0x2B: wr32(c, r[rs] + simm, r[rt]); break;
            case 0x2E: psx_swr(c, r[rs] + simm, r[rt]); break;
            case 0x32: gte_write_data(c, rt, rd32(c, r[rs] + simm)); break;
            case 0x3A: wr32(c, r[rs] + simm, gte_read_data(c, rt)); break;
            default: psx_illegal(c, pc, w);
        }
        if (pl_reg) r[pl_reg] = pl_val;
        if (!is_branch) { pc = next; continue; }

        // branch: link, then execute the delay slot, then transfer control
        if (link && link_reg) r[link_reg] = pc + 8;
        uint32_t dpc = pc + 4;
        uint32_t dw = rd32(c, dpc);
        {
            // delay slot: executed as a plain instruction (it cannot itself branch)
            uint32_t dop = dw >> 26, drs = (dw >> 21) & 31, drt = (dw >> 16) & 31, drd = (dw >> 11) & 31, dsh = (dw >> 6) & 31;
            uint32_t dimm = dw & 0xFFFF;
            int32_t dsimm = sx16(dimm);
            uint32_t pl2_reg = ld_reg, pl2_val = ld_val;
            ld_reg = 0;
            auto sr2 = [&](uint32_t n, uint32_t v) { if (n) { r[n] = v; if (n == pl2_reg) pl2_reg = 0; } };
            auto ld2 = [&](uint32_t n, uint32_t v) { if (n) { ld_reg = n; ld_val = v; } };
            switch (dop) {
                case 0:
                    switch (dw & 63) {
                        case 0x00: sr2(drd, r[drt] << dsh); break;
                        case 0x02: sr2(drd, r[drt] >> dsh); break;
                        case 0x03: sr2(drd, (uint32_t)((int32_t)r[drt] >> dsh)); break;
                        case 0x04: sr2(drd, r[drt] << (r[drs] & 31)); break;
                        case 0x06: sr2(drd, r[drt] >> (r[drs] & 31)); break;
                        case 0x07: sr2(drd, (uint32_t)((int32_t)r[drt] >> (r[drs] & 31))); break;
                        case 0x10: sr2(drd, c->hi); break;
                        case 0x11: c->hi = r[drs]; break;
                        case 0x12: sr2(drd, c->lo); break;
                        case 0x13: c->lo = r[drs]; break;
                        case 0x18: { int64_t p = (int64_t)(int32_t)r[drs] * (int32_t)r[drt]; c->lo = (uint32_t)p; c->hi = (uint32_t)(p >> 32); break; }
                        case 0x19: { uint64_t p = (uint64_t)r[drs] * r[drt]; c->lo = (uint32_t)p; c->hi = (uint32_t)(p >> 32); break; }
                        case 0x1A: psx_div(c, r[drs], r[drt]); break;
                        case 0x1B: psx_divu(c, r[drs], r[drt]); break;
                        case 0x20: case 0x21: sr2(drd, r[drs] + r[drt]); break;
                        case 0x22: case 0x23: sr2(drd, r[drs] - r[drt]); break;
                        case 0x24: sr2(drd, r[drs] & r[drt]); break;
                        case 0x25: sr2(drd, r[drs] | r[drt]); break;
                        case 0x26: sr2(drd, r[drs] ^ r[drt]); break;
                        case 0x27: sr2(drd, ~(r[drs] | r[drt])); break;
                        case 0x2A: sr2(drd, (int32_t)r[drs] < (int32_t)r[drt]); break;
                        case 0x2B: sr2(drd, r[drs] < r[drt]); break;
                        case 0x0C: psx_syscall(c, dpc); break;
                        default: break;
                    }
                    break;
                case 8: case 9: sr2(drt, r[drs] + (uint32_t)dsimm); break;
                case 10: sr2(drt, (int32_t)r[drs] < dsimm); break;
                case 11: sr2(drt, r[drs] < (uint32_t)dsimm); break;
                case 12: sr2(drt, r[drs] & dimm); break;
                case 13: sr2(drt, r[drs] | dimm); break;
                case 14: sr2(drt, r[drs] ^ dimm); break;
                case 15: sr2(drt, dimm << 16); break;
                case 0x10:
                    if (drs == 0) sr2(drt, cop0_read(c, drd));
                    else if (drs == 4) cop0_write(c, drd, r[drt]);
                    else if ((dw & 63) == 0x10) cop0_rfe(c);
                    break;
                case 0x12:
                    if (dw & (1u << 25)) gte_command(c, dw & 0x1FFFFFF);
                    else if (drs == 0) sr2(drt, gte_read_data(c, drd));
                    else if (drs == 2) sr2(drt, gte_read_ctrl(c, drd));
                    else if (drs == 4) gte_write_data(c, drd, r[drt]);
                    else if (drs == 6) gte_write_ctrl(c, drd, r[drt]);
                    break;
                case 0x20: ld2(drt, (uint32_t)(int32_t)(int8_t)rd8(c, r[drs] + dsimm)); break;
                case 0x21: ld2(drt, (uint32_t)(int32_t)(int16_t)rd16(c, r[drs] + dsimm)); break;
                case 0x22: ld2(drt, psx_lwl(c, r[drs] + dsimm, r[drt])); break;
                case 0x23: ld2(drt, rd32(c, r[drs] + dsimm)); break;
                case 0x24: ld2(drt, rd8(c, r[drs] + dsimm)); break;
                case 0x25: ld2(drt, rd16(c, r[drs] + dsimm)); break;
                case 0x26: ld2(drt, psx_lwr(c, r[drs] + dsimm, r[drt])); break;
                case 0x28: wr8(c, r[drs] + dsimm, r[drt]); break;
                case 0x29: wr16(c, r[drs] + dsimm, r[drt]); break;
                case 0x2A: psx_swl(c, r[drs] + dsimm, r[drt]); break;
                case 0x2B: wr32(c, r[drs] + dsimm, r[drt]); break;
                case 0x2E: psx_swr(c, r[drs] + dsimm, r[drt]); break;
                case 0x32: gte_write_data(c, drt, rd32(c, r[drs] + dsimm)); break;
                case 0x3A: wr32(c, r[drs] + dsimm, gte_read_data(c, drt)); break;
                default: break;
            }
            if (pl2_reg) r[pl2_reg] = pl2_val;
        }
        PSX_TICK(c, 1);
        if (!taken) { pc = pc + 8; continue; }
        if (link) {
            // a call: known code (recompiled or an override) runs natively, then we continue after the delay slot
            uint32_t tp = target & 0x1FFFFFFFu;
            if (tp == 0xA0 || tp == 0xB0 || tp == 0xC0 || g_funcs.count(norm(target)) || g_overrides.count(norm(target))) {
                psx_call(c, target);
                pc = pc + 8;
                continue;
            }
        }
        if (!(w >> 26 == 0 && (w & 63) == 8 && rs == 31)) {
            // j / jr rX to known code is a tail call: run it, then return to our caller the way it would have
            uint32_t tp = target & 0x1FFFFFFFu;
            if (tp == 0xA0 || tp == 0xB0 || tp == 0xC0 || g_funcs.count(norm(target)) || g_overrides.count(norm(target))) {
                psx_call(c, target);
                pc = c->r[31];
                continue;
            }
        }
        if (link) {
#ifdef PSX_TRACE_CALLS
            psx_trace_enter(c, target);
#endif
        }
        pc = target;
    }
}

// ------------------------------------------------------------------------------------------- tracing
#ifdef PSX_TRACE_CALLS
#include <csignal>
#include <unordered_set>
static std::unordered_set<uint32_t> g_seen;
static std::vector<uint32_t> g_first;
static uint32_t g_trace[8192], g_trace_ra[8192];
static uint32_t g_trace_pos = 0;
static uint32_t g_skip = 0;
extern "C" void psx_trace_enter2(Cpu* c, uint32_t addr);
static bool count_hit(uint32_t addr) {
    static std::unordered_set<uint32_t> watch;
    static bool init = false;
    if (!init) {
        init = true;
        if (const char* e = getenv("PSX_WATCH")) {  // comma separated hex addresses: print each entry with the frame number
            const char* p = e;
            while (*p) { watch.insert((uint32_t)strtoul(p, (char**)&p, 16)); if (*p == ',') p++; }
        }
    }
    return watch.count(addr) != 0;
}
bool trace_in_range() {
    static int lo = -1, hi = -1;
    static bool init = false;
    if (!init) { init = true; if (const char* e = getenv("PSX_TRACE_RANGE")) sscanf(e, "%d,%d", &lo, &hi); }
    int f = host_frame_number();
    return lo >= 0 && f >= lo && f <= hi;
}
extern "C" void psx_trace_enter(Cpu* c, uint32_t addr) { if (trace_in_range()) fprintf(stderr, "ENTER f%d %08X ra=%08X\n", host_frame_number(), addr, c->r[31]); if (count_hit(addr)) fprintf(stderr, "WATCH f%d %08X ra=%08X a0=%08X\n", host_frame_number(), addr, c->r[31], c->r[4]); psx_trace_enter2(c, addr); }
extern "C" void psx_trace_enter2(Cpu* c, uint32_t addr) { if (!g_skip) { const char* s = getenv("PSX_TRACE_SKIP"); g_skip = s ? (uint32_t)strtoul(s, nullptr, 16) : 1; } if (addr == g_skip) return; if (g_seen.insert(addr).second) g_first.push_back(addr); g_trace_ra[g_trace_pos & 8191] = c->r[31]; g_trace[g_trace_pos++ & 8191] = addr; }
void trace_dump(int n) {
    fprintf(stderr, "--- recent functions entered (run-length) ---\n");
    uint32_t prev = 0; int run = 0;
    int shown = 0;
    for (int i = 8192; i > 0 && shown < n; i--) {
        uint32_t a = g_trace[(g_trace_pos - i) & 8191];
        if (a == prev) { run++; continue; }
        if (run) { fprintf(stderr, " %08X%s", prev, run > 1 ? "" : ""); if (run > 1) fprintf(stderr, "x%d", run); shown++; }
        prev = a; run = 1;
    }
    if (run) fprintf(stderr, " %08X x%d (ra=%08X)", prev, run, g_trace_ra[(g_trace_pos - 1) & 8191]);
    fprintf(stderr, "\nfirst-entered functions (latest 80 of %zu):\n", g_first.size());
    for (size_t i = g_first.size() > 80 ? g_first.size() - 80 : 0; i < g_first.size(); i++) fprintf(stderr, " %08X", g_first[i]);
    fprintf(stderr, "\nlast interpreted pc: %08X  I_STAT=%03X I_MASK=%03X\n", g_last_interp_pc, g_istat, g_imask);
}
static void on_term(int) { trace_dump(300); FILE* f = fopen("/tmp/psx_ram.bin", "wb"); if (f) { fwrite(psx_ram, 1, PSX_RAM_SIZE, f); fclose(f); } fflush(stderr); _Exit(3); }
struct TraceInit { TraceInit() { signal(SIGTERM, on_term); signal(SIGINT, on_term); } } g_trace_init;
#endif
