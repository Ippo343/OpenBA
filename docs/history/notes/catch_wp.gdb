set pagination off
set confirm off
set breakpoint pending on
set cwd C:/GOG Games/Banished
set environment BANISHED_REDZONE 1
# Break at the entry of the menu Geometry deserialize body (04d0).
break geometry_serialize.cpp:185
run
# Stopped at 04d0: the allocator singleton is live. Arm a conditional HW
# watchpoint on the bucket-12 free-list head (alloc+0x88), firing only when
# the stored value has any bit >=32 set (the deterministic +0x100000000 wild
# write; all legit heap pointers are < 0x100000000).
watch *(unsigned long long*)((char*)g_pMemAllocSingleton + 0x88) if ((*(unsigned long long*)((char*)g_pMemAllocSingleton + 0x88) >> 32) != 0)
# Remove the source breakpoint so repeated 04d0 calls don't re-stop us.
delete 1
printf "=== WP_ARMED alloc=%p head=0x%llx ===\n", g_pMemAllocSingleton, *(unsigned long long*)((char*)g_pMemAllocSingleton + 0x88)
continue
# We only get here if execution STOPPED (watchpoint fired, or a SIGSEGV).
printf "=== STOPPED ===\n"
printf "head now = 0x%llx\n", *(unsigned long long*)((char*)g_pMemAllocSingleton + 0x88)
info registers rip rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15
printf "=== BACKTRACE ===\n"
backtrace full
printf "=== DISAS AROUND PC ===\n"
x/8i $pc-24
printf "=== CORRUPT TAG BLOCK (r14) ===\n"
x/24gx $r14
printf "=== CORRUPT TAG BLOCK (r11) ===\n"
x/8gx $r11
printf "=== UnlinkFreeTag full disas ===\n"
disassemble $pc-185, $pc+24
printf "=== DONE ===\n"
quit
