// Runs recorded debug logs through the attempt logic without the game.
//
//   ReplayLogs <debug_ticks.csv> [--speed 0.5] [--out attempts.csv] [--expect-left 6/12] [--expect-right 10/10]
//              [--expect-diagnosis too_early=1]
//
// With --expect-*, exits non-zero when the counts differ, so it works as a test.

#include "core/AttemptCsv.h"
#include "core/AttemptTracker.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace walldash;

namespace
{
	std::vector<std::string> SplitCommas(const std::string& line)
	{
		std::vector<std::string> fields;
		std::string field;
		std::istringstream stream(line);
		while (std::getline(stream, field, ','))
			fields.push_back(field);
		return fields;
	}

	class Row
	{
	public:
		Row(const std::map<std::string, size_t>& columns, std::vector<std::string> fields)
			: columns(columns), fields(std::move(fields))
		{
		}

		float Float(const std::string& name) const { return std::stof(fields.at(columns.at(name))); }
		int Int(const std::string& name) const { return std::stoi(fields.at(columns.at(name))); }
		const std::string& Text(const std::string& name) const { return fields.at(columns.at(name)); }
		Vec3 Vector(const std::string& prefix) const { return { Float(prefix + "_x"), Float(prefix + "_y"), Float(prefix + "_z") }; }

	private:
		const std::map<std::string, size_t>& columns;
		std::vector<std::string> fields;
	};

	TickSample ToSample(const Row& row, float gameSpeed)
	{
		TickSample s;
		s.frame = row.Int("physics_frame");
		s.gameSpeed = gameSpeed;
		s.throttle = row.Float("throttle");
		s.pitch = row.Float("pitch");
		s.yaw = row.Float("yaw");
		s.roll = row.Float("roll");
		s.powerslide = row.Int("handbrake") != 0;
		s.boost = row.Int("holding_boost") != 0 || row.Int("activate_boost") != 0;
		s.pos = row.Vector("pos");
		s.vel = row.Vector("vel");
		s.right = row.Vector("right");
		s.up = row.Vector("up");
		s.wheelContacts = row.Int("num_wheel_world_contacts");
		s.isOnWall = row.Int("is_on_wall") != 0;
		s.dodgeDir = row.Vector("dodge_dir");
		s.dodgeDeadzone = row.Float("gamepad_dodge_threshold");
		s.controllerDeadzone = row.Float("gamepad_controller_deadzone");
		return s;
	}

	bool CheckExpected(const char* side, const std::string& expected, int makes, int attempts)
	{
		if (expected.empty())
			return true;
		const std::string actual = std::to_string(makes) + "/" + std::to_string(attempts);
		const bool ok = actual == expected;
		std::cout << (ok ? "PASS " : "FAIL ") << side << " wall: expected " << expected << ", got " << actual << "\n";
		return ok;
	}
}

int main(int argc, char** argv)
{
	if (argc == 4 && std::string(argv[1]) == "--anonymize")
	{
		std::ifstream in(argv[2]);
		std::ofstream out(argv[3], std::ios::trunc);
		std::map<std::string, int> sessions;
		std::cout << AnonymizeAttemptCsv(in, out, sessions) << " rows, " << sessions.size() << " sessions\n";
		return 0;
	}

	if (argc == 3 && std::string(argv[1]) == "--count-chain-sample")
	{
		std::ifstream in(argv[2]);
		ChainSampleCounts counts;
		AddChainSampleFromCsv(in, counts);
		std::cout << "left " << counts.left << " right " << counts.right << "\n";
		return 0;
	}

	if (argc < 2)
	{
		std::cerr << "Usage: ReplayLogs <debug_ticks.csv> [--speed 0.5] [--out attempts.csv] [--expect-left 6/12] [--expect-right 10/10]\n";
		return 2;
	}

	float gameSpeed = 1.f;
	std::string outPath;
	std::string expectLeft;
	std::string expectRight;
	std::vector<std::string> expectDiagnoses;
	for (int i = 2; i + 1 < argc; i += 2)
	{
		const std::string flag = argv[i];
		if (flag == "--speed") gameSpeed = std::stof(argv[i + 1]);
		else if (flag == "--out") outPath = argv[i + 1];
		else if (flag == "--expect-left") expectLeft = argv[i + 1];
		else if (flag == "--expect-right") expectRight = argv[i + 1];
		else if (flag == "--expect-diagnosis") expectDiagnoses.push_back(argv[i + 1]);
		else
		{
			std::cerr << "Unknown option: " << flag << "\n";
			return 2;
		}
	}

	std::ifstream log(argv[1]);
	if (!log)
	{
		std::cerr << "Could not open " << argv[1] << "\n";
		return 2;
	}

	std::ofstream out;
	if (!outPath.empty())
	{
		out.open(outPath, std::ios::trunc);
		out << AttemptCsvHeader() << "\n";
	}

	SessionStats stats;
	int makes[2] = { 0, 0 };
	int attempts[2] = { 0, 0 };
	int attemptId = 0;
	std::map<std::string, int> diagnosisCounts;
	int bestChain = 0;
	ChainSampleCounts liveSample;

	AttemptTracker tracker([&](const AttemptResult& a)
		{
			attemptId++;
			diagnosisCounts[ToString(a.diagnosis)]++;
			const int side = a.wallSide == WallSide::Right ? 1 : 0;
			if (!a.spamInput)
			{
				attempts[side]++;
				if (a.make) makes[side]++;
			}
			stats.Add(a);

			bestChain = std::max(bestChain, a.make ? a.chainIndex : a.chainIndex - 1);
			if (CountsTowardChainSample(a))
				(a.wallSide == WallSide::Right ? liveSample.right : liveSample.left)++;

			char line[256];
			std::snprintf(line, sizeof(line), "%3d chain %3d.%-2d lost %6.1f %-5s %-9s %-4s p2p %3d ticks  flip %6.1f  %s", attemptId, a.chainId, a.chainIndex, a.speedLostSinceLast,
				ToString(a.wallSide), ToString(a.surface), a.make ? "MAKE" : "miss", a.pressToPressTicks, a.flipAngleDeg, a.make ? "" : a.diagnosisText.c_str());
			std::cout << line << "\n";
			if (out.is_open())
				out << AttemptCsvRow(a, "replay", "replay", attemptId) << "\n";
		});

	std::string line;
	std::getline(log, line);
	std::map<std::string, size_t> columns;
	{
		const std::vector<std::string> names = SplitCommas(line);
		for (size_t i = 0; i < names.size(); i++)
			columns[names[i]] = i;
	}

	while (std::getline(log, line))
	{
		std::vector<std::string> fields = SplitCommas(line);
		if (fields.size() != columns.size())
			continue; // The last line can be cut short if the log was still being written.

		const Row row(columns, std::move(fields));
		const std::string& kind = row.Text("kind");
		if (kind == "tick") tracker.OnTick(ToSample(row, gameSpeed));
		else if (kind == "jump_pressed") tracker.OnJumpPressed();
		else if (kind == "jump_released") tracker.OnJumpReleased();
		else if (kind == "jump_begin") tracker.OnFirstJumpBegin();
		else if (kind == "double_jump_begin") tracker.OnDoubleJumpBegin();
		else if (kind == "dodge_begin") tracker.OnDodgeBegin();
	}

	std::cout << "\n" << stats.Summary(StatsOptions{}) << "\n";
	std::cout << "Best chain " << bestChain << "\n";
	std::cout << "Chain sample left " << liveSample.left << " right " << liveSample.right << "\n";
	std::cout << "Left wall " << makes[0] << "/" << attempts[0] << ", right wall " << makes[1] << "/" << attempts[1] << "\n";

	const bool leftOk = CheckExpected("left", expectLeft, makes[0], attempts[0]);
	const bool rightOk = CheckExpected("right", expectRight, makes[1], attempts[1]);

	bool diagnosesOk = true;
	for (const std::string& expected : expectDiagnoses)
	{
		const size_t split = expected.find('=');
		const std::string name = expected.substr(0, split);
		const std::string actual = name + "=" + std::to_string(diagnosisCounts[name]);
		const bool ok = actual == expected;
		std::cout << (ok ? "PASS " : "FAIL ") << "diagnosis: expected " << expected << ", got " << actual << "\n";
		diagnosesOk = diagnosesOk && ok;
	}

	return leftOk && rightOk && diagnosesOk ? 0 : 1;
}
