#!/usr/bin/env bash
# Build docs/runtime_port/wave25_alloc_logs/EXTRACTS.txt from the archived cell logs.
# Same precedent as Wave 10's shadowbisect_logs/EXTRACTS.txt: the raw logs are
# gitignored (*.log) and ~900K, so the decisive lines are committed instead -- read
# from the archived log, never from a live `tail`.
set -u
cd "$(dirname "$0")/.." || exit 1
L=docs/runtime_port/wave25_alloc_logs
OUT="$L/EXTRACTS.txt"
PAT='ATTEST|RtAllocShadow\]|NEGATIVE CONTROL|GAP DETECTOR|LEGACY CONTROL|\[REDZONE\]|watchdog (thread started|stopped)|REDZONE-SELFTEST|corruption at checkpoint'

{
    echo "WAVE 25 LANE ALLOC -- decisive lines per cell."
    echo "Raw logs are gitignored; re-make with tools/wave25_alloc_extract.sh."
    echo
    for f in w25_shadow_base w25_gapctl w25_negctl_size w25_legacy_directfree \
             w25_armed_redzone_1 w25_armed_redzone_autoload w25_unarmed_redzone_autoload; do
        echo "=== $f ==="
        if [ -f "$L/${f}_error.log" ]; then
            grep -E "$PAT" "$L/${f}_error.log"
            if [ -s "$L/${f}_crash.log" ]; then
                echo "--- crash log (first 14 lines) ---"
                head -14 "$L/${f}_crash.log"
            else
                echo "--- no crash log (clean exit) ---"
            fi
        else
            echo "(log absent)"
        fi
        echo
    done
} > "$OUT"
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
