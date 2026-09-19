# B.3.4 caller batch brief (batches B, C, D) — coordinator, 2026-09-19

You convert every call through an IO::Serializer DOOR in YOUR directories into a member call on the
real class `IO::Serializer` (runtime/types/io/Serializer.h). Batch A (code/Game/Component, commit
e7498818 on branch b34-callers-a) is the reference: read `git log -1 --format=%B e7498818` IN FULL
first — its per-door table, worked example per call shape, removed-line scan and TRAPS section are
your recipe. Then read docs/runtime_port/endgame_b/B34_RULES.md §1.2, §6.4, §6.5, §7.2, §7.3 (same
authority as CLAUDE.md) and CLAUDE.md's hard rules.

SETUP: your worktree base is STALE. `git checkout -b <your-branch> && git merge main` first, and
`git merge main` again before your final build.

RULES (each one cost a mistake already):
1. SCOPE BY THE RESOLVER, never by name. Sweep every `ResolveAddr(Module::Exe, <slot>)` for slots
   0x140152ad8 CTOR, ad0 DTOR, a48 GetVersion, ac8 int&, a58 bool&, a60 float&, a50 uint&, 468 short&,
   418 uchar&, 410, 488, 3f8, 490, 618, 478, 0x140151278 (n,int*), 0x140151458 (n,bool*),
   0x140151ea8 (n,float*), 0x140152400 (n,uchar*), plus every mangled `@Serializer@IO@@` string, plus
   the global doors g_fn_Serializer_*/g_fn_Serialize_*/fn_Serializer_*/SerializerEncodingVCall, and
   trace each to the variable it lands in (file-statics in any spelling, block-locals).
2. THE DOOR DECIDES THE OVERLOAD, never the argument's type and never the static's name. Arguments
   are mostly untyped `s + 0xNN`; spell the door's type at the call:
   `reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(p), name);`
   Cast-free only when the argument's declared type is EXACTLY the door's type; audit each.
   Check arity of each local typedef against the door (Ghidra drops args).
3. Ctor/Dtor: placement-new into the SAME raw buffer at the SAME statement, explicit
   `->~Serializer()` at the SAME statement as the old dtor call; add `alignas(8)` to the LOCAL
   buffer. Never a typed local. A buffer that is a struct member, carved from a larger array, or
   < 0x10 bytes is a REPORT, not a conversion.
4. GUARDS STAY (`if (g_fn_X) ...` keeps its condition); pointers, typedefs, statics, resolver
   blocks stay. You delete nothing; the closing pass does.
5. HAND-INLINED ReaderWriter slot calls (`*(void**)(ser + 8)` -> vtable -> +0xNN, often misnamed
   "Encoding"/"Stream"; several carry our null guards) stay BYTE-IDENTICAL. `serializer+8` is the
   READERWRITER. List every one (file:line, offset, guards) in the commit message. Raw
   `*(int*)(*(uintptr_t*)ser + 8)` direction reads: list, do not convert.
6. A call inside a printf argument is not converted. Pointer-class doors
   (fn_ExternalPtr_*Serialize*, fn_InternalBasePtr_Serialize, fn_ExternalRefArray_Serialize) are
   batch G: leave them, do not list them.
7. Never fix a faithful crash with a null check. Never getenv. No new BANISHED_* flags. Never
   `find /` or unscoped searches (Grep/Glob scoped to the worktree). No pacman. Never hand-edit
   `.inc` files. Build with /c/msys64/ucrt64/bin/mingw32-make.exe.
8. Edit ONLY files under your directories. Anything outside: REPORT.
9. You may fan out to sonnet sub-workers by file shard (generate shards programmatically from the
   measured scope; diff the union against the scope). Sub-workers report to YOU by your exact agent
   name — put that name in their prompt — and get their own scratchpad/<batch>/<worker>/ directory.
   You verify the final diff yourself, not their reports.

VERIFY before reporting: standalone-debug, standalone-perf, unit exit 0, zero new warnings in
changed files; `python tools/dispatch_slot_map.py --check` 0 mismatches; removed-line scan (every
removed line is a door call or a buffer decl; any removed line with serObj/encVtable/+0x98 math/
a changed `if` condition is a FAILURE); removed CALLs == added member calls, per door; ctor count
== dtor count == alignas count. Game runs: take tools/game_dir_lock.sh with your agent name, block
in the FOREGROUND, release after; never break a lock, never run unlocked. Resave gate
(tools/resave_gate/resave_gate.sh Gate): copy the git-ignored golden from
C:\code\GIT\untitled_rev_eng_project\docs\runtime_port\resave_gate\golden\Gate.resaved.bin into
your worktree; need `verdict: PASS` (not GOLDEN CREATED) 3 CONSECUTIVE at 21075114 bytes. A single
DIFF with same size and only the resident-row count/ids differing is the known loader race: re-run,
never bisect on one DIFF. One clean new-game boot (BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120), polled with tools/boot_state.sh --wait 300; read
standalone_crash.log first if it dies. `[GUL::Help] topics=-1`/openCompFail is a known pre-existing
intermittent (docs/runtime_port/endgame_b/LOADER_READ_SPIN.md): not yours, do not guard it.

ONE commit, title `ENDGAME B.3.4 callers batch <X>: <dirs>`, body in batch A's format (per-door
table, reports, not-converted list, verification, gate coverage honesty), ending with
`Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. No push, no merge to main. Report the
hash to `main` via SendMessage.
