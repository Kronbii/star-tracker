import numpy as np

from startracker.cli import main


def test_budget_and_exposure(capsys):
    assert main(["budget", "--focal", "135", "--times", "10", "30"]) == 0
    out = capsys.readouterr().out
    assert "PREDICTED" in out and "longest sharp sub" in out
    assert main(["exposure", "--focal", "50", "--sqm", "18"]) == 0
    assert "500 rule" in capsys.readouterr().out


def test_polaris(capsys):
    assert main(["polaris", "--time", "2026-11-14T21:15+02:00"]) == 0
    assert "o'clock" in capsys.readouterr().out


def test_pec_from_csv(tmp_path, capsys):
    x = np.arange(0, 3 * 3200, 50.0)
    e = 3 * np.sin(2 * np.pi * x / 3200)
    p = tmp_path / "pe.csv"
    np.savetxt(p, np.column_stack([x, e]), delimiter=",", header="steps,err", comments="")
    assert main(["pec", str(p), "--period", "3200"]) == 0
    lines = capsys.readouterr().out.strip().splitlines()
    assert lines[0].startswith("# fitted: 1/rev 3.00")
    assert lines[-1] == "save"
