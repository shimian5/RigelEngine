# game_logic_duke1

Duke Nukem I gameplay, written independently from the original 1991 DOS program and data. It has no
dependency on Duke Nukem II code. No game data is included; levels, graphics and sounds are read from the
player's own DOS files at runtime.

| File | Purpose |
|---|---|
| `core.*` | Deterministic player, collision, jump, damage and projectile core with the original integer coordinate rules, driven by a fixed update gate |
| `play.*` | A running level: map, backdrops, actors, pickups, bonuses, camera and drawing into an RGBA frame |
| `actors.cpp`, `actor_table.inc` | Behaviour of every actor type, keyed by the original actor ID |
| `*_main.cpp` | Standalone tools: `dn1_sandbox` (scripted core), `dn1_scene` (still map render), `dn1_play_shot` (headless level run to an image), `dn1_episode_info` |
| `tests/` | Unit tests and original-behaviour comparisons |

Related code elsewhere: `assets/duke1_assets.*` (DOS file formats), `assets/duke1_exe_text.*` (unpacks the
EXE and reads the story and level tables), `audio/duke1_speaker.*` (PC speaker sounds),
`frontend/duke1_mode.*`, `duke1_saves.*` and `duke1_story.*` (game mode, saves, story screens).

## Building and testing

The game itself is built by the main project. The tools and tests also build on their own:

```sh
cmake -S src/game_logic_duke1 -B build-dn1 -DBUILD_TESTING=ON
cmake --build build-dn1
ctest --test-dir build-dn1 --output-on-failure
```

Tests that compare against the original program need local data and are off by default:
`-DDN1_DATA_DIR=<folder with the original DN1 files>` enables the data-based tests, and
`-DDN1_RESEARCH_ROOT=<folder with the recorded original-routine traces>` enables the trace comparisons.
Neither is needed to play.

## Running a level without a window

```sh
build-dn1/dn1_play_shot <DN1 folder> WORLDAL1 out.ppm 200 right:60 idle:20
```

Optional environment variables: `DN1_TELEPORT=col,row`, `DN1_GRANT=1`, `DN1_ALL=1`, `DN1_TRACE=1`.

## Notes on fidelity

Values that the original program defines (coordinates, tables, timing) follow it. Where the original
behaviour is not known, a choice is made on purpose and marked in the code, rather than borrowing Duke
Nukem II behaviour. Out-of-map reads in the core are treated as solid.
