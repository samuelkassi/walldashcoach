# Calibration: what drives a wall dash make

To reproduce this, run `python tools/analyze.py` and `python tools/band_tables.py` from the project root. They read [`data/attempts.csv`](../data/attempts.csv). `tools/build_dataset.py` shows how that file was made from the plugin's own CSVs.

## Sample

1212 real dashes from the 1484 attempts in the dataset: flat walls, 100% speed, no air roll, a dodge with a flip angle from 0 to 90 degrees, and press to press of 21 ticks or less. 858 were makes (71%). The left wall has 627 and the right wall 585.

## Answer in one paragraph

Three things decide most attempts: speed at the first press, press to press time and flip angle. Using only those three, a model trained on the earlier 70% of attempts separated makes from misses on the later 30% with an AUC of 0.895 and a Brier score of 0.076, against 0.177 for always predicting the training rate. Hold time adds a smaller but real effect. Once those are accounted for, the wall itself, stick position before the second press and practice over time add almost nothing.

## Effects when everything is compared together

Logistic regression on all 1212 dashes, pseudo R squared 0.518. Odds above 1 help a make.

| Factor | Odds ratio | 95% interval | p |
|---|---|---|---|
| Each extra tick of press to press | 0.33 | 0.27 to 0.41 | below 0.001 |
| Each extra tick of hold | 0.47 | 0.39 to 0.56 | below 0.001 |
| Each extra 100 uu/s at the first press | 1.81 | 1.64 to 2.00 | below 0.001 |
| Flip angle, 10 degrees, linear part | 1.95 | 1.63 to 2.33 | below 0.001 |
| Flip angle, curved part | 0.56 | 0.51 to 0.62 | below 0.001 |
| Back wall instead of side wall | 1.70 | 1.16 to 2.50 | 0.007 |
| Quick follow-up dash, under 150 ticks after the last | 0.62 | 0.40 to 0.97 | 0.036 |
| Right wall instead of left wall | 1.02 | 0.66 to 1.58 | 0.94 |
| Stick already set at first press | 1.31 | 0.70 to 2.44 | 0.40 |
| Stick already set at release | 1.16 | 0.71 to 1.92 | 0.55 |
| Practice, first attempt to last | 1.25 | 0.61 to 2.58 | 0.54 |
| Travel angle, per 5 degrees upward | 0.90 | 0.78 to 1.03 | 0.13 |

The linear and curved flip angle terms together put the best angle at about 66 degrees, holding the rest equal.

## Answers to the questions we started with

**Which timing predicts makes best?** Press to press (AUC 0.753), then hold (0.691). Release to press is weak (0.564). The community's press to press measure is the right one to lead with, and the FastAerialTrainer-style release to press measure adds little.

**45 or 60 degrees?** Closer to 60. The speed gain trade-off is real, but make rates fall sharply below 50.

| Flip angle | Dashes | Make rate | Median speed gain on makes |
|---|---|---|---|
| 40 to 45 | 31 | 16% | 282 |
| 45 to 50 | 47 | 62% | 307 |
| 50 to 55 | 62 | 81% | 283 |
| 55 to 60 | 102 | 88% | 257 |
| 60 to 65 | 203 | 87% | 219 |
| 65 to 70 | 182 | 73% | 186 |
| 70 to 75 | 210 | 79% | 140 |
| 75 to 80 | 125 | 74% | 104 |
| 80 to 85 | 70 | 66% | 69 |
| 85 to 90 | 108 | 61% | 2 |

A flip of 85 to 90 degrees often reattaches but gains almost no speed.

**Does the stick need the dodge direction at all three moments?** No. Here "set" means past the deadzone and within 20 degrees of the final flip direction, a threshold chosen for this analysis. The stick at the second press decides the flip, so it is always set by definition.

| Stick set at | Dashes | Make rate |
|---|---|---|
| Second press only | 280 | 76% |
| Release and second press | 788 | 70% |
| All three | 144 | 62% |

The regression finds no significant effect from setting it earlier.

**Is the left wall harder?** Not once the other factors are counted (odds ratio 1.02, p 0.94). The left wall's lower make rate came from how it was played. Over the left wall's first and last 200 dashes, the make rate went from 48% to 80%, the median flip angle from 59 to 75 degrees and the median start speed from 1411 to 1654 uu/s, with median press to press unchanged at 13 ticks.

**Is speed a real help?** Yes, and it is the top factor by permutation importance on later attempts, level with press to press (AUC drops of 0.140 and 0.138, against 0.093 for flip angle and 0.024 for hold). It does not need a separate interaction with timing (p 0.37): the two simply add up on the odds scale, which is enough to make speed look very forgiving in raw tables.

**Does the best flip angle depend on speed?** Yes (`tools/speed_angle.py`, likelihood ratio test p 0.0001, though see the limits below on p values). Speed mostly widens the range of angles that work, and 50 to 60 degrees holds up at every speed.

| Flip angle | Under 1300 | 1300 to 1500 | 1500 to 1700 | 1700 to 1900 | 1900 and up |
|---|---|---|---|---|---|
| 40 to 50 | 33% | 32% | 58% | 70% | 57% |
| 50 to 60 | 87% | 80% | 86% | 92% | 88% |
| 60 to 70 | 62% | 72% | 80% | 90% | 96% |
| 70 to 80 | 52% | 56% | 75% | 91% | 92% |
| 80 to 90 | 27% | 49% | 57% | 73% | 95% |

Some of these cells are small, with 7 to 18 dashes in the 40 to 50 row and the under 1300 column's outer cells. Speed gain depends mostly on angle, so a high angle at high speed is safe but gains little: 95% and a median of 11 uu/s at 80 to 90 degrees from 1900 uu/s. The flip angle bands stay fixed rather than moving with speed, because 50 to 70 degrees is a sound target throughout and a moving target would be harder to learn. Press to press already moves with speed.

**Did practice itself help?** Practice has no significant effect left after angle, speed and timing are counted. The improvement shows up in those measurable things.

**Going up or down the wall?** Slightly down has the highest raw make rate: 79% from 3 to 10 degrees downward, 73% level and 67% from 3 to 10 degrees upward. It is not significant in the full model (p 0.13). This does not test the turn before the press, which is not logged yet.

**Hold time**

| Hold ticks | Dashes | Make rate |
|---|---|---|
| 3 or less | 34 | 82% |
| 4 | 77 | 87% |
| 5 | 231 | 86% |
| 6 | 328 | 79% |
| 7 | 326 | 63% |
| 8 to 9 | 198 | 49% |
| 10 or more | 18 | 17% |

## The bands

These are now the plugin's defaults, and all of them can be changed in the settings window. They aim at making the dash, and are not the fastest a dash can be done. Stronger players will likely press much faster, so they may want tighter limits.

Press to press uses dashes with a flip angle from 50 to 80 degrees, with green at about 75% or better and yellow at about 50% or better.

| Speed at first press | Green up to | Yellow up to | Red from |
|---|---|---|---|
| Under 1300 | 12 ticks (100 ms) | 14 ticks (117 ms) | 15 ticks (125 ms) |
| 1300 to 1500 | 13 ticks (108 ms) | none | 14 ticks (117 ms) |
| 1500 to 1700 | 13 ticks (108 ms) | 14 ticks (117 ms) | 15 ticks (125 ms) |
| 1700 to 1900 | 14 ticks (117 ms) | 15 ticks (125 ms) | 16 ticks (133 ms) |
| 1900 and up | 15 ticks (125 ms) | 16 ticks (133 ms) | 17 ticks (142 ms) |

Below 7 ticks (58 ms) stays red as too early. The under 1300 row has few attempts, and its 14 tick figure (62% of 13) sits above its 13 tick figure (52% of 33) only through noise.

The brief's starting bands of green to 80 ms and red above 95 ms do not fit this data: only 8 of 1212 dashes were at 80 ms or less, and 703 of 858 makes were above 95 ms.

| Measure | Band |
|---|---|
| Flip angle | Green 50 to 70. Yellow 45 to 50 and 70 to 85. Too forward below 45, too sideways above 85 |
| Hold | Green up to 6 ticks (50 ms). Yellow 7 to 9 ticks (58 to 75 ms). Red from 10 ticks (83 ms), which also turns on the "held too long" miss reason |
| Release to press | No bands, as it predicts little |

The brief hoped for a press to press band that depends on flip angle. It is not worth adding: from 50 to 80 degrees the timing limits barely move, and outside that the angle itself is the problem.

## Do the bands match makes?

`tools/green_check.py` scores each dash by how many of press to press, hold and flip angle were green. These are the same dataset as above, plus the first 244 dashes played with the bands on screen.

| Greens | Dataset: dashes | Dataset: make rate | Played on the bands: dashes | Played on the bands: make rate |
|---|---|---|---|---|
| 0 | 124 | 16% | 8 | 25% |
| 1 | 339 | 49% | 36 | 53% |
| 2 | 513 | 86% | 113 | 90% |
| 3 | 241 | 97% | 87 | 94% |

Three greens nearly always means a make, but it is not required: most makes had fewer. Press to press matters most of the three. In the dataset, a green press with a non-green flip angle and hold still made 67%, while a non-green press with a green hold and flip angle made 76%, and with neither of the others only 16 to 36%. A non-green hold matters least when the other two are green (95%).

The player's view after playing on them is that the timing feels a little generous and could have more yellow. The bands stay as they are for now, to be reviewed after public feedback.

## Limits

- One player, one set of controller settings, and one evening of practice. A stronger or weaker player may have different limits, so the bands should be seen as a starting point and stay adjustable.
- This is observational. Fast starts, for example, often come from chaining, which could carry other differences. A causal claim would need deliberately controlled practice.
- Attempts are not independent, and they drift with practice, so the p values and intervals are optimistic.
- Only dashes that produced a dodge are here, so too-early presses are not part of these make rates.
- The later attempts have a higher base rate than the earlier ones (80% against 67%), which is why AUC and Brier are both reported.
- There is little data under 1100 uu/s, and none of this applies to curves, corners, air roll (17 dashes at a 6% make rate) or slowed-down play.
