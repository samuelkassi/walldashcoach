#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "core/AttemptCsv.h"
#include "core/AttemptTracker.h"
#include "overlay/Overlay.h"
#include "version.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

// Connects the game to the attempt logic in src/core: reads the car on every
// physics tick, forwards the jump events, and reports each finished attempt to
// the overlay, the attempt CSV and optionally the console.
class WallDashCoach : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
public:
	void onLoad() override;
	void onUnload() override;

	// The F2 settings window, in SettingsWindow.cpp.
	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;

private:
	void SettingsCheckbox(const char* label, const std::string& cvarName);
	void SettingsSlider(const char* label, const std::string& cvarName, float min, float max, const char* format);
	void ResetSessionStats();
	void ResetSettingsToDefaults();
	void ExportShareData();
	void LoadChainSampleCounts();
	void RenderOverlay(CanvasWrapper canvas);
	walldash::StatsOptions CurrentStatsOptions() const;
	bool ShouldLogAttempt(const walldash::AttemptResult& attempt) const;

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

	void RegisterCvars();
	void RegisterConfigCvar(const std::string& name, const std::string& defaultValue, const std::string& description,
		std::function<void(walldash::AttemptConfig&, float)> apply);

	void HookCarEvent(const std::string& eventName, const std::string& kind);
	void HookComponentEvent(const std::string& eventName, const std::string& kind);

	void OnVehicleInput(CarWrapper car, const ControllerInput& input);
	void OnCarEvent(CarWrapper car, const std::string& kind);
	void ForwardEvent(const std::string& kind);
	void OnAttemptFinished(const walldash::AttemptResult& attempt);

	bool IsLocalCarInFreeplay(CarWrapper& car);
	float GetWorldTime();
	void RefreshSettings(CarWrapper& car, float now);
	walldash::TickSample BuildSample(CarWrapper& car, const ControllerInput& input, int frame);
	void UpdateGameSpeed();

	void WriteDebugRow(const std::string& kind, CarWrapper& car, const ControllerInput& input, float now);
	bool OpenDebugLog();
	void CloseDebugLog();
	void WriteAttemptRow(const walldash::AttemptResult& attempt);
	std::filesystem::path AttemptFilePath();
	void PrintStatus();

	walldash::AttemptTracker tracker{ [this](const walldash::AttemptResult& attempt) { OnAttemptFinished(attempt); } };
	walldash::SessionStats stats;
	walldash::ChainSampleCounts chainSample;

	Overlay overlay;
	walldash::AttemptResult lastAttempt;
	bool hasLastAttempt = false;
	std::vector<std::string> resettableCvars;

	std::shared_ptr<bool> debugLogEnabled = std::make_shared<bool>(false);
	std::shared_ptr<bool> attemptLogEnabled = std::make_shared<bool>(true);
	std::shared_ptr<bool> consoleLogEnabled = std::make_shared<bool>(false);
	std::shared_ptr<bool> loggingPaused = std::make_shared<bool>(false);

	// Dashes faster than this at the first press can be split out of the
	// session stats, left out of them, or left out of the attempt CSV.
	std::shared_ptr<float> fastSpeed = std::make_shared<float>(2100.f);
	std::shared_ptr<int> fastStatsMode = std::make_shared<int>(0); // A walldash::FastDashMode.
	std::shared_ptr<bool> fastSkipCsv = std::make_shared<bool>(false);
	std::shared_ptr<bool> statsShowSpeedGain = std::make_shared<bool>(true);

	std::string sessionId;
	int attemptCount = 0;
	std::string attemptFileDate;
	std::filesystem::path attemptFile;

	std::ofstream debugLogFile;
	std::string debugLogPath;
	uint64_t debugRowsWritten = 0;
	float nextFlushTime = -1.f;

	// How many SetVehicleInput calls landed on the same physics frame. Phase 0
	// measured exactly one, and this column keeps checking it.
	int lastPhysicsFrame = -1;
	int callsThisFrame = 0;

	// Game speed comes from real time per tick, so it works however it was set.
	float gameSpeed = 1.f;
	int speedWindowTicks = 0;
	std::chrono::steady_clock::time_point speedWindowStart;
	std::chrono::steady_clock::time_point lastTickTime;

	CachedSettings settings;

	// Fire counts per event, so an event name that never fires is easy to spot.
	std::map<std::string, uint64_t> eventCounts;
};
