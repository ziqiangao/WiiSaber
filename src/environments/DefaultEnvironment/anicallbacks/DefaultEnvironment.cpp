#include "DefaultEnvironment.h"

#include <math.h>
#include <stdio.h>

#include "LightingUtils.h"
#include "WireLight.h"
#include "WireNode.h"
#include "WireNodeLight.h"
#include "WireSpatial.h"
#include "WireStateMaterial.h"

using namespace Wire;

namespace BeatScene {

namespace {

// Map this environment's basic color lanes to its exported light object names.
static const Char *TargetName(UInt lane, UInt index) {
	static const Char *names[][12] = {
		{"DoubleColorLaserL", "DoubleColorLaserL (1)", "DoubleColorLaserL (2)", "DoubleColorLaserL (3)", "DoubleColorLaserL (4)",
			"DoubleColorLaserR", "DoubleColorLaserR (1)", "DoubleColorLaserR (2)", "DoubleColorLaserR (3)", "DoubleColorLaserR (4)"},
		{"NeonTubeBothSidesDirectional", "NeonTubeBothSidesDirectional (1)", "NeonTubeBothSidesDirectional (2)", "NeonTubeBothSidesDirectional (3)"},
		{"BaseL", "NearBuildingLeft (1)", "NearBuildingLeft (2)", "NeonTubeL"},
		{"BaseR", "NearBuildingRight (1)", "NearBuildingRight (2)", "NeonTubeR"},
		{"FrontLights", "BackColumnNeon", "DoubleColorLaserL", "DoubleColorLaserL (1)", "DoubleColorLaserL (2)", "DoubleColorLaserL (3)", "DoubleColorLaserL (4)",
			"DoubleColorLaserR", "DoubleColorLaserR (1)", "DoubleColorLaserR (2)", "DoubleColorLaserR (3)", "DoubleColorLaserR (4)"}
	};
	static const UInt counts[] = {10, 4, 4, 4, 12};
	return lane < 5 && index < counts[lane] ? names[lane][index] : NULL;
}

static UInt TargetCount(UInt lane) {
	static const UInt counts[] = {10, 4, 4, 4, 12};
	return lane < 5 ? counts[lane] : 0;
}

static void CacheNamedNodes(Node *scene, const Char *name, Node **nodes,
	UInt &nodeCount, UInt maximum) {
	if (!scene || !name || !*name || nodeCount >= maximum) return;
	TArray<Spatial*> matches;
	scene->FindChildren(name, matches);
	for (UInt i = 0; i < matches.GetQuantity() && nodeCount < maximum; ++i) {
		Node *node = DynamicCast<Node>(matches[i]);
		if (!node) continue;
		Bool duplicate = false;
		for (UInt j = 0; j < nodeCount; ++j) {
			if (nodes[j] == node) { duplicate = true; break; }
		}
		if (!duplicate) nodes[nodeCount++] = node;
	}
}

static void CacheRingNodes(Node *scene, Node **nodes, UInt &nodeCount) {
	CacheNamedNodes(scene, "SmallTrackLaneRing(Clone)", nodes, nodeCount, 64);
}

static void SortNodesByZ(Node **nodes, UInt start, UInt count) {
	for (UInt i = start + 1; i < count; ++i) {
		Node *current = nodes[i];
		const Float z = current->Local.GetTranslate().Z();
		UInt j = i;
		while (j > start && nodes[j - 1]->Local.GetTranslate().Z() > z) {
			nodes[j] = nodes[j - 1];
			--j;
		}
		nodes[j] = current;
	}
}

static Matrix3F EulerRotation(const Vector3F &axis, Float angleDegrees) {
	const Float radians = 3.14159265358979323846F / 180.0F;
	const Matrix3F rotateX(Vector3F::UNIT_X, axis.X() * angleDegrees * radians);
	const Matrix3F rotateY(Vector3F::UNIT_Y, axis.Y() * angleDegrees * radians);
	const Matrix3F rotateZ(Vector3F::UNIT_Z, axis.Z() * angleDegrees * radians);
	return rotateY * rotateX * rotateZ;
}

static Float LaserPairStartAngle(UInt index) {
	return fmodf(37.0F + index * 97.0F, 360.0F);
}

static void LogColorTargetTree(Node *node, UInt depth, UInt &logged) {
	if (!node || depth > 8 || logged >= 24) return;
	const Char *name = static_cast<const Char*>(node->GetName());
	NodeLight *nodeLight = DynamicCast<NodeLight>(node);
	printf("[WiiSaber] color target node: %s mesh=%u lights=%u nodeLight=%u children=%u\n",
		name ? name : "<unnamed>", node->GetRenderObject() ? 1 : 0,
		node->GetLightQuantity(), nodeLight && nodeLight->Get() ? 1 : 0,
		node->GetQuantity());
	++logged;
	for (UInt i = 0; i < node->GetQuantity() && logged < 24; ++i)
		LogColorTargetTree(DynamicCast<Node>(node->GetChild(i)), depth + 1, logged);
}

static void LogColorTargetTree(Node *scene, const Char *targetName) {
	TArray<Spatial*> matches;
	scene->FindChildren(targetName, matches);
	printf("[WiiSaber] color target probe: %s matches=%u\n",
		targetName, matches.GetQuantity());
	UInt logged = 0;
	for (UInt i = 0; i < matches.GetQuantity() && logged < 24; ++i)
		LogColorTargetTree(DynamicCast<Node>(matches[i]), 0, logged);
}

} // namespace

DefaultEnvironmentAnimationCallbacks::DefaultEnvironmentAnimationCallbacks()
	: mpScene(NULL), mpEffectLight(NULL),
	  mAmbientLight(0.05F, 0.05F, 0.05F, 1.0F),
	  mKeyLight(0.75F, 0.78F, 0.9F), mKeyDirection(0.2F, -1.0F, -0.3F),
	  mRenderStateDirty(false), mLaneCount(0), mGLSGroupCount(0),
	  mRingNodeCount(0), mLeftLaserNodeCount(0), mRightLaserNodeCount(0),
	  mpLeftLaserWrapper(NULL), mpRightLaserWrapper(NULL),
	  mRingRotationCounter(false), mRingZoomEventIndex(0),
	  mRingRotationElapsed(0.0F),
	  mSmallRingNodeCount(0), mBigRingNodeOffset(0),
	  mLeftWrapperAngle(0.0F), mRightWrapperAngle(0.0F),
	  mLeftWrapperSpeed(0.0F), mRightWrapperSpeed(0.0F),
	  mLeftWrapperInitialRotation(Matrix3F(false)),
	  mRightWrapperInitialRotation(Matrix3F(false)) {
	// Track the last active color and brightness for each lane. The beatmap
	// runner updates these values as events start, fade, and turn off.
	for (UInt i = 0; i < 20; ++i) {
		mLaneColors[i] = ColorRGB::BLACK;
		mLaneBrightness[i] = 0.0F;
	}
	for (UInt i = 0; i < 64; ++i) {
		mRingRotation[i] = 0.0F;
		mRingRotationTarget[i] = 0.0F;
		mRingRotationStartDelay[i] = 0.0F;
		mRingZoom[i] = 0.0F;
		mRingZoomTarget[i] = 0.0F;
		mRingInitialPosition[i] = Vector3F::ZERO;
	}
	for (UInt i = 0; i < 16; ++i) {
		mLeftLaserAngles[i] = mRightLaserAngles[i] = 0.0F;
		mLeftLaserSpeeds[i] = mRightLaserSpeeds[i] = 0.0F;
	}
	for (UInt lane = 0; lane < 5; ++lane)
		for (UInt i = 0; i < 12; ++i)
			mMissingLightTargetLogged[lane][i] = false;
	for (UInt id = 0; id <= 4; ++id) RegisterLane(id, ANIMATION_LANE_COLOUR);
	// Ring and laser movement is handled by the Float FX callback.
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

void DefaultEnvironmentAnimationCallbacks::Bind(Node *scene) {
	// Bind once after import so callbacks can affect this environment's scene.
	mpScene = scene;
	if (mpScene) {
		CacheRingNodes(mpScene, mpRingNodes, mRingNodeCount);
		SortNodesByZ(mpRingNodes, 0, mRingNodeCount);
		mSmallRingNodeCount = mRingNodeCount;
		CacheNamedNodes(mpScene, "BigTrackLaneRing(Clone)", mpRingNodes,
			mRingNodeCount, 64);
		mBigRingNodeOffset = mSmallRingNodeCount;
		SortNodesByZ(mpRingNodes, mBigRingNodeOffset, mRingNodeCount);
		CacheNamedNodes(mpScene, "BaseL", mpLeftLaserNodes,
			mLeftLaserNodeCount, 16);
		CacheNamedNodes(mpScene, "BaseR", mpRightLaserNodes,
			mRightLaserNodeCount, 16);
		mpLeftLaserWrapper = DynamicCast<Node>(
			mpScene->FindChild("LightWrapper0"));
		mpRightLaserWrapper = DynamicCast<Node>(
			mpScene->FindChild("LightWrapper1"));
		if (mpLeftLaserWrapper)
			mLeftWrapperInitialRotation = mpLeftLaserWrapper->Local.GetRotate();
		if (mpRightLaserWrapper)
			mRightWrapperInitialRotation = mpRightLaserWrapper->Local.GetRotate();
		for (UInt i = 0; i < mRingNodeCount; ++i) {
			mRingInitialPosition[i] = mpRingNodes[i]->Local.GetTranslate();
			mRingInitialRotation[i] = mpRingNodes[i]->Local.GetRotate();
			mRingZoom[i] = mRingInitialPosition[i].Z();
			mRingZoomTarget[i] = mRingZoom[i];
		}
		for (UInt i = 0; i < mLeftLaserNodeCount; ++i) {
			mLeftLaserInitialRotation[i] = mpLeftLaserNodes[i]->Local.GetRotate();
		}
		for (UInt i = 0; i < mRightLaserNodeCount; ++i) {
			mRightLaserInitialRotation[i] = mpRightLaserNodes[i]->Local.GetRotate();
		}
		printf("[WiiSaber] animation bindings: small rings=%u, big rings=%u, laser pairs=%u/%u, wrappers=%u/%u\n",
			mSmallRingNodeCount, mRingNodeCount - mBigRingNodeOffset,
			mLeftLaserNodeCount, mRightLaserNodeCount,
			mpLeftLaserWrapper ? 1 : 0, mpRightLaserWrapper ? 1 : 0);
	}
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
	mRingRotationCounter = false;
	mRingZoomEventIndex = 0;
	mRingRotationElapsed = 0.0F;
	mLeftWrapperAngle = 0.0F;
	mRightWrapperAngle = 0.0F;
	mLeftWrapperSpeed = 0.0F;
	mRightWrapperSpeed = 0.0F;
	for (UInt i = 0; i < 16; ++i) {
		mLeftLaserAngles[i] = mRightLaserAngles[i] = 0.0F;
		mLeftLaserSpeeds[i] = mRightLaserSpeeds[i] = 0.0F;
	}
	for (UInt i = 0; i < mRingNodeCount; ++i) {
		const Bool smallRing = i < mBigRingNodeOffset;
		const UInt ringIndex = smallRing ? i : i - mBigRingNodeOffset;
		mRingRotation[i] = 45.0F + (smallRing ? ringIndex * 5.0F : 0.0F);
		mRingRotationTarget[i] = mRingRotation[i];
		mRingZoom[i] = mRingInitialPosition[i].Z();
		mRingZoomTarget[i] = mRingZoom[i];
		mRingRotationStartDelay[i] = 0.0F;
		mpRingNodes[i]->Local.SetTranslate(mRingInitialPosition[i]);
		Matrix3F startupRotation(Vector3F::UNIT_Z,
			mRingRotation[i] * 3.14159265358979323846F / 180.0F);
		mpRingNodes[i]->Local.SetRotate(mRingInitialRotation[i] * startupRotation);
	}
	for (UInt i = 0; i < mLeftLaserNodeCount; ++i)
		mpLeftLaserNodes[i]->Local.SetRotate(mLeftLaserInitialRotation[i]);
	for (UInt i = 0; i < mRightLaserNodeCount; ++i)
		mpRightLaserNodes[i]->Local.SetRotate(mRightLaserInitialRotation[i]);
	if (mpLeftLaserWrapper)
		mpLeftLaserWrapper->Local.SetRotate(mLeftWrapperInitialRotation);
	if (mpRightLaserWrapper)
		mpRightLaserWrapper->Local.SetRotate(mRightWrapperInitialRotation);
	if (mpEffectLight) mpEffectLight->Enabled = false;
}

// GLS callbacks are placeholders until the beatmap runner parses and dispatches
// GLS event boxes. Implement these when that parser support is added.
void DefaultEnvironmentAnimationCallbacks::GLSColourCallback(UInt, UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSTransformCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSRotationCallback(UInt, UInt, Float, Float, Float) {}

void DefaultEnvironmentAnimationCallbacks::GLSFloatFXCallback(UInt, UInt, Float) {}

void DefaultEnvironmentAnimationCallbacks::FloatFXEventLaneCallback(UInt lane, Float value) {
	if (lane == 8) {
		if (!mRingNodeCount) return;
		mRingRotationCounter = !mRingRotationCounter;
		const Float direction = mRingRotationCounter ? -1.0F : 1.0F;
		const Float firstRingTarget = mRingRotationTarget[0] +
			direction * 90.0F;
		mRingRotationElapsed = 0.0F;
		for (UInt i = 0; i < mRingNodeCount; ++i) {
			const Bool smallRing = i < mBigRingNodeOffset;
			const UInt ringIndex = smallRing ? i : i - mBigRingNodeOffset;
			const Float rotationStep = smallRing ? 10.0F : 5.0F;
			const Float stepDirection = mRingRotationCounter ? -1.0F : 1.0F;
			mRingRotationTarget[i] = firstRingTarget +
				(ringIndex * rotationStep * stepDirection);
			mRingRotationStartDelay[i] = ringIndex * 0.02F;
		}
		return;
	}
	if (lane == 9) {
		if (value < 0.0F) return;
		mRingZoomEventIndex++;
		// ChroMapper uses alternating 1.0 / 3.5 world units for the
		// DefaultEnvironment legacy ring-zoom values.
		const Float step = (mRingZoomEventIndex & 1) ? 1.0F : 3.5F;
		for (UInt i = 0; i < mRingNodeCount; ++i) {
			const UInt ringIndex = i < mBigRingNodeOffset ? i :
				i - mBigRingNodeOffset;
			mRingZoomTarget[i] = mRingInitialPosition[i].Z() +
				ringIndex * step;
		}
		return;
	}
	if (lane == 12) {
		if (value == 0.0F) {
			for (UInt i = 0; i < mLeftLaserNodeCount; ++i) {
				mLeftLaserSpeeds[i] = 0.0F;
				mLeftLaserAngles[i] = 0.0F;
			}
			mLeftWrapperSpeed = 0.0F;
			mLeftWrapperAngle = 0.0F;
		} else if (value > 0.0F) {
			for (UInt i = 0; i < mLeftLaserNodeCount; ++i) {
				const Float direction = (i & 1) ? -1.0F : 1.0F;
				mLeftLaserAngles[i] = LaserPairStartAngle(i);
				mLeftLaserSpeeds[i] = value * 20.0F * direction;
			}
			mLeftWrapperAngle = 0.0F;
			mLeftWrapperSpeed = -value * 40.0F;
		}
		return;
	}
	if (lane == 13) {
		if (value == 0.0F) {
			for (UInt i = 0; i < mRightLaserNodeCount; ++i) {
				mRightLaserSpeeds[i] = 0.0F;
				mRightLaserAngles[i] = 0.0F;
			}
			mRightWrapperSpeed = 0.0F;
			mRightWrapperAngle = 0.0F;
		} else if (value > 0.0F) {
			for (UInt i = 0; i < mRightLaserNodeCount; ++i) {
				const Float direction = (i & 1) ? -1.0F : 1.0F;
				mRightLaserAngles[i] = -LaserPairStartAngle(i);
				mRightLaserSpeeds[i] = -value * 20.0F * direction;
			}
			mRightWrapperAngle = 0.0F;
			mRightWrapperSpeed = value * 40.0F;
		}
	}
}

void DefaultEnvironmentAnimationCallbacks::ColourEventLaneCallback(
	UInt lane, ColorRGB colour, Float brightness) {
	if (lane >= 20) return;
	if (brightness < 0.0F) brightness = 0.0F;
	if (brightness > 1.0F) brightness = 1.0F;
	const Float red = colour.R() * brightness;
	const Float green = colour.G() * brightness;
	const Float blue = colour.B() * brightness;
	mLaneColors[lane] = colour;
	mLaneBrightness[lane] = red + green + blue > 0.01F ? brightness : 0.0F;
	if (mpScene) {
		const ColorRGB lightColor(red, green, blue);
		for (UInt i = 0; i < TargetCount(lane); ++i) {
			const Char *targetName = TargetName(lane, i);
			if (!targetName) continue;
			if (EnvironmentLighting::SetNamedLightColor(
				mpScene, targetName, lightColor)) {
				mRenderStateDirty = true;
			} else if (!mMissingLightTargetLogged[lane][i]) {
				printf("[WiiSaber] no colorable light target for lane %u: %s\n",
					lane, targetName);
				LogColorTargetTree(mpScene, targetName);
				mMissingLightTargetLogged[lane][i] = true;
			}
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
			// Average active lanes and boost the scene fill light's color.
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

void DefaultEnvironmentAnimationCallbacks::AnimationUpdate(Float deltaSeconds) {
	if (deltaSeconds < 0.0F) deltaSeconds = 0.0F;
	const Float zoomBlend = 1.0F - powf(2.0F, -5.0F * deltaSeconds);
	mRingRotationElapsed += deltaSeconds;
	for (UInt i = 0; i < mRingNodeCount; ++i) {
		if (mRingRotationElapsed >= mRingRotationStartDelay[i]) {
			const Bool smallRing = i < mBigRingNodeOffset;
			const Float speed = smallRing ? 4.0F : 2.0F;
			const Float tickBlend = 0.02F * speed;
			const Float ringBlend = tickBlend >= 1.0F ? 1.0F :
				1.0F - powf(1.0F - tickBlend, deltaSeconds / 0.02F);
			mRingRotation[i] +=
				(mRingRotationTarget[i] - mRingRotation[i]) * ringBlend;
		}
		mRingZoom[i] += (mRingZoomTarget[i] - mRingZoom[i]) * zoomBlend;
		Matrix3F ringRotation(Vector3F::UNIT_Z,
			mRingRotation[i] * 3.14159265358979323846F / 180.0F);
		mpRingNodes[i]->Local.SetRotate(
			mRingInitialRotation[i] * ringRotation);
		Vector3F position = mpRingNodes[i]->Local.GetTranslate();
		position.Z() = mRingZoom[i];
		mpRingNodes[i]->Local.SetTranslate(position);
	}

	for (UInt i = 0; i < mLeftLaserNodeCount; ++i) {
		mLeftLaserAngles[i] += mLeftLaserSpeeds[i] * deltaSeconds;
		const Matrix3F leftRotation(Vector3F::UNIT_Y,
			mLeftLaserAngles[i] * 3.14159265358979323846F / 180.0F);
		mpLeftLaserNodes[i]->Local.SetRotate(
			mLeftLaserInitialRotation[i] * leftRotation);
	}
	for (UInt i = 0; i < mRightLaserNodeCount; ++i) {
		mRightLaserAngles[i] += mRightLaserSpeeds[i] * deltaSeconds;
		const Matrix3F rightRotation(Vector3F::UNIT_Y,
			mRightLaserAngles[i] * 3.14159265358979323846F / 180.0F);
		mpRightLaserNodes[i]->Local.SetRotate(
			mRightLaserInitialRotation[i] * rightRotation);
	}

	mLeftWrapperAngle += mLeftWrapperSpeed * deltaSeconds;
	mRightWrapperAngle += mRightWrapperSpeed * deltaSeconds;
	const Vector3F leftWrapperAxis(-0.5565387F, -0.24192187F, -0.79481965F);
	const Vector3F rightWrapperAxis(0.5565387F, -0.24192187F, -0.79481965F);
	if (mpLeftLaserWrapper)
		mpLeftLaserWrapper->Local.SetRotate(mLeftWrapperInitialRotation *
			EulerRotation(leftWrapperAxis, mLeftWrapperAngle));
	if (mpRightLaserWrapper)
		mpRightLaserWrapper->Local.SetRotate(mRightWrapperInitialRotation *
			EulerRotation(rightWrapperAxis, mRightWrapperAngle));

	// Apply queued render-state changes once per frame after callbacks modify
	// materials.
	if (mRenderStateDirty && mpScene) {
		mpScene->UpdateRS();
		mRenderStateDirty = false;
	}
}

}
