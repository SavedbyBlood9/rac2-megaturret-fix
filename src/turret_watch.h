// SPDX-License-Identifier: MIT
// When a Megaturret's unfold or fold is stuck, and what the game does once it is done.
//
// A Megaturret (moby class 4967) runs megaturret_update (rac2-decomp's func_00306A40; PAL's own copy
// is func_00307CE8). Two of its states wait on an animation, read with moby_get_anim_value:
//   state 2, the unfold: anim 1 forward until its frame reaches 6.5 (its last keyframe); then anim 2
//            (anim_start_blended_with_flags(self, 2, 0, 0, 0)) and state 3, where it looks for
//            targets and fires (state 4);
//   state 5, the fold: anim 1 backward (playback -1, +0x48) until its frame is 1.0 or less; then
//            anim 0, playback 1 and state 6, folded, until it is among the turrets nearest the hero
//            again (only so many stay unfolded: the nearest), when it unfolds (state 2).
// PAL plays animations 1.2 times as fast to make up for its 50 Hz, so either animation can step past
// the frame its state waits for and start over: the turret bobs on its stalk, replaying the unfold
// or the fold, and never leaves the state. NTSC-U plays them at 1.0 and gets there.
//
// The watch sees an animation start over (its frame jumps back to the start: down by more than
// RESTART_JUMP from near the end of an unfold, up by as much near the end of a fold) and says to
// finish that state there, as the game would have a tick earlier. A turret still in one of the
// states after STUCK_TICKS is finished too, in case a restart was missed.
#ifndef RAC2_MEGATURRET_FIX_TURRET_WATCH_H
#define RAC2_MEGATURRET_FIX_TURRET_WATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The Megaturret, the turret the Megaturret Glove throws (MOBY_CLASS_MEGATURRET).
#define MEGATURRET_CLASS 4967
// megaturret_update's states.
#define MEGATURRET_UNFOLDING 2
#define MEGATURRET_LOOKING 3
#define MEGATURRET_FOLDING 5
#define MEGATURRET_FOLDED 6
// The animations the game starts as it leaves the unfold (to aim) and the fold (to rest).
#define MEGATURRET_ANIM_LOOK 2
#define MEGATURRET_ANIM_REST 0

// What the watch says to do with one turret this tick.
enum turret_action {
    TURRET_LEAVE = 0,      // nothing
    TURRET_FINISH_UNFOLD,  // anim 2, state 3
    TURRET_FINISH_FOLD     // anim 0, playback 1, state 6
};

// One moby slot's watch: zeroed for a slot never seen.
typedef struct turret_slot {
    uint8_t state;  // the state the last tick saw (0: none)
    uint8_t ticks;  // ticks in that state
    float frame;    // the animation's frame the last tick saw
} turret_slot;

// An unfold that has started over drops from at least UNFOLD_NEAR_END by more than RESTART_JUMP; a
// fold that has started over rises from at most FOLD_NEAR_END by as much.
#define UNFOLD_NEAR_END 4.0f
#define FOLD_NEAR_END 3.0f
#define RESTART_JUMP 2.0f
// Ticks in the unfold or the fold before the watch finishes it anyway (0.6 s at 50 Hz; each takes
// under 8 ticks when it works).
#define STUCK_TICKS 30

// Moves one moby slot's watch on by one tick, given the moby's class and state now and its
// animation's frame (moby_get_anim_value; read only for a Megaturret unfolding or folding, any value
// otherwise). Returns what to do with it now.
enum turret_action turret_watch_step(turret_slot* slot, int16_t oclass, uint8_t state, float frame);

#ifdef __cplusplus
}
#endif

#endif
