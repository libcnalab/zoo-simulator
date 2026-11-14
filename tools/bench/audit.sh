#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Run the sanitizer game matrix using only this checkout and its existing build-asan/.
# Logs, saves and screenshots stay in build-probe/. Each game is bounded and runs alone.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root/build-asan" || exit 1
mkdir -p ../build-probe/finish-asan ../build-probe/shots ../build-probe/audit-saves
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
failed=0
run() {
    local name="$1" leaks="$2"
    shift 2
    while [ "$(free -g | awk 'NR==2{print $7}')" -lt 6 ]; do sleep 60; done
    local log="../build-probe/finish-asan/$name.log" start status=0 reports
    start=$(date +%s)
    ASAN_OPTIONS="detect_leaks=$leaks:abort_on_error=0:halt_on_error=0" \
        timeout 900 ./zoo-simulator --saves ../build-probe/audit-saves "$@" >"$log" 2>&1 || status=$?
    reports=$(grep -Ec 'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error|AddressSanitizer:DEADLYSIGNAL' "$log")
    printf '%s exit=%s seconds=%s reports=%s\n' "$name" "$status" "$(($(date +%s) - start))" "$reports"
    if [ "$status" -ne 0 ] || [ "$reports" -ne 0 ]; then failed=1; fi
}
run start 0 --frames 5
run lion 0 --stand habitat:lion --frames 30
run zebra 0 --stand habitat:zebra --frames 30
run flamingo 0 --stand habitat:flamingo --frames 30
run board 0 --stand board:0 --frames 30 --script ../tests/game/read_board.script \
    --screenshot ../build-probe/shots/board-finished-asan.png
run people-800 0 --people 800 --frames 30
run people-0 0 --people 0 --frames 30
# Bench excludes its first 40 frames; 300 gives a meaningful frame-time sample.
run bench 0 --bench --frames 300
run viewer 0 --viewer --animal lion --clip walk --frames 40
run editor 0 --editor --frames 100 --lockstep --saves ../build-probe/audit-editor-saves \
    --script ../tests/ui/save_load.script
run visit-load-twice 0 --stand board:0 --frames 400 --lockstep --bench \
    --saves ../build-probe/audit-visit-saves --script ../tests/game/visit_save_load.script
if [ "$(grep -c '^zoo: loaded ' ../build-probe/finish-asan/visit-load-twice.log)" -ne 2 ]; then
    echo "visit-load-twice did not load exactly twice" >&2
    failed=1
fi
run ticks-20000 0 --ticks 20000 --frames 5
# A file in place of a directory is a portable, deterministic failed-write case.
touch ../build-probe/audit-not-a-directory
run refused-save 0 --frames 110 --lockstep --saves ../build-probe/audit-not-a-directory/saves \
    --script ../tests/game/save_refused.script
run leaks 1 --stand habitat:lion --frames 5
exit "$failed"
