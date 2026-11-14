// CD-ROM controller and disc image (single-file .bin/.cue, MODE2/2352).
#include <cstring>
#include <deque>
#include <fstream>
#include <vector>

#include "runtime.h"

namespace {

std::vector<uint8_t> g_disc;  // whole image in memory (raw 2352-byte sectors)
uint32_t g_sectors = 0;

constexpr uint64_t kCpuHz = 33868800;

int g_index = 0;
uint8_t g_ie = 0x1F, g_if = 0;  // the BIOS leaves all CD interrupts enabled
std::deque<uint8_t> g_params, g_resp;
uint8_t g_mode = 0;
bool g_motor = true;
uint32_t g_loc = 0, g_cur = 0;
bool g_reading = false;
uint8_t g_filter_file = 0, g_filter_chan = 0;

struct Pending { int irq; std::vector<uint8_t> data; uint64_t delay; };
std::deque<Pending> g_pending;

uint8_t g_sector[2352];
uint32_t g_data_pos = 0, g_data_len = 0, g_data_off = 0;
bool g_data_ready = false;  // the host asked for the data (BFRD)

uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
int unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }

uint64_t g_due = 0;
bool g_due_set = false;

void schedule(Cpu* c, uint64_t delay) {
    if (g_due_set) return;
    g_due = c->cycles + delay;
    g_due_set = true;
    sched_set(c, EV_CD, g_due);
}

void queue(Cpu* c, int irq, std::vector<uint8_t> data, uint64_t delay) {
    g_pending.push_back({irq, std::move(data), delay});
    schedule(c, delay);
}

void raise_now(Cpu* c, int irq, const std::vector<uint8_t>& data) {
    LOG(cd, "INT%d [%zu bytes]", irq, data.size());
    g_resp.assign(data.begin(), data.end());
    g_if = (uint8_t)((g_if & ~7) | irq);
    if (g_ie & g_if & 7) irq_raise(c, IRQ_CD);
}

uint8_t status_byte() {
    uint8_t s = g_motor ? 0x02 : 0;
    if (g_reading) s |= 0x20;
    return s;
}

}  // namespace

bool cd_open(const std::string& cue) {
    std::ifstream f(cue);
    if (!f) return false;
    std::string dir = cue.substr(0, cue.find_last_of("/\\") == std::string::npos ? 0 : cue.find_last_of("/\\") + 1);
    std::string bin = cue, line;
    bool isbin = cue.size() > 4 && (cue.substr(cue.size() - 4) == ".bin" || cue.substr(cue.size() - 4) == ".BIN");
    if (!isbin) {
        bin.clear();
        while (std::getline(f, line)) {
            size_t a = line.find("FILE");
            if (a != std::string::npos) {
                size_t q1 = line.find('"', a), q2 = line.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) { bin = dir + line.substr(q1 + 1, q2 - q1 - 1); break; }
            }
        }
    }
    std::ifstream b(bin, std::ios::binary);
    if (!b) return false;
    b.seekg(0, std::ios::end);
    size_t n = (size_t)b.tellg();
    b.seekg(0);
    g_disc.resize(n);
    b.read((char*)g_disc.data(), (std::streamsize)n);
    g_sectors = (uint32_t)(n / 2352);
    return true;
}

bool cd_read_sector_user(uint32_t lba, uint8_t* out) {
    if (lba >= g_sectors) return false;
    memcpy(out, &g_disc[(size_t)lba * 2352 + 24], 2048);
    return true;
}

bool cd_find_file(const char* path, uint32_t* lba, uint32_t* size) {
    uint8_t pvd[2048];
    if (!cd_read_sector_user(16, pvd)) return false;
    uint32_t dir_lba = pvd[158] | (pvd[159] << 8) | (pvd[160] << 16) | ((uint32_t)pvd[161] << 24);
    uint32_t dir_size = pvd[166] | (pvd[167] << 8) | (pvd[168] << 16) | ((uint32_t)pvd[169] << 24);
    std::string p = path;
    if (p.rfind("cdrom:", 0) == 0) p = p.substr(6);
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : p) {
        if (ch == '\\' || ch == '/') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += ch;
    }
    if (!cur.empty()) parts.push_back(cur);
    for (size_t pi = 0; pi < parts.size(); pi++) {
        bool found = false;
        std::vector<uint8_t> buf(((dir_size + 2047) / 2048) * 2048);
        for (uint32_t s = 0; s * 2048 < buf.size(); s++) cd_read_sector_user(dir_lba + s, &buf[s * 2048]);
        size_t o = 0;
        while (o < dir_size) {
            uint8_t len = buf[o];
            if (!len) { o = (o / 2048 + 1) * 2048; continue; }
            std::string name((char*)&buf[o + 33], buf[o + 32]);
            bool is_dir = buf[o + 25] & 2;
            std::string want = parts[pi];
            if (name == want || name.substr(0, name.find(';')) == want.substr(0, want.find(';'))) {
                uint32_t l = buf[o + 2] | (buf[o + 3] << 8) | (buf[o + 4] << 16) | ((uint32_t)buf[o + 5] << 24);
                uint32_t sz = buf[o + 10] | (buf[o + 11] << 8) | (buf[o + 12] << 16) | ((uint32_t)buf[o + 13] << 24);
                if (pi + 1 == parts.size()) { *lba = l; *size = sz; return true; }
                if (!is_dir) return false;
                dir_lba = l; dir_size = sz;
                found = true;
                break;
            }
            o += len;
        }
        if (!found) return false;
    }
    return false;
}

static void do_command(Cpu* c, uint8_t cmd) {
    std::vector<uint8_t> p(g_params.begin(), g_params.end());
    g_params.clear();
    LOG(cd, "cmd %02X params=%zu [%02X %02X %02X]", cmd, p.size(), p.size() > 0 ? p[0] : 0, p.size() > 1 ? p[1] : 0,
        p.size() > 2 ? p[2] : 0);
    const uint64_t ack = 50000;
    auto stat = [&]() { return std::vector<uint8_t>{status_byte()}; };
    switch (cmd) {
        case 0x01: queue(c, 3, stat(), ack); break;
        case 0x02:
            if (p.size() >= 3) g_loc = (uint32_t)((unbcd(p[0]) * 60 + unbcd(p[1])) * 75 + unbcd(p[2])) - 150;
            queue(c, 3, stat(), ack);
            break;
        case 0x03: queue(c, 3, stat(), ack); break;
        case 0x06: case 0x1B: {
            g_cur = g_loc;
            g_reading = true;
            queue(c, 3, stat(), ack);
            uint64_t per = kCpuHz / ((g_mode & 0x80) ? 150 : 75);
            queue(c, 1, {}, per / 2 + 20000);
            break;
        }
        case 0x08: g_reading = false; queue(c, 3, stat(), ack); queue(c, 2, {0x02}, ack + 100000); break;
        case 0x09: g_reading = false; queue(c, 3, stat(), ack); queue(c, 2, {status_byte()}, ack + 60000); break;
        case 0x0A: g_reading = false; g_mode = 0; queue(c, 3, stat(), ack); queue(c, 2, {status_byte()}, ack + 120000); break;
        case 0x0B: case 0x0C: queue(c, 3, stat(), ack); break;
        case 0x0D: if (p.size() >= 2) { g_filter_file = p[0]; g_filter_chan = p[1]; } queue(c, 3, stat(), ack); break;
        case 0x0E: if (!p.empty()) g_mode = p[0]; queue(c, 3, stat(), ack); break;
        case 0x0F: queue(c, 3, {status_byte(), g_mode, 0, g_filter_file, g_filter_chan}, ack); break;
        case 0x10: case 0x11: {
            uint32_t l = g_cur + 150;
            queue(c, 3, {bcd((int)(l / 75 / 60)), bcd((int)(l / 75 % 60)), bcd((int)(l % 75)), 2, 0, 0, 0, 0}, ack);
            break;
        }
        case 0x13: queue(c, 3, {status_byte(), 1, 1}, ack); break;
        case 0x14: queue(c, 3, {status_byte(), 0, 2}, ack); break;
        case 0x15: case 0x16: g_cur = g_loc; queue(c, 3, stat(), ack); queue(c, 2, {status_byte()}, ack + 100000); break;
        case 0x19:
            if (!p.empty() && p[0] == 0x20) queue(c, 3, {0x94, 0x09, 0x19, 0xC0}, ack);
            else queue(c, 3, stat(), ack);
            break;
        case 0x1A:
            queue(c, 3, stat(), ack);
            queue(c, 2, {0x02, 0x00, 0x20, 0x00, 'S', 'C', 'E', 'I'}, ack + 100000);
            break;
        case 0x1C: case 0x1E: queue(c, 3, stat(), ack); queue(c, 2, {status_byte()}, ack + 100000); break;
        default: queue(c, 5, {(uint8_t)(status_byte() | 1), 0x40}, ack); break;
    }
}

void cd_event(Cpu* c) {
    g_due_set = false;
    LOG(cd, "event pending=%zu if=%02X ie=%02X", g_pending.size(), g_if, g_ie);
    if (g_pending.empty()) return;
    if ((g_if & 7) != 0) {  // the previous interrupt has not been acknowledged yet
        schedule(c, 5000);
        return;
    }
    Pending p = std::move(g_pending.front());
    g_pending.pop_front();
    if (p.irq == 1) {
        if (!g_reading) { if (!g_pending.empty()) schedule(c, 1000); return; }
        if (g_cur < g_sectors) memcpy(g_sector, &g_disc[(size_t)g_cur * 2352], 2352);
        else memset(g_sector, 0, sizeof g_sector);
        if (g_log_cd && (g_cur % 256) == 0) logf("cd", "sector lba=%u", g_cur);
        g_cur++;
        g_data_off = (g_mode & 0x20) ? 12 : 24;
        g_data_len = (g_mode & 0x20) ? 0x924 : 0x800;
        g_data_pos = 0;
        g_data_ready = false;
        raise_now(c, 1, {status_byte()});
        uint64_t per = kCpuHz / ((g_mode & 0x80) ? 150 : 75);
        queue(c, 1, {}, per);
    } else {
        raise_now(c, p.irq, p.data);
    }
    if (!g_pending.empty() && !g_due_set) schedule(c, 5000);
}

uint32_t cd_read_reg2(Cpu*, int reg);
uint32_t cd_read_reg(Cpu* c_, int reg) {
    uint32_t r_ = cd_read_reg2(c_, reg);
    LOG(cd, "  R idx%d reg%d -> %02X", g_index, reg, r_);
    return r_;
}

uint32_t cd_read_reg2(Cpu*, int reg) {
    switch (reg) {
        case 0: {
            uint8_t s = (uint8_t)g_index;
            if (g_params.empty()) s |= 0x08;
            if (g_params.size() < 16) s |= 0x10;
            if (!g_resp.empty()) s |= 0x20;
            if (g_data_ready && g_data_pos < g_data_len) s |= 0x40;
            return s;
        }
        case 1: {
            if (g_resp.empty()) return 0;
            uint8_t v = g_resp.front();
            g_resp.pop_front();
            return v;
        }
        case 2:
            if (g_data_ready && g_data_pos < g_data_len) return g_sector[g_data_off + g_data_pos++];
            return 0;
        case 3:
            return (g_index == 0 || g_index == 2) ? (g_ie | 0xE0u) : (g_if | 0xE0u);
    }
    return 0;
}

void cd_write_reg(Cpu* c, int reg, uint32_t v) {
    v &= 0xFF;
    LOG(cd, "  W idx%d reg%d = %02X", g_index, reg, v);
    if (reg == 0) { g_index = v & 3; return; }
    int idx = g_index;
    if (reg == 1) { if (idx == 0) do_command(c, (uint8_t)v); return; }
    if (reg == 2) {
        if (idx == 0) g_params.push_back((uint8_t)v);
        else if (idx == 1) { g_ie = v & 0x1F; if (g_ie & g_if & 7) irq_raise(c, IRQ_CD); }
        return;
    }
    if (reg == 3) {
        if (idx == 0) {
            if (v & 0x80) { if (!g_data_ready) { g_data_ready = true; g_data_pos = 0; } }  // only a rising edge restarts
            else g_data_ready = false;
        } else if (idx == 1) {
            g_if &= ~(v & 0x1F);
            if (v & 0x40) g_params.clear();
            if ((g_if & 7) == 0) {
                g_resp.clear();
                if (!g_pending.empty() && !g_due_set) schedule(c, 4000);
            }
        }
    }
}

void cd_dma(uint8_t* dst, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes; i++) dst[i] = (g_data_pos < g_data_len) ? g_sector[g_data_off + g_data_pos++] : 0;
}
