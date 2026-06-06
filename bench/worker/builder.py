from __future__ import annotations

import os
import shutil
import subprocess
import tempfile

from bench import gitutil


class BuildError(Exception):
    pass


def ensure_build(cache_dir: str, source: str, sha: str, build_cmd: str, binary: str,
                 sandbox=None) -> str:
    dest = os.path.join(cache_dir, sha)
    final = os.path.join(dest, os.path.basename(binary))
    if os.path.exists(final):
        return final

    os.makedirs(cache_dir, exist_ok=True)
    build_root = tempfile.mkdtemp(prefix=f"bench-build-{gitutil.short(sha)}-")
    try:
        gitutil.export_commit(source, sha, build_root)
        if sandbox is None:
            proc = subprocess.run(build_cmd, cwd=build_root, shell=True,
                                  capture_output=True, text=True)
        else:
            proc = sandbox.build(build_root, build_cmd)
        if proc.returncode != 0:
            raise BuildError(f"build failed for {gitutil.short(sha)}:\n"
                             f"{proc.stdout[-1500:]}\n{proc.stderr[-1500:]}")
        built = os.path.join(build_root, binary)
        if not os.path.exists(built):
            raise BuildError(f"build for {gitutil.short(sha)} did not produce '{binary}'")

        os.makedirs(dest, exist_ok=True)
        shutil.copy2(built, final)
        os.chmod(final, 0o755)
        return final
    finally:
        shutil.rmtree(build_root, ignore_errors=True)
