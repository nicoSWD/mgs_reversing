// Geometry Transformation Engine (COP2). Register layout and command semantics follow the public
// PlayStation hardware documentation (psx-spx); arithmetic is done on 44/32-bit values with the same
// saturation and flag behaviour.
#include <algorithm>
#include <cstring>

#include "runtime.h"

namespace {

struct G {
    Cpu* c;
    int sf, lm;
    uint32_t flag = 0;
    int32_t* d;
    int32_t* k;
};

inline int16_t lo16(int32_t v) { return (int16_t)(v & 0xFFFF); }
inline int16_t hi16(int32_t v) { return (int16_t)((uint32_t)v >> 16); }

// data register names
enum { RGBC = 6, OTZ = 7, IR0 = 8, IR1 = 9, IR2 = 10, IR3 = 11, SXY0 = 12, SXY1 = 13, SXY2 = 14, SXYP = 15,
       SZ0 = 16, SZ1 = 17, SZ2 = 18, SZ3 = 19, RGB0 = 20, RGB1 = 21, RGB2 = 22, MAC0 = 24, MAC1 = 25, MAC2 = 26,
       MAC3 = 27, IRGB = 28, ORGB = 29, LZCS = 30, LZCR = 31 };
// control register names
enum { TRX = 5, TRY = 6, TRZ = 7, RBK = 13, GBK = 14, BBK = 15, RFC = 21, GFC = 22, BFC = 23, OFX = 24, OFY = 25,
       H_ = 26, DQA = 27, DQB = 28, ZSF3 = 29, ZSF4 = 30, FLAG = 31 };

constexpr uint32_t F_IR0 = 1u << 12, F_SY2 = 1u << 13, F_SX2 = 1u << 14, F_MAC0P = 1u << 15, F_MAC0N = 1u << 16,
                   F_DIV = 1u << 17, F_SZ3 = 1u << 18, F_B = 1u << 19, F_G = 1u << 20, F_R = 1u << 21,
                   F_IR3 = 1u << 22, F_IR2 = 1u << 23, F_IR1 = 1u << 24;

// 44-bit checked result for MAC1..3
inline int64_t chk44(G& g, int i, int64_t v) {
    if (v >= (1LL << 43)) g.flag |= 1u << (31 - i);        // i=1 -> bit 30
    else if (v < -(1LL << 43)) g.flag |= 1u << (28 - i);   // i=1 -> bit 27
    return (v << 20) >> 20;
}

inline int32_t set_mac(G& g, int i, int64_t v) {  // v already 44-bit-checked; shift by sf
    int32_t r = (int32_t)(v >> (g.sf * 12));
    g.d[MAC0 + i] = r;
    return r;
}

inline int32_t sat_ir(G& g, int i, int32_t v, bool lm) {
    int32_t lo = lm ? 0 : -0x8000, hi = 0x7FFF;
    if (v < lo) { g.flag |= 1u << (25 - i); return lo; }   // i=1 -> bit 24
    if (v > hi) { g.flag |= 1u << (25 - i); return hi; }
    return v;
}

inline void set_ir(G& g, int i, int32_t mac, bool lm) { g.d[IR0 + i] = sat_ir(g, i, mac, lm); }

inline uint8_t sat_col(G& g, int i, int32_t v) {  // i=0 R -> F_R ...
    static const uint32_t fl[3] = {F_R, F_G, F_B};
    if (v < 0) { g.flag |= fl[i]; return 0; }
    if (v > 255) { g.flag |= fl[i]; return 255; }
    return (uint8_t)v;
}

inline void push_color(G& g) {
    uint32_t code = (uint32_t)g.d[RGBC] >> 24;
    uint8_t r = sat_col(g, 0, g.d[MAC1] >> 4), gg = sat_col(g, 1, g.d[MAC2] >> 4), b = sat_col(g, 2, g.d[MAC3] >> 4);
    g.d[RGB0] = g.d[RGB1];
    g.d[RGB1] = g.d[RGB2];
    g.d[RGB2] = (int32_t)(r | (gg << 8) | (b << 16) | (code << 24));
}

inline int32_t mac0_check(G& g, int64_t v) {
    if (v > 0x7FFFFFFFLL) g.flag |= F_MAC0P;
    else if (v < -0x80000000LL) g.flag |= F_MAC0N;
    return (int32_t)v;
}

inline void push_sz(G& g, int32_t v) {
    g.d[SZ0] = g.d[SZ1];
    g.d[SZ1] = g.d[SZ2];
    g.d[SZ2] = g.d[SZ3];
    if (v < 0) { g.flag |= F_SZ3; v = 0; }
    else if (v > 0xFFFF) { g.flag |= F_SZ3; v = 0xFFFF; }
    g.d[SZ3] = v;
}

inline void push_sxy(G& g, int32_t x, int32_t y) {
    g.d[SXY0] = g.d[SXY1];
    g.d[SXY1] = g.d[SXY2];
    if (x < -0x400) { g.flag |= F_SX2; x = -0x400; } else if (x > 0x3FF) { g.flag |= F_SX2; x = 0x3FF; }
    if (y < -0x400) { g.flag |= F_SY2; y = -0x400; } else if (y > 0x3FF) { g.flag |= F_SY2; y = 0x3FF; }
    g.d[SXY2] = (int32_t)((uint16_t)x | ((uint32_t)(uint16_t)y << 16));
    g.d[SXYP] = g.d[SXY2];
}

uint8_t g_unr[257];
bool g_unr_init = false;
void unr_init() {
    for (int i = 0; i <= 0x100; i++) {
        int v = ((0x40000 / (i + 0x100)) + 1) / 2 - 0x101;
        g_unr[i] = (uint8_t)(v < 0 ? 0 : v);
    }
    g_unr[0x100] = 0;
    g_unr_init = true;
}

uint32_t divide(G& g, uint32_t h, uint32_t sz3) {
    if (!g_unr_init) unr_init();
    if (h >= sz3 * 2) { g.flag |= F_DIV; return 0x1FFFF; }
    int z = 0;
    for (uint32_t t = sz3; z < 16 && !(t & 0x8000); t <<= 1) z++;
    uint64_t n = (uint64_t)h << z;
    uint32_t dd = sz3 << z;
    uint32_t u = g_unr[((dd - 0x7FC0) >> 7)] + 0x101;
    int64_t dv = ((int64_t)0x2000080 - (int64_t)dd * u) >> 8;
    dv = ((int64_t)0x0000080 + dv * u) >> 8;
    uint64_t r = (n * (uint64_t)dv + 0x8000) >> 16;
    return r > 0x1FFFF ? 0x1FFFF : (uint32_t)r;
}

struct Mat { int32_t m[3][3]; };
Mat get_matrix(G& g, int sel) {
    Mat r{};
    if (sel == 3) {
        int32_t rr = (g.d[RGBC] & 0xFF) << 4;
        r.m[0][0] = -rr; r.m[0][1] = rr; r.m[0][2] = g.d[IR0];
        int16_t rt13 = lo16(g.k[1]), rt22 = lo16(g.k[2]);
        for (int j = 0; j < 3; j++) { r.m[1][j] = rt13; r.m[2][j] = rt22; }
        return r;
    }
    int b = sel * 8;  // RT at 0, LLM at 8, LCM at 16
    r.m[0][0] = lo16(g.k[b]);     r.m[0][1] = hi16(g.k[b]);
    r.m[0][2] = lo16(g.k[b + 1]); r.m[1][0] = hi16(g.k[b + 1]);
    r.m[1][1] = lo16(g.k[b + 2]); r.m[1][2] = hi16(g.k[b + 2]);
    r.m[2][0] = lo16(g.k[b + 3]); r.m[2][1] = hi16(g.k[b + 3]);
    r.m[2][2] = lo16(g.k[b + 4]);
    return r;
}

void get_vec(G& g, int sel, int32_t v[3]) {
    if (sel == 3) { v[0] = (int16_t)g.d[IR1]; v[1] = (int16_t)g.d[IR2]; v[2] = (int16_t)g.d[IR3]; return; }
    v[0] = lo16(g.d[sel * 2]); v[1] = hi16(g.d[sel * 2]); v[2] = lo16(g.d[sel * 2 + 1]);
}

// MAC/IR = (tr*0x1000 + M*V) >> sf*12, with the hardware's flag behaviour (tr: 0..2 = TR,BK,FC, 3 = none)
void mvmva_core(G& g, int mx, int vx, int cv, bool lm_ir) {
    Mat M = get_matrix(g, mx);
    int32_t V[3];
    get_vec(g, vx, V);
    int64_t T[3] = {0, 0, 0};
    if (cv < 3) {
        static const int base[3] = {TRX, RBK, RFC};
        for (int i = 0; i < 3; i++) T[i] = (int64_t)g.k[base[i] + i] * 0x1000;
    }
    for (int i = 0; i < 3; i++) {
        int64_t acc;
        if (cv == 2) {
            // far colour: the hardware flags the translation + first column, but the result only has the other two
            int64_t first = chk44(g, i + 1, T[i] + (int64_t)M.m[i][0] * V[0]);
            sat_ir(g, i + 1, (int32_t)(first >> (g.sf * 12)), false);
            acc = chk44(g, i + 1, (int64_t)M.m[i][1] * V[1]);
            acc = chk44(g, i + 1, acc + (int64_t)M.m[i][2] * V[2]);
            set_mac(g, i + 1, acc);
        } else {
            acc = T[i] + (int64_t)M.m[i][0] * V[0];
            acc = chk44(g, i + 1, acc);
            acc += (int64_t)M.m[i][1] * V[1];
            acc = chk44(g, i + 1, acc);
            acc += (int64_t)M.m[i][2] * V[2];
            acc = chk44(g, i + 1, acc);
            set_mac(g, i + 1, acc);
        }
    }
    for (int i = 0; i < 3; i++) set_ir(g, i + 1, g.d[MAC1 + i], lm_ir);
}

}  // namespace

namespace {
void rtp_clean(G& g, int vi, bool last) {
    int32_t V[3] = {lo16(g.d[vi * 2]), hi16(g.d[vi * 2]), lo16(g.d[vi * 2 + 1])};
    Mat M = get_matrix(g, 0);
    int64_t mac[3];
    for (int i = 0; i < 3; i++) {
        int64_t acc = (int64_t)g.k[TRX + i] * 0x1000 + (int64_t)M.m[i][0] * V[0];
        acc = chk44(g, i + 1, acc);
        acc += (int64_t)M.m[i][1] * V[1];
        acc = chk44(g, i + 1, acc);
        acc += (int64_t)M.m[i][2] * V[2];
        acc = chk44(g, i + 1, acc);
        mac[i] = acc;
        set_mac(g, i + 1, acc);
    }
    set_ir(g, 1, g.d[MAC1], g.lm);
    set_ir(g, 2, g.d[MAC2], g.lm);
    {
        int32_t v = (int32_t)(mac[2] >> 12);
        if (v < -0x8000 || v > 0x7FFF) g.flag |= F_IR3;
        int32_t lo = g.lm ? 0 : -0x8000;
        int32_t m3 = g.d[MAC3];
        g.d[IR3] = m3 < lo ? lo : (m3 > 0x7FFF ? 0x7FFF : m3);
    }
    push_sz(g, (int32_t)(mac[2] >> ((1 - g.sf) * 12)));
    uint32_t q = divide(g, (uint32_t)(uint16_t)g.k[H_], (uint32_t)(uint16_t)g.d[SZ3]);
    int64_t sx = (int64_t)q * (int16_t)g.d[IR1] + g.k[OFX];
    int64_t sy = (int64_t)q * (int16_t)g.d[IR2] + g.k[OFY];
    mac0_check(g, sx);
    mac0_check(g, sy);
    push_sxy(g, (int32_t)(sx >> 16), (int32_t)(sy >> 16));
    if (last) {
        int64_t m0 = (int64_t)q * (int16_t)g.k[DQA] + g.k[DQB];
        g.d[MAC0] = mac0_check(g, m0);
        int32_t v = (int32_t)(m0 >> 12);
        if (v < 0) { g.flag |= F_IR0; v = 0; } else if (v > 0x1000) { g.flag |= F_IR0; v = 0x1000; }
        g.d[IR0] = v;
    }
}

// lighting: MAC/IR = LLM*V, then BK + LCM*IR
void light(G& g, int vi) {
    mvmva_core(g, 1, vi, 3, g.lm);
}
void light_color(G& g) {
    mvmva_core(g, 2, 3, 1, g.lm);
}

void col_mul(G& g) {  // [MAC] = [R*IR1, G*IR2, B*IR3] << 4
    int32_t r = g.d[RGBC] & 0xFF, gg = (g.d[RGBC] >> 8) & 0xFF, b = (g.d[RGBC] >> 16) & 0xFF;
    int32_t ir[3] = {(int16_t)g.d[IR1], (int16_t)g.d[IR2], (int16_t)g.d[IR3]};
    int32_t col[3] = {r, gg, b};
    for (int i = 0; i < 3; i++) {
        int64_t v = chk44(g, i + 1, ((int64_t)col[i] * ir[i]) << 4);
        set_mac(g, i + 1, v);
    }
    for (int i = 0; i < 3; i++) set_ir(g, i + 1, g.d[MAC1 + i], g.lm);
}

void depth_cue(G& g, bool from_ir_color) {  // far colour interpolation with IR0
    int32_t col[3] = {g.d[RGBC] & 0xFF, (g.d[RGBC] >> 8) & 0xFF, (g.d[RGBC] >> 16) & 0xFF};
    int32_t ir[3] = {(int16_t)g.d[IR1], (int16_t)g.d[IR2], (int16_t)g.d[IR3]};
    int32_t ir0 = (int16_t)g.d[IR0];
    for (int i = 0; i < 3; i++) {
        int64_t base = from_ir_color ? ((int64_t)col[i] * ir[i]) << 4 : (int64_t)col[i] << 16;
        int64_t fcv = (int64_t)g.k[RFC + i] << 12;
        int32_t t = (int32_t)(chk44(g, i + 1, fcv - base) >> (g.sf * 12));
        t = sat_ir(g, i + 1, t, false);
        int64_t v = chk44(g, i + 1, base + (int64_t)ir0 * t);
        set_mac(g, i + 1, v);
    }
    for (int i = 0; i < 3; i++) set_ir(g, i + 1, g.d[MAC1 + i], g.lm);
}

void nc_common(G& g, int vi, int kind) {  // kind 0 NCS, 1 NCCS, 2 NCDS
    light(g, vi);
    light_color(g);
    if (kind == 1) col_mul(g);
    else if (kind == 2) depth_cue(g, true);
    push_color(g);
}
}  // namespace

// ------------------------------------------------------------------------------------ registers

extern "C" uint32_t gte_read_data(Cpu* c, int n) {
    switch (n) {
        case 1: case 3: case 5: case 8: case 9: case 10: case 11:
            return (uint32_t)(int32_t)(int16_t)c->gd[n];
        case 7: case 16: case 17: case 18: case 19:
            return (uint32_t)(uint16_t)c->gd[n];
        case 15: return (uint32_t)c->gd[14];
        case 28: case 29: {
            auto s = [&](int32_t v) { int32_t x = (int16_t)v >> 7; return (uint32_t)(x < 0 ? 0 : (x > 0x1F ? 0x1F : x)); };
            return s(c->gd[9]) | (s(c->gd[10]) << 5) | (s(c->gd[11]) << 10);
        }
        case 31: {
            uint32_t v = (uint32_t)c->gd[30];
            uint32_t x = (v & 0x80000000u) ? ~v : v;
            uint32_t n2 = 0;
            while (n2 < 32 && !(x & (0x80000000u >> n2))) n2++;
            return n2;
        }
    }
    return (uint32_t)c->gd[n];
}

extern "C" void gte_write_data(Cpu* c, int n, uint32_t v) {
    switch (n) {
        case 7: case 16: case 17: case 18: case 19: c->gd[n] = (uint16_t)v; return;
        case 15:  // SXYP: push the FIFO
            c->gd[12] = c->gd[13];
            c->gd[13] = c->gd[14];
            c->gd[14] = (int32_t)v;
            c->gd[15] = (int32_t)v;
            return;
        case 28:
            c->gd[28] = (int32_t)(v & 0x7FFF);
            c->gd[9] = (int32_t)((v & 0x1F) * 0x80);
            c->gd[10] = (int32_t)(((v >> 5) & 0x1F) * 0x80);
            c->gd[11] = (int32_t)(((v >> 10) & 0x1F) * 0x80);
            return;
        case 29: case 31: return;
    }
    c->gd[n] = (int32_t)v;
}

extern "C" uint32_t gte_read_ctrl(Cpu* c, int n) {
    switch (n) {
        case 4: case 12: case 20: case 26: case 27: case 29: case 30:
            return (uint32_t)(int32_t)(int16_t)c->gc[n];
        case 31: {
            uint32_t f = (uint32_t)c->gc[31] & 0x7FFFF000u;
            if (f & 0x7F87E000u) f |= 0x80000000u;
            return f;
        }
    }
    return (uint32_t)c->gc[n];
}

extern "C" void gte_write_ctrl(Cpu* c, int n, uint32_t v) {
    if (n == 31) { c->gc[31] = (int32_t)(v & 0x7FFFF000u); return; }
    c->gc[n] = (int32_t)v;
}

// ------------------------------------------------------------------------------------- commands

extern "C" void gte_command(Cpu* c, uint32_t cmd) {
    G g;
    g.c = c;
    g.d = c->gd;
    g.k = c->gc;
    g.sf = (cmd >> 19) & 1;
    g.lm = (cmd >> 10) & 1;
    g.flag = 0;
    switch (cmd & 0x3F) {
        case 0x01: rtp_clean(g, 0, true); break;                                  // RTPS
        case 0x06: {                                                              // NCLIP
            int32_t x0 = lo16(c->gd[12]), y0 = hi16(c->gd[12]), x1 = lo16(c->gd[13]), y1 = hi16(c->gd[13]);
            int32_t x2 = lo16(c->gd[14]), y2 = hi16(c->gd[14]);
            int64_t m = (int64_t)x0 * (y1 - y2) + (int64_t)x1 * (y2 - y0) + (int64_t)x2 * (y0 - y1);
            c->gd[MAC0] = mac0_check(g, m);
            break;
        }
        case 0x0C: {                                                              // OP
            int32_t d1 = lo16(c->gc[0]), d2 = lo16(c->gc[2]), d3 = lo16(c->gc[4]);
            int32_t i1 = (int16_t)c->gd[IR1], i2 = (int16_t)c->gd[IR2], i3 = (int16_t)c->gd[IR3];
            set_mac(g, 1, chk44(g, 1, (int64_t)d2 * i3 - (int64_t)d3 * i2));
            set_mac(g, 2, chk44(g, 2, (int64_t)d3 * i1 - (int64_t)d1 * i3));
            set_mac(g, 3, chk44(g, 3, (int64_t)d1 * i2 - (int64_t)d2 * i1));
            for (int i = 0; i < 3; i++) set_ir(g, i + 1, c->gd[MAC1 + i], g.lm);
            break;
        }
        case 0x10: {                                                              // DPCS
            depth_cue(g, false);
            push_color(g);
            break;
        }
        case 0x11: {                                                              // INTPL
            int32_t ir[3] = {(int16_t)c->gd[IR1], (int16_t)c->gd[IR2], (int16_t)c->gd[IR3]};
            int32_t ir0 = (int16_t)c->gd[IR0];
            for (int i = 0; i < 3; i++) {
                int64_t base = (int64_t)ir[i] << 12;
                int64_t fcv = (int64_t)c->gc[RFC + i] << 12;
                int32_t t = (int32_t)(chk44(g, i + 1, fcv - base) >> (g.sf * 12));
                t = sat_ir(g, i + 1, t, false);
                set_mac(g, i + 1, chk44(g, i + 1, base + (int64_t)ir0 * t));
            }
            for (int i = 0; i < 3; i++) set_ir(g, i + 1, c->gd[MAC1 + i], g.lm);
            push_color(g);
            break;
        }
        case 0x12:                                                                // MVMVA
            mvmva_core(g, (cmd >> 17) & 3, (cmd >> 15) & 3, (cmd >> 13) & 3, g.lm);
            break;
        case 0x13: nc_common(g, 0, 2); break;                                     // NCDS
        case 0x14: {                                                              // CDP
            light_color(g);
            depth_cue(g, true);
            push_color(g);
            break;
        }
        case 0x16: for (int v = 0; v < 3; v++) nc_common(g, v, 2); break;         // NCDT
        case 0x1B: nc_common(g, 0, 1); break;                                     // NCCS
        case 0x1C: {                                                              // CC
            light_color(g);
            col_mul(g);
            push_color(g);
            break;
        }
        case 0x1E: nc_common(g, 0, 0); break;                                     // NCS
        case 0x20: for (int v = 0; v < 3; v++) nc_common(g, v, 0); break;         // NCT
        case 0x28: {                                                              // SQR
            for (int i = 0; i < 3; i++) {
                int32_t v = (int16_t)c->gd[IR1 + i];
                set_mac(g, i + 1, chk44(g, i + 1, (int64_t)v * v));
            }
            for (int i = 0; i < 3; i++) set_ir(g, i + 1, c->gd[MAC1 + i], g.lm);
            break;
        }
        case 0x29: {                                                              // DCPL
            depth_cue(g, true);
            push_color(g);
            break;
        }
        case 0x2A: for (int v = 0; v < 3; v++) { depth_cue(g, false); push_color(g); } break;  // DPCT
        case 0x2D: {                                                              // AVSZ3
            int64_t m = (int64_t)(int16_t)c->gc[ZSF3] * ((int32_t)(uint16_t)c->gd[SZ1] + (uint16_t)c->gd[SZ2] + (uint16_t)c->gd[SZ3]);
            c->gd[MAC0] = mac0_check(g, m);
            int32_t o = (int32_t)(m >> 12);
            if (o < 0) { g.flag |= F_SZ3; o = 0; } else if (o > 0xFFFF) { g.flag |= F_SZ3; o = 0xFFFF; }
            c->gd[OTZ] = o;
            break;
        }
        case 0x2E: {                                                              // AVSZ4
            int64_t m = (int64_t)(int16_t)c->gc[ZSF4] * ((int32_t)(uint16_t)c->gd[SZ0] + (uint16_t)c->gd[SZ1] + (uint16_t)c->gd[SZ2] + (uint16_t)c->gd[SZ3]);
            c->gd[MAC0] = mac0_check(g, m);
            int32_t o = (int32_t)(m >> 12);
            if (o < 0) { g.flag |= F_SZ3; o = 0; } else if (o > 0xFFFF) { g.flag |= F_SZ3; o = 0xFFFF; }
            c->gd[OTZ] = o;
            break;
        }
        case 0x30:                                                                // RTPT
            for (int v = 0; v < 3; v++) rtp_clean(g, v, v == 2);
            break;
        case 0x3D: {                                                              // GPF
            int32_t ir0 = (int16_t)c->gd[IR0];
            for (int i = 0; i < 3; i++) set_mac(g, i + 1, chk44(g, i + 1, (int64_t)(int16_t)c->gd[IR1 + i] * ir0));
            for (int i = 0; i < 3; i++) set_ir(g, i + 1, c->gd[MAC1 + i], g.lm);
            push_color(g);
            break;
        }
        case 0x3E: {                                                              // GPL
            int32_t ir0 = (int16_t)c->gd[IR0];
            for (int i = 0; i < 3; i++) {
                int64_t v = ((int64_t)c->gd[MAC1 + i] << (g.sf * 12)) + (int64_t)(int16_t)c->gd[IR1 + i] * ir0;
                set_mac(g, i + 1, chk44(g, i + 1, v));
            }
            for (int i = 0; i < 3; i++) set_ir(g, i + 1, c->gd[MAC1 + i], g.lm);
            push_color(g);
            break;
        }
        case 0x3F: for (int v = 0; v < 3; v++) nc_common(g, v, 1); break;         // NCCT
        default: break;
    }
    if (g.flag & 0x7F87E000u) g.flag |= 0x80000000u;
    c->gc[FLAG] = (int32_t)g.flag;
}
