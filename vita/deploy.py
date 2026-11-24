#!/usr/bin/env python3
"""Deploy the Vita port to a console mounted as a drive (or any folder root).

Steps:
  1. extract the 31 RP banks from the K800i 1.4.20 JAR into a temp folder
     (never inside the repository: game data stays untracked);
  2. copy them to <root>/data/gow-betrayal/data and create the saves folder;
  3. copy build/gow_betrayal.vpk to <root>/ for a VitaShell install.

Usage:
    python vita/deploy.py [jar] [root] [--no-vpk]
Defaults: $GOW_JAR or ./God-of-War-Betrayal_K800i_v1420.jar, root = M:\
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BANKS = tuple(f"RP{i}" for i in range(1, 34) if i not in (11, 31))
DEFAULT_JAR = Path(os.environ.get("GOW_JAR", "God-of-War-Betrayal_K800i_v1420.jar"))
DEFAULT_ROOT = Path("M:/")
VPK = REPO / "build" / "gow_betrayal.vpk"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("jar", nargs="?", type=Path, default=DEFAULT_JAR)
    p.add_argument("root", nargs="?", type=Path, default=DEFAULT_ROOT)
    p.add_argument("--no-vpk", action="store_true", help="skip copying the VPK")
    a = p.parse_args()

    if not a.jar.is_file():
        sys.exit(f"jar not found: {a.jar}\n"
                 f"pass the path: python vita/deploy.py <jar> [root]  (or set GOW_JAR)")
    if not a.root.is_dir():
        sys.exit(f"console root not found: {a.root} (is the USB mount connected?)")

    home = a.root / "data" / "gow-betrayal"
    target = home / "data"

    # 1. banks -> temp staging (outside the repo, so nothing copyrighted is tracked)
    with tempfile.TemporaryDirectory(prefix="gow_vita_") as tmp:
        stage = Path(tmp)
        subprocess.run(
            [sys.executable, str(REPO / "psp" / "prepare_data.py"), str(a.jar), str(stage)],
            check=True,
        )
        # 2. staging -> console
        target.mkdir(parents=True, exist_ok=True)
        (home / "saves").mkdir(parents=True, exist_ok=True)
        copied = 0
        for name in BANKS:
            src = stage / "data" / name
            if not src.is_file():
                sys.exit(f"prepare_data.py did not produce {name}")
            shutil.copy2(src, target / name)
            copied += 1
        print(f"{copied} banks -> {target}")

        # 2b. optional baked MIDI cache (psp/bake_timgm.py): real instrument
        # samples instead of the built-in fallback synthesiser.
        cache = REPO / "data" / "timgm.dat"
        if cache.is_file():
            shutil.copy2(cache, target / "timgm.dat")
            print(f"timgm.dat -> {target / 'timgm.dat'} ({cache.stat().st_size} bytes)")
        else:
            print("timgm.dat not baked yet (psp/bake_timgm.py): fallback synth will be used")

    # 3. VPK -> console root
    if not a.no_vpk:
        if not VPK.is_file():
            sys.exit(f"VPK not built: {VPK} (run cmake --build build)")
        dst = a.root / VPK.name
        shutil.copy2(VPK, dst)
        print(f"{VPK.name} -> {dst} ({dst.stat().st_size} bytes)")

    # 4. verify what actually landed on the console
    missing = [n for n in BANKS if not (target / n).is_file()]
    if missing:
        sys.exit(f"verification failed, missing on console: {', '.join(missing)}")
    total = sum((target / n).stat().st_size for n in BANKS)
    print(f"verified: {len(BANKS)} banks, {total} bytes at {target}")
    print("Launch from LiveArea; log: ux0:data/gow-betrayal/gow-vita.log")
    return 0


if __name__ == "__main__":
    sys.exit(main())
