#include "pch.h"
#include "WallDashCoach.h"

#include "imgui.h"

// RenderSettings runs on the render thread, so cvar changes are handed to the
// game thread instead of being made here.

std::string WallDashCoach::GetPluginName()
{
	return "Wall Dash Coach";
}

void WallDashCoach::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void WallDashCoach::SettingsCheckbox(const char* label, const std::string& cvarName)
{
	CVarWrapper cvar = cvarManager->getCvar(cvarName);
	if (cvar.IsNull())
		return;

	bool value = cvar.getBoolValue();
	if (ImGui::Checkbox(label, &value))
		gameWrapper->Execute([cvar, value](GameWrapper*) mutable { cvar.setValue(value ? 1 : 0); });
}

void WallDashCoach::SettingsSlider(const char* label, const std::string& cvarName, float min, float max, const char* format)
{
	CVarWrapper cvar = cvarManager->getCvar(cvarName);
	if (cvar.IsNull())
		return;

	float value = cvar.getFloatValue();
	if (ImGui::SliderFloat(label, &value, min, max, format))
		gameWrapper->Execute([cvar, value](GameWrapper*) mutable { cvar.setValue(value); });
}

void WallDashCoach::RenderSettings()
{
	ImGui::TextUnformatted("Wall Dash Coach: measures wall dash attempts in freeplay.");
	ImGui::Separator();

	SettingsCheckbox("Show the overlay (while hidden, attempts are not counted or saved)", "walldash_overlay_on");
	SettingsCheckbox("Pause logging (attempts are not saved until you untick this)", "walldash_logging_paused");
	SettingsCheckbox("Split session stats by wall", "walldash_stats_by_wall");
	SettingsCheckbox("Show average speed gain in session stats", "walldash_stats_speed_gain");

	if (ImGui::CollapsingHeader("Overlay panels"))
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

	if (ImGui::CollapsingHeader("Chain dashes"))
	{
		ImGui::TextUnformatted("Chain dashes are always logged. This shows the chain counter and speed lost between dashes on the overlay.");
		SettingsCheckbox("Show chain dashes", "walldash_chains_on");
		SettingsSlider("Longest gap from reattaching to the next jump (s)", "walldash_chain_gap_s", 0.1f, 3.f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Fast dashes"))
	{
		ImGui::TextUnformatted("A dash is fast when its speed along the wall at the first press is above this.");
		SettingsSlider("Fast above (uu/s)", "walldash_fast_speed", 0.f, 2300.f, "%.0f");

		CVarWrapper mode = cvarManager->getCvar("walldash_fast_stats_mode");
		if (!mode.IsNull())
		{
			ImGui::TextUnformatted("In the session stats:");
			const int current = mode.getIntValue();
			static const char* const labels[] = { "Include", "Show separately", "Leave out" };
			for (int i = 0; i < 3; i++)
			{
				if (i > 0)
					ImGui::SameLine();
				if (ImGui::RadioButton(labels[i], current == i))
					gameWrapper->Execute([mode, i](GameWrapper*) mutable { mode.setValue(i); });
			}
		}
		SettingsCheckbox("Leave fast dashes out of the attempt CSV", "walldash_fast_skip_csv");
	}

	if (ImGui::CollapsingHeader("Press to press bands"))
	{
		ImGui::TextUnformatted("Times are in game time. The defaults come from one player's data, so tighten them as you improve.");
		SettingsSlider("Too early below (ms)", "walldash_press_early_ms", 0.f, 150.f, "%.0f");

		static const char* const speedLabels[walldash::kPressBandCount] = {
			"Under 1300 uu/s", "1300 to 1500 uu/s", "1500 to 1700 uu/s", "1700 to 1900 uu/s", "1900 uu/s and up"
		};
		for (int i = 0; i < walldash::kPressBandCount; i++)
		{
			const std::string row = std::to_string(i + 1);
			ImGui::PushID(i);
			ImGui::TextUnformatted(speedLabels[i]);
			SettingsSlider("Green up to (ms)", "walldash_press_green_ms_" + row, 0.f, 200.f, "%.0f");
			SettingsSlider("Yellow up to, then too late (ms)", "walldash_press_yellow_ms_" + row, 0.f, 200.f, "%.0f");
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Flip angle bands"))
	{
		ImGui::TextUnformatted("Degrees from straight ahead, toward the ceiling. The defaults come from one player's data.");
		SettingsSlider("Too forward below", "walldash_flip_yellow_min", 0.f, 90.f, "%.0f");
		SettingsSlider("Green from", "walldash_flip_green_min", 0.f, 90.f, "%.0f");
		SettingsSlider("Green up to", "walldash_flip_green_max", 0.f, 90.f, "%.0f");
		SettingsSlider("Too sideways above", "walldash_flip_yellow_max", 0.f, 120.f, "%.0f");
	}

	if (ImGui::CollapsingHeader("Speed gain"))
	{
		ImGui::TextUnformatted("At 0, only a speed loss is red. Raise it to also mark small gains in red.");
		SettingsSlider("Red below (uu/s)", "walldash_speed_red_below", 0.f, 600.f, "%.0f");
	}

	if (ImGui::CollapsingHeader("Advanced"))
	{
		if (ImGui::TreeNode("Make and miss rules"))
		{
			SettingsSlider("Attempt timeout (s)", "walldash_timeout_s", 0.25f, 3.f, "%.2f");
			SettingsSlider("Ticks after first touch to reach the wheel count", "walldash_make_window_ticks", 1.f, 120.f, "%.0f");
			SettingsSlider("Wheels on the wall for a make", "walldash_make_min_wheels", 1.f, 4.f, "%.0f");
			SettingsSlider("How upright for a make", "walldash_upright_dot", 0.f, 1.f, "%.2f");
			SettingsSlider("Peak speed window after reattaching (s)", "walldash_speed_window_s", 0.f, 1.f, "%.2f");
			SettingsSlider("Jump presses that count as spam", "walldash_spam_presses", 2.f, 12.f, "%.0f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Hold bands"))
		{
			SettingsSlider("Green up to (ms)", "walldash_hold_green_ms", 0.f, 200.f, "%.0f");
			SettingsSlider("Yellow up to, then held too long (ms, 0 turns the miss reason off)", "walldash_hold_max_ms", 0.f, 200.f, "%.0f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Chain data collection"))
		{
			ImGui::TextUnformatted("Counts follow-up dashes across sessions, from your saved attempt files.");
			SettingsCheckbox("Show chain sample counter", "walldash_sample_counter_on");
			SettingsSlider("Target per wall", "walldash_sample_target", 1.f, 2000.f, "%.0f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Surface detection"))
		{
			SettingsSlider("Curve above this normal height", "walldash_curve_up_z", 0.f, 1.f, "%.2f");
			SettingsSlider("Flat wall at or above this axis match", "walldash_corner_axis", 0.5f, 1.f, "%.3f");
			SettingsSlider("Floor at or above this car up height", "walldash_floor_up_z", 0.5f, 1.f, "%.2f");
			SettingsSlider("Ceiling at or below this car up height", "walldash_ceiling_up_z", -1.f, 0.f, "%.2f");
			SettingsSlider("Floor touch below this height", "walldash_floor_height", 0.f, 200.f, "%.0f");
			SettingsSlider("Flip angle undefined below this right vector height", "walldash_angle_min_right_z", 0.f, 1.f, "%.2f");
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Logging"))
		{
			SettingsCheckbox("Write the attempt CSV", "walldash_attempt_log");
			SettingsCheckbox("Print each attempt to the console", "walldash_console_log");
			SettingsCheckbox("Write the per-tick debug log", "walldash_debug_log");
			ImGui::TreePop();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Reset session stats"))
		gameWrapper->Execute([this](GameWrapper*) { ResetSessionStats(); });
	ImGui::SameLine();
	if (ImGui::Button("Reset all settings to defaults"))
		gameWrapper->Execute([this](GameWrapper*) { ResetSettingsToDefaults(); });
}
