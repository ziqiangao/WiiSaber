# Agent guide

## Project overview

WiiSaber is a Wii homebrew scene viewer built with devkitPPC, libogc, and
Wire3D. The active environment, beatmap, BPM, and camera are selected directly
in `src/app/main.cpp`. Environment scene exports and their assets are loaded
from the SD card at runtime.

## Build and validation

Initialize submodules after cloning:

```sh
git submodule update --init --recursive
source /etc/profile.d/devkit-env.sh
make
```

The build produces `wiisaber.dol`. Run `git diff --check` after edits. Use
Dolphin OSReport logs to verify scene import, event parsing, and scene-node
bindings; a successful build does not verify that names in the SD-card scene
export match the handler's targets.

## Source map

- `src/app/main.cpp`: scene, beatmap, BPM, frame loop, and callback lifecycle.
- `src/beatmap/`: V3/V4 parsing, event scheduling, palette resolution, and
  callback dispatch.
- `src/environment/AnimationCallbacks.h`: shared callback interface.
- `src/environment/LightingUtils.*`: shared render-material and Wire light
  coloring helpers.
- `src/environments/DefaultEnvironment/anicallbacks/DefaultEnvironment.*`:
  DefaultEnvironment event registration, target binding, and animation state.
- `src/environments/PORTING.md`: environment-porting process and ChroMapper
  source map.
- `third_party/ChroMapper/`: ChroMapper `dev` reference submodule.

Keep environment-specific behavior under that environment's directory. Keep
beatmap parsing and shared callback behavior independent of a particular
environment.

## Beatmap event IDs and light targets

The color callback's lane is the basic event type from the map file: V3
`_type` (or its `et` alias) and V4 `t` in `basicEventsData`. It is not a
ChroMapper `LightIntensityData.ID`, a scene-node index, or a zero-based array
offset. Chroma custom per-light IDs such as `_lightID`/`lightID` are a separate
feature; do not silently treat them as basic event types.

ChroMapper's `DefaultEnvironmentTrackDefinitions.asset` is useful for
understanding event-type semantics, but its Unity object names and component
IDs do not prove that the WiiSaber XML export contains renderable targets with
those names. Confirm bindings against the actual exported scene loaded by the
Wii. A target can be a parent transform if the renderable meshes or Wire light
nodes are descendants. Do not color an entire structural subtree if it
includes unrelated geometry.

For missing-target reports, distinguish:

- no matching scene nodes;
- matching nodes with no renderable descendants;
- descendants represented as Wire `NodeLight`/attached lights rather than
  render objects.

The DefaultEnvironment handler logs binding counts and one-time details for
uncolorable target subtrees. Use those diagnostics before changing the event
parser or guessing scene names.

## Current DefaultEnvironment debugging note

Basic event dispatch currently uses the file-format event type. A recent
runtime log reported that color lanes 2 and 3 had no colorable targets for
`BaseL`, `BaseR`, and several related names. The latest local change adds color
updates for Wire lights as well as mesh materials and prints the target
subtree when no colorable object is found. That change builds, but still needs
to be tested with the user's SD-card scene export. Treat the runtime node
hierarchy as authoritative; do not assume ChroMapper's Unity names are
preserved in that export.

## Environment animation conventions

- Cache scene nodes and authored transforms during `Bind()`.
- Store event state in callbacks; advance continuous movement in
  `AnimationUpdate(deltaSeconds)`.
- Restore authored transforms and clear animation/color state in `Reset()`.
- Animate `Local` transforms, then let the frame loop propagate them with
  `UpdateGS()`.
- Call `UpdateRS()` after changing render state.
- Preserve per-object material state; do not mutate a shared mesh or vertex
  buffer to color one instance.

See `src/environments/PORTING.md` for detailed callback flow, scene-export
guidance, and current parser support.
