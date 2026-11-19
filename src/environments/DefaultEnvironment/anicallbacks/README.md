# Registering environment event lanes

Lane IDs are Beat Saber basic event type IDs. Each environment registers its
supported lanes through `RegisterLane(id, kind)` in its callback handler.
`GetLanes(count)` exposes those registrations to the beatmap runner, which
dispatches only lanes the active environment registered. This lets an
environment define additional color lanes without changing the runner.

The DefaultEnvironment registers color lanes `0` through `4` and Float FX
lanes `8`, `9`, `12`, and `13`. To add another color lane, register its ID as
`ANIMATION_LANE_COLOUR` in `DefaultEnvironmentAnimationCallbacks`' constructor,
then add the corresponding node bindings to `TargetName()` and `TargetCount()`.

GLS animation targets use a separate registry. Call
`RegisterGLSGroup(groupId, types, typeCount, objects, objectCount)` from the
environment handler to store each supported group ID, its event type IDs, and
the group's object ID to scene node name mappings. `GetGLSGroups(count)` returns
the registered groups. A type ID is the GLS event family supported for that
group; keep it aligned with the GLS event parser and callback handler. The
registry uses fixed capacities of 16 groups, 4 types per group, and 16 objects
per group. Re-registering an existing group ID replaces its registration.

The beatmap runner reads the basic event's type (`_type` in older maps or `et`
in current maps) and dispatches it according to the registered lane kind:

- `ANIMATION_LANE_COLOUR` calls `ColourEventLaneCallback(lane, colour, brightness)`.
- `ANIMATION_LANE_FLOAT_FX` calls `FloatFXEventLaneCallback(lane, value)`.

The DefaultEnvironment color bindings are in `DefaultEnvironment.cpp`, in
`TargetName()` and `TargetCount()`. Current bindings are:

| Lane ID | DefaultEnvironment target nodes |
| --- | --- |
| 0 | `RotatingLasersPair` and its `(1)` through `(3)` copies |
| 1 | `NeonTubeBothSidesDirectional` and its `(1)` through `(3)` copies; these are the separate ring lights |
| 2 | `DoubleColorLaserL` and its `(1)` through `(4)` copies |
| 3 | `DoubleColorLaserR` and its `(1)` through `(4)` copies |
| 4 | `FrontLights`, `NeonTubeL`, `NeonTubeR` |

## Add or change a color lane

1. Export the environment and find the exact names of its renderable light
   nodes in the scene XML. For a ring, bind its separate light nodes; binding
   the ring mesh itself will color the whole mesh.
2. Add a row to the `names` array in `TargetName()` for the new event type ID.
   Add its number of node names at the same array index in `TargetCount()`.
   Keep the rows aligned: row 0 is lane 0, row 1 is lane 1, and so on.
3. Register the lane ID as `ANIMATION_LANE_COLOUR` in the handler constructor.
4. `ColourEventLaneCallback()` already applies the dispatched RGB color and
   brightness to all nodes returned by the lane table. Off events arrive as
   black. Flash, fade, and transition updates are dispatched during playback
   while their color tween is active.

Beat Saber basic color values use groups of four values per color: `0` is off,
`1`-`4` use environment color 0, `5`-`8` use environment color 1, and `9`-`12`
use white. The runner reads declared environment colors from the map's
`Info.dat`; otherwise `PrimaryColor()`, `SecondaryColor()`, and `WhiteColor()`
in `DefaultEnvironment.cpp` provide the fallback palette.

## Animate a Float FX lane

IDs `5` through `19` are delivered to `FloatFXEventLaneCallback(lane, value)`.
The existing map conventions include `8` for ring rotation, `9` for ring zoom,
`12` for left laser rotation, and `13` for right laser rotation. Add handling
for the ID in `DefaultEnvironmentAnimationCallbacks::FloatFXEventLaneCallback`
in `DefaultEnvironment.cpp`. Store the desired animation state in the handler,
then advance it from `AnimationUpdate()`, which the main loop calls once per
frame. The runner delivers Float FX callbacks when events occur; it does not
interpolate those values for you.

The generic callback declarations are in
[`../environment/AnimationCallbacks.h`](../environment/AnimationCallbacks.h).
The runner's event parsing and dispatch are in
[`../environment/BeatmapLights.cpp`](../environment/BeatmapLights.cpp).
