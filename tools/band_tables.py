"""Leak check on the public dataset, plus the extra tables for the proposed bands."""
import csv
import re

import numpy as np
import pandas as pd

pd.set_option("display.width", 200)
pd.set_option("display.max_columns", 30)

text = open("data/attempts.csv").read()
print("date-like strings:", len(re.findall(r"20\d\d-\d\d-\d\d|20\d{6}_\d{6}|\d\d:\d\d:\d\d", text)))
print("email or path strings:", len(re.findall(r"@|\\Users|gmail", text)))
hits = {}
rows = list(csv.reader(open("data/attempts.csv")))
header = rows[0]
for row in rows[1:]:
    for name, value in zip(header, row):
        if "2026" in value:
            hits[name] = hits.get(name, 0) + 1
print("columns where 2026 appears:", hits)
print("non-numeric columns:", [c for c in pd.read_csv("data/attempts.csv", nrows=50).select_dtypes(exclude="number").columns])

a = pd.read_csv("data/attempts.csv")
a = a[a.attempt_order <= 1484]  # The Phase 3 sample. Later attempts are in analyze_chains.py.
f = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0) & (a.air_roll_fraction == 0)]
d = f[(f.second_press_result == "dodge") & (f.press_to_press_ticks <= 21) & (f.flip_angle_deg > 0) & (f.flip_angle_deg <= 90)].copy()
d["make"] = (d.result == "make").astype(int)
r = lambda v: f"{v.mean() * 100:.0f}%/{len(v)}"

print("totals", len(a), "flat 100% by wall", f.groupby("wall_side").size().to_dict(), "real dashes", len(d))
d["H"] = pd.cut(d.hold_ticks, [0, 3, 4, 5, 6, 7, 9, 60], labels=["<=3", "4", "5", "6", "7", "8-9", "10+"])
d["T"] = pd.cut(d.press_to_press_ticks, [0, 11, 12, 13, 14, 21], labels=["<=11", "12", "13", "14", "15+"])
print("hold alone")
print(d.groupby("H", observed=True).make.agg(["size", "mean"]).round(2))
print("press to press x hold")
print(pd.crosstab(d["T"], d.H, values=d.make, aggfunc=r).fillna(""))

d["fine"] = pd.cut(d.flip_angle_deg, [40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90])
print("flip angle in 5 degree steps")
print(d.groupby("fine", observed=True).agg(n=("make", "size"), rate=("make", "mean"), gain=("speed_gain", "median")).round(2))

d["S"] = pd.cut(d.speed_first_press, [0, 1300, 1500, 1700, 1900, 2500], labels=["<1300", "1300-1500", "1500-1700", "1700-1900", "1900+"])
good = d[(d.flip_angle_deg >= 50) & (d.flip_angle_deg <= 80)]
good = good.assign(tk=good.press_to_press_ticks.clip(upper=17))
print("press to press x speed, angle 50 to 80 only")
print(pd.crosstab(good.tk, good.S, values=good.make, aggfunc=r).fillna(""))

print("current brief bands on real dashes: at 80 ms or less", int((d.press_to_press_ms <= 80).sum()), "of", len(d), "; makes above 95 ms", int(((d.press_to_press_ms > 95) & (d.make == 1)).sum()), "of", int(d.make.sum()))
print("lifetime supersonic ends", int((a.total_speed_end >= 2200).sum()))
print("sessions", a.session.nunique(), "attempts per session", a.groupby("session").size().to_dict())
