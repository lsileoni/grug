#!/usr/bin/env python3
"""Compute an Elo rating (with error bars) for engine A relative to engine B.

Reads a fastchess/cutechess PGN where the two engines are named "dev" and
"base", tallies the dev engine's win/draw/loss counts, and prints an Elo
estimate with a 95% confidence interval and likelihood-of-superiority (LOS).

This is intentionally self-contained (stdlib only) so the benchmark workflow
needs no extra Python packages and no external bench server.

Usage:
    elo_report.py GAMES.PGN [DEV_NAME] [BASE_NAME]

Exit status:
    0  always (the workflow decides pass/fail from the emitted GitHub outputs)
"""

import math
import os
import re
import sys

TAG_RE = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')


def parse_games(pgn_path, dev_name, base_name):
    """Return (wins, draws, losses) from dev's perspective."""
    wins = draws = losses = 0
    white = black = result = None

    def flush():
        nonlocal wins, draws, losses, white, black, result
        if result is None or white is None or black is None:
            white = black = result = None
            return
        # Map the PGN result to dev's score.
        if dev_name not in (white, black):
            white = black = result = None
            return
        dev_is_white = white == dev_name
        if result == "1-0":
            if dev_is_white:
                wins += 1
            else:
                losses += 1
        elif result == "0-1":
            if dev_is_white:
                losses += 1
            else:
                wins += 1
        elif result == "1/2-1/2":
            draws += 1
        # "*" (unfinished) is ignored.
        white = black = result = None

    with open(pgn_path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = TAG_RE.match(line.strip())
            if not m:
                continue
            key, val = m.group(1), m.group(2)
            if key == "White":
                # A new White tag begins a new game record; flush the prior one.
                flush()
                white = val
            elif key == "Black":
                black = val
            elif key == "Result":
                result = val
    flush()
    return wins, draws, losses


def phi_inv(p):
    """Inverse standard-normal CDF (Acklam's rational approximation)."""
    if p <= 0.0:
        return -math.inf
    if p >= 1.0:
        return math.inf
    a = [-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
         1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00]
    b = [-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
         6.680131188771972e+01, -1.328068155288572e+01]
    c = [-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
         -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00]
    d = [7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
         3.754408661907416e+00]
    plow = 0.02425
    phigh = 1 - plow
    if p < plow:
        q = math.sqrt(-2 * math.log(p))
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1)
    if p <= phigh:
        q = p - 0.5
        r = q * q
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / \
               (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1)
    q = math.sqrt(-2 * math.log(1 - p))
    return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
           ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1)


def score_to_elo(score):
    score = min(max(score, 1e-9), 1 - 1e-9)
    return -400.0 * math.log10(1.0 / score - 1.0)


def elo_with_ci(wins, draws, losses, confidence=0.95):
    """Elo point estimate and symmetric CI half-width using the WDL model."""
    n = wins + draws + losses
    if n == 0:
        return 0.0, float("inf"), 0.5
    score = (wins + 0.5 * draws) / n
    # Variance of the per-game score (treats draws as 0.5 outcomes).
    w, d, ls = wins / n, draws / n, losses / n
    mean = w * 1.0 + d * 0.5 + ls * 0.0
    var = w * (1.0 - mean) ** 2 + d * (0.5 - mean) ** 2 + ls * (0.0 - mean) ** 2
    stddev = math.sqrt(var / n) if n > 0 else 0.0

    elo = score_to_elo(score)
    z = phi_inv(0.5 + confidence / 2.0)
    lo = score_to_elo(max(1e-9, score - z * stddev))
    hi = score_to_elo(min(1 - 1e-9, score + z * stddev))
    half = (hi - lo) / 2.0
    # LOS: probability dev is genuinely stronger than base.
    los = 0.5 * (1.0 + math.erf((score - 0.5) / (stddev * math.sqrt(2.0)))) if stddev > 0 \
        else (1.0 if score > 0.5 else 0.0)
    return elo, half, los


def emit_output(name, value):
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as fh:
            fh.write(f"{name}={value}\n")


def main():
    if len(sys.argv) < 2:
        print("usage: elo_report.py GAMES.PGN [DEV_NAME] [BASE_NAME]", file=sys.stderr)
        return 2
    pgn = sys.argv[1]
    dev = sys.argv[2] if len(sys.argv) > 2 else "dev"
    base = sys.argv[3] if len(sys.argv) > 3 else "base"

    if not os.path.exists(pgn):
        print(f"::error::PGN not found: {pgn}")
        emit_output("games", "0")
        emit_output("elo", "0")
        emit_output("error_bar", "0")
        emit_output("los", "0")
        return 0

    wins, draws, losses = parse_games(pgn, dev, base)
    n = wins + draws + losses
    elo, half, los = elo_with_ci(wins, draws, losses)

    print(f"Engine        : {dev}  (PR commit)")
    print(f"Opponent      : {base}  (main / past self)")
    print(f"Games         : {n}  (W:{wins}  D:{draws}  L:{losses})")
    if n:
        print(f"Score         : {(wins + 0.5 * draws) / n * 100:.1f}%")
    print(f"Elo           : {elo:+.1f} +/- {half:.1f}  (95% CI)")
    print(f"LOS           : {los * 100:.1f}%  (chance PR is stronger)")

    emit_output("games", str(n))
    emit_output("wins", str(wins))
    emit_output("draws", str(draws))
    emit_output("losses", str(losses))
    emit_output("elo", f"{elo:.1f}")
    emit_output("error_bar", f"{half:.1f}")
    emit_output("los", f"{los * 100:.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
