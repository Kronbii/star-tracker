"""Autoguiding prototype on synthetic star fields (the "vision" next step).

Design being prototyped (see docs/autoguiding.md):
  small guide camera (IMX290/462-class, 2.9 um, or a Pi HQ camera) behind a
  ~120 mm guide lens -> ~5"/px; 2 s exposures; a host (Pi / laptop) detects
  stars, measures the field shift, and sends `guide <ms> <multiplier>` to the
  tracker's HTTP API. A barn door has one axis, so only the RA component can
  be corrected; the declination drift that remains measures the polar error.

Pipeline here: render frames (Gaussian PSFs, sky, shot + read noise) ->
detect stars (robust background, local maxima, centroids) -> match against
the reference frame -> RA/Dec error -> hysteresis controller -> guide pulses
applied to a simulated tracker with periodic error, rate error and drift.
Nothing here has seen a real sky frame; it proves the algorithm and the
interfaces, with measured numbers to follow.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .constants import SIDEREAL_RATE, arcsec

# ---------------------------------------------------------------------------
# Synthetic frames


def random_field(n: int, shape: tuple[int, int], rng, flux=(3e3, 6e4), margin: int = 12):
    h, w = shape
    xy = np.column_stack([rng.uniform(margin, w - margin, n), rng.uniform(margin, h - margin, n)])
    f = np.exp(rng.uniform(np.log(flux[0]), np.log(flux[1]), n))
    return xy, f


def render(
    shape, xy, flux, sigma_px=1.3, sky_e=200.0, read_noise_e=3.0, rng=None, offset=(0.0, 0.0)
) -> np.ndarray:
    """Render stars (total electrons `flux`) at xy + offset; returns electrons."""
    rng = rng or np.random.default_rng()
    h, w = shape
    img = np.full(shape, sky_e, dtype=float)
    yy, xx = np.mgrid[0:h, 0:w]
    r = int(np.ceil(4 * sigma_px))
    for (x, y), f in zip(xy + np.asarray(offset), flux, strict=True):
        x0, y0 = round(x), round(y)
        ys, xs = (
            slice(max(0, y0 - r), min(h, y0 + r + 1)),
            slice(max(0, x0 - r), min(w, x0 + r + 1)),
        )
        g = np.exp(-((xx[ys, xs] - x) ** 2 + (yy[ys, xs] - y) ** 2) / (2 * sigma_px**2))
        img[ys, xs] += f * g / (2 * np.pi * sigma_px**2)
    img = rng.poisson(np.clip(img, 0, None)).astype(float)
    return img + rng.normal(0.0, read_noise_e, shape)


# ---------------------------------------------------------------------------
# Detection and measurement


def detect_stars(img: np.ndarray, k_sigma: float = 6.0, max_stars: int = 30, box: int = 5):
    """Returns (n, 3): x, y (centroids, px) and background-subtracted flux."""
    med = np.median(img)
    mad = np.median(np.abs(img - med)) * 1.4826
    thr = med + k_sigma * mad
    c = img[1:-1, 1:-1]
    peak = c > thr
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx or dy:
                peak &= c >= img[1 + dy : img.shape[0] - 1 + dy, 1 + dx : img.shape[1] - 1 + dx]
    ys, xs = np.nonzero(peak)
    ys, xs = ys + 1, xs + 1
    order = np.argsort(img[ys, xs])[::-1]
    out = []
    h, w = img.shape
    for i in order:
        y0, x0 = ys[i], xs[i]
        if y0 < box or x0 < box or y0 >= h - box or x0 >= w - box:
            continue
        if any((x0 - s[0]) ** 2 + (y0 - s[1]) ** 2 < (2 * box) ** 2 for s in out):
            continue
        cut = img[y0 - box : y0 + box + 1, x0 - box : x0 + box + 1] - med
        cut = np.clip(cut, 0, None)
        tot = cut.sum()
        if tot <= 0:
            continue
        gy, gx = np.mgrid[-box : box + 1, -box : box + 1]
        out.append((x0 + (gx * cut).sum() / tot, y0 + (gy * cut).sum() / tot, tot))
        if len(out) >= max_stars:
            break
    return np.array(out).reshape(-1, 3)


def match_shift(ref: np.ndarray, cur: np.ndarray, max_dist: float = 8.0, guess=(0.0, 0.0)):
    """Median displacement (dx, dy) of nearest-neighbour matches, and the count.

    `guess` is the expected shift (e.g. the previous frame's), so slow drifts
    larger than max_dist stay matched."""
    if len(ref) == 0 or len(cur) == 0:
        return np.nan, np.nan, 0
    d = cur[None, :, :2] - ref[:, None, :2]
    g = np.asarray(guess, dtype=float)
    dist = np.hypot(d[..., 0] - g[0], d[..., 1] - g[1])
    j = np.argmin(dist, axis=1)
    ok = dist[np.arange(len(ref)), j] < max_dist
    if not ok.any():
        return np.nan, np.nan, 0
    sel = d[np.arange(len(ref))[ok], j[ok]]
    return float(np.median(sel[:, 0])), float(np.median(sel[:, 1])), int(ok.sum())


# ---------------------------------------------------------------------------
# Closed-loop simulation


@dataclass
class GuideConfig:
    scale_arcsec_px: float = 5.0  # guide camera + lens
    exposure_s: float = 2.0
    cycle_s: float = 3.0  # exposure + readout + processing
    multiplier: float = 0.5  # pulses at 0.5x / 1.5x sidereal, as the firmware supports
    # PHD2-style "hysteresis" controller. The tracker integrates the pulses,
    # so a plain proportional move per frame already acts as an integrator;
    # an extra integral term would wind up.
    aggressiveness: float = 0.7
    hysteresis: float = 0.1
    min_pulse_ms: float = 20.0
    seeing_rms_arcsec: float = 1.0  # centroid jitter from seeing over 2 s
    shape: tuple[int, int] = (96, 128)
    n_stars: int = 10


@dataclass
class TrackerModel:
    pe_amp_arcsec: float = 4.1  # lead-screw PE at home, as in errors.py
    pe_period_s: float = 137.0
    pe2_amp_arcsec: float = 1.0
    rate_error_ppm: float = 600.0
    dec_drift_arcsec_s: float = 0.065  # 0.25 deg PA, worst case

    def ra_error(self, t):
        t = np.asarray(t, dtype=float)
        w = 2 * np.pi / self.pe_period_s
        return (
            self.pe_amp_arcsec * np.sin(w * t)
            + self.pe2_amp_arcsec * np.sin(2 * w * t)
            + arcsec(SIDEREAL_RATE) * self.rate_error_ppm * 1e-6 * t
        )


@dataclass
class GuideRun:
    t: np.ndarray
    ra_unguided: np.ndarray
    ra_guided: np.ndarray
    dec: np.ndarray
    measured_ra: np.ndarray
    pulses_ms: np.ndarray
    dec_rate_estimate: float
    stats: dict = field(default_factory=dict)


def simulate_guiding(
    duration_s: float = 1200.0,
    cfg: GuideConfig | None = None,
    model: TrackerModel | None = None,
    seed: int = 1,
) -> GuideRun:
    """Closed loop through rendered frames. RA error in arcsec (+ = board ahead)."""
    cfg = cfg or GuideConfig()
    model = model or TrackerModel()
    rng = np.random.default_rng(seed)
    xy, flux = random_field(cfg.n_stars, cfg.shape, rng)
    centre = np.array([cfg.shape[1] / 2, cfg.shape[0] / 2])
    xy = centre + (xy - centre) * 0.8
    ref = detect_stars(render(cfg.shape, xy, flux, rng=rng))
    sidereal = arcsec(SIDEREAL_RATE)
    pulse_rate = (1.0 - cfg.multiplier) * sidereal  # arcsec/s of correction while pulsing
    n = int(duration_s // cfg.cycle_s)
    t = np.arange(n) * cfg.cycle_s
    correction = 0.0  # accumulated RA correction applied by pulses (arcsec)
    last_u = 0.0
    last_shift = (0.0, 0.0)
    out_g, out_u, out_d, meas, meas_dec, pulses = [], [], [], [], [], []
    for ti in t:
        # Mean error over the exposure (star smeared across it).
        ts = ti + np.linspace(0, cfg.exposure_s, 5)
        ra_u = float(np.mean(model.ra_error(ts)))
        ra_g = ra_u + correction
        dec = float(model.dec_drift_arcsec_s * (ti + cfg.exposure_s / 2))
        jitter = rng.normal(0.0, cfg.seeing_rms_arcsec, 2)
        off_px = (np.array([ra_g, dec]) + jitter) / cfg.scale_arcsec_px
        frame = render(cfg.shape, xy, flux, rng=rng, offset=tuple(off_px))
        dx, dy, nm = match_shift(ref, detect_stars(frame), guess=last_shift)
        if nm >= 2:
            last_shift = (dx, dy)
        meas_dec.append(dy * cfg.scale_arcsec_px if nm >= 2 else np.nan)
        if nm < 2 or not np.isfinite(dx):
            m = np.nan
            pulse_ms = 0.0
        else:
            m = dx * cfg.scale_arcsec_px
            u = cfg.aggressiveness * ((1 - cfg.hysteresis) * m + cfg.hysteresis * last_u)
            last_u = u
            pulse_s = min(abs(u) / pulse_rate, cfg.cycle_s - cfg.exposure_s)
            pulse_ms = 1000.0 * pulse_s if 1000.0 * pulse_s >= cfg.min_pulse_ms else 0.0
            # Board ahead (+) -> slow down (multiplier < 1) -> correction negative.
            correction -= np.sign(u) * pulse_rate * pulse_ms / 1000.0
            pulse_ms *= np.sign(u)
        out_u.append(ra_u)
        out_g.append(ra_g)
        out_d.append(dec)
        meas.append(m)
        pulses.append(pulse_ms)
    dec_arr = np.asarray(out_d)
    md = np.asarray(meas_dec)
    ok = np.isfinite(md)
    rate = float(np.polyfit(t[ok], md[ok], 1)[0])  # from the measured centroids
    run = GuideRun(
        t, np.asarray(out_u), np.asarray(out_g), dec_arr, np.asarray(meas), np.asarray(pulses), rate
    )
    settle = t > 60.0
    detr = run.ra_unguided - np.polyval(np.polyfit(t, run.ra_unguided, 1), t)
    run.stats = {
        "ra_rms_unguided": float(np.std(run.ra_unguided[settle])),
        "ra_rms_unguided_detrended": float(np.std(detr[settle])),
        "ra_rms_guided": float(np.sqrt(np.mean(run.ra_guided[settle] ** 2))),
        "dec_rate_arcsec_s": rate,
        "pa_estimate_deg": float(np.degrees(rate / sidereal)),
    }
    return run
