#include "LightingUtils.h"

#include "WireNode.h"
#include "WireLight.h"
#include "WireNodeLight.h"
#include "WireRenderObject.h"
#include "WireStateAlpha.h"
#include "WireStateMaterial.h"

using namespace Wire;

namespace BeatScene {
namespace EnvironmentLighting {

namespace {

enum { MAX_LIGHT_MATERIAL_STATES = 1024 };
static Node *gLightNodes[MAX_LIGHT_MATERIAL_STATES];
static StateMaterial *gLightStates[MAX_LIGHT_MATERIAL_STATES];
static UInt gLightStateCount = 0;
static Node *gHaloNodes[MAX_LIGHT_MATERIAL_STATES];
static UInt gHaloNodeCount = 0;

static ColorRGB DriveLightColor(ColorRGB color) {
	Float red = color.R();
	Float green = color.G();
	Float blue = color.B();
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
	// can still be colored independently.
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
	// A larger additive copy of the light mesh gives it a simple halo glow.
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

static UInt SetSubtreeColor(Spatial *spatial, ColorRGB color) {
	UInt changed = 0;
	Node *node = DynamicCast<Node>(spatial);
	if (!node) return 0;
	NodeLight *nodeLight = DynamicCast<NodeLight>(node);
	if (nodeLight && nodeLight->Get()) {
		Light *light = nodeLight->Get();
		light->Color = color;
		light->Enabled = color.R() > 0.001F || color.G() > 0.001F ||
			color.B() > 0.001F;
		++changed;
	}
	for (UInt i = 0; i < node->GetLightQuantity(); ++i) {
		Light *light = node->GetLight(i);
		if (!light) continue;
		light->Color = color;
		light->Enabled = color.R() > 0.001F || color.G() > 0.001F ||
			color.B() > 0.001F;
		++changed;
	}
	if (node->GetRenderObject()) {
		RenderObject *renderObject = node->GetRenderObject();
		// Imported copies can share one RenderObject; clone it before changing
		// instance-specific render state.
		if (renderObject->GetReferences() > 1) {
			renderObject = WIRE_NEW RenderObject(renderObject);
			node->SetRenderObject(renderObject);
		}
		StateMaterial *state = GetLightMaterialState(node);
		if (state) {
			const Float alpha = IsHaloNode(node) ? state->Ambient.A() : 1.0F;
			state->Ambient = ColorRGBA(color.R(), color.G(), color.B(), alpha);
			++changed;
		}
		if (!IsHaloNode(node)) AddLightHalo(node);
	}
	if (IsHaloNode(node)) return changed;
	for (UInt i = 0; i < node->GetQuantity(); ++i) {
		Spatial *child = node->GetChild(i);
		if (child) changed += SetSubtreeColor(child, color);
	}
	return changed;
}

} // namespace

UInt SetNamedLightColor(Node *scene, const Char *nodeName, ColorRGB color) {
	if (!scene || !nodeName || !*nodeName) return 0;
	TArray<Spatial*> matches;
	scene->FindChildren(nodeName, matches);
	const ColorRGB driven = DriveLightColor(color);
	UInt changed = 0;
	for (UInt i = 0; i < matches.GetQuantity(); ++i)
		changed += SetSubtreeColor(matches[i], driven);
	return changed;
}

} // namespace EnvironmentLighting
} // namespace BeatScene
