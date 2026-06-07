from __future__ import annotations

import argparse
import functools
import glob
import hmac
import json
import os
import re
import secrets
import socket
import sys
import threading
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)

from flask import (Flask, abort, jsonify, redirect, render_template,  # noqa: E402
                   request, session, url_for)

import db                                     # noqa: E402
import pgnview                                # noqa: E402
import scheduler                              # noqa: E402
from bench import config as configmod         # noqa: E402
from bench import gitutil, stats              # noqa: E402

app = Flask(__name__)
CFG: dict = {}
SRV: dict = {}
AUTH: dict = {}

_SAFE_METHODS = {"GET", "HEAD", "OPTIONS"}
_BOOK_RE = re.compile(r"^[A-Za-z0-9._-]+$")
_TC_RE = re.compile(r"^[A-Za-z0-9 .+=/_:-]{1,64}$")

# Games are parsed and paginated, never replayed in bulk on a request. See
# games_page / _batch_games below.
GAMES_PER_PAGE = 50

# Per-IP fixed-window rate limiter for the heavy read routes. Off unless
# server.read_rate_limit is set. Production runs a single worker process
# (server/wsgi.py), so this in-process counter sees every request; a
# reverse-proxy limit_req is still the recommended outer defence (SECURITY.md).
# State is cleared in create_app so it never leaks across configs/tests.
_RATE_LOCK = threading.Lock()
_RATE_HITS: dict = {}


def _bearer() -> str:
    auth = request.headers.get("Authorization", "")
    return auth[7:] if auth.startswith("Bearer ") else ""


def _token_ok(expected: str) -> bool:
    return bool(expected) and hmac.compare_digest(_bearer(), expected)


def _csrf_ok() -> bool:
    sent = request.headers.get("X-CSRF-Token") or request.form.get("csrf_token", "")
    want = session.get("csrf", "")
    return bool(want) and hmac.compare_digest(sent, want)


def _admin_kind():
    """Return 'session', 'bearer', or None for the current request's admin auth."""
    if session.get("admin"):
        return "session"
    if _token_ok(AUTH.get("admin")):
        return "bearer"
    return None


def require_worker(fn):
    """Worker protocol endpoints: authenticated with the worker-scoped token only."""
    @functools.wraps(fn)
    def wrapper(*a, **k):
        if not _token_ok(AUTH.get("worker")):
            abort(401)
        return fn(*a, **k)
    return wrapper


def require_admin(*, page: bool = False):
    """Gate run-control endpoints behind an admin session or the admin bearer token.

    Browser (cookie) sessions additionally require a CSRF token on unsafe
    methods; the bearer path (CI/scripts) is exempt because an attacker's page
    cannot set an Authorization header cross-origin.
    """
    def decorator(fn):
        @functools.wraps(fn)
        def wrapper(*a, **k):
            kind = _admin_kind()
            if not kind:
                if page:
                    return redirect(url_for("login", next=request.full_path))
                abort(401)
            if kind == "session" and request.method not in _SAFE_METHODS and not _csrf_ok():
                abort(400)
            return fn(*a, **k)
        return wrapper
    return decorator


@app.before_request
def _ensure_csrf():
    if "csrf" not in session:
        session["csrf"] = secrets.token_urlsafe(32)


@app.context_processor
def _template_globals():
    return {"csrf_token": session.get("csrf", ""), "is_admin": bool(session.get("admin"))}


def _safe_next(target: str) -> str:
    # Only allow same-site relative paths so ?next= can't become an open redirect.
    if target and target.startswith("/") and not target.startswith("//"):
        return target
    return url_for("index")


@app.route("/login", methods=["GET", "POST"])
def login():
    nxt = _safe_next(request.values.get("next", ""))
    if request.method == "POST":
        if not _csrf_ok():
            abort(400)
        token = request.form.get("token", "")
        if AUTH.get("admin") and hmac.compare_digest(token, AUTH["admin"]):
            session["admin"] = True
            return redirect(nxt)
        return render_template("login.html", error="Invalid token.", next=nxt), 401
    if session.get("admin"):
        return redirect(nxt)
    return render_template("login.html", next=nxt)


@app.route("/logout", methods=["POST"])
def logout():
    if not _csrf_ok():
        abort(400)
    session.pop("admin", None)
    return redirect(url_for("index"))


def _view(test: dict) -> dict:
    t = dict(test)
    lo, hi = stats.sprt_bounds(test["alpha"], test["beta"])
    t["llr_lower"], t["llr_upper"] = lo, hi
    t["pairs_done"] = (test["w"] + test["d"] + test["l"]) // 2
    t["penta"] = [test["p0"], test["p1"], test["p2"], test["p3"], test["p4"]]
    if test["max_pairs"]:
        t["progress"] = min(100, round(100 * t["pairs_done"] / test["max_pairs"]))
    else:
        span = hi - lo
        t["progress"] = min(100, max(0, round(100 * (test["llr"] - lo) / span))) if span else 0
    return t


def _refs_for(source: str):
    try:
        if gitutil.is_url(source):
            out = gitutil._run(["git", "ls-remote", "--heads", source])
            return [ln.split("refs/heads/")[-1] for ln in out.splitlines() if "refs/heads/" in ln]
        out = gitutil._run(["git", "-C", source, "for-each-ref",
                            "--format=%(refname:short)", "refs/heads/"])
        return out.splitlines()
    except Exception:
        return []


def _algorithms_for(source: str):
    if gitutil.is_url(source):
        return []
    names = set()
    for path in glob.glob(os.path.join(source, "src", "algorithms", "*.c")):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        for match in re.finditer(r"const\s+Algorithm\s+\w+\s*=\s*\{\s*\"([^\"]+)\"", text):
            names.add(match.group(1))
    return sorted(names)


def _algorithms_for_ref(source: str, ref: str):
    if gitutil.is_url(source) or not ref:
        return None
    try:
        paths = gitutil._run(["git", "-C", source, "ls-tree", "-r", "--name-only",
                              ref, "--", "src/algorithms"]).splitlines()
    except Exception:
        return set()
    names = set()
    for path in paths:
        if not path.endswith(".c"):
            continue
        try:
            text = gitutil._run(["git", "-C", source, "show", f"{ref}:{path}"])
        except Exception:
            continue
        for match in re.finditer(r"const\s+Algorithm\s+\w+\s*=\s*\{\s*\"([^\"]+)\"",
                                 text, re.S):
            names.add(match.group(1))
    return names


def _validate_algorithm_ref(source: str, ref: str, side: str, options: dict) -> None:
    algorithm = options.get("Algorithm")
    if not algorithm:
        return
    names = _algorithms_for_ref(source, ref)
    if names is None or algorithm in names:
        return
    available = ", ".join(sorted(names)) if names else "none"
    raise ValueError(
        f"{side} algorithm '{algorithm}' is not present in committed ref '{ref}'. "
        f"Available algorithms at that ref: {available}. Commit the algorithm changes "
        "or choose a ref that contains them.")


def _str_field(data, key: str, default: str = "") -> str:
    value = data.get(key, default)
    if value is None:
        return ""
    return str(value).strip()


def _int_field(data, key: str, default: int = 0) -> int:
    value = data.get(key, default)
    if value in (None, ""):
        value = default
    return int(value)


def _float_field(data, key: str, default: float = 0.0) -> float:
    value = data.get(key, default)
    if value in (None, ""):
        value = default
    return float(value)


def _clamp_pairs(max_pairs: int, ttype: str) -> int:
    """Apply the server's pair ceiling so one run can't monopolise the workers.

    A ceiling of 0 means unbounded (the original behaviour). When a ceiling is
    set, an "until decided" SPRT (max_pairs <= 0) is capped at the ceiling;
    gauntlet/self-test still require their own explicit cap below.
    """
    limit = int(SRV.get("max_pairs_limit") or 0)
    if limit <= 0:
        return max_pairs
    if max_pairs <= 0:
        return limit if ttype == "sprt" else max_pairs
    return min(max_pairs, limit)


def _build_test_fields(data, *, default_base_ref: str = "") -> dict:
    ttype = _str_field(data, "type", "sprt") or "sprt"
    if ttype not in ("sprt", "gauntlet", "selftest"):
        raise ValueError(f"unknown test type '{ttype}'")
    engine = _str_field(data, "engine")
    eng = db.get_engine(engine)
    if not eng:
        raise ValueError(f"unknown engine '{engine}'")

    tc = _str_field(data, "tc", "8+0.08") or "8+0.08"
    if not _TC_RE.match(tc):
        raise ValueError(f"invalid time control '{tc}'")
    book = _str_field(data, "book", "starter.epd") or "starter.epd"
    if not _BOOK_RE.match(book):
        raise ValueError(
            f"invalid opening book '{book}': use a plain file name in the books "
            "directory (letters, digits, '. _ -' only)")
    fields = {
        "type": ttype, "engine": engine, "note": _str_field(data, "note"),
        "tc": tc,
        "book": book,
        "max_pairs": _clamp_pairs(_int_field(data, "max_pairs", 0), ttype),
        "priority": _int_field(data, "priority", 0),
    }

    dev_ref = _str_field(data, "dev_ref")
    fields["dev_ref"] = dev_ref
    fields["dev_sha"] = gitutil.resolve_ref(eng["source"], dev_ref)
    dev_options = _side_options(data, "dev_options", "dev_algorithm")
    _validate_algorithm_ref(eng["source"], fields["dev_sha"], "engine A", dev_options)
    fields["dev_options"] = json.dumps(dev_options)
    if ttype == "sprt":
        base_ref = _str_field(data, "base_ref", default_base_ref)
        fields["base_ref"] = base_ref
        fields["base_sha"] = gitutil.resolve_ref(eng["source"], base_ref)
        base_options = _side_options(data, "base_options", "base_algorithm")
        _validate_algorithm_ref(eng["source"], fields["base_sha"], "engine B", base_options)
        fields["base_options"] = json.dumps(base_options)
        fields["elo0"] = _float_field(data, "elo0", 0.0)
        fields["elo1"] = _float_field(data, "elo1", 5.0)
        fields["alpha"] = _float_field(data, "alpha", 0.05)
        fields["beta"] = _float_field(data, "beta", 0.05)
    elif ttype == "selftest":
        base_ref = _str_field(data, "base_ref", dev_ref)
        fields["base_ref"] = base_ref
        fields["base_sha"] = gitutil.resolve_ref(eng["source"], base_ref)
        base_options = _side_options(data, "base_options", "base_algorithm")
        _validate_algorithm_ref(eng["source"], fields["base_sha"], "engine B", base_options)
        fields["base_options"] = json.dumps(base_options)
        if fields["max_pairs"] <= 0:
            raise ValueError("a self-test needs a game-pair cap (max_pairs > 0)")
    else:
        fields["reference"] = _str_field(data, "reference", "stockfish") or "stockfish"
        fields["ref_options"] = json.dumps(_parse_options(data.get("ref_options", "")))
        if fields["max_pairs"] <= 0:
            raise ValueError("a gauntlet needs a game-pair cap (max_pairs > 0)")
    return fields


@app.route("/")
def index():
    tests = [_view(t) for t in db.list_tests()]
    return render_template("index.html", tests=tests)


@app.route("/test/<int:test_id>")
def test_page(test_id):
    t = db.get_test(test_id)
    if not t:
        abort(404)
    return render_template("test.html", test=_view(t))


def _read_rate_limited() -> bool:
    """True if the caller has exceeded the heavy-read budget this window.

    Fixed-window per-IP counter; disabled when server.read_rate_limit is
    unset/<=0. Reliable under the single-process production server, but a
    reverse-proxy limit also belongs in front of a public deploy (SECURITY.md).
    """
    limit = int(SRV.get("read_rate_limit") or 0)
    if limit <= 0:
        return False
    window = max(1, int(SRV.get("read_rate_window") or 60))
    bucket = int(time.time()) // window
    ip = request.remote_addr or "?"
    with _RATE_LOCK:
        # Drop counters from earlier windows so the dict stays small.
        stale = [k for k in _RATE_HITS if k[1] != bucket]
        for k in stale:
            del _RATE_HITS[k]
        key = (ip, bucket)
        count = _RATE_HITS.get(key, 0) + 1
        _RATE_HITS[key] = count
        return count > limit


@app.route("/test/<int:test_id>/games")
def games_page(test_id):
    if _read_rate_limited():
        abort(429)
    t = db.get_test(test_id)
    if not t:
        abort(404)
    counts = db.batch_game_counts(test_id)
    total_games = sum(c["games"] for c in counts)
    total_batches = len(counts)
    pages = max(1, -(-total_games // GAMES_PER_PAGE))  # ceil
    page = min(max(1, _safe_page(request.args.get("page"))), pages)
    offset = (page - 1) * GAMES_PER_PAGE
    games = _games_page(counts, offset, GAMES_PER_PAGE)
    return render_template("games.html", test=_view(t), games=games,
                           total_games=total_games, total_batches=total_batches,
                           page=page, pages=pages, per_page=GAMES_PER_PAGE)


@app.route("/workers")
def workers_page():
    return render_template("workers.html", workers=db.list_workers())


@app.route("/new", methods=["GET", "POST"])
@require_admin(page=True)
def new_test():
    engines = db.list_engines()
    if request.method == "GET":
        refs = {e["name"]: _refs_for(e["source"]) for e in engines}
        algorithms = {e["name"]: _algorithms_for(e["source"]) for e in engines}
        return render_template("new_test.html", engines=engines, refs=refs, algorithms=algorithms)

    f = request.form
    try:
        fields = _build_test_fields(f)
    except ValueError as e:
        refs = {en["name"]: _refs_for(en["source"]) for en in engines}
        algorithms = {en["name"]: _algorithms_for(en["source"]) for en in engines}
        return render_template("new_test.html", engines=engines, refs=refs,
                               algorithms=algorithms, error=str(e)), 400

    test_id = db.create_test(fields)
    return redirect(url_for("test_page", test_id=test_id))


def _parse_options(text: str) -> dict:
    if isinstance(text, dict):
        return text
    text = text.strip()
    if not text:
        return {}
    if text.startswith("{"):
        return json.loads(text)
    opts = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        opts[k.strip()] = v.strip()
    return opts


def _side_options(data, options_key: str, algorithm_key: str) -> dict:
    opts = _parse_options(data.get(options_key, ""))
    algorithm = _str_field(data, algorithm_key)
    if algorithm:
        opts["Algorithm"] = algorithm
    return opts


@app.route("/api/tests", methods=["GET"])
def api_tests():
    return jsonify([_view(t) for t in db.list_tests()])


@app.route("/api/tests", methods=["POST"])
@require_admin()
def api_create_test():
    try:
        fields = _build_test_fields(request.get_json(force=True) or {}, default_base_ref="main")
    except (TypeError, ValueError) as e:
        return jsonify({"error": str(e)}), 400
    test_id = db.create_test(fields)
    return jsonify({"id": test_id})


@app.route("/api/test/<int:test_id>")
def api_test(test_id):
    t = db.get_test(test_id)
    if not t:
        abort(404)
    return jsonify(_view(t))


@app.route("/api/ci/test/<int:test_id>")
@require_admin()
def api_ci_test(test_id):
    t = db.get_test(test_id)
    if not t:
        abort(404)
    return jsonify(_view(t))


@app.route("/api/test/<int:test_id>/stop", methods=["POST"])
@require_admin()
def api_stop(test_id):
    if not db.get_test(test_id):
        abort(404)
    db.set_test_status(test_id, "stopped")
    return jsonify({"ok": True})


@app.route("/api/worker/heartbeat", methods=["POST"])
@require_worker
def worker_heartbeat():
    j = request.get_json(force=True)
    wid = db.upsert_worker(
        name=j["name"], hostname=j.get("hostname", ""), cores=int(j.get("cores", 0)),
        has_cutechess=bool(j.get("has_cutechess")), has_stockfish=bool(j.get("has_stockfish")))
    return jsonify({"worker_id": wid})


@app.route("/api/worker/request", methods=["POST"])
@require_worker
def worker_request():
    j = request.get_json(force=True)
    worker_id = int(j["worker_id"])
    pairs = int(j.get("max_pairs") or SRV["batch_pairs"])
    task = scheduler.assign_work(worker_id, pairs)
    if not task:
        return jsonify({"task": None})
    return jsonify({"task": task})


@app.route("/api/worker/submit", methods=["POST"])
@require_worker
def worker_submit():
    j = request.get_json(force=True)
    batch_id = int(j["batch_id"])
    worker_id = int(j.get("worker_id", 0))
    if j.get("error"):
        error = str(j["error"])
        batch = db.get_batch(batch_id)
        db.fail_batch(batch_id, error)
        if batch and "does not support UCI option" in error:
            db.set_test_status(batch["test_id"], "failed", error)
        return jsonify({"ok": True, "test": None})
    results = {
        "wdl": j.get("wdl", [0, 0, 0]),
        "pentanomial": j.get("pentanomial", [0, 0, 0, 0, 0]),
        "crashes": j.get("crashes", 0),
        "elapsed": j.get("elapsed", 0),
        "pgn": j.get("pgn", ""),
    }
    test = scheduler.ingest(batch_id, results)
    if worker_id:
        db.add_worker_games(worker_id, sum(results["wdl"]))
    return jsonify({"ok": True, "test": _view(test) if test else None})


def _safe_page(raw) -> int:
    try:
        return int(raw)
    except (TypeError, ValueError):
        return 1


@functools.lru_cache(maxsize=256)
def _batch_games(batch_id: int) -> tuple:
    """Parsed games for one batch, memoised.

    A batch's results are immutable once it is 'done' (scheduler.ingest never
    re-completes), so the expensive PGN replay runs at most once per batch and
    the small LRU keeps memory bounded. The cache is cleared in create_app so
    ids from a previous database can't leak across configs/tests.
    """
    batch = db.get_batch(batch_id)
    if not batch or batch["status"] != "done":
        return ()
    try:
        pgn = (json.loads(batch["results"] or "{}")).get("pgn", "")
    except json.JSONDecodeError:
        return ()
    return tuple(pgnview.game_record(batch_id, idx, game)
                 for idx, game in enumerate(pgnview.split_pgn_games(pgn), start=1))


def _games_page(counts: list[dict], offset: int, limit: int) -> list[dict]:
    """Return the games in window [offset, offset+limit), parsing only the
    batches that overlap it. ``counts`` is the per-batch game tally in id order
    (db.batch_game_counts), so we can map global game positions to batches
    without touching PGN for batches outside the page."""
    end = offset + limit
    games: list[dict] = []
    seen = 0  # global index of the first game in the current batch
    for c in counts:
        n = c["games"]
        if n == 0:
            continue
        start = seen
        seen += n
        if start >= end:
            break
        if seen <= offset:
            continue
        batch_games = _batch_games(c["id"])
        lo = max(0, offset - start)
        hi = min(n, end - start)
        for i in range(lo, min(hi, len(batch_games))):
            g = dict(batch_games[i])
            g["number"] = start + i + 1
            games.append(g)
    return games


def create_app(config_path: str) -> Flask:
    global CFG, SRV, AUTH
    CFG = configmod.load(config_path)
    SRV = configmod.server(CFG)
    AUTH = configmod.auth(CFG)
    configmod.check_secure(AUTH)
    secret = AUTH.get("session_secret")
    if not secret:
        secret = secrets.token_hex(32)
        print("warning: no auth.session_secret set; login sessions will not "
              "survive a restart. Set one for production.", file=sys.stderr)
    app.secret_key = secret
    app.config.update(
        SESSION_COOKIE_HTTPONLY=True,
        SESSION_COOKIE_SAMESITE="Lax",
        SESSION_COOKIE_SECURE=bool(SRV.get("secure_cookies", False)),
    )
    SRV["database"] = configmod.resolve(CFG, SRV["database"])
    db.configure(SRV["database"])
    db.init(CFG["engines"])
    db.backfill_batch_game_counts(pgnview.count_games)
    _batch_games.cache_clear()
    _RATE_HITS.clear()
    return app


def main():
    ap = argparse.ArgumentParser(description="Grug Bench server")
    ap.add_argument("--config", default=os.environ.get("BENCH_CONFIG", "config.yaml"))
    args = ap.parse_args()
    create_app(args.config)
    print(f"Grug Bench: http://{SRV['host']}:{SRV['port']}  (hostname {socket.gethostname()})")
    app.run(host=SRV["host"], port=SRV["port"], threaded=True)


if __name__ == "__main__":
    main()
