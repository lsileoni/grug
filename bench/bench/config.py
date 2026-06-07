from __future__ import annotations

import os
from typing import Any, Dict

import yaml


class ConfigError(Exception):
    pass


# Tokens we ship as placeholders; refusing them keeps a default install from
# being world-writable the moment it is exposed.
WEAK_TOKENS = {"", "change-me", "change-me-shared-secret"}


def load(path: str) -> Dict[str, Any]:
    if not os.path.exists(path):
        raise ConfigError(f"config file not found: {path}")
    with open(path, "r", encoding="utf-8") as fh:
        cfg = yaml.safe_load(fh) or {}
    if not isinstance(cfg, dict):
        raise ConfigError("config root must be a mapping")
    cfg.setdefault("token", "change-me")
    cfg.setdefault("engines", {})
    cfg.setdefault("server", {})
    cfg.setdefault("worker", {})
    cfg.setdefault("auth", {})
    _validate(cfg)
    cfg["_dir"] = os.path.dirname(os.path.abspath(path))
    return cfg


def _validate(cfg: Dict[str, Any]) -> None:
    for name, spec in cfg["engines"].items():
        for key in ("source", "build", "binary"):
            if key not in spec:
                raise ConfigError(f"engine '{name}' is missing required key '{key}'")


def resolve(cfg: Dict[str, Any], path: str) -> str:
    if os.path.isabs(path):
        return path
    return os.path.normpath(os.path.join(cfg.get("_dir", "."), path))


def auth(cfg: Dict[str, Any]) -> Dict[str, Any]:
    """Resolve the two role tokens and the session secret.

    Roles are separated so a worker (which executes untrusted, freshly built
    engine binaries) never holds the credential that can create or stop runs.
    A bare top-level ``token`` is honoured as a legacy single-secret fallback
    for both roles so existing single-host installs keep working.
    """
    a = dict(cfg.get("auth", {}))
    legacy = cfg.get("token")
    return {
        "admin": a.get("admin_token") or legacy,
        "worker": a.get("worker_token") or legacy,
        "session_secret": a.get("session_secret"),
        "allow_insecure": bool(a.get("allow_insecure", False)),
    }


def check_secure(resolved: Dict[str, Any]) -> None:
    """Fail closed when a public-facing server would ship a placeholder secret."""
    if resolved.get("allow_insecure"):
        return
    for role in ("admin", "worker"):
        tok = resolved.get(role)
        if not tok or tok in WEAK_TOKENS:
            raise ConfigError(
                f"refusing to start: the {role} token is unset or a known placeholder. "
                "Set auth.admin_token and auth.worker_token to high-entropy secrets "
                "(or set auth.allow_insecure: true for trusted local-only use).")


def server(cfg: Dict[str, Any]) -> Dict[str, Any]:
    s = dict(cfg.get("server", {}))
    s.setdefault("host", "127.0.0.1")
    s.setdefault("port", 8000)
    s.setdefault("database", "data/bench.db")
    s.setdefault("batch_pairs", 25)
    s.setdefault("max_pairs_limit", 0)   # 0 = unbounded; set a ceiling for public servers
    s.setdefault("read_rate_limit", 0)   # per-IP requests/window on heavy read routes; 0 = off
    s.setdefault("read_rate_window", 60)  # rate-limit window in seconds
    return s


def worker(cfg: Dict[str, Any]) -> Dict[str, Any]:
    w = dict(cfg.get("worker", {}))
    w.setdefault("server_url", "http://127.0.0.1:8000")
    w.setdefault("concurrency", 2)
    w.setdefault("cutechess", "cutechess-cli")
    w.setdefault("cache", "data/engines")
    w.setdefault("sources", {})
    w.setdefault("references", {})
    w.setdefault("sandbox", {})
    return w


def engine_source(cfg: Dict[str, Any], name: str, *, for_worker: bool = False) -> str:
    if for_worker:
        override = cfg.get("worker", {}).get("sources", {}).get(name)
        if override:
            return override
    spec = cfg["engines"].get(name)
    if not spec:
        raise ConfigError(f"unknown engine '{name}'")
    return spec["source"]
