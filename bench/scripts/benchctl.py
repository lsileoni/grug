from __future__ import annotations

import argparse
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

import yaml


ROOT = Path(__file__).resolve().parents[1]
RUN_DIR = ROOT / "data" / "run"


def load_config(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise SystemExit(f"config file not found: {path}")
    with path.open("r", encoding="utf-8") as fh:
        cfg = yaml.safe_load(fh) or {}
    if not isinstance(cfg, dict):
        raise SystemExit("config root must be a mapping")
    cfg.setdefault("server", {})
    cfg.setdefault("worker", {})
    return cfg


def apply_overrides(cfg: dict[str, Any], args: argparse.Namespace) -> None:
    server = cfg.setdefault("server", {})
    worker = cfg.setdefault("worker", {})
    for attr, target, key in (
        ("token", cfg, "token"),
        ("host", server, "host"),
        ("port", server, "port"),
        ("database", server, "database"),
        ("batch_pairs", server, "batch_pairs"),
        ("server_url", worker, "server_url"),
        ("concurrency", worker, "concurrency"),
        ("cutechess", worker, "cutechess"),
        ("runner", worker, "runner"),
    ):
        value = getattr(args, attr, None)
        if value is not None:
            target[key] = value

    if args.port is not None and args.server_url is None:
        host = server.get("host", "127.0.0.1")
        worker["server_url"] = f"http://{host}:{args.port}"


def run_name(args: argparse.Namespace) -> str:
    if args.name:
        return args.name
    return Path(args.config).stem


def rendered_config(args: argparse.Namespace) -> Path:
    cfg_path = (ROOT / args.config).resolve() if not os.path.isabs(args.config) else Path(args.config)
    cfg = load_config(cfg_path)
    apply_overrides(cfg, args)
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    out = cfg_path.parent / f".benchctl-{run_name(args)}.yaml"
    with out.open("w", encoding="utf-8") as fh:
        yaml.safe_dump(cfg, fh, sort_keys=False)
    return out


def pid_file(name: str, role: str) -> Path:
    return RUN_DIR / f"{name}-{role}.pid"


def log_file(name: str, role: str) -> Path:
    return RUN_DIR / f"{name}-{role}.log"


def read_pid(path: Path) -> int | None:
    try:
        return int(path.read_text(encoding="utf-8").strip())
    except (FileNotFoundError, ValueError):
        return None


def is_running(pid: int | None) -> bool:
    if not pid:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def command_for(role: str, cfg: Path) -> list[str]:
    script = ROOT / ("server/app.py" if role == "server" else "worker/worker.py")
    return [str(ROOT / ".venv" / "bin" / "python"), str(script), "--config", str(cfg)]


def start_role(role: str, args: argparse.Namespace) -> None:
    name = run_name(args)
    existing = read_pid(pid_file(name, role))
    if is_running(existing):
        print(f"{role} already running: pid {existing}")
        return

    cfg = rendered_config(args)
    log_path = log_file(name, role)
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    with log_path.open("ab") as log:
        proc = subprocess.Popen(
            command_for(role, cfg),
            cwd=ROOT,
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    pid_file(name, role).write_text(f"{proc.pid}\n", encoding="utf-8")
    print(f"started {role}: pid {proc.pid}, log {log_path.relative_to(ROOT)}")


def wait_for_server(args: argparse.Namespace, timeout: float) -> None:
    cfg = load_config(rendered_config(args))
    server = cfg.get("server", {})
    url = f"http://{server.get('host', '127.0.0.1')}:{server.get('port', 8000)}/"
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            urllib.request.urlopen(url, timeout=1).close()
            return
        except (OSError, urllib.error.URLError):
            time.sleep(0.2)
    print(f"server did not answer within {timeout:g}s: {url}", file=sys.stderr)


def stop_role(role: str, args: argparse.Namespace) -> None:
    name = run_name(args)
    path = pid_file(name, role)
    pid = read_pid(path)
    if not is_running(pid):
        print(f"{role} not running")
        path.unlink(missing_ok=True)
        return
    assert pid is not None
    os.kill(pid, signal.SIGTERM)
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        if not is_running(pid):
            path.unlink(missing_ok=True)
            print(f"stopped {role}: pid {pid}")
            return
        time.sleep(0.1)
    os.kill(pid, signal.SIGKILL)
    path.unlink(missing_ok=True)
    print(f"killed {role}: pid {pid}")


def status_role(role: str, args: argparse.Namespace) -> None:
    name = run_name(args)
    path = pid_file(name, role)
    pid = read_pid(path)
    state = "running" if is_running(pid) else "stopped"
    detail = f"pid {pid}" if pid else "no pid"
    print(f"{role}: {state} ({detail})")


def tail_role(role: str, args: argparse.Namespace) -> None:
    path = log_file(run_name(args), role)
    if not path.exists():
        raise SystemExit(f"log file not found: {path}")
    subprocess.run(["tail", "-n", str(args.lines), str(path)], check=False)


def run_foreground(role: str, args: argparse.Namespace) -> None:
    cfg = rendered_config(args)
    os.chdir(ROOT)
    os.execv(command_for(role, cfg)[0], command_for(role, cfg))


def add_common(p: argparse.ArgumentParser) -> None:
    p.add_argument("--config", default="config.yaml", help="config file relative to bench/ or absolute")
    p.add_argument("--name", help="run name for pid/log/generated config files")
    p.add_argument("--token")
    p.add_argument("--host")
    p.add_argument("--port", type=int)
    p.add_argument("--database")
    p.add_argument("--batch-pairs", type=int)
    p.add_argument("--server-url")
    p.add_argument("--concurrency", type=int)
    p.add_argument("--cutechess")
    p.add_argument("--runner")


def parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="Manage Grug Bench server and worker runs")
    sub = ap.add_subparsers(dest="cmd", required=True)

    for cmd in ("server", "worker"):
        p = sub.add_parser(cmd, help=f"run {cmd} in the foreground")
        add_common(p)

    p = sub.add_parser("start", help="start server, worker, or both in the background")
    add_common(p)
    p.add_argument("role", choices=("server", "worker", "both"))
    p.add_argument("--wait", type=float, default=5.0, help="seconds to wait for server before worker")

    p = sub.add_parser("stop", help="stop server, worker, or both")
    add_common(p)
    p.add_argument("role", choices=("server", "worker", "both"), nargs="?", default="both")
    p.add_argument("--timeout", type=float, default=5.0)

    p = sub.add_parser("status", help="show server and worker status")
    add_common(p)

    p = sub.add_parser("logs", help="print recent server or worker log lines")
    add_common(p)
    p.add_argument("role", choices=("server", "worker"))
    p.add_argument("-n", "--lines", type=int, default=80)
    return ap


def main() -> None:
    args = parser().parse_args()
    if args.cmd == "server":
        run_foreground("server", args)
    elif args.cmd == "worker":
        run_foreground("worker", args)
    elif args.cmd == "start":
        if args.role in ("server", "both"):
            start_role("server", args)
        if args.role == "both":
            wait_for_server(args, args.wait)
        if args.role in ("worker", "both"):
            start_role("worker", args)
    elif args.cmd == "stop":
        if args.role in ("worker", "both"):
            stop_role("worker", args)
        if args.role in ("server", "both"):
            stop_role("server", args)
    elif args.cmd == "status":
        status_role("server", args)
        status_role("worker", args)
    elif args.cmd == "logs":
        tail_role(args.role, args)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        host = socket.gethostname()
        print(f"\nstopped on {host}.", file=sys.stderr)
