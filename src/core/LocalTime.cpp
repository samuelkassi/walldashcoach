#include "core/LocalTime.h"

namespace walldash
{
	std::tm ToLocalTm(std::time_t t) noexcept
	{
		std::tm tm{};
		if (t != static_cast<std::time_t>(-1))
		{
			if (localtime_s(&tm, &t) == 0)
				return tm;
			if (gmtime_s(&tm, &t) == 0)
				return tm;
		}
		// No usable clock: 1970-01-01 00:00:00.
		tm = std::tm{};
		tm.tm_mday = 1;
		tm.tm_year = 70;
		return tm;
	}

	std::string FormatTm(const std::tm& tm, const char* format)
	{
		char buffer[128];
		const size_t length = std::strftime(buffer, sizeof(buffer), format, &tm);
		return std::string(buffer, length);
	}

	std::string FormatLocalNow(const char* format)
	{
		const std::time_t now = std::time(nullptr);
		std::string text = FormatTm(ToLocalTm(now), format);
		if (text.empty() && now != static_cast<std::time_t>(-1))
		{
			std::tm utc{};
			if (gmtime_s(&utc, &now) == 0)
				text = FormatTm(utc, format);
		}
		return text;
	}
}
