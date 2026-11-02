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

#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <sstream>

BAKKESMOD_PLUGIN(WallDashTrainer, "WallDash Trainer", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

namespace
{
	constexpr int kWheelCount = 4;
	constexpr float kUnrealRotToRad = 3.14159265358979f / 32768.f;

	struct CarAxes
	{
		Vector forward;
		Vector right;
		Vector up;
	};

	// Unreal's rotator to axes conversion. The tick log lets us check it: when
	// the car sits on a surface, up must match the wheel contact normal.
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

	void AppendVector(std::ostringstream& out, const Vector& v)
	{
		out << ',' << v.X << ',' << v.Y << ',' << v.Z;
	}

	std::string BuildHeader()
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
}

void WallDashTrainer::onLoad()
{
	_globalCvarManager = cvarManager;

	cvarManager->registerCvar("walldash_debug_log", "0", "Write the per-tick debug log (Phases 0 and 3)", true, true, 0, true, 1, false)
		.bindTo(debugLogEnabled);
	cvarManager->getCvar("walldash_debug_log").addOnValueChanged(
		[this](std::string, CVarWrapper cvar)
		{
			if (cvar.getBoolValue())
				OpenLog();
			else
				CloseLog();
		});

	cvarManager->registerNotifier("walldash_debug_status",
		[this](std::vector<std::string>) { PrintStatus(); },
		"Print the log path, rows written and how many times each event fired", PERMISSION_ALL);

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
	HookComponentEvent("Function CarComponent_Jump_TA.Active.EndState", "jump_end");
	HookComponentEvent("Function CarComponent_DoubleJump_TA.Active.BeginState", "double_jump_begin");
	HookComponentEvent("Function CarComponent_Dodge_TA.Active.BeginState", "dodge_begin");
	HookComponentEvent("Function CarComponent_Dodge_TA.Active.EndState", "dodge_end");

	LOG("WallDash Trainer {} loaded. Set walldash_debug_log 1 to start logging.", plugin_version);
}

void WallDashTrainer::onUnload()
{
	CloseLog();
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

void WallDashTrainer::OnVehicleInput(CarWrapper car, const ControllerInput& input)
{
	if (!IsLocalCarInFreeplay(car))
		return;

	const int frame = gameWrapper->GetEngine().GetPhysicsFrame();
	callsThisFrame = (frame == lastPhysicsFrame) ? callsThisFrame + 1 : 1;
	lastPhysicsFrame = frame;
	eventCounts["tick"]++;

	if (!*debugLogEnabled)
		return;
	WriteRow("tick", car, input, GetWorldTime());
}

void WallDashTrainer::OnCarEvent(CarWrapper car, const std::string& kind)
{
	if (!IsLocalCarInFreeplay(car))
		return;
	eventCounts[kind]++;

	if (!*debugLogEnabled)
		return;
	WriteRow(kind, car, car.GetInput(), GetWorldTime());
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

void WallDashTrainer::WriteRow(const std::string& kind, CarWrapper& car, const ControllerInput& input, float now)
{
	if (!logFile.is_open() && !OpenLog())
		return;

	RefreshSettings(car, now);

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

	// Wheels are read directly so a short or missing array still gives the same columns.
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

	logFile << row.str();
	rowsWritten++;

	if (now >= nextFlushTime || now < nextFlushTime - 1.f)
	{
		logFile.flush();
		nextFlushTime = now + 1.f;
	}
}

bool WallDashTrainer::OpenLog()
{
	if (logFile.is_open())
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

	const auto localNow = std::chrono::zoned_time(std::chrono::current_zone(), std::chrono::system_clock::now());
	const std::string stamp = std::format("{:%Y-%m-%d_%H%M%S}", std::chrono::floor<std::chrono::seconds>(localNow.get_local_time()));
	const fs::path path = folder / ("debug_ticks_" + stamp + ".csv");

	logFile.open(path, std::ios::out | std::ios::trunc);
	if (!logFile.is_open())
	{
		LOG("WallDash Trainer: could not open the log file: {}", path.string());
		*debugLogEnabled = false;
		return false;
	}

	logPath = path.string();
	rowsWritten = 0;
	nextFlushTime = -1.f;
	logFile << BuildHeader() << '\n';
	logFile.flush();
	LOG("WallDash Trainer: logging to {}", logPath);
	return true;
}

void WallDashTrainer::CloseLog()
{
	if (!logFile.is_open())
		return;
	logFile.flush();
	logFile.close();
	LOG("WallDash Trainer: log closed, {} rows in {}", rowsWritten, logPath);
}

void WallDashTrainer::PrintStatus()
{
	LOG("WallDash Trainer: logging {}, file: {}, rows: {}", *debugLogEnabled ? "on" : "off", logPath.empty() ? "none" : logPath, rowsWritten);
	LOG("WallDash Trainer: SetVehicleInput calls on the latest physics frame: {}", callsThisFrame);
	for (const auto& [kind, count] : eventCounts)
	{
		LOG("WallDash Trainer:   {}: {}", kind, count);
	}
}
