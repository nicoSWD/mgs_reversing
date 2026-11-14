// The sound processing unit: 24 ADPCM voices with ADSR envelopes, 512 KB of sound RAM, the IRQ at a
// programmable address (games use it as a stream clock) and a stereo mix pushed to the host audio device.
// Reverb, noise, frequency modulation and CD audio are not modelled yet.
#include <algorithm>
#include <cstring>

#include "runtime.h"

void host_audio_push(const int16_t* stereo, int frames);

namespace {

constexpr uint32_t kRegBase = 0x1F801C00;
constexpr int kStepSamples = 64;                      // generated per scheduler step
constexpr uint64_t kStepCycles = 768ull * kStepSamples;  // 44.1 kHz = 768 CPU cycles per sample

uint16_t g_regs[0x140];
uint8_t g_ram[512 * 1024];
uint32_t g_xfer_addr = 0;  // byte address of the manual/DMA transfer pointer
uint32_t g_irq_addr = 0;   // byte address
bool g_irq_flag = false;
uint32_t g_endx = 0;

struct Voice {
    bool on = false;
    uint32_t addr = 0;       // byte address of the block being played
    uint32_t repeat = 0;     // loop target
    int pos = 0;             // sample index in the block
    uint32_t counter = 0;    // 4.12 fixed point sample position within pos
    int16_t samples[28 + 3] = {};
    int old1 = 0, old2 = 0;
    // envelope
    int phase = 0;           // 0 attack, 1 decay, 2 sustain, 3 release, 4 off
    int level = 0;           // 0..0x7FFF
    uint32_t env_count = 0;
    int last = 0;
};
Voice g_v[24];

inline uint16_t& reg(int off) { return g_regs[off >> 1]; }
inline uint16_t vreg(int v, int r) { return g_regs[(v * 0x10 + r) >> 1]; }

const int kF0[5] = {0, 60, 115, 98, 122};
const int kF1[5] = {0, 0, -52, -55, -60};

void check_irq(Cpu* c, uint32_t byte_lo, uint32_t byte_len) {
    if (!(reg(0x1AA) & 0x40)) return;
    if (g_irq_addr >= byte_lo && g_irq_addr < byte_lo + byte_len && !g_irq_flag) {
        g_irq_flag = true;
        irq_raise(c, IRQ_SPU);
    }
}

void decode_block(Cpu* c, Voice& v) {
    uint32_t a = v.addr & 0x7FFF0;
    check_irq(c, a, 16);
    const uint8_t* b = g_ram + a;
    int shift = b[0] & 15, filter = (b[0] >> 4) & 7;
    if (shift > 12) shift = 9;
    if (filter > 4) filter = 4;
    // keep the last three samples of the previous block for interpolation
    v.samples[0] = v.samples[28];
    v.samples[1] = v.samples[29];
    v.samples[2] = v.samples[30];
    int o1 = v.old1, o2 = v.old2;
    for (int i = 0; i < 28; i++) {
        int nib = (b[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int s = (int16_t)(nib << 12) >> shift;
        s += (o1 * kF0[filter] + o2 * kF1[filter] + 32) >> 6;
        s = std::clamp(s, -32768, 32767);
        v.samples[3 + i] = (int16_t)s;
        o2 = o1;
        o1 = s;
    }
    v.old1 = o1;
    v.old2 = o2;
}

void key_on(Cpu* c, int n) {
    Voice& v = g_v[n];
    v.on = true;
    v.addr = (uint32_t)vreg(n, 6) * 8;
    v.repeat = v.addr;
    v.pos = 0;
    v.counter = 0;
    v.old1 = v.old2 = 0;
    memset(v.samples, 0, sizeof v.samples);
    v.phase = 0;
    v.level = 0;
    v.env_count = 0;
    g_endx &= ~(1u << n);
    decode_block(c, v);
    // the first block's flags decide the repeat address
    if (g_ram[(v.addr & 0x7FFF0) + 1] & 4) v.repeat = v.addr;
}

void key_off(int n) {
    if (g_v[n].on) g_v[n].phase = 3;
}

// one envelope step per sample; returns the new level
void envelope(Voice& v, int n) {
    uint16_t a1 = vreg(n, 8), a2 = vreg(n, 0xA);
    int shift, step;
    bool exp, decrease;
    int target = -1;
    switch (v.phase) {
        case 0:  // attack
            exp = a1 & 0x8000; shift = (a1 >> 10) & 31; step = 7 - ((a1 >> 8) & 3); decrease = false;
            break;
        case 1:  // decay (always exponential decrease)
            exp = true; shift = (a1 >> 4) & 15; step = -8; decrease = true;
            target = ((a1 & 15) + 1) * 0x800;
            break;
        case 2:  // sustain
            exp = a2 & 0x8000; decrease = a2 & 0x4000; shift = (a2 >> 8) & 31;
            step = decrease ? -8 + ((a2 >> 6) & 3) : 7 - ((a2 >> 6) & 3);
            break;
        case 3:  // release
            exp = a2 & 0x20; shift = a2 & 31; step = -8; decrease = true;
            break;
        default: return;
    }
    // rate: shift < 11 => step scaled up each sample; otherwise a step every 2^(shift-11) samples
    int s = step << std::max(0, 11 - shift);
    uint32_t period = shift > 11 ? (1u << (shift - 11)) : 1u;
    if (v.env_count++ % period) { if (v.phase == 1 && v.level <= target) v.phase = 2; return; }
    if (exp && decrease) s = (s * v.level) >> 15;
    else if (exp && !decrease && v.level > 0x6000) { s >>= 2; if (!s) s = 1; }
    v.level = std::clamp(v.level + s, 0, 0x7FFF);
    if (v.phase == 0 && v.level >= 0x7FFF) v.phase = 1;
    else if (v.phase == 1 && v.level <= target) v.phase = 2;
    else if (v.phase == 3 && v.level <= 0) { v.on = false; v.phase = 4; }
}

inline int vol(uint16_t r) {
    if (r & 0x8000) return 0x3FFF;  // sweep modes: approximate with a fixed level
    return (int16_t)(r << 1) >> 1 << 1;
}

void advance(Cpu* c, Voice& v, int n) {
    uint32_t pitch = std::min<uint32_t>(vreg(n, 4), 0x3FFF);
    v.counter += pitch;
    while (v.counter >= 0x1000) {
        v.counter -= 0x1000;
        if (++v.pos >= 28) {
            v.pos = 0;
            uint8_t flags = g_ram[(v.addr & 0x7FFF0) + 1];
            if (flags & 1) {            // loop end
                g_endx |= 1u << n;
                if (flags & 2) v.addr = v.repeat;      // repeat
                else { v.on = false; v.phase = 4; v.level = 0; return; }
            } else {
                v.addr = (v.addr + 16) & 0x7FFFF;
            }
            if (g_ram[(v.addr & 0x7FFF0) + 1] & 4) v.repeat = v.addr;  // loop start
            decode_block(c, v);
        }
    }
}

}  // namespace

bool spu_read(uint32_t phys, uint32_t* v, int size) {
    uint32_t off = phys - kRegBase;
    uint32_t val = 0;
    auto rd = [&](uint32_t o) -> uint32_t {
        if (o == 0x1AE) {  // SPUSTAT
            uint32_t s = reg(0x1AA) & 0x3F;
            if (g_irq_flag) s |= 0x40;
            return s;
        }
        if (o == 0x19C) return g_endx & 0xFFFF;
        if (o == 0x19E) return (g_endx >> 16) & 0xFF;
        if (o < 0x180 && (o & 0xF) == 0xC) {  // current envelope volume of a voice
            int n = o >> 4;
            return n < 24 ? (uint32_t)g_v[n].level : 0u;
        }
        if (o == 0x1A8) {
            uint16_t w;
            memcpy(&w, g_ram + (g_xfer_addr & 0x7FFFE), 2);
            return w;
        }
        return o >> 1 < 0x140 ? g_regs[o >> 1] : 0u;
    };
    val = rd(off & ~1u);
    if (size == 4) val |= rd((off & ~1u) + 2) << 16;
    *v = val;
    return true;
}

bool spu_write(Cpu* c, uint32_t phys, uint32_t v, int size) {
    uint32_t off = (phys - kRegBase) & ~1u;
    auto wr = [&](uint32_t o, uint16_t val) {
        if (o >> 1 >= 0x140) return;
        g_regs[o >> 1] = val;
        switch (o) {
            case 0x188: for (int i = 0; i < 16; i++) if (val & (1u << i)) key_on(c, i); break;
            case 0x18A: for (int i = 0; i < 8; i++) if (val & (1u << i)) key_on(c, 16 + i); break;
            case 0x18C: for (int i = 0; i < 16; i++) if (val & (1u << i)) key_off(i); break;
            case 0x18E: for (int i = 0; i < 8; i++) if (val & (1u << i)) key_off(16 + i); break;
            case 0x1A4: g_irq_addr = (uint32_t)val * 8; break;
            case 0x1A6: g_xfer_addr = (uint32_t)val * 8; break;
            case 0x1A8:
                memcpy(g_ram + (g_xfer_addr & 0x7FFFE), &val, 2);
                check_irq(c, g_xfer_addr & 0x7FFF8, 8);
                g_xfer_addr += 2;
                break;
            case 0x1AA:
                if (!(val & 0x40)) g_irq_flag = false;  // acknowledge
                break;
        }
    };
    wr(off, (uint16_t)v);
    if (size == 4) wr(off + 2, (uint16_t)(v >> 16));
    return true;
}

void spu_dma(uint32_t* words, uint32_t count, bool to_spu) {
    uint32_t bytes = count * 4;
    uint32_t a = g_xfer_addr & 0x7FFFF;
    if (a + bytes > sizeof g_ram) bytes = (uint32_t)sizeof g_ram - a;
    if (to_spu) memcpy(g_ram + a, words, bytes);
    else memcpy(words, g_ram + a, bytes);
    g_xfer_addr += bytes;
}

// one scheduler step: generate kStepSamples of audio and move the voices along
void spu_step(Cpu* c) {
    static int16_t out[kStepSamples * 2];
    bool enabled = reg(0x1AA) & 0x8000;
    bool unmuted = reg(0x1AA) & 0x4000;
    int mvl = vol(reg(0x180)), mvr = vol(reg(0x182));
    for (int i = 0; i < kStepSamples; i++) {
        int64_t l = 0, r = 0;
        if (enabled) {
            for (int n = 0; n < 24; n++) {
                Voice& v = g_v[n];
                if (!v.on) continue;
                // linear interpolation between this and the next sample
                int s0 = v.samples[3 + v.pos];
                int s1 = v.pos + 1 < 28 ? v.samples[3 + v.pos + 1] : s0;
                int s = s0 + (int)(((int64_t)(s1 - s0) * v.counter) >> 12);
                envelope(v, n);
                int e = (int)(((int64_t)s * v.level) >> 15);
                l += ((int64_t)e * vol(vreg(n, 0))) >> 15;
                r += ((int64_t)e * vol(vreg(n, 2))) >> 15;
                advance(c, v, n);
            }
        }
        l = (l * mvl) >> 15;
        r = (r * mvr) >> 15;
        if (!unmuted) l = r = 0;
        out[i * 2] = (int16_t)std::clamp<int64_t>(l, -32768, 32767);
        out[i * 2 + 1] = (int16_t)std::clamp<int64_t>(r, -32768, 32767);
    }
    host_audio_push(out, kStepSamples);
    sched_set(c, EV_SPU, c->cycles + kStepCycles);
}

void spu_init(Cpu* c) { sched_set(c, EV_SPU, c->cycles + kStepCycles); }
