"""Tracking rates and unit conversions (mirrors firmware/star_tracker/src/core/st_math.h)."""

from __future__ import annotations

import math

ARCSEC_PER_RAD = 206264.80624709636
DEG_PER_RAD = 180.0 / math.pi

SIDEREAL_DAY_S = 86164.0905
SOLAR_DAY_S = 86400.0
SIDEREAL_MONTH_DAYS = 27.321661

#: Mount rotation rates about the polar axis, rad/s.
SIDEREAL_RATE = 2.0 * math.pi / SIDEREAL_DAY_S
SOLAR_RATE = 2.0 * math.pi / SOLAR_DAY_S
LUNAR_RATE = SIDEREAL_RATE - 2.0 * math.pi / (SIDEREAL_MONTH_DAYS * SOLAR_DAY_S)

RATES = {"sidereal": SIDEREAL_RATE, "solar": SOLAR_RATE, "lunar": LUNAR_RATE}


def arcsec(rad: float) -> float:
    """Radians -> arcseconds."""
    return rad * ARCSEC_PER_RAD


def rad_from_arcsec(a: float) -> float:
    """Arcseconds -> radians."""
    return a / ARCSEC_PER_RAD
