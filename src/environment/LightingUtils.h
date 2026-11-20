#pragma once

#include "WireTypes.h"
#include "WireColorRGB.h"

namespace Wire { class Node; }

namespace BeatScene {
namespace EnvironmentLighting {

// Apply the shared light-object overdrive curve and additive halo to every
// renderable node below scene objects with this name. Returns changed objects.
UInt SetNamedLightColor(Wire::Node *scene, const Char *nodeName,
	Wire::ColorRGB color);

} // namespace EnvironmentLighting
} // namespace BeatScene
