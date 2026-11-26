# Megaturret Fix (PAL)

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of
*Ratchet & Clank: Going Commando* (*Locked and Loaded*), **PAL disc only**
(`SCES_516.07`).

## The bug

On PAL, the Megaturret Glove's turrets land, then bob up and down on their
stalks and never fire a rocket. On NTSC-U they unfold, aim and fire.

A Megaturret (moby class 4967) runs `megaturret_update`. In its state 2 it
plays its unfold animation and waits for the animation to reach frame 6.5, the
last keyframe. Only then does it start its aiming animation and move to state 3,
where it looks for targets and fires. PAL plays animations 1.2 times as fast to
make up for running at 50 Hz, so the unfold can step past that last keyframe and
start over. The turret replays its unfold for ever (the bobbing) and never
reaches state 3. Both discs wait for the same 6.5.

The fold has the same problem. Only the turrets nearest Ratchet stay unfolded;
when too many are out, the others fold (state 5: the unfold played backward
until its frame is 1.0 or less, then state 6 until they are near enough again).
At PAL's speed the fold can skip past 1.0 as well, and those turrets bob too.

## The fix

After each game tick the mod walks the moby table and reads each Megaturret's
animation frame (the game's `moby_get_anim_value`, PAL's `func_0029FEF0`) while
it unfolds or folds. On the tick the animation starts over, the frame it waited
for gone by, the mod gives the turret what the game gives one that got there:

- **unfold:** its aiming animation (`anim_start_blended_with_flags(turret, 2,
  0, 0, 0)`, PAL's `func_002A7FF8`) and state 3. The game's own code then finds
  a target and fires, as on NTSC-U;
- **fold:** its resting animation (anim 0), playback 1 and state 6, folded until
  it is near enough to unfold again.

A turret still unfolding or folding after 30 ticks (0.6 s; either takes under 8
when it works) is finished anyway, in case a restart was missed. Nothing else
changes: the range, the damage, the rate of fire, how many turrets stay
unfolded and how long a turret lasts all stay the game's.

The mod does not load on NTSC-U or USA v2.00, whose Megaturrets already work.

## Building

```sh
python3 tools/fetch_sdk.py          # the OpenRAC mod SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build              # the stuck-unfold rule, and the library as the port loads it
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/megaturret_fix -o dist
```

## Releasing

Push a tag `v<version>` (the manifest's version). The workflow in
`.github/workflows/build.yml` builds the mod on Windows, Linux and macOS and
publishes the archives and `SHA256SUMS` on that release. Players then paste this
repository's address into the OpenRAC launcher's Mods tab (**Add from a link**).
