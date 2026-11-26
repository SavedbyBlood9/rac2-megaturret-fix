// SPDX-License-Identifier: MIT
// The stuck-animation rule alone (turret_watch.h): an unfold or a fold that works is left alone; one
// whose animation starts over is finished on that tick; one still going after STUCK_TICKS is
// finished anyway; other states, other classes and free slots are forgotten.
#include <stdio.h>
#include <stdlib.h>

#include "turret_watch.h"

#define CHECK(c)                                                    \
    do {                                                            \
        if (!(c)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
            exit(1);                                                \
        }                                                           \
    } while (0)

#define MEGA MEGATURRET_CLASS

int main(void) {
    turret_slot s = {0, 0, 0.0f};

    // An unfold the game finishes itself: 0, 1.2, 2.4 ... 6.0, then state 3. Never touched.
    for (int i = 0; i <= 5; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 1.2f * i) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_LOOKING, 0.0f) == TURRET_LEAVE);
    CHECK(s.state == 0);

    // A stuck unfold: up to 6.0, then back to 0.2. Finished on the tick it starts over.
    for (int i = 0; i <= 5; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 1.2f * i) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 0.2f) == TURRET_FINISH_UNFOLD);
    CHECK(s.state == 0 && s.ticks == 0);

    // An unfold blending in from another animation (a small dip early on) is not a restart.
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 3.0f) == TURRET_LEAVE);
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 0.5f) == TURRET_LEAVE);
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 1.7f) == TURRET_LEAVE);
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_LOOKING, 0.0f) == TURRET_LEAVE);

    // A fold the game finishes itself: 6.5 down to 0.5, then state 6.
    for (int i = 0; i <= 5; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 6.5f - 1.2f * i) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDED, 0.0f) == TURRET_LEAVE);

    // A stuck fold: down to 1.1, then back up to 6.9. Finished on that tick.
    for (int i = 0; i <= 4; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 5.9f - 1.2f * i) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 6.9f) == TURRET_FINISH_FOLD);

    // A frame that never moves (no restart seen): finished after STUCK_TICKS ticks, not before.
    for (int i = 1; i < STUCK_TICKS; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 3.0f) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 3.0f) == TURRET_FINISH_UNFOLD);
    for (int i = 1; i < STUCK_TICKS; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 3.5f) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 3.5f) == TURRET_FINISH_FOLD);

    // Going straight from the unfold to the fold counts the fold from its start.
    for (int i = 1; i < STUCK_TICKS - 1; ++i) {
        CHECK(turret_watch_step(&s, MEGA, MEGATURRET_UNFOLDING, 3.0f) == TURRET_LEAVE);
    }
    CHECK(turret_watch_step(&s, MEGA, MEGATURRET_FOLDING, 3.0f) == TURRET_LEAVE);
    CHECK(s.ticks == 1);

    // The Miniturret (4966), other mobys and free slots are left alone.
    for (int i = 0; i < 200; ++i) {
        CHECK(turret_watch_step(&s, 4966, MEGATURRET_UNFOLDING, i % 7) == TURRET_LEAVE);
        CHECK(turret_watch_step(&s, -1, MEGATURRET_FOLDING, i % 7) == TURRET_LEAVE);
    }
    CHECK(s.state == 0);

    puts("turret_watch: ok");
    return 0;
}
