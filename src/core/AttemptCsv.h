#pragma once

#include "Attempt.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace walldash
{
	// One header line, snake_case names, no units in values. A value that was
	// not measured is left blank, which pandas reads as NaN.
	std::string AttemptCsvHeader();
	std::string AttemptCsvRow(const AttemptResult& attempt, const std::string& timestamp, const std::string& sessionId, int attemptId);

	// Copies an attempt CSV with the timestamp column removed and session ids,
	// which contain the date and time, replaced by plain numbers. Pass the same
	// map for every file so sessions stay numbered in order. Returns rows written.
	int AnonymizeAttemptCsv(std::istream& in, std::ostream& out, std::map<std::string, int>& sessions);

	// Progress toward a sample of chain dashes: follow-up dashes on flat walls at
	// normal speed without air roll. Only attempts that logged the movement before
	// the press count, since that is what the sample is for.
	struct ChainSampleCounts
	{
		int left = 0;
		int right = 0;
	};
	bool CountsTowardChainSample(const AttemptResult& attempt);
	void AddChainSampleFromCsv(std::istream& in, ChainSampleCounts& counts);

	// What to do with dashes whose speed at the first press is above a limit.
	enum class FastDashMode { Include, Separate, Exclude };

	// True when the speed at the first press is above the limit. An attempt with
	// no measured speed is never fast.
	bool IsFastDash(const AttemptResult& attempt, float limit);

	struct StatsOptions
	{
		bool splitByWall = true;
		bool showSpeedGain = true;
		FastDashMode fastMode = FastDashMode::Include;
		float fastSpeed = 2100.f;
	};

	// Make counts split the way the overlay shows them: flat walls apart from
	// curves and corners, normal game speed apart from slowed down, optionally
	// fast dashes apart from the rest, and optionally left wall apart from right.
	// Each attempt is kept, so changing an option regroups the whole session.
	struct SessionStats
	{
		struct Bucket
		{
			int makes = 0;
			int attempts = 0;
			float pressToPressTotalMs = 0.f;
			float speedGainTotal = 0.f;
			int speedGainCount = 0;

			float MakePercent() const { return attempts == 0 ? 0.f : 100.f * makes / attempts; }
			float AveragePressToPressMs() const { return makes == 0 ? 0.f : pressToPressTotalMs / makes; }
			float AverageSpeedGain() const { return speedGainCount == 0 ? kNotMeasured : speedGainTotal / speedGainCount; }
		};

		struct Entry
		{
			bool make = false;
			bool flat = true;
			bool slow = false;
			WallSide wallSide = WallSide::Left;
			float speedFirstPress = kNotMeasured;
			float pressToPressMs = kNotMeasured;
			float speedGain = kNotMeasured;
		};

		std::vector<Entry> entries;

		// Spam attempts are left out.
		void Add(const AttemptResult& attempt);
		std::string Summary(const StatsOptions& options) const;
		// One line per bucket that has attempts. Normal speed flat walls always show.
		std::vector<std::string> Lines(const StatsOptions& options) const;
	};
}
