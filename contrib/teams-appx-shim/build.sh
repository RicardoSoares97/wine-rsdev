#!/bin/bash
# Builds appxdeploymentclient.dll (64-bit) and appxdeploymentclient32.dll
# (32-bit) from this directory's sources, using winegcc directly rather than
# Wine's own dlls/*/Makefile.in build system — this project is standalone
# (not part of the Wine source tree), so it doesn't get the usual
# widl/winebuild machinery for free.
#
# Requires: winegcc, widl, i686-w64-mingw32-gcc, x86_64-w64-mingw32-gcc
# (on Ubuntu: the wine-staging package + gcc-mingw-w64-i686/gcc-mingw-w64-x86-64)
#
# Note: classes.idl is NOT compiled in — it's leftover from the real,
# separate upstream Wine dlls/appxdeploymentclient/ module this project's
# name collides with (by Mohamad Al-Jaf); our own DllGetClassObject /
# DllGetActivationFactory dispatch in main.c doesn't use a generated
# typelib, so building classes.idl in isn't necessary and previously
# produced an empty winebuild object that failed the link.
set -eu
cd "$(dirname "$0")"

SOURCES="main.c package.c pkginfo.c composition.c miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c"
LIBS="-lcombase -lole32 -loleaut32 -luuid -ld3d11 -ldxgi -lgdi32 -luser32 -ladvapi32 -lshlwapi"

echo "[build] 64-bit -> appxdeploymentclient.dll"
winegcc -shared -b x86_64-w64-mingw32 -Ishim -I. \
    $SOURCES appxdeploymentclient.def $LIBS \
    -o appxdeploymentclient.dll

echo "[build] 32-bit -> appxdeploymentclient32.dll"
winegcc -shared -b i686-w64-mingw32 -Ishim -I. \
    $SOURCES appxdeploymentclient.def $LIBS \
    -o appxdeploymentclient32.dll

echo "[build] done:"
ls -la appxdeploymentclient.dll appxdeploymentclient32.dll
