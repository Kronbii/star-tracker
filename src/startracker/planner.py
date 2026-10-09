"""When is the Orion Nebula well placed from Beirut, and what to shoot.

A night is sampled every few minutes from local noon to the next noon. A
sample is usable when

* the Sun is below -18 deg (astronomical darkness),
* the target is at least `target_alt_min` (30 deg: less air, less glow), and
* the Moon is below the horizon, or a thin crescent (illumination <= 10 %).

Usable samples are merged into windows; nights are labelled by the evening's
date in local time (Asia/Beirut, so EET/EEST changes are handled).
"""

from __future__ import annotations

import datetime as dt
import math
from dataclasses import dataclass, field
from zoneinfo import ZoneInfo

import numpy as np

from .ephemeris import ApproxBackend, get_backend


@dataclass(frozen=True)
class Site:
    name: str = "Beirut"
    lat_deg: float = 33.8938
    lon_deg: float = 35.5018
    elevation_m: float = 50.0
    tz: str = "Asia/Beirut"


@dataclass(frozen=True)
class Target:
    name: str = "M42 (Orion Nebula)"
    ra_deg: float = 83.82208  # 05h35m17.3s, J2000
    dec_deg: float = -5.39111  # -05d23m28s


@dataclass(frozen=True)
class Criteria:
    sun_alt_max: float = -18.0
    target_alt_min: float = 30.0
    moon_alt_max: float = 0.0
    moon_illum_ok: float = 0.10
    step_min: int = 5


@dataclass
class Night:
    date: dt.date  # evening date, local
    windows: list[tuple[dt.datetime, dt.datetime]]
    usable_h: float
    dark_h: float
    moon_illum: float  # at local midnight
    target_max_alt: float  # during darkness
    transit_local: dt.datetime | None
    weekend: bool  # Friday or Saturday evening
    dark_start: dt.datetime | None = None  # astronomical dusk
    dark_end: dt.datetime | None = None  # astronomical dawn

    @property
    def best_window(self) -> tuple[dt.datetime, dt.datetime] | None:
        if not self.windows:
            return None
        return max(self.windows, key=lambda w: w[1] - w[0])


@dataclass
class Season:
    nights: list[Night]
    backend: str
    site: Site
    target: Target
    criteria: Criteria
    samples: dict = field(default_factory=dict)  # raw arrays for figures


def _runs(mask: np.ndarray) -> list[tuple[int, int]]:
    """Index ranges [i, j) of consecutive True values."""
    out = []
    i = None
    for k, v in enumerate(mask):
        if v and i is None:
            i = k
        elif not v and i is not None:
            out.append((i, k))
            i = None
    if i is not None:
        out.append((i, len(mask)))
    return out


def scan_season(
    start: dt.date,
    n_nights: int,
    site: Site | None = None,
    target: Target | None = None,
    criteria: Criteria | None = None,
    backend=None,
) -> Season:
    site = site or Site()
    target = target or Target()
    crit = criteria or Criteria()
    backend = backend or get_backend()
    tz = ZoneInfo(site.tz)
    step = crit.step_min * 60
    per_night = 24 * 3600 // step + 1
    starts = []
    for i in range(n_nights):
        d = start + dt.timedelta(days=i)
        noon = dt.datetime(d.year, d.month, d.day, 12, 0, tzinfo=tz)
        starts.append(noon.timestamp())
    unix = (np.asarray(starts)[:, None] + np.arange(per_night)[None, :] * step).ravel()
    r = backend.compute(
        unix, site.lat_deg, site.lon_deg, site.elevation_m, target.ra_deg, target.dec_deg
    )
    shape = (n_nights, per_night)
    sun = r["sun_alt"].reshape(shape)
    moon = r["moon_alt"].reshape(shape)
    illum = r["moon_illum"].reshape(shape)
    talt = r["target_alt"].reshape(shape)
    unix2 = unix.reshape(shape)

    dark = sun < crit.sun_alt_max
    moon_ok = (moon < crit.moon_alt_max) | (illum <= crit.moon_illum_ok)
    good = dark & moon_ok & (talt >= crit.target_alt_min)

    nights = []
    for i in range(n_nights):
        d = start + dt.timedelta(days=i)
        wins = []
        for a, b in _runs(good[i]):
            t0 = dt.datetime.fromtimestamp(unix2[i, a], tz)
            t1 = dt.datetime.fromtimestamp(unix2[i, b - 1] + step, tz)
            wins.append((t0, t1))
        usable = good[i].sum() * step / 3600.0
        dark_h = dark[i].sum() * step / 3600.0
        midnight = per_night // 2
        tmax = float(talt[i][dark[i]].max()) if dark[i].any() else float("nan")
        k = int(np.argmax(talt[i]))
        transit = dt.datetime.fromtimestamp(unix2[i, k], tz) if 0 < k < per_night - 1 else None
        druns = _runs(dark[i])
        d0 = dt.datetime.fromtimestamp(unix2[i, druns[0][0]], tz) if druns else None
        d1 = dt.datetime.fromtimestamp(unix2[i, druns[-1][1] - 1] + step, tz) if druns else None
        nights.append(
            Night(
                date=d,
                windows=wins,
                usable_h=float(usable),
                dark_h=float(dark_h),
                moon_illum=float(illum[i, midnight]),
                target_max_alt=tmax,
                transit_local=transit,
                weekend=d.weekday() in (4, 5),
                dark_start=d0,
                dark_end=d1,
            )
        )
    return Season(
        nights=nights,
        backend=backend.name,
        site=site,
        target=target,
        criteria=crit,
        samples={
            "unix": unix2,
            "sun_alt": sun,
            "moon_alt": moon,
            "moon_illum": illum,
            "target_alt": talt,
            "good": good,
        },
    )


def dark_runs(season: Season, min_hours: float = 2.0) -> list[list[Night]]:
    """Group consecutive nights with at least min_hours usable (moonless spells)."""
    groups: list[list[Night]] = []
    cur: list[Night] = []
    for n in season.nights:
        if n.usable_h >= min_hours:
            cur.append(n)
        elif cur:
            groups.append(cur)
            cur = []
    if cur:
        groups.append(cur)
    return groups


def recommend_first_session(
    season: Season, min_hours: float = 2.0, not_before: dt.date | None = None
) -> tuple[Night, list[Night]]:
    """First moonless spell (starting on/after `not_before`) with >= min_hours:
    its best weekend night (or best night if it has no weekend), plus backups
    from the same and the next spell."""
    groups = dark_runs(season, min_hours)
    if not_before is not None:
        groups = [[n for n in g if n.date >= not_before] for g in groups]
        groups = [g for g in groups if g]
    if not groups:
        raise ValueError("no usable nights in the scanned period")
    first = groups[0]
    weekend = [n for n in first if n.weekend]
    pick = max(weekend or first, key=lambda n: n.usable_h)
    backups = sorted([n for n in first if n is not pick], key=lambda n: -n.usable_h)[:2]
    if len(groups) > 1:
        nxt = groups[1]
        backups += sorted([n for n in nxt if n.weekend] or nxt, key=lambda n: -n.usable_h)[:2]
    return pick, backups


def monthly_summary(season: Season) -> list[dict]:
    rows = {}
    for n in season.nights:
        key = (n.date.year, n.date.month)
        r = rows.setdefault(
            key, {"month": f"{n.date:%b %Y}", "nights_2h": 0, "hours": 0.0, "best": None}
        )
        r["hours"] += n.usable_h
        if n.usable_h >= 2.0:
            r["nights_2h"] += 1
        if r["best"] is None or n.usable_h > r["best"].usable_h:
            r["best"] = n
    return list(rows.values())


def fmt_window(w: tuple[dt.datetime, dt.datetime]) -> str:
    a, b = w
    return f"{a:%H:%M}-{b:%H:%M}"


def hours_to_hm(h: float) -> str:
    m = round(h * 60)
    return f"{m // 60}h{m % 60:02d}"


def is_approx(backend) -> bool:
    return isinstance(backend, ApproxBackend)


def moon_phase_name(illum: float) -> str:
    if illum < 0.03:
        return "new"
    if illum > 0.97:
        return "full"
    return f"{round(illum * 100)} % lit"


def airmass(alt_deg: float) -> float:
    """Kasten & Young (1989)."""
    return 1.0 / (math.sin(math.radians(alt_deg)) + 0.50572 * (alt_deg + 6.07995) ** -1.6364)
