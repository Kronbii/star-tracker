"""Swap OpenSCAD's render background for the figures' surface colour and copy
the renders into figures/ (hero image and the in-use view)."""

from pathlib import Path

import matplotlib.image as mpimg
import numpy as np

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "figures"
SRC_BG = np.array([0x1D, 0x1F, 0x21]) / 255.0  # "Tomorrow Night" background
DST_BG = np.array([0x12, 0x16, 0x14]) / 255.0  # #121614


def recolour(src: Path, dst: Path) -> None:
    img = mpimg.imread(src)[..., :3].astype(float)
    d = np.abs(img - SRC_BG).max(axis=-1)
    # Exact background -> new colour; near-background (anti-aliased edges) blended.
    w = np.clip(1.0 - d / 0.06, 0.0, 1.0)[..., None]
    out = img * (1 - w) + (img - SRC_BG + DST_BG) * w
    mpimg.imsave(dst, np.clip(out, 0, 1))
    print("wrote", dst)


for name, target in (("hero_raw.png", "hero_render.png"), ("in_use_raw.png", "cad_in_use.png"),
                     ("contact_raw.png", "cad_contact_39deg.png")):
    p = HERE / "build" / name
    if p.is_file():
        recolour(p, OUT / target)
