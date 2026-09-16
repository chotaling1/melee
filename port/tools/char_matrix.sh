#!/usr/bin/env bash
# Run the forced headless match for every playable character against an
# opponent (default Fox, CharacterKind 2) on the line stage, in parallel,
# and print one result line per character.
#
#     port/tools/char_matrix.sh [OPPONENT] [RETRACES]
#
# The match starts at retrace ~6250 and a demo lasts 1200 frames, so the
# default 9250 retraces covers the whole first demo and part of the next.
# Columns: CharacterKind, exit status (124 = timeout/hang), last traced
# damage of the character and of the opponent. Logs stay in the printed
# directory.

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2
OPP=${1:-2}
FRAMES=${2:-9250}
OUT=$(mktemp -d)
for k in $(seq 0 25); do
    (
        MELEE_PORT_TRACE=600 MELEE_PORT_DEMO_MATCH="$k,$OPP" MELEE_PORT_STAGE=line \
            MELEE_PORT_INPUT="60:A,120:A" MELEE_PORT_FRAMES=$FRAMES \
            timeout 300 port/build/melee >"$OUT/$k.log" 2>&1
        echo $? >"$OUT/$k.rc"
    ) &
done
wait
fail=0
for k in $(seq 0 25); do
    rc=$(cat "$OUT/$k.rc")
    d0=$(grep -E "\] f[0-9]+ p0" "$OUT/$k.log" | tail -1 | grep -o "dmg [0-9.]*")
    d1=$(grep -E "\] f[0-9]+ p1" "$OUT/$k.log" | tail -1 | grep -o "dmg [0-9.]*")
    printf "ckind %2d exit %3s  p0 %-10s p1 %s\n" "$k" "$rc" "$d0" "$d1"
    [ "$rc" = 0 ] || fail=$((fail + 1))
done
echo "$((26 - fail))/26 exit 0; logs: $OUT"
