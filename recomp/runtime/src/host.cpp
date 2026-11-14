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

struct Press { int button; int from, len; };
std::vector<Press> g_script;
std::vector<int> g_shots;
std::string g_shot_prefix = "shot";

}  // namespace

bool g_turbo = false;
int g_max_frames = 0;
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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
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

uint16_t host_pad_state(int port) {
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
    return state;
}

void host_frame(Cpu* c) {
    (void)c;
    g_frame++;
    int w, h;
    bool want_shot = false;
    for (int f : g_shots) if (f == (int)g_frame) want_shot = true;
    if (!g_headless || want_shot) build_image(&w, &h);
    if (want_shot) save_shot((int)g_frame, w, h);
    if (!g_headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) g_quit = true;
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
        if (const char* dump = getenv("PSX_DUMP_RAM")) { FILE* f = fopen(dump, "wb"); if (f) { fwrite(psx_ram, 1, PSX_RAM_SIZE, f); fclose(f); } }
        fflush(stderr);
        _Exit(0);
    }
}
