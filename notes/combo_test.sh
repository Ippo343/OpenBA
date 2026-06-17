#!/usr/bin/env bash
# Session 5: isolate lock vs FS-relative. menu-drawn = [FrameLimiter] fps in
# stderr (FrameLimiter::Tick is called ONLY from MenuDLT::Draw, so it proves the
# menu is drawing, not just the loop running). CRASH = process death.
cd "/c/GOG Games/Banished" || exit 9
N="${N:-4}"; MAXW="${MAXW:-22}"
alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }
trial() { # $1=label $2=extra-env
  local c=0 drawn=0 stall=0
  for i in $(seq 1 $N); do
    reap; sleep 1; rm -f standalone_error.log
    env $2 ./reimpl-x64.exe >/dev/null 2>&1 &
    local dead=1; for t in $(seq 1 $MAXW); do sleep 1; if ! alive; then dead=0; break; fi; done
    local fps=$(grep -c 'FrameLimiter\] fps' standalone_error.log 2>/dev/null || echo 0)
    reap
    if [ "$dead" = 0 ]; then c=$((c+1));
    elif [ "${fps:-0}" -ge 2 ]; then drawn=$((drawn+1)); else stall=$((stall+1)); fi
  done
  echo "[$1] crash=$c menu-drawn=$drawn stall=$stall (of $N)"
}
trial "A lock=ON  fsrel=ON " ""
trial "B lock=ON  fsrel=OFF" "BANISHED_NO_FS_RELATIVE=1"
trial "C lock=OFF fsrel=ON " "BANISHED_NO_FOL_LOCK=1"
trial "D lock=OFF fsrel=OFF" "BANISHED_NO_FOL_LOCK=1 BANISHED_NO_FS_RELATIVE=1"
reap
