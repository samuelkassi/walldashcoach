# Phase 4 results (2 Nov 2026)

**Result: chains and the sample counter work and the analysis is done. Reset to wall was built in this phase but removed before version 1.0.0, without having been tested in game. It may return in a later version.**

Source: 1,080 attempts recorded after the chain measures were added, in `data/attempts.csv`. That gives 888 real dashes, of which 647 are follow-ups in a chain (310 left wall, 337 right wall), with 89% makes. Run `python tools/analyze_chains.py` and `python tools/analyze_normal_speed.py` to reproduce this.

"Real dashes" means flat walls, 100% speed, no air roll, a dodge, a flip angle from 0 to 90 degrees and press to press of 21 ticks or less.

## Testing the player's theories

### A smaller turn keeps more speed: supported

Speed lost is the previous dash's peak speed minus the next first press speed. Turn size is the highest minus the lowest travel angle in the 0.25 s before that press. These dashes have a previous peak under 2250 uu/s, to stay clear of the game's speed cap.

| Turn size | Dashes | Median speed lost |
|---|---|---|
| Up to 5 degrees | 133 | 45 uu/s |
| 5 to 8 | 151 | 66 |
| 8 to 11 | 116 | 81 |
| 11 to 15 | 67 | 99 |
| 15 to 20 | 33 | 145 |
| Over 20 | 35 | 206 |

With the other factors held equal, in a regression explaining 66% of the variation:

| Factor | Effect on speed lost | 95% interval |
|---|---|---|
| Each extra degree of turn | +5.2 uu/s | 3.6 to 6.8 |
| Each extra degree heading upward at the start of the window | +4.8 uu/s | 4.1 to 5.6 |
| Each extra tick between first presses | +0.55 uu/s | 0.38 to 0.72 |
| Each uu of height dropped in the window | -0.31 uu/s | -0.52 to -0.10 |
| Each extra uu/s of previous peak | +0.085 uu/s | 0.05 to 0.12 |

Left or right wall and side or back wall made no significant difference. Speed is also lost faster the longer the gap after landing, while the make rate barely changes:

| Ticks since the last landing | Dashes | Make rate | Median speed lost |
|---|---|---|---|
| 30 to 40 | 108 | 90% | 50 uu/s |
| 40 to 50 | 195 | 91% | 70 |
| 50 to 60 | 146 | 88% | 87 |
| 60 to 80 | 137 | 87% | 105 |

Only 2 follow-ups came sooner than 30 ticks, so nothing can be said below that.

### The downward movement lets the car leave the wall: not as stated

- All 4 wheels stayed down for the whole 0.25 s before the press in 608 of 647 follow-ups, and at least 2 stayed down in the rest. The car does not leave the wall before the jump.
- Turn size and height dropped have no significant effect on whether the next dash makes, with the rest held equal (odds ratios 0.85 per 5 degrees, p 0.50, and 0.98 per 10 uu, p 0.85).

The turn matters for speed, not for making the dash.

### What may explain the feeling: speed away from the wall

The car's speed away from the wall at the first press was not part of any theory, but it predicts makes strongly.

| Speed away from the wall at the press | Dashes | Make rate |
|---|---|---|
| 5 uu/s or less, including into the wall | 65 | 68% |
| 5 to 10 | 20 | 75% |
| 10 to 13 | 52 | 71% |
| 13 to 16 | 228 | 82% |
| 16 to 19 | 206 | 91% |
| 19 to 22 | 273 | 95% |
| Over 22 | 44 | 95% |

It rises with speed (Spearman 0.84), but still matters with speed held equal: each extra 10 uu/s multiplies the odds of a make by 2.2 (95% interval 1.8 to 2.8). Trained on the first 70% of these dashes and tested on the rest, adding it lifts AUC from 0.906 to 0.953. Adding turn size and height dropped instead only reaches 0.909.

We do not know yet what produces it, or whether a player can control it. Moving away from the wall may be part of what feels like the car leaving it. This needs a dedicated test.

### Pressing while moving into the wall

54 of 890 dashes had the car moving into the wall at the press. They made 61% of the time against 89% otherwise, and 39% never touched the wall again. It does not come from pressing too soon after landing: the median was 142 ticks since the last landing. The cause is unknown.

## Reaching supersonic

115 of 275 chains reached 2200 uu/s, and the longest had 9 makes.

| First speed in the chain | Chains that reached it | Median dash number |
|---|---|---|
| Under 1300 | 7 | 6 |
| 1300 to 1500 | 19 | 5 |
| 1500 to 1700 | 13 | 4 |
| 1700 to 1900 | 32 | 2 |
| 1900 to 2100 | 32 | 1 |

The first three rows are broadly in line with the player's estimate of 5 to 8 dashes, a little faster at the higher starts. Net speed per dash is the gain on a make minus what was lost before it, for chain dashes starting under 2000 uu/s:

| Flip angle | Dashes | Median gain | Median lost | Median net | Make rate, all new dashes |
|---|---|---|---|---|---|
| 40 to 50 | 16 | 313 | 127 | 162 | 32 to 70% |
| 50 to 60 | 130 | 262 | 109 | 144 | 92 to 94% |
| 60 to 70 | 137 | 209 | 92 | 116 | 90 to 91% |
| 70 to 80 | 64 | 132 | 68 | 65 | 87 to 89% |
| 80 to 90 | 10 | 26 | 63 | -26 | 72 to 78% |

So 50 to 60 degrees is the best angle for chaining: nearly the highest net gain, with the highest make rate.

As a rough illustration only, going from 1400 to 2200 uu/s at a net 144 per dash takes between 5 and 6 dashes. If the speed lost dropped from 109 to the 45 seen with the smallest turns, the same climb would take about 4.

## Left and right walls

The gap has closed on follow-ups: 90% on the left and 88% on the right. Flip angles still differ, with a median of 66 degrees on the left and 59 on the right.

## Extending Phase 3: testing it on new data

The Phase 3 model was trained on 1,212 dashes. These 1,439 newer dashes were never part of that, which makes this a stronger test than the original time split.

| Measure | Result |
|---|---|
| AUC, speed, timing and angle | 0.849 |
| Brier score, against 0.145 for always predicting the old make rate | 0.074 |
| Mean predicted make rate, against 85.5% actual | 86.4% |
| AUC on follow-ups | 0.913 |
| AUC on the first dash of a chain | 0.761 |

It predicts first dashes less well, expecting 88% when the real rate was 82%. The first dash is 241 of the dashes in this comparison.

The bands also hold up:

| Band | Press to press | Flip angle | Hold |
|---|---|---|---|
| Green | 91% of 1,286 | 92% of 956 | 90% of 919 |
| Yellow | 55% of 76 | 84% of 382 | 80% of 504 |
| Red | 29% of 77 | 34% of 101 | 19% of 16 |

| Greens | Dashes | Make rate |
|---|---|---|
| 0 | 41 | 22% |
| 1 | 218 | 64% |
| 2 | 597 | 87% |
| 3 | 583 | 96% |

Across all 2,651 dashes, the best flip angle is 67 degrees with the rest held equal, against 66 before. The left and right walls still show no difference (odds ratio 1.08, p 0.60).

**One correction to Phase 3:** the back wall advantage has weakened to an odds ratio of 1.25 (p 0.11), so it should not be treated as established.

The default bands stay as they are, as decided, until after public feedback. The new data does show one thing to look at then: at 1300 to 1500 uu/s, 14 ticks now makes 55% of the time (11 dashes), which would suit a yellow zone where there is currently none.

## Plugin observations

All 29 misses reported as "Unclassified" never touched the wall again. They would be better described as a miss reason of their own.

## Limits

One player, one evening, and observational data. Players did not have turn size assigned to them, so a smaller turn could partly stand in for smoother play in general. Attempts are not independent, so the intervals and p values are optimistic. Turn size is measured over a fixed 0.25 s window, which may not capture the whole movement.

## Still to check in game

- Whether settings and the counter survive a game restart.
- `walldash_export_share_data`.
