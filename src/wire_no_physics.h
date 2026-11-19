#pragma once

#include "WireVector3.h"

// Wire3D's importer calls this method even when Bullet support is disabled.
// The WiiSaber scene importer intentionally does not construct physics worlds.
class PhysicsWorld {
public:
    void SetGravity(const Wire::Vector3F &) {}
};
