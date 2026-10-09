"""Camera and lens description, with Canon APS-C defaults.

Defaults describe a 24 MP Canon APS-C body (EOS 2000D / 250D / 800D / 80D
class): 22.3 x 14.9 mm, 6000 x 4000 px, 3.72 um pixels, crop factor 1.6.
Noise figures are rough assumptions for that class (see `exposure.py`); put
your own body's numbers in when you have them.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, replace

from .constants import ARCSEC_PER_RAD


@dataclass(frozen=True)
class Camera:
    name: str = "Canon APS-C 24 MP (assumed)"
    sensor_w_mm: float = 22.3
    sensor_h_mm: float = 14.9
    pixels_w: int = 6000
    pixels_h: int = 4000
    crop_factor: float = 1.6

    @property
    def pixel_um(self) -> float:
        return self.sensor_w_mm / self.pixels_w * 1000.0

    @property
    def half_diagonal_mm(self) -> float:
        return 0.5 * math.hypot(self.sensor_w_mm, self.sensor_h_mm)

    def with_(self, **kw) -> Camera:
        return replace(self, **kw)


CANON_APSC_24MP = Camera()
CANON_APSC_18MP = Camera(
    name="Canon APS-C 18 MP (assumed)",
    sensor_w_mm=22.3,
    sensor_h_mm=14.9,
    pixels_w=5184,
    pixels_h=3456,
)


@dataclass(frozen=True)
class Lens:
    focal_mm: float = 200.0
    f_number: float = 5.6
    name: str = ""

    @property
    def aperture_mm(self) -> float:
        return self.focal_mm / self.f_number


def plate_scale_arcsec_per_px(camera: Camera, focal_mm: float) -> float:
    return camera.pixel_um * 1e-3 / focal_mm * ARCSEC_PER_RAD


def field_of_view_deg(camera: Camera, focal_mm: float) -> tuple[float, float]:
    w = 2 * math.degrees(math.atan(camera.sensor_w_mm / 2 / focal_mm))
    h = 2 * math.degrees(math.atan(camera.sensor_h_mm / 2 / focal_mm))
    return w, h


def half_diagonal_rad(camera: Camera, focal_mm: float) -> float:
    return math.atan(camera.half_diagonal_mm / focal_mm)
