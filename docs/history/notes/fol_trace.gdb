set pagination off
set confirm off
set breakpoint pending on
set cwd C:/GOG Games/Banished
set environment BANISHED_REDZONE 1
handle SIGSEGV stop nopass print
# FindOrLoad entry; RDX = const Filename& ; *(char**)RDX = path string.
# Break by symbol (pending) so it resolves at the ASLR'd address once Runtime loads.
break '?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z'
commands
  silent
  printf "FOL: %s\n", *(char**)$rdx
  continue
end
run
printf "=== STOPPED (crash) ===\n"
printf "head now\n"
bt
printf "=== material this (r15) +0x50 array, +0x58 count ===\n"
printf "count=%d data=%p\n", *(int*)($r15+0x58), *(void**)($r15+0x50)
quit
