#!/usr/bin/env bash
# Build Rockbox (Trimpod) as an SDL application for the TrimUI Brick / NextUI (tg5040).
# Cross-compiles inside the Trimpod toolchain image. Output lands in build-trimpod/.
#
#   ./build.sh            # incremental build
#   ./build.sh clean      # wipe build dir and reconfigure from scratch
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
IMG="trimpod-toolchain:latest"
BASE="ghcr.io/loveretro/tg5040-toolchain:latest"
BUILD_DIR="build-trimpod"
TARGET_ID=210          # retro-handheld (TrimUI Brick SDL app), see tools/configure
JOBS="$(nproc)"

# Keep the image in sync with Dockerfile.trimpod. Docker reuses unchanged
# layers, so this is quick after the first run and picks up new host tools.
echo ">> Preparing toolchain image $IMG ..."
docker build -t "$IMG" -f "$ROOT/Dockerfile.trimpod" "$ROOT"

CLEAN=0
[ "${1:-}" = "clean" ] && CLEAN=1

# Run the container as the host user so files written under the bind-mounted
# /build are owned by you, not root (Docker bind mounts map container uid 0 ->
# host uid 0).  HOME=/tmp gives the non-root user a writable home for any tool
# that wants one.  This replaces the post-build sudo chown.
docker run --rm \
  --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$ROOT":/build -w /build "$IMG" bash -lc "
  set -e
  # Generate the licensed UI font derivatives before packaging. convttf is a
  # host tool, so it is built with the container's native gcc, not AArch64 gcc.
  bash tools/build_trimpod_fonts.sh

  # /usr/local/bin (our sdl2-config wrapper, baked into the image) must win over
  # the sysroot's broken sdl2-config, so do NOT prepend \$SYSROOT/usr/bin here.
  if [ $CLEAN -eq 1 ] || [ ! -f $BUILD_DIR/Makefile ]; then
    rm -rf $BUILD_DIR && mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    ../tools/configure --target=$TARGET_ID --type=N
  else
    cd $BUILD_DIR
  fi
  make -j$JOBS
  # Produce the runtime zip package.sh assembles the .pak from.
  make fullzip
"

echo ">> Done. Binary: $BUILD_DIR/trimpod (runtime zip: $BUILD_DIR/trimpod-full.zip)"
