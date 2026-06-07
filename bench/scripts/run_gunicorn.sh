#!/usr/bin/env bash
# Production server: gunicorn behind a TLS reverse proxy (see deploy/).
# Single worker process (the scheduler lock is in-process), scaled with threads.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

if [ ! -d .venv ]; then
  python3 -m venv .venv
  .venv/bin/python -m pip install --quiet --upgrade pip
  .venv/bin/python -m pip install --quiet -r requirements.txt
fi
# A .venv created by run_server.sh before gunicorn was a dependency won't have it.
if [ ! -x .venv/bin/gunicorn ]; then
  .venv/bin/python -m pip install --quiet -r requirements.txt
fi

# gunicorn runs with --chdir server, so resolve a relative config to an absolute
# path now (relative to bench/) - otherwise wsgi.py looks for it under server/.
cfg="${1:-config.yaml}"
case "$cfg" in
  /*) ;;
  *) cfg="$here/$cfg" ;;
esac
export BENCH_CONFIG="$cfg"
export PYTHONUNBUFFERED=1

# Bind to localhost only; the reverse proxy terminates TLS and faces the world.
exec .venv/bin/gunicorn \
  --chdir "$here/server" \
  --workers 1 \
  --threads "${BENCH_THREADS:-8}" \
  --timeout 120 \
  --bind "${BENCH_BIND:-127.0.0.1:8000}" \
  wsgi:app
