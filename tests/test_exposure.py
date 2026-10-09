import math

import pytest

from startracker import exposure
from startracker.optics import Camera, Lens, field_of_view_deg, plate_scale_arcsec_per_px


def test_camera_defaults():
    c = Camera()
    assert c.pixel_um == pytest.approx(3.717, abs=1e-3)
    assert plate_scale_arcsec_per_px(c, 200) == pytest.approx(3.83, abs=0.01)
    w, h = field_of_view_deg(c, 200)
    assert w == pytest.approx(6.38, abs=0.02)
    assert h == pytest.approx(4.27, abs=0.02)


def test_rules_of_thumb():
    assert exposure.rule_500(200, 1.6) == pytest.approx(1.5625)
    assert exposure.rule_300(50, 1.6) == pytest.approx(3.75)
    assert exposure.npf_simple(200, 5.6, 3.72) == pytest.approx((35 * 5.6 + 30 * 3.72) / 200)
    # Detailed NPF at the equator, cos(dec)=1; grows away from it.
    a = exposure.npf_detailed(200, 5.6, 3.72, 0.0)
    assert a == pytest.approx((16.856 * 5.6 + 0.0997 * 200 + 13.713 * 3.72) / 200)
    assert exposure.npf_detailed(200, 5.6, 3.72, 60.0) == pytest.approx(2 * a)


def test_sky_rate_scaling():
    cam = Camera()
    n = exposure.SensorNoise()
    a = exposure.surface_rate_e_s(20.0, Lens(200, 5.6), cam, n)
    # Etendue: depends on (pixel / f-number)^2, not on focal length.
    assert exposure.surface_rate_e_s(20.0, Lens(50, 5.6), cam, n) == pytest.approx(a)
    assert exposure.surface_rate_e_s(20.0, Lens(200, 2.8), cam, n) == pytest.approx(4 * a)
    assert exposure.surface_rate_e_s(22.5, Lens(200, 5.6), cam, n) == pytest.approx(a / 10)
    # Order of magnitude sanity: a suburban sky gives ~1-5 e-/s/px at f/5.6.
    assert 0.5 < exposure.surface_rate_e_s(18.0, Lens(200, 5.6), cam, n) < 10


def test_noise_model_interpolates():
    n = exposure.SensorNoise()
    assert n.rn(800) == 3.0
    assert n.rn(400) > n.rn(1000) > n.rn(1600)
    assert n.clip_e(1600) == pytest.approx(21000 / 16)


def test_plan_subs_respects_limits():
    cam = Camera()
    p = exposure.plan_subs(20.0, 18.0, Lens(200, 5.6), cam, window_s=3600)
    assert p.sub_s <= 20.0
    assert p.n_subs == int(3600 // (p.sub_s + 3.0))
    assert not p.read_noise_limited
    dark = exposure.plan_subs(20.0, 21.5, Lens(200, 5.6), cam, window_s=3600)
    assert dark.read_noise_limited and dark.notes
    capped = exposure.plan_subs(20.0, 18.0, Lens(200, 5.6), cam, window_s=36000, max_total_s=600)
    assert capped.total_s <= 600


def test_stack_snr_grows_as_sqrt_n():
    a = exposure.stack_snr(1.0, 2.0, 30, 10, 3.0, 0.05)
    b = exposure.stack_snr(1.0, 2.0, 30, 40, 3.0, 0.05)
    assert b / a == pytest.approx(2.0, rel=1e-6)
    # Read noise makes many short subs worse than few long ones at equal time.
    short = exposure.stack_snr(0.1, 0.1, 10, 360, 3.0, 0.0)
    long_ = exposure.stack_snr(0.1, 0.1, 120, 30, 3.0, 0.0)
    assert long_ > short
    assert math.isfinite(short)
