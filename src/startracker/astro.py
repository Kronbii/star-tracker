"""Low-precision positional astronomy (vectorised numpy), used when the JPL
ephemeris is not available offline.

Sources: Astronomical Almanac "low-precision formulae" for the Sun (~0.01 deg)
and Moon (~0.3 deg in longitude, 0.2 deg in latitude); Meeus eq. 12.4 for
GMST; IAU 1976 precession (Lieske). UTC is used for UT1 (< 1 s error).
tests/test_astro.py checks these against skyfield + DE421: Sun altitude within
0.05 deg, Moon altitude within ~0.5 deg, illumination within 0.03, star
altitude within 0.02 deg - plenty for planning a night.
"""

from __future__ import annotations

import numpy as np

D2R = np.pi / 180.0

UNIX_EPOCH_JD = 2440587.5


def jd_from_unix(unix_s):
    return UNIX_EPOCH_JD + np.asarray(unix_s, dtype=float) / 86400.0


def gmst_deg(jd):
    d = np.asarray(jd, dtype=float) - 2451545.0
    t = d / 36525.0
    g = 280.46061837 + 360.98564736629 * d + 0.000387933 * t**2 - t**3 / 38710000.0
    return np.mod(g, 360.0)


def precess_from_j2000(ra_deg, dec_deg, jd):
    """IAU 1976 precession of mean coordinates J2000.0 -> epoch jd (degrees)."""
    t = (np.asarray(jd, dtype=float) - 2451545.0) / 36525.0
    zeta = (2306.2181 + (0.30188 + 0.017998 * t) * t) * t / 3600.0 * D2R
    z = (2306.2181 + (1.09468 + 0.018203 * t) * t) * t / 3600.0 * D2R
    theta = (2004.3109 - (0.42665 + 0.041833 * t) * t) * t / 3600.0 * D2R
    ra0 = np.asarray(ra_deg, dtype=float) * D2R
    dec0 = np.asarray(dec_deg, dtype=float) * D2R
    a = np.cos(dec0) * np.sin(ra0 + zeta)
    b = np.cos(theta) * np.cos(dec0) * np.cos(ra0 + zeta) - np.sin(theta) * np.sin(dec0)
    c = np.sin(theta) * np.cos(dec0) * np.cos(ra0 + zeta) + np.cos(theta) * np.sin(dec0)
    ra = np.mod(np.arctan2(a, b) + z, 2 * np.pi) / D2R
    dec = np.arctan2(c, np.hypot(a, b)) / D2R
    return ra, dec


def _ecl_to_eq(lam_deg, beta_deg, eps_deg):
    lam, beta, eps = lam_deg * D2R, beta_deg * D2R, eps_deg * D2R
    ra = np.arctan2(np.sin(lam) * np.cos(eps) - np.tan(beta) * np.sin(eps), np.cos(lam))
    dec = np.arcsin(np.sin(beta) * np.cos(eps) + np.cos(beta) * np.sin(eps) * np.sin(lam))
    return np.mod(ra / D2R, 360.0), dec / D2R


def sun_position(jd):
    """Apparent-ish RA/Dec of the Sun (deg) and its ecliptic longitude."""
    n = np.asarray(jd, dtype=float) - 2451545.0
    L = 280.460 + 0.9856474 * n
    g = (357.528 + 0.9856003 * n) * D2R
    lam = L + 1.915 * np.sin(g) + 0.020 * np.sin(2 * g)
    eps = 23.439 - 0.0000004 * n
    ra, dec = _ecl_to_eq(np.mod(lam, 360.0), np.zeros_like(lam), eps)
    return ra, dec, np.mod(lam, 360.0)


def moon_position(jd):
    """Geocentric RA/Dec (deg), ecliptic lon/lat (deg) and horizontal parallax (deg)."""
    jd = np.asarray(jd, dtype=float)
    T = (jd - 2451545.0) / 36525.0

    def s(a, b):
        return np.sin((a + b * T) * D2R)

    def c(a, b):
        return np.cos((a + b * T) * D2R)

    lam = (
        218.32
        + 481267.881 * T
        + 6.29 * s(135.0, 477198.87)
        - 1.27 * s(259.3, -413335.36)
        + 0.66 * s(235.7, 890534.22)
        + 0.21 * s(269.9, 954397.74)
        - 0.19 * s(357.5, 35999.05)
        - 0.11 * s(186.5, 966404.03)
    )
    beta = (
        5.13 * s(93.3, 483202.02)
        + 0.28 * s(228.2, 960400.89)
        - 0.28 * s(318.3, 6003.15)
        - 0.17 * s(217.6, -407332.21)
    )
    par = (
        0.9508
        + 0.0518 * c(135.0, 477198.87)
        + 0.0095 * c(259.3, -413335.36)
        + 0.0078 * c(235.7, 890534.22)
        + 0.0028 * c(269.9, 954397.74)
    )
    eps = 23.439 - 0.0000004 * (jd - 2451545.0)
    lam = np.mod(lam, 360.0)
    ra, dec = _ecl_to_eq(lam, beta, eps)
    return ra, dec, lam, beta, par


def moon_illumination(jd):
    """Illuminated fraction of the Moon's disc (0..1)."""
    _, _, lam_s = sun_position(jd)
    _, _, lam_m, beta_m, _ = moon_position(jd)
    cos_psi = np.cos(beta_m * D2R) * np.cos((lam_m - lam_s) * D2R)
    return (1.0 - cos_psi) / 2.0


def alt_az(ra_deg, dec_deg, jd, lat_deg, lon_deg):
    """Geometric altitude and azimuth (deg, azimuth from north through east)."""
    ha = (gmst_deg(jd) + lon_deg - np.asarray(ra_deg)) * D2R
    dec = np.asarray(dec_deg) * D2R
    lat = lat_deg * D2R
    alt = np.arcsin(np.sin(lat) * np.sin(dec) + np.cos(lat) * np.cos(dec) * np.cos(ha))
    az = np.arctan2(
        -np.cos(dec) * np.sin(ha),
        np.sin(dec) * np.cos(lat) - np.cos(dec) * np.cos(ha) * np.sin(lat),
    )
    return alt / D2R, np.mod(az / D2R, 360.0)


def moon_topocentric_alt(jd, lat_deg, lon_deg):
    ra, dec, _, _, par = moon_position(jd)
    alt, _ = alt_az(ra, dec, jd, lat_deg, lon_deg)
    # Parallax lowers the Moon by ~ p cos(alt).
    return alt - np.degrees(np.arcsin(np.sin(par * D2R) * np.cos(alt * D2R)))


POLARIS_J2000 = (37.954542, 89.264111)  # deg (02h31m49.09s, +89d15m50.8s)
POLARIS_PM_MAS = (44.48, -11.85)


def polaris_clock(unix_s, lon_deg):
    """Polaris' clock position around the pole, facing north (12 = up).

    Mirrors firmware/star_tracker/src/core/sky.cpp."""
    jd = jd_from_unix(unix_s)
    years = (jd - 2451545.0) / 365.25
    ra0 = POLARIS_J2000[0] + POLARIS_PM_MAS[0] * 1e-3 * years / 3600.0 / np.cos(
        POLARIS_J2000[1] * D2R
    )
    dec0 = POLARIS_J2000[1] + POLARIS_PM_MAS[1] * 1e-3 * years / 3600.0
    ra, dec = precess_from_j2000(ra0, dec0, jd)
    ha_h = np.mod(gmst_deg(jd) + lon_deg - ra, 360.0) / 15.0
    clock = np.mod(12.0 - ha_h / 2.0, 12.0)
    clock = np.where(clock <= 0.0, clock + 12.0, clock)
    return clock, ha_h, 90.0 - dec
