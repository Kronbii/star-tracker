"""Barn-door geometry: Python mirror of firmware/star_tracker/src/core/geometry.cpp.

Tangent-arm ("type 1") barn door with a roller on the nut carriage:

    R sin(theta) - h cos(theta) = c
    h(theta) = R tan(theta) - c sec(theta)
    theta(h) = atan2(h, R) + asin(c / sqrt(R^2 + h^2))

R is the hinge-to-screw distance, h the roller-centre height above the hinge
plane, c the roller radius plus any strike-plane offset. The tests check this
module against the C++ implementation through firmware/test/track_sim.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

import numpy as np

from .constants import SIDEREAL_RATE


@dataclass(frozen=True)
class BarnDoorGeometry:
    radius_mm: float = 200.0
    contact_offset_mm: float = 11.0
    lead_mm: float = 2.0
    full_steps_per_rev: int = 200
    microsteps: int = 16
    home_height_mm: float = 3.0
    max_height_mm: float = 150.0

    @property
    def steps_per_rev(self) -> float:
        return float(self.full_steps_per_rev * self.microsteps)

    @property
    def steps_per_mm(self) -> float:
        return self.steps_per_rev / self.lead_mm

    def height_for_angle(self, theta):
        theta = np.asarray(theta, dtype=float)
        return (self.radius_mm * np.sin(theta) - self.contact_offset_mm) / np.cos(theta)

    def angle_for_height(self, h):
        h = np.asarray(h, dtype=float)
        rho = np.hypot(self.radius_mm, h)
        return np.arctan2(h, self.radius_mm) + np.arcsin(self.contact_offset_mm / rho)

    def angle_for_steps(self, steps):
        return self.angle_for_height(self.home_height_mm + np.asarray(steps) / self.steps_per_mm)

    def steps_for_angle(self, theta):
        return (self.height_for_angle(theta) - self.home_height_mm) * self.steps_per_mm

    @property
    def max_steps(self) -> int:
        return int(np.floor((self.max_height_mm - self.home_height_mm) * self.steps_per_mm))

    def dtheta_dh(self, theta):
        """Local ratio d(theta)/dh in rad/mm: cos^2(theta) / (R - c sin(theta))."""
        theta = np.asarray(theta, dtype=float)
        return np.cos(theta) ** 2 / (self.radius_mm - self.contact_offset_mm * np.sin(theta))

    def rad_per_microstep(self, theta):
        return self.dtheta_dh(theta) / self.steps_per_mm

    def screw_speed_mm_s(self, theta, rate=SIDEREAL_RATE):
        """Nut speed needed to track at `rate` at board angle theta."""
        return rate / self.dtheta_dh(theta)

    def screw_period_s(self, theta, rate=SIDEREAL_RATE):
        """Time for one lead-screw revolution while tracking at theta."""
        return self.lead_mm / self.screw_speed_mm_s(theta, rate)

    @property
    def home_angle(self) -> float:
        return float(self.angle_for_steps(0))

    @property
    def max_angle(self) -> float:
        return float(self.angle_for_steps(self.max_steps))

    def tracking_duration_s(self, rate=SIDEREAL_RATE) -> float:
        return (self.max_angle - self.home_angle) / rate

    def with_(self, **kw) -> BarnDoorGeometry:
        return replace(self, **kw)


def constant_speed_error(geom: BarnDoorGeometry, t_s, rate=SIDEREAL_RATE):
    """Angle error (rad) of an uncorrected drive: screw at the start speed, constant."""
    t_s = np.asarray(t_s, dtype=float)
    th0 = geom.home_angle
    v = geom.screw_speed_mm_s(th0, rate)
    h = geom.home_height_mm + v * t_s
    return geom.angle_for_height(h) - (th0 + rate * t_s)


def rate_error_from_geometry(
    true: BarnDoorGeometry, assumed: BarnDoorGeometry, theta, rate=SIDEREAL_RATE
):
    """Fractional angular-rate error when the firmware assumes `assumed` but the
    hardware is `true`, at true board angle theta.

    The firmware drives the nut at the speed its model needs; the board then
    turns at true dtheta/dh times that speed.
    """
    theta = np.asarray(theta, dtype=float)
    # Height on the true mechanism; the firmware believes the same step count
    # means the same height relative to its own home height.
    h_true = true.height_for_angle(theta)
    h_assumed = h_true - true.home_height_mm + assumed.home_height_mm
    theta_assumed = assumed.angle_for_height(h_assumed)
    commanded_mm_s = rate / assumed.dtheta_dh(theta_assumed)
    # A lead error scales the real nut speed for a commanded step rate.
    actual_mm_s = commanded_mm_s * true.lead_mm / assumed.lead_mm
    return actual_mm_s * true.dtheta_dh(theta) / rate - 1.0
