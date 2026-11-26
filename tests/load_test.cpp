// SPDX-License-Identifier: MIT
// The mod's library as the port loads it, against a host table and game memory made here: it
// reports its id and the interface its manifest names, refuses NTSC-U, USA v2.00 and another game,
// and refuses a build without anim_start_blended_with_flags or moby_get_anim_value. On PAL, a
// Megaturret whose unfold starts over gets the game's aiming animation and state 3 on that tick; one
// whose fold starts over gets the resting animation, playback 1 and state 6; one the game unfolds
// itself, a Miniturret, a dead one and a moby past the table's end are left alone.
//
// Run with the library's path: load_test <megaturret_fix.dll|.so|.dylib>.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "openrac/openrac_mod.h"

#if defined(_WIN32)
#include <windows.h>

static void* open_library(const char* path) {
    return (void*)LoadLibraryA(path);
}

static void* symbol(void* lib, const char* name) {
    return (void*)GetProcAddress((HMODULE)lib, name);
}
#else
#include <dlfcn.h>

static void* open_library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void* symbol(void* lib, const char* name) {
    return dlsym(lib, name);
}
#endif

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

namespace {

constexpr std::uint32_t kMemory = 0x02000000u;
constexpr std::uint32_t kTableBase = 0x001B1B5Cu;
constexpr std::uint32_t kTableEnd = 0x001B1B64u;
constexpr std::uint32_t kFirstMoby = 0x00300000u;
constexpr std::uint32_t kSlots = 16;

std::uint8_t* g_mem;
std::string g_failed;
std::vector<std::string> g_logs;
openrac_frame_fn g_frame;
void* g_frame_user;
bool g_has_anim_start = true;
bool g_has_anim_frame = true;

struct AnimCall {
    std::uint32_t moby;
    std::int32_t anim, a, b, c;
};

std::vector<AnimCall> g_anim_calls;
// Each moby's animation frame, as moby_get_anim_value would read it.
std::map<std::uint32_t, float> g_frames;
int g_frame_reads;

void game_anim_start(openrac_gaddr moby, std::int32_t anim, std::int32_t a, std::int32_t b,
                     std::int32_t c) {
    g_anim_calls.push_back({moby, anim, a, b, c});
}

float game_anim_frame(openrac_gaddr moby) {
    ++g_frame_reads;
    return g_frames[moby];
}

template <class T>
void put(std::uint32_t at, T v) {
    std::memcpy(g_mem + at, &v, sizeof v);
}

template <class T>
T get(std::uint32_t at) {
    T v;
    std::memcpy(&v, g_mem + at, sizeof v);
    return v;
}

std::uint32_t moby(std::uint32_t slot) {
    return kFirstMoby + slot * 0x100u;
}

void set_moby(std::uint32_t slot, std::int16_t oclass, std::uint8_t state) {
    put<std::int16_t>(moby(slot) + 0xAAu, oclass);
    g_mem[moby(slot) + 0x20u] = state;
}

std::uint8_t state_of(std::uint32_t slot) {
    return g_mem[moby(slot) + 0x20u];
}

void host_log(openrac_mod*, int, const char* text) {
    g_logs.push_back(text);
}

void host_fail(openrac_mod*, const char* reason) {
    g_failed = reason;
}

void* host_function(const char* name) {
    if (g_has_anim_start && std::strcmp(name, "func_002A7FF8") == 0) {
        return reinterpret_cast<void*>(&game_anim_start);
    }
    if (g_has_anim_frame && std::strcmp(name, "func_0029FEF0") == 0) {
        return reinterpret_cast<void*>(&game_anim_frame);
    }
    return nullptr;
}

int host_on_frame(openrac_mod*, openrac_frame_fn fn, void* user) {
    g_frame = fn;
    g_frame_user = user;
    return 0;
}

openrac_gaddr identity(openrac_gaddr a) {
    return a;
}

openrac_api make_api(const char* game, const char* version) {
    openrac_api api;
    std::memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.api_major = OPENRAC_MOD_API_MAJOR;
    api.api_minor = OPENRAC_MOD_API_MINOR;
    api.game = game;
    api.version = version;
    api.openrac_version = "test";
    api.platform = "test";
    api.memory = g_mem;
    api.log = host_log;
    api.fail = host_fail;
    api.function = host_function;
    api.on_frame = host_on_frame;
    api.relocate_data = identity;
    return api;
}

void tick() {
    g_frame(g_frame_user);
}

int count_logs(const char* part) {
    int n = 0;
    for (const std::string& line : g_logs) {
        n += line.find(part) != std::string::npos;
    }
    return n;
}

}  // namespace

int main(int argc, char** argv) {
    CHECK(argc == 2);
    void* lib = open_library(argv[1]);
    CHECK(lib != nullptr);
    auto query = (openrac_mod_query_fn)symbol(lib, OPENRAC_MOD_QUERY);
    auto load = (openrac_mod_load_fn)symbol(lib, OPENRAC_MOD_LOAD);
    CHECK(query != nullptr && load != nullptr);

    openrac_mod_info info;
    std::memset(&info, 0, sizeof info);
    info.size = sizeof info;
    query(&info);
    CHECK(std::strcmp(info.id, "rac2-megaturret-fix") == 0);
    CHECK(std::strcmp(info.version, "0.2.0") == 0);
    CHECK(info.api_major == OPENRAC_MOD_API_MAJOR && info.api_minor == 0);

    g_mem = static_cast<std::uint8_t*>(std::calloc(1, kMemory));
    CHECK(g_mem != nullptr);

    // Not PAL, or a build without one of the two functions: refused, nothing watched.
    const char* games[][2] = {{"rac2", "ntsc"}, {"rac2", "ntsc2"}, {"rac1", "pal"}};
    for (auto& g : games) {
        openrac_api api = make_api(g[0], g[1]);
        g_failed.clear();
        CHECK(load(&api, nullptr) != 0 && !g_failed.empty() && g_frame == nullptr);
    }
    for (int missing = 0; missing < 2; ++missing) {
        g_has_anim_start = missing != 0;
        g_has_anim_frame = missing == 0;
        openrac_api api = make_api("rac2", "pal");
        g_failed.clear();
        CHECK(load(&api, nullptr) != 0 && !g_failed.empty() && g_frame == nullptr);
    }
    g_has_anim_start = g_has_anim_frame = true;

    // PAL: loads and watches the ticks.
    openrac_api api = make_api("rac2", "pal");
    g_failed.clear();
    CHECK(load(&api, nullptr) == 0 && g_failed.empty() && g_frame != nullptr);

    // No moby table yet (a level loading): nothing happens.
    for (int i = 0; i < 100; ++i) {
        tick();
    }
    CHECK(g_anim_calls.empty() && g_frame_reads == 0);

    put<std::uint32_t>(kTableBase, kFirstMoby);
    put<std::uint32_t>(kTableEnd, moby(kSlots));
    for (std::uint32_t s = 0; s < kSlots + 4; ++s) {
        set_moby(s, 0, 0xFF);  // free
    }
    set_moby(1, 4967, 2);           // a Megaturret whose unfold will start over
    set_moby(2, 4967, 2);           // one the game unfolds itself
    set_moby(3, 4966, 2);           // a Miniturret unfolding
    set_moby(4, 4967, 0xFE);        // a dead one
    set_moby(5, 4967, 4);           // one firing
    set_moby(kSlots + 1, 4967, 2);  // past the table's end

    // Both unfold 0, 1.2 ... 6.0; the game finishes slot 2 there.
    for (int i = 0; i <= 5; ++i) {
        g_frames[moby(1)] = g_frames[moby(2)] = 1.2f * i;
        tick();
    }
    set_moby(2, 4967, 3);
    CHECK(g_anim_calls.empty() && state_of(1) == 2);

    // Slot 1's unfold starts over: finished on that tick.
    g_frames[moby(1)] = 0.2f;
    tick();
    CHECK(g_anim_calls.size() == 1);
    CHECK(g_anim_calls[0].moby == moby(1) && g_anim_calls[0].anim == 2);
    CHECK(g_anim_calls[0].a == 0 && g_anim_calls[0].b == 0 && g_anim_calls[0].c == 0);
    CHECK(state_of(1) == 3);
    CHECK(state_of(2) == 3 && state_of(3) == 2 && state_of(4) == 0xFE && state_of(5) == 4);
    CHECK(state_of(kSlots + 1) == 2);
    CHECK(count_logs("stuck unfolding") == 1);

    // Firing (states 3 and 4) is left alone, and its frame is never read.
    const int reads = g_frame_reads;
    for (int i = 0; i < 200; ++i) {
        tick();
    }
    CHECK(g_anim_calls.size() == 1 && g_frame_reads == reads);

    // Slot 1 folds (too many out: the game plays anim 1 backward, playback -1), and its fold starts
    // over below 1.0's frame: anim 0, playback 1, state 6.
    set_moby(1, 4967, 5);
    put<float>(moby(1) + 0x48u, -1.0f);
    for (int i = 0; i <= 4; ++i) {
        g_frames[moby(1)] = 5.9f - 1.2f * i;
        tick();
    }
    CHECK(g_anim_calls.size() == 1 && state_of(1) == 5);
    g_frames[moby(1)] = 6.9f;
    tick();
    CHECK(g_anim_calls.size() == 2);
    CHECK(g_anim_calls[1].moby == moby(1) && g_anim_calls[1].anim == 0);
    CHECK(get<float>(moby(1) + 0x48u) == 1.0f && state_of(1) == 6);
    CHECK(count_logs("stuck folding") == 1);

    // Folded (state 6) it waits, left alone; near again, it unfolds and starts over once more.
    for (int i = 0; i < 50; ++i) {
        tick();
    }
    CHECK(g_anim_calls.size() == 2);
    set_moby(1, 4967, 2);
    for (int i = 0; i <= 5; ++i) {
        g_frames[moby(1)] = 1.2f * i;
        tick();
    }
    g_frames[moby(1)] = 0.2f;
    tick();
    CHECK(g_anim_calls.size() == 3 && state_of(1) == 3);
    CHECK(count_logs("stuck unfolding") == 1);  // one line a session

    std::puts("load: ok");
    return 0;
}
