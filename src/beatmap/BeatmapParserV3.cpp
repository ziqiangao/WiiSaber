#include "BeatmapParserInternal.h"

namespace BeatScene {
namespace {

static AnimationLaneKind LaneKind(const EnvironmentAnimationCallbacks &handler,
	UInt id, Bool &registered) {
	UInt count = 0;
	const AnimationLaneRegistration *lanes = handler.GetLanes(count);
	for (UInt i = 0; lanes && i < count; ++i) {
		if (lanes[i].id == id) { registered = true; return lanes[i].kind; }
	}
	registered = false;
	return ANIMATION_LANE_COLOUR;
}

static UInt PaletteSlot(Int value) {
	if (value <= 0) return 3;
	if (value <= 4) return 0;
	if (value <= 8) return 1;
	return 2;
}

} // namespace

void ParseBeatmapV3Events(const char *json, Int root,
	const EnvironmentAnimationCallbacks &handler) {
	Int events = FindAnyKey(json, root, "_events", "basicBeatmapEvents");
	if (events >= 0 && gTokens[events].type == TOKEN_ARRAY) {
		for (Int i = events + 1; i < gTokens[events].next; i = gTokens[i].next) {
			if (gTokens[i].type != TOKEN_OBJECT) continue;
			const Int time = FindAnyKey(json, i, "_time", "b");
			const Int type = FindAnyKey(json, i, "_type", "et");
			const Int value = FindAnyKey(json, i, "_value", "i");
			if (time < 0 || type < 0 || value < 0) continue;
			const Int id = TokenInt(json, type, -1);
			const Int val = TokenInt(json, value, -1);
			const Float beat = TokenFloat(json, time, 0.0F);
			if (id == 5) {
				QueueColorBoostEvent(beat, val != 0);
				continue;
			}
			if (id < 0 || id > 19 || val < 0) continue;
			Bool registered = false;
			const AnimationLaneKind kind = LaneKind(handler, id, registered);
			if (!registered) continue;
			if (kind == ANIMATION_LANE_COLOUR && val <= 12) {
				const Int brightness = FindAnyKey(json, i, "_floatValue", "f");
				QueueColorEvent(beat, id, val, PaletteSlot(val),
					TokenFloat(json, brightness, 1.0F));
			} else if (kind == ANIMATION_LANE_FLOAT_FX) {
				QueueFloatFXEvent(beat, id, TokenFloat(json, value, 0.0F));
			}
		}
	}
	const Int boosts = FindAnyKey(json, root, "_colorBoostBeatmapEvents",
		"colorBoostBeatmapEvents");
	if (boosts < 0 || gTokens[boosts].type != TOKEN_ARRAY) return;
	for (Int i = boosts + 1; i < gTokens[boosts].next; i = gTokens[i].next) {
		if (gTokens[i].type != TOKEN_OBJECT) continue;
		const Int beat = FindAnyKey(json, i, "_time", "b");
		const Int boost = FindAnyKey(json, i, "_value", "o");
		if (beat >= 0 && boost >= 0)
			QueueColorBoostEvent(TokenFloat(json, beat, 0.0F),
				TokenBool(json, boost, false));
	}
}

} // namespace BeatScene
