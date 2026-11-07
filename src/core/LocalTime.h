#pragma once

#include <ctime>
#include <string>

// Local time stamps for file names and CSV rows. Uses the C runtime
// (localtime_s) and not std::chrono::current_zone(), which throws on some
// Windows time zone setups. Nothing here throws on a bad clock or zone: it
// falls back to UTC, and then to the epoch.
namespace walldash
{
	inline constexpr const char* kSessionIdFormat = "%Y%m%d_%H%M%S";
	inline constexpr const char* kFileDateFormat = "%Y-%m-%d";
	inline constexpr const char* kCsvTimestampFormat = "%Y-%m-%dT%H:%M:%S";
	inline constexpr const char* kDebugLogNameFormat = "debug_ticks_%Y-%m-%d_%H%M%S.csv";

	// Broken down time for t, in local time, or UTC when local time fails.
	std::tm ToLocalTm(std::time_t t) noexcept;

	// strftime into a string. Returns an empty string only if the format fails.
	std::string FormatTm(const std::tm& tm, const char* format);

	// Current local time in the given strftime format.
	std::string FormatLocalNow(const char* format);
}
