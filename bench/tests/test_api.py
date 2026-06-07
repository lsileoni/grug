import json
import os
import subprocess
import sys
from dataclasses import dataclass

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "server"))

import app as server_app  # noqa: E402
import db  # noqa: E402
import pgnview  # noqa: E402


@dataclass
class ApiHarness:
    client: object
    sha: str
    token: str


def _multi_game_pgn(n: int, start_round: int = 1) -> str:
    """A PGN holding ``n`` short games, each opening with an [Event] tag so the
    server's splitter counts them individually."""
    games = []
    for i in range(n):
        white, black = ("A", "B") if i % 2 == 0 else ("B", "A")
        games.append(
            f'[Event "Grug Bench"]\n[Round "{start_round + i}"]\n'
            f'[White "{white}"]\n[Black "{black}"]\n[Result "1-0"]\n\n'
            f'1. e4 e5 2. Nf3 Nc6 1-0\n')
    return "\n".join(games)


def _run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


@pytest.fixture()
def api(tmp_path):
    repo = tmp_path / "repo"
    repo.mkdir()
    _run(["git", "init", "-b", "main"], repo)
    _run(["git", "config", "user.email", "bench@example.invalid"], repo)
    _run(["git", "config", "user.name", "Bench Test"], repo)
    (repo / "engine.txt").write_text("temporary engine repo\n", encoding="utf-8")
    algorithms = repo / "src" / "algorithms"
    algorithms.mkdir(parents=True)
    (algorithms / "basic_search.c").write_text(
        'const Algorithm BasicSearchAlgorithm = {\n    "basic_search",\n};\n',
        encoding="utf-8")
    (algorithms / "first_legal.c").write_text(
        'const Algorithm FirstLegalAlgorithm = {\n    "first_legal",\n};\n',
        encoding="utf-8")
    _run(["git", "add", "engine.txt"], repo)
    _run(["git", "add", "src/algorithms/basic_search.c", "src/algorithms/first_legal.c"], repo)
    _run(["git", "commit", "-m", "initial"], repo)
    sha = _run(["git", "rev-parse", "HEAD"], repo)

    cfg = tmp_path / "config.yaml"
    cfg.write_text(f"""
token: test-token
engines:
  grug:
    source: "{repo}"
    build: "make -j ARCH= EXE=grug"
    binary: "grug"
server:
  database: "{tmp_path / 'bench.db'}"
""", encoding="utf-8")

    app = server_app.create_app(str(cfg))
    app.config.update(TESTING=True)
    return ApiHarness(app.test_client(), sha, "test-token")


def _auth(token: str):
    return {"Authorization": f"Bearer {token}"}


def test_api_create_test_requires_token(api):
    body = {"engine": "grug", "dev_ref": api.sha}

    assert api.client.post("/api/tests", json=body).status_code == 401
    assert api.client.post("/api/tests", json=body, headers=_auth("wrong")).status_code == 401


def test_api_create_sprt_resolves_refs(api):
    resp = api.client.post("/api/tests", headers=_auth(api.token), json={
        "engine": "grug",
        "dev_ref": api.sha,
        "base_ref": "main",
        "elo0": -5,
        "elo1": 0,
        "tc": "nodes=200000",
        "max_pairs": 4000,
    })

    assert resp.status_code == 200
    test_id = resp.get_json()["id"]
    test = api.client.get(f"/api/ci/test/{test_id}", headers=_auth(api.token)).get_json()
    assert test["type"] == "sprt"
    assert test["engine"] == "grug"
    assert test["dev_ref"] == api.sha
    assert test["dev_sha"] == api.sha
    assert test["base_ref"] == "main"
    assert test["base_sha"] == api.sha
    assert test["elo0"] == -5
    assert test["elo1"] == 0
    assert test["tc"] == "nodes=200000"
    assert test["max_pairs"] == 4000


def test_api_create_selftest_assigns_algorithm_options(api):
    resp = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest",
        "engine": "grug",
        "dev_ref": api.sha,
        "base_ref": api.sha,
        "dev_algorithm": "basic_search",
        "base_algorithm": "first_legal",
        "dev_options": {"Threads": 1},
        "base_options": "Move Overhead=20",
        "tc": "nodes=200000",
        "max_pairs": 12,
    })

    assert resp.status_code == 200
    test_id = resp.get_json()["id"]
    test = api.client.get(f"/api/ci/test/{test_id}", headers=_auth(api.token)).get_json()
    assert test["type"] == "selftest"
    assert test["dev_sha"] == api.sha
    assert test["base_sha"] == api.sha
    assert test["dev_options"] == '{"Threads": 1, "Algorithm": "basic_search"}'
    assert test["base_options"] == '{"Move Overhead": "20", "Algorithm": "first_legal"}'

    heartbeat = api.client.post("/api/worker/heartbeat", headers=_auth(api.token), json={
        "name": "worker-1",
        "hostname": "worker-host",
        "cores": 1,
        "has_cutechess": True,
        "has_stockfish": False,
    })
    worker_id = heartbeat.get_json()["worker_id"]
    assigned = api.client.post("/api/worker/request", headers=_auth(api.token), json={
        "worker_id": worker_id,
        "max_pairs": 4,
    }).get_json()["task"]

    assert assigned["test_id"] == test_id
    assert assigned["type"] == "selftest"
    assert assigned["first"]["kind"] == "build"
    assert assigned["second"]["kind"] == "build"
    assert assigned["first"]["options"] == {"Threads": 1, "Algorithm": "basic_search"}
    assert assigned["second"]["options"] == {"Move Overhead": "20", "Algorithm": "first_legal"}


def test_api_create_selftest_defaults_base_ref_to_dev(api):
    resp = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest",
        "engine": "grug",
        "dev_ref": api.sha,
        "dev_algorithm": "basic_search",
        "base_algorithm": "first_legal",
        "max_pairs": 12,
    })

    assert resp.status_code == 200
    test_id = resp.get_json()["id"]
    test = api.client.get(f"/api/ci/test/{test_id}", headers=_auth(api.token)).get_json()
    assert test["base_ref"] == api.sha
    assert test["base_sha"] == api.sha


def test_games_page_shows_submitted_pgn(api):
    created = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest",
        "engine": "grug",
        "dev_ref": api.sha,
        "base_ref": api.sha,
        "max_pairs": 2,
    })
    test_id = created.get_json()["id"]
    heartbeat = api.client.post("/api/worker/heartbeat", headers=_auth(api.token), json={
        "name": "worker-games",
        "hostname": "worker-host",
        "cores": 1,
        "has_cutechess": True,
        "has_stockfish": False,
    })
    worker_id = heartbeat.get_json()["worker_id"]
    task = api.client.post("/api/worker/request", headers=_auth(api.token), json={
        "worker_id": worker_id,
        "max_pairs": 1,
    }).get_json()["task"]

    pgn = """[Event "Grug Bench"]
[Round "1"]
[White "A"]
[Black "B"]
[Result "1-0"]

1. e4 e5 2. Nf3 Nc6 1-0
"""
    submitted = api.client.post("/api/worker/submit", headers=_auth(api.token), json={
        "batch_id": task["batch_id"],
        "worker_id": worker_id,
        "wdl": [1, 0, 0],
        "pentanomial": [0, 0, 0, 0, 1],
        "pgn": pgn,
    })
    assert submitted.status_code == 200

    page = api.client.get(f"/test/{test_id}/games")
    assert page.status_code == 200
    body = page.get_data(as_text=True)
    assert "Games for test" in body
    assert "1. e4 e5 2. Nf3 Nc6 1-0" in body
    assert "<td class=\"mono\">1</td>" in body


def test_worker_option_validation_error_marks_test_failed(api):
    created = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest",
        "engine": "grug",
        "dev_ref": api.sha,
        "base_ref": api.sha,
        "max_pairs": 2,
    })
    test_id = created.get_json()["id"]
    heartbeat = api.client.post("/api/worker/heartbeat", headers=_auth(api.token), json={
        "name": "worker-option-error",
        "hostname": "worker-host",
        "cores": 1,
        "has_cutechess": True,
        "has_stockfish": False,
    })
    worker_id = heartbeat.get_json()["worker_id"]
    task = api.client.post("/api/worker/request", headers=_auth(api.token), json={
        "worker_id": worker_id,
        "max_pairs": 1,
    }).get_json()["task"]

    error = "engine B does not support UCI option(s): Algorithm. Available options: Hash."
    resp = api.client.post("/api/worker/submit", headers=_auth(api.token), json={
        "batch_id": task["batch_id"],
        "worker_id": worker_id,
        "error": error,
    })

    assert resp.status_code == 200
    test = api.client.get(f"/api/ci/test/{test_id}", headers=_auth(api.token)).get_json()
    assert test["status"] == "failed"
    assert test["result"] == error


def test_api_create_test_rejects_invalid_engine_and_ref(api):
    bad_engine = api.client.post("/api/tests", headers=_auth(api.token), json={
        "engine": "missing",
        "dev_ref": api.sha,
    })
    assert bad_engine.status_code == 400
    assert "unknown engine" in bad_engine.get_json()["error"]

    bad_ref = api.client.post("/api/tests", headers=_auth(api.token), json={
        "engine": "grug",
        "dev_ref": "does-not-exist",
        "base_ref": "main",
    })
    assert bad_ref.status_code == 400
    assert "not found" in bad_ref.get_json()["error"]


def test_api_create_rejects_algorithm_missing_from_committed_ref(api):
    resp = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest",
        "engine": "grug",
        "dev_ref": api.sha,
        "dev_algorithm": "not_committed",
        "base_algorithm": "first_legal",
        "max_pairs": 12,
    })

    assert resp.status_code == 400
    assert "not present in committed ref" in resp.get_json()["error"]


def _new_worker(api, name):
    return api.client.post("/api/worker/heartbeat", headers=_auth(api.token), json={
        "name": name, "hostname": "worker-host", "cores": 1,
        "has_cutechess": True, "has_stockfish": False,
    }).get_json()["worker_id"]


def _submit_one_batch(api, worker_id, pgn):
    task = api.client.post("/api/worker/request", headers=_auth(api.token), json={
        "worker_id": worker_id, "max_pairs": 1,
    }).get_json()["task"]
    assert task is not None
    resp = api.client.post("/api/worker/submit", headers=_auth(api.token), json={
        "batch_id": task["batch_id"], "worker_id": worker_id,
        "wdl": [0, 2, 0], "pentanomial": [0, 0, 1, 0, 0], "pgn": pgn,
    })
    assert resp.status_code == 200


def test_games_page_paginates_many_batches(api, monkeypatch):
    created = api.client.post("/api/tests", headers=_auth(api.token), json={
        "type": "selftest", "engine": "grug", "dev_ref": api.sha,
        "base_ref": api.sha, "max_pairs": 1000,
    })
    test_id = created.get_json()["id"]
    worker_id = _new_worker(api, "worker-many")

    per_batch, n_batches = 10, 12
    total = per_batch * n_batches  # 120 games across 12 done batches
    for b in range(n_batches):
        _submit_one_batch(api, worker_id, _multi_game_pgn(per_batch, start_round=b * per_batch + 1))

    per_page = server_app.GAMES_PER_PAGE
    pages = -(-total // per_page)
    assert pages > 1  # the scenario must actually span multiple pages

    # Count the expensive PGN replay so we can prove a page does not parse every
    # game of the test (the DoS the pagination + cache exist to prevent).
    calls = {"n": 0}
    real = pgnview.positions_from_pgn

    def counting(pgn):
        calls["n"] += 1
        return real(pgn)

    monkeypatch.setattr(pgnview, "positions_from_pgn", counting)

    page1 = api.client.get(f"/test/{test_id}/games")
    assert page1.status_code == 200
    body1 = page1.get_data(as_text=True)
    assert f"{total} games from {n_batches} batches" in body1
    assert f"Page 1 of {pages}" in body1
    assert body1.count("Show PGN") == per_page          # only one page of rows rendered
    assert 0 < calls["n"] <= per_page + per_batch        # bounded, not the full 120
    assert calls["n"] < total

    # Re-loading the same page replays nothing: done batches are cached.
    calls["n"] = 0
    again = api.client.get(f"/test/{test_id}/games")
    assert again.status_code == 200
    assert calls["n"] == 0

    # Last page holds just the remainder, and its rows are distinct from page 1.
    last = api.client.get(f"/test/{test_id}/games?page={pages}")
    assert last.status_code == 200
    body_last = last.get_data(as_text=True)
    assert f"Page {pages} of {pages}" in body_last
    assert body_last.count("Show PGN") == total - per_page * (pages - 1)

    # Out-of-range and garbage page values clamp instead of erroring.
    assert api.client.get(f"/test/{test_id}/games?page=9999").status_code == 200
    assert api.client.get(f"/test/{test_id}/games?page=-3").status_code == 200
    assert api.client.get(f"/test/{test_id}/games?page=abc").status_code == 200


def test_games_page_rate_limit(api):
    test_id = api.client.post("/api/tests", headers=_auth(api.token), json={
        "engine": "grug", "dev_ref": api.sha, "base_ref": "main",
    }).get_json()["id"]

    server_app.SRV["read_rate_limit"] = 3
    server_app.SRV["read_rate_window"] = 60
    server_app._RATE_HITS.clear()
    try:
        for _ in range(3):
            assert api.client.get(f"/test/{test_id}/games").status_code == 200
        # The fourth request in the window is turned away before any DB work.
        assert api.client.get(f"/test/{test_id}/games").status_code == 429
    finally:
        server_app.SRV["read_rate_limit"] = 0
        server_app._RATE_HITS.clear()


def test_backfill_counts_legacy_batches(tmp_path):
    # A database from before the cached `games` column: done batches have no count.
    db.configure(str(tmp_path / "legacy.db"))
    conn = db.connect()
    conn.executescript(
        "CREATE TABLE batches(id INTEGER PRIMARY KEY AUTOINCREMENT, test_id INTEGER, "
        "worker_id INTEGER, pairs INTEGER, status TEXT, results TEXT, "
        "assigned_at TEXT, completed_at TEXT);")
    conn.execute("INSERT INTO batches(test_id, pairs, status, results) VALUES(1,1,'done',?)",
                 (json.dumps({"pgn": _multi_game_pgn(3)}),))
    conn.execute("INSERT INTO batches(test_id, pairs, status, results) VALUES(1,1,'assigned','{}')")
    conn.commit()
    conn.close()

    # Migration: add the column (NULL on legacy rows), then backfill done rows.
    conn = db.connect()
    db._ensure_column(conn, "batches", "games", "INTEGER")
    conn.commit()
    conn.close()
    assert db.backfill_batch_game_counts(pgnview.count_games) == 1

    by_status = {c["status"]: c["games"] for c in db.batch_game_counts(1)}
    assert by_status == {"done": 3, "assigned": 0}
    # Idempotent: nothing left to fill on a second pass.
    assert db.backfill_batch_game_counts(pgnview.count_games) == 0


def test_ci_poll_requires_token_and_matches_public_payload(api):
    created = api.client.post("/api/tests", headers=_auth(api.token), json={
        "engine": "grug",
        "dev_ref": api.sha,
        "base_ref": "main",
    })
    test_id = created.get_json()["id"]

    assert api.client.get(f"/api/ci/test/{test_id}").status_code == 401
    ci = api.client.get(f"/api/ci/test/{test_id}", headers=_auth(api.token)).get_json()
    public = api.client.get(f"/api/test/{test_id}").get_json()

    for key in ("id", "type", "engine", "dev_sha", "base_sha", "status", "result", "llr"):
        assert ci[key] == public[key]
