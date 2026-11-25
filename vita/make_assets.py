#!/usr/bin/env python3
"""Generate the LiveArea artwork the VPK ships with.

Usage:
    python3 vita/make_assets.py <icon> <startup> <bg> <out>

The three sources are supplied locally and are never committed (see NOTICE.md):
  icon     key art for the 128x128 bubble icon
  startup  wordmark for the 280x158 gate image; fitted, never stretched,
           transparent background preserved
  bg       the 840x500 LiveArea background

Output: <out>/sce_sys/{icon0.png,livearea/contents/{bg.png,startup.png,template.xml}}
"""
import sys
from pathlib import Path

from PIL import Image, ImageChops

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


def fit(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Resize to fit inside size without distortion, centred on a transparent canvas."""
    tw, th = size
    scale = min(tw / image.width, th / image.height)
    resized = image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))),
                           Image.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(resized, ((tw - resized.width) // 2, (th - resized.height) // 2))
    return canvas


def save(image: Image.Image, path: Path) -> None:
    # The LiveArea installer rejects true-colour PNGs with 0x8010113D; every
    # image in a shipping VPK (and the GravityDefied reference) is 8-bit palette.
    image.convert("P", palette=Image.Palette.ADAPTIVE, colors=256).save(path, optimize=True)


def save_transparent(image: Image.Image, path: Path) -> None:
    """8-bit palette PNG with a single transparent index.

    Same format as the reference VPK's startup.png (palette + one tRNS index);
    true-colour PNGs fail installation with 0x8010113D.
    """
    # Keep only the clearly-opaque pixels: the gate image sits over bg.png, and
    # a single tRNS index is all the installer is known to accept.
    alpha = image.getchannel("A").point(lambda v: 255 if v >= 128 else 0)
    # A sentinel green cannot occur in the artwork, so its palette entry stays
    # easy to find even after adaptive quantisation.
    base = Image.new("RGB", image.size, (0, 255, 0))
    base.paste(image.convert("RGB"), (0, 0), alpha)
    palette = base.convert("P", palette=Image.Palette.ADAPTIVE, colors=256)
    palette.info["transparency"] = palette.getpixel((0, 0))  # canvas corner is empty
    palette.save(path, optimize=True)


def warm_bbox(image: Image.Image) -> tuple[int, int, int, int] | None:
    """Bounding box of the bright, warm pixels — a gold wordmark on dark art."""
    r, g, b = image.convert("RGB").split()

    def thresh(channel: Image.Image, low: int) -> Image.Image:
        return channel.point(lambda v, low=low: 255 if v > low else 0)

    mask = ImageChops.multiply(thresh(r, 140), thresh(g, 90))
    mask = ImageChops.multiply(mask, thresh(ImageChops.subtract(r, b), 60))
    return mask.getbbox()


def icon(image: Image.Image) -> Image.Image:
    """Square icon: crop to the wordmark's horizontal extent, fit without cropping.

    A square crop of a 4:3 cover cuts the wordmark off at both ends; fitting the
    full width keeps "GOD OF WAR / BETRAYAL" readable and letterboxes the rest.
    """
    box = warm_bbox(image)
    if box is None:
        crop = image
    else:
        pad = round(image.width * 0.03)
        left = max(0, box[0] - pad)
        right = min(image.width, box[2] + pad)
        crop = image.crop((left, 0, right, image.height))
    tw, th = ICON
    scale = min(tw / crop.width, th / crop.height)
    resized = crop.resize((max(1, round(crop.width * scale)), max(1, round(crop.height * scale))),
                          Image.LANCZOS)
    canvas = Image.new("RGB", ICON, (0, 0, 0))
    canvas.paste(resized, ((tw - resized.width) // 2, (th - resized.height) // 2))
    return canvas


def main(icon_src: Path, startup_src: Path, bg_src: Path, out: Path) -> None:
    art = Image.open(icon_src).convert("RGB")
    logo = Image.open(startup_src).convert("RGBA")
    background = Image.open(bg_src).convert("RGB")

    livearea = out / "sce_sys" / "livearea" / "contents"
    livearea.mkdir(parents=True, exist_ok=True)
    (out / "sce_sys").mkdir(parents=True, exist_ok=True)

    save(icon(art), out / "sce_sys" / "icon0.png")
    save(cover(background, BG), livearea / "bg.png")
    save_transparent(fit(logo, STARTUP), livearea / "startup.png")
    (livearea / "template.xml").write_text(TEMPLATE, encoding="utf-8")
    print(f"LiveArea artwork -> {out / 'sce_sys'}")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__.strip())
    main(*(Path(a) for a in sys.argv[1:]))
