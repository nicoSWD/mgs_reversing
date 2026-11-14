// Host integration: the window, frame pacing, keyboard input, screenshots and scripted test input.
#include <SDL3/SDL.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "runtime.h"

namespace {

SDL_Window* g_win = nullptr;
SDL_Renderer* g_ren = nullptr;
SDL_Texture* g_tex = nullptr;
SDL_AudioStream* g_audio = nullptr;
int g_tex_w = 0, g_tex_h = 0;
std::vector<uint32_t> g_pix;
uint64_t g_frame = 0;
bool g_quit = false;
std::chrono::steady_clock::time_point g_last;
SDL_Gamepad* g_pad = nullptr;
int g_rumble_small = 0, g_rumble_big = 0, g_rumble_age = 0;

void open_first_gamepad() {
    if (g_pad) return;
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    for (int i = 0; ids && i < n && !g_pad; i++) {
        g_pad = SDL_OpenGamepad(ids[i]);
        if (g_pad) fprintf(stderr, "[host] gamepad: %s\n", SDL_GetGamepadName(g_pad));
    }
    SDL_free(ids);
}

void close_gamepad(SDL_JoystickID which) {
    if (!g_pad || SDL_GetGamepadID(g_pad) != which) return;
    SDL_CloseGamepad(g_pad);
    g_pad = nullptr;
    fprintf(stderr, "[host] gamepad removed\n");
    open_first_gamepad();
}

uint8_t axis_byte(SDL_GamepadAxis a) {  // -32768..32767 -> 0..255, 0x80 in the middle
    int v = SDL_GetGamepadAxis(g_pad, a);
    int b = (v + 32768) >> 8;
    return (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
}

struct Press { int button; int from, len; };
std::vector<Press> g_script;
std::vector<int> g_shots;
std::string g_shot_prefix = "shot";

}  // namespace

int host_frame_number() { return (int)g_frame; }
void host_log_set(bool on) {
    struct Saved { bool bios, irq, cd, gpu, dma, pad; };
    static Saved saved;
    static bool have = false;
    if (!have) { have = true; saved = {g_log_bios, g_log_irq, g_log_cd, g_log_gpu, g_log_dma, g_log_pad}; }
    g_log_bios = on && saved.bios; g_log_irq = on && saved.irq; g_log_cd = on && saved.cd;
    g_log_gpu = on && saved.gpu; g_log_dma = on && saved.dma; g_log_pad = on && saved.pad;
}
#ifdef PSX_TRACE_CALLS
void trace_dump(int n);
#endif
bool g_turbo = false;
int g_max_frames = 0;
int g_log_from = 0;
bool g_headless = false;

void host_set_screenshots(const std::vector<int>& frames, const std::string& prefix) {
    g_shots = frames;
    g_shot_prefix = prefix;
}

static int button_from_name(const std::string& n) {
    static const char* names[16] = {"SELECT", "L3", "R3", "START", "UP", "RIGHT", "DOWN", "LEFT",
                                    "L2", "R2", "L1", "R1", "TRIANGLE", "CIRCLE", "CROSS", "SQUARE"};
    for (int i = 0; i < 16; i++) if (n == names[i]) return i;
    return -1;
}

void host_set_script(const std::string& s) {  // BUTTON@FRAME[+FRAMES],...
    size_t i = 0;
    while (i < s.size()) {
        size_t e = s.find(',', i);
        if (e == std::string::npos) e = s.size();
        std::string item = s.substr(i, e - i);
        i = e + 1;
        size_t at = item.find('@');
        if (at == std::string::npos) continue;
        int b = button_from_name(item.substr(0, at));
        std::string rest = item.substr(at + 1);
        int len = 6;
        size_t plus = rest.find('+');
        if (plus != std::string::npos) { len = atoi(rest.c_str() + plus + 1); rest = rest.substr(0, plus); }
        if (b >= 0) g_script.push_back({b, atoi(rest.c_str()), len});
    }
}

bool host_init(const char* title, bool headless) {
    g_headless = headless;
    if (headless) return true;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");  // keep the controller alive when the window is not focused
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    g_win = SDL_CreateWindow(title, 1280, 960, SDL_WINDOW_RESIZABLE);
    if (!g_win) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return false; }
    g_ren = SDL_CreateRenderer(g_win, nullptr);
    if (!g_ren) { fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); return false; }
    SDL_SetRenderVSync(g_ren, 0);
    {
        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = 44100;
        g_audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (g_audio) SDL_ResumeAudioStreamDevice(g_audio);
        else fprintf(stderr, "no audio device: %s\n", SDL_GetError());
    }
    open_first_gamepad();
    g_last = std::chrono::steady_clock::now();
    return true;
}

static void build_image(int* w, int* h) {
    int x, y, dw, dh;
    bool on, rgb24;
    gpu_display_info(&x, &y, &dw, &dh, &on, &rgb24);
    *w = dw; *h = dh;
    g_pix.assign((size_t)dw * dh, 0xFF000000u);
    if (!on) return;
    const uint16_t* vram = gpu_vram();
    if (!rgb24) {
        for (int j = 0; j < dh; j++) {
            const uint16_t* row = vram + (((y + j) & 511) * 1024);
            for (int i = 0; i < dw; i++) {
                uint16_t p = row[(x + i) & 1023];
                uint32_t r = (p & 31) << 3, g = ((p >> 5) & 31) << 3, b = ((p >> 10) & 31) << 3;
                g_pix[(size_t)j * dw + i] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
    } else {
        for (int j = 0; j < dh; j++) {
            const uint8_t* row = (const uint8_t*)(vram + (((y + j) & 511) * 1024));
            for (int i = 0; i < dw; i++) {
                int o = ((x * 2) + i * 3) & 2047;
                g_pix[(size_t)j * dw + i] = 0xFF000000u | (row[o] << 16) | (row[(o + 1) & 2047] << 8) | row[(o + 2) & 2047];
            }
        }
    }
}

// A minimal PNG writer (stored deflate blocks), enough for screenshots without another dependency.
static uint32_t crc32_bytes(const uint8_t* p, size_t n, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
    return ~crc;
}

static void put_be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}

static void png_chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hdr;
    put_be32(hdr, (uint32_t)data.size());
    fwrite(hdr.data(), 1, 4, f);
    std::vector<uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    fwrite(body.data(), 1, body.size(), f);
    std::vector<uint8_t> crc;
    put_be32(crc, crc32_bytes(body.data(), body.size()));
    fwrite(crc.data(), 1, 4, f);
}

static bool write_png(const char* path, int w, int h, const uint32_t* argb) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put_be32(ihdr, (uint32_t)w); put_be32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);  // 8-bit RGB
    png_chunk(f, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (w * 3 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            uint32_t p = argb[(size_t)y * w + x];
            raw.push_back((p >> 16) & 255); raw.push_back((p >> 8) & 255); raw.push_back(p & 255);
        }
    }
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size() || pos == 0) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t ch : raw) { a = (a + ch) % 65521; b = (b + a) % 65521; }
    put_be32(z, (b << 16) | a);
    png_chunk(f, "IDAT", z);
    png_chunk(f, "IEND", {});
    fclose(f);
    return true;
}

static void save_shot(int frame, int w, int h) {
    char name[512];
    snprintf(name, sizeof name, "%s_%d.png", g_shot_prefix.c_str(), frame);
    size_t lit = 0;
    for (uint32_t p : g_pix) if ((p & 0xFFFFFF) != 0) lit++;
    if (write_png(name, w, h, g_pix.data())) logf("host", "screenshot %s (%dx%d, %zu lit pixels)", name, w, h, lit);
}

void host_audio_push(const int16_t* stereo, int frames) {
    if (!g_audio || g_turbo) return;
    if (SDL_GetAudioStreamQueued(g_audio) > 44100 * 4 / 4) return;  // more than 250 ms queued: drop
    SDL_PutAudioStreamData(g_audio, stereo, frames * 4);
}

// Controller state in the PlayStation's format: a 16 bit word, a clear bit means "pressed" (bit 0 SELECT, 1 L3,
// 2 R3, 3 START, 4-7 up/right/down/left, 8 L2, 9 R2, 10 L1, 11 R1, 12 triangle, 13 circle, 14 cross, 15 square).
// `analog` tells whether the game put the pad in analog mode; if not, the left stick doubles as the d-pad.
uint16_t host_pad_state(int port, bool analog) {
    if (port != 0) return 0xFFFF;
    uint16_t state = 0xFFFF;
    auto press = [&](int bit) { state &= ~(1u << bit); };
    for (const Press& p : g_script)
        if ((int)g_frame >= p.from && (int)g_frame < p.from + p.len) press(p.button);
    if (g_headless) return state;
    const bool* k = SDL_GetKeyboardState(nullptr);
    if (k[SDL_SCANCODE_UP]) press(4);
    if (k[SDL_SCANCODE_RIGHT]) press(5);
    if (k[SDL_SCANCODE_DOWN]) press(6);
    if (k[SDL_SCANCODE_LEFT]) press(7);
    if (k[SDL_SCANCODE_RETURN]) press(3);
    if (k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_RSHIFT]) press(0);
    if (k[SDL_SCANCODE_E]) press(8);
    if (k[SDL_SCANCODE_R]) press(9);
    if (k[SDL_SCANCODE_Q]) press(10);
    if (k[SDL_SCANCODE_W]) press(11);
    if (k[SDL_SCANCODE_S]) press(12);
    if (k[SDL_SCANCODE_X]) press(13);
    if (k[SDL_SCANCODE_Z]) press(14);
    if (k[SDL_SCANCODE_A]) press(15);
    if (g_pad) {
        auto b = [&](SDL_GamepadButton btn) { return SDL_GetGamepadButton(g_pad, btn); };
        if (b(SDL_GAMEPAD_BUTTON_DPAD_UP)) press(4);
        if (b(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) press(5);
        if (b(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) press(6);
        if (b(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) press(7);
        if (b(SDL_GAMEPAD_BUTTON_START)) press(3);                                  // Options
        if (b(SDL_GAMEPAD_BUTTON_BACK) || b(SDL_GAMEPAD_BUTTON_TOUCHPAD)) press(0);  // Create / touchpad = SELECT
        if (b(SDL_GAMEPAD_BUTTON_LEFT_STICK)) press(1);
        if (b(SDL_GAMEPAD_BUTTON_RIGHT_STICK)) press(2);
        if (b(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) press(10);
        if (b(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) press(11);
        if (SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000) press(8);
        if (SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000) press(9);
        if (b(SDL_GAMEPAD_BUTTON_NORTH)) press(12);  // triangle
        if (b(SDL_GAMEPAD_BUTTON_EAST)) press(13);   // circle
        if (b(SDL_GAMEPAD_BUTTON_SOUTH)) press(14);  // cross
        if (b(SDL_GAMEPAD_BUTTON_WEST)) press(15);   // square
        if (!analog) {
            int lx = SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTX), ly = SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTY);
            if (ly < -16000) press(4);
            if (lx > 16000) press(5);
            if (ly > 16000) press(6);
            if (lx < -16000) press(7);
        }
    }
    if (g_log_pad) {
        static uint16_t last = 0xFFFF;
        if (state != last) { logf("pad", "buttons %04X (a clear bit is a pressed button)", state); last = state; }
    }
    return state;
}

// The four stick bytes of an analog pad in poll order: right X, right Y, left X, left Y (0x80 = centred).
void host_pad_sticks(uint8_t out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0x80;
    if (g_headless || !g_pad) return;
    out[0] = axis_byte(SDL_GAMEPAD_AXIS_RIGHTX);
    out[1] = axis_byte(SDL_GAMEPAD_AXIS_RIGHTY);
    out[2] = axis_byte(SDL_GAMEPAD_AXIS_LEFTX);
    out[3] = axis_byte(SDL_GAMEPAD_AXIS_LEFTY);
}

// Vibration requested by the game: the small motor is on/off, the large one has 255 levels.
void host_pad_rumble(int small, int big) {
    if (g_headless || !g_pad) return;
    if (small == g_rumble_small && big == g_rumble_big && g_rumble_age < 20) { g_rumble_age++; return; }
    g_rumble_small = small; g_rumble_big = big; g_rumble_age = 0;
    SDL_RumbleGamepad(g_pad, (Uint16)(big * 257), (Uint16)(small ? 0xFFFF : 0), 250);
}

void host_frame(Cpu* c) {
    (void)c;
    g_frame++;
    if (g_log_from) {  // logging window: the categories chosen with --log are on for two frames from --log-from
        if ((int)g_frame == g_log_from) host_log_set(true);
        if ((int)g_frame == g_log_from + 2) host_log_set(false);
    }
    int w, h;
    bool want_shot = false;
    for (int f : g_shots) if (f == (int)g_frame) want_shot = true;
    static const char* redscan = getenv("PSX_REDSCAN");  // debug: save frames that have solid red rows
    if (!g_headless || want_shot || redscan) build_image(&w, &h);
    if (redscan && !want_shot) {
        int bad = 0;
        for (int j = 0; j < h; j++) {
            int red = 0;
            for (int i = 0; i < w; i++) {
                uint32_t p = g_pix[(size_t)j * w + i];
                if (((p >> 16) & 255) > 150 && ((p >> 8) & 255) < 60 && (p & 255) < 60) red++;
            }
            if (red * 2 > w) bad++;
        }
        if (bad) {
            char name[512];
            snprintf(name, sizeof name, "%s_%d.png", redscan, (int)g_frame);
            write_png(name, w, h, g_pix.data());
            logf("host", "red rows: %d in frame %d", bad, (int)g_frame);
        }
    }
    if (want_shot) save_shot((int)g_frame, w, h);
    if (want_shot && getenv("PSX_VRAM_SHOTS")) {  // debug: the whole 1024x512 VRAM next to the screenshot
        std::vector<uint32_t> all(1024 * 512);
        const uint16_t* vram = gpu_vram();
        for (int i = 0; i < 1024 * 512; i++) {
            uint16_t p = vram[i];
            all[i] = 0xFF000000u | (((p & 31) << 3) << 16) | ((((p >> 5) & 31) << 3) << 8) | (((p >> 10) & 31) << 3);
        }
        char name[512];
        snprintf(name, sizeof name, "%s_vram_%d.png", g_shot_prefix.c_str(), (int)g_frame);
        write_png(name, 1024, 512, all.data());
    }
    if (!g_headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) g_quit = true;
            if (e.type == SDL_EVENT_GAMEPAD_ADDED) open_first_gamepad();
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED) close_gamepad(e.gdevice.which);
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) g_quit = true;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_TAB) g_turbo = !g_turbo;
        }
        if (!g_tex || g_tex_w != w || g_tex_h != h) {
            if (g_tex) SDL_DestroyTexture(g_tex);
            g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            SDL_SetTextureScaleMode(g_tex, SDL_SCALEMODE_LINEAR);
            g_tex_w = w; g_tex_h = h;
        }
        SDL_UpdateTexture(g_tex, nullptr, g_pix.data(), w * 4);
        SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
        SDL_RenderClear(g_ren);
        int ww, wh;
        SDL_GetWindowSize(g_win, &ww, &wh);
        // keep a 4:3 picture
        float aspect = 4.0f / 3.0f;
        float dw = (float)ww, dh = dw / aspect;
        if (dh > wh) { dh = (float)wh; dw = dh * aspect; }
        SDL_FRect dst{((float)ww - dw) / 2, ((float)wh - dh) / 2, dw, dh};
        SDL_RenderTexture(g_ren, g_tex, nullptr, &dst);
        SDL_RenderPresent(g_ren);
    }
    if (!g_turbo && !g_headless) {
        auto now = std::chrono::steady_clock::now();
        auto target = g_last + std::chrono::microseconds(16667);
        if (now < target) SDL_DelayNS((Uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(target - now).count());
        g_last = std::chrono::steady_clock::now();
    }
    if (g_quit || (g_max_frames && (int)g_frame >= g_max_frames)) {
#ifdef PSX_TRACE_CALLS
        if (getenv("PSX_TRACE_DUMP")) trace_dump(300);
#endif
        if (const char* dump = getenv("PSX_DUMP_RAM")) { FILE* f = fopen(dump, "wb"); if (f) { fwrite(psx_ram, 1, PSX_RAM_SIZE, f); fclose(f); } }
        fflush(stderr);
        _Exit(0);
    }
}
