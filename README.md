# God of War: Betrayal — PSP and PS Vita ports

A native PlayStation Portable port of the 2007 J2ME game *God of War: Betrayal*. It reuses the translated C++ game logic from [samioan/gow-betrayal-remastered](https://github.com/samioan/gow-betrayal-remastered) (commit `bad458d`) and replaces the Windows platform layer with a PSP one. No Java emulator is involved.

The same core also builds for the **PlayStation Vita**: a vita2d renderer, `sceAudioOut` audio, a 640×360 framebuffer scaled 1.5× to the 960×544 panel and a LiveArea VPK. See [`vita/README_VITA.md`](vita/README_VITA.md).

The game data is **not** included. You supply your own copy of the Sony Ericsson K800i JAR, MIDlet version **1.4.20**.

## Contents

| Path | What it holds |
|---|---|
| `src/` | Shared game core, software renderer and Java runtime shims |
| `src/gen/` | Generated C++ translated from the decompiled Java (do not edit) |
| `psp/` | PSP entry point, audio, intro, localization and build scripts |
| `vita/` | PS Vita entry point, vita2d renderer, `sceAudioOut` audio, LiveArea VPK and deploy tools |
| `tools/gow_psp_patches.py` | Build-time adaptation of the generated game code |
| `third_party/stb/` | `stb_image` (public domain / MIT) |
| `dist/EBOOT.PBP` | Prebuilt executable, including optional XMB menu artwork |

Windows-only sources from the upstream PC port are intentionally omitted. Generated files are produced from `names.map` and the patch scripts; see `tools/gow_psp_patches.py` and `psp/gen_locales.py`.

## Play on a PSP

1. Copy the contents of `dist/` into `PSP/GAME/GOWBETRAYAL/` on the memory stick (or `PSP/GAME/` on internal storage for a PSP Go). A firmware that allows unsigned homebrew is required.
2. Extract the banks from your JAR into the same folder:

   ```sh
   python3 psp/prepare_data.py God-of-War-Betrayal_K800i_v1420.jar PSP/GAME/GOWBETRAYAL
   ```

3. Launch **Game → Memory Stick → God of War: Betrayal**.

`prepare_data.py` verifies the JAR version and the RP19 layout, then writes exactly 31 unmodified resource banks into `data/`. An optional `INTRO.BIN` placed next to `EBOOT.PBP` plays before the game; press **Start** or **Cross** to skip it.

Saves are written to `GOWBETRAYAL/saves/`. Replace only `EBOOT.PBP` when updating, so progress is kept.

### PPSSPP

Open `dist/EBOOT.PBP` with **File → Load**, after running `prepare_data.py` against the `dist/` folder as well. Do not launch it from a path containing `PSP/GAME`: PPSSPP interprets that as a path on its own virtual memory stick. Verified with PPSSPP 1.20.4. For a crisp pixel grid use an integer window scale and **Nearest** texture filtering.

## Play on a PS Vita

1. Build the VPK — needs [VitaSDK](https://vitasdk.org) in `C:\vitasdk` (or `$VITASDK`) with `vita2d` and `zlib`, plus CMake, Ninja and Python 3 with Pillow:

   ```sh
   python3 vita/make_assets.py artwork/banner.png vita   # LiveArea art, once
   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vitasdk/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build                                   # -> build/gow_betrayal.vpk
   ```

2. With the console mounted as a drive, copy the banks, the audio cache and the VPK in one step:

   ```sh
   python3 vita/deploy.py /path/to/God-of-War-Betrayal_K800i_v1420.jar /media/PSVITA
   ```

3. Install `gow_betrayal.vpk` with VitaShell and launch the bubble from LiveArea.

The banks are not shipped in the VPK; without them the app opens an error screen naming `ux0:data/gow-betrayal/data`. Controls, the port-specific options rows and troubleshooting are documented in [`vita/README_VITA.md`](vita/README_VITA.md). Runtime diagnostics append to `ux0:data/gow-betrayal/gow-vita.log`.

## Controls

| PSP | Action |
|---|---|
| D-pad / analog stick | Move; navigate menus |
| Cross | Jump; confirm in menus; skip intro |
| Square | Attack |
| Circle | Block / interact; back in menus |
| Triangle / R | Cycle weapon or magic |
| L / Select | Upgrade screen |
| Start | Pause |

## Display and performance

Rendering is **480×272, 1:1**: sprites, tiles and fonts are not resampled. The camera and UI are adapted to the shorter screen instead of stretching the phone's 240×320 canvas. The default pacing keeps the original 25 logic steps per second with up to 25 drawn frames; a 30 fps interpolated mode is available in the in-game options.

The build sets `MEMSIZE=0`, requests a 16 MiB heap and uses two 8888 framebuffers of about 1.06 MiB total from the 2 MiB VRAM. It does not request the 64 MB mode and does not change the CPU clock. The configuration targets the 32 MB **PSP-1000**; the 25 fps mode is recommended there.

Frame times have only been measured in PPSSPP. Speed and stability on a physical PSP-1000 have not been validated yet.

## Audio

All 11 original MIDI tracks are pre-rendered with [TimGM6mb](https://packages.debian.org/timgm6mb-soundfont) into a compact `data/timgm.dat` (~386 KiB), so neither FluidSynth nor a SoundFont is needed on the console. If that file is missing, a simpler built-in synthesizer takes over. TimGM6mb is not identical to the K800i's own synth.

Only the main-menu theme loops; levels have sound effects but no background music, which matches the original K800i build.

To rebuild the audio cache you need FluidSynth, FFmpeg and `TimGM6mb.sf2`:

```sh
python3 psp/bake_timgm.py God-of-War-Betrayal_K800i_v1420.jar /path/to/TimGM6mb.sf2 PSP/GAME/GOWBETRAYAL
```

## Languages

Russian and Ukrainian were added, with Cyrillic glyphs drawn to match the original font's height, serifs and palette. Their menus, dialogue and hints are translated; the original logo and credits are preserved. On the language screen scroll down with the D-pad and confirm with Cross. The choice is saved.

Help text and in-game control hints use PSP button names (□ × ○ △, D-pad, L/R) in **all eight languages**. Outside those instructions the six original language tables are unchanged. Translation sources live in `psp/locales.tsv` and `psp/controls_psp.tsv`; both are compiled into C++ by `psp/gen_locales.py`.

## Build

Requires the pspdev toolchain (psp-gcc 15.x), `make`, `python3` and `zlib`. Build from WSL or Linux:

```sh
export PSPDEV="$HOME/pspdev"          # adjust if installed elsewhere
export PATH="$PSPDEV/bin:$PATH"
make build
```

The top-level `Makefile` wraps `psp/Makefile`; the output is `psp/EBOOT.PBP`. Optional XMB artwork can be dropped into `artwork/` (`icon0.png`, `ICON1.PMF`, `pic1.png`, `SND0.at3`) before building — otherwise those PBP members are left empty. The checked-in `dist/EBOOT.PBP` includes the artwork supplied for this build.

Host-side checks (no PSP toolchain needed):

```sh
make test
```

They verify clipped/mirrored blits, the 480×272 framebuffer layout, every Cyrillic glyph and all eight control-guide tables.

An automated PPSSPP smoke test runs headless under Xvfb and does not open a window on the desktop:

```sh
xvfb-run -a python3 psp/smoke_ppsspp.py \
  /path/to/PPSSPPSDL dist test-output/run --layout portable --stage story --language 6
```

`--language` selects 0–7 (six original languages, Russian, Ukrainian); `--stage help` opens the help panel; `--skip-intro` skips `INTRO.BIN`.

The game writes `gow-psp.log` and, on Java-style exceptions, `exceptions.log` next to `EBOOT.PBP`.

## License and attribution

See [NOTICE.md](NOTICE.md). No original game data is committed here. The upstream PC port does not declare a license in its repository; confirm the terms you want to publish under before making the fork public. Guidance for PSP-specific work came from [SandstoneDev/psp-developer](https://github.com/SandstoneDev/psp-developer).

This project is not affiliated with, endorsed by, or supported by Sony Interactive Entertainment, Santa Monica Studio, Glu Mobile, Metaflow or the game's original developers and publishers. *God of War* is a trademark of Sony Interactive Entertainment LLC.
