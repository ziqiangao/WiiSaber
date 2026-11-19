#include "DefaultEnvironment.h"

#include "WireLight.h"
#include "WireNode.h"
#include "WireRenderer.h"

using namespace Wire;

namespace BeatScene {

DefaultEnvironmentAnimationCallbacks::DefaultEnvironmentAnimationCallbacks()
	: mpScene(NULL), mpRenderer(NULL), mpEffectLight(NULL),
	  mAmbientLight(0.05F, 0.05F, 0.05F, 1.0F),
	  mKeyLight(0.75F, 0.78F, 0.9F), mKeyDirection(0.2F, -1.0F, -0.3F),
	  mRenderStateDirty(false), mLaneCount(0), mGLSGroupCount(0) {
	// Track the last active color and brightness for each lane. The beatmap
	// runner updates these values as events start, fade, and turn off.
	for (UInt i = 0; i < 20; ++i) {
		mLaneColors[i] = ColorRGB::BLACK;
		mLaneBrightness[i] = 0.0F;
	}
	for (UInt id = 0; id <= 4; ++id) RegisterLane(id, ANIMATION_LANE_COLOUR);
	// Float FX events are registered separately from color lanes. Their
	// callbacks currently need environment-specific animation implementations.
	RegisterLane(8, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(9, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(12, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(13, ANIMATION_LANE_FLOAT_FX);
}

void DefaultEnvironmentAnimationCallbacks::RegisterLane(UInt id, AnimationLaneKind kind) {
	// Add a lane once; if it is registered again, update its event kind.
	if (mLaneCount >= 20 || id >= 20) return;
	for (UInt i = 0; i < mLaneCount; ++i) {
		if (mLanes[i].id != id) continue;
		mLanes[i].kind = kind;
		return;
	}
	mLanes[mLaneCount].id = id;
	mLanes[mLaneCount++].kind = kind;
}

void DefaultEnvironmentAnimationCallbacks::RegisterGLSGroup(UInt id,
	const UInt *types, UInt typeCount, const GLSObjectRegistration *objects,
	UInt objectCount) {
	// Store a GLS group ID, supported event types, and its object-ID/name map.
	// These registrations describe environment IDs; they are not scene indices.
	if (mGLSGroupCount >= 16 || (typeCount && !types) ||
		(objectCount && !objects)) return;
	for (UInt i = 0; i < mGLSGroupCount; ++i) {
		if (mGLSGroups[i].id != id) continue;
		GLSGroupRegistration &existing = mGLSGroups[i];
		existing.typeCount = typeCount > 4 ? 4 : typeCount;
		existing.objectCount = objectCount > 16 ? 16 : objectCount;
		for (UInt j = 0; j < existing.typeCount; ++j) existing.types[j] = types[j];
		for (UInt j = 0; j < existing.objectCount; ++j) existing.objects[j] = objects[j];
		return;
	}
	GLSGroupRegistration &group = mGLSGroups[mGLSGroupCount++];
	group.id = id;
	group.typeCount = typeCount > 4 ? 4 : typeCount;
	group.objectCount = objectCount > 16 ? 16 : objectCount;
	for (UInt i = 0; i < group.typeCount; ++i) group.types[i] = types[i];
	for (UInt i = 0; i < group.objectCount; ++i) group.objects[i] = objects[i];
}

const AnimationLaneRegistration *DefaultEnvironmentAnimationCallbacks::GetLanes(UInt &count) const {
	count = mLaneCount;
	return mLanes;
}

const GLSGroupRegistration *DefaultEnvironmentAnimationCallbacks::GetGLSGroups(UInt &count) const {
	count = mGLSGroupCount;
	return mGLSGroups;
}

void DefaultEnvironmentAnimationCallbacks::Bind(Node *scene, Renderer *renderer) {
	// Bind once after import so callbacks can affect this scene and renderer.
	mpScene = scene;
	mpRenderer = renderer;
	// Low ambient plus a directional key light keeps unlit areas dark while
	// still revealing scene geometry. Tune these for the environment export.
	Light *light = WIRE_NEW Light(Light::LT_DIRECTIONAL);
	light->Ambient = ColorRGB(mAmbientLight.R(), mAmbientLight.G(), mAmbientLight.B());
	light->Color = mKeyLight;
	light->Direction = mKeyDirection;
	light->Direction.Normalize();
	if (mpScene) mpScene->AttachLight(light);
	// A single colored fill light washes nearby geometry when a color lane is on.
	// Keep the total lights affecting any render object within the GX limit.
	mpEffectLight = WIRE_NEW Light(Light::LT_POINT);
	mpEffectLight->Position = Vector3F(0.0F, 2.6F, 5.0F);
	mpEffectLight->Color = ColorRGB::BLACK;
	mpEffectLight->Constant = 0.28F;
	mpEffectLight->Linear = 0.035F;
	mpEffectLight->Quadric = 0.004F;
	mpEffectLight->Enabled = false;
	if (mpScene) mpScene->AttachLight(mpEffectLight);
}

// Fallback palette used when the beatmap does not declare environment colors.
ColorRGB DefaultEnvironmentAnimationCallbacks::PrimaryColor() { return ColorRGB(0.20F, 0.48F, 1.0F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::SecondaryColor() { return ColorRGB(1.0F, 0.12F, 0.32F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::WhiteColor() { return ColorRGB(1.0F, 1.0F, 1.0F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::BoostPrimaryColor() { return ColorRGB(0.85F, 0.12F, 0.9F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::BoostSecondaryColor() { return ColorRGB(0.05F, 0.95F, 0.72F); }

void DefaultEnvironmentAnimationCallbacks::Reset() {
	// Clear playback state so a restarted map begins with every lane off.
	mRenderStateDirty = false;
	for (UInt i = 0; i < 20; ++i) {
		mLaneColors[i] = ColorRGB::BLACK;
		mLaneBrightness[i] = 0.0F;
	}
	if (mpEffectLight) mpEffectLight->Enabled = false;
}

// GLS callbacks are placeholders until the beatmap runner parses and dispatches
// GLS event boxes. Implement these when that parser support is added.
void DefaultEnvironmentAnimationCallbacks::GLSColourCallback(UInt, UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSTransformCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSRotationCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSFloatFXCallback(UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::FloatFXEventLaneCallback(UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::AnimationUpdate() {
	// Apply queued render-state changes once per frame after callbacks modify
	// materials; transforms can also be advanced here for animated effects.
	if (mRenderStateDirty && mpScene) {
		mpScene->UpdateRS();
		mRenderStateDirty = false;
	}
}

}
