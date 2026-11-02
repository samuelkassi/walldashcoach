#include "AttemptTracker.h"

#include <algorithm>
#include <format>

namespace walldash
{
	namespace
	{
		constexpr float kRadToDeg = 57.29578f;

		// A longer gap means the game was left or the log was stopped.
		constexpr int kMaxFrameGap = 5;
		// Further than this in one tick is a teleport, such as a freeplay reset.
		constexpr float kTeleportDistance = 300.f;

		Surface ClassifySurface(Vec3 up, const AttemptConfig& config)
		{
			if (std::abs(up.z) > config.curveUpZ)
				return Surface::Curve;
			if (std::abs(up.x) >= config.cornerAxis)
				return Surface::SideWall;
			if (std::abs(up.y) >= config.cornerAxis)
				return Surface::BackWall;
			return Surface::Corner;
		}
	}

	const char* ToString(SecondPress value)
	{
		switch (value)
		{
		case SecondPress::Dodge: return "dodge";
		case SecondPress::DoubleJump: return "double_jump";
		default: return "none";
		}
	}

	const char* ToString(Surface value)
	{
		switch (value)
		{
		case Surface::SideWall: return "side_wall";
		case Surface::BackWall: return "back_wall";
		case Surface::Curve: return "curve";
		default: return "corner";
		}
	}

	const char* ToString(WallSide value)
	{
		return value == WallSide::Right ? "right" : "left";
	}

	const char* ToString(Diagnosis value)
	{
		switch (value)
		{
		case Diagnosis::None: return "none";
		case Diagnosis::NotFullyOnWall: return "not_fully_on_wall";
		case Diagnosis::NoSecondJump: return "no_second_jump";
		case Diagnosis::DoubleJump: return "double_jump";
		case Diagnosis::TooLate: return "too_late";
		case Diagnosis::HeldTooLong: return "held_too_long";
		case Diagnosis::BadFlipAngle: return "bad_flip_angle";
		case Diagnosis::NotUpright: return "not_upright";
		case Diagnosis::Scraped: return "scraped";
		default: return "unclassified";
		}
	}

	AttemptTracker::AttemptTracker(Callback callback)
		: onFinished(std::move(callback))
	{
	}

	void AttemptTracker::Reset()
	{
		active = false;
		hasLast = false;
	}

	float AttemptTracker::SpeedInWallPlane(const TickSample& sample) const
	{
		const Vec3 inPlane = sample.vel - attempt.normal * Dot(sample.vel, attempt.normal);
		return Length(inPlane);
	}

	void AttemptTracker::OnTick(const TickSample& sample)
	{
		const bool frameGap = hasLast && (sample.frame <= last.frame || sample.frame - last.frame > kMaxFrameGap);
		const bool teleport = hasLast && Length(sample.pos - last.pos) > kTeleportDistance;
		last = sample;
		hasLast = true;

		if (!active)
			return;
		if (frameGap || teleport)
		{
			active = false;
			return;
		}

		RecordInputs(sample);
		CaptureFlipAngle(sample);

		if (!settled)
			attempt.maxWallDistance = std::max(attempt.maxWallDistance, Dot(sample.pos - startPos, attempt.normal));

		if (settled)
		{
			attempt.speedPeak = std::max(attempt.speedPeak, SpeedInWallPlane(sample));
			if (sample.frame - reattachFrame >= config.speedWindowTicks)
				Finish();
			return;
		}

		UpdateContact(sample);
	}

	void AttemptTracker::RecordInputs(const TickSample& sample)
	{
		ticksInAttempt++;
		if (sample.throttle > 0.f) throttleTicks++;
		if (sample.boost) boostTicks++;
		if (sample.powerslide) powerslideTicks++;
		if (sample.roll != 0.f) airRollTicks++;

		if (dodgeFrame < 0)
			attempt.stickPath.push_back({ sample.pitch, sample.yaw });
	}

	void AttemptTracker::CaptureFlipAngle(const TickSample& sample)
	{
		// The dodge direction still holds the previous dodge on the frame the
		// dodge starts, so it is read one tick later.
		if (flipCaptured || dodgeFrame < 0 || sample.frame <= dodgeFrame)
			return;
		flipCaptured = true;

		attempt.flipAngleUndefined = std::abs(rightAtDodge.z) < config.angleMinRightZ;
		if (attempt.flipAngleUndefined)
			return;

		// Positive is toward the ceiling, so left and right walls compare directly.
		const float side = rightAtDodge.z > 0.f ? 1.f : -1.f;
		attempt.flipAngleDeg = std::atan2(sample.dodgeDir.y * side, sample.dodgeDir.x) * kRadToDeg;
	}

	void AttemptTracker::UpdateContact(const TickSample& sample)
	{
		const int contacts = sample.wheelContacts;

		// Wheels stay on the wall for about 7 ticks after the jump, so contact
		// only counts as a return once it has been lost.
		if (!leftWall)
		{
			if (contacts == 0)
				leftWall = true;
		}
		else if (contacts > 0)
		{
			if (sample.pos.z < config.floorHeight)
			{
				attempt.maxWheels = std::max(attempt.maxWheels, contacts);
				Finish();
				return;
			}

			if (firstTouchFrame < 0)
			{
				firstTouchFrame = sample.frame;
				attempt.firstTouchTicks = sample.frame - attempt.startFrame;
			}
			attempt.maxWheels = std::max(attempt.maxWheels, contacts);

			if (contacts >= config.makeMinWheels && sample.frame - firstTouchFrame <= config.makeWindowTicks)
			{
				attempt.carUpDotNormal = Dot(sample.up, attempt.normal);
				attempt.reattachTicks = sample.frame - attempt.startFrame;
				attempt.make = attempt.secondPress == SecondPress::Dodge && attempt.carUpDotNormal > config.uprightDot;
				attempt.speedReattach = SpeedInWallPlane(sample);

				if (!attempt.make)
				{
					Finish();
					return;
				}

				// A make stays open briefly to catch the peak speed after reattaching.
				settled = true;
				reattachFrame = sample.frame;
				attempt.speedPeak = attempt.speedReattach;
				return;
			}
		}

		if (firstTouchFrame >= 0 && sample.frame - firstTouchFrame > config.makeWindowTicks)
		{
			Finish();
			return;
		}
		if (sample.frame - attempt.startFrame >= config.timeoutTicks)
			Finish();
	}

	void AttemptTracker::StartAttempt(const TickSample& sample)
	{
		attempt = AttemptResult{};
		attempt.startFrame = sample.frame;
		attempt.normal = sample.up;
		attempt.gameSpeed = sample.gameSpeed;
		attempt.surface = ClassifySurface(sample.up, config);
		attempt.wallSide = sample.right.z > 0.f ? WallSide::Right : WallSide::Left;
		attempt.wheelContactsAtStart = sample.wheelContacts;
		attempt.notFullyOnWall = !sample.isOnWall || sample.wheelContacts < config.makeMinWheels;
		attempt.heightAtStart = sample.pos.z;
		attempt.jumpPresses = 1;
		attempt.dodgeDeadzone = sample.dodgeDeadzone;
		attempt.controllerDeadzone = sample.controllerDeadzone;

		startPos = sample.pos;
		attempt.speedFirstPress = SpeedInWallPlane(sample);
		const Vec3 inPlane = sample.vel - attempt.normal * Dot(sample.vel, attempt.normal);
		const float speed = Length(inPlane);
		if (speed > 1.f)
			attempt.travelAngleDeg = std::asin(std::clamp(inPlane.z / speed, -1.f, 1.f)) * kRadToDeg;

		ticksInAttempt = 0;
		throttleTicks = boostTicks = powerslideTicks = airRollTicks = 0;
		releaseFrame = -1;
		dodgeFrame = -1;
		flipCaptured = false;
		leftWall = false;
		firstTouchFrame = -1;
		reattachFrame = -1;
		settled = false;
		active = true;

		RecordInputs(sample);
	}

	void AttemptTracker::OnJumpPressed()
	{
		if (active)
			attempt.jumpPresses++;
	}

	void AttemptTracker::OnJumpReleased()
	{
		if (!active || releaseFrame >= 0 || !hasLast)
			return;
		releaseFrame = last.frame;
		attempt.holdTicks = last.frame - attempt.startFrame;
	}

	void AttemptTracker::OnFirstJumpBegin()
	{
		if (!hasLast)
			return;

		// A first jump is only possible with wheels down, so an attempt still
		// open here has already come back to a surface.
		if (active)
			Finish();

		const float upZ = last.up.z;
		if (upZ >= config.floorUpZ || upZ <= config.ceilingUpZ)
			return;

		StartAttempt(last);
		// The press that started this jump fired just before it, while no attempt was open.
	}

	void AttemptTracker::RecordSecondPress(SecondPress result)
	{
		if (!active || !hasLast || attempt.secondPress != SecondPress::None)
			return;

		attempt.secondPress = result;
		attempt.pressToPressTicks = last.frame - attempt.startFrame;
		if (releaseFrame >= 0)
			attempt.releaseToPressTicks = last.frame - releaseFrame;

		attempt.stickPitch = last.pitch;
		attempt.stickYaw = last.yaw;
		attempt.stickRoll = last.roll;
		attempt.stickMagnitude = std::sqrt(last.pitch * last.pitch + last.yaw * last.yaw);
		attempt.stickSum = std::abs(last.pitch) + std::abs(last.yaw) + std::abs(last.roll);
		attempt.pastDodgeDeadzone = attempt.stickSum >= last.dodgeDeadzone;
	}

	void AttemptTracker::OnDoubleJumpBegin()
	{
		RecordSecondPress(SecondPress::DoubleJump);
	}

	void AttemptTracker::OnDodgeBegin()
	{
		if (!active || !hasLast || attempt.secondPress != SecondPress::None)
			return;
		RecordSecondPress(SecondPress::Dodge);
		dodgeFrame = last.frame;
		rightAtDodge = last.right;
		attempt.speedDodge = SpeedInWallPlane(last);
	}

	void AttemptTracker::Finish()
	{
		active = false;

		attempt.spamInput = attempt.jumpPresses >= config.spamPresses;
		if (ticksInAttempt > 0)
		{
			const float total = static_cast<float>(ticksInAttempt);
			attempt.throttleFraction = throttleTicks / total;
			attempt.boostFraction = boostTicks / total;
			attempt.powerslideFraction = powerslideTicks / total;
			attempt.airRollFraction = airRollTicks / total;
		}
		if (attempt.make)
			attempt.speedGain = attempt.speedPeak - attempt.speedFirstPress;

		Diagnose();

		if (onFinished)
			onFinished(attempt);
	}

	void AttemptTracker::Diagnose()
	{
		AttemptResult& a = attempt;

		if (a.make)
		{
			a.diagnosis = Diagnosis::None;
			a.diagnosisText.clear();
			return;
		}

		if (a.notFullyOnWall)
		{
			a.diagnosis = Diagnosis::NotFullyOnWall;
			a.diagnosisText = std::format("Not fully on the wall ({} wheels)", a.wheelContactsAtStart);
			return;
		}
		if (a.secondPress == SecondPress::None)
		{
			a.diagnosis = Diagnosis::NoSecondJump;
			a.diagnosisText = "No second jump";
			return;
		}
		if (a.secondPress == SecondPress::DoubleJump)
		{
			a.diagnosis = Diagnosis::DoubleJump;
			a.diagnosisText = std::format("Double jump, stick not past the dodge deadzone (stick {:.2f}, needs {:.2f})", a.stickSum, a.dodgeDeadzone);
			return;
		}
		if (a.PressToPressMs() > config.latePressMs)
		{
			a.diagnosis = Diagnosis::TooLate;
			a.diagnosisText = std::format("Second press too late: {:.0f} ms", a.PressToPressMs());
			return;
		}
		if (config.holdMaxMs > 0.f && a.HoldMs() > config.holdMaxMs)
		{
			a.diagnosis = Diagnosis::HeldTooLong;
			a.diagnosisText = std::format("First jump held {:.0f} ms", a.HoldMs());
			return;
		}
		if (!a.flipAngleUndefined && !std::isnan(a.flipAngleDeg))
		{
			if (a.flipAngleDeg < 0.f && a.flipAngleDeg >= -90.f)
			{
				a.diagnosis = Diagnosis::BadFlipAngle;
				a.diagnosisText = std::format("Flip angle {:.0f} deg, aimed at the floor", a.flipAngleDeg);
				return;
			}
			if (std::abs(a.flipAngleDeg) > 90.f)
			{
				a.diagnosis = Diagnosis::BadFlipAngle;
				a.diagnosisText = std::format("Flip angle {:.0f} deg, a backflip", a.flipAngleDeg);
				return;
			}
		}
		if (a.reattachTicks >= 0)
		{
			a.diagnosis = Diagnosis::NotUpright;
			a.diagnosisText = std::format("Came back twisted (upright {:.2f})", a.carUpDotNormal);
			return;
		}
		if (a.firstTouchTicks >= 0 && a.maxWheels < config.makeMinWheels)
		{
			a.diagnosis = Diagnosis::Scraped;
			a.diagnosisText = std::format("Scraped: touched with {} wheels but didn't stick", a.maxWheels);
			return;
		}

		a.diagnosis = Diagnosis::Unclassified;
		a.diagnosisText = std::format("Unclassified: press to press {:.0f} ms, flip angle {:.0f} deg, max distance {:.0f}",
			a.PressToPressMs(), a.flipAngleDeg, a.maxWallDistance);
	}
}
