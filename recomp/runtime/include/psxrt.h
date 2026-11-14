/* psxrt: the ABI between recompiled PlayStation code and the runtime. */
#pragma once
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Cpu {
    uint32_t r[32];
    uint32_t hi, lo;
    uint32_t cop0[32];
    int32_t gd[32];  /* GTE data registers */
    int32_t gc[32];  /* GTE control registers */
    uint64_t cycles;
    uint64_t next_event;
} Cpu;

typedef void (*PsxFnPtr)(Cpu*);
typedef struct PsxFunc {
    uint32_t addr;
    PsxFnPtr fn;
} PsxFunc;

#define PSX_RAM_SIZE 0x200000u
extern uint8_t psx_ram[PSX_RAM_SIZE];
extern uint8_t psx_scratch[1024];
extern int psx_cache_isolated;

uint32_t psx_read_slow(Cpu* c, uint32_t addr, int size);
void psx_write_slow(Cpu* c, uint32_t addr, uint32_t value, int size);

#if defined(__GNUC__)
#define PSX_LIKELY(x) __builtin_expect(!!(x), 1)
#define PSX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define PSX_LIKELY(x) (x)
#define PSX_UNLIKELY(x) (x)
#endif

static inline uint32_t rd32(Cpu* c, uint32_t a) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) {
        uint32_t v;
        memcpy(&v, psx_ram + (p & 0x1FFFFFu), 4);
        return v;
    }
    if ((p & 0x1FFFFC00u) == 0x1F800000u) {
        uint32_t v;
        memcpy(&v, psx_scratch + (p & 0x3FCu), 4);
        return v;
    }
    return psx_read_slow(c, a, 4);
}
static inline uint32_t rd16(Cpu* c, uint32_t a) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) {
        uint16_t v;
        memcpy(&v, psx_ram + (p & 0x1FFFFFu), 2);
        return v;
    }
    if ((p & 0x1FFFFC00u) == 0x1F800000u) {
        uint16_t v;
        memcpy(&v, psx_scratch + (p & 0x3FEu), 2);
        return v;
    }
    return psx_read_slow(c, a, 2);
}
static inline uint32_t rd8(Cpu* c, uint32_t a) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) return psx_ram[p & 0x1FFFFFu];
    if ((p & 0x1FFFFC00u) == 0x1F800000u) return psx_scratch[p & 0x3FFu];
    return psx_read_slow(c, a, 1);
}
static inline void wr32(Cpu* c, uint32_t a, uint32_t v) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) {
        if (PSX_UNLIKELY(psx_cache_isolated)) return;
        memcpy(psx_ram + (p & 0x1FFFFFu), &v, 4);
        return;
    }
    if ((p & 0x1FFFFC00u) == 0x1F800000u) {
        memcpy(psx_scratch + (p & 0x3FCu), &v, 4);
        return;
    }
    psx_write_slow(c, a, v, 4);
}
static inline void wr16(Cpu* c, uint32_t a, uint32_t v) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) {
        if (PSX_UNLIKELY(psx_cache_isolated)) return;
        uint16_t h = (uint16_t)v;
        memcpy(psx_ram + (p & 0x1FFFFFu), &h, 2);
        return;
    }
    if ((p & 0x1FFFFC00u) == 0x1F800000u) {
        uint16_t h = (uint16_t)v;
        memcpy(psx_scratch + (p & 0x3FEu), &h, 2);
        return;
    }
    psx_write_slow(c, a, v, 2);
}
static inline void wr8(Cpu* c, uint32_t a, uint32_t v) {
    uint32_t p = a & 0x1FFFFFFFu;
    if (PSX_LIKELY(p < 0x800000u)) {
        if (PSX_UNLIKELY(psx_cache_isolated)) return;
        psx_ram[p & 0x1FFFFFu] = (uint8_t)v;
        return;
    }
    if ((p & 0x1FFFFC00u) == 0x1F800000u) {
        psx_scratch[p & 0x3FFu] = (uint8_t)v;
        return;
    }
    psx_write_slow(c, a, v, 1);
}

/* unaligned access (lwl/lwr/swl/swr), little endian */
static inline uint32_t psx_lwl(Cpu* c, uint32_t a, uint32_t cur) {
    uint32_t w = rd32(c, a & ~3u), s = (a & 3u) * 8;
    return (cur & (0x00FFFFFFu >> s)) | (w << (24 - s));
}
static inline uint32_t psx_lwr(Cpu* c, uint32_t a, uint32_t cur) {
    uint32_t w = rd32(c, a & ~3u), s = (a & 3u) * 8;
    return (cur & (0xFFFFFF00u << (24 - s))) | (w >> s);
}
static inline void psx_swl(Cpu* c, uint32_t a, uint32_t v) {
    uint32_t w = rd32(c, a & ~3u), s = (a & 3u) * 8;
    wr32(c, a & ~3u, (w & (0xFFFFFF00u << s)) | (v >> (24 - s)));
}
static inline void psx_swr(Cpu* c, uint32_t a, uint32_t v) {
    uint32_t w = rd32(c, a & ~3u), s = (a & 3u) * 8;
    wr32(c, a & ~3u, (w & (0x00FFFFFFu >> (24 - s))) | (v << s));
}

static inline void psx_div(Cpu* c, uint32_t a, uint32_t b) {
    int32_t x = (int32_t)a, y = (int32_t)b;
    if (y == 0) { c->lo = x < 0 ? 1u : 0xFFFFFFFFu; c->hi = a; }
    else if (a == 0x80000000u && y == -1) { c->lo = 0x80000000u; c->hi = 0; }
    else { c->lo = (uint32_t)(x / y); c->hi = (uint32_t)(x % y); }
}
static inline void psx_divu(Cpu* c, uint32_t a, uint32_t b) {
    if (b == 0) { c->lo = 0xFFFFFFFFu; c->hi = a; }
    else { c->lo = a / b; c->hi = a % b; }
}

/* coprocessors */
uint32_t cop0_read(Cpu* c, int n);
void cop0_write(Cpu* c, int n, uint32_t v);
void cop0_rfe(Cpu* c);
uint32_t gte_read_data(Cpu* c, int n);
void gte_write_data(Cpu* c, int n, uint32_t v);
uint32_t gte_read_ctrl(Cpu* c, int n);
void gte_write_ctrl(Cpu* c, int n, uint32_t v);
void gte_command(Cpu* c, uint32_t cmd);

/* control flow into the runtime */
void psx_poll(Cpu* c);                         /* the scheduler: hardware events, interrupts */
void psx_call(Cpu* c, uint32_t addr);          /* run the guest function at addr; returns when it returns */
void psx_jump(Cpu* c, uint32_t addr);          /* same, for a tail jump (the caller then returns) */
void psx_fell_off(Cpu* c, uint32_t addr);      /* recompiled flow ran past its known end */
void psx_syscall(Cpu* c, uint32_t pc);
void psx_break(Cpu* c, uint32_t pc, uint32_t code);
void psx_illegal(Cpu* c, uint32_t pc, uint32_t word);
void bios_setconf(Cpu* c);                     /* libapi SetConf replacement, for game hooks */

#ifdef PSX_TRACE_CALLS
void psx_trace_enter(Cpu* c, uint32_t addr);
#define PSX_TRACE(c, a) psx_trace_enter(c, a)
#else
#define PSX_TRACE(c, a) ((void)0)
#endif

#define PSX_TICK(c, n) do { (c)->cycles += (n); if (PSX_UNLIKELY((c)->cycles >= (c)->next_event)) psx_poll(c); } while (0)
#define PSX_RETURN(c, t) do { (void)(t); return; } while (0)

#ifdef __cplusplus
}
#endif
