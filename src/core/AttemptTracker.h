#pragma once

#include "Attempt.h"

#include <functional>

namespace walldash
{
	// Turns ticks and jump events into finished attempts. On each physics frame,
	// call OnTick first and then any events for that frame, which is the order
	// the game fires them in.
	class AttemptTracker
	{
	public:
		using Callback = std::function<void(const AttemptResult&)>;

		explicit AttemptTracker(Callback onFinished);

		void SetConfig(const AttemptConfig& newConfig) { config = newConfig; }
		const AttemptConfig& GetConfig() const { return config; }

		void OnTick(const TickSample& sample);
		void OnJumpPressed();
		void OnJumpReleased();
		void OnFirstJumpBegin();
		void OnDoubleJumpBegin();
		void OnDodgeBegin();

		// Drops any attempt in progress without reporting it.
		void Reset();

	private:
		void StartAttempt(const TickSample& sample);
		void RecordInputs(const TickSample& sample);
		void CaptureFlipAngle(const TickSample& sample);
		void UpdateContact(const TickSample& sample);
		void MeasureSecondPress(const TickSample& sample);
		void ClearSecondPressMeasurements();
		void RecordSecondPress(SecondPress result);
		void Finish();
		void Diagnose();
		float SpeedInWallPlane(const TickSample& sample) const;

		AttemptConfig config;
		Callback onFinished;

		TickSample last;
		bool hasLast = false;

		bool active = false;
		AttemptResult attempt;
		Vec3 startPos;
		int ticksInAttempt = 0;
		int throttleTicks = 0;
		int boostTicks = 0;
		int powerslideTicks = 0;
		int airRollTicks = 0;
		int releaseFrame = -1;
		int lastPressFrame = -1;
		int measuredPressFrame = -1;
		int dodgeFrame = -1;
		Vec3 rightAtDodge;
		bool flipCaptured = false;
		bool leftWall = false;
		int firstTouchFrame = -1;
		int reattachFrame = -1;
		bool settled = false;
	};
}
