#!/usr/bin/env bash
# Launch one of the project's executables: canabalt (default), dino, flappy or
# samples. On macOS this also points the dynamic linker + Vulkan loader at the
# Homebrew install so the VULKAN backend works without manual setup (harmless
# for the OpenGL backends) — which is why this wrapper exists at all.
#
#   tools/run.sh                    # canabalt
#   tools/run.sh samples            # the engine samples
#   tools/run.sh samples --sample 5 # ...opened on one sample
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

TARGET="canabalt"
case "$1" in
  canabalt|dino|flappy|samples) TARGET="$1"; shift ;;
esac
BIN="$ROOT/build/bin/$TARGET"

if [ "$(uname)" = "Darwin" ]; then
  export DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/lib:/usr/local/lib:${DYLD_FALLBACK_LIBRARY_PATH}"
  : "${VK_ICD_FILENAMES:=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json}"
  export VK_ICD_FILENAMES
fi

if [ ! -x "$BIN" ]; then
  echo "No '$TARGET' binary. Build first:"
  echo "  cmake -S \"$ROOT\" -B \"$ROOT/build\" && cmake --build \"$ROOT/build\" -j8"
  exit 1
fi
exec "$BIN" "$@"
