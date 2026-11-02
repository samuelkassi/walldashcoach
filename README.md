# WallDash Trainer

A BakkesMod plugin for Rocket League that measures your wall dash attempts in freeplay and tells you why each one worked or failed.

It trains the controlled two-press method: a short first jump off the wall, a flick of the stick, then a second jump that dodges the car back onto the wall.

**Status: early development.** There is no overlay or settings window yet, and no release to install. See [Roadmap](#roadmap).

## What works today

- Hooks the jump, dodge and input events for your car in freeplay.
- Writes a per-tick debug log to a CSV file, off by default.

| Console command | What it does |
|---|---|
| `walldash_debug_log 1` | Starts a new log in `<BakkesMod data folder>/walldashtrainer/`. Set to `0` to stop. |
| `walldash_debug_status` | Prints the log path, rows written and how many times each event fired. |

## Building from source

You need Windows, BakkesMod, and Visual Studio 2022 or its Build Tools with the v143 C++ toolset and a Windows 11 SDK.

```
msbuild WallDashTrainer.sln -p:Configuration=Release -p:Platform=x64
```

The build finds the BakkesMod SDK through the registry key BakkesMod creates when installed, and the post-build step copies the plugin into your BakkesMod plugins folder. In game, press F6 to open the console and run `plugin load walldashtrainer`.

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
