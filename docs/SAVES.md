# Disk saves (replay saves)

Upstream RigelEngine only keeps quick saves in memory. This fork additionally writes them to disk,
so they survive quitting the game or Android killing the app.

## How it works
The game logic is deterministic (original-style fixed 15 Hz updates, table-based RNG). A save
therefore does not store the world. It stores:
- how the level was started (episode, level, difficulty, the player's state at level start,
  checkpoint position, gameplay style),
- the simulation viewport used (it decides which objects are active),
- the player's input for every logic update since then (run-length encoded),
- a hash of the world state, to verify the result after loading.

Loading creates the level and fast-forwards through the input log with sound muted (about 0.1 ms
per update on a desktop CPU, so even a long level takes about a second or two). If the state hash
doesn't match afterwards, a warning is logged ("Replay save diverged").

Files: `<prefs>/saves/quicksave.dn2` (F5 / L1+Start) and `autosave.dn2` (written at level start).
Typical size: under 2 KB. Format: `src/frontend/replay_save.{hpp,cpp}`, versioned (magic `RGLS`).

## Known limits
- The window or HUD style changing mid-level triggers a camera recenter inside `render()`,
  which can make a replay drift slightly (the hash check will warn).
- Saves are tied to the build's game logic: a save from a different logic version may diverge.
- The Classic and Enhanced gameplay styles are separate worlds; a save loads in the style it was made in.

## Developer hooks (build with -DRIGEL_DEV_SCREENSHOT=ON)
- `RIGEL_REPLAY_SELFTEST=<ticks>`: play scripted input, save, replay into a second world, compare hashes.
- `RIGEL_DEV_SCRIPTED_INPUT=1`, `RIGEL_DEV_SAVE_AT_TICK=<n>`, `RIGEL_DEV_LOAD_SAVE=<slot>`.

## Console-style pause menu
With the "Console-style pause menu" option on (default on Android), Esc / controller Start / Android Back
open `ui::PauseMenu` instead of the original DOS menus: Quick Save, Load Last Save, Save Game (8 slots),
Load Game, Options (the regular options screen) and Quit, with episode/level/score/time shown beside it.
F1/H/F2/F3 and the other DOS menu keys are inactive in this mode. Slots are `slot1.dn2`..`slot8.dn2`
in the saves folder; "Load Last Save" picks the most recently written save of any kind.

## Console main menu
With console menus on, the title/main menu is `ConsoleMenuMode` (frontend/console_menu_mode.*) instead of the
scripted DOS menu: New Game, Episode Select (episodes 2-4 locked in the shareware), Load Game (quick save,
autosave and 8 slots), Stats (per-episode high scores), Options, Quit. Menus share the drawing code in
`ui/console_menu.*` with the pause menu. `createMainMenuMode()` picks the console or DOS version.
Still the original DOS screens: intro/Apogee logo, story/instructions, bonus screens, end-of-episode
sequences and high score name entry.

## Title screen, quit prompt, boss bar and timer
- With console menus on, startup shows a title screen ("A START") unless `--skip-intro` / the "Skip intro
  sequence" option is set; it replaces the Apogee logo and intro movie.
- Quitting from the main menu or the pause menu asks for confirmation first (default answer: keep playing),
  with a random quip. The wording is our own.
- The overlay HUD draws the boss health bar (bottom, right of the radar panel) and an optional level timer
  (`Show level timer` option, under the score panel). Both come from the HUD skin atlas.
