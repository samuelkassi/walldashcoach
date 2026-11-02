"""Phase 3 analysis of data/attempts.csv: what drives wall dash makes."""
import warnings

import numpy as np
import pandas as pd
import statsmodels.formula.api as smf
from sklearn.ensemble import HistGradientBoostingClassifier as GB
from sklearn.inspection import permutation_importance as PI
from sklearn.linear_model import LogisticRegression as LR
from sklearn.metrics import brier_score_loss as brier
from sklearn.metrics import roc_auc_score as auc
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler as SS

warnings.filterwarnings("ignore")
pd.set_option("display.width", 220)
pd.set_option("display.max_columns", 30)
pd.set_option("display.max_rows", 100)

a = pd.read_csv("data/attempts.csv")
a = a[a.attempt_order <= 1484]  # The Phase 3 sample. Later attempts are in analyze_chains.py.
f = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0)]
print("flat 100%", len(f), f.groupby("wall_side").size().to_dict())
n = f[f.air_roll_fraction == 0]
print("non-dodge second press results, no air roll:", n[n.second_press_result != "dodge"].groupby("second_press_result").size().to_dict())

d = n[(n.second_press_result == "dodge") & (n.press_to_press_ticks <= 21) & (n.flip_angle_deg > 0) & (n.flip_angle_deg <= 90) & (n.flip_angle_undefined == 0)].copy()
d["make"] = (d.result == "make").astype(int)
d = d.sort_values("attempt_order")
d["gap"] = d.groupby("session").start_frame.diff()
d["followup"] = (d.gap < 150).astype(int)
d["right"] = (d.wall_side == "right").astype(int)
d["back"] = (d.surface == "back_wall").astype(int)
d["order"] = d.attempt_order / d.attempt_order.max()
d["set_first"] = ((d.stick_sum_first_press >= d.dodge_deadzone) & ((d.stick_angle_first_press - d.flip_angle_deg).abs() <= 20)).astype(int)
d["set_release"] = ((d.stick_sum_release >= d.dodge_deadzone) & ((d.stick_angle_release - d.flip_angle_deg).abs() <= 20)).astype(int)
d["drift_release"] = (d.stick_angle_release - d.flip_angle_deg).abs().clip(upper=180).fillna(180)
print("real dashes", len(d), "makes", d.make.sum(), "rate", round(d.make.mean(), 3), d.groupby("wall_side").make.agg(["size", "mean"]).round(3).to_dict("index"))

print("\n== single feature AUC")
for c in ["press_to_press_ticks", "hold_ticks", "release_to_press_ticks", "flip_angle_deg", "speed_first_press", "travel_angle_deg", "height_at_start", "drift_release", "order"]:
    x = d[[c, "make"]].dropna()
    v = auc(x.make, x[c])
    print(f"{c:28s} n={len(x):5d} AUC={max(v, 1 - v):.3f} {'higher better' if v > 0.5 else 'lower better'}")
print("by wall:")
for w, g in d.groupby("wall_side"):
    print(" ", w, {c: round(max(auc(g.make, g[c]), 1 - auc(g.make, g[c])), 3) for c in ["press_to_press_ticks", "hold_ticks", "release_to_press_ticks", "flip_angle_deg", "speed_first_press"]})

FEATURES = ["press_to_press_ticks", "hold_ticks", "flip_angle_deg", "speed_first_press", "travel_angle_deg", "right", "back", "followup", "set_first", "set_release"]
split = int(0.7 * len(d))
tr = d.iloc[:split]
te = d.iloc[split:]
print("\n== train on first 70%, test on last 30%", len(tr), len(te), "base rate", round(tr.make.mean(), 3), round(te.make.mean(), 3))


def ev(name, m, cols):
    m.fit(tr[cols], tr.make)
    p = m.predict_proba(te[cols])[:, 1]
    print(f"{name:34s} AUC {auc(te.make, p):.3f} Brier {brier(te.make, p):.3f}")
    return m


print(f"{'always predict training rate':34s} AUC 0.500 Brier {brier(te.make, np.full(len(te), tr.make.mean())):.3f}")
for name, cols in [
    ("timing only", ["press_to_press_ticks"]),
    ("angle only", ["flip_angle_deg"]),
    ("speed only", ["speed_first_press"]),
    ("timing+angle", ["press_to_press_ticks", "flip_angle_deg"]),
    ("timing+angle+speed", ["press_to_press_ticks", "flip_angle_deg", "speed_first_press"]),
    ("all features", FEATURES),
]:
    ev("LR " + name, make_pipeline(SS(), LR(max_iter=2000)), cols)
gb = ev("GB timing+angle+speed", GB(max_depth=3, learning_rate=0.05, max_iter=200, random_state=0), ["press_to_press_ticks", "flip_angle_deg", "speed_first_press"])
gb = ev("GB all features", GB(max_depth=3, learning_rate=0.05, max_iter=200, random_state=0), FEATURES)
pi = PI(gb, te[FEATURES], te.make, scoring="roc_auc", n_repeats=30, random_state=0)
print("permutation importance on later attempts (drop in AUC):")
print(pd.Series(pi.importances_mean, FEATURES).sort_values(ascending=False).round(3).to_string())

print("\n== logistic regression on all dashes")
d2 = d.assign(ticks=d.press_to_press_ticks, hold=d.hold_ticks, a10=(d.flip_angle_deg - 60) / 10, a2=((d.flip_angle_deg - 60) / 10) ** 2, s100=d.speed_first_press / 100, tr5=d.travel_angle_deg / 5)
m = smf.logit("make~ticks+hold+a10+a2+s100+tr5+right+back+followup+set_first+set_release+order", d2).fit(disp=0)
print(pd.DataFrame({"odds": np.exp(m.params), "lo": np.exp(m.conf_int()[0]), "hi": np.exp(m.conf_int()[1]), "p": m.pvalues}).round(3))
print("pseudo R2", round(m.prsquared, 3))
m2 = smf.logit("make~ticks*s100+a10*right+a2+hold+tr5+back+followup+order", d2).fit(disp=0)
print("interactions")
print(pd.DataFrame({"odds": np.exp(m2.params), "p": m2.pvalues}).round(3).loc[["ticks:s100", "a10:right"]])

print("\n== rate grids, percent/attempts")
d["A"] = pd.cut(d.flip_angle_deg, [0, 40, 50, 60, 70, 80, 90])
d["T"] = pd.cut(d.press_to_press_ticks, [0, 10, 11, 12, 13, 14, 15, 21], labels=["<=10", "11", "12", "13", "14", "15", "16+"])
d["S"] = pd.cut(d.speed_first_press, [0, 1300, 1500, 1700, 1900, 2500], labels=["<1300", "1300-1500", "1500-1700", "1700-1900", "1900+"])
r = lambda v: f"{v.mean() * 100:.0f}%/{len(v)}"
print("ticks x angle")
print(pd.crosstab(d["T"], d.A, values=d.make, aggfunc=r).fillna(""))
print("ticks x speed")
print(pd.crosstab(d["T"], d.S, values=d.make, aggfunc=r).fillna(""))
print("angle x speed")
print(pd.crosstab(d.A, d.S, values=d.make, aggfunc=r).fillna(""))
print("angle x wall")
print(pd.crosstab(d.A, d.wall_side, values=d.make, aggfunc=r))
print("ticks x wall")
print(pd.crosstab(d["T"], d.wall_side, values=d.make, aggfunc=r))
print("median speed gain on makes, angle x speed")
print(d[d.make == 1].pivot_table(index="A", columns="S", values="speed_gain", aggfunc="median", observed=True).round(0))
print("stick set patterns")
d["pat"] = d.set_first.map({1: "F", 0: "-"}) + d.set_release.map({1: "R", 0: "-"}) + "S"
print(d.groupby("pat").make.agg(["size", "mean"]).round(2))
print("travel")
print(d.groupby(pd.cut(d.travel_angle_deg, [-90, -10, -3, 3, 10, 90]), observed=True).make.agg(["size", "mean"]).round(2))
print("first vs last 200 per wall")
for w, g in d.groupby("wall_side"):
    h, t = g.head(200), g.tail(200)
    print(w, "rate", round(h.make.mean(), 2), "->", round(t.make.mean(), 2), "angle", h.flip_angle_deg.median(), "->", t.flip_angle_deg.median(), "speed", h.speed_first_press.median(), "->", t.speed_first_press.median(), "ticks", h.press_to_press_ticks.median(), "->", t.press_to_press_ticks.median())
ar = f[(f.air_roll_fraction > 0) & (f.second_press_result == "dodge") & (f.flip_angle_deg > 0) & (f.press_to_press_ticks <= 21)]
print("\nair roll dashes", len(ar), "rate", round((ar.result == "make").mean(), 2))
