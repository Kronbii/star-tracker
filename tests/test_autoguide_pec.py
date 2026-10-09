import numpy as np
import pytest

from startracker import autoguide, pec


def test_detection_and_centroids_are_subpixel():
    rng = np.random.default_rng(7)
    shape = (96, 128)
    xy, f = autoguide.random_field(10, shape, rng, flux=(2e4, 6e4))
    img = autoguide.render(shape, xy, f, rng=rng, sky_e=100, read_noise_e=2.0)
    found = autoguide.detect_stars(img)
    assert len(found) >= 8
    # Each detection is near a true star, within 0.15 px.
    d = np.hypot(found[:, None, 0] - xy[None, :, 0], found[:, None, 1] - xy[None, :, 1]).min(1)
    assert np.median(d) < 0.15


def test_shift_measurement():
    rng = np.random.default_rng(3)
    shape = (96, 128)
    xy, f = autoguide.random_field(12, shape, rng, flux=(2e4, 6e4))
    ref = autoguide.detect_stars(autoguide.render(shape, xy, f, rng=rng))
    cur = autoguide.detect_stars(autoguide.render(shape, xy, f, rng=rng, offset=(0.37, -0.21)))
    dx, dy, n = autoguide.match_shift(ref, cur)
    assert n >= 6
    assert dx == pytest.approx(0.37, abs=0.06)
    assert dy == pytest.approx(-0.21, abs=0.06)
    # A large drift is still matched when the previous shift is the guess.
    cur2 = autoguide.detect_stars(autoguide.render(shape, xy, f, rng=rng, offset=(1.0, 11.0)))
    dx, dy, n = autoguide.match_shift(ref, cur2, guess=(0.9, 10.5))
    assert n >= 4 and dy == pytest.approx(11.0, abs=0.1)


def test_guiding_reduces_ra_error_and_measures_dec_drift():
    run = autoguide.simulate_guiding(900, seed=2)
    st = run.stats
    assert st["ra_rms_guided"] < 0.5 * st["ra_rms_unguided_detrended"]
    assert st["dec_rate_arcsec_s"] == pytest.approx(0.065, rel=0.1)
    assert np.isfinite(run.measured_ra).mean() > 0.95


def test_pec_fit_recovers_harmonics_and_table():
    rng = np.random.default_rng(0)
    P = 3200.0  # microsteps per screw revolution
    x = np.arange(0, 6 * P, 25.0)
    truth = 4.0 * np.sin(2 * np.pi * x / P + 0.6) + 1.0 * np.sin(4 * np.pi * x / P - 1.1)
    drift = 0.002 * x + 3.0
    fit = pec.fit_periodic_error(x, truth + drift + rng.normal(0, 0.5, x.size), P, harmonics=2)
    a1, a2 = fit.amplitudes()
    assert a1 == pytest.approx(4.0, abs=0.1)
    assert a2 == pytest.approx(1.0, abs=0.1)
    tab = fit.table()
    assert tab.shape == (64,)
    phases = np.arange(64) / 64 * P
    expect = 4.0 * np.sin(2 * np.pi * phases / P + 0.6) + np.sin(4 * np.pi * phases / P - 1.1)
    np.testing.assert_allclose(tab, expect, atol=0.15)
    cmds = pec.pec_commands(fit)
    assert cmds[0] == "pec off" and cmds[-2:] == ["pec on", "save"]
    assert len(cmds) == 64 + 3
    # Every command fits the firmware parser's ranges (index 0..63, |v| <= 600).
    for c in cmds[1:-2]:
        _, i, v = c.split()
        assert 0 <= int(i) <= 63 and abs(float(v)) <= 600
