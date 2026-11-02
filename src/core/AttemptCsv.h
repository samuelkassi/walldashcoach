#pragma once

#include "Attempt.h"

#include <string>
#include <vector>

namespace walldash
{
	// One header line, snake_case names, no units in values. A value that was
	// not measured is left blank, which pandas reads as NaN.
	std::string AttemptCsvHeader();
	std::string AttemptCsvRow(const AttemptResult& attempt, const std::string& timestamp, const std::string& sessionId, int attemptId);

	// Make counts split the way the overlay will show them: flat walls apart
	// from curves and corners, and normal game speed apart from slowed down.
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

		Bucket flatNormal;
		Bucket otherNormal;
		Bucket flatSlow;
		Bucket otherSlow;

		// Spam attempts are left out.
		void Add(const AttemptResult& attempt);
		std::string Summary() const;
		// One line per bucket that has attempts. Normal speed flat walls always shows.
		std::vector<std::string> Lines() const;
	};
}
