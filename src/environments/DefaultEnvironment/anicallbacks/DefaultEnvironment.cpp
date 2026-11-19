#include "DefaultEnvironment.h"

#include "WireLight.h"
#include "WireNode.h"
#include "WireRenderer.h"

using namespace Wire;

namespace BeatScene {

DefaultEnvironmentAnimationCallbacks::DefaultEnvironmentAnimationCallbacks()
	: mpScene(NULL), mpRenderer(NULL), mAmbientLight(0.12F, 0.12F, 0.12F, 1.0F),
	  mKeyLight(0.75F, 0.78F, 0.9F), mKeyDirection(0.2F, -1.0F, -0.3F),
	  mRenderStateDirty(false), mLaneCount(0), mGLSGroupCount(0) {
	for (UInt id = 0; id <= 4; ++id) RegisterLane(id, ANIMATION_LANE_COLOUR);
	RegisterLane(8, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(9, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(12, ANIMATION_LANE_FLOAT_FX);
	RegisterLane(13, ANIMATION_LANE_FLOAT_FX);
}

void DefaultEnvironmentAnimationCallbacks::RegisterLane(UInt id, AnimationLaneKind kind) {
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
	mpScene = scene;
	mpRenderer = renderer;
	Light *light = WIRE_NEW Light(Light::LT_DIRECTIONAL);
	light->Ambient = ColorRGB(mAmbientLight.R(), mAmbientLight.G(), mAmbientLight.B());
	light->Color = mKeyLight;
	light->Direction = mKeyDirection;
	light->Direction.Normalize();
	if (mpScene) mpScene->AttachLight(light);
}

ColorRGB DefaultEnvironmentAnimationCallbacks::PrimaryColor() { return ColorRGB(0.20F, 0.48F, 1.0F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::SecondaryColor() { return ColorRGB(1.0F, 0.12F, 0.32F); }
ColorRGB DefaultEnvironmentAnimationCallbacks::WhiteColor() { return ColorRGB(1.0F, 1.0F, 1.0F); }

void DefaultEnvironmentAnimationCallbacks::Reset() { mRenderStateDirty = false; }

void DefaultEnvironmentAnimationCallbacks::GLSColourCallback(UInt, UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSTransformCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSRotationCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSFloatFXCallback(UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::FloatFXEventLaneCallback(UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::AnimationUpdate() {
	if (mRenderStateDirty && mpScene) {
		mpScene->UpdateRS();
		mRenderStateDirty = false;
	}
}

}
