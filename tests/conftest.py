from pathlib import Path

import pytest

from startracker.ephemeris import find_ephemeris

REPO = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session")
def skyfield_backend():
    path = find_ephemeris()
    if path is None:
        pytest.skip("DE421 not present (run `startracker fetch-ephemeris`)")
    from startracker.ephemeris import SkyfieldBackend

    return SkyfieldBackend(path)
