#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="$here/tools"
mkdir -p "$dest"

pick_arch() {
  if [ -n "${1:-}" ]; then echo "$1"; return; fi
  local flags; flags=$(grep -m1 '^flags' /proc/cpuinfo || true)
  if   echo "$flags" | grep -q bmi2;  then echo "bmi2"
  elif echo "$flags" | grep -q avx2;  then echo "avx2"
  else echo "sse41-popcnt"; fi
}

arch="$(pick_arch "${1:-}")"
file="stockfish-ubuntu-x86-64-${arch}.tar"
url="https://github.com/official-stockfish/Stockfish/releases/latest/download/${file}"

echo "Downloading Stockfish (${arch}) ..."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
if ! curl -fL "$url" -o "$tmp/$file"; then
  echo "Download failed. Try a different arch: $0 [avx2|sse41-popcnt]" >&2
  exit 1
fi

tar -xf "$tmp/$file" -C "$tmp"
bin="$(find "$tmp" -type f -name 'stockfish*' -perm -u+x | head -n1)"
[ -n "$bin" ] || bin="$(find "$tmp" -type f -path '*/stockfish/*' | head -n1)"
cp "$bin" "$dest/stockfish"
chmod +x "$dest/stockfish"

echo "Installed: $dest/stockfish"
"$dest/stockfish" --help >/dev/null 2>&1 || true
echo
echo "Point your worker config at it:"
echo "  worker.references.stockfish.path: \"$dest/stockfish\""
