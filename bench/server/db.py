from __future__ import annotations

import json
import os
import sqlite3
from typing import Any, Dict, List, Optional

_DB_PATH: Optional[str] = None
_SCHEMA_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "schema.sql")


def configure(db_path: str) -> None:
    global _DB_PATH
    _DB_PATH = db_path
    os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)


def connect() -> sqlite3.Connection:
    assert _DB_PATH, "db.configure() must be called first"
    conn = sqlite3.connect(_DB_PATH, timeout=30.0)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def init(engines: Dict[str, Dict[str, str]]) -> None:
    with open(_SCHEMA_PATH, "r", encoding="utf-8") as fh:
        schema = fh.read()
    conn = connect()
    try:
        conn.executescript(schema)
        for name, spec in engines.items():
            conn.execute(
                "INSERT INTO engines(name, source, build, binary) VALUES(?,?,?,?) "
                "ON CONFLICT(name) DO UPDATE SET source=excluded.source, "
                "build=excluded.build, binary=excluded.binary",
                (name, spec["source"], spec["build"], spec["binary"]),
            )
        _ensure_column(conn, "tests", "dev_options", "TEXT DEFAULT '{}'")
        _ensure_column(conn, "tests", "base_options", "TEXT DEFAULT '{}'")
        # Cached game count per batch (NULL on legacy rows -> backfilled lazily).
        _ensure_column(conn, "batches", "games", "INTEGER")
        conn.commit()
    finally:
        conn.close()


def _ensure_column(conn: sqlite3.Connection, table: str, column: str, decl: str) -> None:
    cols = {r["name"] for r in conn.execute(f"PRAGMA table_info({table})")}
    if column not in cols:
        conn.execute(f"ALTER TABLE {table} ADD COLUMN {column} {decl}")


def _row(r: Optional[sqlite3.Row]) -> Optional[Dict[str, Any]]:
    return dict(r) if r is not None else None


def get_engine(name: str) -> Optional[Dict[str, Any]]:
    conn = connect()
    try:
        return _row(conn.execute("SELECT * FROM engines WHERE name=?", (name,)).fetchone())
    finally:
        conn.close()


def list_engines() -> List[Dict[str, Any]]:
    conn = connect()
    try:
        return [dict(r) for r in conn.execute("SELECT * FROM engines ORDER BY name")]
    finally:
        conn.close()


def create_test(fields: Dict[str, Any]) -> int:
    cols = ", ".join(fields.keys())
    qs = ", ".join("?" for _ in fields)
    conn = connect()
    try:
        cur = conn.execute(f"INSERT INTO tests({cols}) VALUES({qs})", tuple(fields.values()))
        conn.commit()
        return int(cur.lastrowid)
    finally:
        conn.close()


def get_test(test_id: int) -> Optional[Dict[str, Any]]:
    conn = connect()
    try:
        return _row(conn.execute("SELECT * FROM tests WHERE id=?", (test_id,)).fetchone())
    finally:
        conn.close()


def list_tests() -> List[Dict[str, Any]]:
    conn = connect()
    try:
        return [dict(r) for r in conn.execute(
            "SELECT * FROM tests ORDER BY "
            "CASE status WHEN 'active' THEN 0 WHEN 'pending' THEN 1 ELSE 2 END, id DESC")]
    finally:
        conn.close()


def set_test_status(test_id: int, status: str, result: str = None) -> None:
    conn = connect()
    try:
        if result is None:
            conn.execute("UPDATE tests SET status=?, updated_at=datetime('now') WHERE id=?",
                         (status, test_id))
        else:
            conn.execute("UPDATE tests SET status=?, result=?, updated_at=datetime('now') WHERE id=?",
                         (status, result, test_id))
        conn.commit()
    finally:
        conn.close()


def update_test_rollup(test_id: int, roll: Dict[str, Any]) -> None:
    keys = ("games", "w", "d", "l", "p0", "p1", "p2", "p3", "p4",
            "llr", "elo", "elo_lo", "elo_hi", "los")
    sets = ", ".join(f"{k}=?" for k in keys)
    conn = connect()
    try:
        conn.execute(
            f"UPDATE tests SET {sets}, updated_at=datetime('now') WHERE id=?",
            tuple(roll[k] for k in keys) + (test_id,),
        )
        conn.commit()
    finally:
        conn.close()


def next_pending_test() -> Optional[Dict[str, Any]]:
    conn = connect()
    try:
        r = conn.execute(
            "SELECT * FROM tests WHERE status IN ('active','pending') "
            "ORDER BY CASE status WHEN 'active' THEN 0 ELSE 1 END, priority DESC, id "
            "LIMIT 1").fetchone()
        return _row(r)
    finally:
        conn.close()


def assigned_pairs(test_id: int) -> int:
    conn = connect()
    try:
        r = conn.execute(
            "SELECT COALESCE(SUM(pairs),0) AS n FROM batches "
            "WHERE test_id=? AND status IN ('assigned','done')", (test_id,)).fetchone()
        return int(r["n"])
    finally:
        conn.close()


def create_batch(test_id: int, worker_id: int, pairs: int) -> int:
    conn = connect()
    try:
        cur = conn.execute(
            "INSERT INTO batches(test_id, worker_id, pairs, status) VALUES(?,?,?, 'assigned')",
            (test_id, worker_id, pairs))
        conn.commit()
        return int(cur.lastrowid)
    finally:
        conn.close()


def get_batch(batch_id: int) -> Optional[Dict[str, Any]]:
    conn = connect()
    try:
        return _row(conn.execute("SELECT * FROM batches WHERE id=?", (batch_id,)).fetchone())
    finally:
        conn.close()


def complete_batch(batch_id: int, results: Dict[str, Any], games: int = 0) -> None:
    """Mark a batch done. ``games`` caches the number of games in its PGN so the
    games page can paginate without re-parsing every batch (see scheduler.ingest)."""
    conn = connect()
    try:
        conn.execute(
            "UPDATE batches SET status='done', results=?, games=?, "
            "completed_at=datetime('now') WHERE id=?",
            (json.dumps(results), int(games), batch_id))
        conn.commit()
    finally:
        conn.close()


def fail_batch(batch_id: int, reason: str = "") -> None:
    conn = connect()
    try:
        conn.execute(
            "UPDATE batches SET status='failed', results=?, completed_at=datetime('now') WHERE id=?",
            (json.dumps({"error": reason}), batch_id))
        conn.commit()
    finally:
        conn.close()


def done_batches(test_id: int) -> List[Dict[str, Any]]:
    conn = connect()
    try:
        rows = conn.execute(
            "SELECT results FROM batches WHERE test_id=? AND status='done'", (test_id,)).fetchall()
        return [json.loads(r["results"]) for r in rows]
    finally:
        conn.close()


def batch_game_counts(test_id: int) -> List[Dict[str, Any]]:
    """Per-batch game tally for a test, in id order, without loading any PGN.

    Lets the games page size pagination and locate which batches a page spans
    cheaply; the actual PGN is parsed only for the batches on the page."""
    conn = connect()
    try:
        rows = conn.execute(
            "SELECT id, status, COALESCE(games, 0) AS games "
            "FROM batches WHERE test_id=? ORDER BY id", (test_id,)).fetchall()
        return [dict(r) for r in rows]
    finally:
        conn.close()


def backfill_batch_game_counts(counter) -> int:
    """Populate the cached game count for done batches predating the column.

    ``counter`` maps a PGN string to a game count. Runs once per legacy batch
    (rows where games IS NULL), so subsequent startups are no-ops."""
    conn = connect()
    try:
        rows = conn.execute(
            "SELECT id, results FROM batches "
            "WHERE status='done' AND games IS NULL").fetchall()
        for r in rows:
            try:
                pgn = (json.loads(r["results"] or "{}")).get("pgn", "")
            except json.JSONDecodeError:
                pgn = ""
            conn.execute("UPDATE batches SET games=? WHERE id=?", (int(counter(pgn)), r["id"]))
        if rows:
            conn.commit()
        return len(rows)
    finally:
        conn.close()


def requeue_stale_batches(timeout_s: int) -> int:
    conn = connect()
    try:
        cur = conn.execute(
            "UPDATE batches SET status='failed', "
            "results=json_object('error','stale'), completed_at=datetime('now') "
            "WHERE status='assigned' AND assigned_at <= datetime('now', ?)",
            (f"-{int(timeout_s)} seconds",))
        conn.commit()
        return cur.rowcount
    finally:
        conn.close()


def upsert_worker(name: str, hostname: str, cores: int,
                  has_cutechess: bool, has_stockfish: bool) -> int:
    conn = connect()
    try:
        conn.execute(
            "INSERT INTO workers(name, hostname, cores, has_cutechess, has_stockfish, last_seen) "
            "VALUES(?,?,?,?,?, datetime('now')) "
            "ON CONFLICT(name) DO UPDATE SET hostname=excluded.hostname, cores=excluded.cores, "
            "has_cutechess=excluded.has_cutechess, has_stockfish=excluded.has_stockfish, "
            "last_seen=datetime('now')",
            (name, hostname, cores, int(has_cutechess), int(has_stockfish)))
        conn.commit()
        r = conn.execute("SELECT id FROM workers WHERE name=?", (name,)).fetchone()
        return int(r["id"])
    finally:
        conn.close()


def add_worker_games(worker_id: int, games: int) -> None:
    conn = connect()
    try:
        conn.execute("UPDATE workers SET games_done = games_done + ?, last_seen=datetime('now') "
                     "WHERE id=?", (games, worker_id))
        conn.commit()
    finally:
        conn.close()


def list_workers() -> List[Dict[str, Any]]:
    conn = connect()
    try:
        return [dict(r) for r in conn.execute("SELECT * FROM workers ORDER BY last_seen DESC")]
    finally:
        conn.close()
