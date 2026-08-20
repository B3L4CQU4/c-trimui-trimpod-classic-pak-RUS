#!/usr/bin/env bash
# Package the object files needed to relink a release with a modified projectM.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-trimpod"
OUT="$ROOT/dist/TrimPod(RUS)-relink-kit.tar.gz"

[ -f "$BUILD/Makefile" ] && [ -f "$BUILD/make.dep" ] || {
    echo "Missing build-trimpod build metadata -- run ./build.sh first." >&2
    exit 1
}

mkdir -p "$ROOT/dist"
list="$(mktemp)"
trap 'rm -f "$list"' EXIT

(
    cd "$ROOT"
    printf '%s\0' \
        build-trimpod/Makefile \
        build-trimpod/make.dep \
        build-trimpod/autoconf.h \
        build-trimpod/lang_enum.h \
        build-trimpod/rbversion.h \
        build-trimpod/sysfont.c \
        build-trimpod/sysfont.h \
        lib/projectm/include \
        lib/projectm/lib \
        lib/projectm/source \
        lib/projectm/patches \
        lib/projectm/toolchains \
        lib/projectm/PROVENANCE.md \
        lib/projectm/RELINKING.md \
        tools/build_projectm.sh
    find build-trimpod -type f \( -name '*.o' -o -name '*.a' \) -print0
) > "$list"

tar -C "$ROOT" --null -T "$list" -czf "$OUT"
echo ">> Relinking materials: $OUT"
