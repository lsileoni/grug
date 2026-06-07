import os
import subprocess
import sys

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "server"))

import app as server_app  # noqa: E402
from bench import config as configmod  # noqa: E402


def _run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def _make_repo(tmp_path):
    repo = tmp_path / "repo"
    repo.mkdir()
    _run(["git", "init", "-b", "main"], repo)
    _run(["git", "config", "user.email", "bench@example.invalid"], repo)
    _run(["git", "config", "user.name", "Bench Test"], repo)
    (repo / "engine.txt").write_text("engine\n", encoding="utf-8")
    _run(["git", "add", "engine.txt"], repo)
    _run(["git", "commit", "-m", "initial"], repo)
    return repo, _run(["git", "rev-parse", "HEAD"], repo)


def _client(tmp_path, *, auth_block):
    repo, sha = _make_repo(tmp_path)
    cfg = tmp_path / "config.yaml"
    cfg.write_text(f"""
{auth_block}
engines:
  grug:
    source: "{repo}"
    build: "make"
    binary: "grug"
server:
  database: "{tmp_path / 'bench.db'}"
""", encoding="utf-8")
    app = server_app.create_app(str(cfg))
    app.config.update(TESTING=True)
    return app, app.test_client(), sha


def _auth(token):
    return {"Authorization": f"Bearer {token}"}


SPLIT = "auth:\n  admin_token: admin-secret\n  worker_token: worker-secret\n  session_secret: sess"


def test_server_refuses_placeholder_token(tmp_path):
    with pytest.raises(configmod.ConfigError):
        _client(tmp_path, auth_block='token: "change-me"')


def test_roles_are_separated(tmp_path):
    _, client, sha = _client(tmp_path, auth_block=SPLIT)
    body = {"engine": "grug", "dev_ref": sha, "base_ref": "main"}

    # anonymous and worker token cannot create runs; admin can.
    assert client.post("/api/tests", json=body).status_code == 401
    assert client.post("/api/tests", json=body, headers=_auth("worker-secret")).status_code == 401
    assert client.post("/api/tests", json=body, headers=_auth("admin-secret")).status_code == 200

    # the admin token is not a worker token and vice versa.
    hb = {"name": "w1", "hostname": "h", "cores": 1}
    assert client.post("/api/worker/heartbeat", json=hb, headers=_auth("admin-secret")).status_code == 401
    assert client.post("/api/worker/heartbeat", json=hb, headers=_auth("worker-secret")).status_code == 200


def test_new_form_redirects_anonymous_to_login(tmp_path):
    _, client, _ = _client(tmp_path, auth_block=SPLIT)
    resp = client.get("/new")
    assert resp.status_code == 302
    assert "/login" in resp.headers["Location"]


def test_stop_requires_admin(tmp_path):
    _, client, sha = _client(tmp_path, auth_block=SPLIT)
    tid = client.post("/api/tests", headers=_auth("admin-secret"),
                      json={"engine": "grug", "dev_ref": sha, "base_ref": "main"}).get_json()["id"]
    assert client.post(f"/api/test/{tid}/stop").status_code == 401
    assert client.post(f"/api/test/{tid}/stop", headers=_auth("admin-secret")).status_code == 200


def test_session_login_enforces_csrf(tmp_path):
    _, client, sha = _client(tmp_path, auth_block=SPLIT)
    with client.session_transaction() as s:
        s["csrf"] = "csrf-tok"

    # wrong token does not grant a session.
    bad = client.post("/login", data={"token": "nope", "csrf_token": "csrf-tok"})
    assert bad.status_code == 401

    ok = client.post("/login", data={"token": "admin-secret", "csrf_token": "csrf-tok", "next": "/"})
    assert ok.status_code == 302

    body = {"engine": "grug", "dev_ref": sha, "base_ref": "main"}
    # session present but no CSRF header -> rejected.
    assert client.post("/api/tests", json=body).status_code == 400
    # session + matching CSRF header -> accepted.
    assert client.post("/api/tests", json=body,
                       headers={"X-CSRF-Token": "csrf-tok"}).status_code == 200


def test_invalid_ref_rejected(tmp_path):
    _, client, _ = _client(tmp_path, auth_block=SPLIT)
    resp = client.post("/api/tests", headers=_auth("admin-secret"),
                       json={"engine": "grug", "dev_ref": "--upload-pack=touch /tmp/x",
                             "base_ref": "main"})
    assert resp.status_code == 400
    assert "invalid ref" in resp.get_json()["error"]


def test_book_traversal_rejected(tmp_path):
    _, client, sha = _client(tmp_path, auth_block=SPLIT)
    resp = client.post("/api/tests", headers=_auth("admin-secret"),
                       json={"engine": "grug", "dev_ref": sha, "base_ref": "main",
                             "book": "../../etc/passwd"})
    assert resp.status_code == 400
    assert "opening book" in resp.get_json()["error"]
