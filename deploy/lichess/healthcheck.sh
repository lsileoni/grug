#!/usr/bin/env bash
set -euo pipefail

repo="${GRUG_REPO:-/srv/grug}"
app="${GRUG_LICHESS_HOME:-/srv/grug-lichess}"
compose_bin="${COMPOSE_BIN:-docker compose}"
engine="$app/current/grug"
compose_dir="$app/compose"

if [ ! -x "$engine" ]; then
  echo "engine is not executable: $engine" >&2
  exit 1
fi

uci_out="$(mktemp)"
trap 'rm -f "$uci_out"' EXIT
printf 'uci\nisready\nposition startpos\ngo depth 1\nquit\n' | "$engine" >"$uci_out"
grep -q '^uciok' "$uci_out"
grep -q '^readyok' "$uci_out"
grep -q '^bestmove ' "$uci_out"

if [ -f "$compose_dir/compose.yml" ]; then
  cd "$compose_dir"
  $compose_bin ps grug-lichess-bot
fi

git -C "$repo" rev-parse --short HEAD
