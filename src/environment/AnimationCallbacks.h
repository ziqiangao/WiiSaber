#pragma once

#include "WireTypes.h"
#include "WireColorRGB.h"

namespace BeatScene {

enum AnimationLaneKind {
	ANIMATION_LANE_COLOUR,
	ANIMATION_LANE_FLOAT_FX
};

// Basic event registration: id is the Beat Saber lane/event type, kind says
// whether it dispatches a color value or a Float FX value.
struct AnimationLaneRegistration {
	UInt id;
	AnimationLaneKind kind;
};

struct GLSObjectRegistration {
	// Beat Saber GLS object ID paired with the exported scene node name.
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
	// Environments override these registries; the runner queries them before
	// dispatching events so it can support environment-specific IDs.
	virtual const AnimationLaneRegistration *GetLanes(UInt &count) const {
		count = 0; return NULL;
	}
	virtual const GLSGroupRegistration *GetGLSGroups(UInt &count) const {
		count = 0; return NULL;
	}
	// Event hooks receive parsed map events; AnimationUpdate runs once per frame.
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
	virtual void AnimationUpdate(Float deltaSeconds) {}
};

}
