#!/usr/bin/env bash
set -euo pipefail

repo="${GRUG_REPO:-/srv/grug}"
app="${GRUG_LICHESS_HOME:-/srv/grug-lichess}"
ref="${1:-main}"
compose_bin="${COMPOSE_BIN:-docker compose}"

release_root="$app/releases"
config_dir="$app/config"
current_link="$app/current"
compose_dir="$app/compose"
engine_mount="$compose_dir/engines"
env_file="$config_dir/lichess.env"

log() {
  printf '[%s] %s\n' "$(date -Is)" "$*"
}

require_file() {
  if [ ! -f "$1" ]; then
    echo "missing required file: $1" >&2
    exit 1
  fi
}

require_file "$env_file"
require_file "$repo/deploy/lichess/compose.yml"
require_file "$repo/deploy/lichess/config.yml.template"

set -a
# shellcheck disable=SC1090
. "$env_file"
set +a

if [ -z "${LICHESS_TOKEN:-}" ]; then
  echo "LICHESS_TOKEN is empty in $env_file" >&2
  exit 1
fi

log "fetching $ref in $repo"
git -C "$repo" fetch origin
git -C "$repo" checkout -q "$ref"
if [ "$(git -C "$repo" symbolic-ref --short -q HEAD || true)" = "$ref" ]; then
  git -C "$repo" pull --ff-only origin "$ref"
fi
sha="$(git -C "$repo" rev-parse HEAD)"
release="$release_root/$sha"

mkdir -p "$release" "$compose_dir" "$engine_mount" "$compose_dir/game_records"

if [ ! -x "$release/grug" ]; then
  log "building grug $sha"
  make -C "$repo" GRUG_IN_CONTAINER=1 native ARCH= EXE="$release/grug"
fi

log "running UCI smoke test"
uci_out="$(mktemp)"
trap 'rm -f "$uci_out"' EXIT
printf 'uci\nisready\nsetoption name Algorithm value basic_search\nposition startpos\ngo depth 1\nquit\n' \
  | "$release/grug" >"$uci_out"
grep -q '^uciok' "$uci_out"
grep -q '^readyok' "$uci_out"
grep -q '^bestmove ' "$uci_out"

log "publishing release $sha"
ln -sfn "$release" "$current_link"
cp "$repo/deploy/lichess/compose.yml" "$compose_dir/compose.yml"
sed "s|\${LICHESS_TOKEN}|$LICHESS_TOKEN|g" \
  "$repo/deploy/lichess/config.yml.template" >"$compose_dir/config.yml"
chmod 600 "$compose_dir/config.yml"
install -m 0755 "$current_link/grug" "$engine_mount/grug"

log "starting lichess-bot container"
cd "$compose_dir"
$compose_bin pull
$compose_bin up -d --remove-orphans

log "health check"
"$repo/deploy/lichess/healthcheck.sh"
log "deployed $sha"
