#pragma once

#include "Attempt.h"

#include <deque>
#include <functional>

namespace walldash
{
	struct ChainStatus
	{
		int current = 0; // Makes in a row in the chain still going.
		int best = 0;
		float lastSpeedLost = kNotMeasured;
	};

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

		// The attempt in progress, for live display. Only valid while IsActive().
		bool IsActive() const { return active; }
		const AttemptResult& Current() const { return attempt; }
		int LatestFrame() const { return last.frame; }

		const ChainStatus& GetChainStatus() const { return chain; }
		void ResetChainRecords();

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
		void MeasurePrepress(const TickSample& sample);
		void BeginChainEntry();
		void FinishChainEntry();
		void EndChain();
		void UpdateChainWhileIdle(const TickSample& sample);

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

		std::deque<TickSample> history;

		ChainStatus chain;
		bool chainAlive = false;
		bool chainReachedSupersonic = false;
		int chainId = 0;
		int chainIndex = 0;
		int chainReattachFrame = -1;
		int chainAirTicks = 0;
		float chainLastPeak = kNotMeasured;
	};
}
