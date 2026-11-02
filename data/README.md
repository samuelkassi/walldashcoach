# Attempt data

`attempts.csv` holds every wall dash attempt one player recorded with this plugin during development, in the order they were played. It has 1484 attempts across 6 plugin sessions, mostly at 100% game speed, with some at 50%.

It was all played on a controller with a dodge deadzone of 0.90, a controller deadzone of 0.08 and air control sensitivity of 1.40. It is one player's data, recorded while they were improving quickly, so it shows that player rather than Rocket League players in general.

## What differs from the plugin's own CSV

- Timestamps and session ids are removed. `attempt_order` keeps the order, and `session` numbers the plugin sessions from 1.
- `chain_id` and `chain_index` are removed, because chains did not exist yet.
- The stick angle and stick sum columns were recalculated from `stick_path`, because earlier versions of the plugin did not log them. They leave out air roll.
- Columns added during development are blank for earlier attempts. `speed_end`, `total_speed_first_press`, `total_speed_end` and `wheels_at_second_press` are the main ones.

## Things to watch for

- **`result` is consistent throughout.** The rule for a make did not change.
- **`diagnosis` and `diagnosis_text` are not.** They show the rules in the plugin version used for that attempt. The early `no_second_jump` rows include presses that later versions call `too_early`, and later versions check flip angle before timing.
- **Times are game time.** `game_speed` shows how fast the game was running. At 0.5, a 58 ms press took about 117 ms of real time, so split attempts by speed before comparing them.
- **Filter before comparing makes.** Some attempts are deliberate tests from early sessions, such as flips toward the floor. A sensible filter for real dashes is `game_speed >= 0.95`, a `surface` of `side_wall` or `back_wall`, `air_roll_fraction == 0`, `second_press_result == "dodge"`, `press_to_press_ticks <= 21` and `flip_angle_deg` between 0 and 90.

`stick_path` holds one `pitch:yaw` pair per tick from the first press up to the dodge, separated by semicolons.
