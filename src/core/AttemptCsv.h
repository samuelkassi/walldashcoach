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

	// Make counts split the way the overlay shows them: flat walls apart from
	// curves and corners, normal game speed apart from slowed down, and optionally
	// left wall apart from right wall.
	struct SessionStats
	{
		struct Bucket
		{
			int makes = 0;
			int attempts = 0;
			float pressToPressTotalMs = 0.f;

			float MakePercent() const { return attempts == 0 ? 0.f : 100.f * makes / attempts; }
			float AveragePressToPressMs() const { return makes == 0 ? 0.f : pressToPressTotalMs / makes; }
		};

		// A left wall and a right wall bucket for one kind of attempt.
		struct WallPair
		{
			Bucket left;
			Bucket right;
		};

		WallPair flatNormal;
		WallPair otherNormal;
		WallPair flatSlow;
		WallPair otherSlow;

		// Spam attempts are left out.
		void Add(const AttemptResult& attempt);
		std::string Summary(bool splitByWall) const;
		// One line per bucket that has attempts. Normal speed flat walls always show.
		std::vector<std::string> Lines(bool splitByWall) const;
	};
}
