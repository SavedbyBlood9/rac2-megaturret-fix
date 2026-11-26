// SPDX-License-Identifier: MIT
// When a Megaturret's unfold or fold is stuck (turret_watch.h).
#include "turret_watch.h"

static enum turret_action finish(turret_slot* slot, enum turret_action action) {
    // The mod puts it in another state: the next tick starts over.
    slot->state = 0;
    slot->ticks = 0;
    return action;
}

enum turret_action turret_watch_step(turret_slot* slot, int16_t oclass, uint8_t state, float frame) {
    const int unfolding = state == MEGATURRET_UNFOLDING;
    const int folding = state == MEGATURRET_FOLDING;

    // Anything else in the slot, or the turret in a state that waits on nothing: forget it.
    if (oclass != MEGATURRET_CLASS || (!unfolding && !folding)) {
        slot->state = 0;
        slot->ticks = 0;
        return TURRET_LEAVE;
    }

    // Just come into the state: nothing to compare with yet.
    if (slot->state != state) {
        slot->state = state;
        slot->ticks = 1;
        slot->frame = frame;
        return TURRET_LEAVE;
    }

    const float last = slot->frame;
    slot->frame = frame;
    ++slot->ticks;
    const enum turret_action action = unfolding ? TURRET_FINISH_UNFOLD : TURRET_FINISH_FOLD;

    // The animation started over: the frame the state waits for went by.
    if (unfolding && last >= UNFOLD_NEAR_END && frame < last - RESTART_JUMP) {
        return finish(slot, action);
    }
    if (folding && last <= FOLD_NEAR_END && frame > last + RESTART_JUMP) {
        return finish(slot, action);
    }

    // There far longer than either takes.
    if (slot->ticks >= STUCK_TICKS) {
        return finish(slot, action);
    }

    return TURRET_LEAVE;
}
