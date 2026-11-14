// Entry point: loads the game's executable from the disc image into guest RAM, brings up the machine and
// runs the recompiled code from the executable's entry point.
#include <cstring>
#include <string>

#include "runtime.h"

extern "C" {
extern const PsxFunc main_table[];
extern const unsigned main_count;
extern const unsigned main_base;
extern const unsigned main_size;
extern const unsigned main_hash;
void rt_register_table(const PsxFunc* t, unsigned n);
}
void rt_override(uint32_t addr, void (*fn)(Cpu*));
void game_init(Cpu* c);
const char* game_exe_path();

bool host_init(const char* title, bool headless);
void host_set_screenshots(const std::vector<int>& frames, const std::string& prefix);
void host_set_script(const std::string& s);
extern bool g_turbo;
extern int g_max_frames;

static uint32_t fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static void usage() {
    printf("usage: psxrecomp-mgs [--cue FILE] [--turbo] [--headless] [--frames N] [--log cats]\n"
           "                     [--screenshot F1,F2,... PREFIX] [--press BUTTON@FRAME[+LEN],...]\n"
           "keys: arrows = d-pad, Z cross, X circle, A square, S triangle, Q/W L1/R1, E/R L2/R2,\n"
           "      Enter start, Backspace select, Tab turbo, Esc quit\n");
}

int main(int argc, char** argv) {
    std::string cue = "assets/image/disc.cue";
    bool headless = false;
    std::vector<int> shots;
    std::string shot_prefix = "shot";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cue" && i + 1 < argc) cue = argv[++i];
        else if (a == "--turbo") g_turbo = true;
        else if (a == "--headless") headless = true;
        else if (a == "--frames" && i + 1 < argc) g_max_frames = atoi(argv[++i]);
        else if (a == "--press" && i + 1 < argc) host_set_script(argv[++i]);
        else if (a == "--screenshot" && i + 2 < argc) {
            std::string l = argv[++i];
            shot_prefix = argv[++i];
            size_t p = 0;
            while (p < l.size()) { size_t e = l.find(',', p); if (e == std::string::npos) e = l.size(); shots.push_back(atoi(l.substr(p, e - p).c_str())); p = e + 1; }
        } else if (a == "--log" && i + 1 < argc) {
            std::string l = argv[++i];
            bool all = l.find("all") != std::string::npos;
            g_log_bios = all || l.find("bios") != std::string::npos;
            g_log_irq = all || l.find("irq") != std::string::npos;
            g_log_cd = all || l.find("cd") != std::string::npos;
            g_log_gpu = all || l.find("gpu") != std::string::npos;
            g_log_dma = all || l.find("dma") != std::string::npos;
            g_log_pad = all || l.find("pad") != std::string::npos;
        } else { usage(); return a == "--help" ? 0 : 2; }
    }
    host_set_screenshots(shots, shot_prefix);

    if (!cd_open(cue)) { fprintf(stderr, "cannot open the disc image %s\n", cue.c_str()); return 1; }
    uint32_t lba, size;
    if (!cd_find_file(game_exe_path(), &lba, &size)) { fprintf(stderr, "%s not found on the disc\n", game_exe_path()); return 1; }
    std::vector<uint8_t> exe(((size + 2047) / 2048) * 2048);
    for (uint32_t i = 0; i < exe.size() / 2048; i++) cd_read_sector_user(lba + i, &exe[i * 2048]);
    if (memcmp(exe.data(), "PS-X EXE", 8) != 0) { fprintf(stderr, "not a PS-X EXE\n"); return 1; }
    uint32_t pc0, gp0, t_addr, t_size;
    memcpy(&pc0, &exe[0x10], 4);
    memcpy(&gp0, &exe[0x14], 4);
    memcpy(&t_addr, &exe[0x18], 4);
    memcpy(&t_size, &exe[0x1C], 4);
    if (t_addr != main_base || t_size != main_size || fnv(&exe[0x800], t_size) != main_hash) {
        fprintf(stderr, "this disc's executable is not the one the code was recompiled from\n");
        return 1;
    }
    memcpy(psx_ram + (t_addr & (PSX_RAM_SIZE - 1)), &exe[0x800], t_size);

    if (!host_init("PSX recomp", headless)) return 1;

    static Cpu cpu;
    memset(&cpu, 0, sizeof cpu);
    gpu_init();
    hw_init(&cpu);
    bios_init(&cpu);
    rt_register_table(main_table, main_count);
    game_init(&cpu);
    cpu.r[28] = gp0;
    cpu.r[29] = 0x801FFFF0u;
    cpu.r[30] = 0x801FFFF0u;
    cpu.r[31] = 0;
    psx_call(&cpu, pc0);
    fprintf(stderr, "the program returned from its entry point\n");
    return 0;
}
