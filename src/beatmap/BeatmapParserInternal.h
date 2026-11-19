#pragma once

#include "AnimationCallbacks.h"
#include "WireColorRGB.h"

namespace BeatScene {

enum BeatmapTokenType { TOKEN_OBJECT, TOKEN_ARRAY, TOKEN_STRING, TOKEN_PRIMITIVE };
struct BeatmapJsonToken { Int start, end, next; BeatmapTokenType type; };
struct BeatmapScheduledEvent {
	Float beat;
	UInt lane, value, paletteSlot;
	Wire::ColorRGB color;
	Float brightness;
	Bool floatFX, colorBoost, boostEnabled;
};

extern BeatmapJsonToken gTokens[];
extern UInt gTokenCount;
extern BeatmapScheduledEvent gEvents[];
extern UInt gEventCount;

Int FindKey(const char *json, Int objectIndex, const char *name);
Int FindAnyKey(const char *json, Int object, const char *primary,
	const char *alternate);
Float TokenFloat(const char *json, Int index, Float fallback);
Int TokenInt(const char *json, Int index, Int fallback);
Bool TokenBool(const char *json, Int index, Bool fallback);
void QueueColorEvent(Float beat, UInt lane, UInt value, UInt paletteSlot,
	Float brightness);
void QueueFloatFXEvent(Float beat, UInt lane, Float value);
void QueueColorBoostEvent(Float beat, Bool enabled);

void ParseBeatmapV3Events(const char *json, Int root,
	const EnvironmentAnimationCallbacks &handler);
void ParseBeatmapV4Events(const char *json, Int root,
	const EnvironmentAnimationCallbacks &handler);

} // namespace BeatScene
