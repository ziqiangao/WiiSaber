#pragma once

#include "AnimationCallbacks.h"
#include "WireColorRGBA.h"
#include "WireColorRGB.h"
#include "WireVector3.h"

namespace Wire { class Node; class Renderer; }
namespace Wire { class Light; }

namespace BeatScene {

class DefaultEnvironmentAnimationCallbacks : public EnvironmentAnimationCallbacks {
public:
	DefaultEnvironmentAnimationCallbacks();
	// Cache the imported scene and renderer, and attach this environment's lights.
	void Bind(Wire::Node *scene, Wire::Renderer *renderer);
	// Declare which Beat Saber event IDs this environment supports.
	void RegisterLane(UInt id, AnimationLaneKind kind);
	// Declare GLS group/type IDs and map GLS object IDs to exported node names.
	void RegisterGLSGroup(UInt id, const UInt *types, UInt typeCount,
		const GLSObjectRegistration *objects, UInt objectCount);
	virtual const AnimationLaneRegistration *GetLanes(UInt &count) const;
	virtual const GLSGroupRegistration *GetGLSGroups(UInt &count) const;
	static Wire::ColorRGB PrimaryColor();
	static Wire::ColorRGB SecondaryColor();
	static Wire::ColorRGB WhiteColor();
	static Wire::ColorRGB BoostPrimaryColor();
	static Wire::ColorRGB BoostSecondaryColor();
	virtual void Reset();
	virtual void GLSFloatFXCallback(UInt group, UInt id, Float value);
	virtual void GLSTransformCallback(UInt group, UInt id, Float x, Float y, Float z);
	virtual void GLSRotationCallback(UInt group, UInt id, Float x, Float y, Float z);
	virtual void GLSColourCallback(UInt group, UInt id, UInt col, Float brightness);
	virtual void FloatFXEventLaneCallback(UInt lane, Float value);
	virtual void ColourEventLaneCallback(UInt lane, Wire::ColorRGB colour, Float brightness);
	virtual void AnimationUpdate();

private:
	// Bound engine objects used by callbacks; these pointers are owned by the
	// scene/renderer lifecycle, not by the beatmap parser.
	Wire::Node *mpScene;
	Wire::Renderer *mpRenderer;
	Wire::Light *mpEffectLight;
	Wire::ColorRGBA mAmbientLight;
	Wire::ColorRGB mKeyLight;
	Wire::Vector3F mKeyDirection;
	Bool mRenderStateDirty;
	// Current color-lane state feeds the colored fill light and supports resets.
	Wire::ColorRGB mLaneColors[20];
	Float mLaneBrightness[20];
	AnimationLaneRegistration mLanes[20];
	UInt mLaneCount;
	GLSGroupRegistration mGLSGroups[16];
	UInt mGLSGroupCount;
};

}
