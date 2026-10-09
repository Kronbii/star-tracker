"""Command-line entry point: ``startracker <command>``.

budget           trail budget and longest sharp exposure for a lens
exposure         untracked rules vs tracked limit, sub plan for a sky
plan             M42 season scan + first-session plan (writes markdown)
polaris          Polaris clock position for the sight-tube reticle, now or at a time
fetch-ephemeris  download JPL DE421 for the skyfield backend
figures          regenerate figures/
pec              fit a PEC table from a CSV of (t_s, error_arcsec) and print commands
"""

from __future__ import annotations

import argparse
import datetime as dt
import sys
import time
from pathlib import Path

from . import errors, exposure
from .optics import Lens


def _setup(a) -> errors.Setup:
    s = errors.Setup(lens=Lens(a.focal, a.fnum), polar_error_deg=a.pa)
    return s.with_(mech=s.mech.with_(screw_pe_um=a.pe_um, pec_residual=a.pec_residual))


def _common(p):
    p.add_argument("--focal", type=float, default=200.0, help="focal length, mm")
    p.add_argument("--fnum", type=float, default=5.6, help="f-number")
    p.add_argument("--pa", type=float, default=0.25, help="polar alignment error, deg")
    p.add_argument("--pe-um", type=float, default=4.0, help="lead-screw PE, um zero-to-peak")
    p.add_argument("--pec-residual", type=float, default=1.0, help="PE fraction left after PEC")
    p.add_argument("--blur", type=float, default=1.5, help="allowed trail, pixels")


def cmd_budget(a) -> int:
    s = _setup(a)
    print(
        f'{s.lens.focal_mm:.0f} mm f/{s.lens.f_number}, {s.scale:.2f}"/px, '
        f"PA {s.polar_error_deg} deg (all values PREDICTED)"
    )
    for T in a.times:
        b = errors.blur_budget(s, T)
        parts = ", ".join(f"{k} {v:.2f}" for k, v in b.terms.items())
        print(
            f'  {T:6.0f} s: worst {b.total:5.2f}" ({b.total_px:.2f} px), '
            f'rss {b.rss:5.2f}"  [{parts}]'
        )
    print(
        f"  longest sharp sub ({a.blur} px): expected "
        f"{errors.longest_sharp_exposure(s, a.blur, use_rss=True):.1f} s, worst "
        f"{errors.longest_sharp_exposure(s, a.blur):.1f} s, untracked "
        f"{errors.untracked_limit(s, a.blur):.2f} s"
    )
    return 0


def cmd_exposure(a) -> int:
    s = _setup(a)
    cam = s.camera
    f, N = s.lens.focal_mm, s.lens.f_number
    print(
        f"Untracked: 500 rule {float(exposure.rule_500(f, cam.crop_factor)):.2f} s, "
        f"300 rule {float(exposure.rule_300(f, cam.crop_factor)):.2f} s, "
        f"NPF {float(exposure.npf_simple(f, N, cam.pixel_um)):.2f} s, "
        f"NPF detailed {float(exposure.npf_detailed(f, N, cam.pixel_um, s.dec_deg)):.2f} s, "
        f"exact {a.blur} px {errors.untracked_limit(s, a.blur):.2f} s"
    )
    lim = errors.longest_sharp_exposure(s, a.blur, use_rss=True)
    p = exposure.plan_subs(min(lim, 120.0), a.sqm, s.lens, cam, window_s=a.window * 3600)
    print(
        f"Tracked (predicted): limit {lim:.1f} s -> ISO {p.iso}, {p.n_subs} x {p.sub_s:.0f} s "
        f"({p.total_s / 60:.0f} min); sky {p.sky_e_s:.2f} e-/s/px, swamp {p.swamp_s:.0f} s, "
        f"clip {p.clip_s:.0f} s"
    )
    for n in p.notes:
        print("  note:", n)
    return 0


def cmd_plan(a) -> int:
    from .session import build_plan

    start = dt.date.fromisoformat(a.start) if a.start else dt.date.today()
    nb = dt.date.fromisoformat(a.not_before) if a.not_before else None
    text, _, pick = build_plan(start, not_before=nb)
    if a.out:
        Path(a.out).write_text(text)
        print(f"wrote {a.out}; first session {pick.date:%a %d %b %Y}")
    else:
        sys.stdout.write(text)
    return 0


def cmd_polaris(a) -> int:
    from .astro import polaris_clock

    t = dt.datetime.fromisoformat(a.time).timestamp() if a.time else time.time()
    clock, ha, sep = polaris_clock(t, a.lon)
    h = int(clock) or 12
    m = round((float(clock) - int(clock)) * 60)
    print(
        f"Polaris at {h}:{m:02d} o'clock on the reticle ring (hour angle {float(ha):.2f} h, "
        f"{float(sep):.3f} deg from the pole), facing north."
    )
    return 0


def cmd_fetch(a) -> int:
    from .ephemeris import DEFAULT_PATH, fetch_ephemeris

    p = fetch_ephemeris(Path(a.dest) if a.dest else DEFAULT_PATH)
    print(f"saved {p}")
    return 0


def cmd_figures(a) -> int:
    from .figures import make_all

    for p in make_all(Path(a.out)):
        print("wrote", p)
    return 0


def cmd_pec(a) -> int:
    import numpy as np

    from .pec import fit_periodic_error, pec_commands

    data = np.loadtxt(a.csv, delimiter=",", skiprows=1)
    fit = fit_periodic_error(data[:, 0], data[:, 1], a.period, harmonics=a.harmonics)
    print(f"# fitted: {fit.describe()}")
    for line in pec_commands(fit):
        print(line)
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        prog="startracker",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("budget", help="trail budget")
    _common(p)
    p.add_argument("--times", type=float, nargs="+", default=[10, 20, 30, 60, 120])
    p.set_defaults(fn=cmd_budget)
    p = sub.add_parser("exposure", help="exposure rules and sub plan")
    _common(p)
    p.add_argument("--sqm", type=float, default=20.5, help="sky brightness, mag/arcsec^2")
    p.add_argument("--window", type=float, default=1.0, help="imaging window, hours")
    p.set_defaults(fn=cmd_exposure)
    p = sub.add_parser("plan", help="season scan and first-session plan")
    p.add_argument("--start", help="YYYY-MM-DD (default today)")
    p.add_argument("--not-before", help="earliest first-session date, YYYY-MM-DD")
    p.add_argument("--out", help="write markdown here")
    p.set_defaults(fn=cmd_plan)
    p = sub.add_parser("polaris", help="Polaris clock position")
    p.add_argument("--time", help="ISO time with offset, e.g. 2026-11-14T21:15+02:00")
    p.add_argument("--lon", type=float, default=35.5018)
    p.set_defaults(fn=cmd_polaris)
    p = sub.add_parser("fetch-ephemeris", help="download DE421")
    p.add_argument("--dest")
    p.set_defaults(fn=cmd_fetch)
    p = sub.add_parser("figures", help="regenerate figures")
    p.add_argument("--out", default="figures")
    p.set_defaults(fn=cmd_figures)
    p = sub.add_parser("pec", help="fit PEC table")
    p.add_argument("csv")
    p.add_argument("--period", type=float, required=True, help="screw period, s")
    p.add_argument("--harmonics", type=int, default=2)
    p.set_defaults(fn=cmd_pec)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    raise SystemExit(main())
