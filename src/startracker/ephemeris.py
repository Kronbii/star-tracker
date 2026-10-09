"""Ephemeris backends for the planner.

* SkyfieldBackend: skyfield + JPL DE421 (16 MB, 1900-2050), the reference.
  Fetch it once with ``startracker fetch-ephemeris``; it is looked up at
  $STARTRACKER_EPHEMERIS, then data/ephemeris/de421.bsp in the repository.
* ApproxBackend: the low-precision formulae in `astro`, no files needed.

Both return the same dictionary of arrays for a vector of UTC unix times.
"""

from __future__ import annotations

import os
import time
import urllib.request
from pathlib import Path

import numpy as np

from . import astro

DE421_URLS = (
    "https://ssd.jpl.nasa.gov/ftp/eph/planets/bsp/de421.bsp",
    "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/a_old_versions/de421.bsp",
)
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PATH = REPO_ROOT / "data" / "ephemeris" / "de421.bsp"


def find_ephemeris() -> Path | None:
    env = os.environ.get("STARTRACKER_EPHEMERIS")
    for p in ([Path(env)] if env else []) + [DEFAULT_PATH]:
        if p.is_file() and p.stat().st_size > 1_000_000:
            return p
    return None


def fetch_ephemeris(dest: Path = DEFAULT_PATH, retries: int = 5) -> Path:
    """Download DE421 with retries (the network may be slow or flaky)."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(".part")
    last = None
    for attempt in range(retries):
        for url in DE421_URLS:
            try:
                with urllib.request.urlopen(url, timeout=120) as r, open(tmp, "wb") as f:
                    while chunk := r.read(1 << 16):
                        f.write(chunk)
                if tmp.stat().st_size > 1_000_000:
                    tmp.replace(dest)
                    return dest
            except OSError as e:  # URLError, timeouts, resets
                last = e
        time.sleep(min(30, 2**attempt))
    raise RuntimeError(f"could not download DE421: {last}")


class ApproxBackend:
    name = "low-precision formulae (Astronomical Almanac)"

    def compute(self, unix_s, lat_deg, lon_deg, elevation_m, ra_j2000_deg, dec_j2000_deg):
        jd = astro.jd_from_unix(unix_s)
        sra, sdec, _ = astro.sun_position(jd)
        sun_alt, _ = astro.alt_az(sra, sdec, jd, lat_deg, lon_deg)
        moon_alt = astro.moon_topocentric_alt(jd, lat_deg, lon_deg)
        illum = astro.moon_illumination(jd)
        tra, tdec = astro.precess_from_j2000(ra_j2000_deg, dec_j2000_deg, jd)
        talt, taz = astro.alt_az(tra, tdec, jd, lat_deg, lon_deg)
        return {
            "sun_alt": sun_alt,
            "moon_alt": moon_alt,
            "moon_illum": illum,
            "target_alt": talt,
            "target_az": taz,
        }


class SkyfieldBackend:
    def __init__(self, path: Path):
        from skyfield.api import load, load_file

        self.ts = load.timescale(builtin=True)
        self.eph = load_file(str(path))
        self.name = f"skyfield + JPL {Path(path).stem.upper()}"

    def _time(self, unix_s):
        """Unix seconds -> skyfield Time. Unix time has no leap seconds, so go
        through calendar fields; ts.utc(1970, 1, 1, 0, 0, s) would count SI
        seconds and land ~27 s late."""
        us = (
            np.round(np.asarray(unix_s, dtype=float) * 1e6).astype("int64").astype("datetime64[us]")
        )
        days = us.astype("datetime64[D]")
        y = days.astype("datetime64[Y]").astype(int) + 1970
        m = days.astype("datetime64[M]").astype(int) % 12 + 1
        d = (days - days.astype("datetime64[M]")).astype(int) + 1
        sec = (us - days).astype("int64") / 1e6
        return self.ts.utc(y, m, d, 0, 0, sec)

    def compute(self, unix_s, lat_deg, lon_deg, elevation_m, ra_j2000_deg, dec_j2000_deg):
        from skyfield import almanac
        from skyfield.api import Star, wgs84

        t = self._time(unix_s)
        site = self.eph["earth"] + wgs84.latlon(lat_deg, lon_deg, elevation_m=elevation_m)
        obs = site.at(t)
        sun_alt = obs.observe(self.eph["sun"]).apparent().altaz()[0].degrees
        moon_alt = obs.observe(self.eph["moon"]).apparent().altaz()[0].degrees
        illum = almanac.fraction_illuminated(self.eph, "moon", t)
        star = Star(ra_hours=ra_j2000_deg / 15.0, dec_degrees=dec_j2000_deg)
        talt, taz, _ = obs.observe(star).apparent().altaz()
        return {
            "sun_alt": sun_alt,
            "moon_alt": moon_alt,
            "moon_illum": np.asarray(illum),
            "target_alt": talt.degrees,
            "target_az": taz.degrees,
        }


def get_backend(prefer: str = "auto"):
    """'auto' -> skyfield if DE421 is present, else the approximation."""
    if prefer in ("auto", "skyfield"):
        p = find_ephemeris()
        if p is not None:
            return SkyfieldBackend(p)
        if prefer == "skyfield":
            raise FileNotFoundError("DE421 not found; run `startracker fetch-ephemeris`")
    return ApproxBackend()
