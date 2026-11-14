// A high-level implementation of the PlayStation kernel: the A0/B0/C0 function tables, events, the
// interrupt/exception path, threads and a no-card memory card service. There is no BIOS ROM image.
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <sys/mman.h>
#include <ucontext.h>

#include <csetjmp>
#include <cstring>
#include <set>
#include <string>

#include "runtime.h"

int core_poll_depth();
void core_set_poll_depth(int d);

namespace {

uint32_t& A(Cpu* c, int n) { return c->r[4 + n]; }
void ret(Cpu* c, uint32_t v) { c->r[2] = v; }

std::string cstr(uint32_t addr, size_t max = 4096) {
    std::string s;
    Cpu* nc = nullptr;
    (void)nc;
    for (size_t i = 0; i < max; i++) {
        uint8_t ch = psx_ram[(addr + i) & 0x1FFFFF];
        if (!ch) break;
        s += (char)ch;
    }
    return s;
}

// ---------------------------------------------------------------------------------------- events

constexpr uint32_t EV_DISABLED = 0x1000, EV_ENABLED = 0x2000, EV_READY = 0x4000;
constexpr uint32_t EVMODE_CALLBACK = 0x1000;

struct Event { bool used = false; uint32_t cls = 0, spec = 0, mode = 0, func = 0, status = 0; };
Event g_events[64];

uint32_t event_handle(int i) { return 0xF1000000u | (uint32_t)i; }
Event* event_from_handle(uint32_t h) {
    uint32_t i = h & 0xFFFF;
    if ((h & 0xFFFF0000u) != 0xF1000000u || i >= 64 || !g_events[i].used) return nullptr;
    return &g_events[i];
}

void deliver_event(Cpu* c, uint32_t cls, uint32_t spec) {
    for (auto& e : g_events) {
        if (!e.used || e.cls != cls || e.spec != spec || e.status != EV_ENABLED) continue;
        if (e.mode == EVMODE_CALLBACK && e.func) {
            Cpu saved = *c;
            rt_guest_call(c, e.func, 0);
            memcpy(c->r, saved.r, sizeof c->r);
            c->hi = saved.hi;
            c->lo = saved.lo;
        } else {
            e.status = EV_READY;
        }
    }
}

// --------------------------------------------------------------------------------------- exceptions

uint32_t g_hook_buf = 0;
uint32_t g_int_chain[4] = {};
bool g_in_exception = false;
std::jmp_buf g_exc_jmp;

// ----------------------------------------------------------------------------------------- threads
// Threads are real kernel objects as far as guest code can tell: the Table of Tables at 0x100 holds the
// PCB (whose first word points at the running thread's TCB) and the TCB array. Each thread also gets its
// own host stack, because recompiled code keeps its call stack on the host stack; ChangeTh and the
// return from an interrupt switch between them with swapcontext.

constexpr int kMaxThreads = 32;
struct GThread {
    bool used = false, started = false;
    uint32_t r[32] = {}, hi = 0, lo = 0, sr = 0;
    bool in_exc = false;
    int depth = 0;
    std::jmp_buf exc;
    ucontext_t ctx;
    void* stack = nullptr;
};
GThread g_threads[kMaxThreads];
int g_cur_th = 0;
Cpu* g_th_cpu = nullptr;
constexpr size_t kHostStack = 64u << 20;

constexpr uint32_t kToT = 0x100, kTcbh = 0x80008000u, kTcbArr = 0x80008100u, kTcbSize = 0xC0;
constexpr uint32_t kTcbStatus = 0, kTcbMode = 4, kTcbReg = 8;
constexpr uint32_t kRegSP = 29, kRegGP = 28, kRegEPC = 32, kRegHI = 33, kRegLO = 34, kRegSR = 35;
constexpr uint32_t kTcbFree = 0x1000, kTcbUsed = 0x4000;
int g_tcb_count = 4;

uint32_t tcb_addr(int i) { return kTcbArr + (uint32_t)i * kTcbSize; }
uint32_t tcb_reg(Cpu* c, int i, uint32_t r) { return rd32(c, tcb_addr(i) + kTcbReg + 4 * r); }
void set_tcb_reg(Cpu* c, int i, uint32_t r, uint32_t v) { wr32(c, tcb_addr(i) + kTcbReg + 4 * r, v); }

void kernel_tables_init(Cpu* c, int tcbs) {
    g_tcb_count = tcbs;
    wr32(c, kToT + 8, kTcbh);
    wr32(c, kToT + 12, 8);
    wr32(c, kToT + 16, kTcbArr);
    wr32(c, kToT + 20, (uint32_t)tcbs * kTcbSize);
    for (int i = 0; i < tcbs; i++) {
        for (uint32_t o = 0; o < kTcbSize; o += 4) wr32(c, tcb_addr(i) + o, 0);
        wr32(c, tcb_addr(i) + kTcbStatus, i == 0 ? kTcbUsed : kTcbFree);
        wr32(c, tcb_addr(i) + kTcbMode, 0x1000);
    }
    wr32(c, kTcbh, tcb_addr(0));
}

int tcb_index(uint32_t addr) {
    uint32_t a = addr & 0x1FFFFFFFu, base = kTcbArr & 0x1FFFFFFFu;
    if (a < base || (a - base) % kTcbSize || (a - base) / kTcbSize >= (uint32_t)g_tcb_count) return -1;
    return (int)((a - base) / kTcbSize);
}
uint32_t thread_handle(int i) { return 0xFF000000u | (uint32_t)i; }
int thread_from_handle(uint32_t h) {
    uint32_t i = h & 0xFFFF;
    if ((h & 0xFFFF0000u) != 0xFF000000u || i >= (uint32_t)kMaxThreads || (i != 0 && !g_threads[i].used)) return -1;
    return (int)i;
}

void thread_entry() {
    const int idx = g_cur_th;
    Cpu* c = g_th_cpu;
    GThread& t = g_threads[idx];
    uint32_t func = tcb_reg(c, idx, kRegEPC), sp = tcb_reg(c, idx, kRegSP), gp = tcb_reg(c, idx, kRegGP);
    uint32_t sr = tcb_reg(c, idx, kRegSR);
    memset(c->r, 0, sizeof c->r);
    c->r[29] = sp;
    c->r[28] = gp;
    c->cop0[12] = sr | 1u;
    g_in_exception = false;
    c->next_event = 0;
    rt_guest_call(c, func, 0);  // a thread that returns is finished
    t.used = false;
    wr32(c, tcb_addr(idx) + kTcbStatus, kTcbFree);
    g_cur_th = 0;
    wr32(c, kTcbh, tcb_addr(0));
    setcontext(&g_threads[0].ctx);
}

void thread_switch(Cpu* c, int target) {
    const int me = g_cur_th;
    if (target == me) return;
    GThread& t = g_threads[me];
    memcpy(t.r, c->r, sizeof t.r);
    t.hi = c->hi; t.lo = c->lo; t.sr = c->cop0[12];
    t.in_exc = g_in_exception;
    t.depth = core_poll_depth();
    memcpy(&t.exc, &g_exc_jmp, sizeof g_exc_jmp);
    g_th_cpu = c;
    for (uint32_t r = 1; r < 32; r++) set_tcb_reg(c, me, r, c->r[r]);
    set_tcb_reg(c, me, kRegHI, c->hi);
    set_tcb_reg(c, me, kRegLO, c->lo);
    set_tcb_reg(c, me, kRegSR, c->cop0[12]);
    wr32(c, kTcbh, tcb_addr(target));
    GThread& n = g_threads[target];
    g_cur_th = target;
    core_set_poll_depth(n.depth);
    if (!n.started) {
        n.started = true;
        if (!n.stack) n.stack = mmap(nullptr, kHostStack, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (n.stack == MAP_FAILED) fatal("cannot allocate a thread stack");
        getcontext(&n.ctx);
        n.ctx.uc_stack.ss_sp = n.stack;
        n.ctx.uc_stack.ss_size = kHostStack;
        n.ctx.uc_link = nullptr;
        makecontext(&n.ctx, thread_entry, 0);
    }
    swapcontext(&t.ctx, &n.ctx);
    memcpy(c->r, t.r, sizeof c->r);
    c->hi = t.hi; c->lo = t.lo; c->cop0[12] = t.sr;
    g_in_exception = t.in_exc;
    memcpy(&g_exc_jmp, &t.exc, sizeof g_exc_jmp);
}

// ---------------------------------------------------------------------------------------- printf

std::string format_printf(Cpu* c, const std::string& fmt, int first_arg) {
    int argn = first_arg;
    auto next = [&]() -> uint32_t {
        uint32_t v = argn < 4 ? c->r[4 + argn] : rd32(c, c->r[29] + 16 + 4 * (argn - 4));
        argn++;
        return v;
    };
    std::string out;
    for (size_t i = 0; i < fmt.size(); i++) {
        if (fmt[i] != '%') { out += fmt[i]; continue; }
        std::string spec = "%";
        i++;
        while (i < fmt.size() && strchr("-+ #0123456789.", fmt[i])) spec += fmt[i++];
        while (i < fmt.size() && (fmt[i] == 'l' || fmt[i] == 'h')) i++;
        if (i >= fmt.size()) break;
        char ch = fmt[i];
        char buf[512];
        if (ch == '%') out += '%';
        else if (ch == 'd' || ch == 'i') { spec += "d"; snprintf(buf, sizeof buf, spec.c_str(), (int)next()); out += buf; }
        else if (ch == 'u') { spec += "u"; snprintf(buf, sizeof buf, spec.c_str(), next()); out += buf; }
        else if (ch == 'x' || ch == 'X' || ch == 'o') { spec += ch; snprintf(buf, sizeof buf, spec.c_str(), next()); out += buf; }
        else if (ch == 'c') { out += (char)next(); }
        else if (ch == 's') { spec += "s"; std::string s = cstr(next(), 256); snprintf(buf, sizeof buf, spec.c_str(), s.c_str()); out += buf; }
        else if (ch == 'p') { snprintf(buf, sizeof buf, "%08x", next()); out += buf; }
        else out += spec + ch;
    }
    return out;
}

void console_write(const std::string& s) {
    fputs(s.c_str(), stderr);
    fflush(stderr);
}

std::set<uint32_t> g_warned;
void unimplemented(Cpu* c, uint32_t table, uint32_t fn) {
    uint32_t key = (table << 8) | fn;
    if (g_warned.insert(key).second)
        logf("bios", "%02X:%02X not implemented (a0=%08X a1=%08X ra=%08X): returning 0", table, fn, A(c, 0), A(c, 1), c->r[31]);
    ret(c, 0);
}

// card service: no memory card is inserted, every operation times out
void card_timeout(Cpu* c) {
    deliver_event(c, 0xF0000011u, 0x0100);
    deliver_event(c, 0xF4000001u, 0x0100);
}

uint32_t g_heap_base = 0, g_heap_size = 0, g_rand = 1;

}  // namespace

bool bios_in_exception() { return g_in_exception; }

void bios_setconf(Cpu* c) {  // libapi SetConf(events, tcbs, stack)
    int n = (int)c->r[5];
    if (n < 4) n = 4;
    if (n > kMaxThreads) n = kMaxThreads;
    kernel_tables_init(c, n);
    g_threads[0].used = true;
    g_cur_th = 0;
}

void bios_init(Cpu* c) {
    kernel_tables_init(c, 4);
    g_threads[0].used = true;
    c->cop0[12] = 0x405;  // as the kernel leaves it: interrupts on
}

static void a0_call(Cpu* c, uint32_t fn) {
    switch (fn) {
        case 0x13:  // setjmp
        case 0x14: unimplemented(c, 0xA0, fn); return;
        case 0x17: ret(c, (uint32_t)strcmp(cstr(A(c, 0)).c_str(), cstr(A(c, 1)).c_str())); return;
        case 0x18: ret(c, (uint32_t)strncmp(cstr(A(c, 0)).c_str(), cstr(A(c, 1)).c_str(), A(c, 2))); return;
        case 0x19: { std::string s = cstr(A(c, 1)); for (size_t i = 0; i <= s.size(); i++) wr8(c, A(c, 0) + i, i < s.size() ? (uint8_t)s[i] : 0); ret(c, A(c, 0)); return; }
        case 0x1B: ret(c, (uint32_t)cstr(A(c, 0), 1 << 20).size()); return;
        case 0x28: for (uint32_t i = 0; i < A(c, 1); i++) wr8(c, A(c, 0) + i, 0); return;
        case 0x2A: for (uint32_t i = 0; i < A(c, 2); i++) wr8(c, A(c, 0) + i, rd8(c, A(c, 1) + i)); ret(c, A(c, 0)); return;
        case 0x2B: for (uint32_t i = 0; i < A(c, 2); i++) wr8(c, A(c, 0) + i, A(c, 1)); ret(c, A(c, 0)); return;
        case 0x2F: g_rand = g_rand * 1103515245u + 12345u; ret(c, (g_rand >> 16) & 0x7FFF); return;
        case 0x30: g_rand = A(c, 0); return;
        case 0x39: g_heap_base = A(c, 0); g_heap_size = A(c, 1); return;
        case 0x3F: console_write(format_printf(c, cstr(A(c, 0), 1024), 1)); return;
        case 0x44: return;  // FlushCache
        case 0x49: gpu_write_gp1(0x01000000); gpu_write_gp0(A(c, 0)); return;
        case 0x70: return;  // _bu_init
        case 0x71: case 0x72: ret(c, 0); return;
        case 0x9F: return;  // SetMem
        case 0xAB: case 0xAC: card_timeout(c); ret(c, 1); return;
        default: unimplemented(c, 0xA0, fn);
    }
}

static void b0_call(Cpu* c, uint32_t fn) {
    switch (fn) {
        case 0x07: deliver_event(c, A(c, 0), A(c, 1)); ret(c, 0); return;
        case 0x08:
            for (int i = 0; i < 64; i++)
                if (!g_events[i].used) {
                    g_events[i] = {true, A(c, 0), A(c, 1), A(c, 2), A(c, 3), EV_DISABLED};
                    ret(c, event_handle(i));
                    return;
                }
            ret(c, 0xFFFFFFFF);
            return;
        case 0x09: if (Event* e = event_from_handle(A(c, 0))) e->used = false; ret(c, 1); return;
        case 0x0A: {  // WaitEvent
            Event* e = event_from_handle(A(c, 0));
            if (!e || e->status == EV_DISABLED) { ret(c, 0); return; }
            while (e->status != EV_READY) {
                c->cycles = std::max(c->cycles + 1, c->next_event);
                psx_poll(c);
            }
            e->status = EV_ENABLED;
            ret(c, 1);
            return;
        }
        case 0x0B: {  // TestEvent
            Event* e = event_from_handle(A(c, 0));
            if (e && e->status == EV_READY) { e->status = EV_ENABLED; ret(c, 1); }
            else ret(c, 0);
            return;
        }
        case 0x0C: if (Event* e = event_from_handle(A(c, 0))) e->status = EV_ENABLED; ret(c, 1); return;
        case 0x0D: if (Event* e = event_from_handle(A(c, 0))) e->status = EV_DISABLED; ret(c, 1); return;
        case 0x0E:  // OpenTh(func, sp, gp)
            for (int i = 1; i < g_tcb_count && i < kMaxThreads; i++)
                if (rd32(c, tcb_addr(i) + kTcbStatus) == kTcbFree && !g_threads[i].used) {
                    g_threads[i] = GThread();
                    g_threads[i].used = true;
                    for (uint32_t o = 8; o < kTcbSize; o += 4) wr32(c, tcb_addr(i) + o, 0);
                    wr32(c, tcb_addr(i) + kTcbStatus, kTcbUsed);
                    set_tcb_reg(c, i, kRegEPC, A(c, 0));
                    set_tcb_reg(c, i, kRegSP, A(c, 1));
                    set_tcb_reg(c, i, kRegGP, A(c, 2));
                    ret(c, thread_handle(i));
                    return;
                }
            ret(c, 0xFFFFFFFF);
            return;
        case 0x0F: {
            int i = thread_from_handle(A(c, 0));
            if (i > 0) { g_threads[i].used = false; wr32(c, tcb_addr(i) + kTcbStatus, kTcbFree); }
            ret(c, i > 0 ? 1 : 0);
            return;
        }
        case 0x10: {
            int i = thread_from_handle(A(c, 0));
            if (i < 0) { ret(c, 0); return; }
            thread_switch(c, i);
            ret(c, 1);
            return;
        }
        case 0x12: case 0x13: case 0x14: case 0x15: ret(c, 1); return;  // pad BIOS (unused: libpad drives SIO0)
        case 0x17:  // ReturnFromException
            if (!g_in_exception) fatal("ReturnFromException outside an exception (ra=%08X)", c->r[31]);
            std::longjmp(g_exc_jmp, 1);
        case 0x18: g_hook_buf = 0; return;
        case 0x19: g_hook_buf = A(c, 0); return;
        case 0x32: ret(c, 0xFFFFFFFF); return;  // open: no file system devices
        case 0x33: case 0x34: case 0x36: ret(c, 0xFFFFFFFF); return;
        case 0x35: {  // write(fd, buf, len): the console
            if (A(c, 0) == 1) { std::string s; for (uint32_t i = 0; i < A(c, 2); i++) s += (char)rd8(c, A(c, 1) + i); console_write(s); }
            ret(c, A(c, 2));
            return;
        }
        case 0x3D: console_write(std::string(1, (char)A(c, 0))); return;
        case 0x3F: console_write(cstr(A(c, 0), 1024) + "\n"); return;
        case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: ret(c, 0); return;
        case 0x4A: ret(c, 1); return;
        case 0x4B: case 0x4C: ret(c, 1); return;
        case 0x4E: case 0x4F: case 0x50: card_timeout(c); ret(c, 1); return;
        case 0x56: ret(c, 0x674); return;
        case 0x57: ret(c, 0x874); return;
        case 0x5B: ret(c, 0); return;
        default: unimplemented(c, 0xB0, fn);
    }
}

static void c0_call(Cpu* c, uint32_t fn) {
    switch (fn) {
        case 0x02: {  // SysEnqIntRP(priority, element): push at the chain head
            uint32_t pri = A(c, 0) & 3, el = A(c, 1);
            wr32(c, el, g_int_chain[pri]);
            g_int_chain[pri] = el;
            ret(c, 0);
            return;
        }
        case 0x03: {  // SysDeqIntRP
            uint32_t pri = A(c, 0) & 3, el = A(c, 1);
            // (walk properly through guest memory)
            uint32_t prev = 0, cur = g_int_chain[pri];
            while (cur && cur != el) { prev = cur; cur = rd32(c, cur); }
            if (cur == el) {
                uint32_t nxt = rd32(c, cur);
                if (prev) wr32(c, prev, nxt); else g_int_chain[pri] = nxt;
            }
            ret(c, 0);
            return;
        }
        case 0x07: case 0x08: case 0x09: ret(c, 0); return;
        case 0x0A: ret(c, 0); return;  // ChangeClearRCnt
        default: unimplemented(c, 0xC0, fn);
    }
}

void bios_call(Cpu* c, uint32_t table, uint32_t fn) {
    if (g_log_bios && !(table == 0xB0 && (fn == 0x0B || fn == 0x17)))
        logf("bios", "%02X:%02X a0=%08X a1=%08X a2=%08X ra=%08X", table, fn, A(c, 0), A(c, 1), A(c, 2), c->r[31]);
    if (table == 0xA0) a0_call(c, fn);
    else if (table == 0xB0) b0_call(c, fn);
    else c0_call(c, fn);
}

void bios_take_interrupt(Cpu* c) {
    if (g_in_exception) return;
    Cpu saved = *c;
    g_in_exception = true;
    uint32_t sr = c->cop0[12];
    c->cop0[12] = (sr & ~0x3Fu) | ((sr << 2) & 0x3Fu);  // push KU/IE: interrupts off
    LOG(irq, "take th=%d stat=%03X mask=%03X sr=%08X", g_cur_th, irq_stat(), irq_mask(), c->cop0[12]);
    if (setjmp(g_exc_jmp) == 0) {
        for (int pri = 0; pri < 4; pri++) {
            for (uint32_t el = g_int_chain[pri]; el; el = rd32(c, el)) {
                uint32_t f2 = rd32(c, el + 4), f1 = rd32(c, el + 8);
                if (!f1) continue;
                c->r[29] = saved.r[29] - 0x100;
                uint32_t v = rt_guest_call(c, f1, 0);
                if (v && f2) rt_guest_call(c, f2, v);
            }
        }
        if (g_hook_buf) {
            uint32_t b = g_hook_buf;
            c->r[29] = rd32(c, b + 4);
            c->r[30] = rd32(c, b + 8);
            for (int i = 0; i < 8; i++) c->r[16 + i] = rd32(c, b + 12 + 4 * i);
            c->r[28] = rd32(c, b + 44);
            c->r[2] = 1;
            c->r[31] = 0;
            LOG(irq, "hook -> %08X sp=%08X gp=%08X", rd32(c, b), c->r[29], c->r[28]);
            psx_call(c, rd32(c, b));
            LOG(irq, "hook returned normally");
        }
    }
    memcpy(c->r, saved.r, sizeof c->r);
    c->hi = saved.hi;
    c->lo = saved.lo;
    sr = c->cop0[12];
    c->cop0[12] = (sr & ~0xFu) | ((sr >> 2) & 0xFu);  // pop KU/IE
    g_in_exception = false;
    // a handler (the game's task scheduler) made another thread the running one: continue that one
    int next = tcb_index(rd32(c, kTcbh));
    if (next >= 0 && next != g_cur_th) thread_switch(c, next);
    c->next_event = 0;
}
