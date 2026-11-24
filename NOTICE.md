# Notices and attribution

This repository is a native PSP port of *God of War: Betrayal*, the 2007 J2ME/mobile Java game by Glu Mobile, built by Metaflow. The game logic comes from the from-scratch behavioural re-implementation in [samioan/gow-betrayal-remastered](https://github.com/samioan/gow-betrayal-remastered) (`bad458d`), which was written by reading the original game's decompiled bytecode. The PSP-specific layer in `psp/` replaces that project's Win32 shell.

The same tree additionally carries a **PlayStation Vita** layer in `vita/` (vita2d renderer, `sceAudioOut` audio, LiveArea packaging and the deploy script), written for this fork.

It is not affiliated with, endorsed by, or supported by Sony Interactive Entertainment, Santa Monica Studio, Glu Mobile, Metaflow, or the game's original developers and publishers.

## What is not included

**The game.** No script, image, sound, string table or byte of the original `God-of-War-Betrayal_*.jar` release is committed here, and none is distributed with any release. `src/gen/` is generated C++ derived from decompiler output; it cannot run without the original resource banks, which you supply yourself with `psp/prepare_data.py`. `data/`, `timgm.dat`, `INTRO.BIN`, `artwork/` and `saves/` are gitignored.

The translations in `psp/locales.tsv` and `psp/controls_psp.tsv` are text written for this port; the underlying string tables remain in the user's JAR.

## Third-party components

| Component | License | Where |
|---|---|---|
| `stb_image` | public domain / MIT | `third_party/stb/` |
| `javalang` (development tool only, not shipped) | MIT | used by the upstream translation tooling |
| Vineflower (development tool only, not shipped) | Apache-2.0 | upstream decompiler |
| TimGM6mb.sf2 (used offline to render `timgm.dat`) | GPL-2 | Tim Brechbill, David Bolton; <https://packages.debian.org/timgm6mb-soundfont> |
| PSPSDK / pspdev toolchain | BSD | <https://github.com/pspdev/pspsdk> |
| VitaSDK (headers, toolchain, vdpm) | see the upstream repositories; nothing is vendored here | <https://github.com/vitasdk> |
| `vita2d` (PS Vita rendering) | see its upstream repository; only the static library is linked | Vita build |
| zlib | zlib | linked for intro decompression |

`javalang`, Vineflower, FluidSynth, FFmpeg and PPSSPP are build- or test-time tools; none is embedded in `EBOOT.PBP` or in `gow_betrayal.vpk`. VitaSDK, `vita2d` and the Sony system libraries are linked from the toolchain at build time and are not redistributed by this repository.

## Artwork and intro

`dist/EBOOT.PBP` was built with optional XMB menu artwork (`icon0.png`, `ICON1.PMF`, `pic1.png`, `SND0.at3`) supplied by the owner of this repository. It is not committed; place your own files in `artwork/` before rebuilding. `INTRO.BIN` is likewise a local, optional PINT video file. The PS Vita LiveArea art (`icon0.png`, `bg.png`, `startup.png`) is generated into the gitignored `vita/sce_sys/` by `vita/make_assets.py` from artwork supplied by the owner. The "God of War" wordmark and Kratos belong to their owners.

## Upstream license note

The upstream PC port does not declare a license in its repository. This fork grants no new rights over the upstream code or the original game. Replace this note with your chosen license for the PSP-specific additions before publishing.

## Guidance

PSP hardware and packaging guidance came from [SandstoneDev/psp-developer](https://github.com/SandstoneDev/psp-developer) by SandstoneDev (<https://discord.gg/wV8x46ZTDu>), distributed under the PS2RC License 1.2. No material from that repository is copied into this one.

## Trademarks

"God of War" and "God of War: Betrayal" are trademarks of Sony Interactive Entertainment LLC. All names are used here for identification and documentation only.
