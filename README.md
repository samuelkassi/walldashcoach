# WallDash Trainer

A BakkesMod plugin for Rocket League that measures your wall dash attempts in freeplay and tells you why each one worked or failed.

It trains the controlled two-press method: a short first jump off the wall, a flick of the stick, then a second jump that dodges the car back onto the wall. It works in freeplay only and does nothing in online matches. It is built for controllers.

## Getting started

1. Install it from bakkesplugins.com, or build it from source as described below.
2. Go into freeplay, drive up a wall and try a wall dash.
3. Read the overlay. Press F2 and find WallDash Trainer under Plugins to move panels or change settings.

## Reading the overlay

**At the top:**

- **Press to press:** time from the first jump press to the second. Red on the left means too early, because the second press does nothing until the wheels leave the wall, about 58 ms after the first jump on a flat wall. Green and yellow follow, then red for too late. The limits move with your speed at the first press, because going faster is more forgiving.
- **Hold:** how long you held the first jump.
- **Release to press:** time from letting go of the first jump to the second press. This one is for information, and has no target.
- **Verdict:** MAKE or MISS, with the reason for a miss.

**At the bottom:**

- **Stick display:** your stick's path from the first press to the flip, one dot per tick, so widely spaced dots mean a fast flick. The coloured rim marks good flip angles for the wall you are on. The line shows the direction you actually flipped, and the yellow diamond is your dodge deadzone. The game adds up pitch, yaw and roll to compare against the deadzone, which is why it is a diamond rather than a circle.
- **Speed gain:** what the dash gained or lost, with your speed before and after.
- **Max distance from the wall.**
- **Session stats:** makes and attempts, split by wall. Flat walls, curves and corners, and slowed-down play are counted separately.

Three greens almost always means a make. If you are missing, start with press to press and flip angle, as those matter most. Going faster helps too.

Times are in game time, so at 50% speed the overlay also shows the real time your hands took.

## Options

Everything can be changed in the settings window, including every band and each panel's position and size.

**Chains** (off by default): shows your current and best chain and the speed lost between dashes. A chain continues while each dash is a make and the next jump comes within 1 second of landing. An optional counter tracks how many chain dashes you have recorded per wall.

**Reset to wall** (off by default): places the car on a chosen wall at a set height, speed, direction and boost, so every rep starts the same. Turn it on in the settings, then bind `walldash_reset_to_wall` to a button in the BakkesMod Bindings tab. It needs a standard-size map.

**Console commands** (F6):

| Command | What it does |
|---|---|
| `walldash_reset_stats` | Clears the session stats, best chain and last result. |
| `walldash_console_log 1` | Also prints each attempt to the console. |
| `walldash_attempt_log 0` | Stops saving attempts to a file. On by default. |
| `walldash_debug_log 1` | Starts a detailed per-tick log. Set to `0` to stop. |
| `walldash_debug_status` | Prints session stats, log paths and how many times each game event fired. |

## Your data

Each attempt is saved as one row in `<BakkesMod data folder>/walldashtrainer/attempts_YYYY-MM-DD.csv`, which is easy to open in a spreadsheet or pandas. These files stay on your PC. **The plugin sends nothing anywhere.**

### Sharing your data

This is optional and not in the settings window. If you would like to share your attempts or suggest tighter bands, run this in the console:

```
walldash_export_share_data
```

It writes copies of your attempt files to `<BakkesMod data folder>/walldashtrainer/share/`, with timestamps removed and session ids replaced by plain numbers. They still contain your inputs, car movement and controller deadzones. Sending them is entirely up to you. Contact details will be added here before release.

## Where the numbers come from

The default bands come from about 1,200 recorded dashes by one player, and held up when tested on about 1,400 more. They aim at making the dash, not at the fastest possible press, so stronger players will likely want tighter limits.

- [What drives a make](docs/CALIBRATION.md): flip angle, timing and speed.
- [Chains](docs/PHASE4_RESULTS.md): the turn before a dash, speed lost between dashes and reaching supersonic.
- [The data](data/README.md): every recorded attempt, without timestamps.
- `tools/`: the scripts behind those write-ups.

Some findings so far: a flip angle of 50 to 60 degrees is the best all-round target, more forward flips gain more speed but make far less often, and a smaller turn between dashes keeps more speed. This is one player's data and shows patterns rather than proven causes.

## Building from source

You need Windows, BakkesMod, and Visual Studio 2022 or its Build Tools with the v143 C++ toolset and a Windows 11 SDK.

```
msbuild WallDashTrainer.sln -p:Configuration=Release -p:Platform=x64
```

The build finds the BakkesMod SDK through the registry key BakkesMod creates when installed. The post-build step copies the plugin into your BakkesMod plugins folder. Then press F6 in game and run `plugin load walldashtrainer`.

There are no external DLLs or downloads. A pre-build step, `update_version.ps1`, increases the build number in `version.h`.

### Testing without the game

The attempt logic in `src/core` does not depend on the BakkesMod SDK, so it can run on recorded logs. This replays a real 50% speed session and checks the make counts:

```
msbuild tests\ReplayLogs.vcxproj -p:Configuration=Release -p:Platform=x64
build\tests\ReplayLogs.exe tests\data\debug_ticks_50pct.csv --speed 0.5 --expect-left 6/12 --expect-right 10/10 --expect-diagnosis too_early=1
```

## How it was built

Development results are in [`docs/`](docs/): [Phase 0](docs/PHASE0_RESULTS.md) proved the measurements, [Phase 1](docs/PHASE1_RESULTS.md) added attempts, [Phase 2](docs/PHASE2_RESULTS.md) added the overlay, [Phase 3](docs/CALIBRATION.md) set the bands from data, and [Phase 4](docs/PHASE4_RESULTS.md) added chains.

## Licence

MIT, see [LICENSE](LICENSE). Third-party code is listed in [THIRD_PARTY.md](THIRD_PARTY.md).
