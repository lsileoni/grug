#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="$here/tools"
mkdir -p "$dest"
src="$here/tools/cutechess-src"

need_apt=0
command -v cmake   >/dev/null 2>&1 || need_apt=1
command -v qmake6  >/dev/null 2>&1 || command -v qmake >/dev/null 2>&1 || need_apt=1

if [ "$need_apt" = 1 ]; then
  echo "Installing build deps (needs sudo) ..."
  sudo apt-get update
  sudo apt-get install -y build-essential cmake git \
       qt6-base-dev qt6-base-dev-tools \
    || sudo apt-get install -y build-essential cmake git \
       qtbase5-dev qtbase5-dev-tools libqt5svg5-dev
fi

if [ ! -d "$src" ]; then
  git clone --depth 1 https://github.com/cutechess/cutechess "$src"
fi

cd "$src"
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j --target cutechess-cli

bin="$(find "$src/build" -type f -name 'cutechess-cli' | head -n1)"
[ -n "$bin" ] || { echo "build produced no cutechess-cli" >&2; exit 1; }
cp "$bin" "$dest/cutechess-cli"
chmod +x "$dest/cutechess-cli"

echo "Installed: $dest/cutechess-cli"
"$dest/cutechess-cli" --version || true
echo
echo "Point your worker config at it:"
echo "  worker.cutechess: \"$dest/cutechess-cli\""
