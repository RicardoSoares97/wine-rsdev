#!/bin/bash
# Copies the built appxdeploymentclient.dll(s) into a Wine prefix's
# system32/syswow64 and imports every register_*.reg file in this directory.
#
# Usage: ./deploy.sh [path-to-wineprefix]
#   defaults to $HOME/.local/share/lutris/teams, the prefix used throughout
#   PROGRESS.md and by test-harness.sh.
#
# Requires build.sh to have been run first (appxdeploymentclient.dll and
# appxdeploymentclient32.dll present in this directory).
set -eu
cd "$(dirname "$0")"

WINEPREFIX="${1:-$HOME/.local/share/lutris/teams}"
export WINEPREFIX

[ -f appxdeploymentclient.dll ] || { echo "[deploy] appxdeploymentclient.dll missing - run build.sh first" >&2; exit 1; }
[ -f appxdeploymentclient32.dll ] || { echo "[deploy] appxdeploymentclient32.dll missing - run build.sh first" >&2; exit 1; }
[ -d "$WINEPREFIX/drive_c/windows/system32" ] || { echo "[deploy] $WINEPREFIX doesn't look like a Wine prefix" >&2; exit 1; }

echo "[deploy] prefix: $WINEPREFIX"
cp -v appxdeploymentclient.dll   "$WINEPREFIX/drive_c/windows/system32/appxdeploymentclient.dll"
cp -v appxdeploymentclient32.dll "$WINEPREFIX/drive_c/windows/syswow64/appxdeploymentclient.dll"

echo "[deploy] importing .reg files"
for regfile in register_*.reg; do
    echo "  $regfile"
    wine regedit "$regfile"
done

echo "[deploy] done. WINEDLLOVERRIDES=appxdeploymentclient=n,appxpackaging=n is still required at runtime"
echo "[deploy] (test-harness.sh already sets this)."
