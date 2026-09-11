#!/usr/bin/env bash
# Build both manuals to PDF with tectonic (the HTML pages are hand-authored).
#   - Engine (Otacon):  docs/otacon-engine.pdf
#   - Game   (Canabalt): src/game/canabalt/docs/canabalt.pdf
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
command -v tectonic >/dev/null || { echo "need 'tectonic' (brew install tectonic)"; exit 1; }
echo "== engine =="; tectonic "$ROOT/docs/otacon-engine.tex"
echo "== game   =="; tectonic "$ROOT/src/game/canabalt/docs/canabalt.tex"
echo "done: docs/otacon-engine.pdf + src/game/canabalt/docs/canabalt.pdf"
