#pragma once

#include "bakkesmod/wrappers/canvaswrapper.h"
#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#include "core/Attempt.h"
#include "core/AttemptCsv.h"

#include <memory>
#include <string>
#include <vector>

// Everything the overlay needs for one frame.
struct OverlayData
{
	const walldash::AttemptResult* live = nullptr;     // The attempt in progress, if any.
	const walldash::AttemptResult* finished = nullptr; // The last finished attempt, if any.
	int latestFrame = 0;
	const walldash::SessionStats* stats = nullptr;
	const walldash::AttemptConfig* config = nullptr;
	int chainCurrent = 0;
	int chainBest = 0;
	float chainLastSpeedLost = walldash::kNotMeasured;
	walldash::ChainSampleCounts chainSample;
};

// One movable part of the overlay. Position is a fraction of the screen and
// size is a multiplier, so the layout holds at any resolution.
struct OverlayPanel
{
	std::string id;
	std::string label;
	std::shared_ptr<bool> on = std::make_shared<bool>(true);
	std::shared_ptr<float> x = std::make_shared<float>(0.f);
	std::shared_ptr<float> y = std::make_shared<float>(0.f);
	std::shared_ptr<float> size = std::make_shared<float>(1.f);
};

class Overlay
{
public:
	void RegisterCvars(const std::shared_ptr<CVarManagerWrapper>& cvarManager);
	void Draw(CanvasWrapper& canvas, const OverlayData& data);

	const std::vector<OverlayPanel>& Panels() const { return panels; }
	const std::vector<std::string>& CvarNames() const { return cvarNames; }
	bool StatsByWall() const { return *statsByWall; }

private:
	struct Band
	{
		float min;
		float max;
		LinearColor color;
	};

	void AddPanel(const std::shared_ptr<CVarManagerWrapper>& cvarManager, const std::string& id, const std::string& label, float x, float y);

	void DrawBars(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawVerdict(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawSurfaceNote(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawStick(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawSpeed(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawDistance(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawStats(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawChain(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawSpeedLoss(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);
	void DrawChainSample(CanvasWrapper& canvas, const OverlayPanel& panel, const OverlayData& data, float scale);

	void DrawRangeBar(CanvasWrapper& canvas, const std::string& text, float value, float maxValue, Vector2F pos, Vector2F size, float scale, const std::vector<Band>& bands);

	std::vector<OverlayPanel> panels;
	std::vector<std::string> cvarNames;
	Vector2F screen{ 1920.f, 1080.f };

	std::shared_ptr<bool> enabled = std::make_shared<bool>(true);
	std::shared_ptr<float> pressEarlyMs = std::make_shared<float>(58.f);
	std::shared_ptr<float> speedRedBelow = std::make_shared<float>(0.f);
	std::shared_ptr<bool> statsByWall = std::make_shared<bool>(true);
	std::shared_ptr<bool> chainsOn = std::make_shared<bool>(false);
	std::shared_ptr<bool> sampleCounterOn = std::make_shared<bool>(false);
	std::shared_ptr<float> sampleTarget = std::make_shared<float>(300.f);
};
