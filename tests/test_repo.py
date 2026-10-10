"""Repository consistency: BOM arithmetic, exported parts, firmware/Python defaults."""

import csv
import re

import pytest

from startracker.geometry import BarnDoorGeometry

from .conftest import REPO


def test_bom_arithmetic():
    with open(REPO / "hardware" / "bom.csv") as fh:
        rows = list(csv.DictReader(fh))
    total = 0.0
    for r in rows:
        if r["group"] == "total":
            continue
        q, u, t = float(r["qty"]), float(r["unit_usd_est"]), float(r["total_usd_est"])
        assert q * u == pytest.approx(t, abs=0.01), r["item"]
        total += t
    stated = float(next(r for r in rows if r["group"] == "total")["total_usd_est"])
    assert stated == pytest.approx(total, abs=0.01)


def test_every_part_in_build_script_has_an_stl():
    script = (REPO / "cad" / "build.sh").read_text()
    block = script[script.index("PARTS=(") : script.index(")", script.index("PARTS=("))]
    parts = block.replace("PARTS=(", "").split()
    assert len(parts) >= 10
    for p in parts:
        assert (REPO / "cad" / "stl" / f"{p}.stl").is_file(), p


def test_firmware_and_python_defaults_agree():
    """The C++ BarnDoorGeometry defaults and the Python mirror must match."""
    src = (REPO / "firmware" / "star_tracker" / "src" / "core" / "geometry.h").read_text()

    def cpp(name):
        return float(re.search(rf"{name}\s*=\s*([0-9.]+)", src).group(1))

    g = BarnDoorGeometry()
    assert cpp("radius_mm") == g.radius_mm
    assert cpp("contact_offset_mm") == g.contact_offset_mm
    assert cpp("lead_mm") == g.lead_mm
    assert cpp("full_steps_per_rev") == g.full_steps_per_rev
    assert cpp("microsteps") == g.microsteps
    assert cpp("home_height_mm") == g.home_height_mm
    assert cpp("max_height_mm") == g.max_height_mm


def test_cad_parameters_match_firmware_geometry():
    scad = (REPO / "cad" / "star_tracker.scad").read_text()

    def p(name):
        return float(re.search(rf"^{name}\s*=\s*([0-9.]+);", scad, re.M).group(1))

    g = BarnDoorGeometry()
    assert p("R") == g.radius_mm
    assert p("roller_od") / 2 == g.contact_offset_mm  # strike face in the hinge plane
    assert p("h_home") == g.home_height_mm
    assert p("h_max") == g.max_height_mm
