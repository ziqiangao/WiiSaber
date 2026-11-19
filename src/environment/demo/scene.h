#pragma once

#include "WireNode.h"

namespace BeatScene {

Wire::Node *CreateDefaultEnvironment();
void UpdateEnvironment(Wire::Node *scene, Float elapsedTime);

}
