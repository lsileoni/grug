from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Sequence, Tuple

Z_95 = 1.959963984540054

_PENTA_VALUES = (0.0, 0.25, 0.5, 0.75, 1.0)
_TRI_VALUES = (0.0, 0.5, 1.0)

def elo_to_score(elo: float) -> float:
    return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))


def score_to_elo(score: float) -> float:
    score = min(max(score, 1e-9), 1.0 - 1e-9)
    return -400.0 * math.log10(1.0 / score - 1.0)

def phi(x: float) -> float:
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def phi_inv(p: float) -> float:
    if p <= 0.0:
        return float("-inf")
    if p >= 1.0:
        return float("inf")

    a = (-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
         1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00)
    b = (-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
         6.680131188771972e+01, -1.328068155288572e+01)
    c = (-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
         -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00)
    d = (7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
         3.754408661907416e+00)

    p_low = 0.02425
    p_high = 1.0 - p_low
    if p < p_low:
        q = math.sqrt(-2.0 * math.log(p))
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)
    if p <= p_high:
        q = p - 0.5
        r = q * q
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / \
               (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0)
    q = math.sqrt(-2.0 * math.log(1.0 - p))
    return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0)

def _mean_var(counts: Sequence[int], values: Sequence[float]) -> Tuple[float, float, int]:
    n = sum(counts)
    if n == 0:
        return 0.5, 0.0, 0
    mean = sum(c * v for c, v in zip(counts, values)) / n
    var = sum(c * (v - mean) ** 2 for c, v in zip(counts, values)) / n
    return mean, var, n

@dataclass
class SPRTResult:
    llr: float
    lower: float
    upper: float
    state: str

    @property
    def finished(self) -> bool:
        return self.state != "running"


def sprt_bounds(alpha: float, beta: float) -> Tuple[float, float]:
    lower = math.log(beta / (1.0 - alpha))
    upper = math.log((1.0 - beta) / alpha)
    return lower, upper


def _llr(mean: float, var: float, n: int, s0: float, s1: float) -> float:
    if n == 0 or var <= 1e-12:
        return 0.0
    return n * (s1 - s0) / var * (mean - 0.5 * (s0 + s1))


def sprt(pentanomial: Sequence[int], elo0: float, elo1: float,
         alpha: float = 0.05, beta: float = 0.05) -> SPRTResult:
    s0, s1 = elo_to_score(elo0), elo_to_score(elo1)
    mean, var, n = _mean_var(pentanomial, _PENTA_VALUES)
    llr = _llr(mean, var, n, s0, s1)
    lower, upper = sprt_bounds(alpha, beta)
    state = "accepted" if llr >= upper else "rejected" if llr <= lower else "running"
    return SPRTResult(llr=llr, lower=lower, upper=upper, state=state)


def sprt_trinomial(wdl: Sequence[int], elo0: float, elo1: float,
                   alpha: float = 0.05, beta: float = 0.05) -> SPRTResult:
    w, d, l = wdl
    s0, s1 = elo_to_score(elo0), elo_to_score(elo1)
    mean, var, n = _mean_var((l, d, w), _TRI_VALUES)
    llr = _llr(mean, var, n, s0, s1)
    lower, upper = sprt_bounds(alpha, beta)
    state = "accepted" if llr >= upper else "rejected" if llr <= lower else "running"
    return SPRTResult(llr=llr, lower=lower, upper=upper, state=state)

@dataclass
class EloResult:
    elo: float
    elo_lo: float
    elo_hi: float
    margin: float
    los: float
    score: float
    games: int
    pairs: int
    wdl: Tuple[int, int, int] = (0, 0, 0)
    pentanomial: Tuple[int, int, int, int, int] = (0, 0, 0, 0, 0)
    draw_ratio: float = 0.0


def _elo_from_dist(mean: float, var: float, n: int, confidence: float,
                   games: int, pairs: int) -> EloResult:
    if n == 0:
        return EloResult(0.0, 0.0, 0.0, 0.0, 0.5, games, pairs)
    se = math.sqrt(var / n) if var > 0 else 0.0
    z = Z_95 if abs(confidence - 0.95) < 1e-9 else phi_inv(0.5 + confidence / 2.0)
    lo_score = min(max(mean - z * se, 1e-9), 1.0 - 1e-9)
    hi_score = min(max(mean + z * se, 1e-9), 1.0 - 1e-9)
    elo = score_to_elo(mean)
    elo_lo = score_to_elo(lo_score)
    elo_hi = score_to_elo(hi_score)
    los = phi((mean - 0.5) / se) if se > 0 else (1.0 if mean > 0.5 else 0.0 if mean < 0.5 else 0.5)
    return EloResult(
        elo=elo, elo_lo=elo_lo, elo_hi=elo_hi, margin=0.5 * (elo_hi - elo_lo),
        los=los, score=mean, games=games, pairs=pairs,
    )


def elo_pentanomial(pentanomial: Sequence[int], confidence: float = 0.95) -> EloResult:
    mean, var, n = _mean_var(pentanomial, _PENTA_VALUES)
    ll, ld, dd, dw, ww = pentanomial
    wins = 2 * ww + dw
    losses = 2 * ll + ld
    draws = 2 * dd + ld + dw
    games = 2 * n
    res = _elo_from_dist(mean, var, n, confidence, games=games, pairs=n)
    res.wdl = (wins, draws, losses)
    res.pentanomial = tuple(pentanomial)  # type: ignore[assignment]
    res.draw_ratio = draws / games if games else 0.0
    return res


def elo_trinomial(wdl: Sequence[int], confidence: float = 0.95) -> EloResult:
    w, d, l = wdl
    mean, var, n = _mean_var((l, d, w), _TRI_VALUES)
    res = _elo_from_dist(mean, var, n, confidence, games=n, pairs=0)
    res.wdl = (w, d, l)
    res.draw_ratio = d / n if n else 0.0
    return res
