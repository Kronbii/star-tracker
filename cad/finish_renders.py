"""Post-process the OpenSCAD renders for figures/.

* Swaps OpenSCAD's render background for the figures' surface colour.
* Burns a label into every render: these are renders of a design that has not
  been built, and must never be mistaken for photographs of a prototype.
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.image as mpimg  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "figures"
SRC_BG = np.array([0x1D, 0x1F, 0x21]) / 255.0  # OpenSCAD "Tomorrow Night" background
DST_BG = np.array([0x12, 0x16, 0x14]) / 255.0  # #121614
LABEL = "CAD RENDER  |  unbuilt design, not a photograph"


def finish(src: Path, dst: Path, sub: str) -> None:
    img = mpimg.imread(src)[..., :3].astype(float)
    d = np.abs(img - SRC_BG).max(axis=-1)
    w = np.clip(1.0 - d / 0.06, 0.0, 1.0)[..., None]  # blend anti-aliased edges too
    img = np.clip(img * (1 - w) + (img - SRC_BG + DST_BG) * w, 0, 1)
    h, wd = img.shape[:2]
    dpi = 100
    fig = plt.figure(figsize=(wd / dpi, h / dpi), dpi=dpi)
    ax = fig.add_axes((0, 0, 1, 1))
    ax.imshow(img, interpolation="none")
    ax.set_axis_off()
    size = max(12, wd / 75)
    box = {
        "boxstyle": "round,pad=0.45",
        "facecolor": "#191e1b",
        "edgecolor": "#b7d3a8",
        "linewidth": 1.2,
    }
    fig.text(0.018, 0.965, LABEL, color="#eef1ec", fontsize=size, fontweight="bold",
             va="top", ha="left", bbox=box)
    sub_box = {"boxstyle": "round,pad=0.35", "facecolor": "#121614", "edgecolor": "none",
               "alpha": 0.85}
    fig.text(0.018, 0.03, sub, color="#b5b8b4", fontsize=size * 0.75, va="bottom", ha="left",
             bbox=sub_box)
    fig.savefig(dst, dpi=dpi, facecolor="#121614")
    plt.close(fig)
    print("wrote", dst)


JOBS = (
    ("hero_raw.png", "hero_render.png",
     "OpenSCAD model, board angle 18 deg, camera omitted. Design only: not built, not sky-tested."),
    ("in_use_raw.png", "cad_in_use.png",
     "OpenSCAD model with stand-in camera and ball head (grey blocks). Design only: not built."),
    ("contact_raw.png", "cad_contact_39deg.png",
     "Clearance check at the end of travel (39 deg), viewed along the hinge axis. Design only."),
)

for raw, target, sub in JOBS:
    p = HERE / "build" / raw
    if p.is_file():
        finish(p, OUT / target, sub)
