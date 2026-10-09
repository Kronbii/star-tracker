import itertools
import math

import numpy as np
import pytest

from startracker import errors
from startracker.constants import ARCSEC_PER_RAD, SIDEREAL_RATE, arcsec
from startracker.optics import Lens


def test_pa_rotation_small_angle_limit():
    eps = math.radians(0.5)
    phi = SIDEREAL_RATE * 120
    assert errors.pa_rotation_angle(eps, phi) == pytest.approx(eps * phi, rel=1e-3)
    assert errors.pa_rotation_angle(0.0, phi) == pytest.approx(0.0, abs=1e-12)


@pytest.mark.parametrize("dec", [-5.39, 30.0, 60.0])
def test_simulation_worst_case_equals_analytic_bound(dec):
    """No star moves more than chi, and some orientation reaches it."""
    T = 120.0
    chi = errors.pa_rotation_angle(math.radians(0.5), SIDEREAL_RATE * T) * ARCSEC_PER_RAD
    worst = 0.0
    for psi in range(0, 360, 10):
        d = errors.pa_motion_simulation(0.5, psi, dec, 0.0, [0.0, T], np.zeros((1, 2)))
        worst = max(worst, float(np.linalg.norm(d[-1, 0])))
    assert worst <= chi * 1.001
    assert worst == pytest.approx(chi, rel=0.01)


def test_field_rotation_when_error_points_at_target():
    """Error axis along the target's direction: centre still, corners rotate."""
    cam = errors.Setup().camera
    pts = errors.field_points(cam, 50.0)
    # Target on the meridian at dec 0: misalignment toward the meridian (psi=0)
    # makes the net rotation axis point at the target.
    d = errors.pa_motion_simulation(1.0, 0.0, 0.0, 0.0, [0.0, 600.0], pts)
    centre = np.linalg.norm(d[-1, 0])
    corner = np.linalg.norm(d[-1, 1:5], axis=1).max()
    assert corner > 5 * centre


def test_mount_phase_hook_adds_rate_error():
    """A perfectly aligned mount with a 0.1 % fast drive drifts w*T*1e-3."""
    T = 300.0
    d = errors.pa_motion_simulation(
        0.0,
        0.0,
        0.0,
        0.0,
        [0.0, T],
        np.zeros((1, 2)),
        mount_phase=lambda t: SIDEREAL_RATE * t * 1.001,
    )
    assert np.linalg.norm(d[-1, 0]) == pytest.approx(arcsec(SIDEREAL_RATE) * T * 1e-3, rel=1e-3)


def test_periodic_term_worst_range():
    t = errors.PeriodicTerm(((4.0, 1, 0.0),), 100.0)
    assert t.worst_range(50.0) == pytest.approx(8.0)
    assert t.worst_range(500.0) == pytest.approx(8.0)
    assert t.worst_range(10.0) == pytest.approx(8 * math.sin(math.pi * 0.1))
    two = errors.PeriodicTerm(((4.0, 1, 0.0), (1.0, 2, 0.0)), 100.0)
    # Combined waveform range is no more than the sum of the parts.
    assert two.worst_range(30.0) <= t.worst_range(30.0) + 2.0 + 1e-9
    assert two.worst_range(30.0) >= t.worst_range(30.0) * 0.9


def test_budget_terms_and_monotonic_total():
    s = errors.Setup()
    prev = 0.0
    for T in (5, 10, 20, 40, 80):
        b = errors.blur_budget(s, T)
        assert set(b.terms) >= {"polar alignment", "drive rate", "lead screw PE"}
        assert b.rss <= b.total
        assert b.total >= prev
        prev = b.total


def test_longest_exposure_behaviour():
    base = errors.Setup()
    lims = [
        errors.longest_sharp_exposure(base.with_(lens=Lens(f, 5.6)), use_rss=True)
        for f in (50, 135, 200, 300)
    ]
    assert all(a > b for a, b in itertools.pairwise(lims))
    s200 = base.with_(lens=Lens(200, 5.6))
    worst = errors.longest_sharp_exposure(s200)
    exp_ = errors.longest_sharp_exposure(s200, use_rss=True)
    assert worst < exp_
    b = errors.blur_budget(s200, worst)
    assert b.total_px == pytest.approx(1.5, abs=0.01)
    # PEC and better alignment can only help.
    pec = s200.with_(mech=s200.mech.with_(pec_residual=0.25))
    assert errors.longest_sharp_exposure(pec, use_rss=True) > 2 * exp_
    # Tracking beats untracked by well over an order of magnitude.
    assert exp_ > 20 * errors.untracked_limit(s200)


def test_untracked_limit_formula():
    s = errors.Setup(lens=Lens(50, 2.8), dec_deg=0.0)
    assert errors.untracked_limit(s, 1.0) == pytest.approx(s.scale / arcsec(SIDEREAL_RATE))


def test_rate_budget():
    b = errors.rate_error_budget_ppm()
    assert b["radius"] == pytest.approx(380, rel=0.15)  # ~0.1 mm in 200 mm, plus geometry
    assert 300 < b["rss"] < 1000
    assert errors.MechanicalAssumptions().rate_error_ppm >= b["rss"]


def test_refraction():
    assert errors.refraction_arcsec(90.0) == pytest.approx(0.0, abs=1.0)
    assert errors.refraction_arcsec(45.0) == pytest.approx(60.0, abs=3.0)
    # At transit the altitude is stationary: almost no refraction drift.
    assert errors.refraction_drift(33.89, -5.39, -0.125, 60) < 0.01


def test_typical_orientation_factor():
    f = errors.pa_blur_typical_factor(-5.39)
    assert 0.55 < f < 0.75  # ~2/pi for a target near the equator
