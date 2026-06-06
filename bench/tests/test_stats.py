import math
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from bench import stats  # noqa: E402


def test_elo_score_roundtrip():
    assert stats.elo_to_score(0.0) == pytest.approx(0.5)
    assert stats.score_to_elo(0.5) == pytest.approx(0.0)
    assert stats.score_to_elo(0.75) == pytest.approx(190.848, abs=1e-2)
    for elo in (-300, -50, 7, 120, 480):
        assert stats.score_to_elo(stats.elo_to_score(elo)) == pytest.approx(elo, abs=1e-6)


def test_elo_score_monotonic():
    assert stats.elo_to_score(-100) < stats.elo_to_score(0) < stats.elo_to_score(100)


def test_phi():
    assert stats.phi(0.0) == pytest.approx(0.5)
    assert stats.phi(1.959963984540054) == pytest.approx(0.975, abs=1e-4)


def test_phi_inv():
    assert stats.phi_inv(0.5) == pytest.approx(0.0, abs=1e-9)
    assert stats.phi_inv(0.975) == pytest.approx(stats.Z_95, abs=1e-6)
    for p in (0.01, 0.2, 0.5, 0.8, 0.99):
        assert stats.phi(stats.phi_inv(p)) == pytest.approx(p, abs=1e-6)


def test_sprt_bounds():
    lo, hi = stats.sprt_bounds(0.05, 0.05)
    assert lo == pytest.approx(math.log(0.05 / 0.95))
    assert hi == pytest.approx(math.log(0.95 / 0.05))
    assert lo == pytest.approx(-hi)
    assert hi == pytest.approx(2.9444, abs=1e-3)


def test_sprt_symmetric_sample_is_neutral():
    penta = [100, 200, 400, 200, 100]
    r = stats.sprt(penta, elo0=-2.5, elo1=2.5)
    assert r.llr == pytest.approx(0.0, abs=1e-9)
    assert r.state == "running"


def test_sprt_neutral_sample_drifts_negative_in_asymmetric_window():
    r = stats.sprt([100, 200, 400, 200, 100], elo0=0, elo1=5)
    assert r.llr < 0


def test_sprt_empty_is_neutral():
    r = stats.sprt([0, 0, 0, 0, 0], elo0=0, elo1=5)
    assert r.llr == 0.0
    assert r.state == "running"


def test_sprt_sign_follows_advantage():
    good = stats.sprt([10, 40, 200, 60, 90], elo0=-5, elo1=5)
    bad = stats.sprt([90, 60, 200, 40, 10], elo0=-5, elo1=5)
    assert good.llr > 0
    assert bad.llr < 0
    assert good.llr == pytest.approx(-bad.llr, rel=1e-9)


def test_sprt_is_linear_in_sample_size():
    base = [20, 50, 200, 80, 60]
    doubled = [2 * x for x in base]
    r1 = stats.sprt(base, 0, 5)
    r2 = stats.sprt(doubled, 0, 5)
    assert r2.llr == pytest.approx(2.0 * r1.llr, rel=1e-9)


def test_sprt_accepts_clear_improvement():
    penta = [5, 30, 300, 320, 200]
    r = stats.sprt([x * 4 for x in penta], elo0=0, elo1=5)
    assert r.state == "accepted"
    assert r.llr >= r.upper


def test_sprt_rejects_clear_regression():
    penta = [200, 320, 300, 30, 5]
    r = stats.sprt([x * 4 for x in penta], elo0=0, elo1=5)
    assert r.state == "rejected"
    assert r.llr <= r.lower


def test_sprt_all_draws_is_neutral():
    r = stats.sprt([0, 0, 500, 0, 0], elo0=0, elo1=5)
    assert r.llr == 0.0
    assert r.state == "running"


def test_elo_pentanomial_neutral():
    res = stats.elo_pentanomial([100, 200, 400, 200, 100])
    assert res.elo == pytest.approx(0.0, abs=1e-6)
    assert res.elo_lo < 0 < res.elo_hi
    assert res.los == pytest.approx(0.5, abs=1e-6)
    assert res.games == 2 * 1000
    assert res.pairs == 1000


def test_elo_pentanomial_wdl_reconstruction():
    res = stats.elo_pentanomial([1, 2, 3, 4, 5])
    w, d, l = res.wdl
    assert w == 2 * 5 + 4
    assert l == 2 * 1 + 2
    assert d == 2 * 3 + 2 + 4
    assert w + d + l == res.games == 2 * 15


def test_elo_positive_sample_has_positive_elo_and_high_los():
    res = stats.elo_pentanomial([10, 40, 200, 60, 90])
    assert res.elo > 0
    assert res.los > 0.5
    assert res.elo_lo < res.elo < res.elo_hi
    assert res.margin == pytest.approx(0.5 * (res.elo_hi - res.elo_lo))


def test_elo_trinomial_matches_score():
    res = stats.elo_trinomial([60, 20, 20])
    assert res.score == pytest.approx(0.70)
    assert res.elo == pytest.approx(stats.score_to_elo(0.70))
    assert res.pairs == 0
    assert res.games == 100


def test_more_games_tightens_interval():
    small = stats.elo_pentanomial([10, 40, 100, 60, 90])
    big = stats.elo_pentanomial([100, 400, 1000, 600, 900])
    assert big.margin < small.margin
