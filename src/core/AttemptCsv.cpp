#include "AttemptCsv.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <istream>
#include <ostream>
#include <sstream>

namespace walldash
{
	namespace
	{
		constexpr float kNormalSpeedMin = 0.95f;

		// Splits on commas outside quotes, keeping each field's text exactly as written.
		std::vector<std::string> SplitCsvLine(const std::string& line)
		{
			std::vector<std::string> fields(1);
			bool quoted = false;
			for (char c : line)
			{
				if (c == '"')
					quoted = !quoted;
				if (c == ',' && !quoted)
					fields.emplace_back();
				else
					fields.back() += c;
			}
			return fields;
		}

		std::string Number(float value, int decimals = 2)
		{
			return std::isnan(value) ? std::string() : std::format("{:.{}f}", value, decimals);
		}

		std::string Ticks(int value)
		{
			return value < 0 ? std::string() : std::to_string(value);
		}

		std::string Quoted(const std::string& text)
		{
			std::string out = "\"";
			for (char c : text)
			{
				if (c == '"')
					out += '"';
				out += c;
			}
			out += '"';
			return out;
		}

		std::string StickPath(const std::vector<StickPoint>& path)
		{
			std::string out;
			for (const StickPoint& point : path)
			{
				if (!out.empty())
					out += ';';
				out += std::format("{:.3f}:{:.3f}", point.pitch, point.yaw);
			}
			return out;
		}

		std::string BucketText(const SessionStats::Bucket& bucket, bool showSpeedGain)
		{
			std::string text = std::format("{}/{} ({:.2f}%), avg {:.0f} ms", bucket.makes, bucket.attempts, bucket.MakePercent(), bucket.AveragePressToPressMs());
			if (showSpeedGain)
			{
				const float gain = bucket.AverageSpeedGain();
				text += std::isnan(gain) ? ", avg gain none" : std::format(", avg gain {:+.0f} uu/s", gain);
			}
			return text;
		}
	}

	std::string AttemptCsvHeader()
	{
		return "timestamp,session_id,attempt_id,result,diagnosis,diagnosis_text,surface,wall_side,game_speed,spam_input,not_fully_on_wall,"
			"start_frame,wheel_contacts_at_start,jump_presses,"
			"hold_ticks,hold_ms,release_to_press_ticks,release_to_press_ms,press_to_press_ticks,press_to_press_ms,"
			"second_press_result,wheels_at_second_press,stick_pitch,stick_yaw,stick_roll,stick_magnitude,stick_sum,past_dodge_deadzone,"
			"stick_angle_first_press,stick_angle_release,stick_angle_second_press,stick_sum_first_press,stick_sum_release,stick_sum_second_press,"
			"dodge_deadzone,controller_deadzone,"
			"flip_angle_deg,flip_angle_undefined,travel_angle_deg,max_wall_distance,height_at_start,normal_x,normal_y,normal_z,"
			"speed_first_press,speed_dodge,speed_reattach,speed_peak,speed_gain,speed_end,total_speed_first_press,total_speed_end,"
			"throttle_fraction,boost_fraction,powerslide_fraction,air_roll_fraction,"
			"first_touch_ticks,reattach_ticks,max_wheels,car_up_dot_normal,chain_id,chain_index,speed_lost_since_last,dashes_to_supersonic,max_total_speed,"
			"prepress_ticks,travel_angle_prepress,travel_angle_prepress_min,turn_size_prepress,height_drop_prepress,speed_change_prepress,wheels_min_prepress,normal_speed_at_press,"
			"stick_path";
	}

	std::string AttemptCsvRow(const AttemptResult& a, const std::string& timestamp, const std::string& sessionId, int attemptId)
	{
		std::ostringstream row;
		row << timestamp << ',' << sessionId << ',' << attemptId << ','
			<< (a.make ? "make" : "miss") << ',' << ToString(a.diagnosis) << ',' << Quoted(a.diagnosisText) << ','
			<< ToString(a.surface) << ',' << ToString(a.wallSide) << ',' << Number(a.gameSpeed) << ','
			<< (a.spamInput ? 1 : 0) << ',' << (a.notFullyOnWall ? 1 : 0) << ','
			<< a.startFrame << ',' << a.wheelContactsAtStart << ',' << a.jumpPresses << ','
			<< Ticks(a.holdTicks) << ',' << Number(a.HoldMs()) << ','
			<< Ticks(a.releaseToPressTicks) << ',' << Number(a.ReleaseToPressMs()) << ','
			<< Ticks(a.pressToPressTicks) << ',' << Number(a.PressToPressMs()) << ','
			<< ToString(a.secondPress) << ',' << Ticks(a.wheelsAtSecondPress) << ','
			<< Number(a.stickPitch, 3) << ',' << Number(a.stickYaw, 3) << ',' << Number(a.stickRoll, 3) << ','
			<< Number(a.stickMagnitude, 3) << ',' << Number(a.stickSum, 3) << ',' << (a.pastDodgeDeadzone ? 1 : 0) << ','
			<< Number(a.stickAngleFirstPress, 1) << ',' << Number(a.stickAngleRelease, 1) << ',' << Number(a.stickAngleSecondPress, 1) << ','
			<< Number(a.stickSumFirstPress, 3) << ',' << Number(a.stickSumRelease, 3) << ',' << Number(a.stickSumSecondPress, 3) << ','
			<< Number(a.dodgeDeadzone) << ',' << Number(a.controllerDeadzone) << ','
			<< Number(a.flipAngleDeg, 1) << ',' << (a.flipAngleUndefined ? 1 : 0) << ',' << Number(a.travelAngleDeg, 1) << ','
			<< Number(a.maxWallDistance, 1) << ',' << Number(a.heightAtStart, 1) << ','
			<< Number(a.normal.x, 3) << ',' << Number(a.normal.y, 3) << ',' << Number(a.normal.z, 3) << ','
			<< Number(a.speedFirstPress, 1) << ',' << Number(a.speedDodge, 1) << ',' << Number(a.speedReattach, 1) << ','
			<< Number(a.speedPeak, 1) << ',' << Number(a.speedGain, 1) << ','
			<< Number(a.speedEnd, 1) << ',' << Number(a.totalSpeedFirstPress, 1) << ',' << Number(a.totalSpeedEnd, 1) << ','
			<< Number(a.throttleFraction, 3) << ',' << Number(a.boostFraction, 3) << ','
			<< Number(a.powerslideFraction, 3) << ',' << Number(a.airRollFraction, 3) << ','
			<< Ticks(a.firstTouchTicks) << ',' << Ticks(a.reattachTicks) << ',' << a.maxWheels << ','
			<< Number(a.carUpDotNormal, 3) << ','
			<< a.chainId << ',' << a.chainIndex << ',' << Number(a.speedLostSinceLast, 1) << ',' << Ticks(a.dashesToSupersonic) << ',' << Number(a.maxTotalSpeed, 1) << ','
			<< a.prepressTicks << ',' << Number(a.travelAnglePrepress, 1) << ',' << Number(a.travelAnglePrepressMin, 1) << ',' << Number(a.turnSizePrepress, 1) << ','
			<< Number(a.heightDropPrepress, 1) << ',' << Number(a.speedChangePrepress, 1) << ',' << Ticks(a.wheelsMinPrepress) << ',' << Number(a.normalSpeedAtPress, 1) << ','
			<< StickPath(a.stickPath);
		return row.str();
	}

	bool CountsTowardChainSample(const AttemptResult& a)
	{
		const bool flat = a.surface == Surface::SideWall || a.surface == Surface::BackWall;
		return flat && a.gameSpeed >= kNormalSpeedMin && !a.spamInput && a.airRollFraction == 0.f
			&& a.secondPress == SecondPress::Dodge && a.chainIndex >= 2;
	}

	void AddChainSampleFromCsv(std::istream& in, ChainSampleCounts& counts)
	{
		std::string line;
		if (!std::getline(in, line))
			return;

		const std::vector<std::string> header = SplitCsvLine(line);
		const auto column = [&](const char* name)
			{
				const auto found = std::find(header.begin(), header.end(), name);
				return found == header.end() ? -1 : static_cast<int>(found - header.begin());
			};
		const int surface = column("surface");
		const int side = column("wall_side");
		const int speed = column("game_speed");
		const int spam = column("spam_input");
		const int airRoll = column("air_roll_fraction");
		const int second = column("second_press_result");
		const int chainIndex = column("chain_index");
		// Older files have no movement before the press, so they do not count.
		if (column("turn_size_prepress") < 0 || surface < 0 || side < 0 || speed < 0 || spam < 0 || airRoll < 0 || second < 0 || chainIndex < 0)
			return;

		const auto number = [](const std::string& text)
			{
				return text.empty() ? 0.f : std::strtof(text.c_str(), nullptr);
			};
		while (std::getline(in, line))
		{
			const std::vector<std::string> f = SplitCsvLine(line);
			if (f.size() != header.size())
				continue;
			const bool flat = f[surface] == "side_wall" || f[surface] == "back_wall";
			if (flat && number(f[speed]) >= kNormalSpeedMin && number(f[spam]) == 0.f && number(f[airRoll]) == 0.f
				&& f[second] == "dodge" && number(f[chainIndex]) >= 2.f)
			{
				(f[side] == "right" ? counts.right : counts.left)++;
			}
		}
	}

	int AnonymizeAttemptCsv(std::istream& in, std::ostream& out, std::map<std::string, int>& sessions)
	{
		const auto split = SplitCsvLine;

		std::string line;
		if (!std::getline(in, line))
			return 0;

		const std::vector<std::string> header = split(line);
		int timestampColumn = -1;
		int sessionColumn = -1;
		for (int i = 0; i < static_cast<int>(header.size()); i++)
		{
			if (header[i] == "timestamp") timestampColumn = i;
			if (header[i] == "session_id") sessionColumn = i;
		}

		const auto writeRow = [&](const std::vector<std::string>& fields)
			{
				bool first = true;
				for (int i = 0; i < static_cast<int>(fields.size()); i++)
				{
					if (i == timestampColumn)
						continue;
					if (!first)
						out << ',';
					first = false;
					out << fields[i];
				}
				out << '\n';
			};

		std::vector<std::string> newHeader = header;
		if (sessionColumn >= 0)
			newHeader[sessionColumn] = "session";
		writeRow(newHeader);

		int rows = 0;
		while (std::getline(in, line))
		{
			std::vector<std::string> fields = split(line);
			if (fields.size() != header.size())
				continue;
			if (sessionColumn >= 0)
			{
				const auto inserted = sessions.try_emplace(fields[sessionColumn], static_cast<int>(sessions.size()) + 1);
				fields[sessionColumn] = std::to_string(inserted.first->second);
			}
			writeRow(fields);
			rows++;
		}
		return rows;
	}

	bool IsFastDash(const AttemptResult& a, float limit)
	{
		return !std::isnan(a.speedFirstPress) && a.speedFirstPress > limit;
	}

	void SessionStats::Add(const AttemptResult& a)
	{
		if (a.spamInput)
			return;

		Entry entry;
		entry.make = a.make;
		entry.flat = a.surface == Surface::SideWall || a.surface == Surface::BackWall;
		entry.slow = a.gameSpeed < kNormalSpeedMin;
		entry.wallSide = a.wallSide;
		entry.speedFirstPress = a.speedFirstPress;
		entry.pressToPressMs = a.PressToPressMs();
		entry.speedGain = a.speedGain;
		entries.push_back(entry);
	}

	std::vector<std::string> SessionStats::Lines(const StatsOptions& options) const
	{
		// Indexed by [slow][fast][curve or corner][right wall].
		Bucket buckets[2][2][2][2] = {};
		for (const Entry& e : entries)
		{
			const bool fast = !std::isnan(e.speedFirstPress) && e.speedFirstPress > options.fastSpeed;
			if (fast && options.fastMode == FastDashMode::Exclude)
				continue;
			const bool separate = fast && options.fastMode == FastDashMode::Separate;

			Bucket& bucket = buckets[e.slow][separate][!e.flat][e.wallSide == WallSide::Right];
			bucket.attempts++;
			if (!e.make)
				continue;
			bucket.makes++;
			if (!std::isnan(e.pressToPressMs))
				bucket.pressToPressTotalMs += e.pressToPressMs;
			if (!std::isnan(e.speedGain))
			{
				bucket.speedGainTotal += e.speedGain;
				bucket.speedGainCount++;
			}
		}

		std::vector<std::string> lines;
		const auto addPair = [&](const std::string& label, const Bucket& left, const Bucket& right, bool alwaysShow)
			{
				if (options.splitByWall)
				{
					if (alwaysShow || left.attempts > 0)
						lines.push_back(std::format("{} left {}", label, BucketText(left, options.showSpeedGain)));
					if (alwaysShow || right.attempts > 0)
						lines.push_back(std::format("{} right {}", label, BucketText(right, options.showSpeedGain)));
					return;
				}

				Bucket both;
				both.makes = left.makes + right.makes;
				both.attempts = left.attempts + right.attempts;
				both.pressToPressTotalMs = left.pressToPressTotalMs + right.pressToPressTotalMs;
				both.speedGainTotal = left.speedGainTotal + right.speedGainTotal;
				both.speedGainCount = left.speedGainCount + right.speedGainCount;
				if (alwaysShow || both.attempts > 0)
					lines.push_back(std::format("{} {}", label, BucketText(both, options.showSpeedGain)));
			};

		const std::string fastSuffix = std::format(" over {:.0f} uu/s", options.fastSpeed);
		for (int slow = 0; slow < 2; slow++)
		{
			for (int fast = 0; fast < 2; fast++)
			{
				const std::string prefix = slow ? "Slowed " : "";
				const std::string suffix = fast ? fastSuffix : "";
				const auto& group = buckets[slow][fast];
				addPair(prefix + (slow ? "flat" : "Flat") + suffix, group[0][0], group[0][1], !slow && !fast);
				addPair(prefix + (slow ? "curves and corners" : "Curves and corners") + suffix, group[1][0], group[1][1], false);
			}
		}
		return lines;
	}

	std::string SessionStats::Summary(const StatsOptions& options) const
	{
		std::string out;
		for (const std::string& line : Lines(options))
		{
			if (!out.empty())
				out += " | ";
			out += line;
		}
		return out;
	}
}
