#!/bin/bash
# SPDX-License-Identifier: MIT
# Points an existing Zoo Simulator build directory at the snapshot of the CNA libraries,
# ~/deps/zoo-libcna (configure only; README "Building"). The build type, sanitizers and other
# settings of the directory stay as they are.
# Usage: tools/build/use-libcna-snapshot.sh <source-dir> <build-dir> <log>
set -u
src=$1; bin=$2; log=$3
D=$HOME/deps/zoo-libcna; S=$D/cna/.sdl-prebuilt-Linux-x86_64-wayland
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake -S "$src" -B "$bin" \
  -DZOO_LIBCNA_ROOT=$D -DCNA_ENGINE_ROOT=$D/cna-engine -DZOO_SHARP_RUNTIME_ROOT=$D/sharp-runtime \
  -DZOO_NLOHMANN_JSON_INCLUDE=$D/sharp-runtime/vendor -DZOO_GOOGLETEST_DIR=$D/sharp-runtime/vendor/googletest \
  -DCNA_EASYGL_ROOT=$D/easy-gl -DCNA_ENGINE_CNA_DIR=$D/cna -DCNA_ENGINE_EXTENDED_DIR=$D/cna-extended \
  -DCNA_ENGINE_SHARP_RUNTIME_DIR=$D/sharp-runtime -DCNA_SHARP_RUNTIME_ROOT=$D/sharp-runtime \
  -DCNA_GAMER_SERVICES_SERVER_ROOT=$D/cna-gamer-services-server -DCNA_SDL_PREBUILT_ROOT=$S \
  -USDL3_DIR -USDL3_mixer_DIR -UCNA_SDL_PREBUILT_DIR -DCNA_MAX_VENDORED_BUILD_JOBS=2 \
  -DFETCHCONTENT_SOURCE_DIR_FNA3D=$D/FNA3D -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache > "$log" 2>&1
status=$?
left=$(grep -E '^[A-Za-z0-9_]+:[A-Z]+=.*github\.com/libcna/' "$bin/CMakeCache.txt" | grep -v 'FNA3D.git' | wc -l)
echo "$bin: cmake exit $status, cache entries still at libcna: $left"
