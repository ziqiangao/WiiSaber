#include "BeatmapParserInternal.h"

namespace BeatScene {
namespace {

static Int ArrayEntry(const char *json, Int array, UInt entry) {
	if (array < 0 || gTokens[array].type != TOKEN_ARRAY) return -1;
	Int item = array + 1;
	while (entry-- && item < gTokens[array].next) item = gTokens[item].next;
	return item < gTokens[array].next ? item : -1;
}

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

void ParseBeatmapV4Events(const char *json, Int root,
	const EnvironmentAnimationCallbacks &handler) {
	const Int events = FindKey(json, root, "basicEvents");
	const Int eventData = FindKey(json, root, "basicEventsData");
	if (events >= 0 && eventData >= 0 && gTokens[events].type == TOKEN_ARRAY &&
		gTokens[eventData].type == TOKEN_ARRAY) {
		for (Int i = events + 1; i < gTokens[events].next; i = gTokens[i].next) {
			if (gTokens[i].type != TOKEN_OBJECT) continue;
			const Int time = FindKey(json, i, "b");
			const Int index = FindKey(json, i, "i");
			if (time < 0 || index < 0) continue;
			const Int data = ArrayEntry(json, eventData,
				static_cast<UInt>(TokenInt(json, index, -1)));
			if (data < 0) continue;
			const Int type = FindKey(json, data, "t");
			const Int value = FindKey(json, data, "i");
			if (type < 0 || value < 0) continue;
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
				const Int brightness = FindKey(json, data, "f");
				QueueColorEvent(beat, id, val, PaletteSlot(val),
					TokenFloat(json, brightness, 1.0F));
			} else if (kind == ANIMATION_LANE_FLOAT_FX) {
				QueueFloatFXEvent(beat, id, TokenFloat(json, value, 0.0F));
			}
		}
	}

	const Int boosts = FindKey(json, root, "colorBoostEvents");
	const Int boostData = FindKey(json, root, "colorBoostEventsData");
	if (boosts < 0 || boostData < 0 || gTokens[boosts].type != TOKEN_ARRAY ||
		gTokens[boostData].type != TOKEN_ARRAY) return;
	for (Int i = boosts + 1; i < gTokens[boosts].next; i = gTokens[i].next) {
		if (gTokens[i].type != TOKEN_OBJECT) continue;
		const Int time = FindKey(json, i, "b");
		const Int index = FindKey(json, i, "i");
		if (time < 0 || index < 0) continue;
		const Int data = ArrayEntry(json, boostData,
			static_cast<UInt>(TokenInt(json, index, -1)));
		if (data < 0) continue;
		const Int enabled = FindKey(json, data, "b");
		if (enabled >= 0)
			QueueColorBoostEvent(TokenFloat(json, time, 0.0F),
				TokenBool(json, enabled, false));
	}
}

} // namespace BeatScene
