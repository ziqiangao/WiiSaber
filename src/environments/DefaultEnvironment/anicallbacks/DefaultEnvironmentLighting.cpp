#include "DefaultEnvironment.h"

#include "WireColor32.h"
#include "WireMesh.h"
#include "WireNode.h"
#include "WireRenderObject.h"
#include "WireRenderer.h"
#include "WireStateMaterial.h"
#include "WireVertexBuffer.h"

using namespace Wire;

namespace BeatScene {

namespace {

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

enum { MAX_LIGHT_MATERIAL_STATES = 1024 };
static Node *gLightNodes[MAX_LIGHT_MATERIAL_STATES];
static StateMaterial *gLightStates[MAX_LIGHT_MATERIAL_STATES];
static UInt gLightStateCount = 0;

static StateMaterial *GetLightMaterialState(Node *node) {
	for (UInt i = 0; i < gLightStateCount; ++i)
		if (gLightNodes[i] == node) return gLightStates[i];
	if (gLightStateCount >= MAX_LIGHT_MATERIAL_STATES) return NULL;
	StateMaterial *state = WIRE_NEW StateMaterial;
	StateMaterial *previous = DynamicCast<StateMaterial>(node->GetState(State::MATERIAL));
	if (previous) state->Ambient = previous->Ambient;
	node->AttachState(state);
	gLightNodes[gLightStateCount] = node;
	gLightStates[gLightStateCount++] = state;
	return state;
}

static UInt SetSubtreeColor(Spatial *spatial, Float red, Float green,
	Float blue, Renderer *renderer) {
	UInt changed = 0;
	Node *node = DynamicCast<Node>(spatial);
	if (!node) return 0;
	if (node->GetRenderObject()) {
		Mesh *mesh = node->GetRenderObject()->GetMesh();
		if (mesh) {
			VertexBuffer *vertices = mesh->GetVertexBuffer();
			if (vertices && mesh->HasColor()) {
				const Int r = static_cast<Int>(red * 255.0F);
				const Int g = static_cast<Int>(green * 255.0F);
				const Int b = static_cast<Int>(blue * 255.0F);
				for (UInt i = 0; i < vertices->GetQuantity(); ++i)
					vertices->Color4(i) = Color32(static_cast<UChar>(r),
						static_cast<UChar>(g), static_cast<UChar>(b), 255);
				if (renderer) renderer->Update(vertices);
			} else {
				StateMaterial *state = GetLightMaterialState(node);
				if (state) {
					state->Ambient = ColorRGBA(red, green, blue, state->Ambient.A());
					++changed;
				}
			}
		}
	}
	for (UInt i = 0; i < node->GetQuantity(); ++i) {
		Spatial *child = node->GetChild(i);
		if (child) changed += SetSubtreeColor(child, red, green, blue, renderer);
	}
	return changed;
}

static UInt SetNamedColor(Node *scene, const Char *name, Float red,
	Float green, Float blue, Renderer *renderer) {
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
	if (!mpScene) return;
	if (brightness < 0.0F) brightness = 0.0F;
	if (brightness > 1.0F) brightness = 1.0F;
	const Float red = colour.R() * brightness;
	const Float green = colour.G() * brightness;
	const Float blue = colour.B() * brightness;
	for (UInt i = 0; i < TargetCount(lane); ++i) {
		const Char *targetName = TargetName(lane, i);
		if (targetName && SetNamedColor(mpScene, targetName,
			red, green, blue, mpRenderer)) mRenderStateDirty = true;
	}
}

} // namespace BeatScene
