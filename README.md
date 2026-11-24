# God of War: Betrayal — PS Vita port

A native PlayStation Vita port of the 2007 J2ME game *God of War: Betrayal*. It runs the translated C++ game logic — no Java emulator is involved — renders with vita2d and mixes audio through `sceAudioOut`.

It builds on the PSP port by [Akrgood/gow-betrayal-psp](https://github.com/Akrgood/gow-betrayal-psp) and on the translated game logic from [samioan/gow-betrayal-remastered](https://github.com/samioan/gow-betrayal-remastered) (commit `bad458d`), which was written by reading the original game's decompiled bytecode.

The game data is **not** included. You supply your own copy of the Sony Ericsson K800i JAR, MIDlet version **1.4.20**.

## Contents

| Path | What it holds |
|---|---|
| `src/` | Shared game core, software renderer and Java runtime shims |
| `src/gen/` | Generated C++ translated from the decompiled Java (do not edit) |
| `vita/` | Entry point, vita2d renderer, `sceAudioOut` audio, LiveArea VPK and deploy tools |
| `tools/gow_psp_patches.py` | Build-time adaptation of the generated game code |
| `third_party/stb/` | `stb_image` (public domain / MIT) |

Generated files are produced from `names.map` and the patch scripts; see `tools/gow_psp_patches.py`.

## Build

Requires [VitaSDK](https://vitasdk.org) in `C:\vitasdk` (or `$VITASDK`) with `vita2d` and `zlib`, plus CMake, Ninja and Python 3 with Pillow:

```sh
python3 vita/make_assets.py artwork/banner.png vita   # LiveArea art, once
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vitasdk/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build                                   # -> build/gow_betrayal.vpk
```

The VPK carries the LiveArea artwork only; no game data is committed or distributed.

## Install

1. With the console mounted as a drive, copy the banks, the audio cache and the VPK in one step:

   ```sh
   python3 vita/deploy.py God-of-War-Betrayal_K800i_v1420.jar <console-root>
   ```

   Without a mount, stage the banks yourself with `python3 psp/prepare_data.py <jar> <folder>` and copy `build/gow_betrayal.vpk` over for VitaShell.

2. Install the VPK with VitaShell and launch the bubble from LiveArea.

Layout on the console:

```
ux0:data/gow-betrayal/data/        <- RP1..RP34 (31 files) + timgm.dat
ux0:data/gow-betrayal/saves/       <- created automatically
ux0:data/gow-betrayal/INTRO.BIN    <- optional PINT intro video
ux0:data/gow-betrayal/gow-vita.log <- runtime log
```

Without the banks the app opens an error screen naming this exact path. Controls, the port-specific options rows and troubleshooting are documented in [`vita/README_VITA.md`](vita/README_VITA.md).

## Display and performance

The game renders **640×360** and is scaled 1.5× to the 960×544 panel; point (sharp) or bilinear (smooth) filtering can be switched in the in-game options. The default pacing keeps the original **25 logic steps per second with up to 25 drawn frames**; a 30 fps mode is available in the same menu (`FPS: 25 / 30`).

## Audio

All 11 original MIDI tracks are pre-rendered with [TimGM6mb](https://packages.debian.org/timgm6mb-soundfont) into a compact `data/timgm.dat` (~386 KiB), so neither FluidSynth nor a SoundFont is needed on the console. If that file is missing, a simpler built-in synthesizer takes over. TimGM6mb is not identical to the K800i's own synth.

Only the main-menu theme loops; levels have sound effects but no background music, which matches the original K800i build.

To rebuild the audio cache you need FluidSynth, FFmpeg and `TimGM6mb.sf2`:

```sh
python3 psp/bake_timgm.py God-of-War-Betrayal_K800i_v1420.jar /path/to/TimGM6mb.sf2 <staging-folder>
cp <staging-folder>/data/timgm.dat data/timgm.dat     # deploy.py ships it automatically
```

## Languages

Russian and Ukrainian were added, with Cyrillic glyphs drawn to match the original font's height, serifs and palette. Their menus, dialogue and hints are translated; the original logo and credits are preserved. On the language screen scroll down with the D-pad and confirm with Cross. The choice is saved.

Control hints keep the original face-button names (□ × ○ △, D-pad, L/R) in **all eight languages**. Outside those instructions the six original language tables are unchanged. Translation sources live in `psp/locales.tsv` and `psp/controls_psp.tsv`; both are compiled into C++ by `psp/gen_locales.py`.

## License and attribution

See [NOTICE.md](NOTICE.md). No original game data is committed here. This fork's own additions are released under the [MIT license](LICENSE). The upstream PC port does not declare a license in its repository, so this license covers only the changes made for the port in this repository. Guidance for the handheld-specific work came from [SandstoneDev/psp-developer](https://github.com/SandstoneDev/psp-developer).

This project is not affiliated with, endorsed by, or supported by Sony Interactive Entertainment, Santa Monica Studio, Glu Mobile, Metaflow or the game's original developers and publishers. *God of War* is a trademark of Sony Interactive Entertainment LLC.
