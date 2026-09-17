#!/usr/bin/env bash
# Local verification for pc-port changes (CI minutes are not available, so
# this is the merge gate). Run from the repo root of any checkout/worktree:
#
#     port/tools/check.sh            # everything
#     port/tools/check.sh --quick    # skip the matching build
#
# Needs .venv and orig/GALE01 (the user's disc) in the checkout; worktrees
# can symlink both from the main checkout.
#
# Exit status 0 only if every gate passes:
#   1. matching build: build/GALE01/main.dol sha1 is the retail one
#      (--quick skips it once build/GALE01/include exists)
#   2. port build (configure + ninja), plus the Windows cross build
#      (x86-windows-gnu, link only)
#   3. headless forced match (Fox vs Marth, line stage) reaches 9000
#      retraces and exits 0
#   4. the same match on Battlefield (GrNBa.dat), where a fighter must also
#      stand on a platform (on the ground above y=0)
# It also reports (without failing) whether the fighter traces differ from
# port/tests/demo_line.trace and port/tests/demo_bf.trace, and the
# swap-audit warning count; a changed trace must be explained in the PR.
# --update-trace rewrites both baselines.

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

QUICK=0
UPDATE=0
for a in "$@"; do
    case "$a" in
    --quick) QUICK=1 ;;
    --update-trace) UPDATE=1 ;;
    *) echo "unknown option $a" >&2; exit 2 ;;
    esac
done

PY=.venv/bin/python
NINJA=.venv/bin/ninja
EXPECTED_SHA1=08e0bf20134dfcb260699671004527b2d6bb1a45
LOG=$(mktemp -d)
fail=0

step() { printf '== %s\n' "$*"; }
ok() { printf '   PASS %s\n' "$*"; }
bad() { printf '   FAIL %s\n' "$*"; fail=1; }

# The port build includes headers the matching build extracts from the
# user's DOL (build/GALE01/include), so this runs first on fresh checkouts.
if [ "$QUICK" = 0 ] || [ ! -d build/GALE01/include ]; then
    step "matching build"
    if (source .venv/bin/activate && python configure.py && ninja) >"$LOG/match.log" 2>&1; then
        sha=$(sha1sum build/GALE01/main.dol | cut -d' ' -f1)
        if [ "$sha" = "$EXPECTED_SHA1" ]; then
            ok "main.dol sha1 $sha"
        else
            bad "main.dol sha1 $sha != $EXPECTED_SHA1"
        fi
    else
        bad "matching build (log: $LOG/match.log)"
        tail -20 "$LOG/match.log"
    fi
fi

step "port build"
if $PY port/configure.py >"$LOG/port.log" 2>&1 && $NINJA -C port >>"$LOG/port.log" 2>&1; then
    ok "port/build/melee"
else
    bad "port build (log: $LOG/port.log)"
    tail -20 "$LOG/port.log"
fi

step "windows cross build (link only; can't run here)"
if PORT_TARGET=x86-windows-gnu $PY port/configure.py >"$LOG/win.log" 2>&1 &&
    $NINJA -C port -f build-x86-windows-gnu.ninja >>"$LOG/win.log" 2>&1; then
    ok "port/build-x86-windows-gnu/melee.exe"
else
    bad "windows build (log: $LOG/win.log)"
    tail -20 "$LOG/win.log"
fi

# $1 label, $2 MELEE_PORT_STAGE value ("" = the demo's stage, Battlefield),
# $3 baseline trace file. Leaves the run log in $LOG/<name>.log.
match_gate() {
    label=$1
    stage=$2
    baseline=$3
    name=$(basename "$baseline" .trace)
    step "headless forced match ($label, 9000 retraces)"
    MELEE_PORT_TRACE=60 MELEE_PORT_SWAP_AUDIT=1 MELEE_PORT_DEMO_MATCH=1 \
        MELEE_PORT_STAGE="$stage" MELEE_PORT_INPUT="60:A,120:A" \
        MELEE_PORT_FRAMES=9000 timeout 600 port/build/melee \
        >"$LOG/$name.log" 2>&1
    rc=$?
    if [ $rc = 0 ] && grep -q "reached MELEE_PORT_FRAMES=9000" "$LOG/$name.log"
    then
        ok "exit 0 at 9000 retraces"
    else
        bad "run exit $rc (log: $LOG/$name.log)"
        grep -v "no type known\|retrace\|^\[audit\]" "$LOG/$name.log" | tail -15
    fi
    grep -E '^\[port\] (f[0-9]+ (p[0-9]|item spawn)|p[0-9] kind [0-9]+ attrs)' \
        "$LOG/$name.log" | sed 's/^\[port\] //' >"$LOG/$name.trace"
    step "trace vs $baseline (informational)"
    if [ "$UPDATE" = 1 ]; then
        mkdir -p port/tests && cp "$LOG/$name.trace" "$baseline"
        echo "   baseline updated ($(wc -l <"$baseline") lines)"
    elif [ -f "$baseline" ]; then
        if cmp -s "$LOG/$name.trace" "$baseline"; then
            echo "   unchanged"
        else
            echo "   CHANGED: first differences:"
            diff "$baseline" "$LOG/$name.trace" | head -10
        fi
    else
        echo "   no baseline yet"
    fi
    echo "   'no type known' warnings: $(grep -c 'no type known' "$LOG/$name.log")"
}

if [ -x port/build/melee ]; then
    match_gate "line stage" line port/tests/demo_line.trace
    match_gate "Battlefield" "" port/tests/demo_bf.trace
    # Battlefield's platforms: a fighter has to stand above y=0 at some
    # point, which the flat line stage can never show.
    step "Battlefield platforms"
    if grep -Eq 'gnd pos \([^,]*, [1-9][0-9]*\.[0-9]+\)' "$LOG/demo_bf.trace"
    then
        ok "a fighter stands on a platform (y > 0 on the ground)"
    else
        bad "no fighter ever stood above y=0 (log: $LOG/demo_bf.log)"
    fi
fi

echo "logs: $LOG"
if [ $fail = 0 ]; then
    echo "ALL GATES PASSED"
else
    echo "GATES FAILED"
fi
exit $fail
