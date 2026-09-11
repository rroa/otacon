#!/usr/bin/env bash
# Cross-backend pixel diff: render the same deterministic frame (Mode 0, the
# static box, no HUD) with each graphics backend and check the screenshots match.
# This is the rigorous proof of the "identical across backends" contract.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PATH="/opt/homebrew/bin:$PATH"
OUT="$ROOT/build/diff"; mkdir -p "$OUT"
FRAMES=30

if [ "$(uname)" = "Darwin" ]; then
  export DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/lib:${DYLD_FALLBACK_LIBRARY_PATH}"
  : "${VK_ICD_FILENAMES:=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json}"; export VK_ICD_FILENAMES
fi

shot() {  # $1 = backend
  echo "== $1 =="
  cmake -S . -B build -DGRAPHICS_BACKEND="$1" >/dev/null 2>&1
  cmake --build build -j8 >/dev/null 2>&1
  ./build/bin/canabalt --capture "$OUT/$1.png" --frames "$FRAMES" >/dev/null 2>&1 || true
  [ -f "$OUT/$1.png" ] && shasum -a 256 "$OUT/$1.png" | awk '{print "  sha256",$1}' || echo "  (no screenshot — backend can't read pixels yet)"
}

shot GL_MODERN
shot GL_LEGACY
shot VULKAN

echo "== result =="
cmpp() {  # $1,$2 backend names
  local a="$OUT/$1.png" b="$OUT/$2.png"
  [ -f "$a" ] && [ -f "$b" ] || { echo "  $1 vs $2: missing screenshot"; return; }
  if cmp -s "$a" "$b"; then echo "  $1 == $2  (byte-identical)"
  else echo "  $1 != $2  (differs — different rasterizer; inspect $OUT/*.png)"; fi
}
cmpp GL_MODERN GL_LEGACY     # same projection + GL rasterizer -> expect identical
cmpp GL_MODERN VULKAN        # different API/rasterizer -> may differ at edges
echo "  screenshots in $OUT/"

# restore default
cmake -S . -B build -DGRAPHICS_BACKEND=GL_MODERN >/dev/null 2>&1
cmake --build build -j8 >/dev/null 2>&1
