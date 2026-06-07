#!/usr/bin/env bash
set -euo pipefail

app="${GRUG_LICHESS_HOME:-/srv/grug-lichess}"
compose_bin="${COMPOSE_BIN:-docker compose}"
release="${1:-}"

if [ -z "$release" ]; then
  echo "usage: $0 <release-sha-or-release-path>" >&2
  ls -1 "$app/releases" >&2
  exit 1
fi

case "$release" in
  /*) target="$release" ;;
  *) target="$app/releases/$release" ;;
esac

if [ ! -x "$target/grug" ]; then
  echo "release does not contain executable grug: $target" >&2
  exit 1
fi

ln -sfn "$target" "$app/current"
install -m 0755 "$target/grug" "$app/compose/engines/grug"
cd "$app/compose"
$compose_bin up -d --remove-orphans
