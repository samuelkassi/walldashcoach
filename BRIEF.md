# Brief: WallDash Trainer (BakkesMod plugin)

This is a handover for Claude Code. Read all of it before writing any code, then propose a plan for Phase 0 only and wait for me.

## 1. What we are building

A BakkesMod plugin that runs in freeplay, measures my inputs and the car's state every time I attempt a wall dash, decides whether it was a make or a miss, and tells me why on an on-screen overlay. The overlay style is the one from SpeedFlipTrainer and FastAerialTrainer: coloured range bars with green / yellow / red bands, plus text.

It starts as my personal training tool, but I intend to publish it on bakkesplugins.com when it is done, so it must be clean enough for other people: everything configurable in the F2 settings window, no hard-coded paths, sensible defaults, and the optional features below behind toggles.

About me and my setup:
- Windows, Rocket League on Epic, BakkesMod, launched without EAC by my own `.bat` script. I play at 120 fps.
- Controller player. The plugin is controller-first. Keyboard should not crash anything, but its stick angles will only ever be the 8 key directions, which is fine.
- I have built BakkesMod plugins from source before (a modified SpeedFlipTrainer, and `../nexto-sparring`). Build fixes that were needed: `-p:PlatformToolset=v143` and the Windows 11 SDK component in the Visual Studio Installer. Copy the build setup (`BakkesMod.props`, `.vcxproj` settings, post-build patch step, version.h / update_version.ps1 pattern) from `../nexto-sparring`.
- I am a CS graduate and comfortable with C++ and git, but explain build or SDK errors plainly.

## 2. The wall dash, and which method this trains

The controlled two-press method only. Stick down (or neutral), short tap of jump (first jump off the wall), flick the stick to the flip direction, second jump press, which produces a dodge whose rotation brings the wheels back onto the wall, and the car carries the extra speed along the wall.

I do NOT want to train the jitter-click / spam method (rapidly vibrating the jump button with air roll held). The plugin does not need to coach it. If inputs look like spam (many jump presses inside one attempt), just tag the attempt `spam_input` and keep it out of the stats.

### What the community says (treat every number here as a hypothesis to test, not a fact)

Sources: two r/RocketLeagueSchool threads I read ("Has the frequency requirement to wall dash consistently been measured?" and "How does one even wall dash?").

- About 80 ms between the first and second jump press makes a wall dash on any wall at any angle ("12.5 Hz"). The person measured this with a script, so it is press-to-press. Another player using a modified FastAerialTrainer got dashes at 95 ms, but that plugin measures release-of-first-jump to second press, so the two numbers are not the same measurement. We measure all three timings (section 4) and let my data decide which one actually predicts makes.
- The required timing depends on the flip angle and on speed: the more forward the flip, the faster the dash, but the tighter the timing window. Consensus flip angle is about 60 degrees from straight ahead (a diagonal flip close to a sideflip, into the direction of the wall). My own hypothesis: closer to 45 degrees gains more speed but needs a faster second press. The plugin exists partly to test this.
- Slow speed makes it harder; you need some forward momentum. With ~80 ms and some momentum you can reach supersonic without boost.
- Significantly easier going slightly DOWN the wall than slightly up.
- Air roll is not needed. Powerslide can help keep the speed gained.
- Curves and corners make it much easier (corners are curved on a different axis). Just above where the wall starts curving is more forgiving.
- Sequences: a "slow" chain and a "fast" chain are the same two-press dash; only the pause between dashes differs.
- Prior art worth reading: a FastAerialTrainer modification for wall dashing on bakkesplugins.com (https://bakkesplugins.com/plugins/view/556).

## 3. Reference code

- `reference/FastAerialTrainer/`: source I supplied (read-only, never edited). Its hooks are the model for timing: `CarComponent_Jump_TA.Active.BeginState` (first jump start), `TAGame.Car_TA.OnJumpReleased`, `CarComponent_DoubleJump_TA.Active.BeginState`, the `TAGame.Car_TA.SetVehicleInput` per-tick hook with `ControllerInput`, `WorldInfo.GetTimeSeconds()` for time, and `DrawBar` for the range bars. Its licence is unknown (no licence file in what I gave you): check its GitHub page before copying code verbatim.
- SpeedFlipTrainer by tynidev: https://github.com/tynidev/SpeedFlipTrainer . Clone into `reference/SpeedFlipTrainer` (read-only). Use it for (a) how it reads the dodge / stick angle and (b) its overlay layout. It is GPL-3.0.

Licence rule: keep a `THIRD_PARTY.md` listing anything copied or closely adapted and from where. If any SpeedFlipTrainer code is copied, the published plugin must be GPL-3.0 and its source released; flag this to me before it happens rather than deciding silently.

## 4. What to measure, per attempt

Physics runs at 120 Hz (about 8.33 ms per tick), so every timing is effectively quantised to ticks. Show ms on the overlay but log ticks too.

An attempt starts when a first jump begins while the car's wheels are on a wall (not floor, not ceiling). Record the wall normal `n` at that moment (car up vector). It ends when (a) the wheels are back on the wall, (b) the car touches the floor, or (c) a timeout (default 1.0 s, cvar).

Measure:
- `hold_ms`: first jump press to first jump release.
- `release_to_press_ms`: first jump release to second jump press.
- `press_to_press_ms`: first jump press to second jump press. Main timing bar.
- `second_press_result`: dodge / double_jump / none. Use the Dodge and DoubleJump component BeginState events, not guesses from input.
- Stick at the second press: raw pitch and yaw values, magnitude, and whether it was past the dodge deadzone (read the player's dodge deadzone and controller deadzone from gamepad settings, as FastAerialTrainer reads AirControlSensitivity).
- `flip_angle_deg`: direction of the dodge in the car's frame. 0 = straight ahead, positive = toward the ceiling, negative = toward the floor, ±180 = backflip. Compute the sign geometrically (does the dodge direction, mapped to world space, point up or down?) so left-wall and right-wall dashes are directly comparable regardless of which way I am driving. Prefer the dodge component's actual direction if the SDK exposes it; fall back to the stick values at the dodge tick. Phase 0 must verify the sign with a deliberate test flip toward the ceiling.
- `travel_angle_deg`: my direction of travel along the wall at the first press. 0 = horizontal, positive = going up the wall, negative = going down.
- `max_wall_distance`: max of `(pos - pos_at_first_press) · n` during the attempt, in unreal units.
- Speeds, all in unreal units per second, measured in the wall plane (`v - (v·n)n`): at first press, at the dodge, at reattach, and the peak within a short window after reattach (default 0.15 s). `speed_gain` = reattach-window peak minus speed at first press.
- Throttle held, boost held, powerslide held, air roll held during the attempt (fractions of ticks).
- Surface type at the start: `side_wall`, `back_wall`, `curve` (normal has a significant vertical component: floor-to-wall or wall-to-ceiling transition), `corner` (normal horizontal but not aligned to an arena axis). Thresholds as cvars.
- Car height on the wall at the start.
- Number of jump presses in the attempt (for `spam_input`).
- Per-tick stick path (pitch, yaw) from first press to dodge, for the stick display.

Every SDK function name in this brief is from memory. Verify against the headers in the BakkesMod SDK (`$(BakkesModPath)\bakkesmodsdk\include`) in Phase 0 and tell me what differs.

## 5. Make, miss and the diagnosis

Make = wheels back on the wall (same surface) within the timeout and the car upright on it (`car up · n` above a threshold, cvar). Speed gain is graded separately: a make that loses speed is still a make, shown with a red speed bar.

Miss diagnosis: check in this order and report the first that matches, in plain words with the number that caused it:
1. Not on a wall when the first jump started (jumped from the floor transition): "Not fully on the wall".
2. No second press inside the timeout: "No second jump".
3. Second press gave a double jump: "Double jump, stick not past the dodge deadzone (stick 0.31)". The most common failure.
4. Press-to-press too slow: "Second press too late: 112 ms".
5. First jump held too long: "First jump held 70 ms".
6. Flip angle out of range: backflip component, toward the floor, or too sideways: "Flip angle -20°, aimed at the floor".
7. Car came back but not upright (twisted / landed on side).
8. Otherwise: "Unclassified", with the key numbers.

Thresholds for every rule are cvars with these starting values, which we will replace after calibration (Phase 3):
- press-to-press: green ≤ 80 ms, yellow 80 to 95, red > 95.
- hold and release-to-press: no bands at first, just display. Bands come from my data.
- flip angle: no "correct" band at first. Display the value; bands come from my data.

After calibration the press-to-press band should become a function of flip angle (tighter for more forward flips), if my data supports it.

## 6. Overlay

Only in freeplay, hidden when paused. Split between top and bottom of the screen so the car stays visible. Every panel has its own position and size in settings and its own on/off checkbox. All on by default.

Top:
- Bars: press-to-press, hold, release-to-press (FastAerialTrainer `DrawBar` style, with the band preview).
- Verdict line: MAKE / MISS + the diagnosis text.
- Surface note when not on a flat wall: "Curve: easier than a flat wall, logged separately" / "Corner: much easier (curved on a different axis), logged separately".

Bottom:
- Stick circle: the stick's path from first press to dodge, one dot per tick (dot spacing shows flick speed), markers at first press, first release and second press, the dodge deadzone drawn as an inner ring, and a line for the final flip direction with the flip angle printed next to it.
- Speed gain bar (red if negative), with speed before / after as text.
- Max distance from the wall.
- Session stats: flat-wall makes / attempts and %, average press-to-press on makes. Curves and corners shown on a separate line.
- Chain counter (only when chains are on): current chain and best chain.

No pitch history graph (the stick circle replaces it).

## 7. Optional features (toggles, default off)

- **Chains.** A chain continues while each dash is a make and the next first jump starts within a gap after reattaching (default 0.5 s, cvar). A miss, leaving the wall or touching the floor ends it. Each dash is still measured and logged individually, with a chain id and index.
- **Reset to wall.** A bindable command that places the car on a chosen flat wall (left/right side wall, or back wall) at a chosen height, speed (uu/s), direction along the wall, and boost amount, so every rep starts identically. Freeplay only. I set up my own dashes, but other users will want this.

Not in v1: saving / replaying attempts.

## 8. Logging

- One CSV row per attempt, in the BakkesMod data folder (`gameWrapper->GetDataFolder()` / `walldashtrainer/attempts_YYYY-MM-DD.csv`). Columns: timestamp, session id, make/miss, diagnosis code, surface, every measurement in section 4 (ticks and ms), stick at second press, deadzone settings, chain id/index, spam flag.
- A per-tick debug log behind a cvar (default off) for Phases 0 and 3.
- The CSV must be easy to analyse in pandas: one header, no units in values, snake_case names.

## 9. Phases

### Phase 0: prove it (spike)
Scaffold the project from nexto-sparring's build setup, plugin type freeplay. Hook the events in section 3, verify each fires on the wall, verify the SDK functions exist (wall contact, jumped / double jumped / has flip, dodge component and its direction, wheel contacts, gamepad deadzone settings). Write the per-tick debug log. Test: I do a few dashes and deliberate flips, we check timings are sane at 8.33 ms resolution and the flip angle sign is right on both side walls. Write `docs/PHASE0_RESULTS.md`.

### Phase 1: attempts
Attempt start/end, make/miss, surface detection, all measurements, the attempt CSV.

### Phase 2: overlay
Bars, stick circle, verdict, surface notes, speed, session stats, settings window.

### Phase 3: calibration
I record 50 to 100 attempts on flat walls. Analyse the CSV (Python/pandas is fine): which timing best separates makes from misses, make rate vs press-to-press by flip-angle bucket, speed gain vs flip angle (tests my 45° vs 60° hypothesis), going up vs down the wall, speed at first press. Set the bands from the data and propose the angle-dependent band. Write findings to `docs/CALIBRATION.md`.

### Phase 4: toggles
Chains, reset to wall.

### Phase 5: publish prep
Settings polish, defaults, README with screenshots, licence decision, THIRD_PARTY.md, bakkesplugins.com submission requirements.

## 10. Working style

- One phase at a time. Propose the plan for a phase, wait for my go, build, then tell me exactly what to test in game.
- Use git, small commits.
- Never edit anything under `reference/`.
- Before changing any existing file of mine outside this folder, back it up first.
- Explain build and SDK errors plainly.
- No em dashes in anything written for me (docs, README, overlay text).
