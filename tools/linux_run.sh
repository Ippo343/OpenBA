#!/usr/bin/env bash
# tools/linux_run.sh -- one unattended route of the LINUX game exe (Phase D.4 LINUX-1).
#
# The Linux twin of what phase_d/gate.sh's route(), worldcmp.sh and tools/resave_gate do on
# Windows, run in a Linux game directory of its own (never C:/GOG Games/Banished). The
# environment each route sets is the Windows route's, plus DXVK's own variables, which DXVK
# reads and our code never does (D4_RULES C11).
#
# usage (inside WSL; the checkout's scripts are CRLF on Windows, so strip them):
#   bash <(tr -d '\r' < tools/linux_run.sh) <route> <gamedir> <outdir> [secs]
#   routes:  menu     boot to the main menu, click Quit (BANISHED_AUTOMENU_BUTTON=6)
#            newgame  autostart a new game, pause-menu Quit at frame 600 (gate.sh newgame)
#            world    newgame with BANISHED_SEED=12345 (worldcmp.sh's run; writes world.txt)
#            resave   load Gate.sav, roundtrip-resave, pause-menu Quit (resave_gate.sh roundtrip)
#
# <gamedir> holds reimpl-x64, libSDL3.so.0, libdxvk_d3d11.so.0 / libdxvk_dxgi.so.0 and a
# WinData link to the retail packages (tools/linux_gamedir.sh builds it). The exe reads
# its data relative to its own path (/proc/self/exe) and writes its logs to the working
# directory, which is <gamedir>.
#
# Polls standalone_boot_state.txt the way tools/boot_state.sh does (never a grep loop):
# prints each state change, stops waiting when the process exits, kills it after [secs].
# Writes <outdir>/{route.txt, the four standalone logs, the DXVK logs, frames.txt}.
set -u
ROUTE="${1:?route}"; G="${2:?gamedir}"; OUT="${3:?outdir}"; SECS="${4:-1800}"
mkdir -p "$OUT"
G="$(cd "$G" && pwd)"; OUT="$(cd "$OUT" && pwd)"

ENVV=(DXVK_WSI_DRIVER=SDL3
      VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
      DXVK_LOG_LEVEL=info DXVK_LOG_PATH="$G" DXVK_SHADER_CACHE=0
      LD_LIBRARY_PATH="$G")
case "$ROUTE" in
  menu)    ENVV+=(BANISHED_AUTOMENU_BUTTON=6) ;;
  newgame) ENVV+=(BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120) ;;
  world)   ENVV+=(BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120) ;;
  resave)  ENVV+=(BANISHED_MENU_SCENE=0 BANISHED_AUTOLOAD=Gate BANISHED_SAVE_TEST=roundtrip
                  BANISHED_PAUSEMENU=300 BANISHED_PAUSEMENU_QUIT=120) ;;
  *) echo "unknown route $ROUTE"; exit 2 ;;
esac
[ -n "${EXTRA_ENV:-}" ] && ENVV+=($EXTRA_ENV)

cd "$G" || exit 2
rm -f standalone_{debug,error,crash,hang}.log standalone_boot_state.txt reimpl-x64_d3d11.log reimpl-x64_dxgi.log \
      standalone_roundtrip_loaded.bin standalone_roundtrip_resaved.bin
T0=$(date +%s.%N)
env "${ENVV[@]}" ./reimpl-x64 & PID=$!
echo "[linux_run] $ROUTE pid $PID env: ${ENVV[*]}" | tee "$OUT/route.txt"

last=""; t_running=""; f_running=""; t_last=""; f_last=""
deadline=$(( $(date +%s) + SECS ))
while kill -0 $PID 2>/dev/null; do
  if [ "$(date +%s)" -ge "$deadline" ]; then
    echo "[linux_run] ALIVE after ${SECS}s: kill -9" | tee -a "$OUT/route.txt"; kill -9 $PID; break
  fi
  line="$(cat standalone_boot_state.txt 2>/dev/null)"
  st="$(printf '%s' "$line" | sed -n 's/.*STATE=\([A-Z_]*\).*/\1/p')"
  fr="$(printf '%s' "$line" | sed -n 's/.*[ =]frame=\([0-9]*\).*/\1/p')"
  now=$(date +%s.%N)
  if [ -n "$st" ] && [ "$st" != "$last" ]; then
    echo "[linux_run] $(printf '%7.1f' "$(echo "$now - $T0" | bc)")s $line" | tee -a "$OUT/route.txt"
    last="$st"
    if [ "$st" = RUNNING ] && [ -z "$t_running" ]; then t_running=$now; f_running=${fr:-0}; fi
  fi
  if [ -n "$fr" ]; then t_last=$now; f_last=$fr; fi
  sleep 2
done
wait $PID 2>/dev/null; RC=$?
T1=$(date +%s.%N)
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log standalone_boot_state.txt \
         reimpl-x64_d3d11.log reimpl-x64_dxgi.log standalone_roundtrip_loaded.bin standalone_roundtrip_resaved.bin; do
  [ -f "$f" ] && cp -f "$f" "$OUT/$f"
done
crash=$(stat -c %s standalone_crash.log 2>/dev/null || echo absent)
hang=no; [ -f standalone_hang.log ] && hang=YES
{
  echo "[linux_run] $ROUTE: exit $RC crashlog=$crash hang=$hang wall=$(printf '%.1f' "$(echo "$T1 - $T0" | bc)")s"
  if [ -n "$t_running" ] && [ -n "$t_last" ]; then
    dt=$(echo "$t_last - $t_running" | bc); df=$(( f_last - f_running ))
    if [ "$df" -gt 0 ]; then
      echo "[linux_run] frames after RUNNING: $df in ${dt}s = $(echo "scale=1; $df / $dt" | bc) fps, $(echo "scale=1; 1000 * $dt / $df" | bc) ms/frame (boot-state heartbeat)"
    fi
  fi
  grep -h '\[ExtLoad\] calls=' standalone_error.log standalone_debug.log 2>/dev/null | tail -1
  grep -h 'OnCommand\] -> App::Quit' standalone_error.log standalone_debug.log 2>/dev/null | tail -1
  grep -h -i 'Device: \|GPU\b\|Vulkan version\|D3D11InternalCreateDevice\|Feature level' reimpl-x64_d3d11.log reimpl-x64_dxgi.log 2>/dev/null | head -8
} | tee -a "$OUT/route.txt"
if [ "$ROUTE" = world ]; then
  cat standalone_error.log standalone_debug.log 2>/dev/null \
    | grep -E '^\[worldgen|^\[RegionSeed\]|^\[PfSeed\]|^\[startTown|^\[NewGameDialog::StartGame\]|^\[TownCensus\]|^\[GUL::Load\] ENTER' \
    | sed -E 's/(0x)?[0-9A-Fa-f]{9,16}/PTR/g; s/overlayVB\(\+0x160\)(\.size)?=[^ ]+/overlayVB=X/' \
    | sed -E 's/\(nil\)/PTR/g' > "$OUT/new.world.txt"
  # worldcmp.sh's two seds, then one for glibc: a null %p prints "(nil)" where MinGW prints
  # sixteen zeros, which the first sed already turns into PTR.
  echo "[linux_run] world: $(wc -l < "$OUT/new.world.txt") line(s), md5 $(md5sum "$OUT/new.world.txt" | cut -c1-8)" | tee -a "$OUT/route.txt"
fi
if [ "$ROUTE" = resave ]; then
  grep -h '\[Roundtrip\] RESULT=' standalone_error.log | tail -1 | tee -a "$OUT/route.txt"
  [ -f standalone_roundtrip_resaved.bin ] && echo "[linux_run] resaved md5 $(md5sum standalone_roundtrip_resaved.bin | cut -c1-32) ($(stat -c %s standalone_roundtrip_resaved.bin) bytes)" | tee -a "$OUT/route.txt"
fi
exit $RC
