"""Follow-up to analyze_chains.py: speed away from the wall at the first press, and unclassified misses.

Run from the project root. Reads data/attempts.csv.
"""
import warnings

import numpy as np
import pandas as pd
import statsmodels.formula.api as smf
from sklearn.ensemble import HistGradientBoostingClassifier as GB
from sklearn.metrics import roc_auc_score as auc

warnings.filterwarnings("ignore")
pd.set_option("display.width", 220)
pd.set_option("display.max_columns", 30)
pd.set_option("display.max_rows", 80)
pd.set_option("display.max_colwidth", 70)

a = pd.read_csv("data/attempts.csv").rename(columns={"session": "session_id"})
a = a[a.turn_size_prepress.notna()].sort_values(["session_id", "start_frame"]).copy()
d = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0) & (a.air_roll_fraction == 0)
      & (a.second_press_result == "dodge") & (a.press_to_press_ticks <= 21) & (a.flip_angle_deg > 0) & (a.flip_angle_deg <= 90)].copy()
d["make"] = (d.result == "make").astype(int)
d["gap"] = d.groupby("session_id").start_frame.diff()
print("dashes", len(d))

print("\n== make rate by speed away from the wall at the press")
d["N"] = pd.cut(d.normal_speed_at_press, [-100, 5, 10, 13, 16, 19, 22, 40])
print(d.groupby("N", observed=True).agg(n=("make", "size"), make=("make", "mean"), speed=("speed_first_press", "median"), ticks=("press_to_press_ticks", "median"), angle=("flip_angle_deg", "median"), dist=("max_wall_distance", "median"), touch=("first_touch_ticks", "median")).round(2))
print("\nwithin one start speed range and one timing range")
fmt = lambda v: f"{v.mean() * 100:.0f}%/{len(v)}"
s = d[(d.flip_angle_deg >= 50) & (d.flip_angle_deg <= 80)]
print(pd.crosstab(pd.cut(s.speed_first_press, [0, 1500, 1700, 1900, 2100, 2500]), pd.cut(s.normal_speed_at_press, [-100, 10, 14, 18, 22, 40]), values=s.make, aggfunc=fmt).fillna(""))
print(pd.crosstab(pd.cut(s.press_to_press_ticks, [0, 12, 13, 14, 15, 21]), pd.cut(s.normal_speed_at_press, [-100, 10, 14, 18, 22, 40]), values=s.make, aggfunc=fmt).fillna(""))

print("\n== what goes with it (Spearman)")
cols = ["normal_speed_at_press", "speed_first_press", "turn_size_prepress", "travel_angle_prepress", "travel_angle_prepress_min", "height_drop_prepress", "speed_change_prepress", "height_at_start", "gap", "chain_index"]
print(d[cols].corr(method="spearman").round(2).loc["normal_speed_at_press"].to_dict())
m = smf.ols("normal_speed_at_press~speed_first_press+travel_angle_prepress+travel_angle_prepress_min+turn_size_prepress+height_at_start+C(wall_side)+C(surface)", d).fit(cov_type="HC3")
print(pd.DataFrame({"coef": m.params, "p": m.pvalues}).round(4), "R2", round(m.rsquared, 3))
print("by surface and wall", d.groupby(["surface", "wall_side"]).normal_speed_at_press.median().round(1).to_dict())

print("\n== does it help predict later attempts?")
d = d.reset_index(drop=True)
split = int(0.7 * len(d))
tr, te = d.iloc[:split], d.iloc[split:]
for name, c in [("speed+timing+angle", ["press_to_press_ticks", "flip_angle_deg", "speed_first_press"]),
                ("+hold", ["press_to_press_ticks", "flip_angle_deg", "speed_first_press", "hold_ticks"]),
                ("+hold+away speed", ["press_to_press_ticks", "flip_angle_deg", "speed_first_press", "hold_ticks", "normal_speed_at_press"]),
                ("+hold+turn+drop", ["press_to_press_ticks", "flip_angle_deg", "speed_first_press", "hold_ticks", "turn_size_prepress", "height_drop_prepress"])]:
    g = GB(max_depth=3, learning_rate=0.05, max_iter=200, random_state=0).fit(tr[c], tr.make)
    print(f"{name:24s} AUC {auc(te.make, g.predict_proba(te[c])[:, 1]):.3f}")

print("\n== logistic regression, all new dashes, with and without away speed")
h = d.assign(ticks=d.press_to_press_ticks, hold=d.hold_ticks, a10=(d.flip_angle_deg - 60) / 10, a2=((d.flip_angle_deg - 60) / 10) ** 2, s100=d.speed_first_press / 100, n10=d.normal_speed_at_press / 10, turn5=d.turn_size_prepress / 5)
for formula in ["make~ticks+hold+a10+a2+s100+turn5", "make~ticks+hold+a10+a2+s100+turn5+n10"]:
    r = smf.logit(formula, h).fit(disp=0)
    print(pd.DataFrame({"odds": np.exp(r.params), "lo": np.exp(r.conf_int()[0]), "hi": np.exp(r.conf_int()[1]), "p": r.pvalues}).round(3).drop("Intercept"), "pseudo R2", round(r.prsquared, 3))

print("\n== moving into the wall at the press, and time since the last landing")
w = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0) & (a.air_roll_fraction == 0) & (a.second_press_result == "dodge")].copy()
w["make"] = (w.result == "make").astype(int)
prev = a.groupby("session_id")
a["landed_frame"] = a.start_frame + a.reattach_ticks
w["since_landing"] = w.start_frame - a.groupby("session_id").landed_frame.shift().reindex(w.index)
print("into the wall (below 0):", int((w.normal_speed_at_press < 0).sum()), "of", len(w), "| make rate", round(w[w.normal_speed_at_press < 0].make.mean(), 2), "against", round(w[w.normal_speed_at_press >= 0].make.mean(), 2))
print(w.groupby(w.normal_speed_at_press < 0).agg(n=("make", "size"), make=("make", "mean"), ticks=("press_to_press_ticks", "median"), angle=("flip_angle_deg", "median"), since=("since_landing", "median"), speed=("speed_first_press", "median"), never=("first_touch_ticks", lambda s: s.isna().mean())).round(2))
print("by ticks since the last landing, follow-ups")
b = w[w.since_landing.notna() & (w.since_landing >= 0) & (w.chain_index >= 2)]
print(b.groupby(pd.cut(b.since_landing, [0, 10, 20, 30, 40, 50, 60, 80, 130]), observed=True).agg(n=("make", "size"), make=("make", "mean"), away=("normal_speed_at_press", "median"), into=("normal_speed_at_press", lambda s: (s < 0).mean()), lost=("speed_lost_since_last", "median")).round(2))
print("into the wall, by wheels at the press and the lowest wheels before", w[w.normal_speed_at_press < 0].wheels_min_prepress.value_counts().to_dict())

print("\n== misses the plugin could not explain")
u = a[a.diagnosis == "unclassified"]
print(len(u), "of", int((a.result == "miss").sum()), "misses; surfaces", u.surface.value_counts().to_dict())
print(u[["press_to_press_ms", "hold_ms", "flip_angle_deg", "speed_first_press", "first_touch_ticks", "reattach_ticks", "max_wheels", "car_up_dot_normal", "max_wall_distance", "normal_speed_at_press"]].describe().round(1).loc[["count", "min", "50%", "max"]])
print("never touched again", int(u.first_touch_ticks.isna().sum()), "| reached 3 wheels", int(u.reattach_ticks.notna().sum()), "| max wheels", u.max_wheels.value_counts().to_dict())
print(u[["press_to_press_ms", "flip_angle_deg", "speed_first_press", "first_touch_ticks", "reattach_ticks", "max_wheels", "car_up_dot_normal", "diagnosis_text"]].head(10).round(2).to_string())
