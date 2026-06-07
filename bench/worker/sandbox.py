"""Isolation for the two operations that run untrusted code on a worker:

  1. building an engine from a caller-supplied git ref (its Makefile runs), and
  2. executing the freshly built engine binaries (the match, and the UCI probe).

A worker compiles arbitrary refs and runs the resulting binaries, so anyone who
can create a run effectively gets code execution here. The ``ContainerSandbox``
confines each step to a throwaway, network-less, resource-capped container that
runs as a non-root user on a read-only root filesystem.

Bind mounts use identical host/container paths, so engine binaries, opening
books and PGN output all live at the same absolute path inside and outside the
container -- no path rewriting in the callers.

``NullSandbox`` keeps the original "run straight on the host" behaviour for
trusted single-host/local-dev setups (``worker.sandbox.enabled: false``).
"""
from __future__ import annotations

import os
import subprocess
from typing import Any, Dict, Iterable, List, Optional, Sequence


class SandboxError(Exception):
    pass


def from_config(worker_cfg: Dict[str, Any]) -> "Sandbox":
    sb = dict(worker_cfg.get("sandbox") or {})
    if not sb.get("enabled"):
        return NullSandbox()
    return ContainerSandbox(sb)


class Sandbox:
    enabled = False

    def build(self, build_root: str, build_cmd: str,
              timeout: Optional[int] = None) -> subprocess.CompletedProcess:
        raise NotImplementedError

    def exec(self, argv: Sequence[str], *, ro: Iterable[str] = (), rw: Iterable[str] = (),
             workdir: Optional[str] = None, input: Optional[str] = None,
             timeout: Optional[int] = None) -> subprocess.CompletedProcess:
        raise NotImplementedError


class NullSandbox(Sandbox):
    """No isolation: run directly on the host (trusted/local use only)."""

    enabled = False

    def build(self, build_root, build_cmd, timeout=None):
        return subprocess.run(build_cmd, cwd=build_root, shell=True,
                              capture_output=True, text=True, timeout=timeout)

    def exec(self, argv, *, ro=(), rw=(), workdir=None, input=None, timeout=None):
        return subprocess.run(list(argv), cwd=workdir, input=input,
                              capture_output=True, text=True, timeout=timeout)


class ContainerSandbox(Sandbox):
    """Run each build/match in a locked-down ``docker``/``podman`` container."""

    enabled = True

    def __init__(self, sb: Dict[str, Any]):
        self.engine = sb.get("engine", "docker")
        self.image = sb.get("image")
        if not self.image:
            raise SandboxError("worker.sandbox.image is required when sandbox.enabled is true")
        self.network = sb.get("network", "none")
        self.cpus = str(sb.get("cpus", "2"))
        self.memory = str(sb.get("memory", "2g"))
        self.pids = int(sb.get("pids", 512))
        self.build_timeout = int(sb.get("build_timeout", 900))
        self.run_timeout = int(sb.get("run_timeout", 0)) or None
        self.extra_args = [str(a) for a in sb.get("extra_args", [])]

    def _base(self, *, workdir: Optional[str], interactive: bool) -> List[str]:
        cmd = [self.engine, "run", "--rm"]
        if interactive:
            cmd.append("-i")
        cmd += [
            f"--network={self.network}",
            f"--cpus={self.cpus}",
            f"--memory={self.memory}",
            f"--pids-limit={self.pids}",
            "--security-opt", "no-new-privileges",
            "--cap-drop", "ALL",
            "--read-only",
            "--tmpfs", "/tmp:rw,exec,nosuid,size=512m",
            "-e", "HOME=/tmp",
        ]
        getuid = getattr(os, "getuid", None)
        if getuid is not None:
            cmd += ["--user", f"{getuid()}:{os.getgid()}"]
        if workdir:
            cmd += ["-w", workdir]
        cmd += self.extra_args
        return cmd

    @staticmethod
    def _mounts(ro: Iterable[str], rw: Iterable[str]) -> List[str]:
        args: List[str] = []
        seen = set()
        for path in rw:
            real = os.path.abspath(path)
            if real not in seen and os.path.exists(real):
                args += ["-v", f"{real}:{real}:rw"]
                seen.add(real)
        for path in ro:
            real = os.path.abspath(path)
            if real not in seen and os.path.exists(real):
                args += ["-v", f"{real}:{real}:ro"]
                seen.add(real)
        return args

    def build(self, build_root, build_cmd, timeout=None):
        cmd = self._base(workdir=build_root, interactive=False)
        cmd += self._mounts(ro=(), rw=[build_root])
        cmd += [self.image, "/bin/sh", "-c", build_cmd]
        return subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout or self.build_timeout)

    def exec(self, argv, *, ro=(), rw=(), workdir=None, input=None, timeout=None):
        cmd = self._base(workdir=workdir, interactive=input is not None)
        cmd += self._mounts(ro=ro, rw=list(rw) + ([workdir] if workdir else []))
        cmd += [self.image, *map(str, argv)]
        return subprocess.run(cmd, input=input, capture_output=True, text=True,
                              timeout=timeout if timeout is not None else self.run_timeout)
