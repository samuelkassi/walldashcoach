// Checks that core/LocalTime gives the same strings as the std::chrono code
// it replaced in 1.1.2, so attempt CSVs, file names and session ids keep the
// layout that tools and older files expect.
//
//   TimeFormat
//
// Exits non-zero on any mismatch.

#include "core/LocalTime.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <format>
#include <regex>
#include <string>

using namespace walldash;

namespace
{
	int failures = 0;

	void Check(bool ok, const std::string& what)
	{
		if (!ok)
		{
			failures++;
			std::printf("FAIL %s\n", what.c_str());
		}
	}

	void CheckSame(const std::string& got, const std::string& expected, const std::string& what)
	{
		Check(got == expected, what + ": got '" + got + "', expected '" + expected + "'");
	}

	// The old code formatted a chrono time point floored to seconds. Using the
	// same instant in UTC on both sides takes the time zone out of the test.
	void CompareWithChrono(std::time_t t)
	{
		const auto chronoTime = std::chrono::sys_seconds(std::chrono::seconds(t));
		std::tm utc{};
		gmtime_s(&utc, &t);

		CheckSame(FormatTm(utc, kSessionIdFormat), std::format("{:%Y%m%d_%H%M%S}", chronoTime), "session id");
		CheckSame(FormatTm(utc, kFileDateFormat), std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(chronoTime)), "file date");
		CheckSame(FormatTm(utc, kCsvTimestampFormat), std::format("{:%Y-%m-%dT%H:%M:%S}", chronoTime), "csv timestamp");
		CheckSame(FormatTm(utc, kDebugLogNameFormat), std::format("debug_ticks_{:%Y-%m-%d_%H%M%S}.csv", chronoTime), "debug log name");
	}
}

int main()
{
	// Single digit fields, end of year, a leap day and a recent date.
	const std::time_t samples[] = { 0, 1704067199, 1709164805, 1735689599, 1793836800, 1794012345 };
	for (std::time_t t : samples)
		CompareWithChrono(t);

	// The live strings have the expected shape.
	Check(std::regex_match(FormatLocalNow(kSessionIdFormat), std::regex(R"(\d{8}_\d{6})")), "live session id shape");
	Check(std::regex_match(FormatLocalNow(kFileDateFormat), std::regex(R"(\d{4}-\d{2}-\d{2})")), "live file date shape");
	Check(std::regex_match(FormatLocalNow(kCsvTimestampFormat), std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})")), "live csv timestamp shape");
	Check(std::regex_match(FormatLocalNow(kDebugLogNameFormat), std::regex(R"(debug_ticks_\d{4}-\d{2}-\d{2}_\d{6}\.csv)")), "live debug log name shape");

	// A failed clock gives the epoch rather than garbage or a throw.
	CheckSame(FormatTm(ToLocalTm(static_cast<std::time_t>(-1)), kCsvTimestampFormat), "1970-01-01T00:00:00", "no clock fallback");

	// Where this PC's zone database works, the live local time matches the old code.
	try
	{
		for (int tries = 0; tries < 3; tries++)
		{
			const auto zoned = std::chrono::zoned_time(std::chrono::current_zone(), std::chrono::system_clock::now());
			const std::string old = std::format("{:%Y-%m-%dT%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(zoned.get_local_time()));
			const std::string now = FormatLocalNow(kCsvTimestampFormat);
			if (old == now || tries == 2)
			{
				CheckSame(now, old, "live local time against current_zone");
				break;
			}
		}
	}
	catch (const std::exception& e)
	{
		std::printf("skip live local time check, current_zone failed here: %s\n", e.what());
	}

	if (failures == 0)
		std::printf("TimeFormat: all checks passed\n");
	return failures == 0 ? 0 : 1;
}
