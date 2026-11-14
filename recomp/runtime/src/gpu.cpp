// The GPU: 1024x512 16-bit VRAM, the GP0/GP1 command interfaces and a software rasterizer.
#include <algorithm>
#include <cstring>
#include <vector>

#include "runtime.h"

namespace {

uint16_t g_vram[1024 * 512];

// drawing environment
int g_page_x = 0, g_page_y = 0, g_semi = 0, g_depth = 0;
bool g_dither = false, g_draw_display = false, g_set_mask = false, g_check_mask = false, g_tex_disable = false;
int g_twin_mx = 0, g_twin_my = 0, g_twin_ox = 0, g_twin_oy = 0;
int g_area_x1 = 0, g_area_y1 = 0, g_area_x2 = 1023, g_area_y2 = 511;
int g_off_x = 0, g_off_y = 0;
uint32_t g_tw_reg = 0, g_ta_reg = 0, g_tb_reg = 0, g_to_reg = 0;

// display
int g_disp_x = 0, g_disp_y = 0;
int g_hres = 320, g_vres = 240;
bool g_disp_on = false, g_rgb24 = false, g_interlace = false, g_pal = false;
uint32_t g_mode_reg = 0;
int g_dma_dir = 0;
bool g_irq1 = false;
uint32_t g_read_latch = 0;

// command state
std::vector<uint32_t> g_fifo;
int g_need = 0;               // words still needed for the current command (0 = idle)
bool g_polyline = false;
// CPU->VRAM / VRAM->CPU
bool g_wr_active = false, g_rd_active = false;
int g_tx, g_ty, g_tw, g_th, g_tpos;
uint32_t g_rd_word_count;

inline int sx11(uint32_t v) { return ((int)(v & 0x7FF) << 21) >> 21; }

struct Vtx { int x, y; int r, g, b; int u, v; };

inline uint16_t pack555(int r, int g, int b) { return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)); }

const int kDither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};

inline uint16_t blend(uint16_t back, int fr, int fg, int fb, int mode) {
    int br = (back & 31) << 3, bg = ((back >> 5) & 31) << 3, bb = ((back >> 10) & 31) << 3;
    int r, g, b;
    switch (mode) {
        case 0: r = (br + fr) / 2; g = (bg + fg) / 2; b = (bb + fb) / 2; break;
        case 1: r = br + fr; g = bg + fg; b = bb + fb; break;
        case 2: r = br - fr; g = bg - fg; b = bb - fb; break;
        default: r = br + fr / 4; g = bg + fg / 4; b = bb + fb / 4; break;
    }
    r = std::clamp(r, 0, 255); g = std::clamp(g, 0, 255); b = std::clamp(b, 0, 255);
    return pack555(r, g, b);
}

inline void put_pixel(int x, int y, int r, int g, int b, bool semi, int semi_mode, bool dither, uint16_t force_mask_bit) {
    if (x < g_area_x1 || x > g_area_x2 || y < g_area_y1 || y > g_area_y2) return;
    if (x < 0 || x > 1023 || y < 0 || y > 511) return;
    uint16_t& dst = g_vram[y * 1024 + x];
    if (g_check_mask && (dst & 0x8000)) return;
    if (dither) {
        int d = kDither[y & 3][x & 3];
        r = std::clamp(r + d, 0, 255); g = std::clamp(g + d, 0, 255); b = std::clamp(b + d, 0, 255);
    }
    uint16_t px;
    if (semi) px = blend(dst, r, g, b, semi_mode);
    else px = pack555(r, g, b);
    px |= (g_set_mask ? 0x8000 : 0) | force_mask_bit;
    dst = px;
}

inline uint16_t fetch_texel(int u, int v, int clut_x, int clut_y, int page_x, int page_y, int depth) {
    u = (u & ~(g_twin_mx * 8)) | ((g_twin_ox & g_twin_mx) * 8);
    v = (v & ~(g_twin_my * 8)) | ((g_twin_oy & g_twin_my) * 8);
    u &= 255; v &= 255;
    int by = (page_y * 256 + v) & 511;
    if (depth == 0) {
        uint16_t hw = g_vram[by * 1024 + ((page_x * 64 + (u >> 2)) & 1023)];
        int idx = (hw >> ((u & 3) * 4)) & 15;
        return g_vram[(clut_y & 511) * 1024 + ((clut_x * 16 + idx) & 1023)];
    }
    if (depth == 1) {
        uint16_t hw = g_vram[by * 1024 + ((page_x * 64 + (u >> 1)) & 1023)];
        int idx = (hw >> ((u & 1) * 8)) & 255;
        return g_vram[(clut_y & 511) * 1024 + ((clut_x * 16 + idx) & 1023)];
    }
    return g_vram[by * 1024 + ((page_x * 64 + u) & 1023)];
}

struct PolyState {
    bool gouraud, textured, semi, raw;
    int clut_x, clut_y, page_x, page_y, depth, semi_mode;
};

void draw_triangle(const Vtx* v, const PolyState& s) {
    int x0 = v[0].x, y0 = v[0].y, x1 = v[1].x, y1 = v[1].y, x2 = v[2].x, y2 = v[2].y;
    int64_t area = (int64_t)(x1 - x0) * (y2 - y0) - (int64_t)(x2 - x0) * (y1 - y0);
    if (area == 0) return;
    int minx = std::max(std::min({x0, x1, x2}), g_area_x1), maxx = std::min(std::max({x0, x1, x2}), g_area_x2);
    int miny = std::max(std::min({y0, y1, y2}), g_area_y1), maxy = std::min(std::max({y0, y1, y2}), g_area_y2);
    if (std::max({x0, x1, x2}) - std::min({x0, x1, x2}) > 1023 || std::max({y0, y1, y2}) - std::min({y0, y1, y2}) > 511) return;
    minx = std::max(minx, 0); miny = std::max(miny, 0); maxx = std::min(maxx, 1023); maxy = std::min(maxy, 511);
    // top-left fill rule via a bias on the edge functions
    auto bias = [&](int xa, int ya, int xb, int yb) {
        // edge a->b; for a clockwise (area<0 flips) triangle the owning edges are top or left
        int dx = xb - xa, dy = yb - ya;
        bool top_left = (area > 0) ? ((dy < 0) || (dy == 0 && dx > 0)) : ((dy > 0) || (dy == 0 && dx < 0));
        return top_left ? 0 : -1;
    };
    int b0 = bias(x1, y1, x2, y2), b1 = bias(x2, y2, x0, y0), b2 = bias(x0, y0, x1, y1);
    int sgn = area > 0 ? 1 : -1;
    int64_t a = area * sgn;
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int64_t w0 = ((int64_t)(x2 - x1) * (y - y1) - (int64_t)(y2 - y1) * (x - x1)) * sgn;
            int64_t w1 = ((int64_t)(x0 - x2) * (y - y2) - (int64_t)(y0 - y2) * (x - x2)) * sgn;
            int64_t w2 = ((int64_t)(x1 - x0) * (y - y0) - (int64_t)(y1 - y0) * (x - x0)) * sgn;
            if (w0 + b0 < 0 || w1 + b1 < 0 || w2 + b2 < 0) continue;
            int r, g, b;
            if (s.gouraud) {
                r = (int)((w0 * v[0].r + w1 * v[1].r + w2 * v[2].r) / a);
                g = (int)((w0 * v[0].g + w1 * v[1].g + w2 * v[2].g) / a);
                b = (int)((w0 * v[0].b + w1 * v[1].b + w2 * v[2].b) / a);
            } else { r = v[0].r; g = v[0].g; b = v[0].b; }
            bool semi = s.semi;
            if (s.textured) {
                int u = (int)((w0 * v[0].u + w1 * v[1].u + w2 * v[2].u + a / 2) / a);
                int vv = (int)((w0 * v[0].v + w1 * v[1].v + w2 * v[2].v + a / 2) / a);
                uint16_t t = fetch_texel(u, vv, s.clut_x, s.clut_y, s.page_x, s.page_y, s.depth);
                if (t == 0) continue;
                int tr = (t & 31) << 3, tg = ((t >> 5) & 31) << 3, tb = ((t >> 10) & 31) << 3;
                if (!s.raw) { tr = std::min(255, tr * r / 128); tg = std::min(255, tg * g / 128); tb = std::min(255, tb * b / 128); }
                r = tr; g = tg; b = tb;
                semi = s.semi && (t & 0x8000);
                put_pixel(x, y, r, g, b, semi, s.semi_mode, g_dither && !s.raw, 0);
                continue;
            }
            put_pixel(x, y, std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), semi, s.semi_mode,
                      g_dither && s.gouraud, 0);
        }
    }
}

void draw_line(const Vtx& a, const Vtx& b, bool gouraud, bool semi, int semi_mode) {
    int dx = std::abs(b.x - a.x), dy = std::abs(b.y - a.y);
    int n = std::max(dx, dy);
    if (dx > 1023 || dy > 511) return;
    for (int i = 0; i <= n; i++) {
        int x = n ? a.x + (b.x - a.x) * i / n : a.x;
        int y = n ? a.y + (b.y - a.y) * i / n : a.y;
        int r = a.r, g = a.g, bl = a.b;
        if (gouraud && n) { r = a.r + (b.r - a.r) * i / n; g = a.g + (b.g - a.g) * i / n; bl = a.b + (b.b - a.b) * i / n; }
        put_pixel(x, y, r, g, bl, semi, semi_mode, g_dither && gouraud, 0);
    }
}

void draw_rect(int x, int y, int w, int h, int r, int g, int b, bool textured, bool raw, bool semi, int u0, int v0,
               int clut_x, int clut_y) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j;
            if (px < g_area_x1 || px > g_area_x2 || py < g_area_y1 || py > g_area_y2) continue;
            if (textured) {
                uint16_t t = fetch_texel((u0 + i) & 255, (v0 + j) & 255, clut_x, clut_y, g_page_x, g_page_y, g_depth);
                if (t == 0) continue;
                int tr = (t & 31) << 3, tg = ((t >> 5) & 31) << 3, tb = ((t >> 10) & 31) << 3;
                if (!raw) { tr = std::min(255, tr * r / 128); tg = std::min(255, tg * g / 128); tb = std::min(255, tb * b / 128); }
                put_pixel(px, py, tr, tg, tb, semi && (t & 0x8000), g_semi, false, 0);
            } else {
                put_pixel(px, py, r, g, b, semi, g_semi, false, 0);
            }
        }
    }
}

void fill_rect(uint32_t color, uint32_t xy, uint32_t wh) {
    int x = xy & 0x3F0, y = (xy >> 16) & 0x1FF;
    int w = ((wh & 0x3FF) + 0xF) & ~0xF, h = (wh >> 16) & 0x1FF;
    uint16_t px = pack555(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) g_vram[((y + j) & 511) * 1024 + ((x + i) & 1023)] = px;
}

void copy_rect(uint32_t sxy, uint32_t dxy, uint32_t wh) {
    int sx = sxy & 0x3FF, sy = (sxy >> 16) & 0x1FF, dx = dxy & 0x3FF, dy = (dxy >> 16) & 0x1FF;
    int w = wh & 0x3FF, h = (wh >> 16) & 0x1FF;
    if (!w) w = 1024;
    if (!h) h = 512;
    std::vector<uint16_t> tmp((size_t)w * h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) tmp[(size_t)j * w + i] = g_vram[((sy + j) & 511) * 1024 + ((sx + i) & 1023)];
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            uint16_t& d = g_vram[((dy + j) & 511) * 1024 + ((dx + i) & 1023)];
            if (g_check_mask && (d & 0x8000)) continue;
            d = tmp[(size_t)j * w + i] | (g_set_mask ? 0x8000 : 0);
        }
}

void set_texpage(uint32_t v) {
    g_page_x = v & 15;
    g_page_y = (v >> 4) & 1;
    g_semi = (v >> 5) & 3;
    g_depth = (v >> 7) & 3;
    if (g_depth == 3) g_depth = 2;
    g_dither = (v >> 9) & 1;
    g_draw_display = (v >> 10) & 1;
    g_tex_disable = (v >> 11) & 1;
}

void exec_polygon(const std::vector<uint32_t>& f) {
    uint32_t cmd = f[0] >> 24;
    PolyState s{};
    s.gouraud = cmd & 0x10;
    bool quad = cmd & 0x08;
    s.textured = cmd & 0x04;
    s.semi = cmd & 0x02;
    s.raw = cmd & 0x01;
    int n = quad ? 4 : 3;
    Vtx v[4];
    size_t p = 0;
    for (int i = 0; i < n; i++) {
        uint32_t col = (i == 0 || s.gouraud) ? f[p++] : f[0];
        if (i == 0) col = f[0], p = 1;
        uint32_t xy = f[p++];
        v[i].x = sx11(xy) + g_off_x;
        v[i].y = sx11(xy >> 16) + g_off_y;
        v[i].r = col & 0xFF; v[i].g = (col >> 8) & 0xFF; v[i].b = (col >> 16) & 0xFF;
        if (!s.gouraud) { v[i].r = f[0] & 0xFF; v[i].g = (f[0] >> 8) & 0xFF; v[i].b = (f[0] >> 16) & 0xFF; }
        if (s.textured) {
            uint32_t t = f[p++];
            v[i].u = t & 0xFF; v[i].v = (t >> 8) & 0xFF;
            if (i == 0) { s.clut_x = (t >> 16) & 63; s.clut_y = (t >> 22) & 511; }
            if (i == 1) {
                s.page_x = (t >> 16) & 15; s.page_y = (t >> 20) & 1; s.semi_mode = (t >> 21) & 3;
                int d = (t >> 23) & 3; s.depth = d == 3 ? 2 : d;
                g_page_x = s.page_x; g_page_y = s.page_y; g_semi = s.semi_mode; g_depth = s.depth;
            }
        }
    }
    if (!s.textured) s.semi_mode = g_semi;
    draw_triangle(v, s);
    if (quad) {
        Vtx t[3] = {v[1], v[2], v[3]};
        draw_triangle(t, s);
    }
}

void exec_line(const std::vector<uint32_t>& f) {
    uint32_t cmd = f[0] >> 24;
    bool gouraud = cmd & 0x10, poly = cmd & 0x08, semi = cmd & 0x02;
    Vtx prev{};
    size_t p = 0;
    bool first = true;
    uint32_t col = f[0];
    while (p < f.size()) {
        if (poly && (f[p] & 0xF000F000u) == 0x50005000u && p > 0) break;
        Vtx cur{};
        if (first) { col = f[p]; p++; }
        else if (gouraud) { col = f[p]; p++; }
        if (p >= f.size()) break;
        uint32_t xy = f[p++];
        cur.x = sx11(xy) + g_off_x; cur.y = sx11(xy >> 16) + g_off_y;
        cur.r = col & 0xFF; cur.g = (col >> 8) & 0xFF; cur.b = (col >> 16) & 0xFF;
        if (!first) draw_line(prev, cur, gouraud, semi, g_semi);
        prev = cur;
        first = false;
        if (!poly && p >= f.size()) break;
    }
}

void exec_rect(const std::vector<uint32_t>& f) {
    uint32_t cmd = f[0] >> 24;
    bool textured = cmd & 4, semi = cmd & 2, raw = cmd & 1;
    int size = (cmd >> 3) & 3;
    uint32_t xy = f[1];
    int x = sx11(xy) + g_off_x, y = sx11(xy >> 16) + g_off_y;
    size_t p = 2;
    int u = 0, v = 0, cx = 0, cy = 0;
    if (textured) { uint32_t t = f[p++]; u = t & 0xFF; v = (t >> 8) & 0xFF; cx = (t >> 16) & 63; cy = (t >> 22) & 511; }
    int w = 1, h = 1;
    if (size == 0) { uint32_t wh = f[p++]; w = wh & 0x3FF; h = (wh >> 16) & 0x1FF; }
    else if (size == 2) w = h = 8;
    else if (size == 3) w = h = 16;
    draw_rect(x, y, w, h, f[0] & 0xFF, (f[0] >> 8) & 0xFF, (f[0] >> 16) & 0xFF, textured, raw, semi, u, v, cx, cy);
}

int command_len(uint32_t w, bool* poly) {
    uint32_t cmd = w >> 24;
    *poly = false;
    if (cmd >= 0x20 && cmd < 0x40) {
        bool g = cmd & 0x10, q = cmd & 8, t = cmd & 4;
        int n = q ? 4 : 3;
        return 1 + n * (1 + (t ? 1 : 0)) + (n - 1) * (g ? 1 : 0);
    }
    if (cmd >= 0x40 && cmd < 0x60) {
        bool g = cmd & 0x10;
        if (cmd & 8) { *poly = true; return 0; }
        return g ? 4 : 3;
    }
    if (cmd >= 0x60 && cmd < 0x80) {
        bool t = cmd & 4;
        int size = (cmd >> 3) & 3;
        return 2 + (t ? 1 : 0) + (size == 0 ? 1 : 0);
    }
    if (cmd == 0x02) return 3;
    if (cmd >= 0x80 && cmd < 0xA0) return 4;
    if (cmd >= 0xA0 && cmd < 0xC0) return 3;
    if (cmd >= 0xC0 && cmd < 0xE0) return 3;
    return 1;
}

void exec_command() {
    const auto& f = g_fifo;
    uint32_t cmd = f[0] >> 24;
    if (g_log_gpu) {
        char buf[256]; int n = 0;
        for (size_t i = 0; i < f.size() && n < 230; i++) n += snprintf(buf + n, sizeof buf - n, " %08X", f[i]);
        logf("gpu", "cmd%s", buf);
    }
    if (cmd >= 0x20 && cmd < 0x40) exec_polygon(f);
    else if (cmd >= 0x40 && cmd < 0x60) exec_line(f);
    else if (cmd >= 0x60 && cmd < 0x80) exec_rect(f);
    else if (cmd == 0x02) fill_rect(f[0] & 0xFFFFFF, f[1], f[2]);
    else if (cmd >= 0x80 && cmd < 0xA0) copy_rect(f[1], f[2], f[3]);
    else if (cmd >= 0xA0 && cmd < 0xC0) {
        g_tx = f[1] & 0x3FF; g_ty = (f[1] >> 16) & 0x1FF;
        g_tw = f[2] & 0x3FF; g_th = (f[2] >> 16) & 0x1FF;
        if (!g_tw) g_tw = 1024;
        if (!g_th) g_th = 512;
        g_tpos = 0;
        g_wr_active = true;
    } else if (cmd >= 0xC0 && cmd < 0xE0) {
        g_tx = f[1] & 0x3FF; g_ty = (f[1] >> 16) & 0x1FF;
        g_tw = f[2] & 0x3FF; g_th = (f[2] >> 16) & 0x1FF;
        if (!g_tw) g_tw = 1024;
        if (!g_th) g_th = 512;
        g_tpos = 0;
        g_rd_active = true;
    }
}

void write_vram_pixel(uint16_t v) {
    int x = (g_tx + g_tpos % g_tw) & 1023, y = (g_ty + g_tpos / g_tw) & 511;
    uint16_t& d = g_vram[y * 1024 + x];
    if (!(g_check_mask && (d & 0x8000))) d = v | (g_set_mask ? 0x8000 : 0);
    g_tpos++;
    if (g_tpos >= g_tw * g_th) g_wr_active = false;
}

}  // namespace

void gpu_init() {
    memset(g_vram, 0, sizeof g_vram);
    g_fifo.clear();
    g_need = 0;
    g_wr_active = g_rd_active = false;
}

const uint16_t* gpu_vram() { return g_vram; }

void gpu_write_gp0(uint32_t v) {
    if (g_wr_active) {
        write_vram_pixel((uint16_t)v);
        if (g_wr_active) write_vram_pixel((uint16_t)(v >> 16));
        else if (false) {}
        return;
    }
    if (g_need == 0 && !g_polyline) {
        uint32_t cmd = v >> 24;
        if (cmd == 0x00 || cmd == 0x01 || cmd == 0x1F || (cmd >= 0x03 && cmd <= 0x1E)) return;
        if (cmd >= 0xE0) {
            LOG(gpu, "env %08X", v);
            switch (cmd) {
                case 0xE1: set_texpage(v); break;
                case 0xE2: g_twin_mx = v & 31; g_twin_my = (v >> 5) & 31; g_twin_ox = (v >> 10) & 31; g_twin_oy = (v >> 15) & 31; g_tw_reg = v; break;
                case 0xE3: g_area_x1 = v & 0x3FF; g_area_y1 = (v >> 10) & 0x3FF; g_ta_reg = v; break;
                case 0xE4: g_area_x2 = v & 0x3FF; g_area_y2 = (v >> 10) & 0x3FF; g_tb_reg = v; break;
                case 0xE5: g_off_x = ((int)(v & 0x7FF) << 21) >> 21; g_off_y = ((int)((v >> 11) & 0x7FF) << 21) >> 21; g_to_reg = v; break;
                case 0xE6: g_set_mask = v & 1; g_check_mask = (v >> 1) & 1; break;
            }
            return;
        }
        g_fifo.clear();
        bool poly;
        g_need = command_len(v, &poly);
        g_polyline = poly;
        g_fifo.push_back(v);
        if (!g_polyline) {
            g_need--;
            if (g_need == 0) { exec_command(); g_fifo.clear(); }
        }
        return;
    }
    g_fifo.push_back(v);
    if (g_polyline) {
        bool gouraud = (g_fifo[0] >> 24) & 0x10;
        if ((v & 0xF000F000u) == 0x50005000u && g_fifo.size() > (gouraud ? 4u : 3u)) {
            exec_command();
            g_fifo.clear();
            g_polyline = false;
        }
        return;
    }
    if (--g_need == 0) {
        exec_command();
        g_fifo.clear();
    }
}

void gpu_write_gp1(uint32_t v) {
    uint32_t cmd = v >> 24;
    LOG(gpu, "GP1 %08X", v);
    switch (cmd) {
        case 0x00:
            set_texpage(0);
            g_twin_mx = g_twin_my = g_twin_ox = g_twin_oy = 0;
            g_area_x1 = g_area_y1 = 0; g_area_x2 = 1023; g_area_y2 = 511;
            g_off_x = g_off_y = 0;
            g_set_mask = g_check_mask = false;
            g_disp_on = false;
            g_fifo.clear(); g_need = 0; g_polyline = false; g_wr_active = g_rd_active = false;
            g_dma_dir = 0; g_irq1 = false;
            break;
        case 0x01: g_fifo.clear(); g_need = 0; g_polyline = false; g_wr_active = g_rd_active = false; break;
        case 0x02: g_irq1 = false; break;
        case 0x03: g_disp_on = !(v & 1); break;
        case 0x04: g_dma_dir = v & 3; break;
        case 0x05: g_disp_x = v & 0x3FF; g_disp_y = (v >> 10) & 0x1FF; break;
        case 0x08: {
            g_mode_reg = v;
            static const int hr[4] = {256, 320, 512, 640};
            g_hres = (v & 0x40) ? 368 : hr[v & 3];
            g_vres = (v & 4) ? 480 : 240;
            g_pal = v & 8;
            g_rgb24 = v & 0x10;
            g_interlace = v & 0x20;
            break;
        }
        case 0x10: {
            switch (v & 0xFFFFFF) {
                case 2: g_read_latch = g_tw_reg & 0xFFFFF; break;
                case 3: g_read_latch = g_ta_reg & 0xFFFFF; break;
                case 4: g_read_latch = g_tb_reg & 0xFFFFF; break;
                case 5: g_read_latch = g_to_reg & 0x3FFFFF; break;
                case 7: g_read_latch = 2; break;
                default: break;
            }
            break;
        }
        default: break;
    }
}

uint32_t gpu_read_status() {
    static bool odd = false;
    uint32_t s = 0;
    s |= (g_page_x & 15) | ((g_page_y & 1) << 4) | ((g_semi & 3) << 5) | ((g_depth & 3) << 7);
    s |= (g_dither ? 1u : 0u) << 9;
    s |= (g_draw_display ? 1u : 0u) << 10;
    s |= (g_set_mask ? 1u : 0u) << 11;
    s |= (g_check_mask ? 1u : 0u) << 12;
    s |= 1u << 13;
    s |= (g_tex_disable ? 1u : 0u) << 15;
    s |= ((g_mode_reg & 0x40) ? 1u : 0u) << 16;
    s |= (g_mode_reg & 3) << 17;
    s |= ((g_mode_reg & 4) ? 1u : 0u) << 19;
    s |= (g_pal ? 1u : 0u) << 20;
    s |= (g_rgb24 ? 1u : 0u) << 21;
    s |= (g_interlace ? 1u : 0u) << 22;
    s |= (g_disp_on ? 0u : 1u) << 23;
    s |= (g_irq1 ? 1u : 0u) << 24;
    s |= 1u << 26;                           // ready for a command
    s |= (g_rd_active ? 1u : 0u) << 27;      // ready to send VRAM to the CPU (set while read-back data is pending)
    s |= 1u << 28;                           // ready for a DMA block
    s |= (uint32_t)g_dma_dir << 29;
    uint32_t drq = g_dma_dir == 0 ? 0 : g_dma_dir == 1 ? 1 : g_dma_dir == 2 ? 1 : (g_rd_active ? 1 : 0);
    s |= drq << 25;
    odd = !odd;
    if (g_interlace && g_vres == 480) s |= (odd ? 1u : 0u) << 31;
    return s;
}

uint32_t gpu_read_data() {
    if (g_rd_active) {
        uint32_t w = 0;
        for (int i = 0; i < 2 && g_rd_active; i++) {
            int x = (g_tx + g_tpos % g_tw) & 1023, y = (g_ty + g_tpos / g_tw) & 511;
            w |= (uint32_t)g_vram[y * 1024 + x] << (16 * i);
            g_tpos++;
            if (g_tpos >= g_tw * g_th) g_rd_active = false;
        }
        g_read_latch = w;
    }
    return g_read_latch;
}

void gpu_dma_to(const uint32_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) gpu_write_gp0(words[i]);
}

void gpu_dma_from(uint32_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) words[i] = gpu_read_data();
}

void gpu_dma_chain(Cpu* c, uint32_t addr) {
    (void)c;
    for (int guard = 0; guard < 1000000; guard++) {
        addr &= 0x1FFFFC;
        uint32_t hdr;
        memcpy(&hdr, psx_ram + addr, 4);
        uint32_t n = hdr >> 24;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t w;
            memcpy(&w, psx_ram + ((addr + 4 + 4 * i) & 0x1FFFFF), 4);
            gpu_write_gp0(w);
        }
        if (hdr & 0x800000) break;
        addr = hdr & 0xFFFFFF;
    }
}

void gpu_display_info(int* x, int* y, int* w, int* h, bool* on, bool* rgb24) {
    *x = g_disp_x; *y = g_disp_y; *w = g_hres; *h = g_vres; *on = g_disp_on; *rgb24 = g_rgb24;
}
