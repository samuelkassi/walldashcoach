#include "pch.h"
#include "WallDashTrainer.h"

#include "imgui.h"

// RenderSettings runs on the render thread, so cvar changes are handed to the
// game thread instead of being made here.

std::string WallDashTrainer::GetPluginName()
{
	return "WallDash Trainer";
}

void WallDashTrainer::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void WallDashTrainer::SettingsCheckbox(const char* label, const std::string& cvarName)
{
	CVarWrapper cvar = cvarManager->getCvar(cvarName);
	if (cvar.IsNull())
		return;

	bool value = cvar.getBoolValue();
	if (ImGui::Checkbox(label, &value))
		gameWrapper->Execute([cvar, value](GameWrapper*) mutable { cvar.setValue(value ? 1 : 0); });
}

void WallDashTrainer::SettingsSlider(const char* label, const std::string& cvarName, float min, float max, const char* format)
{
	CVarWrapper cvar = cvarManager->getCvar(cvarName);
	if (cvar.IsNull())
		return;

	float value = cvar.getFloatValue();
	if (ImGui::SliderFloat(label, &value, min, max, format))
		gameWrapper->Execute([cvar, value](GameWrapper*) mutable { cvar.setValue(value); });
}

void WallDashTrainer::RenderSettings()
{
	ImGui::TextUnformatted("WallDash Trainer: measures wall dash attempts in freeplay.");
	ImGui::Separator();

	SettingsCheckbox("Show the overlay", "walldash_overlay_on");

	if (ImGui::CollapsingHeader("Overlay panels", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const OverlayPanel& panel : overlay.Panels())
		{
			ImGui::PushID(panel.id.c_str());
			if (ImGui::TreeNode(panel.label.c_str()))
			{
				const std::string prefix = "walldash_panel_" + panel.id;
				SettingsCheckbox("Show", prefix + "_on");
				SettingsSlider("Horizontal position", prefix + "_x", 0.f, 1.f, "%.3f");
				SettingsSlider("Vertical position", prefix + "_y", 0.f, 1.f, "%.3f");
				SettingsSlider("Size", prefix + "_size", 0.25f, 3.f, "%.2f");
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Press to press bands"))
	{
		ImGui::TextUnformatted("Times are in game time. These are starting values until they are set from recorded data.");
		SettingsSlider("Too early below (ms)", "walldash_press_early_ms", 0.f, 150.f, "%.0f");
		SettingsSlider("Green up to (ms)", "walldash_press_green_ms", 0.f, 150.f, "%.0f");
		SettingsSlider("Yellow up to, then too late (ms)", "walldash_late_press_ms", 0.f, 150.f, "%.0f");
	}

	if (ImGui::CollapsingHeader("Make and miss rules"))
	{
		SettingsSlider("Attempt timeout (s)", "walldash_timeout_s", 0.25f, 3.f, "%.2f");
		SettingsSlider("Ticks after first touch to reach the wheel count", "walldash_make_window_ticks", 1.f, 120.f, "%.0f");
		SettingsSlider("Wheels on the wall for a make", "walldash_make_min_wheels", 1.f, 4.f, "%.0f");
		SettingsSlider("How upright for a make", "walldash_upright_dot", 0.f, 1.f, "%.2f");
		SettingsSlider("Peak speed window after reattaching (s)", "walldash_speed_window_s", 0.f, 1.f, "%.2f");
		SettingsSlider("First jump held too long (ms, 0 is off)", "walldash_hold_max_ms", 0.f, 300.f, "%.0f");
		SettingsSlider("Jump presses that count as spam", "walldash_spam_presses", 2.f, 12.f, "%.0f");
	}

	if (ImGui::CollapsingHeader("Surface detection"))
	{
		SettingsSlider("Curve above this normal height", "walldash_curve_up_z", 0.f, 1.f, "%.2f");
		SettingsSlider("Flat wall at or above this axis match", "walldash_corner_axis", 0.5f, 1.f, "%.3f");
		SettingsSlider("Floor at or above this car up height", "walldash_floor_up_z", 0.5f, 1.f, "%.2f");
		SettingsSlider("Ceiling at or below this car up height", "walldash_ceiling_up_z", -1.f, 0.f, "%.2f");
		SettingsSlider("Floor touch below this height", "walldash_floor_height", 0.f, 200.f, "%.0f");
		SettingsSlider("Flip angle undefined below this right vector height", "walldash_angle_min_right_z", 0.f, 1.f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Logging"))
	{
		SettingsCheckbox("Write the attempt CSV", "walldash_attempt_log");
		SettingsCheckbox("Print each attempt to the console", "walldash_console_log");
		SettingsCheckbox("Write the per-tick debug log", "walldash_debug_log");
	}

	ImGui::Separator();
	if (ImGui::Button("Reset session stats"))
		gameWrapper->Execute([this](GameWrapper*) { ResetSessionStats(); });
	ImGui::SameLine();
	if (ImGui::Button("Reset all settings to defaults"))
		gameWrapper->Execute([this](GameWrapper*) { ResetSettingsToDefaults(); });
}
