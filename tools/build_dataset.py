"""Builds data/attempts.csv from the raw attempt files: removes timestamps, keeps attempt order."""
import glob
import os

import numpy as np
import pandas as pd

RAW = os.path.join(os.environ["APPDATA"], "bakkesmod", "bakkesmod", "data", "walldashtrainer", "attempts_*.csv")

frames = [pd.read_csv(f) for f in sorted(glob.glob(RAW))]
d = pd.concat(frames, ignore_index=True)
d["_t"] = pd.to_datetime(d.timestamp)
d = d.sort_values(["_t", "session_id", "attempt_id"], kind="stable").reset_index(drop=True)

sessions = {s: i + 1 for i, s in enumerate(pd.unique(d.session_id))}
d["session"] = d.session_id.map(sessions)
d.insert(0, "attempt_order", np.arange(1, len(d) + 1))


def at(path, i):
    if not isinstance(path, str) or i != i or i < 0:
        return (np.nan, np.nan)
    pts = path.split(";")
    if int(i) >= len(pts):
        return (np.nan, np.nan)
    p, y = pts[int(i)].split(":")
    return float(p), float(y)


# The stick columns only exist in later files, and stick_path lets every attempt have them.
side = np.where(d.wall_side == "right", 1.0, -1.0)
for name, col in [("first_press", pd.Series(0, index=d.index)), ("release", d.hold_ticks), ("second_press", d.press_to_press_ticks)]:
    v = [at(p, i) for p, i in zip(d.stick_path, col)]
    p = np.array([a for a, _ in v])
    y = np.array([b for _, b in v])
    s = np.abs(p) + np.abs(y)
    a = np.degrees(np.arctan2(y * side, -p))
    a[~(s > 0)] = np.nan
    d[f"stick_angle_{name}"] = np.round(a, 1)
    d[f"stick_sum_{name}"] = np.round(s, 3)

d = d.drop(columns=["timestamp", "session_id", "attempt_id", "_t", "chain_id", "chain_index"])
cols = ["attempt_order", "session"] + [c for c in d.columns if c not in ("attempt_order", "session")]
d[cols].to_csv("data/attempts.csv", index=False)

print(len(d), "attempts,", len(sessions), "sessions,", d[cols].shape[1], "columns,", os.path.getsize("data/attempts.csv") // 1024, "KB")
print("leak check, time or date columns:", [c for c in cols if "time" in c.lower() or "date" in c.lower()])
print("any 2026 in file:", "2026" in open("data/attempts.csv").read())
