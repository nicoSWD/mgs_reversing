/* Runtime internals shared between the runtime's own files. */
#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#include "psxrt.h"

[[noreturn]] void fatal(const char* fmt, ...);
void logf(const char* cat, const char* fmt, ...);
extern bool g_log_bios, g_log_irq, g_log_cd, g_log_gpu, g_log_dma, g_log_pad;
#define LOG(flag, ...) do { if (g_log_##flag) logf(#flag, __VA_ARGS__); } while (0)

/* the machine */
void machine_init(Cpu* c);
uint64_t sched_now(Cpu* c);

/* interrupts: bits of I_STAT */
enum { IRQ_VBLANK = 1, IRQ_GPU = 2, IRQ_CD = 4, IRQ_DMA = 8, IRQ_TMR0 = 16, IRQ_TMR1 = 32, IRQ_TMR2 = 64,
       IRQ_PAD = 128, IRQ_SIO = 256, IRQ_SPU = 512, IRQ_PIO = 1024 };
void irq_raise(Cpu* c, uint32_t bits);
bool irq_pending(Cpu* c);
bool irq_deliverable(Cpu* c);
uint32_t irq_stat();
uint32_t irq_mask();

/* scheduled hardware events */
enum Ev { EV_VBLANK, EV_HBLANK_TMR, EV_TMR0, EV_TMR1, EV_TMR2, EV_CD, EV_DMA0, EV_DMA1, EV_DMA2, EV_DMA3, EV_DMA4, EV_DMA5,
          EV_DMA6, EV_PAD, EV_SPU, EV_COUNT };
void sched_set(Cpu* c, Ev e, uint64_t when);
void sched_clear(Cpu* c, Ev e);
void sched_recompute(Cpu* c);

/* kernel (HLE BIOS) */
void bios_init(Cpu* c);
void bios_call(Cpu* c, uint32_t table, uint32_t fn);
void bios_take_interrupt(Cpu* c);
bool bios_in_exception();
extern "C" uint32_t rt_guest_call(Cpu* c, uint32_t addr, uint32_t a0);

/* hardware */
uint32_t io_read(Cpu* c, uint32_t phys, int size);
void io_write(Cpu* c, uint32_t phys, uint32_t v, int size);
void hw_init(Cpu* c);
void hw_vblank(Cpu* c);

void gpu_init();
uint32_t gpu_read_data();
uint32_t gpu_read_status();
void gpu_write_gp0(uint32_t v);
void gpu_write_gp1(uint32_t v);
void gpu_dma_to(const uint32_t* words, uint32_t count);          /* RAM -> GPU */
void gpu_dma_from(uint32_t* words, uint32_t count);              /* GPU -> RAM */
void gpu_dma_chain(Cpu* c, uint32_t addr);
const uint16_t* gpu_vram();
void gpu_display_info(int* x, int* y, int* w, int* h, bool* on, bool* rgb24);

/* cd */
bool cd_open(const std::string& cue);
uint32_t cd_read_reg(Cpu* c, int reg);
void cd_write_reg(Cpu* c, int reg, uint32_t v);
void cd_event(Cpu* c);
void cd_dma(uint8_t* dst, uint32_t bytes);
bool cd_find_file(const char* path, uint32_t* lba, uint32_t* size);
bool cd_read_sector_user(uint32_t lba, uint8_t* out2048);

/* host */
void host_present(Cpu* c);
bool host_poll_events(Cpu* c);   /* false: quit */
uint16_t host_pad_state(int port);
