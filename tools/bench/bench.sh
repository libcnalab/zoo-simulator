#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Measures the visit on this machine's own graphics hardware, without a window:
# SDL's offscreen video driver renders through EGL on the GPU's render node
# (it is the real driver, not llvmpipe: the first line the game prints names
# the OpenGL version).
#
#   tools/bench/bench.sh [frames]        from the repository's root; build/ must be built
#
# Each line of the result is one run of ./zoo-simulator --bench.
set -u
frames="${1:-600}"
cd "$(dirname "$0")/../../build" || exit 1
run() {
    local label="$1"
    shift
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=offscreen ./zoo-simulator --bench --frames "$frames" "$@" 2>&1 |
        grep -E "^zoo: bench|^zoo: visit: 800|^zoo: visitor zoo|^zoo: ground|^zoo: scenery" |
        sed "s|^zoo: |[$label] |"
}
echo "== places, as the game is played (the nearest 40 people avatars, the rest drawn flat)"
run entrance
for place in lion penguin elephant giraffe meerkat flamingo; do
    run "$place" --stand "habitat:$place"
done
echo "== avatars at most, at the entrance (everybody else in view is drawn flat)"
for people in 1 20 40 80; do
    run "avatars-$people" --people "$people"
done
echo "== avatars only, as it was: no flat people, the nearest N drawn and nobody else"
for people in 1 30 60 120 250 400; do
    run "only-avatars-$people" --without flat --people "$people"
done
echo "== a part left out, at the entrance"
for part in animals people scenery terrain shadows simulation; do
    run "without-$part" --without "$part"
done
echo "== a part left out, at the lions"
for part in animals people scenery shadows; do
    run "lion-without-$part" --stand habitat:lion --without "$part"
done
