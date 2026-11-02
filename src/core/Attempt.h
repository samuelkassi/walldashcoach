#pragma once

// Data types for wall dash attempts. Nothing under src/core depends on the
// BakkesMod SDK, so tests/ReplayLogs can run the same logic on recorded logs.

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace walldash
{
	constexpr float kMsPerTick = 1000.f / 120.f;
	constexpr float kNotMeasured = std::numeric_limits<float>::quiet_NaN();

	struct Vec3
	{
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
	};

	inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

	// One physics tick of the local car. dodgeDir is in the car frame:
	// x is forward, y is right.
	struct TickSample
	{
		int frame = 0;
		float gameSpeed = 1.f;
		float throttle = 0.f;
		float pitch = 0.f;
		float yaw = 0.f;
		float roll = 0.f;
		bool powerslide = false;
		bool boost = false;
		Vec3 pos;
		Vec3 vel;
		Vec3 right;
		Vec3 up;
		int wheelContacts = 0;
		bool isOnWall = false;
		Vec3 dodgeDir;
		float dodgeDeadzone = 0.f;
		float controllerDeadzone = 0.f;
	};

	enum class SecondPress { None, Dodge, DoubleJump };
	enum class Surface { SideWall, BackWall, Curve, Corner };
	enum class WallSide { Left, Right };

	enum class Diagnosis
	{
		None, // A make.
		NotFullyOnWall,
		NoSecondJump,
		TooEarly, // Pressed again while the wheels were still down, which the game ignores.
		DoubleJump,
		TooLate,
		HeldTooLong,
		BadFlipAngle,
		NotUpright,
		Scraped,
		Unclassified
	};

	// Limits are whole milliseconds, and a tick is 8.33 ms, so comparisons allow
	// this much slack: 13 ticks is 108.33 ms and still counts as "up to 108".
	constexpr float kBandSlackMs = 0.5f;

	// Faster starts forgive later presses, so the press to press limits depend on
	// speed at the first press. See docs/CALIBRATION.md.
	constexpr int kPressBandCount = 5;
	constexpr float kPressBandSpeedLimits[kPressBandCount - 1] = { 1300.f, 1500.f, 1700.f, 1900.f };

	struct PressBand
	{
		float greenMs;
		float yellowMs; // Above this is too late. Equal to green means no yellow zone.
	};

	// Every value is configurable through a cvar. See WallDashTrainer::RegisterCvars.
	struct AttemptConfig
	{
		int timeoutTicks = 120;
		int makeWindowTicks = 30;
		int makeMinWheels = 3;
		float uprightDot = 0.9f;
		int speedWindowTicks = 18;
		PressBand pressBands[kPressBandCount] = {
			{ 100.f, 117.f },
			{ 108.f, 108.f },
			{ 108.f, 117.f },
			{ 117.f, 125.f },
			{ 125.f, 133.f },
		};
		float holdGreenMs = 50.f;
		float holdMaxMs = 75.f; // Above this is red and a miss reason. 0 turns the miss reason off.
		int spamPresses = 4;
		float curveUpZ = 0.1f;
		float cornerAxis = 0.98f;
		float floorUpZ = 0.98f;
		float ceilingUpZ = -0.7f;
		float floorHeight = 60.f;
		float angleMinRightZ = 0.2f;

		// Flip angle bands in degrees toward the ceiling. Inside green is best,
		// inside yellow is acceptable, and outside yellow is a miss reason.
		float flipYellowMin = 45.f;
		float flipGreenMin = 50.f;
		float flipGreenMax = 70.f;
		float flipYellowMax = 85.f;

		// Press to press limits for a given speed at the first press.
		const PressBand& PressBandFor(float speed) const
		{
			for (int i = 0; i < kPressBandCount - 1; i++)
			{
				if (speed < kPressBandSpeedLimits[i])
					return pressBands[i];
			}
			return pressBands[kPressBandCount - 1];
		}
	};

	struct StickPoint
	{
		float pitch = 0.f;
		float yaw = 0.f;
	};

	struct AttemptResult
	{
		bool make = false;
		Diagnosis diagnosis = Diagnosis::Unclassified;
		std::string diagnosisText;

		Surface surface = Surface::SideWall;
		WallSide wallSide = WallSide::Left;
		float gameSpeed = 1.f;
		bool spamInput = false;
		bool notFullyOnWall = false;

		int startFrame = 0;
		int wheelContactsAtStart = 0;
		int jumpPresses = 0;

		// Timings in ticks. -1 means it did not happen.
		int holdTicks = -1;
		int releaseToPressTicks = -1;
		int pressToPressTicks = -1;
		int firstTouchTicks = -1;
		int reattachTicks = -1;

		SecondPress secondPress = SecondPress::None;
		int wheelsAtSecondPress = -1;
		float stickPitch = kNotMeasured;
		float stickYaw = kNotMeasured;
		float stickRoll = kNotMeasured;
		float stickMagnitude = kNotMeasured;
		float stickSum = kNotMeasured; // |pitch| + |yaw| + |roll|, the value the game compares to the deadzone.
		bool pastDodgeDeadzone = false;

		// Stick direction at each of the three moments, using the flip angle
		// convention: positive is toward the ceiling, 0 is forward. Roll is left
		// out, so these show the stick alone.
		float stickAngleFirstPress = kNotMeasured;
		float stickAngleRelease = kNotMeasured;
		float stickAngleSecondPress = kNotMeasured;
		float stickSumFirstPress = kNotMeasured;
		float stickSumRelease = kNotMeasured;
		float stickSumSecondPress = kNotMeasured;

		float dodgeDeadzone = 0.f;
		float controllerDeadzone = 0.f;

		float flipAngleDeg = kNotMeasured;
		bool flipAngleUndefined = false;
		// Dodge direction in the car frame, for drawing. Air roll changes it, so
		// it can differ from the stick.
		float flipDirForward = kNotMeasured;
		float flipDirRight = kNotMeasured;
		float travelAngleDeg = kNotMeasured;
		float maxWallDistance = 0.f;
		float heightAtStart = 0.f;
		Vec3 normal;

		float speedFirstPress = kNotMeasured;
		float speedDodge = kNotMeasured;
		float speedReattach = kNotMeasured;
		float speedPeak = kNotMeasured;
		float speedGain = kNotMeasured;
		float speedEnd = kNotMeasured; // In the wall plane when the attempt ended, for makes and misses.

		// Full car speed including movement off the wall, which is what the
		// game's 2200 uu/s supersonic limit applies to.
		float totalSpeedFirstPress = kNotMeasured;
		float totalSpeedEnd = kNotMeasured;

		float throttleFraction = 0.f;
		float boostFraction = 0.f;
		float powerslideFraction = 0.f;
		float airRollFraction = 0.f;

		int maxWheels = 0;
		float carUpDotNormal = kNotMeasured;

		std::vector<StickPoint> stickPath;

		float HoldMs() const { return holdTicks < 0 ? kNotMeasured : holdTicks * kMsPerTick; }
		float ReleaseToPressMs() const { return releaseToPressTicks < 0 ? kNotMeasured : releaseToPressTicks * kMsPerTick; }
		float PressToPressMs() const { return pressToPressTicks < 0 ? kNotMeasured : pressToPressTicks * kMsPerTick; }
	};

	const char* ToString(SecondPress value);
	const char* ToString(Surface value);
	const char* ToString(WallSide value);
	const char* ToString(Diagnosis value);
}
