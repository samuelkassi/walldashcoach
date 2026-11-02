# Phase 0 results (2 Nov 2026)

**Status: the 100% speed run passes. The 50% speed run is still to do.**

Source: `debug_ticks_2026-11-02_183112.csv`, a snapshot of about 60,000 ticks in freeplay at 100% speed, with 194 jump presses. The snapshot was taken while logging was still on, so it stops shortly before the end of the session.

Your settings for this run: dodge deadzone 0.90, controller deadzone 0.08, air control sensitivity 1.40. The log reads the same values as your settings screen.

## Tests and how they map to the log

The rep blocks in the log line up with the test order, so every test was done. Some reps were on the back walls rather than the side walls. That turned out to be useful, because the sign check below covers all four walls.

## What the SDK really does, compared to the brief

| Brief says | Finding |
|---|---|
| Use `WorldInfo.GetTimeSeconds()` for time | Use `GetEngine().GetPhysicsFrame()` for ticks. It advanced by exactly 1 per `SetVehicleInput` call, with no gaps across the snapshot. World time did stall (125 ticks with no change) and jump (up to 400 ms). Inside attempts the error was at most 1.2 ms, so it is not harmful, but ticks are the true count. |
| One input hook call per tick is unknown | Confirmed: exactly 1 call per physics frame in freeplay. |
| Press timing may lag `BeginState` | No lag. `OnJumpPressed`, the jump bit and the component `BeginState` all land on the same frame. `OnJumpReleased` also matches the jump bit (194 of 194). |
| `CarComponent_Dodge_TA.Active.BeginState` | Exists and fires (67). |
| Jump and Dodge `Active.EndState` | Never fired. Not needed: `GetbActive()` on the component shows the same thing. |
| Read the dodge direction from the dodge component | Works, but **the value is stale on the `BeginState` frame**. It still holds the previous dodge. Read it one tick later. It is in the car frame: X is forward, Y is right, roughly `(-pitch, yaw)` normalised. A double jump does not update it. |
| Dodge deadzone from gamepad settings | Gamepad settings and the PRI both read 0.90. **`DodgeComponentWrapper::GetDodgeInputThreshold()` reads 0.5 and must not be used.** |
| Wheels "back on the wall" ends the attempt | Wheels keep contact for 7 ticks after the jump in nearly every rep, so the attempt must see contact lost first. |
| Stick input | Use the `SetVehicleInput` params. Event rows read `car.GetInput()`, which can differ very slightly on the same frame. |

## The dodge deadzone rule

A dodge happens when `|pitch| + |yaw| + |roll|` reaches the deadzone. It is a sum, not stick magnitude.

- Highest sum that gave a double jump: 0.831
- Lowest sum that gave a dodge: 0.913
- Magnitude does not separate them: a dodge at 0.646 and a double jump at 0.779

Only one rep tests the roll term (roll 1.0, pitch 0.55, yaw 0.06 gave a dodge), so that part needs more data.

Consequences for later phases:
- The "stick not past the deadzone" diagnosis should print this sum.
- The deadzone on the stick circle should be a **diamond**, not a ring. With your 0.90, straight ahead needs 0.90 of stick, but a perfect diagonal needs only about 0.64.

Not known yet: whether logged values are the physical stick position or already scaled by sensitivity. The rule holds on the logged values either way, so this only affects how the stick circle is labelled.

## Flip angle sign

Use `angle = atan2(dy * sign(right_z), dx)`, with `(dx, dy)` read one tick after the dodge starts and `right_z` being the height component of the car's right vector.

Two independent checks agree:
- Test order: the ceiling block came out positive and the floor block negative.
- Physics: all 34 flips with a positive angle pushed the car toward the wall, and all 10 with a negative angle pushed it away (angles between 15 and 165 degrees, either side).

It held on all four walls and on steep paths up to a forward vector height of about 0.9. With the car pointing straight up or down the wall, `right_z` nears zero and the sign has no meaning, so the plugin should log that case as undefined.

## Why the wall dash works

Every wall dodge changed height only by gravity. The dodge impulse acts in the **horizontal plane**, not along the wall. So on a wall the sideways part of a dodge points straight into or out of the wall.

| Flip angle | Forward speed added | Velocity into wall |
|---|---|---|
| About 30 | about 430 | about 390 |
| About 50 | about 325 | about 590 |
| About 78 | about 100 | about 750 |
| Toward the floor, about -90 | about 0 | about -720 (pushes away) |

The first jump leaves the wall at about 300 uu/s, so the dodge has to cancel that and bring the car back. This is the trade-off behind your 45 versus 60 degree question: more forward means more speed but less push back to the wall. Phase 3 can now test it directly.

These numbers are from flat walls at about 1100 to 1500 uu/s. Dodge impulse is also known to scale with speed, which this run does not test.

## What a make looks like

First wheel contact is not a make. Many reps touch with 1 or 2 wheels around 19 to 25 ticks, scrape for about 30 ticks, then fall off. Real reattaches reach 3 or 4 wheels.

Using "reaches at least 3 wheels on the wall soon after the first touch, with car up within about 25 degrees of the wall normal", the run has 7 makes:

| Press to press | Flip angle | Speed gain |
|---|---|---|
| 83 ms (10 ticks) | 42 | +370 |
| 100 ms (12) | 49 | +335 |
| 108 ms (13) | 36 | +465 |
| 117 ms (14) | 54 | +411 |
| 117 ms (14) | 50 | +234 |
| 117 ms (14) | 68 | +142 |
| 117 ms (14) | 78 | +49 |

This is a small sample and the make labels are not yet checked against what you saw. The 50% run is meant to check them.

**It conflicts with the starting bands in the brief.** With green at 80 ms or less and red above 95 ms, six of these seven makes would be red and "Second press too late" would fire on them. They may be makes for other reasons, and Phase 3 should decide. Until then I suggest Phase 1 logs press-to-press without using it as a miss reason.

## Other findings for Phase 1

- `IsOnWall()` was true with car up height as high as 0.93, so it includes curves. Surface type must come from the normal.
- Car up and the wheel contact normal agree (median dot product 1.0, lowest 0.87 on curves), so the rotator to axes conversion is correct.
- 4 jump presses triggered no component at all (no flip left). Counting presses for `spam_input` must use `OnJumpPressed`, not the components.

## Still to do

- 50% speed run: check the make labels against your count, and see how game speed affects the tick count and logged times.
- A few more reps with air roll to test the roll term.
