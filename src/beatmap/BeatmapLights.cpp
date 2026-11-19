#include "BeatmapLights.h"
#include "AnimationCallbacks.h"
#include "BeatmapParserInternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace BeatScene {

enum { MAX_JSON_TOKENS = 65536, MAX_CALLBACK_EVENTS = 8192, MAX_JSON_DEPTH = 48 };
BeatmapJsonToken gTokens[MAX_JSON_TOKENS];
BeatmapScheduledEvent gEvents[MAX_CALLBACK_EVENTS];
static Char gJson[1024 * 1024];
UInt gTokenCount = 0, gEventCount = 0;
static UInt gNextEvent = 0;
static Bool gLoaded = false;
static Wire::ColorRGB gLaneColor[20];
static Wire::ColorRGB gTweenStart[20], gTweenEnd[20];
static Float gTweenStartTime[20], gTweenDuration[20];
static Wire::ColorRGB gFlashColor[20];
static Float gFlashStartTime[20];
static Bool gTweenActive[20], gFlashActive[20];
static Bool gTweenFadeOut[20];
static UInt gLanePaletteSlot[20];
static Float gLaneBrightness[20];
static Wire::ColorRGB gPalette0, gPalette1, gPaletteW;
static Wire::ColorRGB gBoostPalette0, gBoostPalette1;
static Bool gBoostActive = false;

static void SkipSpace(const char *json, Int &pos) {
	while (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n') ++pos;
}

static Int ParseValue(const char *json, Int &pos, Int depth) {
	if (depth > MAX_JSON_DEPTH || gTokenCount >= MAX_JSON_TOKENS) return -1;
	SkipSpace(json, pos);
	const Int index = static_cast<Int>(gTokenCount++);
	BeatmapJsonToken &token = gTokens[index];
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

static Bool TokenEquals(const char *json, const BeatmapJsonToken &token, const char *text) {
	const Int length = token.end - token.start;
	return token.type == TOKEN_STRING && static_cast<Int>(strlen(text)) == length &&
		memcmp(json + token.start, text, length) == 0;
}

Int FindKey(const char *json, Int objectIndex, const char *name) {
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

Float TokenFloat(const char *json, Int index, Float fallback) {
	if (index < 0) return fallback;
	Char value[32];
	Int length = gTokens[index].end - gTokens[index].start;
	if (length <= 0) return fallback;
	if (length >= static_cast<Int>(sizeof(value))) length = sizeof(value) - 1;
	memcpy(value, json + gTokens[index].start, length);
	value[length] = '\0';
	return static_cast<Float>(atof(value));
}

Int TokenInt(const char *json, Int index, Int fallback) {
	if (index < 0) return fallback;
	Char value[32];
	Int length = gTokens[index].end - gTokens[index].start;
	if (length <= 0) return fallback;
	if (length >= static_cast<Int>(sizeof(value))) length = sizeof(value) - 1;
	memcpy(value, json + gTokens[index].start, length);
	value[length] = '\0';
	return atoi(value);
}

Bool TokenBool(const char *json, Int index, Bool fallback) {
	if (index < 0) return fallback;
	if (gTokens[index].type == TOKEN_PRIMITIVE) {
		const Int length = gTokens[index].end - gTokens[index].start;
		if (length == 4 && memcmp(json + gTokens[index].start, "true", 4) == 0) return true;
		if (length == 5 && memcmp(json + gTokens[index].start, "false", 5) == 0) return false;
	}
	return TokenInt(json, index, fallback ? 1 : 0) != 0;
}

Int FindAnyKey(const char *json, Int object, const char *primary,
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
		if (length == 6 || length == 7 || length == 8 || length == 9) {
			if (json[start] == '#') { ++start; --length; }
			if (length != 6 && length != 8) return fallback;
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

static Int ArrayEntry(Int array, UInt entry) {
	if (array < 0 || gTokens[array].type != TOKEN_ARRAY) return -1;
	Int item = array + 1;
	while (entry-- && item < gTokens[array].next) item = gTokens[item].next;
	return item < gTokens[array].next ? item : -1;
}

static UInt FindColorSchemeIndex(const char *json, const Char *mapFilename) {
	Int difficulties = FindKey(json, 0, "difficultyBeatmaps");
	if (difficulties >= 0 && gTokens[difficulties].type == TOKEN_ARRAY) {
		for (Int i = difficulties + 1; i < gTokens[difficulties].next; i = gTokens[i].next) {
			Int filename = FindAnyKey(json, i, "beatmapDataFilename", "beatmapFilename");
			if (filename < 0) filename = FindKey(json, i, "_beatmapFilename");
			if (filename < 0 || !TokenEquals(json, gTokens[filename], mapFilename)) continue;
			Int index = FindAnyKey(json, i, "beatmapColorSchemeIdx", "_beatmapColorSchemeIdx");
			return static_cast<UInt>(TokenInt(json, index, 0));
		}
	}
	const Int sets = FindKey(json, 0, "_difficultyBeatmapSets");
	if (sets >= 0 && gTokens[sets].type == TOKEN_ARRAY) {
		for (Int s = sets + 1; s < gTokens[sets].next; s = gTokens[s].next) {
			const Int maps = FindKey(json, s, "_difficultyBeatmaps");
			if (maps < 0 || gTokens[maps].type != TOKEN_ARRAY) continue;
			for (Int i = maps + 1; i < gTokens[maps].next; i = gTokens[i].next) {
				const Int filename = FindKey(json, i, "_beatmapFilename");
				if (filename < 0 || !TokenEquals(json, gTokens[filename], mapFilename)) continue;
				const Int index = FindKey(json, i, "_beatmapColorSchemeIdx");
				return static_cast<UInt>(TokenInt(json, index, 0));
			}
		}
	}
	return 0;
}

static void ReadBeatmapColors(const char *beatmapPath) {
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
	const Char *filename = strrchr(beatmapPath, '/') + 1;
	UInt schemeIndex = FindColorSchemeIndex(gJson, filename);
	Int entry = ArrayEntry(schemes, schemeIndex);
	if (entry < 0) entry = schemes + 1;
	const Int overrideLights = FindKey(gJson, entry, "overrideLights");
	const Int useOverride = FindKey(gJson, entry, "useOverride");
	if ((overrideLights >= 0 && !TokenBool(gJson, overrideLights, true)) ||
		(overrideLights < 0 && useOverride >= 0 && !TokenBool(gJson, useOverride, true))) return;
	Int scheme = FindKey(gJson, entry, "colorScheme");
	if (scheme < 0) scheme = entry;
	Bool found = false;
	gPalette0 = ReadColor(gJson, scheme, "environmentColor0", gPalette0, found);
	gPalette1 = ReadColor(gJson, scheme, "environmentColor1", gPalette1, found);
	gPaletteW = ReadColor(gJson, scheme, "environmentColorW", gPaletteW, found);
	gBoostPalette0 = ReadColor(gJson, scheme, "environmentColor0Boost", gBoostPalette0, found);
	gBoostPalette1 = ReadColor(gJson, scheme, "environmentColor1Boost", gBoostPalette1, found);
	if (found) printf("[WiiSaber] using beatmap-declared environment colors from %s\n", infoPath);
}

static void ReadV4Lightshow(const char *beatmapPath,
	const EnvironmentAnimationCallbacks &handler) {
	Char infoPath[256], lightshowPath[256];
	const Char *slash = strrchr(beatmapPath, '/');
	if (!slash) return;
	const size_t directoryLength = static_cast<size_t>(slash - beatmapPath) + 1;
	if (directoryLength + 10 >= sizeof(infoPath)) return;
	memcpy(infoPath, beatmapPath, directoryLength);
	strcpy(infoPath + directoryLength, "Info.dat");
	FILE *file = fopen(infoPath, "rb");
	if (!file) return;
	size_t length = fread(gJson, 1, sizeof(gJson) - 1, file);
	fclose(file);
	if (length == sizeof(gJson) - 1) return;
	gJson[length] = '\0';
	gTokenCount = 0;
	Int pos = 0;
	if (ParseValue(gJson, pos, 0) < 0) return;
	Int difficulties = FindKey(gJson, 0, "difficultyBeatmaps");
	if (difficulties < 0 || gTokens[difficulties].type != TOKEN_ARRAY) return;
	const Char *mapFilename = slash + 1;
	Int lightshowFilename = -1;
	for (Int i = difficulties + 1; i < gTokens[difficulties].next; i = gTokens[i].next) {
		const Int beatmapFilename = FindKey(gJson, i, "beatmapDataFilename");
		if (beatmapFilename < 0 || !TokenEquals(gJson, gTokens[beatmapFilename], mapFilename)) continue;
		lightshowFilename = FindKey(gJson, i, "lightshowDataFilename");
		break;
	}
	if (lightshowFilename < 0 || gTokens[lightshowFilename].type != TOKEN_STRING) return;
	const Int nameLength = gTokens[lightshowFilename].end - gTokens[lightshowFilename].start;
	if (nameLength <= 0 || directoryLength + static_cast<size_t>(nameLength) >= sizeof(lightshowPath)) return;
	memcpy(lightshowPath, beatmapPath, directoryLength);
	memcpy(lightshowPath + directoryLength, gJson + gTokens[lightshowFilename].start, nameLength);
	lightshowPath[directoryLength + nameLength] = '\0';
	file = fopen(lightshowPath, "rb");
	if (!file) {
		printf("[WiiSaber] v4 lightshow missing: %s\n", lightshowPath);
		return;
	}
	length = fread(gJson, 1, sizeof(gJson) - 1, file);
	fclose(file);
	if (length == sizeof(gJson) - 1) return;
	gJson[length] = '\0';
	gTokenCount = 0;
	pos = 0;
	if (ParseValue(gJson, pos, 0) < 0 || !gTokenCount || gTokens[0].type != TOKEN_OBJECT) return;
	ParseBeatmapV4Events(gJson, 0, handler);
	printf("[WiiSaber] v4 lightshow parsed: %s\n", lightshowPath);
}

void QueueColorEvent(Float beat, UInt lane, UInt value, UInt paletteSlot,
	Float brightness) {
	if (gEventCount >= MAX_CALLBACK_EVENTS) return;
	BeatmapScheduledEvent &event = gEvents[gEventCount++];
	event.beat = beat; event.lane = lane; event.value = value;
	event.color = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
	event.paletteSlot = paletteSlot;
	event.brightness = brightness;
	event.floatFX = event.colorBoost = event.boostEnabled = false;
}

void QueueFloatFXEvent(Float beat, UInt lane, Float value) {
	if (gEventCount >= MAX_CALLBACK_EVENTS) return;
	BeatmapScheduledEvent &event = gEvents[gEventCount++];
	event.beat = beat; event.lane = lane;
	event.value = static_cast<UInt>(value);
	event.paletteSlot = 3;
	event.color = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
	event.brightness = value;
	event.floatFX = true;
	event.colorBoost = event.boostEnabled = false;
}

void QueueColorBoostEvent(Float beat, Bool enabled) {
	if (gEventCount >= MAX_CALLBACK_EVENTS) return;
	BeatmapScheduledEvent &event = gEvents[gEventCount++];
	event.beat = beat;
	event.lane = event.value = event.paletteSlot = 0;
	event.color = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
	event.brightness = 0.0F;
	event.floatFX = false;
	event.colorBoost = true;
	event.boostEnabled = enabled;
}

Bool LoadBeatmapCallbacks(const char *path, const EnvironmentAnimationCallbacks &handler,
	Wire::ColorRGB color0, Wire::ColorRGB color1, Wire::ColorRGB colorW,
	Wire::ColorRGB boostColor0, Wire::ColorRGB boostColor1) {
	gEventCount = gNextEvent = gTokenCount = 0;
	gLoaded = false;
	gPalette0 = color0;
	gPalette1 = color1;
	gPaletteW = colorW;
	gBoostPalette0 = boostColor0;
	gBoostPalette1 = boostColor1;
	gBoostActive = false;
	for (UInt lane = 0; lane < 20; ++lane) {
		gLaneColor[lane] = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
		gLanePaletteSlot[lane] = 3;
		gLaneBrightness[lane] = 0.0F;
		gTweenActive[lane] = false;
		gFlashActive[lane] = false;
		gTweenFadeOut[lane] = false;
	}
	if (!path || !*path) return false;
	ReadBeatmapColors(path);
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
	const Int version = FindKey(gJson, 0, "version");
	const Bool isV4 = FindKey(gJson, 0, "basicEvents") >= 0 ||
		(version >= 0 && gTokens[version].type == TOKEN_STRING &&
		gTokens[version].end - gTokens[version].start > 0 &&
		gJson[gTokens[version].start] == '4');
	printf("[WiiSaber] beatmap parser: %s\n", isV4 ? "V4" : "V3/legacy");
	if (isV4) {
		ParseBeatmapV4Events(gJson, 0, handler);
		ReadV4Lightshow(path, handler);
	} else {
		ParseBeatmapV3Events(gJson, 0, handler);
	}
	printf("[WiiSaber] palette normal=(%.2f,%.2f,%.2f)/(%.2f,%.2f,%.2f) boost=(%.2f,%.2f,%.2f)/(%.2f,%.2f,%.2f)\n",
		gPalette0.R(), gPalette0.G(), gPalette0.B(),
		gPalette1.R(), gPalette1.G(), gPalette1.B(),
		gBoostPalette0.R(), gBoostPalette0.G(), gBoostPalette0.B(),
		gBoostPalette1.R(), gBoostPalette1.G(), gBoostPalette1.B());
	for (UInt i = 1; i < gEventCount; ++i) {
		BeatmapScheduledEvent current = gEvents[i];
		UInt j = i;
		while (j > 0 && (gEvents[j - 1].beat > current.beat ||
			(gEvents[j - 1].beat == current.beat && current.colorBoost &&
			!gEvents[j - 1].colorBoost))) {
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
	gBoostActive = false;
	for (UInt lane = 0; lane < 20; ++lane) {
		gLaneColor[lane] = Wire::ColorRGB(0.0F, 0.0F, 0.0F);
		gLanePaletteSlot[lane] = 3;
		gLaneBrightness[lane] = 0.0F;
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

static Wire::ColorRGB PaletteColor(UInt slot, Bool boost) {
	if (slot == 0) return boost ? gBoostPalette0 : gPalette0;
	if (slot == 1) return boost ? gBoostPalette1 : gPalette1;
	if (slot == 2) return gPaletteW;
	return Wire::ColorRGB(0.0F, 0.0F, 0.0F);
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
		const BeatmapScheduledEvent &event = gEvents[gNextEvent++];
		if (event.colorBoost) {
			gBoostActive = event.boostEnabled;
			for (UInt lane = 0; lane < 20; ++lane) {
				const UInt slot = gLanePaletteSlot[lane];
				if (slot > 1 || gLaneBrightness[lane] <= 0.0F) continue;
				const Wire::ColorRGB color = ScaleColor(PaletteColor(slot, gBoostActive),
					gLaneBrightness[lane]);
				gLaneColor[lane] = color;
				if (gFlashActive[lane]) gFlashColor[lane] = color;
				if (gTweenActive[lane]) gTweenEnd[lane] = color;
				callbacks.ColourEventLaneCallback(lane, color, 1.0F);
			}
			continue;
		}
		if (event.floatFX) {
			callbacks.FloatFXEventLaneCallback(event.lane, event.brightness);
			continue;
		}
		const Float eventTime = event.beat * 60.0F / beatsPerMinute;
		const UInt mode = event.value == 0 ? 0 : (event.value - 1) % 4 + 1;
		gLanePaletteSlot[event.lane] = event.value == 0 ? 3 : event.paletteSlot;
		gLaneBrightness[event.lane] = event.value == 0 ? 0.0F : event.brightness;
		const Wire::ColorRGB color = ScaleColor(
			PaletteColor(event.paletteSlot, gBoostActive), event.brightness);
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
