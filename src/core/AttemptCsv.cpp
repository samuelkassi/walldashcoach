#include "AttemptCsv.h"

#include <cmath>
#include <format>
#include <sstream>

namespace walldash
{
	namespace
	{
		constexpr float kNormalSpeedMin = 0.95f;

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

		std::string BucketText(const SessionStats::Bucket& bucket)
		{
			return std::format("{}/{} ({:.0f}%), avg {:.0f} ms", bucket.makes, bucket.attempts, bucket.MakePercent(), bucket.AveragePressToPressMs());
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
			"first_touch_ticks,reattach_ticks,max_wheels,car_up_dot_normal,chain_id,chain_index,stick_path";
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
			<< Number(a.carUpDotNormal, 3) << ",,," // Chain id and index are filled in by Phase 4.
			<< StickPath(a.stickPath);
		return row.str();
	}

	int AnonymizeAttemptCsv(std::istream& in, std::ostream& out, std::map<std::string, int>& sessions)
	{
		// Splits on commas outside quotes, keeping each field's text exactly as written.
		const auto split = [](const std::string& line)
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
			};

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

	void SessionStats::Add(const AttemptResult& a)
	{
		if (a.spamInput)
			return;

		const bool flat = a.surface == Surface::SideWall || a.surface == Surface::BackWall;
		const bool normalSpeed = a.gameSpeed >= kNormalSpeedMin;
		WallPair& pair = normalSpeed ? (flat ? flatNormal : otherNormal) : (flat ? flatSlow : otherSlow);
		Bucket& bucket = a.wallSide == WallSide::Right ? pair.right : pair.left;

		bucket.attempts++;
		if (a.make)
		{
			bucket.makes++;
			bucket.pressToPressTotalMs += a.PressToPressMs();
		}
	}

	std::vector<std::string> SessionStats::Lines(bool splitByWall) const
	{
		std::vector<std::string> lines;

		const auto addPair = [&](const char* label, const WallPair& pair, bool alwaysShow)
			{
				if (splitByWall)
				{
					if (alwaysShow || pair.left.attempts > 0)
						lines.push_back(std::format("{} left {}", label, BucketText(pair.left)));
					if (alwaysShow || pair.right.attempts > 0)
						lines.push_back(std::format("{} right {}", label, BucketText(pair.right)));
					return;
				}

				Bucket both;
				both.makes = pair.left.makes + pair.right.makes;
				both.attempts = pair.left.attempts + pair.right.attempts;
				both.pressToPressTotalMs = pair.left.pressToPressTotalMs + pair.right.pressToPressTotalMs;
				if (alwaysShow || both.attempts > 0)
					lines.push_back(std::format("{} {}", label, BucketText(both)));
			};

		addPair("Flat", flatNormal, true);
		addPair("Curves and corners", otherNormal, false);
		addPair("Slowed flat", flatSlow, false);
		addPair("Slowed curves and corners", otherSlow, false);
		return lines;
	}

	std::string SessionStats::Summary(bool splitByWall) const
	{
		std::string out;
		for (const std::string& line : Lines(splitByWall))
		{
			if (!out.empty())
				out += " | ";
			out += line;
		}
		return out;
	}
}
