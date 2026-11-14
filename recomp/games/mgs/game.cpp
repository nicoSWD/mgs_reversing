// Metal Gear Solid Integral (disc 1, SLPM-86247): game-specific glue.
#include "../../runtime/src/runtime.h"

void rt_override(uint32_t addr, void (*fn)(Cpu*));

static void setconf(Cpu* c) {
    bios_setconf(c);
    c->r[2] = 0;
}

const char* game_exe_path() { return "cdrom:\\MGS\\SLPM_862.47;1"; }

void game_init(Cpu*) {
    rt_override(0x800997BC, setconf);  // also when that code is interpreted rather than recompiled
}
