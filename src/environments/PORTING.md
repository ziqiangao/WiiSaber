# Porting a Beat Saber environment

Environment behavior is script-bound. The scene export supplies named nodes
and assets; the C++ handler maps beat events to those nodes.

Use [ChroMapper `dev`](../../third_party/ChroMapper/) as the environment
reference: it contains the source implementation for the environments, meshes,
and animation behavior being ported to WiiSaber.

## Files and selection

Create `src/environments/<Name>/` for environment code and put its Wire3D export
and dependencies on the SD card at:

```text
sd:/apps/wiisaber/environments/<Name>/<Name>.xml
sd:/apps/wiisaber/environments/<Name>/<exported assets>
```

Use `DefaultEnvironment/anicallbacks/` as the layout example. Create a handler
subclassing `EnvironmentAnimationCallbacks`, add its `.cpp` to `CPPFILES` and
its source directory to `VPATH` and `INCLUDE` in `Makefile`, then change the
scene path and handler construction in `src/app/main.cpp`. That file also owns
the camera, beatmap path, and initial BPM. There is no environment config file
or runtime environment factory yet.

Keep names stable when exporting. The handler finds objects by their scene
node names; a missing or renamed node simply receives no callback effect.

## Frame and callback flow

Use this sequence to keep beatmap playback and scene updates consistent:

1. `LoadBeatmapCallbacks()` parses the map and stores ordered events.
2. `UpdateBeatmapCallbacks()` is called once per frame. It converts the elapsed
   seconds to beats and dispatches events whose beat time has arrived.
3. The handler callback stores or applies the event. Color callbacks may be
   called repeatedly by the runner for a map's tween or flash.
4. `AnimationUpdate(deltaSeconds)` is called once after beatmap dispatch. Use it for ongoing
   motion and queued scene-render-state changes.
5. `main.cpp` calls `scene->UpdateGS(time)`, then renders. This propagates
   `Local` transform changes through the scene graph.

`AnimationUpdate(deltaSeconds)` receives a capped frame delta from `main.cpp`.
Use it for frame-rate-independent movement. Float FX lane callbacks receive a
discrete value and are not interpolated by the beatmap runner.

Reset state in `Reset()` and call it before a run. In particular, reset cached
angles, speeds, transform targets, lane brightness, and render-state dirty
flags. `Reset()` does not rebuild the scene graph.

## Implement an animation

Treat event callbacks and frame updates differently:

- **Event callback:** receive the new value and cache it. Do not search the
  scene by name in a callback.
- **`AnimationUpdate(deltaSeconds)`:** advance cached state and write transforms or queued
  render state. This method is invoked once per frame.
- **`Reset()`:** restore the authored state and clear target values.

The recommended implementation pattern is:

1. In `Bind(Node *scene)`, find and cache each target `Node*` once. Store an
   authored starting transform if `Reset()` must restore it.
2. In `FloatFXEventLaneCallback()` or a GLS callback, write only the target
   state (for example `mRingSpeed = value`), not a frame-based angle.
3. In `AnimationUpdate(deltaSeconds)`, update cached nodes from that state and set
   `mRenderStateDirty` if render objects need `UpdateRS()`.
4. At the end of the frame, let `main.cpp` call `scene->UpdateGS(time)` so
   Wire3D propagates the new `Local` transforms.

Example (adapt the exported names and axes):

```cpp
// Event callback: cache state; don't search or animate here.
void MyEnvironment::FloatFXEventLaneCallback(UInt lane, Float value) {
    if (lane == RING_ROTATION_LANE) {
        mRingSpeed = value;
    }
}

void MyEnvironment::AnimationUpdate(Float deltaSeconds) {
    mRingAngle += mRingSpeed * deltaSeconds;
    Wire::Matrix3F rotation;
    rotation.FromAxisAngle(Wire::Vector3F::UNIT_Y, mRingAngle);
    if (mpRing) {
        mpRing->Local.SetRotate(rotation);
    }
}
```

The shared callback declaration and implementation use a delta-time argument:

```cpp
// AnimationCallbacks.h
virtual void AnimationUpdate(Float deltaSeconds) {}

// MyEnvironment.h and implementation
virtual void AnimationUpdate(Float deltaSeconds);

// src/app/main.cpp
mpAnimationCallbacks->AnimationUpdate(deltaTime);
```

Then add `mRingAngle += mRingSpeed * deltaSeconds` and use `deltaSeconds`
instead of `mFrameSeconds`. The caller must compute a valid delta once per
frame; the current `deltaTime` variable in `OnIdle()` is already capped at
0.1 seconds for camera motion.

The actual beatmap callback API is not a general animation timeline: it reports
event state and the runner also performs basic color tweens/flashes. Float FX
values are passed through as supplied. A one-shot transform can be set directly
from an event callback, but a continuous or easing effect should usually live in
`AnimationUpdate(deltaSeconds)`.

Animate `Local`, which is relative to the node's parent; avoid writing `World`
directly. Store the initial transform during `Bind()` if `Reset()` must restore
authored placement, scale, or rotation.

## ChroMapper animation source map

The ChroMapper submodule's DefaultEnvironment animation implementation is split
across these files:

- [`TrackLaneRingsRotationEffect.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Effects/Basic/TrackLaneRingsRotationEffect.cs)
  handles basic event type 8. It creates a rotation wave, propagates targets
  ring by ring, then eases each ring toward its target. The event's integer
  value is not used to decide whether to rotate.
- [`TrackLaneRingsPositionEffect.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Effects/Basic/TrackLaneRingsPositionEffect.cs)
  handles ring position/zoom events (type 9) with its own propagated,
  fixed-step movement.
- [`TrackLaneRing.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Components/Rings/TrackLaneRing.cs)
  applies an individual ring's local rotation and position.
- [`TrackLaneRingsManager.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Components/Rings/TrackLaneRingsManager.cs)
  owns the ordered ring list and their spacing. The effects operate on this
  list, not on the manager/container transform.
- [`LightRotationEffect.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Effects/Basic/LightRotationEffect.cs)
  and [`LightPairRotationEffect.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Effects/Basic/LightPairRotationEffect.cs)
  handle rotating laser event types.
- [`BasicEventEffectManager.cs`](../../third_party/ChroMapper/Assets/__Scripts/Environments/Effects/BasicEventEffectManager.cs)
  wires basic event types to their effect state managers.
- [`DefaultEnvironmentTrackDefinitions.asset`](../../third_party/ChroMapper/Assets/__Scripts/Environments/TrackDefinitions/DefaultEnvironmentTrackDefinitions.asset)
  declares the DefaultEnvironment event IDs. Serialized ring effect settings
  (including rotation angle, per-ring step, propagation, and speed) are in
  [`DefaultEnvironment.unity`](../../third_party/ChroMapper/Assets/__Scenes/Environments/DefaultEnvironment.unity).

For the current ChroMapper DefaultEnvironment scene, type 8 uses a 90-degree
rotation step. The small-ring system uses a 10-degree per-ring step and speed 4;
the big-ring system uses a 5-degree step and speed 2. Both start at 45 degrees;
the small-ring startup wave adds 5 degrees per ring, while the big-ring startup
step is zero. ChroMapper chooses direction and per-ring step through its event
state; WiiSaber uses a repeatable direction/step approximation and staggers
individual ring motion at 20 ms intervals. It does not reproduce ChroMapper's
deterministic playback/scrubbing evaluator.

The serialized ChroMapper ring lists contain 30 `SmallTrackLaneRing(Clone)`
objects and 15 `BigTrackLaneRing(Clone)` objects. The WiiSaber handler caches
only those clone names and sorts each set by local Z to approximate the
serialized near-to-far ring order. Do not assign ring event IDs by sorting
object names or by including the ring-manager nodes.

For laser rotation, ChroMapper type 12/13 effects operate on the `BaseL` and
`BaseR` transforms inside each `RotatingLasersPair`; the Unity scene also has
separate rotating lights `LightWrapper0` and `LightWrapper1`. Rotating the
`DoubleColorLaserL/R` color-event nodes is not equivalent. The WiiSaber handler
uses the BaseL/BaseR pair children and those two wrappers.

## Basic events

Register supported event type IDs in the handler constructor with
`RegisterLane(id, kind)`. The beatmap runner uses that registry to dispatch
`ANIMATION_LANE_COLOUR` to `ColourEventLaneCallback()` and
`ANIMATION_LANE_FLOAT_FX` to `FloatFXEventLaneCallback()`.

The current parser reads v3 basic events and v4 `basicEvents` data, resolves a
v4 lightshow file through `Info.dat`, reads normal and boost palettes, and
supports color boost events. The usual legacy IDs are color lanes `0`–`4`,
ring rotation/zoom `8`/`9`, and left/right laser speed `12`/`13`. IDs are event
types, not array positions; register the actual IDs your environment uses.

For a color lane, map the lane ID to only its renderable light nodes. A target
may be a parent transform when its renderable lights are descendants, but do
not include unrelated geometry in that subtree. The DefaultEnvironment
bindings in `anicallbacks/DefaultEnvironment.cpp` follow the ChroMapper
track definitions to map the file-format event type (`_type`/`et` in V3 or
`t` in V4) to targets; they do not use ChroMapper's internal light-component
IDs. `src/environment/LightingUtils.h` provides shared
overdriven light coloring and additive halo support. Map colors are normalized RGB values in the
range `0`–`1`; basic event values choose off, environment color 0, environment
color 1, or white. Boost events select the boost pair from the color scheme.
Fallback palettes belong in the environment handler.

For movement, find the relevant `Wire::Node` and edit its `Local` transform:

```cpp
node->Local.SetTranslate(Wire::Vector3F(x, y, z));
node->Local.SetScale(Wire::Vector3F(sx, sy, sz));
node->Local.SetRotate(rotationMatrix);
```

`Local` is relative to the parent. Wire3D derives world transforms from the
scene hierarchy when `UpdateGS()` runs; do not write `World` for normal
animation. Use `Matrix3F` to build rotations. Cache node pointers during `Bind`
instead of searching the whole scene every frame.

For per-object color, use a `StateMaterial` on the node and call `UpdateRS()`
after changing render states. Avoid editing a shared `Mesh` or
`VertexBuffer`: multiple scene instances can reference the same resource and
all change together. If render states differ between instances, give each a
separate `RenderObject`. `StateAlpha` provides source/destination blending;
the GX renderer supports alpha blending but has no post-processing bloom pass.
`src/environment/LightingUtils.h` provides shared overdrive coloring and
additive halo helpers for renderable light nodes.
The light meshes use an opaque material alpha while their color is driven;
additive halo copies retain their authored translucent alpha.

## GLS groups

The callback interface defines `GLSGroupRegistration` and handlers for color,
rotation, translation, and Float FX. `RegisterGLSGroup(groupId, types, ...,
objects, ...)` stores group IDs, supported event-family types, and object IDs
mapped to node names. A GLS group ID identifies the environment group; an
object ID selects a member of that group. These IDs are not basic lane IDs or
C++ array offsets.

**GLS event groups are not currently parsed or dispatched.** The registry and
callbacks alone do not make v3/v4 GLS lighting work. A complete GLS port needs
the corresponding v3/v4 event-box parser, index-filter and event-data lookup,
time/transition handling, and dispatch to these callbacks. Then register the
environment's real group/object IDs and implement the callbacks in its handler.
Use ChroMapper's environment data to identify IDs and exact node bindings; do
not guess IDs from scene order.

## Engine limits and port checklist

- Wire3D's GX renderer supports up to eight lights in a render object's light
  set. Keep environment and dynamic lights within that limit.
- Check the correct 4:3/16:9 camera framing in Dolphin; aspect compensation is
  selected from the Wii display setting in `main.cpp`.
- Add and select the scene and handler in `main.cpp`; add source paths to the
  Makefile; copy the XML and every referenced mesh/texture file to the SD card.
- Register and implement every basic lane used by the map. For animated lanes,
  store state on event callbacks and update transforms each frame.
- For GLS maps, implement GLS parsing and event dispatch before expecting group
  lighting to work.
- Build with `make`, then inspect Dolphin's OSReport log for scene import and
  beatmap loading messages. Verify object names, colors, off/fade behavior,
  reset state, and framing in the running scene.

See `../beatmap/README.md` for parser formats and current parser coverage.
