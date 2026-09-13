#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

if [[ -z "${ANDROID_NDK_ROOT:-}" ]]; then
  printf '%s\n' 'ANDROID_NDK_ROOT is required' >&2
  exit 2
fi

prebuilt="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
if [[ ! -d "$prebuilt" ]]; then
  prebuilt="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/windows-x86_64"
fi

compiler="$prebuilt/bin/aarch64-linux-android26-clang"
if [[ ! -x "$compiler" && -f "$compiler.cmd" ]]; then
  compiler="$compiler.cmd"
fi
if [[ ! -f "$compiler" ]]; then
  printf 'Android ARM64 compiler not found under %s\n' "$ANDROID_NDK_ROOT" >&2
  exit 3
fi

out_dir="build/ci/native"
mkdir -p "$out_dir"
output="$out_dir/alttab_evdev_bridge"

"$compiler" \
  -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -I native-bridge/src \
  native-bridge/src/alttab_evdev_bridge.c \
  native-bridge/src/bridge_options.c \
  native-bridge/src/key_remap.c \
  native-bridge/src/native_protocol.c \
  native-bridge/src/touchpad_drag_state.c \
  native-bridge/src/touchpad_bridge.c \
  -o "$output"

if command -v file >/dev/null 2>&1; then
  file "$output" | grep -Eq 'ELF 64-bit.*(ARM aarch64|ARM64|AArch64)'
fi
if command -v readelf >/dev/null 2>&1; then
  readelf -h "$output" | grep -Eq 'Machine:[[:space:]]+AArch64'
fi

printf 'Built %s\n' "$output"
