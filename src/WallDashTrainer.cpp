#include "pch.h"
#include "WallDashTrainer.h"

#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/Engine/EngineTAWrapper.h"
#include "bakkesmod/wrappers/Engine/WorldInfoWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/DodgeComponentWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/DoubleJumpComponentWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/JumpComponentWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/VehicleSimWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/WheelWrapper.h"
#include "bakkesmod/wrappers/SettingsWrapper.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <sstream>

BAKKESMOD_PLUGIN(WallDashTrainer, "WallDash Trainer", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

namespace
{
	using walldash::AttemptResult;
	using walldash::Vec3;

	constexpr int kWheelCount = 4;
	constexpr float kUnrealRotToRad = 3.14159265358979f / 32768.f;
	constexpr int kTicksPerSecond = 120;
	constexpr int kGameSpeedWindowTicks = 120;
	constexpr double kGameSpeedMaxWindowMs = 10000.0; // Longer than this means the game was paused.

	struct CarAxes
	{
		Vector forward;
		Vector right;
		Vector up;
	};

	// Unreal's rotator to axes conversion. Phase 0 checked it against the
	// wheel contact normals.
	CarAxes AxesFromRotator(const Rotator& r)
	{
		const float sp = std::sin(r.Pitch * kUnrealRotToRad);
		const float cp = std::cos(r.Pitch * kUnrealRotToRad);
		const float sy = std::sin(r.Yaw * kUnrealRotToRad);
		const float cy = std::cos(r.Yaw * kUnrealRotToRad);
		const float sr = std::sin(r.Roll * kUnrealRotToRad);
		const float cr = std::cos(r.Roll * kUnrealRotToRad);

		CarAxes axes;
		axes.forward = Vector(cp * cy, cp * sy, sp);
		axes.right = Vector(sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp);
		axes.up = Vector(-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp);
		return axes;
	}

	Vec3 ToVec3(const Vector& v)
	{
		return { v.X, v.Y, v.Z };
	}

	void AppendVector(std::ostringstream& out, const Vector& v)
	{
		out << ',' << v.X << ',' << v.Y << ',' << v.Z;
	}

	std::string DebugLogHeader()
	{
		std::ostringstream h;
		h << "kind,physics_frame,calls_this_frame,time_s,"
			"throttle,steer,pitch,yaw,roll,dodge_forward,dodge_strafe,"
			"handbrake,jump,activate_boost,holding_boost,input_jumped,"
			"pos_x,pos_y,pos_z,vel_x,vel_y,vel_z,ang_x,ang_y,ang_z,"
			"rot_pitch,rot_yaw,rot_roll,"
			"fwd_x,fwd_y,fwd_z,right_x,right_y,right_z,up_x,up_y,up_z,"
			"b_on_ground,is_on_ground,is_on_wall,num_wheel_world_contacts,"
			"b_jumped,b_double_jumped,has_flip,is_dodging,"
			"jump_active,double_jump_active,dodge_active,"
			"dodge_dir_x,dodge_dir_y,dodge_dir_z,dodge_torque_x,dodge_torque_y,dodge_torque_z";
		for (int i = 0; i < kWheelCount; i++)
		{
			h << ",wheel" << i << "_contact,wheel" << i << "_normal_x,wheel" << i << "_normal_y,wheel" << i << "_normal_z";
		}
		h << ",gamepad_dodge_threshold,gamepad_controller_deadzone,gamepad_air_control_sensitivity,"
			"pri_dodge_threshold,component_dodge_threshold";
		return h.str();
	}

	std::chrono::local_time<std::chrono::seconds> LocalNow()
	{
		const auto zoned = std::chrono::zoned_time(std::chrono::current_zone(), std::chrono::system_clock::now());
		return std::chrono::floor<std::chrono::seconds>(zoned.get_local_time());
	}

	std::string DescribeAttempt(const AttemptResult& a)
	{
		std::string line = std::format("WallDash: {} {} {}", a.make ? "MAKE" : "MISS", walldash::ToString(a.wallSide), walldash::ToString(a.surface));
		if (a.gameSpeed < 0.95f || a.gameSpeed > 1.05f)
			line += std::format(" at {:.0f}% speed", a.gameSpeed * 100.f);
		if (a.spamInput)
			line += std::format(" [spam_input, {} presses, left out of stats]", a.jumpPresses);
		if (!a.make)
			line += std::format(" | {}", a.diagnosisText);

		if (a.pressToPressTicks >= 0)
			line += std::format(" | press to press {:.0f} ms ({} ticks)", a.PressToPressMs(), a.pressToPressTicks);
		if (a.holdTicks >= 0)
			line += std::format(", hold {:.0f} ms", a.HoldMs());
		if (a.flipAngleUndefined)
			line += ", flip angle undefined";
		else if (!std::isnan(a.flipAngleDeg))
			line += std::format(", flip {:.0f} deg", a.flipAngleDeg);
		if (a.make)
			line += std::format(", speed {:+.0f} uu/s ({:.0f} to {:.0f})", a.speedGain, a.speedFirstPress, a.speedPeak);
		return line;
	}
}

void WallDashTrainer::onLoad()
{
	_globalCvarManager = cvarManager;

	sessionId = std::format("{:%Y%m%d_%H%M%S}", LocalNow());
	speedWindowStart = std::chrono::steady_clock::now();

	RegisterCvars();
	LoadChainSampleCounts();

	cvarManager->registerNotifier("walldash_reset_stats",
		[this](std::vector<std::string>) { ResetSessionStats(); },
		"Clear the session stats and the last result", PERMISSION_ALL);

	// Deliberately command only and not in the settings window. It writes files
	// on this PC and sends nothing.
	cvarManager->registerNotifier("walldash_export_share_data",
		[this](std::vector<std::string>) { ExportShareData(); },
		"Write an anonymised copy of your attempt files, for you to share if you choose. Nothing is sent", PERMISSION_ALL);

	cvarManager->registerNotifier("walldash_debug_status",
		[this](std::vector<std::string>) { PrintStatus(); },
		"Print the session stats, log paths and how many times each event fired", PERMISSION_ALL);

	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper car, void* params, std::string)
		{
			if (params)
				OnVehicleInput(car, *static_cast<ControllerInput*>(params));
		});

	HookCarEvent("Function TAGame.Car_TA.OnJumpPressed", "jump_pressed");
	HookCarEvent("Function TAGame.Car_TA.OnJumpReleased", "jump_released");

	HookComponentEvent("Function CarComponent_Jump_TA.Active.BeginState", "jump_begin");
	HookComponentEvent("Function CarComponent_DoubleJump_TA.Active.BeginState", "double_jump_begin");
	HookComponentEvent("Function CarComponent_Dodge_TA.Active.BeginState", "dodge_begin");

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) { RenderOverlay(canvas); });

	LOG("WallDash Trainer {} loaded. Settings are in the F2 menu, under Plugins.", plugin_version);
}

void WallDashTrainer::onUnload()
{
	gameWrapper->UnregisterDrawables();
	CloseDebugLog();
}

void WallDashTrainer::ExportShareData()
{
	namespace fs = std::filesystem;
	const fs::path folder = gameWrapper->GetDataFolder() / "walldashtrainer";
	const fs::path shareFolder = folder / "share";

	std::error_code error;
	std::vector<fs::path> sources;
	for (const fs::directory_entry& entry : fs::directory_iterator(folder, error))
	{
		const std::string name = entry.path().filename().string();
		if (name.starts_with("attempts_") && entry.path().extension() == ".csv")
			sources.push_back(entry.path());
	}
	if (error || sources.empty())
	{
		LOG("WallDash Trainer: no attempt files to export yet.");
		return;
	}
	std::sort(sources.begin(), sources.end());

	fs::create_directories(shareFolder, error);
	if (error)
	{
		LOG("WallDash Trainer: could not create the share folder: {}", error.message());
		return;
	}

	std::map<std::string, int> sessions;
	int rows = 0;
	int fileNumber = 0;
	for (const fs::path& source : sources)
	{
		std::ifstream in(source);
		std::ofstream out(shareFolder / std::format("attempts_share_{}.csv", ++fileNumber), std::ios::trunc);
		if (in.is_open() && out.is_open())
			rows += walldash::AnonymizeAttemptCsv(in, out, sessions);
	}

	LOG("WallDash Trainer: wrote {} attempts, without timestamps, to {}", rows, shareFolder.string());
	LOG("WallDash Trainer: nothing has been sent. Sharing these files is up to you, and the README explains how.");
}

void WallDashTrainer::LoadChainSampleCounts()
{
	namespace fs = std::filesystem;
	std::error_code error;
	chainSample = walldash::ChainSampleCounts{};
	for (const fs::directory_entry& entry : fs::directory_iterator(gameWrapper->GetDataFolder() / "walldashtrainer", error))
	{
		if (entry.path().filename().string().starts_with("attempts_") && entry.path().extension() == ".csv")
		{
			std::ifstream file(entry.path());
			walldash::AddChainSampleFromCsv(file, chainSample);
		}
	}
}

void WallDashTrainer::RenderOverlay(CanvasWrapper canvas)
{
	if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused())
		return;

	OverlayData data;
	const walldash::ChainStatus& chain = tracker.GetChainStatus();
	data.chainCurrent = chain.current;
	data.chainBest = chain.best;
	data.chainLastSpeedLost = chain.lastSpeedLost;
	data.chainSample = chainSample;
	data.live = tracker.IsActive() ? &tracker.Current() : nullptr;
	data.finished = hasLastAttempt ? &lastAttempt : nullptr;
	data.latestFrame = tracker.LatestFrame();
	data.stats = &stats;
	data.config = &tracker.GetConfig();
	overlay.Draw(canvas, data);
}

void WallDashTrainer::ResetSessionStats()
{
	stats = walldash::SessionStats{};
	hasLastAttempt = false;
	tracker.ResetChainRecords();
}

void WallDashTrainer::ResetSettingsToDefaults()
{
	for (const std::string& name : resettableCvars)
	{
		CVarWrapper cvar = cvarManager->getCvar(name);
		if (!cvar.IsNull())
			cvar.ResetToDefault();
	}
}

void WallDashTrainer::RegisterConfigCvar(const std::string& name, const std::string& defaultValue, const std::string& description,
	std::function<void(walldash::AttemptConfig&, float)> apply)
{
	CVarWrapper cvar = cvarManager->registerCvar(name, defaultValue, description);
	resettableCvars.push_back(name);
	const auto update = [this, apply](float value)
		{
			walldash::AttemptConfig config = tracker.GetConfig();
			apply(config, value);
			tracker.SetConfig(config);
		};
	cvar.addOnValueChanged([update](std::string, CVarWrapper changed) { update(changed.getFloatValue()); });
	update(cvar.getFloatValue());
}

void WallDashTrainer::RegisterCvars()
{
	using walldash::AttemptConfig;

	const auto secondsToTicks = [](float seconds) { return static_cast<int>(std::lround(seconds * kTicksPerSecond)); };

	RegisterConfigCvar("walldash_timeout_s", "1.0", "Seconds after the first jump before an attempt times out",
		[secondsToTicks](AttemptConfig& c, float v) { c.timeoutTicks = secondsToTicks(v); });
	RegisterConfigCvar("walldash_make_window_ticks", "30", "Ticks after first touching the wall to reach the wheel count for a make",
		[](AttemptConfig& c, float v) { c.makeWindowTicks = static_cast<int>(v); });
	RegisterConfigCvar("walldash_make_min_wheels", "3", "Wheels that must be on the wall for a make",
		[](AttemptConfig& c, float v) { c.makeMinWheels = static_cast<int>(v); });
	RegisterConfigCvar("walldash_upright_dot", "0.9", "How upright the car must be on the wall for a make (car up dot wall normal)",
		[](AttemptConfig& c, float v) { c.uprightDot = v; });
	RegisterConfigCvar("walldash_speed_window_s", "0.15", "Seconds after reattaching to look for peak speed",
		[secondsToTicks](AttemptConfig& c, float v) { c.speedWindowTicks = secondsToTicks(v); });
	const AttemptConfig defaults;
	for (int i = 0; i < walldash::kPressBandCount; i++)
	{
		const std::string row = std::to_string(i + 1);
		RegisterConfigCvar("walldash_press_green_ms_" + row, std::format("{:.0f}", defaults.pressBands[i].greenMs), "Press to press green limit for speed band " + row,
			[i](AttemptConfig& c, float v) { c.pressBands[i].greenMs = v; });
		RegisterConfigCvar("walldash_press_yellow_ms_" + row, std::format("{:.0f}", defaults.pressBands[i].yellowMs), "Press to press yellow limit for speed band " + row + ", above this is too late",
			[i](AttemptConfig& c, float v) { c.pressBands[i].yellowMs = v; });
	}
	RegisterConfigCvar("walldash_hold_green_ms", "50", "First jump hold up to this is in the green band",
		[](AttemptConfig& c, float v) { c.holdGreenMs = v; });
	RegisterConfigCvar("walldash_hold_max_ms", "75", "First jump hold above this is red and a miss reason. 0 turns the miss reason off",
		[](AttemptConfig& c, float v) { c.holdMaxMs = v; });
	RegisterConfigCvar("walldash_spam_presses", "4", "Jump presses in one attempt that mark it as spam_input",
		[](AttemptConfig& c, float v) { c.spamPresses = static_cast<int>(v); });
	RegisterConfigCvar("walldash_curve_up_z", "0.1", "Vertical part of the wall normal above which a surface counts as a curve",
		[](AttemptConfig& c, float v) { c.curveUpZ = v; });
	RegisterConfigCvar("walldash_corner_axis", "0.98", "How closely the normal must match an arena axis for a flat wall, below this it is a corner",
		[](AttemptConfig& c, float v) { c.cornerAxis = v; });
	RegisterConfigCvar("walldash_floor_up_z", "0.98", "Car up height at or above which a jump counts as from the floor",
		[](AttemptConfig& c, float v) { c.floorUpZ = v; });
	RegisterConfigCvar("walldash_ceiling_up_z", "-0.7", "Car up height at or below which a jump counts as from the ceiling",
		[](AttemptConfig& c, float v) { c.ceilingUpZ = v; });
	RegisterConfigCvar("walldash_floor_height", "60", "Height below which a wheel touch counts as landing on the floor",
		[](AttemptConfig& c, float v) { c.floorHeight = v; });
	RegisterConfigCvar("walldash_angle_min_right_z", "0.2", "Below this the car is too close to straight up or down the wall for a flip angle sign",
		[](AttemptConfig& c, float v) { c.angleMinRightZ = v; });

	RegisterConfigCvar("walldash_chain_gap_s", "1.0", "Longest gap from reattaching to the next first jump for a chain to continue",
		[secondsToTicks](AttemptConfig& c, float v) { c.chainGapTicks = secondsToTicks(v); });
	RegisterConfigCvar("walldash_prepress_window_s", "0.25", "How far before the first press the turn and height measures look",
		[secondsToTicks](AttemptConfig& c, float v) { c.prepressWindowTicks = secondsToTicks(v); });

	RegisterConfigCvar("walldash_flip_yellow_min", "45", "Flip angle below this is too forward, a miss reason",
		[](AttemptConfig& c, float v) { c.flipYellowMin = v; });
	RegisterConfigCvar("walldash_flip_green_min", "50", "Flip angle from here is in the green band",
		[](AttemptConfig& c, float v) { c.flipGreenMin = v; });
	RegisterConfigCvar("walldash_flip_green_max", "70", "Flip angle up to here is in the green band",
		[](AttemptConfig& c, float v) { c.flipGreenMax = v; });
	RegisterConfigCvar("walldash_flip_yellow_max", "85", "Flip angle above this is too sideways, a miss reason",
		[](AttemptConfig& c, float v) { c.flipYellowMax = v; });

	overlay.RegisterCvars(cvarManager);
	resettableCvars.insert(resettableCvars.end(), overlay.CvarNames().begin(), overlay.CvarNames().end());

	cvarManager->registerCvar("walldash_attempt_log", "1", "Write one CSV row per attempt", true, true, 0, true, 1)
		.bindTo(attemptLogEnabled);
	cvarManager->registerCvar("walldash_console_log", "0", "Print each attempt to the console", true, true, 0, true, 1)
		.bindTo(consoleLogEnabled);
	resettableCvars.push_back("walldash_attempt_log");
	resettableCvars.push_back("walldash_console_log");

	cvarManager->registerCvar("walldash_debug_log", "0", "Write the per-tick debug log (Phases 0 and 3)", true, true, 0, true, 1, false)
		.bindTo(debugLogEnabled);
	cvarManager->getCvar("walldash_debug_log").addOnValueChanged(
		[this](std::string, CVarWrapper cvar)
		{
			if (cvar.getBoolValue())
				OpenDebugLog();
			else
				CloseDebugLog();
		});
}

void WallDashTrainer::HookCarEvent(const std::string& eventName, const std::string& kind)
{
	eventCounts[kind] = 0;
	gameWrapper->HookEventWithCaller<CarWrapper>(eventName,
		[this, kind](CarWrapper car, void*, std::string) { OnCarEvent(car, kind); });
}

void WallDashTrainer::HookComponentEvent(const std::string& eventName, const std::string& kind)
{
	eventCounts[kind] = 0;
	gameWrapper->HookEventWithCaller<CarComponentWrapper>(eventName,
		[this, kind](CarComponentWrapper component, void*, std::string)
		{
			if (component.IsNull())
				return;
			OnCarEvent(component.GetCar(), kind);
		});
}

bool WallDashTrainer::IsLocalCarInFreeplay(CarWrapper& car)
{
	if (car.IsNull() || !gameWrapper->IsInFreeplay())
		return false;
	CarWrapper local = gameWrapper->GetLocalCar();
	return !local.IsNull() && local.memory_address == car.memory_address;
}

float WallDashTrainer::GetWorldTime()
{
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server)
		return 0.f;
	WorldInfoWrapper world = server.GetWorldInfo();
	if (!world)
		return 0.f;
	return world.GetTimeSeconds();
}

void WallDashTrainer::UpdateGameSpeed()
{
	if (++speedWindowTicks < kGameSpeedWindowTicks)
		return;

	const auto now = std::chrono::steady_clock::now();
	const double realMs = std::chrono::duration<double, std::milli>(now - speedWindowStart).count();
	if (realMs > 0.0 && realMs < kGameSpeedMaxWindowMs)
	{
		const double measured = speedWindowTicks * walldash::kMsPerTick / realMs;
		gameSpeed = std::clamp(static_cast<float>(std::round(measured * 20.0) / 20.0), 0.05f, 2.f);
	}
	speedWindowTicks = 0;
	speedWindowStart = now;
}

walldash::TickSample WallDashTrainer::BuildSample(CarWrapper& car, const ControllerInput& input, int frame)
{
	const CarAxes axes = AxesFromRotator(car.GetRotation());

	walldash::TickSample s;
	s.frame = frame;
	s.gameSpeed = gameSpeed;
	s.throttle = input.Throttle;
	s.pitch = input.Pitch;
	s.yaw = input.Yaw;
	s.roll = input.Roll;
	s.powerslide = input.Handbrake != 0;
	s.boost = input.HoldingBoost != 0 || input.ActivateBoost != 0;
	s.pos = ToVec3(car.GetLocation());
	s.vel = ToVec3(car.GetVelocity());
	s.right = ToVec3(axes.right);
	s.up = ToVec3(axes.up);
	s.wheelContacts = car.GetNumWheelWorldContacts();
	s.isOnWall = car.IsOnWall();
	DodgeComponentWrapper dodge = car.GetDodgeComponent();
	if (!dodge.IsNull())
		s.dodgeDir = ToVec3(dodge.GetDodgeDirection());
	s.dodgeDeadzone = settings.gamepadDodgeThreshold;
	s.controllerDeadzone = settings.gamepadControllerDeadzone;
	return s;
}

void WallDashTrainer::OnVehicleInput(CarWrapper car, const ControllerInput& input)
{
	if (!IsLocalCarInFreeplay(car))
		return;

	const int frame = gameWrapper->GetEngine().GetPhysicsFrame();
	callsThisFrame = (frame == lastPhysicsFrame) ? callsThisFrame + 1 : 1;
	lastPhysicsFrame = frame;
	eventCounts["tick"]++;

	const float now = GetWorldTime();
	RefreshSettings(car, now);
	UpdateGameSpeed();

	if (callsThisFrame == 1)
		tracker.OnTick(BuildSample(car, input, frame));

	if (*debugLogEnabled)
		WriteDebugRow("tick", car, input, now);
}

void WallDashTrainer::OnCarEvent(CarWrapper car, const std::string& kind)
{
	if (!IsLocalCarInFreeplay(car))
		return;
	eventCounts[kind]++;

	ForwardEvent(kind);

	if (*debugLogEnabled)
		WriteDebugRow(kind, car, car.GetInput(), GetWorldTime());
}

void WallDashTrainer::ForwardEvent(const std::string& kind)
{
	if (kind == "jump_pressed") tracker.OnJumpPressed();
	else if (kind == "jump_released") tracker.OnJumpReleased();
	else if (kind == "jump_begin") tracker.OnFirstJumpBegin();
	else if (kind == "double_jump_begin") tracker.OnDoubleJumpBegin();
	else if (kind == "dodge_begin") tracker.OnDodgeBegin();
}

void WallDashTrainer::OnAttemptFinished(const AttemptResult& attempt)
{
	attemptCount++;
	stats.Add(attempt);
	lastAttempt = attempt;
	hasLastAttempt = true;

	if (*consoleLogEnabled)
	{
		LOG("{}", DescribeAttempt(attempt));
		LOG("WallDash: session: {}", stats.Summary(overlay.StatsByWall()));
	}

	if (*attemptLogEnabled)
	{
		WriteAttemptRow(attempt);

		// Only attempts saved to a file count, so the number stays the same after a restart.
		if (walldash::CountsTowardChainSample(attempt))
			(attempt.wallSide == walldash::WallSide::Right ? chainSample.right : chainSample.left)++;
	}
}

std::filesystem::path WallDashTrainer::AttemptFilePath()
{
	namespace fs = std::filesystem;

	const std::string date = std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(LocalNow()));
	if (date == attemptFileDate && !attemptFile.empty())
		return attemptFile;

	const fs::path folder = gameWrapper->GetDataFolder() / "walldashtrainer";
	std::error_code error;
	fs::create_directories(folder, error);
	if (error)
	{
		LOG("WallDash Trainer: could not create the data folder: {}", error.message());
		return {};
	}

	// A file written with different columns is left alone, so one CSV never
	// holds two layouts. This matters when a new version adds a column.
	const std::string header = walldash::AttemptCsvHeader();
	fs::path path = folder / std::format("attempts_{}.csv", date);
	for (int suffix = 2; fs::exists(path, error); suffix++)
	{
		std::ifstream existing(path);
		std::string firstLine;
		std::getline(existing, firstLine);
		if (firstLine == header)
			break;
		path = folder / std::format("attempts_{}_{}.csv", date, suffix);
	}

	attemptFileDate = date;
	attemptFile = path;
	return attemptFile;
}

void WallDashTrainer::WriteAttemptRow(const AttemptResult& attempt)
{
	const std::filesystem::path path = AttemptFilePath();
	if (path.empty())
		return;

	std::error_code error;
	const bool isNew = !std::filesystem::exists(path, error);
	std::ofstream file(path, std::ios::app);
	if (!file.is_open())
	{
		LOG("WallDash Trainer: could not write to {}", path.string());
		return;
	}
	if (isNew)
		file << walldash::AttemptCsvHeader() << '\n';

	const std::string timestamp = std::format("{:%Y-%m-%dT%H:%M:%S}", LocalNow());
	file << walldash::AttemptCsvRow(attempt, timestamp, sessionId, attemptCount) << '\n';
}

void WallDashTrainer::RefreshSettings(CarWrapper& car, float now)
{
	// A time jump backwards means a new freeplay session, so refresh then too.
	if (now < settings.nextRefreshTime && settings.nextRefreshTime - now <= 1.f)
		return;
	settings.nextRefreshTime = now + 1.f;

	GamepadSettings gamepad = gameWrapper->GetSettings().GetGamepadSettings();
	settings.gamepadDodgeThreshold = gamepad.DodgeInputThreshold;
	settings.gamepadControllerDeadzone = gamepad.ControllerDeadzone;
	settings.gamepadAirControlSensitivity = gamepad.AirControlSensitivity;

	PriWrapper pri = car.GetPRI();
	settings.priDodgeThreshold = pri.IsNull() ? -1.f : pri.GetDodgeInputThreshold();

	DodgeComponentWrapper dodge = car.GetDodgeComponent();
	settings.componentDodgeThreshold = dodge.IsNull() ? -1.f : dodge.GetDodgeInputThreshold();
}

void WallDashTrainer::WriteDebugRow(const std::string& kind, CarWrapper& car, const ControllerInput& input, float now)
{
	if (!debugLogFile.is_open() && !OpenDebugLog())
		return;

	std::ostringstream row;
	row << kind << ',' << gameWrapper->GetEngine().GetPhysicsFrame() << ',' << callsThisFrame << ','
		<< std::format("{:.5f}", now) << ','
		<< input.Throttle << ',' << input.Steer << ',' << input.Pitch << ',' << input.Yaw << ',' << input.Roll << ','
		<< input.DodgeForward << ',' << input.DodgeStrafe << ','
		<< input.Handbrake << ',' << input.Jump << ',' << input.ActivateBoost << ',' << input.HoldingBoost << ',' << input.Jumped;

	AppendVector(row, car.GetLocation());
	AppendVector(row, car.GetVelocity());
	AppendVector(row, car.GetAngularVelocity());

	const Rotator rotation = car.GetRotation();
	row << ',' << rotation.Pitch << ',' << rotation.Yaw << ',' << rotation.Roll;

	const CarAxes axes = AxesFromRotator(rotation);
	AppendVector(row, axes.forward);
	AppendVector(row, axes.right);
	AppendVector(row, axes.up);

	row << ',' << car.GetbOnGround() << ',' << (car.IsOnGround() ? 1 : 0) << ',' << (car.IsOnWall() ? 1 : 0) << ',' << car.GetNumWheelWorldContacts()
		<< ',' << car.GetbJumped() << ',' << car.GetbDoubleJumped() << ',' << (car.HasFlip() ? 1 : 0) << ',' << (car.IsDodging() ? 1 : 0);

	JumpComponentWrapper jump = car.GetJumpComponent();
	DoubleJumpComponentWrapper doubleJump = car.GetDoubleJumpComponent();
	DodgeComponentWrapper dodge = car.GetDodgeComponent();
	row << ',' << (jump.IsNull() ? -1 : (jump.GetbActive() ? 1 : 0))
		<< ',' << (doubleJump.IsNull() ? -1 : (doubleJump.GetbActive() ? 1 : 0))
		<< ',' << (dodge.IsNull() ? -1 : (dodge.GetbActive() ? 1 : 0));

	AppendVector(row, dodge.IsNull() ? Vector(0.f) : dodge.GetDodgeDirection());
	AppendVector(row, dodge.IsNull() ? Vector(0.f) : dodge.GetDodgeTorque());

	ArrayWrapper<WheelWrapper> wheels = car.GetVehicleSim().GetWheels();
	const int wheelCount = wheels.Count();
	for (int i = 0; i < kWheelCount; i++)
	{
		if (i < wheelCount)
		{
			const WheelContactData contact = wheels.Get(i).GetContact();
			row << ',' << contact.bHasContactWithWorldGeometry;
			AppendVector(row, contact.Normal);
		}
		else
		{
			row << ",-1,0,0,0";
		}
	}

	row << ',' << settings.gamepadDodgeThreshold << ',' << settings.gamepadControllerDeadzone << ',' << settings.gamepadAirControlSensitivity
		<< ',' << settings.priDodgeThreshold << ',' << settings.componentDodgeThreshold << '\n';

	debugLogFile << row.str();
	debugRowsWritten++;

	if (now >= nextFlushTime || now < nextFlushTime - 1.f)
	{
		debugLogFile.flush();
		nextFlushTime = now + 1.f;
	}
}

bool WallDashTrainer::OpenDebugLog()
{
	if (debugLogFile.is_open())
		return true;

	namespace fs = std::filesystem;
	const fs::path folder = gameWrapper->GetDataFolder() / "walldashtrainer";
	std::error_code error;
	fs::create_directories(folder, error);
	if (error)
	{
		LOG("WallDash Trainer: could not create the log folder: {}", error.message());
		*debugLogEnabled = false;
		return false;
	}

	const fs::path path = folder / std::format("debug_ticks_{:%Y-%m-%d_%H%M%S}.csv", LocalNow());
	debugLogFile.open(path, std::ios::out | std::ios::trunc);
	if (!debugLogFile.is_open())
	{
		LOG("WallDash Trainer: could not open the log file: {}", path.string());
		*debugLogEnabled = false;
		return false;
	}

	debugLogPath = path.string();
	debugRowsWritten = 0;
	nextFlushTime = -1.f;
	debugLogFile << DebugLogHeader() << '\n';
	debugLogFile.flush();
	LOG("WallDash Trainer: logging to {}", debugLogPath);
	return true;
}

void WallDashTrainer::CloseDebugLog()
{
	if (!debugLogFile.is_open())
		return;
	debugLogFile.flush();
	debugLogFile.close();
	LOG("WallDash Trainer: log closed, {} rows in {}", debugRowsWritten, debugLogPath);
}

void WallDashTrainer::PrintStatus()
{
	LOG("WallDash Trainer: session {}, {} attempts, game speed {:.2f}", sessionId, attemptCount, gameSpeed);
	LOG("WallDash Trainer: {}", stats.Summary(overlay.StatsByWall()));
	LOG("WallDash Trainer: attempt file: {}", attemptFile.empty() ? "none yet" : attemptFile.string());
	LOG("WallDash Trainer: debug log {}, file: {}, rows: {}", *debugLogEnabled ? "on" : "off", debugLogPath.empty() ? "none" : debugLogPath, debugRowsWritten);
	LOG("WallDash Trainer: SetVehicleInput calls on the latest physics frame: {}", callsThisFrame);
	for (const auto& [kind, count] : eventCounts)
	{
		LOG("WallDash Trainer:   {}: {}", kind, count);
	}
}
