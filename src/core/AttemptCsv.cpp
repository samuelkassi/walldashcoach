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
			"second_press_result,stick_pitch,stick_yaw,stick_roll,stick_magnitude,stick_sum,past_dodge_deadzone,dodge_deadzone,controller_deadzone,"
			"flip_angle_deg,flip_angle_undefined,travel_angle_deg,max_wall_distance,height_at_start,normal_x,normal_y,normal_z,"
			"speed_first_press,speed_dodge,speed_reattach,speed_peak,speed_gain,"
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
			<< ToString(a.secondPress) << ','
			<< Number(a.stickPitch, 3) << ',' << Number(a.stickYaw, 3) << ',' << Number(a.stickRoll, 3) << ','
			<< Number(a.stickMagnitude, 3) << ',' << Number(a.stickSum, 3) << ',' << (a.pastDodgeDeadzone ? 1 : 0) << ','
			<< Number(a.dodgeDeadzone) << ',' << Number(a.controllerDeadzone) << ','
			<< Number(a.flipAngleDeg, 1) << ',' << (a.flipAngleUndefined ? 1 : 0) << ',' << Number(a.travelAngleDeg, 1) << ','
			<< Number(a.maxWallDistance, 1) << ',' << Number(a.heightAtStart, 1) << ','
			<< Number(a.normal.x, 3) << ',' << Number(a.normal.y, 3) << ',' << Number(a.normal.z, 3) << ','
			<< Number(a.speedFirstPress, 1) << ',' << Number(a.speedDodge, 1) << ',' << Number(a.speedReattach, 1) << ','
			<< Number(a.speedPeak, 1) << ',' << Number(a.speedGain, 1) << ','
			<< Number(a.throttleFraction, 3) << ',' << Number(a.boostFraction, 3) << ','
			<< Number(a.powerslideFraction, 3) << ',' << Number(a.airRollFraction, 3) << ','
			<< Ticks(a.firstTouchTicks) << ',' << Ticks(a.reattachTicks) << ',' << a.maxWheels << ','
			<< Number(a.carUpDotNormal, 3) << ",,," // Chain id and index are filled in by Phase 4.
			<< StickPath(a.stickPath);
		return row.str();
	}

	void SessionStats::Add(const AttemptResult& a)
	{
		if (a.spamInput)
			return;

		const bool flat = a.surface == Surface::SideWall || a.surface == Surface::BackWall;
		const bool normalSpeed = a.gameSpeed >= kNormalSpeedMin;
		Bucket& bucket = normalSpeed ? (flat ? flatNormal : otherNormal) : (flat ? flatSlow : otherSlow);

		bucket.attempts++;
		if (a.make)
		{
			bucket.makes++;
			bucket.pressToPressTotalMs += a.PressToPressMs();
		}
	}

	std::string SessionStats::Summary() const
	{
		std::string out = std::format("Flat {}", BucketText(flatNormal));
		if (otherNormal.attempts > 0)
			out += std::format(" | Curves and corners {}", BucketText(otherNormal));
		if (flatSlow.attempts > 0)
			out += std::format(" | Slowed flat {}", BucketText(flatSlow));
		if (otherSlow.attempts > 0)
			out += std::format(" | Slowed curves and corners {}", BucketText(otherSlow));
		return out;
	}
}
