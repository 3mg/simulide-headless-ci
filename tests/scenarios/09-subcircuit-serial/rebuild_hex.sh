#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

run_platformio() {
  if command -v platformio >/dev/null 2>&1; then
    platformio run -d "$1"
    return
  fi

  if command -v pio >/dev/null 2>&1; then
    pio run -d "$1"
    return
  fi

  echo "PlatformIO CLI not found. Install 'platformio' or 'pio' to rebuild firmware." >&2
  exit 1
}

build_and_copy() {
  local project_dir="$1"
  local built_hex="$2"
  local output_hex="$3"

  run_platformio "$project_dir"
  cp "$project_dir/$built_hex" "$output_hex"
}

build_and_copy \
  "$SCRIPT_DIR/sources/mega_master" \
  ".pio/build/megaatmega2560/firmware.hex" \
  "$SCRIPT_DIR/firmware/mega_master.hex"

build_and_copy \
  "$SCRIPT_DIR/sources/slave_0x10" \
  ".pio/build/digispark/firmware.hex" \
  "$SCRIPT_DIR/firmware/slave_0x10.hex"

build_and_copy \
  "$SCRIPT_DIR/sources/slave_0x11" \
  ".pio/build/digispark/firmware.hex" \
  "$SCRIPT_DIR/firmware/slave_0x11.hex"

echo "Updated firmware snapshots in $SCRIPT_DIR/firmware"
