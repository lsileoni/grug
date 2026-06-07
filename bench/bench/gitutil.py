from __future__ import annotations

import os
import re
import subprocess

_SHA_RE = re.compile(r"^[0-9a-fA-F]{7,40}$")

# A deliberately tight allow-list for caller-supplied refs. It covers branch
# names, tags, and hex SHAs while excluding anything that git could parse as an
# option (leading '-') or that has special meaning to revision/transport code
# ('..' ranges, ':' refspecs, '--upload-pack=' style injection). Without this,
# a ref like '--upload-pack=…' is a known git argument-injection / RCE vector.
_REF_RE = re.compile(r"^[0-9A-Za-z_][0-9A-Za-z._/+-]{0,199}$")


def is_url(source: str) -> bool:
    return bool(re.match(r"^(https?://|git://|ssh://|git@)", source))


def valid_ref(ref: str) -> bool:
    ref = ref.strip()
    return bool(_REF_RE.match(ref)) and ".." not in ref


def require_valid_ref(ref: str) -> str:
    ref = ref.strip()
    if not valid_ref(ref):
        raise ValueError(
            f"invalid ref '{ref}': use a branch, tag, or commit SHA "
            "(letters, digits, '. _ / + -' only)")
    return ref


def _run(args, **kw) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True, **kw).stdout.strip()


def resolve_ref(source: str, ref: str) -> str:
    ref = require_valid_ref(ref)
    if is_url(source):
        if _SHA_RE.match(ref) and len(ref) == 40:
            return ref.lower()
        out = _run(["git", "ls-remote", source, ref])
        if not out:
            out = _run(["git", "ls-remote", source, f"refs/heads/{ref}", f"refs/tags/{ref}"])
        if not out:
            raise ValueError(f"ref '{ref}' not found at {source}")
        return out.split()[0].lower()
    try:
        return _run(["git", "-C", source, "rev-parse", "--verify", f"{ref}^{{commit}}"]).lower()
    except subprocess.CalledProcessError as e:
        raise ValueError(f"ref '{ref}' not found in {source}: {e.stderr.strip()}") from e


def short(sha: str) -> str:
    return sha[:10]


def export_commit(source: str, sha: str, dest: str) -> None:
    os.makedirs(dest, exist_ok=True)
    if is_url(source):
        _run(["git", "clone", "--quiet", source, dest])
        _run(["git", "-C", dest, "checkout", "--quiet", sha])
    else:
        archive = subprocess.Popen(["git", "-C", source, "archive", sha],
                                   stdout=subprocess.PIPE)
        try:
            subprocess.run(["tar", "-x", "-C", dest], stdin=archive.stdout, check=True)
        finally:
            if archive.stdout:
                archive.stdout.close()
            archive.wait()
        if archive.returncode != 0:
            raise RuntimeError(f"git archive {short(sha)} from {source} failed")
