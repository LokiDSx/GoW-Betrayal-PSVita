# God of War: Betrayal — PS Vita port

A native PlayStation Vita port of *God of War: Betrayal*: 640x360 internal
framebuffer scaled 1.5x to the 960x544 panel, vita2d rendering, `sceAudioOut`
BGM-port audio at the synth's native 44100 Hz, RU/UK localisation included.
It builds on the upstream portable code by
[Akrgood/gow-betrayal-psp](https://github.com/Akrgood/gow-betrayal-psp).

## Build

Requires [vitasdk](https://vitasdk.org) in `C:\vitasdk` (or `$VITASDK`) with
`vita2d`, `zlib` (vdpm), plus CMake, Ninja and Python 3 with Pillow:

```
python vita\make_assets.py <icon> <startup> <bg> vita   # LiveArea art (once)
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vitasdk/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build\gow_betrayal.vpk` (~0.7 MB, no game data inside).

## Install

1. Copy `build\gow_betrayal.vpk` to the Vita and open it with VitaShell.
2. The LiveArea bubble shows the key art; launch it.

## Game data (required)

The 31 RP resource banks are **not** shipped in the VPK. Prepare them from your
own Sony Ericsson K800i v1.4.20 JAR on the PC:

```
python psp\prepare_data.py God-of-War-Betrayal_K800i_v1420.jar gow_vita_data
```

Then copy the resulting folder to the console:

```
ux0:data\gow-betrayal\data\        <- RP1..RP34 (31 files, from prepare_data.py)
ux0:data\gow-betrayal\saves\       <- created automatically
ux0:data\gow-betrayal\INTRO.BIN    <- optional PINT intro video
ux0:data\gow-betrayal\gow-vita.log <- runtime log
```

With the console mounted as a drive the same thing is one command — it stages
the banks, ships `data\timgm.dat` when it has been baked, and copies the VPK:

```
python vita\deploy.py God-of-War-Betrayal_K800i_v1420.jar <console-root>
```

Without the banks the app opens an error screen naming this exact path.

## Audio cache (recommended)

`data/timgm.dat` (~386 KiB) holds all 11 original MIDI tracks pre-rendered with
the [TimGM6mb](https://packages.debian.org/timgm6mb-soundfont) soundfont, so the
console needs neither FluidSynth nor a SoundFont. Without that file the port
falls back to a built-in synthesizer, which is audibly rougher.

Bake it once on the PC (FluidSynth + FFmpeg + `TimGM6mb.sf2`), then let the
deploy script carry it to the console:

```
python psp\bake_timgm.py God-of-War-Betrayal_K800i_v1420.jar \path\to\TimGM6mb.sf2 <staging-folder>
copy <staging-folder>\data\timgm.dat data\timgm.dat
python vita\deploy.py God-of-War-Betrayal_K800i_v1420.jar <console-root>
```

`vita/deploy.py` copies `data/timgm.dat` automatically whenever it exists.

## Controls

| Vita | In game | Menu |
| --- | --- | --- |
| D-pad / analog | move | navigate |
| Cross | jump, **OK** in QTE | confirm / start intro |
| Circle | — | back / down |
| Square | attack, **OK** in QTE | - |
| Triangle or R1 | switch weapon | — |
| L1 or Select | upgrade and magic menu | — |
| Start | pause | pause / quit intro |

## Options menu rows (port-specific)

- **Screen: 640x360** — fixed frame size (16:9, 1.5x panel scale).
- **Pixel scale: sharp / smooth** — point or bilinear panel scaling.
- **FPS: 25 / 30** — logic rate (the original targets 25).

## Troubleshooting

- Black screen / immediate exit: read `ux0:data\gow-betrayal\gow-vita.log`
  (every stage logs a line: boot, intro, audio, state transitions).
- Error screen text: it names the failing path or exception; START quits.
- No music: check that `data\timgm.dat` was copied next to the banks
  (optional — without it the port falls back to the software MIDI synth).
