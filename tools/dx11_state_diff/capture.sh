#!/usr/bin/env bash
# Capture one in-game frame from reimpl-x64.exe under RenderDoc, for the
# per-draw state comparison. Usage:
#
#   capture.sh <tag> shipped|standalone [frame]
#
# The two arms:
#   shipped     AB mode with every lane flag off -> the shipped VideoDX11 DLL's
#               own vtable slots run. This is the reference.
#   standalone  the default: our twelve vtables in reimpl-x64.exe, DLL not loaded.
#
# Captures use --opt-ref-all-resources so the resource list is the live set
# rather than only what this frame happened to touch (frame-referenced counts
# swing 94-99 between identical runs and are not evidence of anything).
#
# The run is verified, not assumed: Video::Device answers a failed DX11 bring-up
# by silently rendering the whole session on VideoDX9-x64.dll, which otherwise
# looks like success. Any run whose log does not confirm the expected renderer
# is rejected here rather than being compared later.
set -u

GAME="C:/GOG Games/Banished"
OUT="$GAME/rdoc_ab"
RDCMD="C:/Program Files/RenderDoc/renderdoccmd.exe"
SEED="${BANISHED_SEED:-12345}"

TAG="${1:?usage: capture.sh <tag> shipped|standalone [frame]}"
ARM="${2:?usage: capture.sh <tag> shipped|standalone [frame]}"
FRAME="${3:-150}"

case "$ARM" in
  shipped)
    MODE_ENV=(BANISHED_DX11_AB=1
              BANISHED_DX11_BUFFERS=0 BANISHED_DX11_SHADERS=0
              BANISHED_DX11_IMAGES=0  BANISHED_DX11_STATE=0
              BANISHED_DX11_CONTEXT=0 BANISHED_DX11_DEVICE=0
              BANISHED_DX11_DEVICE_STUBS=0 BANISHED_DX11_EXPORTS=0
              BANISHED_DX11_IB_BIND=0)
    WANT_MODULE="VideoDX11-x64.dll" ;;
  standalone)
    MODE_ENV=()
    WANT_MODULE="reimpl-x64.exe" ;;
  *) echo "arm must be 'shipped' or 'standalone'" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
cd "$GAME" || exit 9

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

reap; sleep 1
rm -f "$GAME"/reimpl_capture*.rdc standalone_debug.log standalone_error.log standalone_crash.log 2>/dev/null

echo "=== capture '$TAG' arm=$ARM seed=$SEED frame=$FRAME ==="
env "${MODE_ENV[@]}" \
    BANISHED_AUTOSTART=1 \
    BANISHED_SEED="$SEED" \
    BANISHED_CAPTURE_INGAME="$FRAME" \
    "$RDCMD" capture --opt-ref-all-resources \
      -d "$GAME" -c "$GAME/reimpl_capture" \
      "$GAME/reimpl-x64.exe" >/dev/null 2>&1 &

# Wait for the capture file to appear and stop growing. This is slow: the
# standalone build logs every window message with an fflush, so reaching frame
# $FRAME in-game takes minutes, not seconds.
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
  if ! alive; then
    echo "  process exited before a capture appeared" >&2
    break
  fi
  if [ $((t % 30)) = 0 ]; then
    st=$(grep -c "\[RenderDoc\] capture" standalone_debug.log 2>/dev/null || echo 0)
    echo "  ...${t}0s elapsed, renderdoc capture events so far: $st"
  fi
done

reap; sleep 1

if [ -z "$rdc" ]; then
  echo "FAILED: no capture produced for '$TAG'" >&2
  [ -s standalone_crash.log ] && { echo "--- crash log ---"; head -20 standalone_crash.log; }
  exit 1
fi

# ── verify the run rendered on the renderer we asked for ────────────────────
log="$GAME/standalone_debug.log"
err="$GAME/standalone_error.log"
mode_line=$(grep -m1 "\[Dx11Seam\] mode:" "$log" 2>/dev/null)
vtable_line=$(grep -m1 "active vtable lives in:" "$log" 2>/dev/null)
slots_line=$(grep -m1 "\[Dx11Seam\] \(Installed\|Standalone\):" "$log" 2>/dev/null)
fallbacks=$(grep -m1 "fallbacks to shipped code:" "$log" "$err" 2>/dev/null | head -1)

echo "  $mode_line"
echo "  $slots_line"
echo "  $vtable_line"
echo "  $fallbacks"

bad=0
case "$vtable_line" in
  *"$WANT_MODULE"*) ;;
  *) echo "REJECTED: expected the active DeviceData vtable in $WANT_MODULE" >&2; bad=1 ;;
esac
if [ -s "$GAME/standalone_crash.log" ]; then
  echo "REJECTED: non-empty crash log" >&2; bad=1
fi

cp -f "$rdc" "$OUT/$TAG.rdc" || exit 1
cp -f "$log" "$OUT/$TAG.debug.log" 2>/dev/null
cp -f "$err" "$OUT/$TAG.error.log" 2>/dev/null
rm -f "$GAME"/reimpl_capture*.rdc

sz=$(stat -c%s "$OUT/$TAG.rdc")
echo "  -> $OUT/$TAG.rdc ($((sz / 1024 / 1024)) MB)"
[ "$bad" = "1" ] && { echo "  ^ capture kept, but DO NOT COMPARE IT" >&2; exit 3; }
echo "  verified: rendered on $WANT_MODULE"
exit 0
