import datetime as dt

import numpy as np
import pytest

from startracker import planner
from startracker.ephemeris import ApproxBackend
from startracker.session import build_plan, predicted_headline


@pytest.fixture(scope="module")
def season():
    return planner.scan_season(dt.date(2026, 10, 9), 120, backend=ApproxBackend())


def test_windows_satisfy_criteria(season):
    S = season.samples
    good = S["good"]
    assert good.any()
    assert (S["sun_alt"][good] < -18).all()
    assert (S["target_alt"][good] >= 30).all()
    moon_ok = (S["moon_alt"][good] < 0) | (S["moon_illum"][good] <= 0.10)
    assert moon_ok.all()


def test_nights_are_local_and_dst_aware(season):
    by_date = {n.date: n for n in season.nights}
    # Lebanon leaves summer time on the last Sunday of October (2026-10-25).
    before = by_date[dt.date(2026, 10, 20)]
    after = by_date[dt.date(2026, 11, 20)]
    assert before.dark_start.utcoffset() == dt.timedelta(hours=3)
    assert after.dark_start.utcoffset() == dt.timedelta(hours=2)
    for n in season.nights:
        for a, b in n.windows:
            assert b > a
            assert n.dark_start <= a and b <= n.dark_end + dt.timedelta(minutes=5)


def test_m42_season_shape(season):
    months = {(n.date.year, n.date.month): 0.0 for n in season.nights}
    for n in season.nights:
        months[(n.date.year, n.date.month)] += n.usable_h
    # Winter is the season: December and January beat October.
    assert months[(2026, 12)] > months[(2026, 10)]
    assert months[(2027, 1)] > months[(2026, 10)]
    # Transit is around local midnight in mid December.
    dec15 = next(n for n in season.nights if n.date == dt.date(2026, 12, 15))
    assert dec15.transit_local.hour in (23, 0, 1)
    assert dec15.target_max_alt == pytest.approx(90 - 33.89 - 5.39, abs=0.5)


def test_recommendation_respects_not_before(season):
    pick, backups = planner.recommend_first_session(season, 2.0, dt.date(2026, 11, 1))
    assert pick.date >= dt.date(2026, 11, 1)
    assert pick.usable_h >= 2.0
    assert pick.weekend
    assert all(b.date >= dt.date(2026, 11, 1) for b in backups)
    early, _ = planner.recommend_first_session(season, 2.0)
    assert early.date < pick.date


def test_airmass():
    assert planner.airmass(90) == pytest.approx(1.0, abs=1e-3)
    assert planner.airmass(30) == pytest.approx(1.99, abs=0.01)


def test_runs():
    assert planner._runs(np.array([0, 1, 1, 0, 1], bool)) == [(1, 3), (4, 5)]


def test_session_plan_text():
    text, _, pick = build_plan(
        dt.date(2026, 10, 9), not_before=dt.date(2026, 11, 1), backend=ApproxBackend()
    )
    assert "prediction" in text.lower()
    assert f"{pick.date:%d %B %Y}" in text
    assert "| Lens |" in text and "Calibration frames" in text
    assert "sum (worst)" in text


def test_headline_numbers():
    h = predicted_headline()
    assert h[200.0]["worst"] < h[200.0]["expected"]
    assert h[50.0]["expected"] > h[200.0]["expected"]
    assert h["200_pec"] > h[200.0]["expected"]
    assert 2.2 < h["tracking_h"] < 2.5
