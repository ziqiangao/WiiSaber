# Porting a Beat Saber environment

Environment behavior is script-bound. The scene export supplies named nodes
and assets; the C++ handler maps beat events to those nodes.

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

`LoadBeatmapCallbacks()` reads and schedules events. `UpdateBeatmapCallbacks()`
dispatches events as playback advances. The handler should:

1. Store the latest event state in its callback method.
2. Apply ongoing motion in `AnimationUpdate()`, called once per frame.
3. Set a known state in `Reset()` so a new run starts predictably.

The current interface has no delta-time argument. Add one to
`AnimationUpdate()` and its call in `main.cpp` if an effect needs frame-rate
independent time integration. Float FX lane callbacks are discrete values; the
runner does not interpolate them.

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

For a color lane, map the lane ID to only its renderable light nodes. Do not
bind a whole structure if it contains non-light geometry. The DefaultEnvironment
bindings in `anicallbacks/DefaultEnvironmentLighting.cpp` show named node
lookup and per-instance coloring. Map colors are normalized RGB values in the
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
The DefaultEnvironment's additive halo is a runtime geometry example, not a
shared utility yet.

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
