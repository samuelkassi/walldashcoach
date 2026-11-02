"""Does the best flip angle depend on speed at the first press?"""
import numpy as np
import pandas as pd
import statsmodels.formula.api as smf

pd.set_option("display.width", 200)
pd.set_option("display.max_columns", 30)

a = pd.read_csv("data/attempts.csv")
a = a[a.attempt_order <= 1484]  # The Phase 3 sample. Later attempts are in analyze_chains.py.
d = a[(a.game_speed >= 0.95) & a.surface.isin(["side_wall", "back_wall"]) & (a.spam_input == 0) & (a.air_roll_fraction == 0)
      & (a.second_press_result == "dodge") & (a.press_to_press_ticks <= 21) & (a.flip_angle_deg > 0) & (a.flip_angle_deg <= 90) & (a.flip_angle_undefined == 0)].copy()
d["make"] = (d.result == "make").astype(int)
d["ticks"] = d.press_to_press_ticks
d["hold"] = d.hold_ticks
d["a10"] = (d.flip_angle_deg - 60) / 10
d["a2"] = d.a10 ** 2
d["s100"] = (d.speed_first_press - 1500) / 100
d["back"] = (d.surface == "back_wall").astype(int)

base = smf.logit("make~ticks+hold+a10+a2+s100+back", d).fit(disp=0)
full = smf.logit("make~ticks+hold+a10+a2+s100+back+a10:s100+a2:s100", d).fit(disp=0)
lr = 2 * (full.llf - base.llf)
print(f"{len(d)} dashes. Likelihood ratio test for speed x angle: chi2 {lr:.2f} on 2 df, p {np.exp(-lr / 2):.4f}")
print(pd.DataFrame({"odds": np.exp(full.params), "lo": np.exp(full.conf_int()[0]), "hi": np.exp(full.conf_int()[1]), "p": full.pvalues}).round(3).loc[["a10:s100", "a2:s100"]])
print("AIC without and with:", round(base.aic, 1), round(full.aic, 1))

print("\nBest angle the model implies at each speed, with timing, hold and wall held equal")
p = full.params
for speed in [1200, 1400, 1600, 1800, 2000, 2200]:
    s = (speed - 1500) / 100
    linear = p["a10"] + p["a10:s100"] * s
    curve = p["a2"] + p["a2:s100"] * s
    print(speed, "best angle", round(60 - 10 * linear / (2 * curve), 1) if curve < 0 else "no peak", "curve", round(curve, 3))

d["A"] = pd.cut(d.flip_angle_deg, [40, 50, 60, 70, 80, 90])
d["S"] = pd.cut(d.speed_first_press, [0, 1300, 1500, 1700, 1900, 2500], labels=["<1300", "1300-1500", "1500-1700", "1700-1900", "1900+"])
fmt = lambda v: f"{v.mean() * 100:.0f}%/{len(v)}"
print("\nMake rate, angle x speed, all timing")
print(pd.crosstab(d.A, d.S, values=d.make, aggfunc=fmt).fillna(""))
late = d[d.ticks >= 13]
print("\nMake rate, angle x speed, 13 ticks or later, where it is less forgiving")
print(pd.crosstab(late.A, late.S, values=late.make, aggfunc=fmt).fillna(""))
print("\nMedian speed gain on makes, angle x speed")
print(d[d.make == 1].pivot_table(index="A", columns="S", values="speed_gain", aggfunc="median", observed=True).round(0))
print("\nMedian end speed on makes, angle x speed")
print(d[d.make == 1].pivot_table(index="A", columns="S", values="speed_peak", aggfunc="median", observed=True).round(0))
