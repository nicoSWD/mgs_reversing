// The motion decoder (MDEC): run-length decoded macroblocks -> dequantisation -> inverse DCT -> colour
// conversion, fed and drained by DMA channels 0 and 1. Algorithm per the public PlayStation hardware docs.
#include <algorithm>
#include <cstring>
#include <vector>

#include "runtime.h"

namespace {

const uint8_t kZigzag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
                             41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                             30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

uint8_t g_qt_luma[64], g_qt_chroma[64];
int16_t g_scale[64];
bool g_have_scale = false;

// command state
enum Mode { M_IDLE, M_DECODE, M_QUANT, M_SCALE } g_mode = M_IDLE;
uint32_t g_words_left = 0;
bool g_quant_color = false;
int g_depth = 3;  // 0 = 4 bit, 1 = 8 bit, 2 = 24 bit, 3 = 15 bit
bool g_signed = false, g_bit15 = false;
std::vector<uint16_t> g_rle;
std::vector<uint8_t> g_quant_bytes;
std::vector<int16_t> g_scale_in;
std::vector<uint8_t> g_out;
size_t g_out_pos = 0;
uint32_t g_ctrl = 0;

inline int sx10(int v) { return (v << 22) >> 22; }

// decodes one 8x8 block from the stream; returns false at the end of data
bool decode_block(const std::vector<uint16_t>& s, size_t& pos, const uint8_t* qt, int16_t* coef) {
    memset(coef, 0, 64 * sizeof(int16_t));
    while (pos < s.size() && s[pos] == 0xFE00) pos++;  // padding
    if (pos >= s.size()) return false;
    uint16_t hw = s[pos++];
    int qscale = (hw >> 10) & 63;
    int dc = sx10(hw & 0x3FF);
    int val = dc * qt[0];
    coef[0] = (int16_t)std::clamp(val, -0x400 * 8, 0x3FF * 8);
    if (qscale == 0) coef[0] = (int16_t)std::clamp(dc * 2, -0x400, 0x3FF);
    int n = 0;
    while (pos < s.size()) {
        hw = s[pos++];
        if (hw == 0xFE00) break;
        n += ((hw >> 10) & 63) + 1;
        if (n > 63) break;
        int level = sx10(hw & 0x3FF);
        int v = qscale == 0 ? level * 2 : (level * qt[n] * qscale + 4) >> 3;
        coef[kZigzag[n]] = (int16_t)std::clamp(v, -0x400, 0x3FF);
    }
    return true;
}

void idct(const int16_t* in, int8_t* out) {
    int32_t tmp[64];
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            int64_t sum = 0;
            for (int z = 0; z < 8; z++) sum += (int64_t)in[y + z * 8] * g_scale[x + z * 8];
            tmp[x + y * 8] = (int32_t)((sum + 0x8000) >> 16);
        }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            int64_t sum = 0;
            for (int z = 0; z < 8; z++) sum += (int64_t)tmp[y + z * 8] * g_scale[x + z * 8];
            int v = (int)((sum + 0x8000) >> 16);
            out[x + y * 8] = (int8_t)std::clamp(v, -128, 127);
        }
}

inline int clamp8(int v) { return std::clamp(v, -128, 127); }

void decode_all() {
    size_t pos = 0;
    g_out.clear();
    g_out_pos = 0;
    if (!g_have_scale) {  // the standard table if the program never loaded one
        for (int u = 0; u < 8; u++)
            for (int x = 0; x < 8; x++) {
                double c = u == 0 ? 0.70710678 : 1.0;
                g_scale[x + u * 8] = (int16_t)(c * __builtin_cos((2 * x + 1) * u * 3.14159265358979 / 16.0) * 32768.0 * (u == 0 ? 1.0 : 1.0));
            }
        g_have_scale = true;
    }
    int16_t coef[64];
    int8_t blk[6][64];
    if (g_depth >= 2) {
        while (pos < g_rle.size()) {
            bool ok = true;
            for (int b = 0; b < 6 && ok; b++) {
                if (!decode_block(g_rle, pos, b < 2 ? g_qt_chroma : g_qt_luma, coef)) { ok = false; break; }
                idct(coef, blk[b]);
            }
            if (!ok) break;
            // blk[0] = Cr, blk[1] = Cb, blk[2..5] = Y top-left, top-right, bottom-left, bottom-right
            uint8_t px[16 * 16 * 3];
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    int yb = 2 + (y >> 3) * 2 + (x >> 3);
                    int Y = blk[yb][(y & 7) * 8 + (x & 7)];
                    int ci = (y >> 1) * 8 + (x >> 1);
                    int cr = blk[0][ci], cb = blk[1][ci];
                    int r = clamp8(Y + ((91881 * cr + 32768) >> 16));
                    int g = clamp8(Y - ((22525 * cb + 46812 * cr + 32768) >> 16));
                    int b2 = clamp8(Y + ((116130 * cb + 32768) >> 16));
                    if (!g_signed) { r += 128; g += 128; b2 += 128; }
                    int o = (y * 16 + x) * 3;
                    px[o] = (uint8_t)r; px[o + 1] = (uint8_t)g; px[o + 2] = (uint8_t)b2;
                }
            if (g_depth == 2) {
                g_out.insert(g_out.end(), px, px + sizeof px);
            } else {
                for (int i = 0; i < 256; i++) {
                    uint16_t v = (uint16_t)((px[i * 3] >> 3) | ((px[i * 3 + 1] >> 3) << 5) | ((px[i * 3 + 2] >> 3) << 10) |
                                            (g_bit15 ? 0x8000 : 0));
                    g_out.push_back((uint8_t)v);
                    g_out.push_back((uint8_t)(v >> 8));
                }
            }
        }
    } else {
        while (pos < g_rle.size()) {  // monochrome: one luma block per 8x8
            if (!decode_block(g_rle, pos, g_qt_luma, coef)) break;
            int8_t y8[64];
            idct(coef, y8);
            if (g_depth == 1) {
                for (int i = 0; i < 64; i++) g_out.push_back((uint8_t)(g_signed ? y8[i] : y8[i] + 128));
            } else {
                for (int i = 0; i < 64; i += 2) g_out.push_back((uint8_t)(((y8[i] + 128) >> 4) | (((y8[i + 1] + 128) >> 4) << 4)));
            }
        }
    }
    g_rle.clear();
}

void feed_word(uint32_t w) {
    switch (g_mode) {
        case M_IDLE: {
            uint32_t cmd = w >> 29;
            if (cmd == 1) {
                g_mode = M_DECODE;
                g_depth = (w >> 27) & 3;
                g_signed = (w >> 26) & 1;
                g_bit15 = (w >> 25) & 1;
                g_words_left = w & 0xFFFF;
                g_rle.clear();
                if (!g_words_left) g_mode = M_IDLE;
            } else if (cmd == 2) {
                g_mode = M_QUANT;
                g_quant_color = w & 1;
                g_words_left = g_quant_color ? 32 : 16;
                g_quant_bytes.clear();
            } else if (cmd == 3) {
                g_mode = M_SCALE;
                g_words_left = 32;
                g_scale_in.clear();
            }
            break;
        }
        case M_DECODE:
            g_rle.push_back((uint16_t)w);
            g_rle.push_back((uint16_t)(w >> 16));
            if (--g_words_left == 0) { decode_all(); g_mode = M_IDLE; }
            break;
        case M_QUANT:
            for (int i = 0; i < 4; i++) g_quant_bytes.push_back((uint8_t)(w >> (8 * i)));
            if (--g_words_left == 0) {
                memcpy(g_qt_luma, g_quant_bytes.data(), 64);
                if (g_quant_color) memcpy(g_qt_chroma, g_quant_bytes.data() + 64, 64);
                g_mode = M_IDLE;
            }
            break;
        case M_SCALE:
            g_scale_in.push_back((int16_t)w);
            g_scale_in.push_back((int16_t)(w >> 16));
            if (--g_words_left == 0) {
                for (int i = 0; i < 64; i++) g_scale[i] = g_scale_in[i];
                g_have_scale = true;
                g_mode = M_IDLE;
            }
            break;
    }
}

}  // namespace

bool mdec_read(uint32_t phys, uint32_t* v) {
    if ((phys & 0xF) == 0) {  // data register: output words
        uint32_t w = 0;
        for (int i = 0; i < 4; i++) { if (g_out_pos < g_out.size()) w |= (uint32_t)g_out[g_out_pos++] << (8 * i); }
        *v = w;
        return true;
    }
    uint32_t s = 0;
    bool empty = g_out_pos >= g_out.size();
    if (empty) s |= 0x80000000u;
    if (g_mode == M_DECODE) s |= 0x20000000u;
    if (g_ctrl & 0x40000000u) s |= 0x10000000u;
    if (!empty && (g_ctrl & 0x20000000u)) s |= 0x08000000u;
    s |= (uint32_t)g_depth << 25;
    s |= (g_signed ? 1u : 0u) << 24;
    s |= (g_bit15 ? 1u : 0u) << 23;
    s |= g_mode == M_IDLE ? 0xFFFFu : ((g_words_left - 1) & 0xFFFF);
    *v = s;
    return true;
}

bool mdec_write(Cpu*, uint32_t phys, uint32_t v) {
    if ((phys & 0xF) == 0) { feed_word(v); return true; }
    if (v & 0x80000000u) {  // reset
        g_mode = M_IDLE;
        g_out.clear();
        g_out_pos = 0;
        g_rle.clear();
        v &= ~0x80000000u;
    }
    g_ctrl = v;
    return true;
}

void mdec_dma_in(const uint32_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) feed_word(words[i]);
}

void mdec_dma_out(uint32_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t w = 0;
        for (int k = 0; k < 4; k++) if (g_out_pos < g_out.size()) w |= (uint32_t)g_out[g_out_pos++] << (8 * k);
        words[i] = w;
    }
}
