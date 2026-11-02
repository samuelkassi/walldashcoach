# WallDash Trainer

A BakkesMod plugin for Rocket League that measures your wall dash attempts in freeplay and tells you why each one worked or failed.

It trains the controlled two-press method: a short first jump off the wall, a flick of the stick, then a second jump that dodges the car back onto the wall.

**Status: early development.** There is no overlay or settings window yet, and no release to install. See [Roadmap](#roadmap).

## What works today

In freeplay, each wall dash attempt is detected and judged as a make or a miss. The result prints to the console (F6), with the reason for a miss and your session stats, and is saved as one row in `<BakkesMod data folder>/walldashtrainer/attempts_YYYY-MM-DD.csv`.

Attempts at normal speed and slowed-down attempts are counted separately, as are flat walls and curves or corners. Timings are in game time, so 58 ms at 50% speed took about 117 ms of real time.

| Console command | What it does |
|---|---|
| `walldash_debug_status` | Prints the session stats, log paths and how many times each event fired. |
| `walldash_attempt_log 0` | Stops writing the attempt CSV. On by default. |
| `walldash_debug_log 1` | Starts a per-tick debug log. Set to `0` to stop. |

The thresholds are cvars starting with `walldash_`, such as `walldash_make_window_ticks` and `walldash_late_press_ms`. They are all listed in `RegisterCvars` in `src/WallDashTrainer.cpp`, and will move to the settings window in Phase 2.

## Building from source

You need Windows, BakkesMod, and Visual Studio 2022 or its Build Tools with the v143 C++ toolset and a Windows 11 SDK.

```
msbuild WallDashTrainer.sln -p:Configuration=Release -p:Platform=x64
```

The build finds the BakkesMod SDK through the registry key BakkesMod creates when installed, and the post-build step copies the plugin into your BakkesMod plugins folder. In game, press F6 to open the console and run `plugin load walldashtrainer`.

## Testing without the game

The attempt logic in `src/core` does not depend on the BakkesMod SDK, so it can run on recorded logs. This replays a real 50% speed session and checks the make counts:

```
msbuild tests\ReplayLogs.vcxproj -p:Configuration=Release -p:Platform=x64
build\tests\ReplayLogs.exe tests\data\debug_ticks_50pct.csv --speed 0.5 --expect-left 6/12 --expect-right 10/10
```

## Roadmap

| Phase | Goal | State |
|---|---|---|
| 0 | Prove the events and measurements work | Done, see [results](docs/PHASE0_RESULTS.md) |
| 1 | Detect attempts, judge make or miss, write the attempt CSV | In progress |
| 2 | On-screen overlay and settings window | Planned |
| 3 | Set the timing bands from recorded data | Planned |
| 4 | Optional chains and reset-to-wall | Planned |
| 5 | Prepare for release on bakkesplugins.com | Planned |

The original design notes are in [BRIEF.md](BRIEF.md). Later results docs override it where they differ.

## Licence

MIT, see [LICENSE](LICENSE). Third-party code is listed in [THIRD_PARTY.md](THIRD_PARTY.md).
