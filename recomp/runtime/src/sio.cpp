// Serial port 0: the controller/memory card port. A DualShock in slot 1 is modelled: it starts in digital
// mode (ID 41h), switches to analog mode (73h) when the game asks for it in configuration mode (F3h), and
// passes vibration requests to the host. The memory card slot and port 2 are empty.
#include <cstring>
#include <initializer_list>

#include "runtime.h"

namespace {

uint32_t g_ctrl = 0, g_mode = 0, g_baud = 0;
uint32_t g_stat_irq = 0;      // bit 9 of STAT
bool g_dsr = false;
bool g_rx_ready = false;
uint8_t g_rx = 0xFF;
uint8_t g_pending_byte = 0xFF;
bool g_pending_ack = false;
int g_phase = 0;  // 0 idle, 1 byte in flight, 2 ack pulse running

// the controller
bool g_analog = false;              // analog mode (ID 73h) instead of digital (41h)
bool g_config = false;              // configuration mode (ID F3h)
int g_motor_map[2] = {0x00, 0x01};  // which request byte drives which motor (set with command 4Dh)
int g_state = 0;                    // 0 waiting for the address byte, 1 waiting for the command, 2 exchanging data
uint8_t g_cmd = 0;
uint8_t g_req[16];                  // request bytes, [0] is the one exchanged for 5Ah
int g_nreq = 0;
uint8_t g_rep[16];                  // reply bytes after the ID byte, [0] is 5Ah
int g_rep_len = 0, g_rep_pos = 0;

uint8_t pad_id() { return g_config ? 0xF3 : g_analog ? 0x73 : 0x41; }

// the reply to a poll: 5Ah, the buttons, and the sticks in analog and configuration mode
void poll_reply() {
    uint16_t buttons = host_pad_state(0, g_analog);
    g_rep[0] = 0x5A;
    g_rep[1] = (uint8_t)(buttons & 0xFF);
    g_rep[2] = (uint8_t)(buttons >> 8);
    g_rep_len = 3;
    if (g_analog || g_config) {
        host_pad_sticks(&g_rep[3]);
        g_rep_len = 7;
    }
}

// the reply to a configuration command: 5Ah and six data bytes
void config_reply(std::initializer_list<uint8_t> d) {
    g_rep[0] = 0x5A;
    int i = 1;
    for (uint8_t b : d) g_rep[i++] = b;
    while (i < 7) g_rep[i++] = 0;
    g_rep_len = 7;
}

void start_command(uint8_t cmd) {
    g_cmd = cmd;
    g_nreq = 0;
    g_rep_pos = 0;
    if (!g_config) { poll_reply(); return; }  // outside configuration mode every command is answered like a poll
    switch (cmd) {
        case 0x42: poll_reply(); break;
        case 0x45: config_reply({0x01, 0x02, (uint8_t)(g_analog ? 1 : 0), 0x02, 0x01, 0x00}); break;  // model, mode LED
        case 0x46: config_reply({0x00, 0x00, 0x01, 0x02, 0x00, 0x0A}); break;                           // refined by the request
        case 0x47: config_reply({0x00, 0x00, 0x02, 0x00, 0x01, 0x00}); break;
        case 0x4C: config_reply({0x00, 0x00, 0x00, 0x04, 0x00, 0x00}); break;                           // refined by the request
        case 0x4D: config_reply({(uint8_t)g_motor_map[0], (uint8_t)g_motor_map[1], 0xFF, 0xFF, 0xFF, 0xFF}); break;
        default: config_reply({}); break;
    }
}

// request byte `n` has arrived (n = 0 is the byte exchanged for 5Ah): later reply bytes may depend on it
void request_byte(int n, uint8_t b) {
    if (n >= 16) return;
    g_req[n] = b;
    g_nreq = n + 1;
    if (!g_config || n != 1) return;
    if (g_cmd == 0x46 && b != 0) { g_rep[4] = 0x01; g_rep[5] = 0x01; g_rep[6] = 0x14; }
    if (g_cmd == 0x4C && b != 0) { g_rep[4] = 0x07; }
}

// the transaction is over: apply what the request asked for
void finish_command() {
    switch (g_cmd) {
        case 0x42:  // vibration: the request bytes drive the motors as mapped by 4Dh
            if (g_nreq >= 3) {
                int small = 0, big = 0;
                for (int i = 0; i < 2; i++) {
                    if (g_motor_map[i] == 0x00) small = g_req[1 + i] & 1;
                    else if (g_motor_map[i] == 0x01) big = g_req[1 + i];
                }
                host_pad_rumble(small, big);
            }
            break;
        case 0x43:  // enter or leave configuration mode
            if (g_nreq >= 2) g_config = g_req[1] == 1;
            break;
        case 0x44:  // digital or analog
            if (g_config && g_nreq >= 2) g_analog = g_req[1] == 1;
            break;
        case 0x4D:  // motor mapping
            if (g_config && g_nreq >= 3) { g_motor_map[0] = g_req[1]; g_motor_map[1] = g_req[2]; }
            break;
    }
}

}  // namespace

bool sio_read(uint32_t reg, uint32_t* v, int size) {
    (void)size;
    switch (reg) {
        case 0x40: {
            uint32_t r = g_rx;
            g_rx_ready = false;
            *v = r;
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
                start_command(b);
                out = pad_id();
                g_state = 2;
                ack = true;
            } else {
                out = g_rep[g_rep_pos];
                request_byte(g_rep_pos, b);
                g_rep_pos++;
                if (g_rep_pos >= g_rep_len) { finish_command(); g_state = 0; ack = false; }
                else ack = true;
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
