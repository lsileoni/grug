from __future__ import annotations

import shutil
from typing import Any, Dict


class ReferenceError(Exception):
    pass


def resolve(worker_cfg: Dict[str, Any], name: str) -> str:
    refs = worker_cfg.get("references", {})
    spec = refs.get(name)
    if not spec:
        raise ReferenceError(
            f"reference engine '{name}' is not configured on this worker "
            f"(add it under worker.references in config.yaml)")
    path = spec["path"] if isinstance(spec, dict) else str(spec)
    resolved = shutil.which(path) or path
    if not shutil.which(resolved) and not _is_executable(resolved):
        raise ReferenceError(f"reference engine '{name}' binary not found: {path}")
    return resolved


def available(worker_cfg: Dict[str, Any], name: str) -> bool:
    try:
        resolve(worker_cfg, name)
        return True
    except ReferenceError:
        return False


def _is_executable(path: str) -> bool:
    import os
    return os.path.isfile(path) and os.access(path, os.X_OK)
