# psx-recomp

A static recompiler and runtime for PlayStation 1 games. The game's MIPS R3000A code is translated ahead of
time into C, compiled with a normal compiler and linked against a runtime that implements the PlayStation's
hardware and kernel. There is no CPU emulator in the main path: the game runs as a native program.

The first target is **Metal Gear Solid: Integral** (disc 1, `SLPM-86247`).

**No game data is included.** You need your own copy of the disc. The build reads the main executable from
your disc image and the running program reads everything else from it.

## Status

| Area | State |
|---|---|
| Recompiler (PS-X EXE -> C: functions, jump tables, delay and load-delay slots) | working: 2,559 functions of the main executable |
| Interpreter fallback for code the recompiler cannot see (overlays loaded from disc) | working |
| Kernel (events, interrupts, threads with real TCB tables, memory-card service) | working, high level (no BIOS ROM needed) |
| GPU (software rasterizer: flat/Gouraud/textured polygons, lines, rectangles, VRAM transfers) | working |
| GTE (geometry coprocessor) | implemented from the public documentation, lightly tested |
| CD-ROM controller, DMA, root counters, interrupts | working |
| SPU (24 ADPCM voices, ADSR, IRQ used as a stream clock) with audio output | working, no reverb yet |
| Controller (digital pad, keyboard) | working |
| MDEC (movie decoder) | implemented, untested |
| Gameplay beyond the opening demo, save states, memory cards, discs 2 and 3 | not tried yet |

What you get today: the Konami logo and the opening demo run at 60 fps with sound.

## Building

Needs python 3, cmake, ninja, a C/C++ compiler (clang) and network access once (SDL3 is fetched by CMake).
Tested on macOS (Apple Silicon).

```
mkdir -p assets/image
cp /path/to/your/disc1.bin assets/image/disc.bin
printf 'FILE "disc.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' > assets/image/disc.cue
./build.sh
./psxrecomp-mgs
```

`build.sh` extracts the executable from the disc (`tools/discfs.py`), recompiles it (`tools/psxrecomp.py`,
output in `generated/`), then builds the runtime and the recompiled code. The recompiler refuses nothing, but the
runtime refuses a disc whose executable is not the one the code was generated from.

## Running

```
./psxrecomp-mgs [--cue FILE] [--turbo] [--headless] [--frames N]
                [--screenshot F1,F2,... PREFIX] [--press BUTTON@FRAME[+LEN],...] [--log cats]
```

Keys: arrows = d-pad, Z cross, X circle, A square, S triangle, Q/W = L1/R1, E/R = L2/R2, Enter = start,
Backspace = select, Tab = turbo, Esc = quit.

`--screenshot` writes PNG files of chosen frames and prints how many pixels were lit; `--press` scripts button
presses (`START@700+8`); `--log` takes a comma list of `bios,irq,cd,gpu,dma,pad,all`.

## How it works

```
tools/psxrecomp.py        PS-X EXE -> C. A function is the set of blocks reachable without following jal.
                          Every block leader gets a C label, branches are gotos, delay slots are inlined.
                          `jr rX` that is not a return dispatches with a switch over the function's labels,
                          so jump tables need no pattern recovery; anything unknown leaves the function and
                          goes through the runtime's dispatcher.
runtime/include/psxrt.h   the ABI of the generated code: CPU state, inline memory access, hooks
runtime/src/core.cpp      coprocessor 0, scheduler, interrupt delivery, dispatcher and the interpreter
runtime/src/bios.cpp      high-level kernel: A0/B0/C0 calls, events, exceptions, threads
runtime/src/hw.cpp        I/O bus, root counters, DMA
runtime/src/gpu.cpp gte.cpp cdrom.cpp spu.cpp mdec.cpp sio.cpp host.cpp
games/mgs/                per-game glue: recompiler hooks (hooks.py) and runtime overrides (game.cpp)
```

Design notes that matter when adding a game:

* Recompiled code keeps its call stack on the host stack, so every guest thread (`OpenTh`) gets a host stack
  of its own and `ChangeTh` / the return from an interrupt switch between them with `swapcontext`. The
  kernel's thread tables live in guest RAM where games look for them (some read and write the TCBs).
* Interrupts are delivered at block boundaries by the scheduler. Code that is not recompiled (overlays, code
  copied to RAM at run time) is interpreted, so nothing has to be known in advance.
* Games hook functions per address in `games/<game>/hooks.py` (emitted into the generated code) and
  `rt_override` (also applies to interpreted code).

## Debugging aids

`cmake -DPSX_TRACE=ON` records the functions entered (ring buffer plus first-entry order); `kill -TERM` dumps
them, the interrupt registers and guest RAM to `/tmp/psx_ram.bin`. `tools/symbolize.py MAP` annotates addresses
using a linker map. `PSX_DUMP_RAM=file` dumps RAM at exit when `--frames` is used.

## Licence

MIT, see `LICENSE`. This is an independent implementation written from public PlayStation hardware
documentation. It contains no code from the original console, its BIOS, or any game. PlayStation is a
trademark of Sony Interactive Entertainment; this project is not affiliated with it or with Konami.
