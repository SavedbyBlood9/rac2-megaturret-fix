// SPDX-License-Identifier: MIT
// Megaturret Fix: Going Commando PAL's Megaturrets unfold, fold, aim and fire (README.md).
//
// After each game tick the mod walks the moby table and reads each Megaturret's animation frame
// while it unfolds (state 2) or folds (state 5). One whose animation has started over, the frame
// its state waits for gone by at PAL's animation speed (turret_watch.h), is given what
// megaturret_update gives one that got there:
//   the unfold: anim 2 (anim_start_blended_with_flags, PAL's func_002A7FF8) and state 3, from which
//               the game's own code looks for targets and fires;
//   the fold:   anim 0, playback 1 (+0x48) and state 6, folded until it is near enough again.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "openrac/openrac_mod.h"
#include "turret_watch.h"

OPENRAC_MOD_DEFINE("rac2-megaturret-fix", "0.2.0")

// The moby table: the address of its first moby, and past its last (g_mobyTableBase and
// g_mobyTableEnd; PAL's port/game/rac2-pal/host/rac2_addresses.h).
#define MOBY_TABLE_BASE 0x001B1B5Cu
#define MOBY_TABLE_END 0x001B1B64u
#define MOBY_SIZE 0x100u
#define MOBY_STATE 0x20u     // u8: megaturret_update's state; 0xFD and over a free or dead slot
#define MOBY_PLAYBACK 0x48u  // float: the animation's playback, -1 backward
#define MOBY_OCLASS 0xAAu    // s16: its class
#define STATE_GONE 0xFDu
// The slots the mod keeps a watch for: the game's table and the spare ones mods ask for.
#define MAX_SLOTS 8192u
// PAL's anim_start_blended_with_flags (v1.01's func_002A75E0) and moby_get_anim_value
// (func_0029F530).
#define ANIM_START "func_002A7FF8"
#define ANIM_FRAME "func_0029FEF0"

typedef void (*anim_start_fn)(openrac_gaddr moby, int32_t anim, int32_t a, int32_t b, int32_t c);
typedef float (*anim_frame_fn)(openrac_gaddr moby);

static const openrac_api* g_api;
static openrac_mod* g_mod;
static anim_start_fn g_anim_start;
static anim_frame_fn g_anim_frame;
static turret_slot g_slots[MAX_SLOTS];
static int g_logged_unfold;
static int g_logged_fold;

static int in_ram(uint32_t a) {
    return a >= 0x00100000u && a < 0x02000000u;
}

static uint32_t rd32(uint32_t a) {
    uint32_t v;
    memcpy(&v, g_api->memory + a, sizeof v);
    return v;
}

static int16_t rd16(uint32_t a) {
    int16_t v;
    memcpy(&v, g_api->memory + a, sizeof v);
    return v;
}

static void wrf(uint32_t a, float v) {
    memcpy(g_api->memory + a, &v, sizeof v);
}

static void log_once(int* logged, const char* text) {
    if (!*logged) {
        *logged = 1;
        g_api->log(g_mod, OPENRAC_LOG_INFO, text);
    }
}

static void finish(uint32_t moby, enum turret_action action) {
    if (action == TURRET_FINISH_UNFOLD) {
        g_anim_start(moby, MEGATURRET_ANIM_LOOK, 0, 0, 0);
        g_api->memory[moby + MOBY_STATE] = MEGATURRET_LOOKING;
        log_once(&g_logged_unfold, "a Megaturret stuck unfolding: finished its unfold");
    } else if (action == TURRET_FINISH_FOLD) {
        g_anim_start(moby, MEGATURRET_ANIM_REST, 0, 0, 0);
        wrf(moby + MOBY_PLAYBACK, 1.0f);
        g_api->memory[moby + MOBY_STATE] = MEGATURRET_FOLDED;
        log_once(&g_logged_fold, "a Megaturret stuck folding: finished its fold");
    }
}

static void on_frame(void* user) {
    (void)user;
    const uint32_t base = rd32(g_api->relocate_data(MOBY_TABLE_BASE));
    const uint32_t end = rd32(g_api->relocate_data(MOBY_TABLE_END));
    if (!in_ram(base) || !in_ram(end) || end <= base) {
        return;
    }

    uint32_t slots = (end - base) / MOBY_SIZE;
    if (slots > MAX_SLOTS) {
        slots = MAX_SLOTS;
    }
    for (uint32_t i = 0; i < slots; ++i) {
        const uint32_t moby = base + i * MOBY_SIZE;
        const uint8_t state = g_api->memory[moby + MOBY_STATE];
        const int16_t oclass = state >= STATE_GONE ? -1 : rd16(moby + MOBY_OCLASS);
        // The frame only for the states that wait on it.
        float frame = 0.0f;
        if (oclass == MEGATURRET_CLASS &&
            (state == MEGATURRET_UNFOLDING || state == MEGATURRET_FOLDING)) {
            frame = g_anim_frame(moby);
        }
        finish(moby, turret_watch_step(&g_slots[i], oclass, state, frame));
    }
}

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    g_api = api;
    g_mod = mod;
    if (!api->game || strcmp(api->game, "rac2") != 0 || !api->version ||
        strcmp(api->version, "pal") != 0) {
        api->fail(mod, "only for Going Commando PAL (NTSC-U's Megaturrets already fire)");
        return 1;
    }

    g_anim_start = (anim_start_fn)api->function(ANIM_START);
    g_anim_frame = (anim_frame_fn)api->function(ANIM_FRAME);
    if (!g_anim_start || !g_anim_frame) {
        api->fail(mod, "this build of the game has no " ANIM_START " (anim_start_blended_with_flags) "
                       "or " ANIM_FRAME " (moby_get_anim_value)");
        return 1;
    }

    memset(g_slots, 0, sizeof g_slots);
    g_logged_unfold = 0;
    g_logged_fold = 0;
    if (api->on_frame(mod, on_frame, NULL) != 0) {
        api->fail(mod, "could not watch the game's ticks");
        return 1;
    }

    api->log(mod, OPENRAC_LOG_INFO, "Megaturrets finish unfolding and folding at PAL's animation speed");
    return 0;
}
