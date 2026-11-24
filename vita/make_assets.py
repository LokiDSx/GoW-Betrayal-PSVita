#!/usr/bin/env python3
"""Generate the LiveArea artwork the VPK ships with.

Sources: artwork/ in this repository (banner.png is the PC port's key art).
Output: vita/sce_sys/{icon0.png,livearea/contents/{bg.png,startup.png,template.xml}}
"""
import sys
from pathlib import Path

from PIL import Image

ICON = (128, 128)
BG = (840, 500)
STARTUP = (280, 158)

TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
\t<livearea-background>
\t\t<image>bg.png</image>
\t</livearea-background>

\t<gate>
\t\t<startup-image>startup.png</startup-image>
\t</gate>
</livearea>
"""


def cover(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Resize to fill size, cropping the overflow evenly (keeps the subject centred)."""
    tw, th = size
    scale = max(tw / image.width, th / image.height)
    resized = image.resize((round(image.width * scale), round(image.height * scale)), Image.LANCZOS)
    left = (resized.width - tw) // 2
    top = (resized.height - th) // 2
    return resized.crop((left, top, left + tw, top + th))


def icon(image: Image.Image) -> Image.Image:
    # A square over Kratos' head and the wordmark, anchored to the bottom of the art.
    side = int(image.height * 0.77)
    left = (image.width - side) // 2
    top = image.height - side
    return image.crop((left, top, left + side, top + side)).resize(ICON, Image.LANCZOS)


def main(banner: Path, out: Path) -> None:
    art = Image.open(banner).convert("RGB")
    livearea = out / "sce_sys" / "livearea" / "contents"
    livearea.mkdir(parents=True, exist_ok=True)
    (out / "sce_sys").mkdir(parents=True, exist_ok=True)

    icon(art).save(out / "sce_sys" / "icon0.png")
    cover(art, BG).save(livearea / "bg.png")
    cover(art, STARTUP).save(livearea / "startup.png")
    (livearea / "template.xml").write_text(TEMPLATE, encoding="utf-8")
    print(f"LiveArea artwork -> {out / 'sce_sys'}")


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
