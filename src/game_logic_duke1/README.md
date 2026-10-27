# Duke 1 — M5 phases 1 and 2

Independent C++17 DOS asset/still-scene adapter and deterministic core sandbox. These new files intentionally have **no dependency on Duke II game state, renderer constants, physics or launcher code**. Existing DN2 files and the launcher entry hook are untouched; the user will add that hook.

This coding milestone explicitly supersedes M4's read-only assumption **for additions under src/game_logic_duke1**. Source evidence is the original 1991 DOS shareware and the independently authored research in re-notes/dn1. No commercial/cart/Evercade input, decompiled game source, bundled EXE or game asset is included. Generated maps/exports/tests belong in build-host-dn1 or dn1-work/out, never in this source directory.

## Standalone build

From any directory, configure the new build directory; this does not configure the parent project or use its existing build tree:

```sh
cmake -S /home/matt/rigel/rigel-upstream/src/game_logic_duke1 \
  -B /home/matt/rigel/build-host-dn1 -DCMAKE_BUILD_TYPE=Debug \
  -DDN1_RESEARCH_ROOT=/home/matt/rigel/dn1-work \
  -DDN1_DATA_DIR=/home/matt/rigel/DN1 \
  -DPython3_EXECUTABLE=/home/matt/rigel/dn1-work/.venv/bin/python
cmake --build /home/matt/rigel/build-host-dn1 -j2
ctest --test-dir /home/matt/rigel/build-host-dn1 --output-on-failure
```

The static libraries and CLIs themselves need a C++17 standard library only; Python/Pillow and the original-routine records are validation dependencies. Normal asset loading does not need DN1.EXE, Ghidra, Unicorn or dn1_spec.json. For fresh research fixtures, run `make core-stage1` in dn1-work before CTest. CTest launches finite, silent command processes with timeouts, no SDL window/audio device and no handheld/Pi interaction.

## Phase 1 — asset adapter and still map

`assets.hpp` exposes byte parsers, case-insensitive flat-directory resource lookup, decoded tile/backdrop/screen/sound structures, and `render_scene(...)` returning an image plus warnings. `dn1_scene` is the standalone reference client. The future launcher can upload the returned RGBA buffer through the host renderer and display the warnings; no original DOS file is embedded in the library.

```sh
/home/matt/rigel/build-host-dn1/dn1_scene /home/matt/rigel/DN1 \
  WORLDAL1.DN1 /home/matt/rigel/build-host-dn1/worldal1.ppm --compat
```

The PPM and raw RGBA companion are **full-map static previews, 2048x1440**, matching the Python viewer's map canvas pixel-for-pixel (annotation/header text is outside that canvas). Active cells are left black, never rendered as ordinary tiles. This is not a running level, actor simulation or DOS screenshot recreation. The preview repeats backdrop pieces in map coordinates solely to match the accepted viewer; the eventual gameplay backdrop is screen-fixed.

Strict mode is the default and rejects the supplied short DROP3. Explicit `--compat` decodes its51 complete images, ignores the29-byte partial tail and leaves missing pixels transparent/black. It emits **DROP3 incomplete (shareware)** on stderr and in metadata; `Scene.warnings` carries it to a future UI. A complete20,803-byte replacement loads normally under the same filename. WORLDAL2 has no standalone pair, so the default scene is flat and visibly warns; a verified prior-level pair can be passed with `--context FIRST,SECOND`. `--flat` is an explicit preview option.

All indexed graphics and full screens use the researched original palette/mask layout. Sound parsing preserves header metadata and placeholder warnings; it does not implement audio playback or reinterpret the second-bank runtime-priority anomaly. The CLI's `--inspect` export is for local validation, not asset distribution.

## Phase 2 — independent core

`core.hpp` exposes:

- `PlayerState` with explicit X8, pixel Y and camera-word units; six held input states and persistent jump/fire latches.
- `CollisionWorld`, a mutable128x90 uint16 map with the original coarse sensors. Out-of-map samples are **a declared solid sandbox boundary policy**, not a recovered DOS memory-read behavior.
- `Sandbox::movement()` corresponding to the pre-render stage1 stop boundary. `lateUpdate()`, `cameraY()`, `drawPlayer()`, `damage()` and projectile allocation/motion/culling are independently callable for matching the other original fixtures.
- `Sandbox::tick()` composes the resolved sandbox subset. It does not execute actor-family callbacks, map-loader mutations, a HUD, every player animation pose or full level/game flow.
- `TickGate`, integer timer-callback gating with the researched0x2051 divisor and selected speed0..9. Excess callbacks are discarded after one update, matching the inspected reset policy. The default speed0 is an explicit **sandbox choice**, not a proved original default. Rendering never advances the simulation by itself.

The core uses arithmetic coordinate rules and jump displacement tables from the specification, not a translated/decompiled routine or stored expected trajectory. Event output includes abstract sound requests and map-contact/score notifications; no audio device is opened. First-update projectile flags, coarse tile impacts and the proved0x1800 breakable exception are supported; entity hits and effects are phase3+ work.

The sandbox CLI reads a finite script on stdin and writes one JSON state per stepping/query command:

```sh
printf 'reset\nqueries 1 1 1 0\ninput 0 1 0 0 0 0\nmove\nmove\n' | \
  /home/matt/rigel/build-host-dn1/dn1_sandbox
```

`queries LEFT RIGHT HEAD FLOOR` supplies explicit clear/blocked responses, matching the original controlled probes. `world` returns to actual map sampling; `fill VALUE` and `cell INDEX VALUE` create synthetic geometry. Use `--help` for commands. `speed N` selects the tick gate and `advance N` supplies timer callbacks, advancing at most one sandbox update. `move`, `late`, `camera`, `draw` and `bullets` remain separate because their scheduling boundaries matter to fidelity. DOS word fields/events are serialized as unsigned16-bit values while sensor arithmetic uses signed coordinates.

A useful bounded sequence for real map collisions:

```text
reset
fill 0
cell 775 6144
cell 776 6144
world
input 0 1 0 0 0 0
move
state
```

## Validation and limits

`tests/validate_core.py` compares the C++ output to the independently generated original-routine stage1 JSON, including all recorded movement state fields/events, collision/ceiling side effects, camera states, damage/immunity, bullet allocation, fire-edge behavior and pull-up states. It also verifies integer tick gating and repeatability. It does not use the C++ implementation to generate its expected values.

`dn1-work/tools/check_m5_differential.py BUILD/dn1_sandbox` additionally executes500 fresh original movement updates,100 nonuniform actual-map collision queries and10 original projectile/world cases; this optional research check needs Unicorn and the preserved original unpacked EXE. It writes local evidence under dn1-work/out/m5. It is not a runtime dependency.

`tests/validate_assets.py` compares C++ scenes against crops from the Python viewer and independent parser exports, plus strict/compat replacement behavior. Expected asset bytes are decoded locally from the user's DOS files; no asset golden is committed.

User-provided Pi DOSBox captures in dn1-work/reference contain an original320x200 level1 frame with borders/HUD and four brief-tap frames, from fixed4000 cycles, silent DOSBox0.74. They are visual/layout evidence only. They corroborate the nominal13x10 play area near(16,16), grey border and right-side score/health/firepower/inventory panels. They are not used to set the core timestep or infer held-key speed, and they are not redistributed with this code.

M5 phases1–2 stop here. A complete real-map player run needs phase3 processed-map initialization and actor interactions. Unknown mechanisms are not replaced with guessed Duke II behaviors. The existing DN2 build, launcher hook and upstream files remain the user's work.
