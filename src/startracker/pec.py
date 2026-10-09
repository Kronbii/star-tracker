"""Periodic-error fitting -> firmware PEC table.

Measure the board's angle error against motor position (bench: a laser dot
from the top board on a distant wall filmed by a webcam; sky: guide-camera
centroids with guiding off), then fit

    e(x) = c0 + c1 x + sum_k [a_k sin(2 pi k x / P) + b_k cos(2 pi k x / P)]

The drift terms (c0, c1) absorb polar-alignment drift and rate error; the
harmonics are the periodic error. When x is the microstep position relative
to home and P the microsteps per screw revolution, evaluating the harmonics at
the table phases gives the firmware's 64-entry PEC table (arcsec, true minus
geometric angle). `pec_commands` prints the serial/web commands to load it.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

TABLE_SIZE = 64


@dataclass
class PeFit:
    period: float
    coeffs: np.ndarray  # [c0, c1, a1, b1, a2, b2, ...]
    harmonics: int
    rms_residual: float

    def periodic(self, x):
        x = np.asarray(x, dtype=float)
        w = 2 * np.pi / self.period
        out = np.zeros_like(x)
        for k in range(1, self.harmonics + 1):
            a, b = self.coeffs[2 * k], self.coeffs[2 * k + 1]
            out += a * np.sin(k * w * x) + b * np.cos(k * w * x)
        return out

    def amplitudes(self) -> list[float]:
        return [
            float(np.hypot(self.coeffs[2 * k], self.coeffs[2 * k + 1]))
            for k in range(1, self.harmonics + 1)
        ]

    def table(self) -> np.ndarray:
        phases = np.arange(TABLE_SIZE) / TABLE_SIZE * self.period
        return self.periodic(phases)

    def describe(self) -> str:
        amps = ", ".join(f'{k + 1}/rev {a:.2f}"' for k, a in enumerate(self.amplitudes()))
        return f'{amps}; residual rms {self.rms_residual:.2f}"'


def _design(x, period, harmonics):
    x = np.asarray(x, dtype=float)
    w = 2 * np.pi / period
    cols = [np.ones_like(x), x - x.mean()]
    for k in range(1, harmonics + 1):
        cols += [np.sin(k * w * x), np.cos(k * w * x)]
    return np.stack(cols, axis=1)


def fit_periodic_error(x, err_arcsec, period: float, harmonics: int = 2) -> PeFit:
    A = _design(x, period, harmonics)
    coeffs, *_ = np.linalg.lstsq(A, np.asarray(err_arcsec, dtype=float), rcond=None)
    resid = np.asarray(err_arcsec) - A @ coeffs
    return PeFit(period, coeffs, harmonics, float(np.sqrt(np.mean(resid**2))))


def pec_commands(fit: PeFit) -> list[str]:
    lines = ["pec off"]
    lines += [f"pec {i} {v:.2f}" for i, v in enumerate(fit.table())]
    lines += ["pec on", "save"]
    return lines
