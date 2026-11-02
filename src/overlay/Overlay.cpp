#include "pch.h"
#include "Overlay.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace
{
	using walldash::AttemptResult;
	using walldash::Surface;

	constexpr float kBarMaxMs = 150.f;
	constexpr float kSpeedBarRange = 600.f;
	constexpr float kPreviewAlpha = 60.f;
	constexpr float kTextScale = 1.7f;
	constexpr float kStickUnit = 80.f; // Pixels for a stick value of 1 at scale 1.
	constexpr int kCircleSegments = 48;
	constexpr float kSectorInner = 1.03f; // Rim thickness, in stick units.
	constexpr float kSectorOuter = 1.17f;
	constexpr float kSectorStepDeg = 3.f;

	const LinearColor kBackground(30, 30, 30, 170);
	const LinearColor kWhite(255, 255, 255, 255);
	const LinearColor kFaint(255, 255, 255, 90);
	const LinearColor kGreen(40, 200, 70, 230);
	const LinearColor kYellow(245, 210, 40, 230);
	const LinearColor kRed(230, 50, 50, 230);
	const LinearColor kNeutral(90, 170, 255, 230);
	const LinearColor kFlipLine(90, 220, 255, 255);

	void DrawText(CanvasWrapper& canvas, const std::string& text, Vector2F pos, float scale, const LinearColor& color)
	{
		canvas.SetColor(color);
		canvas.SetPosition(pos);
		canvas.DrawString(text, scale, scale, true);
	}

	void FillRect(CanvasWrapper& canvas, Vector2F pos, Vector2F size, const LinearColor& color)
	{
		canvas.SetColor(color);
		canvas.SetPosition(pos);
		canvas.FillBox(size);
	}

	void DrawDot(CanvasWrapper& canvas, Vector2F center, float size, const LinearColor& color)
	{
		FillRect(canvas, Vector2F{ center.X - size / 2.f, center.Y - size / 2.f }, Vector2F{ size, size }, color);
	}

	// While an attempt runs, a timing with no result yet counts up from what is
	// known so far. -1 means there is nothing to show.
	int HoldTicks(const AttemptResult& a, bool live, int nowTicks)
	{
		if (a.holdTicks >= 0)
			return a.holdTicks;
		return live ? nowTicks : -1;
	}

	int ReleaseToPressTicks(const AttemptResult& a, bool live, int nowTicks)
	{
		if (a.releaseToPressTicks >= 0)
			return a.releaseToPressTicks;
		return live && a.holdTicks >= 0 && a.pressToPressTicks < 0 ? nowTicks - a.holdTicks : -1;
	}

	int PressToPressTicks(const AttemptResult& a, bool live, int nowTicks)
	{
		if (a.pressToPressTicks >= 0)
			return a.pressToPressTicks;
		return live ? nowTicks : -1;
	}

	float TicksToMs(int ticks)
	{
		return ticks * walldash::kMsPerTick;
	}

	std::string TimingText(const std::string& label, int ticks)
	{
		if (ticks < 0)
			return label + ": none";
		return std::format("{}: {:.0f} ms ({} ticks)", label, TicksToMs(ticks), ticks);
	}
}

void Overlay::AddPanel(const std::shared_ptr<CVarManagerWrapper>& cvarManager, const std::string& id, const std::string& label, float x, float y, bool onByDefault)
{
	OverlayPanel panel;
	panel.id = id;
	panel.label = label;
	const std::string prefix = "walldash_panel_" + id;

	cvarManager->registerCvar(prefix + "_on", onByDefault ? "1" : "0", label + ": show this panel", true, true, 0, true, 1).bindTo(panel.on);
	cvarManager->registerCvar(prefix + "_x", std::to_string(x), label + ": horizontal position, as a fraction of the screen", true, true, 0, true, 1).bindTo(panel.x);
	cvarManager->registerCvar(prefix + "_y", std::to_string(y), label + ": vertical position, as a fraction of the screen", true, true, 0, true, 1).bindTo(panel.y);
	cvarManager->registerCvar(prefix + "_size", "1", label + ": size", true, true, 0.25f, true, 3).bindTo(panel.size);

	cvarNames.push_back(prefix + "_on");
	cvarNames.push_back(prefix + "_x");
	cvarNames.push_back(prefix + "_y");
	cvarNames.push_back(prefix + "_size");
	panels.push_back(panel);
}

void Overlay::RegisterCvars(const std::shared_ptr<CVarManagerWrapper>& cvarManager)
{
	cvarManager->registerCvar("walldash_overlay_on", "1", "Show the overlay in freeplay", true, true, 0, true, 1).bindTo(enabled);
	cvarManager->registerCvar("walldash_press_early_ms", "58", "Press to press below this is in the too early band", true, true, 0, true, 150).bindTo(pressEarlyMs);
	cvarManager->registerCvar("walldash_speed_red_below", "0", "Speed gain below this draws red. At 0 only a loss is red", true, true, 0, true, 600).bindTo(speedRedBelow);
	cvarManager->registerCvar("walldash_stats_by_wall", "1", "Show session stats separately for the left and right walls", true, true, 0, true, 1).bindTo(statsByWall);
	cvarManager->registerCvar("walldash_chains_on", "0", "Show the chain counter and speed lost between dashes", true, true, 0, true, 1).bindTo(chainsOn);
	cvarManager->registerCvar("walldash_sample_counter_on", "0", "Show progress toward a sample of chain dashes per wall", true, true, 0, true, 1).bindTo(sampleCounterOn);
	cvarManager->registerCvar("walldash_sample_target", "300", "Chain dashes per wall to collect", true, true, 1, true, 5000).bindTo(sampleTarget);
	cvarNames.push_back("walldash_sample_counter_on");
	cvarNames.push_back("walldash_sample_target");
	cvarNames.push_back("walldash_chains_on");
	cvarNames.push_back("walldash_stats_by_wall");
	cvarNames.push_back("walldash_overlay_on");
	cvarNames.push_back("walldash_press_early_ms");
	cvarNames.push_back("walldash_speed_red_below");

	// Top of the screen for timing, bottom for the rest, so the car stays visible.
	// The default view is kept simple, and the rest can be turned on in the settings.
	AddPanel(cvarManager, "bars", "Timing bars", 0.34f, 0.015f, true);
	AddPanel(cvarManager, "verdict", "Verdict", 0.34f, 0.19f, true);
	AddPanel(cvarManager, "surface", "Surface note", 0.34f, 0.25f, false);
	AddPanel(cvarManager, "stick", "Stick display", 0.26f, 0.73f, true);
	AddPanel(cvarManager, "speed", "Speed gain", 0.42f, 0.76f, true);
	AddPanel(cvarManager, "distance", "Wall distance", 0.42f, 0.84f, false);
	AddPanel(cvarManager, "stats", "Session stats", 0.42f, 0.87f, false);

	// These also need their feature turned on, so they are ready when it is.
	AddPanel(cvarManager, "chain", "Chain counter", 0.64f, 0.76f, true);
	AddPanel(cvarManager, "speedloss", "Speed lost between dashes", 0.64f, 0.80f, true);
	AddPanel(cvarManager, "chainsample", "Chain sample counter", 0.64f, 0.83f, true);
}

void Overlay::Draw(CanvasWrapper& canvas, const OverlayData& data)
{
	if (!*enabled)
		return;

	const Vector2 screenSize = canvas.GetSize();
	screen = Vector2F{ static_cast<float>(screenSize.X), static_cast<float>(screenSize.Y) };
	const float resolutionScale = screen.Y / 1080.f;

	for (const OverlayPanel& panel : panels)
	{
		if (!*panel.on)
			continue;

		const float scale = resolutionScale * *panel.size;
		if (panel.id == "bars") DrawBars(canvas, panel, data, scale);
		else if (panel.id == "verdict") DrawVerdict(canvas, panel, data, scale);
		else if (panel.id == "surface") DrawSurfaceNote(canvas, panel, data, scale);
		else if (panel.id == "stick") DrawStick(canvas, panel, data, scale);
		else if (panel.id == "speed") DrawSpeed(canvas, panel, data, scale);
		else if (panel.id == "distance") DrawDistance(canvas, panel, data, scale);
		else if (panel.id == "stats") DrawStats(canvas, panel, data, scale);
		else if (panel.id == "chain" && *chainsOn) DrawChain(canvas, panel, data, scale);
		else if (panel.id == "speedloss" && *chainsOn) DrawSpeedLoss(canvas, panel, data, scale);
		else if (panel.id == "chainsample" && *sampleCounterOn) DrawChainSample(canvas, panel, data, scale);
	}
}

void Overlay::DrawRangeBar(CanvasWrapper& canvas, const std::string& text, float value, float maxValue, Vector2F pos, Vector2F size, float scale, const std::vector<Band>& bands)
{
	FillRect(canvas, pos, size, kBackground);

	// A faint preview of each band, so the target is always visible.
	for (const Band& band : bands)
	{
		const float left = std::clamp(band.min / maxValue, 0.f, 1.f);
		const float right = std::clamp(band.max / maxValue, 0.f, 1.f);
		LinearColor preview = band.color;
		preview.A = kPreviewAlpha;
		FillRect(canvas, Vector2F{ pos.X + left * size.X, pos.Y }, Vector2F{ (right - left) * size.X, size.Y }, preview);
	}

	if (value >= 0.f)
	{
		LinearColor fill = kNeutral;
		for (const Band& band : bands)
		{
			if (value >= band.min && value < band.max)
				fill = band.color;
		}
		FillRect(canvas, pos, Vector2F{ std::min(value / maxValue, 1.f) * size.X, size.Y }, fill);
	}

	canvas.SetColor(kWhite);
	canvas.SetPosition(pos);
	canvas.DrawBox(size);
	for (const Band& band : bands)
	{
		const float edge = band.max / maxValue;
		if (edge < 1.f)
			FillRect(canvas, Vector2F{ pos.X + edge * size.X - scale, pos.Y }, Vector2F{ 2.f * scale, size.Y }, kWhite);
	}

	DrawText(canvas, text, Vector2F{ pos.X, pos.Y + size.Y + 2.f * scale }, kTextScale * scale, kWhite);
}

void Overlay::DrawBars(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const AttemptResult* a = data.live ? data.live : data.finished;
	const bool live = data.live != nullptr;
	const int nowTicks = a ? data.latestFrame - a->startFrame : 0;

	const int press = a ? PressToPressTicks(*a, live, nowTicks) : -1;
	const int hold = a ? HoldTicks(*a, live, nowTicks) : -1;
	const int release = a ? ReleaseToPressTicks(*a, live, nowTicks) : -1;

	std::string pressText = TimingText("Press to press", press);
	if (a && press >= 0 && a->gameSpeed < 0.95f)
		pressText += std::format(", about {:.0f} ms real time at {:.0f}% speed", TicksToMs(press) / a->gameSpeed, a->gameSpeed * 100.f);

	const float inf = std::numeric_limits<float>::max();
	const float slack = walldash::kBandSlackMs;

	// The limits follow the speed at the first press, so they settle as soon as
	// an attempt starts.
	std::vector<Band> pressBands;
	std::vector<Band> holdBands;
	const std::vector<Band> noBands;
	if (data.config)
	{
		const walldash::AttemptConfig& c = *data.config;
		const walldash::PressBand& limits = c.PressBandFor(a ? a->speedFirstPress : 0.f);
		pressBands = {
			{ 0.f, *pressEarlyMs, kRed },
			{ *pressEarlyMs, limits.greenMs + slack, kGreen },
			{ limits.greenMs + slack, limits.yellowMs + slack, kYellow },
			{ limits.yellowMs + slack, inf, kRed },
		};
		holdBands = {
			{ 0.f, c.holdGreenMs + slack, kGreen },
			{ c.holdGreenMs + slack, c.holdMaxMs + slack, kYellow },
			{ c.holdMaxMs + slack, inf, kRed },
		};
	}

	const Vector2F size{ 620.f * scale, 22.f * scale };
	const float rowHeight = 56.f * scale;
	Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };

	DrawRangeBar(canvas, pressText, press < 0 ? -1.f : TicksToMs(press), kBarMaxMs, pos, size, scale, pressBands);
	pos.Y += rowHeight;
	DrawRangeBar(canvas, TimingText("Hold", hold), hold < 0 ? -1.f : TicksToMs(hold), kBarMaxMs, pos, size, scale, holdBands);
	pos.Y += rowHeight;
	DrawRangeBar(canvas, TimingText("Release to press", release), release < 0 ? -1.f : TicksToMs(release), kBarMaxMs, pos, size, scale, noBands);
}

void Overlay::DrawVerdict(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	if (!data.finished)
		return;
	const AttemptResult& a = *data.finished;

	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	const float bigScale = kTextScale * 1.6f * scale;
	const std::string word = a.make ? "MAKE" : "MISS";
	DrawText(canvas, word, pos, bigScale, a.make ? kGreen : kRed);

	std::string detail = a.make ? std::string(walldash::ToString(a.wallSide)) + " wall" : a.diagnosisText;
	if (a.spamInput)
		detail += " (spam input, left out of stats)";
	const float wordWidth = canvas.GetStringSize(word, bigScale, bigScale).X;
	DrawText(canvas, detail, Vector2F{ pos.X + wordWidth + 12.f * scale, pos.Y + 5.f * scale }, kTextScale * scale, kWhite);
}

void Overlay::DrawSurfaceNote(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const AttemptResult* a = data.live ? data.live : data.finished;
	if (!a)
		return;

	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	if (a->surface == Surface::Curve)
		DrawText(canvas, "Curve: logged separately", pos, kTextScale * scale, kYellow);
	else if (a->surface == Surface::Corner)
		DrawText(canvas, "Corner: logged separately", pos, kTextScale * scale, kYellow);
}

void Overlay::DrawStick(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const float unit = kStickUnit * scale;
	const Vector2F center{ *panel.x * screen.X + unit * 1.3f, *panel.y * screen.Y + unit * 1.3f };

	const auto toScreen = [&](float pitch, float yaw)
		{
			// Pushing the stick forward gives negative pitch and draws upward.
			return Vector2F{ center.X + yaw * unit, center.Y + pitch * unit };
		};

	const AttemptResult* a = data.live ? data.live : data.finished;

	// Coloured sectors on the rim mark the flip angle bands. Toward the ceiling
	// is stick right on the right wall and stick left on the left wall, so only
	// the current wall's side is shown once a wall is known.
	if (data.config)
	{
		const walldash::AttemptConfig& c = *data.config;
		const auto sector = [&](float side, float fromDeg, float toDeg, const LinearColor& color)
			{
				const float radians = 3.14159265f / 180.f;
				canvas.SetColor(color);
				for (float angle = fromDeg; angle < toDeg; angle += kSectorStepDeg)
				{
					const float next = std::min(angle + kSectorStepDeg, toDeg);
					const auto rimPoint = [&](float deg, float radius)
						{
							return Vector2F{ center.X + side * std::sin(deg * radians) * radius * unit, center.Y - std::cos(deg * radians) * radius * unit };
						};
					const Vector2F a1 = rimPoint(angle, kSectorInner);
					const Vector2F a2 = rimPoint(angle, kSectorOuter);
					const Vector2F b1 = rimPoint(next, kSectorInner);
					const Vector2F b2 = rimPoint(next, kSectorOuter);
					canvas.FillTriangle(a1, a2, b1);
					canvas.FillTriangle(a2, b2, b1);
				}
			};
		const auto drawSide = [&](float side)
			{
				sector(side, c.flipYellowMin, c.flipGreenMin, kYellow);
				sector(side, c.flipGreenMin, c.flipGreenMax, kGreen);
				sector(side, c.flipGreenMax, c.flipYellowMax, kYellow);
			};
		if (a)
		{
			drawSide(a->wallSide == walldash::WallSide::Right ? 1.f : -1.f);
		}
		else
		{
			drawSide(1.f);
			drawSide(-1.f);
		}
	}

	// The stick's full range.
	canvas.SetColor(kWhite);
	const float step = 2.f * 3.14159265f / kCircleSegments;
	for (int i = 0; i < kCircleSegments; i++)
	{
		const Vector2F from{ center.X + std::cos(i * step) * unit, center.Y + std::sin(i * step) * unit };
		const Vector2F to{ center.X + std::cos((i + 1) * step) * unit, center.Y + std::sin((i + 1) * step) * unit };
		canvas.DrawLine(from, to, std::max(1.f, 1.5f * scale));
	}
	canvas.SetColor(kFaint);
	canvas.DrawLine(Vector2F{ center.X - unit, center.Y }, Vector2F{ center.X + unit, center.Y }, 1.f);
	canvas.DrawLine(Vector2F{ center.X, center.Y - unit }, Vector2F{ center.X, center.Y + unit }, 1.f);

	// The game flips once |pitch| + |yaw| + |roll| reaches the deadzone, which
	// makes the stick's deadzone a diamond, not a ring.
	if (a && a->dodgeDeadzone > 0.f)
	{
		const float d = a->dodgeDeadzone;
		const Vector2F top = toScreen(-d, 0.f);
		const Vector2F right = toScreen(0.f, d);
		const Vector2F bottom = toScreen(d, 0.f);
		const Vector2F left = toScreen(0.f, -d);
		canvas.SetColor(kYellow);
		canvas.DrawLine(top, right, 1.5f * scale);
		canvas.DrawLine(right, bottom, 1.5f * scale);
		canvas.DrawLine(bottom, left, 1.5f * scale);
		canvas.DrawLine(left, top, 1.5f * scale);
	}

	if (!a || a->stickPath.empty())
		return;

	const int count = static_cast<int>(a->stickPath.size());
	const int last = a->pressToPressTicks >= 0 ? std::min(a->pressToPressTicks, count - 1) : count - 1;

	for (int i = 0; i <= last; i++)
		DrawDot(canvas, toScreen(a->stickPath[i].pitch, a->stickPath[i].yaw), 4.f * scale, kWhite);

	const auto marker = [&](int index, const LinearColor& color)
		{
			if (index >= 0 && index < count)
				DrawDot(canvas, toScreen(a->stickPath[index].pitch, a->stickPath[index].yaw), 11.f * scale, color);
		};
	marker(0, kNeutral);
	marker(a->holdTicks, kYellow);
	marker(a->pressToPressTicks, kGreen);

	// Laid out from the measured text widths, so the labels never run together.
	const float legendScale = kTextScale * 0.8f * scale;
	const float legendGap = 18.f * scale;
	Vector2F legendPos{ center.X - unit * 1.3f, center.Y + unit * 1.4f };
	const auto legendEntry = [&](const std::string& text, const LinearColor& color)
		{
			DrawText(canvas, text, legendPos, legendScale, color);
			legendPos.X += canvas.GetStringSize(text, legendScale, legendScale).X + legendGap;
		};
	legendEntry("1st press", kNeutral);
	legendEntry("release", kYellow);
	legendEntry("2nd press", kGreen);

	if (!std::isnan(a->flipDirForward))
	{
		LinearColor lineColor = kFlipLine;
		if (!a->flipAngleUndefined && data.config)
		{
			const walldash::AttemptConfig& c = *data.config;
			const float flip = a->flipAngleDeg;
			if (flip >= c.flipGreenMin && flip <= c.flipGreenMax) lineColor = kGreen;
			else if (flip >= c.flipYellowMin && flip <= c.flipYellowMax) lineColor = kYellow;
			else lineColor = kRed;
		}

		const Vector2F tip = toScreen(-a->flipDirForward, a->flipDirRight);
		canvas.SetColor(lineColor);
		canvas.DrawLine(center, tip, 3.f * scale);

		// Placed past the coloured rim so it stays readable.
		const std::string angle = a->flipAngleUndefined ? "flip undefined" : std::format("{:.0f} deg", a->flipAngleDeg);
		const Vector2F label = toScreen(-a->flipDirForward * kSectorOuter, a->flipDirRight * kSectorOuter);
		const float labelScale = kTextScale * 0.85f * scale;
		const float labelWidth = canvas.GetStringSize(angle, labelScale, labelScale).X;
		const float labelX = a->flipDirRight < 0.f ? label.X - labelWidth - 8.f * scale : label.X + 8.f * scale;
		DrawText(canvas, angle, Vector2F{ labelX, label.Y - 8.f * scale }, labelScale, lineColor);
	}
}

void Overlay::DrawSpeed(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	const Vector2F size{ 380.f * scale, 20.f * scale };
	const AttemptResult* a = data.finished;
	const bool hasGain = a && !std::isnan(a->speedGain);

	FillRect(canvas, pos, size, kBackground);

	// Zero is in the middle, so a loss fills left in red and a gain fills right.
	const float middle = pos.X + size.X / 2.f;
	if (hasGain)
	{
		const float amount = std::clamp(a->speedGain / kSpeedBarRange, -1.f, 1.f) * size.X / 2.f;
		if (amount >= 0.f)
			FillRect(canvas, Vector2F{ middle, pos.Y }, Vector2F{ amount, size.Y }, a->speedGain < *speedRedBelow ? kRed : kGreen);
		else
			FillRect(canvas, Vector2F{ middle + amount, pos.Y }, Vector2F{ -amount, size.Y }, kRed);
	}

	canvas.SetColor(kWhite);
	canvas.SetPosition(pos);
	canvas.DrawBox(size);
	FillRect(canvas, Vector2F{ middle - scale, pos.Y }, Vector2F{ 2.f * scale, size.Y }, kWhite);

	const std::string text = hasGain
		? std::format("Speed {:+.0f} uu/s ({:.0f} to {:.0f})", a->speedGain, a->speedFirstPress, a->speedPeak)
		: "Speed: no reattach";
	DrawText(canvas, text, Vector2F{ pos.X, pos.Y + size.Y + 2.f * scale }, kTextScale * scale, kWhite);
}

void Overlay::DrawDistance(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	if (!data.finished)
		return;
	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	DrawText(canvas, std::format("Max distance from wall: {:.0f} uu", data.finished->maxWallDistance), pos, kTextScale * scale, kWhite);
}

void Overlay::DrawChain(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	const LinearColor color = data.chainCurrent > 0 ? kGreen : kWhite;
	DrawText(canvas, std::format("Chain {} (best {})", data.chainCurrent, data.chainBest), pos, kTextScale * 1.3f * scale, color);
}

void Overlay::DrawSpeedLoss(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	const std::string text = std::isnan(data.chainLastSpeedLost)
		? "Lost between dashes: none yet"
		: std::format("Lost between dashes: {:.0f} uu/s", data.chainLastSpeedLost);
	DrawText(canvas, text, pos, kTextScale * scale, kWhite);
}

void Overlay::DrawChainSample(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	const Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	const int target = static_cast<int>(*sampleTarget);
	const bool done = data.chainSample.left >= target && data.chainSample.right >= target;
	DrawText(canvas, std::format("Chain sample: left {}/{}, right {}/{}", data.chainSample.left, target, data.chainSample.right, target), pos, kTextScale * scale, done ? kGreen : kWhite);
}

void Overlay::DrawStats(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale)
{
	if (!data.stats)
		return;
	const std::vector<std::string> lines = data.stats->Lines(*statsByWall);
	const float rowHeight = 22.f * scale;

	// More lines appear as more kinds of attempt are played, so the panel moves
	// up if needed to stay on screen.
	Vector2F pos{ *panel.x * screen.X, *panel.y * screen.Y };
	pos.Y = std::max(0.f, std::min(pos.Y, screen.Y - rowHeight * lines.size() - 4.f * scale));

	for (const std::string& line : lines)
	{
		DrawText(canvas, line, pos, kTextScale * scale, kWhite);
		pos.Y += rowHeight;
	}
}
