#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Where the game's memory is (docs/RELIABILITY.md, "Memory"): runs ./zoo-simulator without a
# window and without sound, samples /proc/<pid>/smaps twice a second and prints the largest
# sample by the kind of mapping, then the same figure with one part of the game left out.
#
#   tools/bench/memory.sh [frames] [more arguments of the game]    from the repository's root
#
# One game process at a time; build/ must be built. Resident memory is what the process
# holds in RAM: textures and buffers count as far as the graphics driver keeps them there.
set -uo pipefail
frames="${1:-400}"
[[ "$frames" =~ ^[1-9][0-9]*$ ]] || { echo "frames must be positive" >&2; exit 2; }
shift 2>/dev/null || true
cd "$(dirname "$0")/../../build" || exit 1
mkdir -p ../build-probe/memory-saves ../build-probe/memory

# The largest sample of one run: total and by kind of mapping, in MB.
sample() {
    local label="$1"
    shift
    while [ "$(free -g | awk 'NR==2{print $7}')" -lt 6 ]; do sleep 60; done
    local log="../build-probe/memory/${label// /-}.log"
    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=offscreen timeout 300 ./zoo-simulator --saves ../build-probe/memory-saves --frames "$frames" "$@" \
        >"$log" 2>&1 &
    local guard=$!
    # The game is the child of timeout; timeout itself holds two megabytes and tells nothing.
    local pid=""
    while [ -z "$pid" ] && kill -0 "$guard" 2>/dev/null; do
        pid=$(pgrep -P "$guard" | head -n 1)
        sleep 0.02
    done
    local best=0 line=""
    while [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; do
        local now
        now=$(awk '
            /^[0-9a-f]+-[0-9a-f]+ / {
                name = $6
                if (name == "" ) kind = "anonymous"
                else if (name == "[heap]") kind = "heap"
                else if (name ~ /^\[stack/) kind = "stack"
                else if (name ~ /^\/dev\/|^\/memfd:|^\/SYSV|^\[anon_shmem/) kind = "driver"
                else if (name ~ /^\[/) kind = "other"
                else kind = "files"
            }
            /^Rss:/ { rss[kind] += $2; total += $2 }
            END {
                printf "%d total=%d heap=%d anonymous=%d files=%d driver=%d other=%d\n", total, total / 1024,
                       rss["heap"] / 1024, rss["anonymous"] / 1024, rss["files"] / 1024, rss["driver"] / 1024,
                       (rss["other"] + rss["stack"]) / 1024
            }' "/proc/$pid/smaps" 2>/dev/null)
        local kb="${now%% *}"
        if [ -n "$kb" ] && [ "$kb" -gt "$best" ] 2>/dev/null; then
            best="$kb"
            line="${now#* }"
        fi
        sleep 0.5
    done
    local status=0
    wait "$guard" || status=$?
    if [ "$status" -ne 0 ] || [ -z "$line" ]; then
        echo "[$label] failed: exit=$status; see $log" >&2
        return 1
    fi
    printf '%-22s %s MB\n' "[$label]" "$line"
}

# Only the parts whose resources are not made at all when they are left out: terrain,
# scenery and animals are made and merely not drawn (the game prints those with --bench:
# "zoo: start: ..." gives the resident memory after each part of loading).
sample "as played" "$@" || exit 1
for part in shadows skylight flat; do
    sample "without $part" --without "$part" "$@" || exit 1
done
sample "one avatar" --people 1 "$@" || exit 1
sample "editor, starter zoo" --editor "$@" || exit 1
