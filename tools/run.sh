#!/usr/bin/env bash
# Launch canabalt. On macOS this also points the dynamic linker + Vulkan loader
# at the Homebrew install so the VULKAN backend works without manual setup
# (harmless for the OpenGL backends).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bin/canabalt"

if [ "$(uname)" = "Darwin" ]; then
  export DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/lib:/usr/local/lib:${DYLD_FALLBACK_LIBRARY_PATH}"
  : "${VK_ICD_FILENAMES:=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json}"
  export VK_ICD_FILENAMES
fi

if [ ! -x "$BIN" ]; then
  echo "Build first:  cmake -S \"$ROOT\" -B \"$ROOT/build\" && cmake --build \"$ROOT/build\" -j8"
  exit 1
fi
exec "$BIN" "$@"
