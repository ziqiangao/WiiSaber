# WiiSaber

Wii homebrew scene viewer built with devkitPPC, libogc, and the Wire3D
submodule. The active DefaultEnvironment scene and animation bindings are
currently selected directly in C++.

## Build

Initialize the Wire3D submodule after cloning:

```sh
git submodule update --init --recursive
source /etc/profile.d/devkit-env.sh
make
```

The build produces `wiisaber.dol` in the project root.

## Scene files

Place the DefaultEnvironment Wire3D export and its assets here:

```text
sd:/apps/wiisaber/environments/DefaultEnvironment/DefaultEnvironment.xml
sd:/apps/wiisaber/environments/DefaultEnvironment/<mesh and texture assets>
```

The beatmap path, BPM, and camera are set in `src/app/main.cpp`. Default
lighting and event lane registrations are in
`src/environments/DefaultEnvironment/anicallbacks/DefaultEnvironment.cpp`;
the color lane to scene node bindings are in
`src/environments/DefaultEnvironment/anicallbacks/DefaultEnvironmentLighting.cpp`.

The DefaultEnvironment lighting handler defines its fallback light colors
there. When `Info.dat` declares environment colors, the runner uses those colors
for basic light events; maps without declared environment colors use the
environment defaults.

## Environment animation callbacks

Beatmap events are dispatched through `EnvironmentAnimationCallbacks` in
`src/environment/AnimationCallbacks.h`. An environment can subclass it and
override `GLSFloatFXCallback(group, id, value)`,
`GLSTransformCallback(group, id, x, y, z)`,
`GLSRotationCallback(group, id, x, y, z)`,
`GLSColourCallback(group, id, col, brightness)`,
`FloatFXEventLaneCallback(lane, value)`, and
`ColourEventLaneCallback(lane, colour, brightness)`. `Reset()` establishes the
known start state; `AnimationUpdate()` runs once per rendered frame so an
environment can advance ongoing effects. The beatmap parser dispatches events
forward from beat zero without seeking.

DefaultEnvironment currently applies basic color lanes and boost palettes to
its named light meshes. Float FX and GLS callbacks are interface hooks; GLS
event groups are not yet parsed or dispatched, and Float FX lanes do not yet
animate the scene. See the [environment porting guide](src/environments/PORTING.md)
for the implementation steps and current support boundary.

## Camera controls

Use the Wii Remote D-pad to move forward/back and strafe, buttons **1/2** to
turn, and **A/B** to move up/down.

Wire3D is included as an LGPL-3.0 submodule. Its source and license are in
`third_party/Wire3D`.

## Source layout

```text
src/
  app/                         Wii application entry point and frame loop
  beatmap/                     Versioned event parsers and shared playback
  environment/                 Shared callback interfaces and demo scene
  environments/
    DefaultEnvironment/
      anicallbacks/            handler registration and lighting bindings
        DefaultEnvironment.cpp environment setup and event registry
        DefaultEnvironmentLighting.cpp color lane scene bindings
```

Environment specific scene behavior belongs under its environment folder;
shared event interfaces and beatmap playback stay independent of any one
environment.

See [the environment porting guide](src/environments/PORTING.md) for scene
setup, callbacks, transforms, basic lighting, and GLS implementation notes.
