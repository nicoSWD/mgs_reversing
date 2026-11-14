// Serial port 0: the controller/memory card port. Only a digital controller in slot 1 is modelled.
#include "runtime.h"

namespace {

uint32_t g_ctrl = 0, g_mode = 0, g_baud = 0;
uint32_t g_stat_irq = 0;      // bit 9 of STAT
bool g_dsr = false;
bool g_rx_ready = false;
uint8_t g_rx = 0xFF;
uint8_t g_cmd = 0;
int g_state = 0;              // position in the pad transaction
bool g_selected_pad = false;
uint8_t g_pending_byte = 0xFF;
bool g_pending_ack = false;
int g_phase = 0;  // 0 idle, 1 byte in flight, 2 ack pulse running

}  // namespace

bool sio_read(uint32_t reg, uint32_t* v, int size) {
    (void)size;
    switch (reg) {
        case 0x40: {
            uint32_t r = g_rx;
            g_rx_ready = false;
            *v = r | 0xFFFFFF00u * 0;
            return true;
        }
        case 0x44: {
            uint32_t s = 1 | 4;  // TX ready 1 and 2
            if (g_rx_ready) s |= 2;
            if (g_dsr) s |= 0x80;
            s |= g_stat_irq;
            *v = s;
            return true;
        }
        case 0x48: *v = g_mode; return true;
        case 0x4A: *v = g_ctrl; return true;
        case 0x4E: *v = g_baud; return true;
    }
    *v = 0;
    return true;
}

bool sio_write(Cpu* c, uint32_t reg, uint32_t v, int size) {
    (void)size;
    switch (reg) {
        case 0x40: {
            if (!(g_ctrl & 1)) return true;  // TX disabled
            uint8_t b = (uint8_t)v;
            uint8_t out = 0xFF;
            LOG(pad, "tx %02X state=%d ctrl=%04X", b, g_state, g_ctrl);
            bool ack = false;
            bool port0 = !(g_ctrl & 0x2000);
            if (g_state == 0) {
                if (b == 0x01 && port0) { g_state = 1; ack = true; }
                // else: nothing answers (memory card / absent port)
            } else if (g_state == 1) {
                g_cmd = b;
                out = 0x41;  // a digital controller identifies as 41 whatever the command
                g_state = 2;
                ack = true;
            } else if (g_state == 2) {
                out = 0x5A; g_state = 3; ack = true;
            } else if (g_state == 3) {
                uint16_t pad = g_cmd == 0x42 ? host_pad_state(0) : 0xFFFF;
                out = (uint8_t)(pad & 0xFF); g_state = 4; ack = true;
            } else if (g_state == 4) {
                uint16_t pad = g_cmd == 0x42 ? host_pad_state(0) : 0xFFFF;
                out = (uint8_t)(pad >> 8); g_state = 0; ack = false;
            }
            LOG(pad, "  -> %02X ack=%d", out, ack);
            g_pending_byte = out;
            g_pending_ack = ack;
            g_dsr = false;
            g_phase = 1;
            sched_set(c, EV_PAD, c->cycles + 700);
            return true;
        }
        case 0x48: g_mode = v; return true;
        case 0x4A:
            LOG(pad, "ctrl = %04X", v);
            if (v & 0x40) { g_ctrl = 0; g_state = 0; g_stat_irq = 0; g_dsr = false; g_rx_ready = false; return true; }
            if (v & 0x10) { g_stat_irq = 0; }
            g_ctrl = v & ~0x10u;
            if (!(g_ctrl & 2)) g_state = 0;  // /JOY select released: the transaction is over
            return true;
        case 0x4E: g_baud = v; return true;
    }
    return true;
}

void sio_event(Cpu* c) {
    if (g_phase == 2) {  // the ack pulse is over
        g_dsr = false;
        g_phase = 0;
        return;
    }
    if (g_phase != 1) return;
    g_rx = g_pending_byte;
    g_rx_ready = true;
    if (g_pending_ack) {
        g_dsr = true;
        if (g_ctrl & 0x1000) { g_stat_irq = 0x200; irq_raise(c, IRQ_PAD); }
        g_phase = 2;
        sched_set(c, EV_PAD, c->cycles + 100);
    } else {
        g_phase = 0;
    }
}
