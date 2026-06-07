#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

if [ ! -d .venv ]; then
  python3 -m venv --system-site-packages .venv
fi

if ! .venv/bin/python - <<'PY' >/dev/null 2>&1
import chess
import requests
import yaml
PY
then
  .venv/bin/python -m pip install --quiet -r requirements.txt
fi

exec env PYTHONUNBUFFERED=1 .venv/bin/python worker/worker.py --config "${1:-config.yaml}"
