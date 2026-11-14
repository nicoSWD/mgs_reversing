# Per-game hooks for Metal Gear Solid Integral (SLPM-86247 main executable, as built by mgs_reversing).
HOOKS = {
    # libapi SetConf(events, tcbs, stack) rewrites the kernel's tables in guest RAM; the runtime owns them.
    0x800997BC: ['bios_setconf(c); c->r[2] = 0; return;'],
}
