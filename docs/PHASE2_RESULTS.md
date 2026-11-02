# Phase 2 results (2 Nov 2026)

**Result: the overlay and settings window work, with changes made after the first play session.** The player's verdict was "overall this is really good so far". The second round of changes below has not been seen on screen yet.

Source: 323 attempts at 100% speed in `attempts_2026-11-02_2.csv`, all played with the overlay. That file was still being written when these numbers were taken.

## Changes after the first session

| Asked for | What changed |
|---|---|
| Text under the stick display was cramped | The labels are now spaced from their measured width, and sit lower |
| Coloured sectors for good flip angles | Green and yellow sectors on the rim of the stick display. They show on the ceiling side for the current wall, and on both sides before the first attempt. The flip line takes the colour of its band |
| Flip angle bands adjustable | Four new cvars and a settings section. Starting values: too forward below 40, green from 50 to 80, too sideways above 90 |
| Flip angle should come before speed | "Flip angle" is now checked before "too late" in the miss reasons, with new "too forward" and "too sideways" reasons |
| Option for small speed gains to show red | `walldash_speed_red_below`, default 0, which keeps the old behaviour |
| Track the stick at all three moments | New CSV columns for stick angle and stick sum at first press, release and second press |

## What drives makes so far

These numbers cover flat walls at 100% with no air roll, using dodges with a positive flip angle and press to press of 21 ticks or less: 264 attempts, 147 makes.

### Flip angle

| Flip angle | Makes | Make rate | Median speed gain on makes |
|---|---|---|---|
| Up to 40 | 4 of 34 | 12% | 487 |
| 40 to 50 | 12 of 30 | 40% | 316 |
| 50 to 60 | 22 of 29 | 76% | 271 |
| 60 to 70 | 41 of 61 | 67% | 203 |
| 70 to 80 | 47 of 71 | 66% | 128 |
| 80 to 90 | 21 of 38 | 55% | 23 |

This answers the 45 versus 60 degree question from the brief, for this player so far. More forward flips gain more speed but make far less often. 50 to 60 has the best make rate and still gains well.

### Press to press

| Ticks | Time | Makes | Make rate |
|---|---|---|---|
| 11 or less | 92 ms or less | 19 of 22 | 86% |
| 12 | 100 ms | 34 of 44 | 77% |
| 13 | 108 ms | 48 of 81 | 59% |
| 14 | 117 ms | 32 of 68 | 47% |
| 15 | 125 ms | 12 of 37 | 32% |
| 16 to 21 | 133 to 175 ms | 2 of 12 | 17% |

Timing still matters a lot. These tables look at one factor at a time, so Phase 3 needs to compare them together.

### Left and right walls

| Wall | All makes | Attempts under 50 degrees | Makes at 50 degrees or more |
|---|---|---|---|
| Left | 85 of 175 (49%) | 36% | 69 of 112 (62%) |
| Right | 62 of 89 (70%) | 1% | 62 of 88 (70%) |

Most of the gap between walls comes from the left wall's flatter flips. This fits the player's own finding that their stick comes back down on the left. The stick angle was already low by the release, moving a median of 0 degrees between release and second press on left wall misses, so the drift seems to happen before the release.

### Does the stick need to be set at all three moments?

Not as far as this data shows. "Set" here means the stick was past the deadzone and within 20 degrees of the final flip direction, which is a threshold chosen for this first look.

| Set at | Makes | Make rate |
|---|---|---|
| Second press only | 44 of 62 | 71% |
| Release and second press | 83 of 161 | 52% |
| All three | 19 of 40 | 48% |

Only 20 of 147 makes had the stick set at the first press. Setting it early did not help, and flicking late did better, though this may be because of other differences between those attempts. Being set at the second press is true by definition, because that stick position decides the flip.

### Speed at the first press

Checked later, on 388 attempts with the same filters across all sessions so far. Speed here is measured in the wall plane.

| Speed at first press (uu/s) | Attempts | Make rate | At 50 degrees or more and 13 ticks or less | At 50 degrees or more and 14 ticks or more |
|---|---|---|---|---|
| Up to 1100 | 12 | 8% | 1 attempt | 1 attempt |
| 1100 to 1300 | 62 | 45% | 71% of 21 | 44% of 16 |
| 1300 to 1500 | 144 | 47% | 79% of 58 | 26% of 54 |
| 1500 to 1700 | 88 | 66% | 93% of 41 | 50% of 34 |
| 1700 to 1900 | 38 | 79% | 100% of 19 | 75% of 12 |
| 1900 and up | 44 | 91% | 100% of 20 | 86% of 22 |

Going fast seems to make the timing much more forgiving, which supports the player's feeling that being near supersonic makes dashes easier. Median speed gain on makes falls from 265 to 113 across the same range.

The CSV now also has `speed_end`, `total_speed_first_press` and `total_speed_end`, because `speed_peak` is only filled in for makes.

### Caveats

- The make rate rose from 64 of 132 in the first half to 83 of 132 in the second, so practice is mixed in with everything above. Phase 3 needs to account for this.
- Attempts were not randomly assigned, so these are patterns and not proven causes.

## Now exercised in game

`too_early` (11), `double_jump` (4), `not_fully_on_wall` (6), `scraped` (5), and `curve` surfaces (8). `not_upright` and `spam_input` have still not come up.

## Not confirmed yet

- The second round of changes on screen.
- Whether settings survive a game restart.
- A rebuild reloads the plugin and clears the session stats on screen. The CSV is not affected. Published versions will not rebuild during play, so this only affects development.
