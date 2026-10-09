"""The offline approximation vs skyfield + DE421 (skipped without the file)."""

import datetime as dt

import numpy as np
import pytest

from startracker import astro
from startracker.ephemeris import ApproxBackend
from startracker.planner import Site, Target

# Unix times spread over the planning season, day and night.
T0 = dt.datetime(2026, 10, 9, tzinfo=dt.timezone.utc).timestamp()
TIMES = T0 + np.arange(0, 180 * 86400, 86400 * 3 + 3600 * 7.3)


def test_gmst_meeus_example():
    # Meeus ex. 12.a: 1987-04-10 0h UT -> 13h10m46.3668s
    assert astro.gmst_deg(2446895.5) / 15 == pytest.approx(13 + 10 / 60 + 46.3668 / 3600, abs=2e-6)


def test_precession_meeus_example():
    ra, dec = astro.precess_from_j2000(41.054063, 49.227750, 2462088.69)
    assert ra == pytest.approx(41.547214, abs=2e-5)
    assert dec == pytest.approx(49.348483, abs=2e-5)


def test_polaris_clock_matches_firmware_reference():
    # Same reference cases as firmware/test/test_sky.cpp (skyfield apparent place).
    cases = [(1794081600000, 0.8240), (1799519400000, 11.5015), (1791504000000, 11.8000)]
    for ms, clock in cases:
        c, _, sep = astro.polaris_clock(ms / 1000.0, 35.5018)
        d = (float(c) - clock + 6) % 12 - 6
        assert abs(d) < 0.03
        assert 0.6 < float(sep) < 0.65


def test_moon_illumination_range():
    il = astro.moon_illumination(astro.jd_from_unix(TIMES))
    assert il.min() >= 0 and il.max() <= 1
    assert il.max() > 0.95 and il.min() < 0.05


def test_approx_backend_agrees_with_skyfield(skyfield_backend):
    s, t = Site(), Target()
    args = (TIMES, s.lat_deg, s.lon_deg, s.elevation_m, t.ra_deg, t.dec_deg)
    a = ApproxBackend().compute(*args)
    b = skyfield_backend.compute(*args)
    assert np.max(np.abs(a["sun_alt"] - b["sun_alt"])) < 0.05
    # skyfield's apparent altitude excludes refraction; the residual is the
    # low-precision lunar theory plus our parallax shortcut.
    assert np.max(np.abs(a["moon_alt"] - b["moon_alt"])) < 0.6
    assert np.max(np.abs(a["moon_illum"] - b["moon_illum"])) < 0.03
    assert np.max(np.abs(a["target_alt"] - b["target_alt"])) < 0.02
