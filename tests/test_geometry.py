import csv
import math

import numpy as np
import pytest

from startracker.constants import LUNAR_RATE, SIDEREAL_RATE, SOLAR_RATE, arcsec
from startracker.geometry import BarnDoorGeometry, constant_speed_error, rate_error_from_geometry

from .conftest import REPO


def test_rates():
    assert arcsec(SIDEREAL_RATE) == pytest.approx(15.041067, abs=1e-5)
    assert arcsec(SOLAR_RATE) == pytest.approx(15.0)
    assert arcsec(LUNAR_RATE) == pytest.approx(14.4921, abs=1e-3)


def test_round_trip_and_tangency():
    g = BarnDoorGeometry()
    h = np.linspace(-5, 200, 101)
    th = g.angle_for_height(h)
    np.testing.assert_allclose(g.height_for_angle(th), h, atol=1e-9)
    np.testing.assert_allclose(
        g.radius_mm * np.sin(th) - h * np.cos(th), g.contact_offset_mm, atol=1e-9
    )


def test_ratio_matches_numeric_derivative():
    g = BarnDoorGeometry()
    th = np.radians(np.linspace(5, 38, 12))
    dh = 1e-4
    num = (g.angle_for_height(g.height_for_angle(th) + dh) - th) / dh
    np.testing.assert_allclose(g.dtheta_dh(th), num, rtol=1e-4)


def test_design_numbers():
    g = BarnDoorGeometry()
    assert math.degrees(g.home_angle) == pytest.approx(4.01, abs=0.01)
    assert g.tracking_duration_s() / 3600 == pytest.approx(2.35, abs=0.02)
    assert g.screw_period_s(g.home_angle) == pytest.approx(137, abs=1)
    # ~0.64 arcsec per microstep near home: the effective reduction is ~2000:1.
    assert arcsec(g.rad_per_microstep(g.home_angle)) == pytest.approx(0.644, abs=0.005)


def test_uncorrected_drive_error_is_large():
    g = BarnDoorGeometry()
    assert abs(arcsec(constant_speed_error(g, 3600.0))) > 30


def test_rate_error_zero_for_matching_model_and_scales_with_radius():
    g = BarnDoorGeometry()
    th = np.radians(15)
    assert rate_error_from_geometry(g, g, th) == pytest.approx(0, abs=1e-12)
    # True R 0.1 % larger than assumed -> board turns slower, by somewhat less
    # than 0.1 % at 15 deg (the height/angle mapping partly compensates).
    e = rate_error_from_geometry(g.with_(radius_mm=200.2), g, th)
    assert -1.1e-3 < e < -0.7e-3
    # Lead 200 ppm long -> nut faster -> board ~200 ppm fast.
    e = rate_error_from_geometry(g.with_(lead_mm=2.0 * 1.0002), g, th)
    assert e == pytest.approx(2e-4, rel=0.05)


def test_python_geometry_matches_firmware_core():
    """figures/data/track_sim.csv is written by the C++ core (firmware/test/track_sim)."""
    p = REPO / "figures" / "data" / "track_sim.csv"
    with open(p) as fh:
        rows = list(csv.DictReader(fh))
    g = BarnDoorGeometry()
    steps = np.array([float(r["steps"]) for r in rows])
    ang = np.array([float(r["angle_deg"]) for r in rows])
    np.testing.assert_allclose(np.degrees(g.angle_for_steps(steps)), ang, atol=2e-6)
    # And the firmware's schedule stays within half a microstep of sidereal.
    err = np.array([float(r["err_arcsec"]) for r in rows])
    assert np.max(np.abs(err)) <= 0.5 * arcsec(g.rad_per_microstep(g.home_angle)) + 1e-3
