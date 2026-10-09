"""Tracking-error model -> image blur -> longest sharp exposure.

Contributions (all converted to on-sky arcseconds of trail during one exposure):

* Polar-alignment error eps. The camera turns about the mount axis a, the sky
  about the true pole z. Their composition Ra(phi) Rz(-phi) is a small rotation
  of the field by chi = 2 acos(1 - sin^2(phi/2) (1 - cos eps)) ~ eps * phi about
  an axis perpendicular to the pole. No star can move by more than chi, and the
  worst-oriented star moves by exactly chi, so the worst case is chi(w T). The
  same rotation shows up as drift (translation) and field rotation depending
  on where the target is relative to the error; `pa_motion_simulation` computes
  that exactly with rotation matrices and the tests check it against chi.
* Drive-rate error (fractional, ppm): w * T * ppm * cos(dec).
* Periodic error of the drive (screw 1/rev and 2/rev, stepper microstep
  non-linearity): worst-phase range of each sinusoid within T,
  2 A sin(min(pi T / P, pi / 2)), times cos(dec).
* Microstep quantisation: one microstep peak-to-peak.
* Differential atmospheric refraction while the target's altitude changes.

The budget adds magnitudes linearly (a conservative upper bound) and also
reports the root-sum-square. Every number this produces is a prediction from
assumed inputs, listed in `MechanicalAssumptions`.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace

import numpy as np

from .constants import ARCSEC_PER_RAD, SIDEREAL_RATE, arcsec
from .geometry import BarnDoorGeometry, rate_error_from_geometry
from .optics import Camera, Lens, half_diagonal_rad, plate_scale_arcsec_per_px

# ---------------------------------------------------------------------------
# Assumptions


@dataclass(frozen=True)
class MechanicalAssumptions:
    """Assumed (not measured) mechanical error sources."""

    #: Axial periodic error of lead screw + nut + coupler, zero-to-peak, once per
    #: revolution (um). Rolled Tr8x2 screw with brass nut: assumed 4 um.
    screw_pe_um: float = 4.0
    #: Second harmonic, zero-to-peak (um).
    screw_pe2_um: float = 1.0
    #: Stepper microstep non-linearity, zero-to-peak, as a fraction of a full
    #: step, period one full step. Typical hybrid steppers: 3-10 %.
    microstep_error_frac: float = 0.05
    #: Fraction of the screw PE left after periodic-error correction
    #: (1.0 = no PEC; 0.25 is a plausible target once measured).
    pec_residual: float = 1.0
    #: Residual drive-rate error (ppm) - see `rate_error_budget_ppm`.
    rate_error_ppm: float = 600.0

    def with_(self, **kw) -> MechanicalAssumptions:
        return replace(self, **kw)


@dataclass(frozen=True)
class Tolerances:
    """1-sigma build/measurement uncertainties behind the rate-error budget."""

    radius_mm: float = 0.10  # hinge-to-screw distance measured with calipers
    contact_offset_mm: float = 0.10
    home_height_mm: float = 0.05
    lead_ppm: float = 200.0  # rolled lead screw lead accuracy (assumed)
    clock_ppm: float = 20.0  # ESP32 40 MHz crystal


def rate_error_budget_ppm(
    geom: BarnDoorGeometry | None = None,
    tol: Tolerances | None = None,
    theta: float | None = None,
) -> dict[str, float]:
    """Per-source fractional rate error (ppm) and their RSS, at board angle theta."""
    geom = geom or BarnDoorGeometry()
    tol = tol or Tolerances()
    if theta is None:
        theta = 0.5 * (geom.home_angle + geom.max_angle)
    out = {}
    for key, kw in (
        ("radius", {"radius_mm": geom.radius_mm + tol.radius_mm}),
        ("contact_offset", {"contact_offset_mm": geom.contact_offset_mm + tol.contact_offset_mm}),
        ("home_height", {"home_height_mm": geom.home_height_mm + tol.home_height_mm}),
        ("lead", {"lead_mm": geom.lead_mm * (1 + tol.lead_ppm * 1e-6)}),
    ):
        true = geom.with_(**kw)
        out[key] = abs(float(rate_error_from_geometry(true, geom, theta))) * 1e6
    out["clock"] = tol.clock_ppm
    out["rss"] = math.sqrt(sum(v * v for v in out.values()))
    return out


# ---------------------------------------------------------------------------
# Periodic error


@dataclass(frozen=True)
class PeriodicTerm:
    """Periodic rotation error about the polar axis: a fundamental of period
    `period_s` plus harmonics, given as (amplitude_arcsec zero-to-peak,
    harmonic number, phase_rad)."""

    harmonics: tuple[tuple[float, int, float], ...]
    period_s: float
    label: str = ""

    def __call__(self, t):
        t = np.asarray(t, dtype=float)
        w = 2 * np.pi / self.period_s
        return sum(a * np.sin(k * w * t + ph) for a, k, ph in self.harmonics)

    @property
    def amplitude_arcsec(self) -> float:
        return float(sum(abs(a) for a, _, _ in self.harmonics))

    def worst_range(self, T) -> float:
        """Largest peak-to-peak excursion of the waveform inside any window of
        length T (numerical, over all window start phases)."""
        T = float(T)
        if len(self.harmonics) == 1:
            a = abs(self.harmonics[0][0])
            k = self.harmonics[0][1]
            return float(2 * a * np.sin(min(np.pi * T * k / self.period_s, np.pi / 2)))
        starts = np.linspace(0, self.period_s, 96, endpoint=False)
        span = np.linspace(0, min(T, self.period_s), 192)
        e = self(starts[:, None] + span[None, :])
        return float(np.max(e.max(axis=1) - e.min(axis=1)))


def periodic_terms(
    geom: BarnDoorGeometry,
    mech: MechanicalAssumptions,
    theta: float,
    rate: float = SIDEREAL_RATE,
) -> list[PeriodicTerm]:
    """Lead-screw PE (1/rev + 2/rev, from the same source, so combined as one
    waveform with the harmonics in phase) and stepper microstep error."""
    k = float(geom.dtheta_dh(theta))  # rad per mm of nut travel
    p_screw = float(geom.screw_period_s(theta, rate))
    full_step_mm = geom.lead_mm / geom.full_steps_per_rev
    a1 = arcsec(mech.screw_pe_um * 1e-3 * k) * mech.pec_residual
    a2 = arcsec(mech.screw_pe2_um * 1e-3 * k) * mech.pec_residual
    am = arcsec(mech.microstep_error_frac * full_step_mm * k)
    return [
        PeriodicTerm(((a1, 1, 0.0), (a2, 2, 0.0)), p_screw, "lead screw PE"),
        PeriodicTerm(((am, 1, 0.0),), p_screw / geom.full_steps_per_rev, "stepper microstep"),
    ]


# ---------------------------------------------------------------------------
# Polar alignment: exact rotation model


def pa_rotation_angle(eps_rad, phi_rad):
    """Angle of the net field rotation Ra(phi) Rz(-phi), |a.z| = cos(eps)."""
    eps_rad = np.asarray(eps_rad, dtype=float)
    phi_rad = np.asarray(phi_rad, dtype=float)
    c = 1.0 - np.sin(phi_rad / 2) ** 2 * (1.0 - np.cos(eps_rad))
    return 2.0 * np.arccos(np.clip(c, -1.0, 1.0))


def _rot(axis: np.ndarray, angle: float) -> np.ndarray:
    axis = axis / np.linalg.norm(axis)
    x, y, z = axis
    c, s = math.cos(angle), math.sin(angle)
    C = 1 - c
    return np.array(
        [
            [c + x * x * C, x * y * C - z * s, x * z * C + y * s],
            [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
            [z * x * C - y * s, z * y * C + x * s, c + z * z * C],
        ]
    )


def pa_motion_simulation(
    eps_deg: float,
    orientation_deg: float,
    dec_deg: float,
    ha_deg: float,
    times_s: np.ndarray,
    field_offsets_rad: np.ndarray,
    mount_phase=None,
) -> np.ndarray:
    """Exact image motion of field stars for a misaligned polar axis.

    Frame: z = true celestial pole, x = meridian on the equator, star at hour
    angle H and declination d at (cos d cos H, -cos d sin H, sin d). The sky
    turns as Rz(-w t); the camera as Ra(-phi(t)) with phi = w t by default
    (`mount_phase(t)` overrides it, e.g. to add rate or periodic error).

    Returns displacements (n_times, n_points, 2) in arcsec in the camera's
    tangent plane (x = right, y = towards the pole), relative to t = 0.
    """
    eps = math.radians(eps_deg)
    psi = math.radians(orientation_deg)
    dec = math.radians(dec_deg)
    ha = math.radians(ha_deg)
    a = np.array([math.sin(eps) * math.cos(psi), math.sin(eps) * math.sin(psi), math.cos(eps)])
    z = np.array([0.0, 0.0, 1.0])
    b0 = np.array([math.cos(dec) * math.cos(ha), -math.cos(dec) * math.sin(ha), math.sin(dec)])
    u0 = z - b0 * (z @ b0)
    u0 /= np.linalg.norm(u0)
    r0 = np.cross(u0, b0)
    offs = np.atleast_2d(field_offsets_rad)
    pts = b0[None, :] + offs[:, :1] * r0[None, :] + offs[:, 1:2] * u0[None, :]
    pts /= np.linalg.norm(pts, axis=1, keepdims=True)
    basis = np.stack([r0, u0, b0])  # rows

    def project(v):
        c = v @ basis.T
        return np.stack([c[:, 0] / c[:, 2], c[:, 1] / c[:, 2]], axis=1)

    p0 = project(pts)
    out = np.empty((len(times_s), len(pts), 2))
    for i, t in enumerate(np.asarray(times_s, dtype=float)):
        phi = SIDEREAL_RATE * t if mount_phase is None else float(mount_phase(t))
        M = _rot(a, phi) @ _rot(z, -SIDEREAL_RATE * t)
        out[i] = (project(pts @ M.T) - p0) * ARCSEC_PER_RAD
    return out


def field_points(camera: Camera, focal_mm: float) -> np.ndarray:
    """Centre, corners and edge midpoints of the sensor as tangent-plane offsets (rad)."""
    hw = camera.sensor_w_mm / 2 / focal_mm
    hh = camera.sensor_h_mm / 2 / focal_mm
    xs = [0.0, -hw, hw, -hw, hw, 0.0, 0.0, -hw, hw]
    ys = [0.0, -hh, -hh, hh, hh, -hh, hh, 0.0, 0.0]
    return np.stack([xs, ys], axis=1)


def pa_blur_typical_factor(dec_deg: float, ha_deg: float = 0.0, n: int = 36) -> float:
    """Mean over error orientations of (centre-star drift) / (worst case)."""
    T = 60.0
    worst = float(pa_rotation_angle(math.radians(1.0), SIDEREAL_RATE * T)) * ARCSEC_PER_RAD
    vals = []
    for psi in np.linspace(0, 360, n, endpoint=False):
        d = pa_motion_simulation(1.0, psi, dec_deg, ha_deg, [0.0, T], np.zeros((1, 2)))
        vals.append(np.linalg.norm(d[-1, 0]))
    return float(np.mean(vals) / worst)


# ---------------------------------------------------------------------------
# Refraction


def refraction_arcsec(alt_deg):
    """Saemundsson's formula (true altitude in, arcsec out), standard conditions."""
    alt_deg = np.maximum(np.asarray(alt_deg, dtype=float), 0.5)
    return 60.0 * 1.02 / np.tan(np.radians(alt_deg + 10.3 / (alt_deg + 5.11)))


def altitude_deg(lat_deg, dec_deg, ha_deg):
    lat, dec, ha = (np.radians(np.asarray(v, dtype=float)) for v in (lat_deg, dec_deg, ha_deg))
    return np.degrees(np.arcsin(np.sin(lat) * np.sin(dec) + np.cos(lat) * np.cos(dec) * np.cos(ha)))


def refraction_drift(lat_deg: float, dec_deg: float, ha_start_deg: float, T: float) -> float:
    """Change of refraction (arcsec) during an exposure starting at ha_start."""
    ha_end = ha_start_deg + math.degrees(SIDEREAL_RATE * T)
    a0 = altitude_deg(lat_deg, dec_deg, ha_start_deg)
    a1 = altitude_deg(lat_deg, dec_deg, ha_end)
    return float(abs(refraction_arcsec(a1) - refraction_arcsec(a0)))


# ---------------------------------------------------------------------------
# Budget


@dataclass(frozen=True)
class Setup:
    """Everything the budget needs. Defaults: the design in this repo, a Canon
    APS-C body behind a 200 mm f/5.6 lens, M42, Beirut."""

    geom: BarnDoorGeometry = field(default_factory=BarnDoorGeometry)
    mech: MechanicalAssumptions = field(default_factory=MechanicalAssumptions)
    camera: Camera = field(default_factory=Camera)
    lens: Lens = field(default_factory=Lens)
    polar_error_deg: float = 0.25
    dec_deg: float = -5.39  # M42
    lat_deg: float = 33.89  # Beirut
    #: hour angles over which to take the worst refraction drift (deg)
    ha_range_deg: tuple[float, float] = (-45.0, 45.0)
    include_refraction: bool = True

    def with_(self, **kw) -> Setup:
        return replace(self, **kw)

    @property
    def scale(self) -> float:
        return plate_scale_arcsec_per_px(self.camera, self.lens.focal_mm)


@dataclass
class Budget:
    exposure_s: float
    terms: dict[str, float]  # arcsec of trail per contribution
    scale_arcsec_px: float

    @property
    def total(self) -> float:
        return float(sum(self.terms.values()))

    @property
    def rss(self) -> float:
        return float(math.sqrt(sum(v * v for v in self.terms.values())))

    @property
    def total_px(self) -> float:
        return self.total / self.scale_arcsec_px


def blur_budget(setup: Setup, T: float, theta: float | None = None) -> Budget:
    """Trail (arcsec) per error source for one exposure of T seconds.

    theta: board angle at which to evaluate the drive terms. None = worst case
    over the travel.
    """
    g = setup.geom
    thetas = [theta] if theta is not None else list(np.linspace(g.home_angle, g.max_angle, 7))
    cosd = math.cos(math.radians(setup.dec_deg))
    best = None
    for th in thetas:
        terms: dict[str, float] = {}
        terms["polar alignment"] = (
            float(pa_rotation_angle(math.radians(setup.polar_error_deg), SIDEREAL_RATE * T))
            * ARCSEC_PER_RAD
        )
        terms["drive rate"] = arcsec(SIDEREAL_RATE * T * setup.mech.rate_error_ppm * 1e-6) * cosd
        for term in periodic_terms(g, setup.mech, th):
            terms[term.label] = term.worst_range(T) * cosd
        terms["microstep quantisation"] = arcsec(float(g.rad_per_microstep(th))) * cosd
        if setup.include_refraction:
            has = np.linspace(setup.ha_range_deg[0], setup.ha_range_deg[1], 13)
            terms["refraction"] = max(
                refraction_drift(setup.lat_deg, setup.dec_deg, h, T) for h in has
            )
        b = Budget(T, terms, setup.scale)
        if best is None or b.total > best.total:
            best = b
    assert best is not None
    return best


def longest_sharp_exposure(
    setup: Setup, max_blur_px: float = 1.5, t_max: float = 1800.0, use_rss: bool = False
) -> float:
    """Longest T whose predicted trail stays within max_blur_px (bisection).

    use_rss=False: worst case, contributions added linearly (all in phase).
    use_rss=True: expected case, root-sum-square of independent contributions.
    """
    tol = max_blur_px * setup.scale

    def blur(T):
        b = blur_budget(setup, T)
        return b.rss if use_rss else b.total

    lo, hi = 0.0, t_max
    if blur(hi) <= tol:
        return hi
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if blur(mid) <= tol:
            lo = mid
        else:
            hi = mid
    return lo


def untracked_limit(setup: Setup, max_blur_px: float = 1.5) -> float:
    """Exact untracked limit: the sky moves w cos(dec) past a fixed camera."""
    rate = arcsec(SIDEREAL_RATE) * math.cos(math.radians(setup.dec_deg))
    return max_blur_px * setup.scale / rate


def field_rotation_corner_arcsec(setup: Setup, T: float) -> float:
    """Worst field-rotation displacement at the sensor corner (informational)."""
    chi = float(pa_rotation_angle(math.radians(setup.polar_error_deg), SIDEREAL_RATE * T))
    r = half_diagonal_rad(setup.camera, setup.lens.focal_mm)
    return chi * math.cos(math.radians(setup.dec_deg)) * r * ARCSEC_PER_RAD
