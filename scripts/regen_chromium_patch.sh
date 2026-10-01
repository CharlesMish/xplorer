#!/bin/sh
# Rebuild patches/xplorer_chromium.diff from a working chromium tree.
#
# The patch holds every Chromium-file edit (everything apply.sh's copy steps
# don't produce). Run this after changing Chromium files by hand, then commit
# the new patch so Windows/Linux/x64 hosts get the same tree.
#
#   scripts/regen_chromium_patch.sh [chromium/src]
#
# The tree must be detached at the pin with the edits uncommitted.
set -eu
XPLORER="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "${1:-$XPLORER/../chromium/src}" && pwd)"
PIN="$(git -C "$SRC" rev-parse HEAD)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cd "$SRC"
# 1. Snapshot the current tree.
git stash push -q --include-untracked -m regen-chromium-patch
STASH="$(git rev-parse stash@{0})"
# 2. Clean pin + apply.sh copy steps only (no python edits).
sed -e "s|^XPLORER=.*|XPLORER=\"$XPLORER\"|" -e '/apply_integration.py/d' \
  "$XPLORER/apply.sh" > "$TMP/apply_copy.sh"
sh "$TMP/apply_copy.sh" "$SRC" >/dev/null
git add -A
BASE_TREE="$(git write-tree)"
git reset -q --hard "$PIN"
git clean -qfd chrome components ui third_party/sparkle
# 3. Restore the snapshot and diff it against the copy-only base.
git stash pop -q
git add -A
git diff --binary --cached "$BASE_TREE" -- . \
  ':!chrome/VERSION' > "$XPLORER/patches/xplorer_chromium.diff"
git reset -q
echo "wrote $XPLORER/patches/xplorer_chromium.diff ($(wc -c < "$XPLORER/patches/xplorer_chromium.diff") bytes)"
