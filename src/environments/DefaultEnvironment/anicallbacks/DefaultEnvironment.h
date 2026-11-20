#pragma once

#include "AnimationCallbacks.h"
#include "WireColorRGBA.h"
#include "WireColorRGB.h"
#include "WireMatrix3.h"
#include "WireVector3.h"

namespace Wire { class Node; }
namespace Wire { class Light; }

namespace BeatScene {

class DefaultEnvironmentAnimationCallbacks : public EnvironmentAnimationCallbacks {
public:
	DefaultEnvironmentAnimationCallbacks();
	// Cache the imported scene and attach this environment's lights.
	void Bind(Wire::Node *scene);
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
	virtual void AnimationUpdate(Float deltaSeconds);

private:
	// Bound engine objects used by callbacks; these pointers are owned by the
	// scene/renderer lifecycle, not by the beatmap parser.
	Wire::Node *mpScene;
	Wire::Light *mpEffectLight;
	Wire::ColorRGBA mAmbientLight;
	Wire::ColorRGB mKeyLight;
	Wire::Vector3F mKeyDirection;
	Bool mRenderStateDirty;
	Bool mMissingLightTargetLogged[5][12];
	// Current color-lane state feeds the colored fill light and supports resets.
	Wire::ColorRGB mLaneColors[20];
	Float mLaneBrightness[20];
	AnimationLaneRegistration mLanes[20];
	UInt mLaneCount;
	GLSGroupRegistration mGLSGroups[16];
	UInt mGLSGroupCount;
	Wire::Node *mpRingNodes[64];
	UInt mRingNodeCount;
	Wire::Node *mpLeftLaserNodes[16];
	UInt mLeftLaserNodeCount;
	Wire::Node *mpRightLaserNodes[16];
	UInt mRightLaserNodeCount;
	Wire::Node *mpLeftLaserWrapper;
	Wire::Node *mpRightLaserWrapper;
	Float mRingRotation[64];
	Float mRingRotationTarget[64];
	Float mRingRotationStartDelay[64];
	Float mRingZoom[64];
	Float mRingZoomTarget[64];
	Wire::Vector3F mRingInitialPosition[64];
	Wire::Matrix3F mRingInitialRotation[64];
	Bool mRingRotationCounter;
	UInt mRingZoomEventIndex;
	Float mRingRotationElapsed;
	UInt mSmallRingNodeCount;
	UInt mBigRingNodeOffset;
	Float mLeftLaserAngles[16];
	Float mRightLaserAngles[16];
	Float mLeftLaserSpeeds[16];
	Float mRightLaserSpeeds[16];
	Float mLeftWrapperAngle;
	Float mRightWrapperAngle;
	Float mLeftWrapperSpeed;
	Float mRightWrapperSpeed;
	Wire::Matrix3F mLeftLaserInitialRotation[16];
	Wire::Matrix3F mRightLaserInitialRotation[16];
	Wire::Matrix3F mLeftWrapperInitialRotation;
	Wire::Matrix3F mRightWrapperInitialRotation;
};

}
