#pragma once

#include "AnimationCallbacks.h"
#include "WireColorRGBA.h"
#include "WireColorRGB.h"
#include "WireVector3.h"

namespace Wire { class Node; class Renderer; }

namespace BeatScene {

class DefaultEnvironmentAnimationCallbacks : public EnvironmentAnimationCallbacks {
public:
	DefaultEnvironmentAnimationCallbacks();
	void Bind(Wire::Node *scene, Wire::Renderer *renderer);
	void RegisterLane(UInt id, AnimationLaneKind kind);
	void RegisterGLSGroup(UInt id, const UInt *types, UInt typeCount,
		const GLSObjectRegistration *objects, UInt objectCount);
	virtual const AnimationLaneRegistration *GetLanes(UInt &count) const;
	virtual const GLSGroupRegistration *GetGLSGroups(UInt &count) const;
	static Wire::ColorRGB PrimaryColor();
	static Wire::ColorRGB SecondaryColor();
	static Wire::ColorRGB WhiteColor();
	virtual void Reset();
	virtual void GLSFloatFXCallback(UInt group, UInt id, Float value);
	virtual void GLSTransformCallback(UInt group, UInt id, Float x, Float y, Float z);
	virtual void GLSRotationCallback(UInt group, UInt id, Float x, Float y, Float z);
	virtual void GLSColourCallback(UInt group, UInt id, UInt col, Float brightness);
	virtual void FloatFXEventLaneCallback(UInt lane, Float value);
	virtual void ColourEventLaneCallback(UInt lane, Wire::ColorRGB colour, Float brightness);
	virtual void AnimationUpdate();

private:
	Wire::Node *mpScene;
	Wire::Renderer *mpRenderer;
	Wire::ColorRGBA mAmbientLight;
	Wire::ColorRGB mKeyLight;
	Wire::Vector3F mKeyDirection;
	Bool mRenderStateDirty;
	AnimationLaneRegistration mLanes[20];
	UInt mLaneCount;
	GLSGroupRegistration mGLSGroups[16];
	UInt mGLSGroupCount;
};

}
