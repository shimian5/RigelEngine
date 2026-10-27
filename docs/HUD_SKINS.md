# Overlay HUD skins

The "Overlay" HUD style draws the status display on top of a full-height game view, at native
screen resolution. Its artwork comes from a *skin*, which is resolved in this order:

1. **Your own / imported files**: a `hud_skin` folder inside the game directory
   (or the folder named by the `RIGEL_HUD_SKIN_DIR` environment variable), containing
   `modern_hud_1080.png`, `modern_hud_480.png` and/or `modern_hud_272.png`.
   The atlas must use the sprite layout described in `re-notes/HUD_OVERLAY.md`
   (41 sprites per tier, fixed rectangles). The tier is chosen from the window height
   (>=900 -> 1080, >=400 -> 480, >=256 -> 272, else 240 which reuses the 272 atlas).
2. **Generated default**: if no skin file is found, the engine draws its own artwork
   (`src/ui/hud_skin_generator.cpp`) using the 1080 layout, with icons taken from the game's
   own sprites. No external files are needed.

## Importing from your own Evercade cartridge dump

If you legally own the cartridge and have dumped it, `tools/import_evercade.py` extracts the
HUD artwork:

    python3 tools/import_evercade.py /path/to/cart/uranium /path/to/your/game/folder

The archives on the cartridge are encrypted with a key contained in the cartridge's own
`RigelEngine` binary. The tool finds that key itself (by trial decryption) and keeps it in
memory only. Nothing is stored or shipped by this project. The extracted files are the
cartridge's artwork: keep them for your own use, do not redistribute them.

## Developer notes
- `RIGEL_HUD_TIER=1080|480|272|240` forces a tier. `RIGEL_HUD_SKIN_DIR` selects the skin folder.
- Fonts: the score uses Russo One (SIL OFL 1.1), bundled in `3rd_party/fonts`.
- The HUD is drawn via `IGameServiceProvider::setNativeOverlay`, which runs after the game image
  has been upscaled, so it is independent of the low-res pixel grid.
