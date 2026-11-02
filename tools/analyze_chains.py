"""Phase 4 analysis of chain dashes, plus a test of the Phase 3 model on attempts it never saw.

Run from the project root. Reads data/attempts.csv.
"""
import warnings

import numpy as np
import pandas as pd
import statsmodels.formula.api as smf
from sklearn.ensemble import HistGradientBoostingClassifier as GB
from sklearn.linear_model import LogisticRegression as LR
from sklearn.metrics import brier_score_loss as brier
from sklearn.metrics import roc_auc_score as auc
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler as SS

warnings.filterwarnings("ignore")
pd.set_option("display.width", 220)
pd.set_option("display.max_columns", 30)
pd.set_option("display.max_rows", 120)

a = pd.read_csv("data/attempts.csv").rename(columns={"session": "session_id"})
print("attempts", len(a), "with movement before the press", int(a.turn_size_prepress.notna().sum()))


def real(x):
    return x[(x.game_speed >= 0.95) & x.surface.isin(["side_wall", "back_wall"]) & (x.spam_input == 0) & (x.air_roll_fraction == 0)
             & (x.second_press_result == "dodge") & (x.press_to_press_ticks <= 21) & (x.flip_angle_deg > 0) & (x.flip_angle_deg <= 90) & (x.flip_angle_undefined == 0)].copy()


new = a[a.turn_size_prepress.notna()].copy()
qualifying = new[(new.game_speed >= 0.95) & new.surface.isin(["side_wall", "back_wall"]) & (new.spam_input == 0) & (new.air_roll_fraction == 0) & (new.second_press_result == "dodge") & (new.chain_index >= 2)]
print("\ncounter's number:", qualifying.groupby("wall_side").size().to_dict())
print("new attempts", len(new), "game speeds", new.game_speed.round(2).value_counts().head(4).to_dict(), "surfaces", new.surface.value_counts().to_dict())
print("diagnoses", new.diagnosis.value_counts().to_dict())
print("air roll attempts", int((new.air_roll_fraction > 0).sum()), "spam", int(new.spam_input.sum()), "not fully on wall", int(new.not_fully_on_wall.sum()))

d = real(new)
d["make"] = (d.result == "make").astype(int)
d = d.sort_values(["session_id", "start_frame"])
d["gap"] = d.groupby("session_id").start_frame.diff()
d["prev_peak"] = d.groupby("session_id").speed_peak.shift()
d["prev_make"] = d.groupby("session_id").make.shift()
f = d[d.chain_index >= 2].copy()
print("\nreal dashes", len(d), "follow-ups", len(f), "make rate all", round(d.make.mean(), 3), "follow-ups", round(f.make.mean(), 3))
print(f.groupby("wall_side").make.agg(["size", "mean"]).round(3))

print("\n== spread: did turn size vary?")
print(f[["turn_size_prepress", "travel_angle_prepress", "travel_angle_prepress_min", "height_drop_prepress", "speed_change_prepress", "normal_speed_at_press", "speed_lost_since_last", "gap", "speed_first_press"]].describe().round(1).loc[["count", "min", "25%", "50%", "75%", "max"]])
print("wheels_min_prepress", f.wheels_min_prepress.value_counts().to_dict())

r = lambda v: f"{v.mean() * 100:.0f}%/{len(v)}"
f["turn"] = pd.cut(f.turn_size_prepress, [0, 5, 8, 11, 15, 20, 30, 200])
f["drop"] = pd.cut(f.height_drop_prepress, [-1, 0.5, 5, 15, 30, 60, 1000])
f["tmin"] = pd.cut(f.travel_angle_prepress_min, [-90, -20, -12, -8, -4, 0, 90])
f["S"] = pd.cut(f.speed_first_press, [0, 1500, 1700, 1900, 2100, 2500])

print("\n== by turn size")
print(f.groupby("turn", observed=True).agg(n=("make", "size"), make=("make", "mean"), lost=("speed_lost_since_last", "median"), start=("speed_first_press", "median"), gain=("speed_gain", "median"), gap=("gap", "median")).round(2))
print("\n== by height dropped in the window")
print(f.groupby("drop", observed=True).agg(n=("make", "size"), make=("make", "mean"), lost=("speed_lost_since_last", "median"), start=("speed_first_press", "median"), gap=("gap", "median")).round(2))
print("\n== by lowest travel angle in the window")
print(f.groupby("tmin", observed=True).agg(n=("make", "size"), make=("make", "mean"), lost=("speed_lost_since_last", "median"), start=("speed_first_press", "median")).round(2))

print("\n== what drives speed lost between dashes (ordinary least squares, uu/s)")
g = f.dropna(subset=["speed_lost_since_last", "prev_peak", "gap"]).copy()
g = g[g.gap < 300]
ols = smf.ols("speed_lost_since_last~turn_size_prepress+height_drop_prepress+gap+prev_peak+travel_angle_prepress+C(wall_side)+C(surface)", g).fit(cov_type="HC3")
print(pd.DataFrame({"coef": ols.params, "lo": ols.conf_int()[0], "hi": ols.conf_int()[1], "p": ols.pvalues}).round(3))
print("n", int(ols.nobs), "R2", round(ols.rsquared, 3))
below = g[g.prev_peak < 2250]
ols2 = smf.ols("speed_lost_since_last~turn_size_prepress+height_drop_prepress+gap+prev_peak+travel_angle_prepress+C(wall_side)+C(surface)", below).fit(cov_type="HC3")
print("away from the speed cap (previous peak under 2250):")
print(pd.DataFrame({"coef": ols2.params, "p": ols2.pvalues}).round(3).loc[["turn_size_prepress", "height_drop_prepress", "gap", "prev_peak"]], "n", int(ols2.nobs), "R2", round(ols2.rsquared, 3))
print("by gap, previous peak under 2250")
print(below.groupby(pd.cut(below.gap, [0, 70, 80, 90, 100, 120, 150, 300]), observed=True).speed_lost_since_last.agg(["size", "median"]).round(0))
print("by turn, previous peak under 2250")
print(below.groupby(pd.cut(below.turn_size_prepress, [0, 5, 8, 11, 15, 20, 200]), observed=True).speed_lost_since_last.agg(["size", "median"]).round(0))

print("\n== does the turn affect the next dash making? (logistic regression on follow-ups)")
h = f.assign(ticks=f.press_to_press_ticks, hold=f.hold_ticks, a10=(f.flip_angle_deg - 60) / 10, a2=((f.flip_angle_deg - 60) / 10) ** 2, s100=f.speed_first_press / 100,
             turn5=f.turn_size_prepress / 5, drop10=f.height_drop_prepress / 10, normal10=f.normal_speed_at_press / 10, right=(f.wall_side == "right").astype(int), back=(f.surface == "back_wall").astype(int))
m = smf.logit("make~ticks+hold+a10+a2+s100+turn5+drop10+normal10+right+back", h).fit(disp=0)
print(pd.DataFrame({"odds": np.exp(m.params), "lo": np.exp(m.conf_int()[0]), "hi": np.exp(m.conf_int()[1]), "p": m.pvalues}).round(3))
print("pseudo R2", round(m.prsquared, 3))
print("max distance from wall on makes vs turn and normal speed")
print(h[h.make == 1][["max_wall_distance", "turn_size_prepress", "normal_speed_at_press", "height_drop_prepress", "speed_gain"]].corr(method="spearman").round(2).loc["max_wall_distance"].to_dict())

print("\n== all wheels off before the press?")
print(f.groupby(f.wheels_min_prepress.clip(upper=4)).agg(n=("make", "size"), make=("make", "mean"), lost=("speed_lost_since_last", "median")).round(2))
print("wheels stayed down at the press itself, from wheel contacts at start:", new.wheel_contacts_at_start.value_counts().to_dict())

print("\n== reaching supersonic")
s = d.groupby(["session_id", "chain_id"]).agg(dashes=("make", "size"), makes=("make", "sum"), first_speed=("speed_first_press", "first"), top=("max_total_speed", "max"), reached=("dashes_to_supersonic", "max"))
print("chains", len(s), "reached 2200 in", int(s.reached.notna().sum()))
print("makes per chain", s.makes.describe().round(1).to_dict())
print("dash number that reached it, by first speed in the chain")
t = s[s.reached.notna()]
print(t.groupby(pd.cut(t.first_speed, [0, 1300, 1500, 1700, 1900, 2100, 2500]), observed=True).reached.agg(["size", "median", "min", "max"]))
print("per dash net: speed gain on makes and speed lost before, median, by flip angle, chain dashes starting under 2000")
n = f[(f.make == 1) & (f.speed_first_press < 2000)]
n = n.assign(A=pd.cut(n.flip_angle_deg, [40, 50, 60, 70, 80, 90]))
print(n.groupby("A", observed=True).agg(n=("make", "size"), gain=("speed_gain", "median"), lost=("speed_lost_since_last", "median"), net=("speed_gain", lambda v: (v - n.loc[v.index, "speed_lost_since_last"]).median())).round(0))
print("make rate by chain position")
print(d.groupby(d.chain_index.clip(upper=8)).make.agg(["size", "mean"]).round(2))
print("wall comparison on follow-ups")
print(f.groupby("wall_side").agg(n=("make", "size"), make=("make", "mean"), lost=("speed_lost_since_last", "median"), turn=("turn_size_prepress", "median"), angle=("flip_angle_deg", "median"), ticks=("press_to_press_ticks", "median"), speed=("speed_first_press", "median")).round(2))

print("\n\n=== Phase 3 model tested on attempts it never saw ===")
snapshot_end = 1484
a["order"] = a.attempt_order
old = real(a[a.order <= snapshot_end])
fresh = real(a[a.order > snapshot_end])
old["make"] = (old.result == "make").astype(int)
fresh["make"] = (fresh.result == "make").astype(int)
print("old dashes", len(old), "rate", round(old.make.mean(), 3), "| new dashes", len(fresh), "rate", round(fresh.make.mean(), 3))
cols = ["press_to_press_ticks", "flip_angle_deg", "speed_first_press"]
for name, model in [("GB speed+timing+angle", GB(max_depth=3, learning_rate=0.05, max_iter=200, random_state=0)), ("LR speed+timing+angle", make_pipeline(SS(), LR(max_iter=2000)))]:
    model.fit(old[cols], old.make)
    p = model.predict_proba(fresh[cols])[:, 1]
    base = np.full(len(fresh), old.make.mean())
    print(f"{name}: AUC {auc(fresh.make, p):.3f}, Brier {brier(fresh.make, p):.3f}, always predicting the old rate {brier(fresh.make, base):.3f}, mean predicted {p.mean():.3f} vs actual {fresh.make.mean():.3f}")
for name, part in [("first dash in a chain", fresh[fresh.chain_index == 1]), ("follow-up", fresh[fresh.chain_index >= 2])]:
    gb = GB(max_depth=3, learning_rate=0.05, max_iter=200, random_state=0).fit(old[cols], old.make)
    p = gb.predict_proba(part[cols])[:, 1]
    print(f"  {name}: n {len(part)}, AUC {auc(part.make, p):.3f}, predicted {p.mean():.3f}, actual {part.make.mean():.3f}")

GREEN = np.array([100, 108, 108, 117, 125])
YELLOW = np.array([117, 108, 117, 125, 133])
EDGES = [1300, 1500, 1700, 1900]
for name, x in [("old", old), ("new", fresh)]:
    i = np.digitize(x.speed_first_press, EDGES)
    x["press_band"] = np.where(x.press_to_press_ms <= GREEN[i] + 0.5, "green", np.where(x.press_to_press_ms <= YELLOW[i] + 0.5, "yellow", "red"))
    x["angle_band"] = np.where((x.flip_angle_deg >= 50) & (x.flip_angle_deg <= 70), "green", np.where((x.flip_angle_deg >= 45) & (x.flip_angle_deg <= 85), "yellow", "red"))
    x["hold_band"] = np.where(x.hold_ms <= 50.5, "green", np.where(x.hold_ms <= 75.5, "yellow", "red"))
    print(f"\n{name} data, make rate by band")
    for band in ["press_band", "angle_band", "hold_band"]:
        print(band, x.groupby(band).make.agg(n="size", rate="mean").round(3).to_dict("index"))
    x["greens"] = (x.press_band == "green").astype(int) + (x.angle_band == "green").astype(int) + (x.hold_band == "green").astype(int)
    print("greens", x.groupby("greens").make.agg(n="size", rate="mean").round(3).to_dict("index"))

print("\nnew data, press to press by speed, flip angle 50 to 80")
q = fresh[(fresh.flip_angle_deg >= 50) & (fresh.flip_angle_deg <= 80)]
q = q.assign(S=pd.cut(q.speed_first_press, [0, 1300, 1500, 1700, 1900, 2500], labels=["<1300", "1300-1500", "1500-1700", "1700-1900", "1900+"]), tk=q.press_to_press_ticks.clip(upper=18))
print(pd.crosstab(q.tk, q.S, values=q.make, aggfunc=r).fillna(""))
print("\nnew data, flip angle in 5 degree steps")
print(fresh.groupby(pd.cut(fresh.flip_angle_deg, [40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90]), observed=True).agg(n=("make", "size"), rate=("make", "mean"), gain=("speed_gain", "median")).round(2))
print("\nall data together, regression")
al = pd.concat([old, fresh])
al = al.assign(ticks=al.press_to_press_ticks, hold=al.hold_ticks, a10=(al.flip_angle_deg - 60) / 10, a2=((al.flip_angle_deg - 60) / 10) ** 2, s100=al.speed_first_press / 100, right=(al.wall_side == "right").astype(int), back=(al.surface == "back_wall").astype(int), follow=(al.chain_index.fillna(1) >= 2).astype(int), new=(al.order > snapshot_end).astype(int))
m3 = smf.logit("make~ticks+hold+a10+a2+s100+right+back+new", al).fit(disp=0)
print(pd.DataFrame({"odds": np.exp(m3.params), "lo": np.exp(m3.conf_int()[0]), "hi": np.exp(m3.conf_int()[1]), "p": m3.pvalues}).round(3))
print("best angle", round(60 - 10 * m3.params["a10"] / (2 * m3.params["a2"]), 1), "pseudo R2", round(m3.prsquared, 3), "dashes", len(al))
