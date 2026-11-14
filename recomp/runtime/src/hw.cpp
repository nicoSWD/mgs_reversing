// The bus beyond RAM: memory-mapped I/O registers, interrupt controller, root counters, DMA, the
// serial port stub and the SPU register file.
#include <cstring>

#include "runtime.h"

void irq_set_stat(uint32_t v);
void irq_set_mask(uint32_t v);
bool sio_read(uint32_t reg, uint32_t* v, int size);
bool sio_write(Cpu* c, uint32_t reg, uint32_t v, int size);
bool spu_read(uint32_t phys, uint32_t* v, int size);
bool spu_write(Cpu* c, uint32_t phys, uint32_t v, int size);
void sio_event(Cpu* c);
void spu_dma(uint32_t* words, uint32_t count, bool to_spu);
bool mdec_read(uint32_t phys, uint32_t* v);
bool mdec_write(Cpu* c, uint32_t phys, uint32_t v);
void mdec_dma_in(const uint32_t* words, uint32_t count);
void mdec_dma_out(uint32_t* words, uint32_t count);
void host_frame(Cpu* c);
void spu_step(Cpu* c);
void spu_init(Cpu* c);

static const uint64_t kCpuHz = 33868800;
static const uint64_t kFrameCycles = kCpuHz / 60;
static const uint64_t kLineCycles = kFrameCycles / 263;

// --------------------------------------------------------------------------------------- timers

struct Timer {
    uint32_t mode = 0, target = 0;
    uint64_t base_cycle = 0;   // when `base` was valid
    uint32_t base = 0;         // counter value at base_cycle
    bool hit_target = false, hit_over = false;
    bool irq_armed = true;
};
static Timer g_tmr[3];

static uint32_t tmr_divider(int n, uint32_t mode) {
    switch (n) {
        case 0: return (mode & 0x100) ? 6 : 1;                // dot clock vs system clock
        case 1: return (mode & 0x100) ? (uint32_t)kLineCycles : 1;  // hblank vs system clock
        default: return (mode & 0x200) ? 8 : 1;               // sysclk/8
    }
}
static uint32_t tmr_value(Cpu* c, int n) {
    Timer& t = g_tmr[n];
    if (n == 2 && (t.mode & 1) && ((t.mode >> 1) & 3) != 1 && ((t.mode >> 1) & 3) != 2) return t.base;  // stopped
    uint64_t ticks = (c->cycles - t.base_cycle) / tmr_divider(n, t.mode);
    uint64_t v = t.base + ticks;
    uint32_t limit = (t.mode & 8) ? (t.target + 1) : 0x10000;
    if (limit == 0) limit = 0x10000;
    return (uint32_t)(v % limit);
}
static void tmr_rebase(Cpu* c, int n) {
    g_tmr[n].base = tmr_value(c, n);
    g_tmr[n].base_cycle = c->cycles;
}
static void tmr_schedule(Cpu* c, int n) {
    Timer& t = g_tmr[n];
    uint32_t div = tmr_divider(n, t.mode);
    uint32_t v = tmr_value(c, n);
    uint32_t to_target = (t.target > v) ? t.target - v : (t.target + 0x10000 - v);
    uint32_t to_over = 0x10000 - v;
    uint32_t wait = (t.mode & 8) ? to_target : std::min(to_target, to_over);
    if (!(t.mode & 0x30)) { sched_clear(c, (Ev)(EV_TMR0 + n)); return; }
    if (wait == 0) wait = 1;
    sched_set(c, (Ev)(EV_TMR0 + n), c->cycles + (uint64_t)wait * div);
}
static void tmr_event(Cpu* c, int n) {
    Timer& t = g_tmr[n];
    uint32_t v = tmr_value(c, n);
    if (v == t.target || (v == 0 && (t.mode & 8) == 0)) {}
    bool target = (v == t.target) || ((t.mode & 8) && v == 0);
    bool over = (!(t.mode & 8)) && v == 0;
    if (target) t.mode |= 0x800;
    if (over) t.mode |= 0x1000;
    if ((target && (t.mode & 0x10)) || (over && (t.mode & 0x20))) {
        t.mode &= ~0x400u;  // /IRQ low
        irq_raise(c, IRQ_TMR0 << n);
    }
    tmr_schedule(c, n);
}

static uint32_t timer_read(Cpu* c, uint32_t off) {
    int n = (off >> 4) & 3;
    if (n > 2) return 0;
    switch (off & 15) {
        case 0: return tmr_value(c, n);
        case 4: { uint32_t m = g_tmr[n].mode; g_tmr[n].mode &= ~0x1800u; return m; }
        case 8: return g_tmr[n].target;
    }
    return 0;
}
static void timer_write(Cpu* c, uint32_t off, uint32_t v) {
    int n = (off >> 4) & 3;
    if (n > 2) return;
    Timer& t = g_tmr[n];
    switch (off & 15) {
        case 0: t.base = v & 0xFFFF; t.base_cycle = c->cycles; break;
        case 4:
            t.mode = (v & 0x3FF) | 0x400;
            t.base = 0;
            t.base_cycle = c->cycles;
            break;
        case 8: tmr_rebase(c, n); t.target = v & 0xFFFF; break;
    }
    tmr_schedule(c, n);
}

// ------------------------------------------------------------------------------------------ DMA

static uint32_t g_dma_madr[7], g_dma_bcr[7], g_dma_chcr[7];
static uint32_t g_dpcr = 0x07654321, g_dicr = 0;

static void dma_update_irq(Cpu* c) {
    bool master = (g_dicr >> 23) & 1;
    bool any = ((g_dicr >> 16) & 0x7F) & ((g_dicr >> 24) & 0x7F);
    bool force = (g_dicr >> 15) & 1;
    static bool prev = false;
    bool now = force || (master && any);
    if (now && !prev) irq_raise(c, IRQ_DMA);
    prev = now;
}

static void dma_finish(Cpu* c, int ch) {
    g_dma_chcr[ch] &= ~0x11000000u;
    if ((g_dicr >> (16 + ch)) & 1) g_dicr |= 1u << (24 + ch);
    dma_update_irq(c);
}

static void dma_run(Cpu* c, int ch) {
    uint32_t chcr = g_dma_chcr[ch];
    uint32_t madr = g_dma_madr[ch] & 0x1FFFFC;
    uint32_t bcr = g_dma_bcr[ch];
    bool from_ram = chcr & 1;
    int mode = (chcr >> 9) & 3;
    int step = (chcr & 2) ? -4 : 4;
    uint32_t words = 0;
    if (mode == 0) { words = bcr & 0xFFFF; if (!words) words = 0x10000; }
    else if (mode == 1) { words = (bcr & 0xFFFF) * (bcr >> 16); }
    LOG(dma, "ch%d chcr=%08X madr=%08X bcr=%08X mode=%d words=%u", ch, chcr, madr, bcr, mode, words);
    uint64_t cost = 16;
    switch (ch) {
        case 2:  // GPU
            if (mode == 2) {
                gpu_dma_chain(c, madr);
            } else if (from_ram) {
                gpu_dma_to((const uint32_t*)(psx_ram + (madr & 0x1FFFFF)), words);
                cost += words;
            } else {
                gpu_dma_from((uint32_t*)(psx_ram + (madr & 0x1FFFFF)), words);
                cost += words;
            }
            break;
        case 3:  // CD-ROM data
            cd_dma(psx_ram + (madr & 0x1FFFFF), words * 4);
            cost += words * 6;
            break;
        case 4:  // SPU
            spu_dma((uint32_t*)(psx_ram + (madr & 0x1FFFFF)), words, from_ram);
            cost += words;
            break;
        case 0:  // MDEC in
            mdec_dma_in((const uint32_t*)(psx_ram + (madr & 0x1FFFFF)), words);
            cost += words;
            break;
        case 1:  // MDEC out
            mdec_dma_out((uint32_t*)(psx_ram + (madr & 0x1FFFFF)), words);
            cost += words;
            break;
        case 6: {  // ordering table clear: a reverse linked list
            uint32_t n = bcr & 0xFFFF;
            if (!n) n = 0x10000;
            uint32_t a = madr;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t next = (i + 1 == n) ? 0x00FFFFFFu : ((a - 4) & 0x1FFFFF);
                memcpy(psx_ram + (a & 0x1FFFFF), &next, 4);
                a -= 4;
            }
            cost += n;
            break;
        }
        default: break;
    }
    // completion is not instant: the channel stays busy for a while
    g_dma_chcr[ch] |= 0x01000000u;
    sched_set(c, (Ev)(EV_DMA0 + ch), c->cycles + cost);
}

static uint32_t dma_read(uint32_t off) {
    if (off < 0x70) {
        int ch = off >> 4;
        switch (off & 15) {
            case 0: return g_dma_madr[ch];
            case 4: return g_dma_bcr[ch];
            case 8: return g_dma_chcr[ch];
        }
        return 0;
    }
    if (off == 0x70) return g_dpcr;
    if (off == 0x74) {
        uint32_t v = g_dicr & 0x00FFFFFFu;
        bool master = (g_dicr >> 23) & 1;
        bool any = ((g_dicr >> 16) & 0x7F) & ((g_dicr >> 24) & 0x7F);
        if ((g_dicr >> 15) & 1 || (master && any)) v |= 0x80000000u;
        return v | (g_dicr & 0x7F000000u);
    }
    return 0;
}
static void dma_write(Cpu* c, uint32_t off, uint32_t v) {
    if (off < 0x70) {
        int ch = off >> 4;
        switch (off & 15) {
            case 0: g_dma_madr[ch] = v & 0xFFFFFF; break;
            case 4: g_dma_bcr[ch] = v; break;
            case 8:
                if (ch == 6) g_dma_chcr[ch] = (v & 0x51000002u) | (g_dma_chcr[ch] & ~0x51000002u);
                else g_dma_chcr[ch] = v & 0x71770703u;
                {
                    uint32_t cc = g_dma_chcr[ch];
                    bool enabled = (g_dpcr >> (ch * 4 + 3)) & 1;
                    bool sync0 = ((cc >> 9) & 3) == 0;
                    if ((cc & 0x01000000u) && enabled && (!sync0 || (cc & 0x10000000u))) dma_run(c, ch);
                }
                break;
        }
        return;
    }
    if (off == 0x70) g_dpcr = v;
    else if (off == 0x74) {
        uint32_t flags = g_dicr & 0x7F000000u;
        flags &= ~(v & 0x7F000000u);  // write 1 to acknowledge
        g_dicr = (v & 0x00FF803Fu) | flags;
        dma_update_irq(c);
    }
}

// ------------------------------------------------------------------------------------- bus access

static uint32_t g_mem_ctrl[16];

uint32_t io_read(Cpu* c, uint32_t p, int size) {
    uint32_t off = p - 0x1F801000u;
    uint32_t v = 0;
    if (off < 0x24) return g_mem_ctrl[(off >> 2) & 15];
    if (off >= 0x40 && off < 0x60) { sio_read(off, &v, size); return v; }
    if (off == 0x70) return irq_stat();
    if (off == 0x74) return irq_mask();
    if (off >= 0x80 && off < 0x100) return dma_read(off - 0x80);
    if (off >= 0x100 && off < 0x130) return timer_read(c, off - 0x100);
    if (off >= 0x800 && off < 0x804) return cd_read_reg(c, off - 0x800);
    if (off == 0x810) return gpu_read_data();
    if (off == 0x814) return gpu_read_status();
    if (off == 0x820 || off == 0x824) { mdec_read(p, &v); return v; }
    if (off >= 0xC00 && off < 0xE80) { spu_read(p, &v, size); return v; }
    return 0;
}

void io_write(Cpu* c, uint32_t p, uint32_t v, int size) {
    uint32_t off = p - 0x1F801000u;
    if (off < 0x24) { g_mem_ctrl[(off >> 2) & 15] = v; return; }
    if (off >= 0x40 && off < 0x60) { sio_write(c, off, v, size); return; }
    if (off == 0x70) { irq_set_stat(v); return; }
    if (off == 0x74) { irq_set_mask(v); c->next_event = 0; return; }
    if (off >= 0x80 && off < 0x100) { dma_write(c, off - 0x80, v); return; }
    if (off >= 0x100 && off < 0x130) { timer_write(c, off - 0x100, v); return; }
    if (off >= 0x800 && off < 0x804) { cd_write_reg(c, off - 0x800, v); return; }
    if (off == 0x810) { gpu_write_gp0(v); return; }
    if (off == 0x814) { gpu_write_gp1(v); return; }
    if (off == 0x820 || off == 0x824) { mdec_write(c, p, v); return; }
    if (off >= 0xC00 && off < 0xE80) { spu_write(c, p, v, size); return; }
}

// 32-bit registers are accessed with byte/halfword loads and stores too; those use the lane of the register.
// Byte-wide devices (CD-ROM, serial port, SPU, timers' 16-bit halves...) answer for their exact address.
static bool lane_reg(uint32_t off) {
    return (off >= 0x70 && off < 0x78) || (off >= 0x80 && off < 0x100) || off == 0x810 || off == 0x814 ||
           off == 0x820 || off == 0x824;
}

extern "C" uint32_t psx_read_slow(Cpu* c, uint32_t addr, int size) {
    uint32_t p = addr & 0x1FFFFFFFu;
    if (p >= 0x1F801000u && p < 0x1F803000u) {
        uint32_t off = p - 0x1F801000u;
        if (size < 4 && lane_reg(off)) {
            uint32_t v = io_read(c, p & ~3u, 4);
            uint32_t shift = (p & 3u) * 8;
            return size == 2 ? ((v >> shift) & 0xFFFF) : ((v >> shift) & 0xFF);
        }
        uint32_t v = io_read(c, p, size);
        return size == 2 ? (v & 0xFFFF) : size == 1 ? (v & 0xFF) : v;
    }
    if (p >= 0x1F000000u && p < 0x1F800000u) return size == 4 ? 0xFFFFFFFFu : size == 2 ? 0xFFFFu : 0xFFu;
    return 0;
}

extern "C" void psx_write_slow(Cpu* c, uint32_t addr, uint32_t v, int size) {
    uint32_t p = addr & 0x1FFFFFFFu;
    if (p >= 0x1F801000u && p < 0x1F803000u) {
        uint32_t off = p - 0x1F801000u;
        if (size < 4 && lane_reg(off)) {
            uint32_t base = p & ~3u, shift = (p & 3u) * 8;
            uint32_t mask = (size == 2 ? 0xFFFFu : 0xFFu) << shift;
            uint32_t cur = (off & ~3u) == 0x74 ? (dma_read(0x74) & 0x00FFFFFFu) : io_read(c, base, 4);
            io_write(c, base, (cur & ~mask) | ((v << shift) & mask), 4);
            return;
        }
        io_write(c, p, size == 2 ? (v & 0xFFFF) : size == 1 ? (v & 0xFF) : v, size);
        return;
    }
}

// -------------------------------------------------------------------------------- events and video

void hw_run_event(Cpu* c, int ev) {
    switch (ev) {
        case EV_VBLANK:
            sched_set(c, EV_VBLANK, c->cycles + kFrameCycles);
            hw_vblank(c);
            break;
        case EV_TMR0: case EV_TMR1: case EV_TMR2: tmr_event(c, ev - EV_TMR0); break;
        case EV_CD: cd_event(c); break;
        case EV_PAD: sio_event(c); break;
        case EV_SPU: spu_step(c); break;
        case EV_DMA0: case EV_DMA1: case EV_DMA2: case EV_DMA3: case EV_DMA4: case EV_DMA5: case EV_DMA6:
            dma_finish(c, ev - EV_DMA0);
            break;
        default: break;
    }
}

void hw_vblank(Cpu* c) {
    host_frame(c);
    irq_raise(c, IRQ_VBLANK);
}

void hw_init(Cpu* c) {
    sched_set(c, EV_VBLANK, c->cycles + kFrameCycles);
    spu_init(c);
}
