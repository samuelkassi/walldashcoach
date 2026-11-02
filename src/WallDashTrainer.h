#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "version.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <string>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

// Phase 0 spike: hooks the jump, dodge and input events and writes everything
// to a per-tick CSV. It makes no decisions about attempts. That is Phase 1.
class WallDashTrainer : public BakkesMod::Plugin::BakkesModPlugin
{
public:
	void onLoad() override;
	void onUnload() override;

private:
	// Deadzone and sensitivity values. They rarely change, so they are read
	// once a second instead of on every tick.
	struct CachedSettings
	{
		float gamepadDodgeThreshold = 0.f;
		float gamepadControllerDeadzone = 0.f;
		float gamepadAirControlSensitivity = 0.f;
		float priDodgeThreshold = 0.f;
		float componentDodgeThreshold = 0.f;
		float nextRefreshTime = -1.f;
	};

	void HookCarEvent(const std::string& eventName, const std::string& kind);
	void HookComponentEvent(const std::string& eventName, const std::string& kind);

	void OnVehicleInput(CarWrapper car, const ControllerInput& input);
	void OnCarEvent(CarWrapper car, const std::string& kind);

	bool IsLocalCarInFreeplay(CarWrapper& car);
	float GetWorldTime();
	void RefreshSettings(CarWrapper& car, float now);
	void WriteRow(const std::string& kind, CarWrapper& car, const ControllerInput& input, float now);

	bool OpenLog();
	void CloseLog();
	void PrintStatus();

	std::shared_ptr<bool> debugLogEnabled = std::make_shared<bool>(false);

	std::ofstream logFile;
	std::string logPath;
	uint64_t rowsWritten = 0;
	float nextFlushTime = -1.f;

	// How many SetVehicleInput calls landed on the same physics frame. Phase 0
	// needs this to find out whether freeplay really gives one call per tick.
	int lastPhysicsFrame = -1;
	int callsThisFrame = 0;

	CachedSettings settings;

	// Fire counts per event, so an event name that never fires is easy to spot.
	std::map<std::string, uint64_t> eventCounts;
};
