# Phase 1 results (2 Nov 2026)

**Result: pass, after one fix found while checking the test log.**

Source: one in-game session of 53 attempts, written to the attempt CSV, with the per-tick debug log recording at the same time. The player read the console during the session and confirmed that makes and misses, slowed-down detection, floor jumps being ignored and corner counts were all correct. Make counts per wall were not kept by hand in this session, so the exact count check is the Phase 0 replay below.

## What was checked

| Check | Result |
|---|---|
| Make or miss matches what was seen | Yes, confirmed by watching |
| 50% and 100% attempts detected and counted apart | Yes, 31 at 0.50 and 22 at 1.00, with no other values |
| Jumps from the flat floor ignored | Yes |
| Corners counted apart from flat walls | Yes |
| CSV loads in pandas, one header, no units | Yes |
| Replay of the Phase 0 50% log matches the count made in game | Yes, right wall 10 of 10, left wall 6 of 12 (`tests/ReplayLogs`) |

## Bug found and fixed: "No second jump" was wrong

Seven attempts said "No second jump" and looked correct while playing. The debug log showed otherwise: 6 of them had a second press, 6 to 8 ticks after the first, while all 4 wheels were still touching. The game ignores a press like that and starts no flip, so no event fired and the tracker assumed no press happened.

These now report `too_early`, for example "Second press too early: 58 ms, 4 wheels still on the wall". The CSV has a new column, `wheels_at_second_press`, and press to press is now recorded for these attempts. A press that opens the next attempt is also no longer counted against the previous one.

Across all four debug logs, 393 presses:

| Wheels touching at the press | Flip | Double jump | Nothing |
|---|---|---|---|
| 4 | 0 | 0 | 11 |
| 1 or 2 | 6 | 0 | 2 (no flip left) |
| 0 | 144 | 29 | 2 (no flip left) |

So all 4 wheels down means the press does nothing. No press landed with exactly 3 wheels down.

## There is a lower limit on press to press

Wheels stay on a flat wall for about 7 ticks after the first jump, so 58 ms is about the earliest second press that can work. Every press at 6 ticks was ignored. Timing is not "the faster the better", and the press to press bar should show a too-early zone.

Corners seem to hold the wheels on longer. 6 of 7 corner attempts were too early, at 50 to 67 ms in game time, and the one make was at 58 ms. This does not support the brief's note that corners are "much easier", though 7 attempts at one speed is a small sample and every one was at 50%.

## Session numbers

| Bucket | Makes | Average press to press on makes |
|---|---|---|
| Flat walls, 100% | 6 of 22 (27%) | 110 ms |
| Flat walls, 50% | 21 of 24 (88%) | 63 ms |
| Corners, 50% | 1 of 7 | 58 ms |

## Lead for Phase 3: flip angle at 100%

Among real dash attempts at 100% (positive flip angle, press to press of 21 ticks or less), both walls had the same timing, a median of 14 ticks, but different flip angles:

| Wall | Makes | Median flip angle | Range |
|---|---|---|---|
| Left | 0 of 9 | 35 | 30 to 42 |
| Right | 6 of 12 | 58 | 47 to 68 |

| Flip angle | Makes |
|---|---|
| 45 or less | 0 of 9 |
| 45 to 50 | 0 of 1 |
| 50 to 60 | 4 of 7 |
| Over 60 | 2 of 4 |

At 50% speed, every attempt on both walls at 12 ticks or less was a make, including angles under 35 degrees. This fits the brief's idea that a more forward flip needs faster timing, and the Phase 0 finding that forward flips push less into the wall.

It is not proven. Wall and angle cannot be separated in this data, the left wall reps came first, and the sample is small. Phase 0 also has one make at 36 degrees and 13 ticks. A direct test is to flick more sideways on the left wall at 100%.

## Not exercised in game yet

These are covered by the code, and some by replay, but did not come up in this session:

- `curve` surfaces, which had no attempts.
- `not_fully_on_wall` and `spam_input`, which had none.
- `double_jump` and `not_upright` as live diagnoses. `double_jump` appears when replaying Phase 0 logs.
- The `too_early` fix, which has only been run through replay so far.
