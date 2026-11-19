#pragma once

#include "WireTypes.h"
#include "WireColorRGB.h"

namespace BeatScene {

enum AnimationLaneKind {
	ANIMATION_LANE_COLOUR,
	ANIMATION_LANE_FLOAT_FX
};

struct AnimationLaneRegistration {
	UInt id;
	AnimationLaneKind kind;
};

struct GLSObjectRegistration {
	UInt id;
	const Char *nodeName;
};

struct GLSGroupRegistration {
	UInt id;
	UInt typeCount;
	UInt types[4];
	UInt objectCount;
	GLSObjectRegistration objects[16];
};

class EnvironmentAnimationCallbacks {
public:
	virtual ~EnvironmentAnimationCallbacks() {}
	virtual const AnimationLaneRegistration *GetLanes(UInt &count) const {
		count = 0; return NULL;
	}
	virtual const GLSGroupRegistration *GetGLSGroups(UInt &count) const {
		count = 0; return NULL;
	}
	virtual void Reset() {}
	virtual void GLSFloatFXCallback(UInt group, UInt id, Float value) {}
	virtual void GLSTransformCallback(UInt group, UInt id,
		Float x, Float y, Float z) {}
	virtual void GLSRotationCallback(UInt group, UInt id,
		Float x, Float y, Float z) {}
	virtual void GLSColourCallback(UInt group, UInt id,
		UInt col, Float brightness) {}
	virtual void FloatFXEventLaneCallback(UInt lane, Float value) {}
	virtual void ColourEventLaneCallback(UInt lane,
		Wire::ColorRGB colour, Float brightness) {}
	virtual void AnimationUpdate() {}
};

}
