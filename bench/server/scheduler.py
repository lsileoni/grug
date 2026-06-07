from __future__ import annotations

import json
import threading
from typing import Any, Dict, Optional

import db
import pgnview
from bench import stats

_LOCK = threading.Lock()


def _remaining_pairs(test: Dict[str, Any]) -> Optional[int]:
    cap = test["max_pairs"] or 0
    if cap <= 0:
        return None
    return max(0, cap - db.assigned_pairs(test["id"]))


def assign_work(worker_id: int, batch_pairs: int, stale_timeout_s: int = 1200) -> Optional[Dict[str, Any]]:
    with _LOCK:
        db.requeue_stale_batches(stale_timeout_s)
        test = db.next_pending_test()
        if not test:
            return None

        remaining = _remaining_pairs(test)
        if remaining is not None and remaining <= 0:
            return None
        pairs = batch_pairs if remaining is None else min(batch_pairs, remaining)
        if pairs <= 0:
            return None

        if test["status"] == "pending":
            db.set_test_status(test["id"], "active")

        batch_id = db.create_batch(test["id"], worker_id, pairs)
        return _task_payload(test, batch_id, pairs)


def _task_payload(test: Dict[str, Any], batch_id: int, pairs: int) -> Dict[str, Any]:
    engine = db.get_engine(test["engine"]) or {}
    first = {"kind": "build", "ref": test["dev_ref"], "sha": test["dev_sha"],
             "options": json.loads(test["dev_options"] or "{}")}
    if test["type"] == "gauntlet":
        second = {"kind": "reference", "name": test["reference"],
                  "options": json.loads(test["ref_options"] or "{}")}
    else:
        second = {"kind": "build", "ref": test["base_ref"], "sha": test["base_sha"],
                  "options": json.loads(test["base_options"] or "{}")}
    return {
        "batch_id": batch_id,
        "test_id": test["id"],
        "type": test["type"],
        "tc": test["tc"],
        "book": test["book"],
        "pairs": pairs,
        "engine": {"name": engine.get("name", test["engine"]),
                   "source": engine.get("source", ""),
                   "build": engine.get("build", ""),
                   "binary": engine.get("binary", "")},
        "first": first,
        "second": second,
    }


def ingest(batch_id: int, results: Dict[str, Any]) -> Dict[str, Any]:
    with _LOCK:
        batch = db.get_batch(batch_id)
        if not batch:
            raise KeyError(f"unknown batch {batch_id}")
        if batch["status"] == "done":
            return db.get_test(batch["test_id"])
        db.complete_batch(batch_id, results, games=pgnview.count_games(results.get("pgn", "")))
        return _recompute(batch["test_id"])


def recompute_only(test_id: int) -> Dict[str, Any]:
    with _LOCK:
        return _recompute(test_id)


def _recompute(test_id: int) -> Dict[str, Any]:
    test = db.get_test(test_id)
    if not test:
        raise KeyError(f"unknown test {test_id}")

    penta = [0, 0, 0, 0, 0]
    w = d = l = 0
    for r in db.done_batches(test_id):
        for i, c in enumerate(r.get("pentanomial", [0, 0, 0, 0, 0])):
            penta[i] += c
        ww, dd, ll = r.get("wdl", [0, 0, 0])
        w += ww; d += dd; l += ll

    elo = stats.elo_pentanomial(penta)
    spr = stats.sprt(penta, test["elo0"], test["elo1"], test["alpha"], test["beta"])
    roll = {
        "games": w + d + l,
        "w": w, "d": d, "l": l,
        "p0": penta[0], "p1": penta[1], "p2": penta[2], "p3": penta[3], "p4": penta[4],
        "llr": spr.llr, "elo": elo.elo, "elo_lo": elo.elo_lo, "elo_hi": elo.elo_hi, "los": elo.los,
    }
    db.update_test_rollup(test_id, roll)

    if test["status"] in ("active", "pending"):
        pairs_done = sum(penta)
        cap = test["max_pairs"] or 0
        if test["type"] == "sprt" and spr.finished:
            db.set_test_status(test_id, "finished",
                               "accepted" if spr.state == "accepted" else "rejected")
        elif cap > 0 and pairs_done >= cap:
            db.set_test_status(test_id, "finished",
                               "inconclusive" if test["type"] == "sprt" else "")
    return db.get_test(test_id)
