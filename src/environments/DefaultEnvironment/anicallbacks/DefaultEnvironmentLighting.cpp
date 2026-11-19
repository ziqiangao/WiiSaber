#include "DefaultEnvironment.h"

#include "WireLight.h"
#include "WireNode.h"
#include "WireRenderObject.h"
#include "WireRenderer.h"
#include "WireStateAlpha.h"
#include "WireStateMaterial.h"

using namespace Wire;

namespace BeatScene {

namespace {

// Map basic color lane IDs to the exported light nodes they control. Keep the
// names and counts aligned with the lane IDs registered in DefaultEnvironment.cpp.
static const Char *TargetName(UInt lane, UInt index) {
	static const Char *names[][5] = {
		{"RotatingLasersPair", "RotatingLasersPair (1)", "RotatingLasersPair (2)", "RotatingLasersPair (3)"},
		{"NeonTubeBothSidesDirectional", "NeonTubeBothSidesDirectional (1)", "NeonTubeBothSidesDirectional (2)", "NeonTubeBothSidesDirectional (3)"},
		{"DoubleColorLaserL", "DoubleColorLaserL (1)", "DoubleColorLaserL (2)", "DoubleColorLaserL (3)", "DoubleColorLaserL (4)"},
		{"DoubleColorLaserR", "DoubleColorLaserR (1)", "DoubleColorLaserR (2)", "DoubleColorLaserR (3)", "DoubleColorLaserR (4)"},
		{"FrontLights", "NeonTubeL", "NeonTubeR"}
	};
	static const UInt counts[] = {4, 4, 5, 5, 3};
	return lane < 5 && index < counts[lane] ? names[lane][index] : NULL;
}

static UInt TargetCount(UInt lane) {
	static const UInt counts[] = {4, 4, 5, 5, 3};
	return lane < 5 ? counts[lane] : 0;
}

static ColorRGB DriveLightColor(Float red, Float green, Float blue) {
	// Raise light-object color above normal display range and tint bright colors
	// toward white to approximate overdrive on the Wii's fixed-function renderer.
	if (red < 0.0F) red = 0.0F; else if (red > 1.0F) red = 1.0F;
	if (green < 0.0F) green = 0.0F; else if (green > 1.0F) green = 1.0F;
	if (blue < 0.0F) blue = 0.0F; else if (blue > 1.0F) blue = 1.0F;
	const Float peak = red > green ? (red > blue ? red : blue) : (green > blue ? green : blue);
	Float whiteMix = (peak * 1.5F - 1.0F) / 0.5F;
	if (whiteMix < 0.0F) whiteMix = 0.0F;
	if (whiteMix > 1.0F) whiteMix = 1.0F;
	whiteMix *= 0.4F;
	red = red * 1.5F > 1.0F ? 1.0F : red * 1.5F;
	green = green * 1.5F > 1.0F ? 1.0F : green * 1.5F;
	blue = blue * 1.5F > 1.0F ? 1.0F : blue * 1.5F;
	return ColorRGB(red + (1.0F - red) * whiteMix,
		green + (1.0F - green) * whiteMix,
		blue + (1.0F - blue) * whiteMix);
}

enum { MAX_LIGHT_MATERIAL_STATES = 1024 };
static Node *gLightNodes[MAX_LIGHT_MATERIAL_STATES];
static StateMaterial *gLightStates[MAX_LIGHT_MATERIAL_STATES];
static UInt gLightStateCount = 0;
static Node *gHaloNodes[MAX_LIGHT_MATERIAL_STATES];
static UInt gHaloNodeCount = 0;

static Bool IsHaloNode(Node *node) {
	for (UInt i = 0; i < gHaloNodeCount; ++i)
		if (gHaloNodes[i] == node) return true;
	return false;
}

static StateMaterial *GetLightMaterialState(Node *node) {
	for (UInt i = 0; i < gLightStateCount; ++i)
		if (gLightNodes[i] == node) return gLightStates[i];
	if (gLightStateCount >= MAX_LIGHT_MATERIAL_STATES) return NULL;
	// Give each node its own material state so instances sharing mesh resources
	// can still be lit independently.
	StateMaterial *state = WIRE_NEW StateMaterial;
	StateMaterial *previous = DynamicCast<StateMaterial>(node->GetState(State::MATERIAL));
	if (previous) state->Ambient = previous->Ambient;
	node->AttachState(state);
	gLightNodes[gLightStateCount] = node;
	gLightStates[gLightStateCount++] = state;
	return state;
}

static void AddLightHalo(Node *lightNode) {
	if (IsHaloNode(lightNode) || gHaloNodeCount >= MAX_LIGHT_MATERIAL_STATES ||
		!lightNode->GetRenderObject()) return;
	// Draw a larger, additive copy of the light mesh as a simple halo effect.
	Node *halo = WIRE_NEW Node;
	halo->SetRenderObject(WIRE_NEW RenderObject(lightNode->GetRenderObject()));
	halo->Local.SetScale(Vector3F(3.0F, 1.12F, 3.0F));
	StateAlpha *alpha = WIRE_NEW StateAlpha;
	alpha->BlendEnabled = true;
	alpha->SrcBlend = StateAlpha::SBM_SRC_ALPHA;
	alpha->DstBlend = StateAlpha::DBM_ONE;
	halo->AttachState(alpha);
	StateMaterial *material = GetLightMaterialState(halo);
	if (material) material->Ambient = ColorRGBA(1.0F, 1.0F, 1.0F, 0.28F);
	gHaloNodes[gHaloNodeCount++] = halo;
	lightNode->AttachChild(halo);
}

static UInt SetSubtreeColor(Spatial *spatial, Float red, Float green,
	Float blue, Renderer *renderer) {
	UInt changed = 0;
	Node *node = DynamicCast<Node>(spatial);
	if (!node) return 0;
	if (node->GetRenderObject()) {
		RenderObject *renderObject = node->GetRenderObject();
		// Imported copies may share one RenderObject. Wire stores effective
		// material state on that object, so give each colored instance its own.
		if (renderObject->GetReferences() > 1) {
			renderObject = WIRE_NEW RenderObject(renderObject);
			node->SetRenderObject(renderObject);
		}
		StateMaterial *state = GetLightMaterialState(node);
		if (state) {
			state->Ambient = ColorRGBA(red, green, blue, state->Ambient.A());
			++changed;
		}
		if (!IsHaloNode(node)) AddLightHalo(node);
	}
	if (IsHaloNode(node)) return changed;
	for (UInt i = 0; i < node->GetQuantity(); ++i) {
		Spatial *child = node->GetChild(i);
		if (child) changed += SetSubtreeColor(child, red, green, blue, renderer);
	}
	return changed;
}

static UInt SetNamedColor(Node *scene, const Char *name, Float red,
	Float green, Float blue, Renderer *renderer) {
	// Find every matching exported name, then color renderable descendants.
	if (!scene || !name || !*name) return 0;
	if (red < 0.0F) red = 0.0F; else if (red > 1.0F) red = 1.0F;
	if (green < 0.0F) green = 0.0F; else if (green > 1.0F) green = 1.0F;
	if (blue < 0.0F) blue = 0.0F; else if (blue > 1.0F) blue = 1.0F;
	TArray<Spatial*> matches;
	scene->FindChildren(name, matches);
	UInt changed = 0;
	for (UInt i = 0; i < matches.GetQuantity(); ++i)
		changed += SetSubtreeColor(matches[i], red, green, blue, renderer);
	return changed;
}

} // namespace

void DefaultEnvironmentAnimationCallbacks::ColourEventLaneCallback(
	UInt lane, ColorRGB colour, Float brightness) {
	// This is called when a lane changes, including per-frame fade/flash updates.
	// Store its state for the scene fill light, then apply it to the mapped nodes.
	if (lane >= 20) return;
	if (brightness < 0.0F) brightness = 0.0F;
	if (brightness > 1.0F) brightness = 1.0F;
	const Float red = colour.R() * brightness;
	const Float green = colour.G() * brightness;
	const Float blue = colour.B() * brightness;
	mLaneColors[lane] = colour;
	mLaneBrightness[lane] = red + green + blue > 0.01F ? brightness : 0.0F;
	if (mpScene) {
		const ColorRGB driven = DriveLightColor(red, green, blue);
		for (UInt i = 0; i < TargetCount(lane); ++i) {
			const Char *targetName = TargetName(lane, i);
			if (targetName && SetNamedColor(mpScene, targetName,
				driven.R(), driven.G(), driven.B(),
				mpRenderer)) mRenderStateDirty = true;
		}
	}
	if (mpEffectLight) {
		Float r = 0.0F, g = 0.0F, b = 0.0F, weight = 0.0F;
		for (UInt i = 0; i < 20; ++i) {
			const Float w = mLaneBrightness[i];
			if (w <= 0.0F) continue;
			r += mLaneColors[i].R() * w;
			g += mLaneColors[i].G() * w;
			b += mLaneColors[i].B() * w;
			weight += w;
		}
		if (weight > 0.0F) {
			// Average concurrent lanes, then lift the colored fill light so the
			// environment receives a visible wash from active stage lights.
			const Float gain = 1.6F;
			mpEffectLight->Color = ColorRGB(
				(r / weight) * gain > 1.0F ? 1.0F : (r / weight) * gain,
				(g / weight) * gain > 1.0F ? 1.0F : (g / weight) * gain,
				(b / weight) * gain > 1.0F ? 1.0F : (b / weight) * gain);
			mpEffectLight->Enabled = true;
		} else {
			mpEffectLight->Enabled = false;
		}
	}
}

} // namespace BeatScene
