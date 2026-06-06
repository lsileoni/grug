from __future__ import annotations

import os
import random
import re
import subprocess
import time
from typing import Any, Dict, List, Optional, Sequence

NAME_A = "A"
NAME_B = "B"


def detect_runner(path: str, override: Optional[str] = None) -> str:
    if override:
        return override
    return "fastchess" if "fastchess" in os.path.basename(path).lower() else "cutechess"


def _engine_args(name: str, cmd: str, options: Optional[Dict[str, Any]]) -> List[str]:
    args = ["-engine", f"name={name}", f"cmd={cmd}", "proto=uci"]
    for k, v in (options or {}).items():
        args.append(f"option.{k}={v}")
    return args


def run_match(cutechess: str, cmd_a: str, cmd_b: str, *,
              options_a: Dict[str, Any] = None, options_b: Dict[str, Any] = None,
              tc: str, book: str, pairs: int, concurrency: int,
              workdir: str, hash_mb: int = 16, runner: Optional[str] = None,
              draw=(40, 8, 10), resign=(4, 600),
              sandbox=None, ro_mounts: Sequence[str] = ()) -> Dict[str, Any]:
    os.makedirs(workdir, exist_ok=True)
    pgn = os.path.join(workdir, "games.pgn")
    if os.path.exists(pgn):
        os.remove(pgn)

    which = detect_runner(cutechess, runner)
    opt_a = {"Hash": hash_mb, **(options_a or {})}
    opt_b = {"Hash": hash_mb, **(options_b or {})}
    tc_token = tc if "=" in tc else f"tc={tc}"

    cmd = [cutechess]
    cmd += _engine_args(NAME_A, cmd_a, opt_a)
    cmd += _engine_args(NAME_B, cmd_b, opt_b)
    cmd += ["-each", tc_token]
    cmd += ["-openings", f"file={book}", "format=epd", "order=random"]
    cmd += ["-srand", str(random.randint(1, 2**31 - 1))]
    cmd += ["-games", "2", "-rounds", str(pairs), "-repeat"]
    cmd += ["-concurrency", str(max(1, concurrency))]
    cmd += ["-draw", f"movenumber={draw[0]}", f"movecount={draw[1]}", f"score={draw[2]}"]
    cmd += ["-resign", f"movecount={resign[0]}", f"score={resign[1]}", "twosided=true"]
    cmd += ["-recover"]
    cmd += ["-pgnout", f"file={pgn}"] if which == "fastchess" else ["-pgnout", pgn]

    t0 = time.time()
    if sandbox is None:
        proc = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True)
    else:
        proc = sandbox.exec(cmd, ro=ro_mounts, workdir=workdir)
    elapsed = time.time() - t0
    if proc.returncode != 0 and not os.path.exists(pgn):
        raise RuntimeError(f"cutechess failed (rc={proc.returncode}):\n"
                           f"{proc.stdout[-1500:]}\n{proc.stderr[-1500:]}")

    wdl, penta = score_pgn(pgn)
    crashes = _count_crashes(proc.stdout + proc.stderr)
    pgn_text = ""
    if os.path.exists(pgn):
        with open(pgn, "r", encoding="utf-8", errors="replace") as fh:
            pgn_text = fh.read()
    return {"wdl": wdl, "pentanomial": penta, "crashes": crashes,
            "elapsed": round(elapsed, 1), "pgn": pgn_text}


_CRASH_RE = re.compile(r"(disconnect|terminated|stall|illegal|crash)", re.I)


def _count_crashes(text: str) -> int:
    return sum(1 for ln in text.splitlines() if _CRASH_RE.search(ln))


_TAG_RE = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')


def score_pgn(pgn_path: str):
    rounds: Dict[str, List[float]] = {}
    w = d = l = 0
    if not os.path.exists(pgn_path):
        return [0, 0, 0], [0, 0, 0, 0, 0]

    cur: Dict[str, str] = {}
    with open(pgn_path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = _TAG_RE.match(line)
            if m:
                cur[m.group(1)] = m.group(2)
                if m.group(1) == "Result":
                    a_score = _score_for_a(cur)
                    if a_score is not None:
                        if a_score == 1.0:
                            w += 1
                        elif a_score == 0.5:
                            d += 1
                        else:
                            l += 1
                        rounds.setdefault(cur.get("Round", ""), []).append(a_score)
                    cur = {}

    penta = [0, 0, 0, 0, 0]
    for scores in rounds.values():
        if len(scores) == 2:
            penta[int(round(sum(scores) * 2))] += 1
    return [w, d, l], penta


def _score_for_a(tags: Dict[str, str]) -> Optional[float]:
    result = tags.get("Result", "*")
    white = tags.get("White", "")
    if result == "*":
        return None
    if result == "1/2-1/2":
        return 0.5
    white_won = result == "1-0"
    a_is_white = white == NAME_A
    a_won = white_won == a_is_white
    return 1.0 if a_won else 0.0
