"""Exposure planning: untracked rules of thumb, sky-limited sub length, SNR.

Untracked rules (for comparison with tracking):

* 500 rule: t = 500 / (f x crop);  300 rule: t = 300 / (f x crop).
* NPF rule (F. Michaud, Societe d'astronomie populaire du Havre), simple form
  t = (35 N + 30 p) / f, and the detailed form
  t = (16.856 N + 0.0997 f + 13.713 p) / (f cos(dec)),
  with N the f-number, p the pixel pitch (um), f the focal length (mm).

These rules use looser sharpness criteria than `errors.untracked_limit`
(which allows max_blur_px of trail); the figures show all of them.

Sky-limited sub length (tracked):

* sky rate B = Phi0 10^(-0.4 SQM) (pi/4) (p / N)^2 eta   [e-/s/px], with the
  V-band zero point Phi0 = 8.8e5 photons/s/cm^2 for mag 0 per arcsec^2 and a
  lumped throughput eta (lens x Bayer x QE) of 0.30 - a rough assumption,
  good to perhaps +-50 %.
* swamp: subs long enough that sky noise dominates read noise,
  t >= k RN^2 / B with k = 5 (read noise then adds <= 10 % to the noise).
* clip: keep the sky below 1/3 of the clipping level at the chosen ISO.

Sensor numbers are assumptions for a 24 MP Canon APS-C body; replace them
with measured data (e.g. photonstophotos.net) for a specific camera.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

from .constants import ARCSEC_PER_RAD
from .optics import Camera, Lens

PHI0_PH_S_CM2 = 8.8e5  # photons / s / cm^2 at V = 0 (per arcsec^2 for surface brightness)


def rule_500(focal_mm, crop=1.6):
    return 500.0 / (np.asarray(focal_mm, dtype=float) * crop)


def rule_300(focal_mm, crop=1.6):
    return 300.0 / (np.asarray(focal_mm, dtype=float) * crop)


def npf_simple(focal_mm, f_number, pixel_um):
    return (35.0 * f_number + 30.0 * pixel_um) / np.asarray(focal_mm, dtype=float)


def npf_detailed(focal_mm, f_number, pixel_um, dec_deg=0.0):
    f = np.asarray(focal_mm, dtype=float)
    return (16.856 * f_number + 0.0997 * f + 13.713 * pixel_um) / (
        f * math.cos(math.radians(dec_deg))
    )


@dataclass(frozen=True)
class SensorNoise:
    """Assumed noise model for a 24 MP Canon APS-C sensor (off-sensor ADC
    generation, e.g. EOS 2000D / 800D class). Values are approximate."""

    read_noise_e: dict[int, float] = field(
        default_factory=lambda: {100: 8.0, 200: 5.5, 400: 4.0, 800: 3.0, 1600: 2.5, 3200: 2.2}
    )
    clip_e_iso100: float = 21000.0  # electrons at raw clipping, ISO 100
    dark_e_s: float = 0.05  # dark current near 15 C, e-/s/px
    throughput: float = 0.30  # lens x Bayer filter x QE, lumped

    def rn(self, iso: int) -> float:
        isos = sorted(self.read_noise_e)
        if iso in self.read_noise_e:
            return self.read_noise_e[iso]
        x = np.log(isos)
        y = np.log([self.read_noise_e[i] for i in isos])
        return float(np.exp(np.interp(math.log(iso), x, y)))

    def clip_e(self, iso: int) -> float:
        return self.clip_e_iso100 * 100.0 / iso


def surface_rate_e_s(mag_arcsec2: float, lens: Lens, camera: Camera, noise: SensorNoise) -> float:
    """Electrons/s/pixel from a surface of the given brightness (mag/arcsec^2)."""
    d_cm = lens.aperture_mm / 10.0
    area_cm2 = math.pi * (d_cm / 2.0) ** 2
    scale = camera.pixel_um * 1e-3 / lens.focal_mm * ARCSEC_PER_RAD
    return PHI0_PH_S_CM2 * 10 ** (-0.4 * mag_arcsec2) * area_cm2 * scale**2 * noise.throughput


def swamp_time(rn_e: float, sky_e_s: float, k: float = 5.0) -> float:
    return k * rn_e**2 / sky_e_s


def clip_time(clip_e: float, sky_e_s: float, fraction: float = 1.0 / 3.0) -> float:
    return fraction * clip_e / sky_e_s


def stack_snr(
    signal_e_s: float, sky_e_s: float, sub_s: float, n_subs: int, rn_e: float, dark_e_s: float
) -> float:
    """Per-pixel SNR of a mean stack (dark subtracted; calibration noise ignored)."""
    t = sub_s * n_subs
    s = signal_e_s * t
    var = (signal_e_s + sky_e_s + dark_e_s) * t + n_subs * rn_e**2
    return s / math.sqrt(var)


@dataclass
class SubPlan:
    iso: int
    sub_s: float
    n_subs: int
    total_s: float
    sky_e_s: float
    tracking_limit_s: float
    swamp_s: float
    clip_s: float
    read_noise_limited: bool
    notes: list[str]


def plan_subs(
    tracking_limit_s: float,
    sqm: float,
    lens: Lens,
    camera: Camera,
    noise: SensorNoise | None = None,
    window_s: float = 3600.0,
    gap_s: float = 3.0,
    max_total_s: float | None = None,
    isos=(400, 800, 1600, 3200),
    round_to_s: float = 5.0,
) -> SubPlan:
    """Choose ISO and sub length, then fill the imaging window with subs.

    Sub length = min(tracking limit, sky clip time), rounded down to
    `round_to_s`; ISO = the lowest candidate whose read noise is swamped by the
    sky within that sub (keeps dynamic range), else the highest.
    """
    noise = noise or SensorNoise()
    sky = surface_rate_e_s(sqm, lens, camera, noise)
    notes = []
    chosen = None
    for iso in isos:
        sub = min(tracking_limit_s, clip_time(noise.clip_e(iso), sky))
        if sub >= round_to_s:
            sub = math.floor(sub / round_to_s) * round_to_s
        if swamp_time(noise.rn(iso), sky) <= sub:
            chosen = (iso, sub)
            break
    if chosen is None:
        # Nothing swamps read noise: use ISO 1600, where these Canon sensors'
        # read noise has mostly flattened out.
        iso = 1600 if 1600 in isos else isos[-1]
        sub = min(tracking_limit_s, clip_time(noise.clip_e(iso), sky))
        if sub >= round_to_s:
            sub = math.floor(sub / round_to_s) * round_to_s
        chosen = (iso, sub)
        notes.append(
            "Read-noise limited: the sky is too dark for the tracking-limited sub length "
            "to swamp read noise. Fine for bright M42; take more subs."
        )
    iso, sub = chosen
    n = int(window_s // (sub + gap_s))
    if max_total_s is not None:
        n = min(n, int(max_total_s // sub))
    n = max(n, 1)
    return SubPlan(
        iso=iso,
        sub_s=float(sub),
        n_subs=n,
        total_s=n * sub,
        sky_e_s=sky,
        tracking_limit_s=tracking_limit_s,
        swamp_s=swamp_time(noise.rn(iso), sky),
        clip_s=clip_time(noise.clip_e(iso), sky),
        read_noise_limited=swamp_time(noise.rn(iso), sky) > sub,
        notes=notes,
    )
