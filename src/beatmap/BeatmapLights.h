#pragma once

#include "WireTypes.h"
#include "AnimationCallbacks.h"
#include "WireColorRGB.h"

namespace BeatScene {

// Loads and runs basic events forward from beat zero. Color lanes 0-4 receive
// the parsed palette and tween updates; lanes 5-19 dispatch Float FX values.
Bool LoadBeatmapCallbacks(const char *path,
	const EnvironmentAnimationCallbacks &handler,
	Wire::ColorRGB defaultColor0, Wire::ColorRGB defaultColor1,
	Wire::ColorRGB defaultColorW, Wire::ColorRGB defaultBoostColor0,
	Wire::ColorRGB defaultBoostColor1);
void StartBeatmapCallbacks();
void UpdateBeatmapCallbacks(EnvironmentAnimationCallbacks &callbacks,
	Float elapsedSeconds, Float beatsPerMinute);

}
