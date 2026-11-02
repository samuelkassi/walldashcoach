"""Make rate by how many of press to press, hold and flip angle were green, using the default bands.

Pass attempt CSV paths. With no paths it reads data/attempts.csv.
"""
import sys

import numpy as np
import pandas as pd

GREEN_MS = [100, 108, 108, 117, 125]
SPEED_EDGES = [1300, 1500, 1700, 1900]
SLACK = 0.5

paths = sys.argv[1:] or ["data/attempts.csv"]
a = pd.concat([pd.read_csv(p) for p in paths], ignore_index=True)
d = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0) & (a.air_roll_fraction == 0) & (a.second_press_result == "dodge")].copy()
d["make"] = d.result == "make"

limit = np.array(GREEN_MS)[np.digitize(d.speed_first_press, SPEED_EDGES)]
d["press_green"] = (d.press_to_press_ms >= 58) & (d.press_to_press_ms <= limit + SLACK)
d["hold_green"] = d.hold_ms <= 50 + SLACK
d["angle_green"] = (d.flip_angle_deg >= 50) & (d.flip_angle_deg <= 70)
d["greens"] = d.press_green.astype(int) + d.hold_green.astype(int) + d.angle_green.astype(int)

print(len(d), "dashes, make rate", round(d.make.mean(), 3))
print(d.groupby("greens").make.agg(dashes="size", makes="sum", rate="mean").round(3))
print("misses with 3 greens:", int(((d.greens == 3) & ~d.make).sum()))
print("makes with fewer than 3 greens:", int(((d.greens < 3) & d.make).sum()), "of", int(d.make.sum()))
print(d.groupby(["press_green", "hold_green", "angle_green"]).make.agg(dashes="size", rate="mean").round(2))
print("median press to press ms, makes then misses:", d[d.make].press_to_press_ms.median(), d[~d.make].press_to_press_ms.median())
