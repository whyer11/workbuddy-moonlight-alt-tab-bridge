#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

out_dir="build/ci/tests"
mkdir -p "$out_dir"

compiler="${CC:-cc}"

build_and_run() {
  local name="$1"
  shift
  "$compiler" "$@" \
    -I native-bridge/src -std=c11 -Wall -Wextra -Werror -pedantic \
    -o "$out_dir/$name"

  local output
  output="$($out_dir/$name)"
  printf '%s\n' "$output"
  if [[ "$output" != *"OK"* ]]; then
    printf '%s\n' "$name did not report OK" >&2
    return 1
  fi
}

build_and_run key_remap_test \
  native-bridge/tests/key_remap_test.c \
  native-bridge/src/key_remap.c

build_and_run native_protocol_test \
  native-bridge/tests/native_protocol_test.c \
  native-bridge/src/native_protocol.c

build_and_run touchpad_drag_state_test \
  native-bridge/tests/touchpad_drag_state_test.c \
  native-bridge/src/touchpad_drag_state.c

build_and_run bridge_options_test \
  native-bridge/tests/bridge_options_test.c \
  native-bridge/src/bridge_options.c
