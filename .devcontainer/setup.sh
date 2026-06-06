#!/usr/bin/env bash
set -euo pipefail

root="$(pwd)"

ensure_venv() {
  local venv="$1"
  if [ -f "$venv/pyvenv.cfg" ] && ! grep -q "include-system-site-packages = true" "$venv/pyvenv.cfg"; then
    rm -rf "$venv"
  fi
  if [ ! -d "$venv" ]; then
    python3 -m venv --system-site-packages "$venv"
  fi
}

deps_available() {
  "$1/bin/python" - <<'PY'
import chess
import flask
import pytest
import requests
import yaml
PY
}

ensure_venv "$root/.venv"
ensure_venv "$root/bench/.venv"

if ! deps_available "$root/.venv"; then
  "$root/.venv/bin/python" -m pip install -r "$root/requirements.txt"
fi

if ! deps_available "$root/bench/.venv"; then
  "$root/bench/.venv/bin/python" -m pip install -r "$root/bench/requirements.txt"
fi

if [ ! -f "$root/bench/config.yaml" ]; then
  stockfish_path="$(command -v stockfish || true)"
  if [ -z "$stockfish_path" ] && [ -x /usr/games/stockfish ]; then
    stockfish_path="/usr/games/stockfish"
  fi

  cat > "$root/bench/config.yaml" <<EOF
auth:
  admin_token: "local-admin-token"
  worker_token: "local-worker-token"
  session_secret: "local-session-secret"
  allow_insecure: true

engines:
  grug:
    source: "$root"
    build: "make -j ARCH= EXE=grug"
    binary: "grug"

server:
  host: "127.0.0.1"
  port: 8000
  database: "data/bench.db"
  batch_pairs: 25
  max_pairs_limit: 4000
  secure_cookies: false

worker:
  server_url: "http://127.0.0.1:8000"
  concurrency: 2
  cutechess: "fastchess"
  cache: "data/engines"
  sources: {}
  references:
    stockfish:
      path: "${stockfish_path:-stockfish}"
  sandbox:
    enabled: false
    engine: "docker"
    image: "grug-bench-sandbox:latest"
EOF
fi

make native

cat <<'EOF'

Environment ready.

Useful commands:
  make test
  make bench
  python tools/play.py --engine ./build/grug --side white
  cd bench && ./benchctl start both --config config.yaml
  cd bench && ./benchctl logs server --config config.yaml
  cd bench && ./benchctl stop --config config.yaml

Bench UI:
  http://127.0.0.1:8000
  admin token: local-admin-token

Optional worker sandbox image:
  docker build -t grug-bench-sandbox:latest bench/worker/sandbox
EOF
