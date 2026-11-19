#include "BeatmapLights.h"
#include "AnimationCallbacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace BeatScene {

enum { MAX_JSON_TOKENS = 65536, MAX_CALLBACK_EVENTS = 8192, MAX_JSON_DEPTH = 48 };
enum TokenType { TOKEN_OBJECT, TOKEN_ARRAY, TOKEN_STRING, TOKEN_PRIMITIVE };

struct JsonToken { Int start, end, next; TokenType type; };
struct ScheduledCallback {
	Float beat;
	UInt lane, value;
	Wire::ColorRGB color;
	Float brightness;
	Bool floatFX;
};

static JsonToken gTokens[MAX_JSON_TOKENS];
static ScheduledCallback gEvents[MAX_CALLBACK_EVENTS];
static Char gJson[1024 * 1024];
static UInt gTokenCount = 0, gEventCount = 0, gNextEvent = 0;
static Bool gLoaded = false;
static Wire::ColorRGB gLaneColor[20];
static Wire::ColorRGB gTweenStart[20], gTweenEnd[20];
static Float gTweenStartTime[20], gTweenDuration[20];
static Wire::ColorRGB gFlashColor[20];
static Float gFlashStartTime[20];
static Bool gTweenActive[20], gFlashActive[20];
static Bool gTweenFadeOut[20];

static void SkipSpace(const char *json, Int &pos) {
	while (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n') ++pos;
}

static Int ParseValue(const char *json, Int &pos, Int depth) {
	if (depth > MAX_JSON_DEPTH || gTokenCount >= MAX_JSON_TOKENS) return -1;
	SkipSpace(json, pos);
	const Int index = static_cast<Int>(gTokenCount++);
	JsonToken &token = gTokens[index];
	token.start = pos; token.end = pos; token.next = index + 1;
	if (json[pos] == '{' || json[pos] == '[') {
		const Char close = json[pos++] == '{' ? '}' : ']';
		token.type = close == '}' ? TOKEN_OBJECT : TOKEN_ARRAY;
		SkipSpace(json, pos);
		while (json[pos] && json[pos] != close) {
			if (json[pos] == ',' || json[pos] == ':') { ++pos; continue; }
			if (ParseValue(json, pos, depth + 1) < 0) return -1;
			SkipSpace(json, pos);
		}
		if (json[pos] != close) return -1;
		++pos; token.end = pos; token.next = static_cast<Int>(gTokenCount);
	} else if (json[pos] == '"') {
		token.type = TOKEN_STRING;
		++pos; token.start = pos;
		while (json[pos]) {
			if (json[pos] == '\\' && json[pos + 1]) { pos += 2; continue; }
			if (json[pos++] == '"') break;
		}
		token.end = pos - 1;
	} else {
		token.type = TOKEN_PRIMITIVE;
		while (json[pos] && json[pos] != ',' && json[pos] != ']' && json[pos] != '}' &&
			json[pos] != ' ' && json[pos] != '\t' && json[pos] != '\r' && json[pos] != '\n') ++pos;
		token.end = pos;
	}
	return index;
}

static Bool TokenEquals(const char *json, const JsonToken &token, const char *text) {
	const Int length = token.end - token.start;
	return token.type == TOKEN_STRING && static_cast<Int>(strlen(text)) == length &&
		memcmp(json + token.start, text, length) == 0;
}

static Int FindKey(const char *json, Int objectIndex, const char *name) {
	if (objectIndex < 0 || gTokens[objectIndex].type != TOKEN_OBJECT) return -1;
	for (Int i = objectIndex + 1; i < gTokens[objectIndex].next;) {
		const Int key = i;
		const Int value = key + 1;
		if (value >= gTokens[objectIndex].next) break;
		if (TokenEquals(json, gTokens[key], name)) return value;
		i = gTokens[value].next;
	}
	return -1;
}

static Float TokenFloat(const char *json, Int index, Float fallback) {
	if (index < 0) return fallback;
	Char value[32];
	Int length = gTokens[index].end - gTokens[index].start;
	if (length <= 0) return fallback;
	if (length >= static_cast<Int>(sizeof(value))) length = sizeof(value) - 1;
	memcpy(value, json + gTokens[index].start, length);
	value[length] = '\0';
	return static_cast<Float>(atof(value));
}

static Int TokenInt(const char *json, Int index, Int fallback) {
	if (index < 0) return fallback;
	Char value[32];
	Int length = gTokens[index].end - gTokens[index].start;
	if (length <= 0) return fallback;
	if (length >= static_cast<Int>(sizeof(value))) length = sizeof(value) - 1;
	memcpy(value, json + gTokens[index].start, length);
	value[length] = '\0';
	return atoi(value);
}

static Int FindAnyKey(const char *json, Int object, const char *primary,
	const char *alternate) {
	const Int index = FindKey(json, object, primary);
	return index >= 0 || !alternate ? index : FindKey(json, object, alternate);
}

static Wire::ColorRGB ReadColor(const char *json, Int object,
	const Char *key, Wire::ColorRGB fallback, Bool &found) {
	const Int color = FindKey(json, object, key);
	if (color < 0) return fallback;
	if (gTokens[color].type == TOKEN_STRING) {
		Int start = gTokens[color].start;
		Int length = gTokens[color].end - start;
		if (length == 7 || length == 9) {
			if (json[start] == '#') { ++start; --length; }
			UInt channels[3] = {0, 0, 0};
			for (UInt c = 0; c < 3; ++c) {
				for (UInt n = 0; n < 2; ++n) {
					const Char h = json[start + c * 2 + n];
					const UInt digit = h >= '0' && h <= '9' ? h - '0' :
						(h >= 'a' && h <= 'f' ? h - 'a' + 10 :
						(h >= 'A' && h <= 'F' ? h - 'A' + 10 : 16));
					if (digit > 15) return fallback;
					channels[c] = channels[c] * 16 + digit;
				}
			}
			found = true;
			return Wire::ColorRGB(channels[0] / 255.0F, channels[1] / 255.0F,
				channels[2] / 255.0F);
		}
		return fallback;
	}
	const Int r = FindKey(json, color, "r");
	const Int g = FindKey(json, color, "g");
	const Int b = FindKey(json, color, "b");
	if (r < 0 || g < 0 || b < 0) return fallback;
	found = true;
	return Wire::ColorRGB(TokenFloat(json, r, fallback.R()),
		TokenFloat(json, g, fallback.G()), TokenFloat(json, b, fallback.B()));
}

static void ReadBeatmapColors(const char *beatmapPath,
	Wire::ColorRGB &color0, Wire::ColorRGB &color1, Wire::ColorRGB &colorW) {
	Char infoPath[256];
	const Char *slash = strrchr(beatmapPath, '/');
	if (!slash || static_cast<size_t>(slash - beatmapPath) + 10 >= sizeof(infoPath)) return;
	memcpy(infoPath, beatmapPath, slash - beatmapPath + 1);
	strcpy(infoPath + (slash - beatmapPath) + 1, "Info.dat");
	FILE *file = fopen(infoPath, "rb");
	if (!file) return;
	const size_t length = fread(gJson, 1, sizeof(gJson) - 1, file);
	fclose(file);
	if (length == sizeof(gJson) - 1) return;
	gJson[length] = '\0';
	gTokenCount = 0;
	Int pos = 0;
	if (ParseValue(gJson, pos, 0) < 0 || !gTokenCount) return;
	Int schemes = FindAnyKey(gJson, 0, "_colorSchemes", "colorSchemes");
	if (schemes < 0 || gTokens[schemes].type != TOKEN_ARRAY ||
		schemes + 1 >= gTokens[schemes].next) return;
	const Int entry = schemes + 1;
	const Int overrideLights = FindKey(gJson, entry, "overrideLights");
	const Int useOverride = FindKey(gJson, entry, "useOverride");
	if ((overrideLights >= 0 && TokenInt(gJson, overrideLights, 0) == 0) ||
		(overrideLights < 0 && useOverride >= 0 && TokenInt(gJson, useOverride, 0) == 0)) return;
	Int scheme = FindKey(gJson, entry, "colorScheme");
	if (scheme < 0) scheme = entry;
	Bool found = false;
	color0 = ReadColor(gJson, scheme, "environmentColor0", color0, found);
	color1 = ReadColor(gJson, scheme, "environmentColor1", color1, found);
	colorW = ReadColor(gJson, scheme, "environmentColorW", colorW, found);
	if (found) printf("[WiiSaber] using beatmap-declared environment colors from %s\n", infoPath);
}

static void AddCallback(Float beat, UInt lane, UInt value,
	Wire::ColorRGB color, Float brightness) {
	if (gEventCount >= MAX_CALLBACK_EVENTS) return;
	ScheduledCallback &event = gEvents[gEventCount++];
	event.beat = beat; event.lane = lane; event.value = value; event.color = color;
	event.brightness = brightness;
	event.floatFX = false;
}

static void AddFloatFXCallback(Float beat, UInt lane, Float value) {
	if (gEventCount >= MAX_CALLBACK_EVENTS) return;
	ScheduledCallback &event = gEvents[gEventCount++];
	event.beat = beat; event.lane = lane;
	event.value = static_cast<UInt>(value);
	event.color = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
	event.brightness = value;
	event.floatFX = true;
}

static void AddBasicEvents(const char *json, Int arrayIndex,
	Wire::ColorRGB color0, Wire::ColorRGB color1, Wire::ColorRGB colorW,
	const EnvironmentAnimationCallbacks &handler) {
	if (arrayIndex < 0 || gTokens[arrayIndex].type != TOKEN_ARRAY) return;
	for (Int i = arrayIndex + 1; i < gTokens[arrayIndex].next &&
		gEventCount < MAX_CALLBACK_EVENTS; i = gTokens[i].next) {
		if (gTokens[i].type != TOKEN_OBJECT) continue;
		const Int beatIndex = FindAnyKey(json, i, "_time", "b");
		const Int typeIndex = FindAnyKey(json, i, "_type", "et");
		const Int valueIndex = FindAnyKey(json, i, "_value", "i");
		if (beatIndex < 0 || typeIndex < 0 || valueIndex < 0) continue;
		const Int type = TokenInt(json, typeIndex, -1);
		const Int value = TokenInt(json, valueIndex, -1);
		const Float beat = TokenFloat(json, beatIndex, 0.0F);
		if (type < 0 || type > 19 || value < 0) continue;
		UInt laneCount = 0;
		const AnimationLaneRegistration *lanes = handler.GetLanes(laneCount);
		const AnimationLaneKind *kind = NULL;
		for (UInt lane = 0; lanes && lane < laneCount; ++lane)
			if (lanes[lane].id == static_cast<UInt>(type)) { kind = &lanes[lane].kind; break; }
		if (!kind) continue;
		if (*kind == ANIMATION_LANE_COLOUR && value <= 12) {
			const Int brightnessIndex = FindAnyKey(json, i, "_floatValue", "f");
			Wire::ColorRGB color(0.0F, 0.0F, 0.0F);
			if (value > 0 && value <= 4) color = color0;
			else if (value <= 8) color = color1;
			else if (value <= 12) color = colorW;
			AddCallback(beat, static_cast<UInt>(type), static_cast<UInt>(value),
				color, TokenFloat(json, brightnessIndex, 1.0F));
		} else if (*kind == ANIMATION_LANE_FLOAT_FX) {
			AddFloatFXCallback(beat, static_cast<UInt>(type),
				TokenFloat(json, valueIndex, 0.0F));
		}
	}
}

Bool LoadBeatmapCallbacks(const char *path, const EnvironmentAnimationCallbacks &handler,
	Wire::ColorRGB color0, Wire::ColorRGB color1, Wire::ColorRGB colorW) {
	gEventCount = gNextEvent = gTokenCount = 0;
	gLoaded = false;
	for (UInt lane = 0; lane < 20; ++lane) {
		gLaneColor[lane] = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
		gTweenActive[lane] = false;
		gFlashActive[lane] = false;
		gTweenFadeOut[lane] = false;
	}
	if (!path || !*path) return false;
	ReadBeatmapColors(path, color0, color1, colorW);
	FILE *file = fopen(path, "rb");
	if (!file) {
		printf("[WiiSaber] beatmap open failed: %s\n", path);
		return false;
	}
	const size_t length = fread(gJson, 1, sizeof(gJson) - 1, file);
	fclose(file);
	if (length == sizeof(gJson) - 1) {
		printf("[WiiSaber] beatmap is too large: %s\n", path);
		return false;
	}
	gJson[length] = '\0';
	gTokenCount = 0;
	Int pos = 0;
	if (ParseValue(gJson, pos, 0) < 0 || gTokenCount == 0 ||
		gTokens[0].type != TOKEN_OBJECT) {
		printf("[WiiSaber] beatmap JSON parse failed: %s\n", path);
		return false;
	}
	Int basicEvents = FindAnyKey(gJson, 0, "_events", "basicBeatmapEvents");
	AddBasicEvents(gJson, basicEvents, color0, color1, colorW, handler);
	for (UInt i = 1; i < gEventCount; ++i) {
		ScheduledCallback current = gEvents[i];
		UInt j = i;
		while (j > 0 && gEvents[j - 1].beat > current.beat) {
			gEvents[j] = gEvents[j - 1];
			--j;
		}
		gEvents[j] = current;
	}
	gLoaded = true;
	printf("[WiiSaber] beatmap callbacks loaded: %s (%u events)\n", path, gEventCount);
	return true;
}

void StartBeatmapCallbacks() {
	gNextEvent = 0;
	for (UInt lane = 0; lane < 20; ++lane) {
		gLaneColor[lane] = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
		gTweenActive[lane] = false;
		gFlashActive[lane] = false;
		gTweenFadeOut[lane] = false;
	}
}

static Wire::ColorRGB ScaleColor(Wire::ColorRGB color, Float brightness) {
	if (brightness < 0.0F) brightness = 0.0F;
	if (brightness > 1.0F) brightness = 1.0F;
	return Wire::ColorRGB(color.R() * brightness, color.G() * brightness,
		color.B() * brightness);
}

static void DispatchColor(EnvironmentAnimationCallbacks &callbacks, UInt lane,
	Wire::ColorRGB color) {
	if (lane >= 20) return;
	gLaneColor[lane] = color;
	callbacks.ColourEventLaneCallback(lane, color, 1.0F);
}

static void StartTween(UInt lane, Wire::ColorRGB target, Float now,
	Float duration, Bool fadeOut = false) {
	if (lane >= 20) return;
	gTweenStart[lane] = gLaneColor[lane];
	gTweenEnd[lane] = target;
	gTweenStartTime[lane] = now;
	gTweenDuration[lane] = duration;
	gTweenActive[lane] = true;
	gTweenFadeOut[lane] = fadeOut;
}

void UpdateBeatmapCallbacks(EnvironmentAnimationCallbacks &callbacks,
	Float elapsedSeconds, Float beatsPerMinute) {
	if (!gLoaded || beatsPerMinute <= 0.0F) return;
	const Float beat = elapsedSeconds * beatsPerMinute / 60.0F;
	while (gNextEvent < gEventCount && gEvents[gNextEvent].beat <= beat) {
		const ScheduledCallback &event = gEvents[gNextEvent++];
		if (event.floatFX) {
			callbacks.FloatFXEventLaneCallback(event.lane, event.brightness);
			continue;
		}
		const Float eventTime = event.beat * 60.0F / beatsPerMinute;
		const UInt mode = event.value == 0 ? 0 : (event.value - 1) % 4 + 1;
		const Wire::ColorRGB color = ScaleColor(event.color, event.brightness);
		if (mode == 0) {
			gTweenActive[event.lane] = false;
			gFlashActive[event.lane] = false;
			DispatchColor(callbacks, event.lane, Wire::ColorRGB(0.0F, 0.0F, 0.0F));
		} else if (mode == 1) {
			gTweenActive[event.lane] = false;
			gFlashActive[event.lane] = false;
			DispatchColor(callbacks, event.lane, color);
		} else if (mode == 2) {
			gTweenActive[event.lane] = false;
			gFlashActive[event.lane] = true;
			gFlashColor[event.lane] = color;
			gFlashStartTime[event.lane] = eventTime;
		} else if (mode == 3) {
			gFlashActive[event.lane] = false;
			DispatchColor(callbacks, event.lane, color);
			StartTween(event.lane, Wire::ColorRGB(0.0F, 0.0F, 0.0F),
				eventTime, 1.5F, true);
		} else {
			gFlashActive[event.lane] = false;
			StartTween(event.lane, color, eventTime, 0.6F);
		}
	}
	for (UInt lane = 0; lane < 20; ++lane) {
		if (!gTweenActive[lane]) continue;
		const Float t = (elapsedSeconds - gTweenStartTime[lane]) / gTweenDuration[lane];
		if (t >= 1.0F) {
			gTweenActive[lane] = false;
			DispatchColor(callbacks, lane, gTweenEnd[lane]);
			continue;
		}
		Float amount = t < 0.0F ? 0.0F : t;
		if (gTweenFadeOut[lane] && amount < 1.0F)
			amount = 1.0F - powf(2.0F, -10.0F * amount);
		const Wire::ColorRGB color(
			gTweenStart[lane].R() + (gTweenEnd[lane].R() - gTweenStart[lane].R()) * amount,
			gTweenStart[lane].G() + (gTweenEnd[lane].G() - gTweenStart[lane].G()) * amount,
			gTweenStart[lane].B() + (gTweenEnd[lane].B() - gTweenStart[lane].B()) * amount);
		DispatchColor(callbacks, lane, color);
	}
	for (UInt lane = 0; lane < 20; ++lane) {
		if (!gFlashActive[lane]) continue;
		const Float phase = (elapsedSeconds - gFlashStartTime[lane]) * 12.5663706F;
		const Float brightness = 0.75F + 0.25F * cosf(phase);
		DispatchColor(callbacks, lane, Wire::ColorRGB(
			gFlashColor[lane].R() * brightness,
			gFlashColor[lane].G() * brightness,
			gFlashColor[lane].B() * brightness));
	}
}

}
