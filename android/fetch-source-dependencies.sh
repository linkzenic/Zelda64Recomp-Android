#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

while read -r path revision url; do
    mkdir -p "$path"
    git -C "$path" init --quiet
    git -C "$path" fetch --depth=1 "$url" "$revision"
    git -C "$path" checkout --detach FETCH_HEAD
    git -C "$path" submodule update --init --recursive
done < android/dependencies.txt

for dependency in N64ModernRuntime rt64; do
    patch_file="$(pwd)/android/$dependency.patch"
    git -C "lib/$dependency" apply --check "$patch_file"
    git -C "lib/$dependency" apply "$patch_file"
done
