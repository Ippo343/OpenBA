set pagination off
set confirm off
set cwd C:/GOG Games/Banished
set environment BANISHED_REDZONE 1
handle SIGSEGV stop nopass print
run
printf "=== CRASH ===\n"
printf "rip=%p r15(this)=%p\n", $pc, $r15
printf "count=%d data=%p slotAddr=%p slotVal=%p\n", *(int*)($r15+0x58), *(void**)($r15+0x50), (char*)*(void**)($r15+0x50)+8, *(void**)((char*)*(void**)($r15+0x50)+8)
quit
