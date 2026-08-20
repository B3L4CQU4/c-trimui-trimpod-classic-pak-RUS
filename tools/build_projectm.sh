#!/usr/bin/env bash
# Rebuild the statically linked projectM libraries from their checked-in source.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="trimpod-toolchain:latest"
SRC="/build/lib/projectm/source/projectm-4.1.6"
OUT="/build/build-projectm-tg5040"

echo ">> Preparing toolchain image $IMG ..."
docker build -t "$IMG" -f "$ROOT/Dockerfile.trimpod" "$ROOT"

docker run --rm \
  --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$ROOT":/build -w /build "$IMG" bash -lc "
  set -euo pipefail
  rm -rf '$OUT'
  cmake -S '$SRC' -B '$OUT' \
    -DCMAKE_TOOLCHAIN_FILE=/build/lib/projectm/toolchains/tg5040.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_GLES=ON \
    -DENABLE_SYSTEM_GLM=OFF \
    -DENABLE_SYSTEM_PROJECTM_EVAL=OFF \
    -DENABLE_PLAYLIST=OFF \
    -DENABLE_SDL_UI=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCS=OFF
  cmake --build '$OUT' --parallel --target projectM projectM_eval

  core=\$(find '$OUT' -type f -name 'libprojectM-4.a' -print -quit)
  eval_lib=\$(find '$OUT' -type f -name 'libprojectM_eval.a' -print -quit)
  test -n \"\$core\"
  test -n \"\$eval_lib\"
  install -m 0644 \"\$core\" /build/lib/projectm/lib/libprojectM-4.a
  install -m 0644 \"\$eval_lib\" /build/lib/projectm/lib/libprojectM_eval.a
"

echo ">> Rebuilt lib/projectm/lib/libprojectM-4.a and libprojectM_eval.a"
echo ">> Run ./build.sh to relink TrimPod(RUS)."
