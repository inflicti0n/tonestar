#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

build_dir="${BUILD_DIR:-$root/build-mac}"
arch="${CMAKE_OSX_ARCHITECTURES:-arm64}"
min_os="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$min_os"

cmake --build "$build_dir" --config Release --target ToneStar

app="$build_dir/ToneStar_artefacts/Release/ToneStar.app"
if [[ ! -d "$app" ]]; then
    echo "missing $app" >&2
    exit 1
fi

dist="$root/dist"
mkdir -p "$dist"
zip_path="$dist/ToneStar-mac-${arch}.zip"
rm -f "$zip_path"
ditto -c -k --keepParent "$app" "$zip_path"
echo "zip  $zip_path"
