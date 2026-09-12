#!/usr/bin/env bash
# Capture one in-game frame from reimpl-x64.exe under RenderDoc, for ANY arm.
#
#   capture_arm.sh <tag> [--frame N] [--expect-log REGEX] [--out DIR] -- ENV=V ...
#
# This is the generalisation of tools/dx11_state_diff/capture.sh, whose two arms
# are hard-wired to the VideoDX11 A/B seam flags. A Runtime-port lane's arms are
# its own BANISHED_RT_* kill switch, so the environment is a parameter here.
#
# WHY A SEPARATE SCRIPT AND NOT A PIXEL SHOT
# ------------------------------------------
# Wave 23 measured that seed + camera pin do NOT give a reproducible frame: five
# runs at identical seed, pin and command line split into two clusters ~98%
# apart, independently of the flag under test. What this captures instead is the
# COMMAND STREAM, which trace_oracle compares with set-based screens that need no
# frame determinism.
#
# --expect-log IS NOT OPTIONAL IN PRACTICE
# ----------------------------------------
# A run that silently did not arm your flag produces a perfectly good capture and
# a perfectly clean comparison. Pass a regex that only the armed arm prints (an
# attendance counter, a [port] line) and this script REJECTS the capture if the
# log does not contain it. Without it you are comparing an arm against itself and
# the tool has no way to tell you.
#
# Requires the game-directory lock:  tools/game_dir_lock.sh acquire <LANE> <secs>
# -- acquire it BARE, never piped (a pipeline reports the LAST command's status,
# so a timeout reads as a GRANT).
set -u

GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
RDCMD="${RENDERDOCCMD:-C:/Program Files/RenderDoc/renderdoccmd.exe}"

TAG="${1:?usage: capture_arm.sh <tag> [--frame N] [--expect-log RE] [--out DIR] -- ENV=V ...}"
shift
FRAME=150
OUT="$GAME/rdoc_trace"
EXPECT=""

while [ $# -gt 0 ]; do
  case "$1" in
    --frame)      FRAME="$2"; shift 2 ;;
    --expect-log) EXPECT="$2"; shift 2 ;;
    --out)        OUT="$2";   shift 2 ;;
    --)           shift; break ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
EXTRA_ENV=("$@")

[ -x "$RDCMD" ] || { echo "no renderdoccmd at $RDCMD" >&2; exit 2; }
[ -f "$GAME/reimpl-x64.exe" ] || { echo "no reimpl-x64.exe in $GAME" >&2; exit 2; }

mkdir -p "$OUT"
cd "$GAME" || exit 9

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

reap; sleep 1
rm -f "$GAME"/reimpl_capture*.rdc standalone_debug.log standalone_error.log \
      standalone_crash.log 2>/dev/null

# Record exactly what this arm was, beside the capture. A capture whose arm is
# not written down is not comparable later.
{
  echo "tag=$TAG"
  echo "frame=$FRAME"
  echo "expect=$EXPECT"
  echo "exe_md5=$(md5sum "$GAME/reimpl-x64.exe" 2>/dev/null | cut -d' ' -f1)"
  echo "when=$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  for e in "${EXTRA_ENV[@]:-}"; do [ -n "$e" ] && echo "env=$e"; done
} > "$OUT/$TAG.arm.txt"

echo "=== capture '$TAG' frame=$FRAME env=${EXTRA_ENV[*]:-<none>} ==="

env "${EXTRA_ENV[@]:-BANISHED_TRACE_ORACLE=1}" \
    BANISHED_AUTOSTART=1 \
    BANISHED_CAPTURE_INGAME="$FRAME" \
    "$RDCMD" capture --opt-ref-all-resources \
      -d "$GAME" -c "$GAME/reimpl_capture" \
      "$GAME/reimpl-x64.exe" >/dev/null 2>&1 &

WAIT_ITERS="${CAPTURE_WAIT_ITERS:-450}"   # x2s -> 15 min default
rdc=""
for t in $(seq 1 "$WAIT_ITERS"); do
  sleep 2
  cand=$(ls -t "$GAME"/reimpl_capture*.rdc 2>/dev/null | head -1)
  if [ -n "$cand" ]; then
    s1=$(stat -c%s "$cand" 2>/dev/null || echo 0)
    sleep 3
    s2=$(stat -c%s "$cand" 2>/dev/null || echo 0)
    if [ "$s1" = "$s2" ] && [ "$s1" != "0" ]; then rdc="$cand"; break; fi
  fi
  alive || { echo "  process exited before a capture appeared" >&2; break; }
  [ $((t % 30)) = 0 ] && echo "  ...$((t * 2))s elapsed"
done

reap; sleep 1

if [ -z "$rdc" ]; then
  echo "FAILED: no capture produced for '$TAG'" >&2
  [ -s standalone_crash.log ] && { echo "--- crash log ---"; head -20 standalone_crash.log; }
  exit 1
fi

bad=0
if [ -s "$GAME/standalone_crash.log" ]; then
  echo "REJECTED: non-empty crash log" >&2; bad=1
fi
if [ -n "$EXPECT" ]; then
  if grep -qE "$EXPECT" standalone_debug.log standalone_error.log 2>/dev/null; then
    echo "  arm confirmed: log matches /$EXPECT/"
  else
    echo "REJECTED: log does NOT match /$EXPECT/ -- this arm did not take effect," >&2
    echo "          so comparing it would compare an arm against itself." >&2
    bad=1
  fi
else
  echo "  WARNING: no --expect-log given. NOTHING verified that this arm armed." >&2
fi

cp -f "$rdc" "$OUT/$TAG.rdc" || exit 1
cp -f standalone_debug.log "$OUT/$TAG.debug.log" 2>/dev/null
cp -f standalone_error.log "$OUT/$TAG.error.log" 2>/dev/null
rm -f "$GAME"/reimpl_capture*.rdc

echo "  -> $OUT/$TAG.rdc ($(( $(stat -c%s "$OUT/$TAG.rdc") / 1024 / 1024 )) MB)"
[ "$bad" = "1" ] && { echo "  ^ capture kept, but DO NOT COMPARE IT" >&2; exit 3; }
exit 0
