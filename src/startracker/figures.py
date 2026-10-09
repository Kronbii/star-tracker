"""Regenerates every figure in figures/ (``startracker figures``).

Visual language: Rami Kronbi's Sage system - dark surface #121614, ink
#eef1ec at 100/74/52 %, hairline grid. Multi-series charts use a categorical
order validated (CVD and normal-vision separation, contrast) on that surface;
Sage marks the one series a chart is about, against grey context.
Every tracking-performance curve is a model prediction and is labelled so.
"""

from __future__ import annotations

import csv
import datetime as dt
import itertools
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np

from . import autoguide, errors, exposure, geometry, planner
from .optics import Lens

SURFACE = "#121614"
INK = "#eef1ec"
INK2 = "#b5b8b4"  # 74 %
INK3 = "#848884"  # 52 %
GRID = "#2c302e"  # 12 % hairline
AXIS = "#424644"  # 22 %
SAGE = "#b7d3a8"
# Categorical slots, dark steps, validated on SURFACE (all checks pass).
CAT = ["#3987e5", "#d95926", "#199e70", "#c98500", "#d55181"]
CONTEXT = INK3

REPO = Path(__file__).resolve().parents[2]


def _style():
    plt.rcParams.update(
        {
            "figure.facecolor": SURFACE,
            "axes.facecolor": SURFACE,
            "savefig.facecolor": SURFACE,
            "axes.edgecolor": AXIS,
            "axes.labelcolor": INK2,
            "axes.titlecolor": INK,
            "axes.titlesize": 12,
            "axes.titleweight": "semibold",
            "axes.titlelocation": "left",
            "axes.labelsize": 10,
            "axes.grid": True,
            "grid.color": GRID,
            "grid.linewidth": 0.8,
            "grid.linestyle": "-",
            "axes.spines.top": False,
            "axes.spines.right": False,
            "xtick.color": INK3,
            "ytick.color": INK3,
            "xtick.labelcolor": INK3,
            "ytick.labelcolor": INK3,
            "font.family": "sans-serif",
            "font.size": 10,
            "text.color": INK2,
            "legend.frameon": False,
            "legend.labelcolor": INK2,
            "lines.linewidth": 2.0,
            "lines.solid_capstyle": "round",
            "lines.solid_joinstyle": "round",
            "axes.axisbelow": True,
        }
    )


def _note(fig, text):
    fig.text(0.01, 0.01, text, color=INK3, fontsize=8, ha="left", va="bottom")


def _spread(values, min_gap, log=False):
    """Nudge label positions apart (sorted), keeping order; returns new values."""
    v = np.log10(values) if log else np.asarray(values, dtype=float)
    order = np.argsort(v)
    out = v.copy()
    for a, b in itertools.pairwise(order):
        if out[b] - out[a] < min_gap:
            out[b] = out[a] + min_gap
    return 10**out if log else out


def _save(fig, out: Path, name: str) -> Path:
    p = out / name
    fig.savefig(p, dpi=150)
    plt.close(fig)
    return p


# ---------------------------------------------------------------------------


def fig_exposure_vs_focal(out: Path) -> Path:
    base = errors.Setup()
    f = np.geomspace(14, 400, 28)
    exp_, worst, pec, untr = [], [], [], []
    for fi in f:
        s = base.with_(lens=Lens(float(fi), 5.6))
        exp_.append(errors.longest_sharp_exposure(s, use_rss=True))
        worst.append(errors.longest_sharp_exposure(s))
        sp = s.with_(mech=s.mech.with_(pec_residual=0.25))
        pec.append(errors.longest_sharp_exposure(sp, use_rss=True))
        untr.append(errors.untracked_limit(s))
    cam = base.camera
    fig, ax = plt.subplots(figsize=(8.6, 5.2))
    fig.subplots_adjust(left=0.09, right=0.8, top=0.86, bottom=0.14)
    ax.fill_between(f, worst, exp_, color=SAGE, alpha=0.14, linewidth=0)
    ax.plot(f, exp_, color=SAGE, label="Tracked, expected (RSS)")
    ax.plot(f, worst, color=SAGE, linewidth=1.0, alpha=0.7, label="Tracked, worst case")
    ax.plot(f, pec, color=CAT[0], label="Tracked + PEC (75 % of screw PE removed)")
    ax.plot(f, untr, color=CONTEXT, label="Untracked, same 1.5 px criterion")
    ax.plot(f, exposure.npf_simple(f, 5.6, cam.pixel_um), color=CONTEXT, linewidth=1.0)
    ax.plot(f, exposure.rule_500(f, cam.crop_factor), color=CONTEXT, linewidth=1.0)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(14, 400)
    ax.set_ylim(0.1, 2000)
    ax.set_xticks([14, 24, 35, 50, 85, 135, 200, 300, 400])
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
    ax.get_xaxis().set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.set_yticks([0.1, 0.3, 1, 3, 10, 30, 100, 300, 1000])
    ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:g}"))
    ax.set_xlabel("Focal length (mm), Canon APS-C 3.72 um pixels, f/5.6")
    ax.set_ylabel("Longest sharp sub (s)")
    ax.set_title("Predicted longest sharp exposure vs focal length", pad=22)
    ax.text(
        0,
        1.02,
        "Trail <= 1.5 px on M42; polar error 0.25 deg; lead-screw PE +-4 um (assumed)",
        transform=ax.transAxes,
        color=INK3,
        fontsize=9,
    )
    # Direct labels at the right edge.
    x_end = f[-1]
    labels = [
        (exp_[-1], "tracked, expected", INK),
        (worst[-1], "tracked, worst", INK2),
        (pec[-1], "tracked + PEC", INK),
        (float(exposure.npf_simple(x_end, 5.6, cam.pixel_um)), "NPF rule", INK3),
        (float(exposure.rule_500(x_end, cam.crop_factor)), "500 rule", INK3),
        (untr[-1], "untracked, 1.5 px", INK3),
    ]
    ys = _spread([r[0] for r in labels], 0.17, log=True)
    for (y, txt, col), yy in zip(labels, ys, strict=True):
        ax.annotate(
            txt,
            (x_end, y),
            xytext=(x_end * 1.04, yy),
            color=col,
            fontsize=9,
            va="center",
            annotation_clip=False,
            arrowprops={"arrowstyle": "-", "color": AXIS, "lw": 0.8},
        )
    i200 = int(np.argmin(abs(f - 200)))
    s200 = base.with_(lens=Lens(200.0, 5.6))
    e200 = errors.longest_sharp_exposure(s200, use_rss=True)
    u200 = errors.untracked_limit(s200)
    ax.plot(
        [f[i200]],
        [exp_[i200]],
        "o",
        color=SAGE,
        markersize=7,
        markeredgecolor=SURFACE,
        markeredgewidth=2,
    )
    ax.annotate(
        f"200 mm: {e200:.0f} s tracked\nvs {u200:.2f} s untracked",
        (f[i200], exp_[i200]),
        xytext=(-150, -62),
        textcoords="offset points",
        color=INK,
        fontsize=9,
        arrowprops={"arrowstyle": "-", "color": INK3, "lw": 0.8},
    )
    ax.legend(loc="lower left", fontsize=8.5)
    _note(fig, "Model predictions (src/startracker/errors.py), not sky measurements.")
    return _save(fig, out, "exposure_vs_focal_length.png")


def fig_error_budget(out: Path) -> Path:
    s = errors.Setup()
    T_sub = 20.0
    b = errors.blur_budget(s, T_sub)
    items = sorted(b.terms.items(), key=lambda kv: kv[1])
    T = np.linspace(1, 120, 120)
    tot = [errors.blur_budget(s, t).total for t in T]
    rss = [errors.blur_budget(s, t).rss for t in T]
    tol = 1.5 * s.scale
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(10.5, 4.6), gridspec_kw={"width_ratios": [1, 1.15]})
    fig.subplots_adjust(left=0.2, right=0.97, top=0.82, bottom=0.15, wspace=0.42)
    y = np.arange(len(items))
    a1.barh(y, [v for _, v in items], height=0.55, color=SAGE)
    a1.set_yticks(y, [k for k, _ in items], color=INK2)
    a1.grid(axis="y", visible=False)
    for yi, (_, v) in zip(y, items, strict=True):
        a1.text(v + 0.08, yi, f'{v:.2f}"', va="center", color=INK2, fontsize=9)
    a1.set_xlim(0, max(v for _, v in items) * 1.3)
    a1.set_xlabel("Trail in one 20 s sub (arcsec)")
    a1.set_title("Where the blur comes from", pad=20)
    a1.text(
        0,
        1.03,
        "200 mm f/5.6, worst point of the travel",
        transform=a1.transAxes,
        color=INK3,
        fontsize=9,
    )
    a2.plot(T, tot, color=CAT[1], label="Worst case (linear sum)")
    a2.plot(T, rss, color=CAT[0], label="Expected (RSS)")
    a2.axhline(tol, color=INK3, linewidth=1.0)
    a2.text(118, tol + 0.25, '1.5 px at 3.83"/px', ha="right", color=INK3, fontsize=9)
    a2.set_xlabel("Sub length (s)")
    a2.set_ylabel("Predicted trail (arcsec)")
    a2.set_xlim(0, 120)
    a2.set_ylim(0, max(tot) * 1.08)
    a2.set_title("Total trail vs sub length", pad=20)
    a2.text(
        0,
        1.03,
        "PE saturates at half a screw turn (~70 s)",
        transform=a2.transAxes,
        color=INK3,
        fontsize=9,
    )
    a2.legend(loc="lower right", fontsize=9)
    _note(fig, "Predicted from assumed PE (+-4 um), 0.25 deg polar error, 600 ppm rate error.")
    return _save(fig, out, "error_budget_200mm.png")


def _load_track_csv():
    p = REPO / "figures" / "data" / "track_sim.csv"
    if not p.is_file():
        return None
    with open(p) as fh:
        rows = list(csv.DictReader(fh))
    return {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}


def fig_tangent(out: Path) -> Path:
    d = _load_track_csv()
    g = geometry.BarnDoorGeometry()
    source = "firmware core (C++) run on the host by firmware/test/track_sim"
    if d is None:
        t = np.arange(0, g.tracking_duration_s(), 30.0)
        d = {
            "t_s": t,
            "err_arcsec": np.zeros_like(t),
            "uncorrected_err_arcsec": geometry.constant_speed_error(g, t) * 206264.8,
        }
        source = "Python geometry mirror (track_sim.csv missing)"
    th = d["t_s"] / 3600.0
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(8.6, 5.6), sharex=True)
    fig.subplots_adjust(left=0.11, right=0.97, top=0.88, bottom=0.12, hspace=0.45)
    a1.plot(th, np.abs(d["uncorrected_err_arcsec"]) / 60.0, color=CONTEXT)
    a1.set_ylabel("Error (arcmin)")
    a1.set_title("Constant screw speed: the barn-door tangent error", pad=8)
    a2.plot(th, d["err_arcsec"], color=SAGE, linewidth=1.2)
    half = 0.5 * math.degrees(float(g.rad_per_microstep(g.home_angle))) * 3600
    a2.axhline(half, color=AXIS, linewidth=0.8)
    a2.axhline(-half, color=AXIS, linewidth=0.8)
    a2.text(th[-1], half + 0.05, "+-half a microstep", ha="right", color=INK3, fontsize=9)
    a2.set_ylim(-0.6, 0.6)
    a2.set_ylabel("Error (arcsec)")
    a2.set_xlabel("Hours of sidereal tracking from home")
    a2.set_title("Firmware schedule: every microstep timed from the closed-form inverse", pad=8)
    _note(fig, f"Board angle vs ideal sidereal angle; {source}.")
    return _save(fig, out, "tangent_correction.png")


def fig_pa_sensitivity(out: Path) -> Path:
    pa = np.linspace(0.05, 1.0, 20)
    base = errors.Setup()
    fig, ax = plt.subplots(figsize=(8.6, 4.8))
    fig.subplots_adjust(left=0.1, right=0.83, top=0.86, bottom=0.14)
    # Ordinal ramp (one hue, light -> dark = more PE), validated --ordinal on SURFACE.
    ramp = ["#86b6ef", "#3987e5", "#1c5cab"]
    ends = []
    for um, col in zip((2.0, 4.0, 8.0), ramp, strict=True):
        lim = []
        for p in pa:
            s = base.with_(polar_error_deg=float(p), mech=base.mech.with_(screw_pe_um=um))
            lim.append(errors.longest_sharp_exposure(s, use_rss=True))
        ax.plot(pa, lim, color=col)
        ends.append((lim[-1], f"PE +-{um:g} um"))
    for (_, txt), yy in zip(ends, _spread([e[0] for e in ends], 0.07, log=True), strict=True):
        ax.text(pa[-1] + 0.02, yy, txt, va="center", color=INK2, fontsize=9)
    ax.axvspan(0.15, 0.4, color=SAGE, alpha=0.08, linewidth=0)
    ax.text(0.275, 6.5, "sight tube\n(predicted)", ha="center", color=INK3, fontsize=9)
    ax.set_yscale("log")
    ax.set_yticks([5, 10, 20, 50, 100, 200])
    ax.get_yaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:g}"))
    ax.get_yaxis().set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.set_ylim(5, 200)
    ax.set_xlim(0, 1.0)
    ax.set_xlabel("Polar alignment error (deg)")
    ax.set_ylabel("Longest sharp sub at 200 mm (s)")
    ax.set_title("Polar alignment vs screw periodic error", pad=22)
    ax.text(
        0,
        1.02,
        "Expected (RSS) limit, 1.5 px. At +-4 um the screw PE dominates up to ~1 deg: "
        "PEC is the lever",
        transform=ax.transAxes,
        color=INK3,
        fontsize=9,
    )
    _note(fig, "Model predictions; PE amplitude is the largest assumption in the budget.")
    return _save(fig, out, "pa_vs_pe_sensitivity.png")


def fig_season(out: Path, season: planner.Season, pick: planner.Night) -> Path:
    dates = [n.date for n in season.nights]
    hours = np.array([n.usable_h for n in season.nights])
    illum = np.array([n.moon_illum for n in season.nights]) * 100
    fig, (a1, a2) = plt.subplots(
        2, 1, figsize=(10, 5.4), sharex=True, gridspec_kw={"height_ratios": [2.2, 1]}
    )
    fig.subplots_adjust(left=0.08, right=0.97, top=0.86, bottom=0.12, hspace=0.25)
    x = mdates.date2num(dates)
    a1.bar(x, hours, width=0.72, color=SAGE, linewidth=0)
    a1.set_ylabel("Usable hours")
    a1.set_title("M42 from Beirut: usable dark, moonless hours per night", pad=22)
    a1.text(
        0,
        1.03,
        f"Sun < -18 deg, M42 > 30 deg, Moon down or <= 10 % lit; {season.backend}",
        transform=a1.transAxes,
        color=INK3,
        fontsize=9,
    )
    xp = mdates.date2num(pick.date)
    a1.annotate(
        f"first session\n{pick.date:%a %d %b}",
        (xp, pick.usable_h),
        xytext=(xp + 12, pick.usable_h + 0.4),
        color=INK,
        fontsize=9,
        arrowprops={"arrowstyle": "-", "color": INK3, "lw": 0.8},
    )
    a1.set_ylim(0, 8)
    a2.plot(x, illum, color=CONTEXT, linewidth=1.5)
    a2.set_ylabel("Moon lit (%)")
    a2.set_ylim(0, 100)
    a2.set_yticks([0, 50, 100])
    a2.xaxis.set_major_locator(mdates.MonthLocator())
    a2.xaxis.set_major_formatter(mdates.DateFormatter("%b %Y"))
    _note(fig, "Moon illumination at local midnight. Times local (Asia/Beirut).")
    return _save(fig, out, "m42_season.png")


def fig_first_night(out: Path, season: planner.Season, pick: planner.Night) -> Path:
    i = season.nights.index(pick)
    S = season.samples
    tz = planner.ZoneInfo(season.site.tz)
    t = [dt.datetime.fromtimestamp(u, tz) for u in S["unix"][i]]
    x = mdates.date2num(t)
    sun, moon, tgt, good = S["sun_alt"][i], S["moon_alt"][i], S["target_alt"][i], S["good"][i]
    fig, ax = plt.subplots(figsize=(10, 4.8))
    fig.subplots_adjust(left=0.08, right=0.97, top=0.86, bottom=0.14)
    # Darkness as a recessive band; the usable window as a Sage wash.
    for lo, hi, a in ((-6, 90, 0.0), (-12, -6, 0.03), (-18, -12, 0.06)):
        m = (sun < hi) & (sun >= lo)
        for r0, r1 in planner._runs(m):
            ax.axvspan(x[r0], x[min(r1, len(x) - 1)], color=INK, alpha=a, linewidth=0)
    for r0, r1 in planner._runs(good):
        ax.axvspan(x[r0], x[min(r1, len(x) - 1)], color=SAGE, alpha=0.12, linewidth=0)
    ax.plot(x, tgt, color=SAGE, label="M42 altitude")
    ax.plot(x, moon, color=CAT[0], linewidth=1.5, label="Moon altitude")
    ax.axhline(30, color=INK3, linewidth=0.8)
    ax.text(x[5], 31.5, "30 deg limit", color=INK3, fontsize=9)
    ax.set_ylim(-10, 70)
    ax.set_xlim(x[60], x[-60])
    ax.xaxis.set_major_locator(mdates.HourLocator(interval=2, tz=tz))
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M", tz=tz))
    ax.set_ylabel("Altitude (deg)")
    w = pick.best_window
    ax.set_title(f"First session: {pick.date:%a %d %b %Y}, window {planner.fmt_window(w)}", pad=22)
    ax.text(
        0,
        1.03,
        "Shaded: twilight steps and darkness; Sage wash: usable window",
        transform=ax.transAxes,
        color=INK3,
        fontsize=9,
    )
    k = int(np.argmax(tgt))
    ax.annotate(
        f"transit {t[k]:%H:%M}, {tgt[k]:.0f} deg",
        (x[k], tgt[k]),
        xytext=(10, 10),
        textcoords="offset points",
        color=INK,
        fontsize=9,
    )
    ax.legend(loc="upper left", fontsize=9)
    _note(fig, f"Local time (Asia/Beirut). {season.backend}.")
    return _save(fig, out, "m42_first_night.png")


def fig_autoguide(out: Path) -> Path:
    run = autoguide.simulate_guiding(1800, seed=1)
    tm = run.t / 60
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(9, 5.6), sharex=True)
    fig.subplots_adjust(left=0.1, right=0.97, top=0.87, bottom=0.12, hspace=0.42)
    a1.plot(tm, run.ra_unguided, color=CONTEXT, linewidth=1.4, label="unguided")
    a1.plot(tm, run.ra_guided, color=SAGE, linewidth=1.4, label="guided")
    a1.set_ylabel("RA error (arcsec)")
    st = run.stats
    a1.set_title("Autoguiding prototype on synthetic star fields", pad=22)
    a1.text(
        0,
        1.04,
        f'RA rms {st["ra_rms_unguided_detrended"]:.1f}" (detrended) unguided -> '
        f'{st["ra_rms_guided"]:.1f}" guided; 2 s frames at 5"/px, 1" seeing',
        transform=a1.transAxes,
        color=INK3,
        fontsize=9,
    )
    a1.legend(loc="upper left", fontsize=9, ncol=2)
    md = run.dec
    a2.plot(tm, md, color=CAT[0], linewidth=1.4)
    a2.set_ylabel("Dec drift (arcsec)")
    a2.set_xlabel("Minutes")
    a2.set_title(
        f'Uncorrectable on one axis: Dec drift {st["dec_rate_arcsec_s"]:.3f}"/s '
        f"-> polar error >= {st['pa_estimate_deg']:.2f} deg",
        pad=8,
    )
    _note(
        fig,
        "Simulation: rendered frames -> detection -> centroids -> guide pulses; no real sky data.",
    )
    return _save(fig, out, "autoguide_sim.png")


def make_all(out: Path | str = "figures", season=None, pick=None) -> list[Path]:
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    _style()
    paths = [
        fig_exposure_vs_focal(out),
        fig_error_budget(out),
        fig_tangent(out),
        fig_pa_sensitivity(out),
        fig_autoguide(out),
    ]
    if season is None:
        from .session import build_plan

        _, season, pick = build_plan(dt.date(2026, 10, 9), not_before=dt.date(2026, 11, 1))
    paths += [fig_season(out, season, pick), fig_first_night(out, season, pick)]
    return paths
