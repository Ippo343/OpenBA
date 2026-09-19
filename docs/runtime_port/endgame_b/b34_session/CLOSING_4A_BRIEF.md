# B.3.4a closing pass brief (coordinator, 2026-09-19)

State of main when you start: every IO class block (core, STREAMS 1-6, PACKAGE+FS 1-6) and every caller
batch (A, A-residual, B, C, D, G, H, P) is merged. The single-loader fix is merged. Caller batches DELETED
NOTHING: every door pointer, typedef, file-static cache, resolver block and readiness guard is still there.
Your job is the deletions and the loose ends, in SEPARATE commits so each is attributable.

Read first, in full: CLAUDE.md; docs/runtime_port/endgame_b/B34_RULES.md (same authority; esp. §4.2, §6.4,
§6.5, §7); .claude/rules/address-registry.md; memory feedback "zero-caller sweep rules" is summarised here:
docs/comments/strings are NOT callers; wrapper call forms count; a call inside a printf argument counts.
Commit-message models: `git log -1 --format=%B` of e7498818, 920e78a7, 6d1c0d28.

## Commit 1 — the whole-tree residual sweep (proof, plus any conversions it finds)
Sweep code/, engine/, standalone/, runtime/, dx11/ for every remaining CALL through a Serializer door, by
BINDING not name. The binding spellings found so far (each one cost a batch a miss):
 1. `ResolveAddr(Module::Exe, <slot>)` on one line with the variable;
 2. helpers: RSlot(), ResolveSlotPtr<>(), ResolveSlot(...) template, loadExref(addr) lambda, ResolveFn<T>("mangled");
 3. `ResolveExport("…@Serializer@IO@@…")` by mangled name (sweep with string literals INTACT);
 4. two-statement indirection through a bare uintptr_t scratch local (`iat = ResolveAddr(...); if (iat) s_fn = *(T*)iat;`)
    — resolve POSITIONALLY: nearest preceding assignment wins, reassignment from an unrecognised source KILLS the
    binding (moveto.cpp rebinds one `iat` ten times; a whole-file closure mis-binds s_fn_IsDerivedFrom);
 5. plain alias `s_fn_X = g_fn_Y;` and uintptr_t round-trips;
 6. globals bound in engine/funcs/engine_funcs.cpp and called from a game dir (fn_Serializer_SerializeBulk ->
    0x140152400; fn_Ser_Slot_SerSizedRaw_400/_3F8, SerByte_410, SerUShortArr_488, SerByteArr_618,
    fn_Serializer_Serialize_Uint a50).
Slots: 0x140152ad8 CTOR, ad0 DTOR, a48 GetVersion, ac8 int&, a58 bool&, a60 float&, a50 uint&, 468 short&,
418 uchar&, 410, 488, 3f8, 490, 618 SerializeBytes, 478, 400 (n,uchar*), 0x140151278 (n,int*), 0x140151458
(n,bool*), 0x140151ea8 (n,float*), 0x140152aa0 InternalBasePtr::Serialize. A static's NAME and its COMMENT prove
nothing (s_fn_Serialize_Char binds the uchar door; s_fn_SerSChar is commented uchar but binds schar;
s_fn_IBP_Serialize in cleararea.cpp / ambientemittercomponent.cpp binds ExternalPtr EXE doors 0x140002340 /
0x1400019a0 — those stay).
Known remaining door calls, to convert here with the branch they sit in handled honestly:
 - code/Game/Game/geometry_serialize.cpp:152/:167 (slot -> global fallback in CallSerializerCtor/Dtor);
 - code/Game/Component/highlightcomponent.cpp:98/99 (ctor/dtor chosen by ternary, else arm prints a desync warning);
 - code/Game/Component/citizendescription.cpp:127 (block-local float[] door with an else fallback);
 - code/Game/Dialog/mod_dialog_vtables.cpp:381-383 (fn_Modification_GetCount inside a printf argument — hoist the
   call out of the printf into a local FIRST, then convert; the perf build strips printf arguments);
 - out-of-scope doors batch P left: game_settings_persist.cpp:53-59 (Persistence Get/Set statics),
   w2_takescreenshot.cpp:155-156 (?GetSaveDirectory@FileSystem — hidden-retslot by-value return; GetSaveDirectory
   is NOT yet a member, see Commit 4 — and ?Exists@FileSystem), game_main.cpp:400 (?ClosePackages@FileSystem).
NOT yours, leave byte-identical: the 125 sites through the twelve ExternalPtr EXE doors (exe-phase work);
work_base_walkpos.cpp:86 (a record COUNT, not a stream); RtSystem_Time_Serialize (+0x90 int64 — step 4b);
dx11/src/buffers.cpp:271; the unproved-receiver direction reads listed in d6ee7bfc.
End the commit message with the per-spelling count of remaining door CALLs (target: 0 plus the named exceptions).

## Commit 2 — guards and pointers removed TOGETHER
For every Serializer/InternalBasePtr/Modification/Persistence/FileSystem door pointer that now has ZERO CALLs
tree-wide: remove, in one change per door, the readiness guards that test it (`if (g_fn_X) <member call>` becomes
the bare member call — the guard tested a POINTER that the member call no longer uses; removing the pointer and
keeping the guard would not compile, keeping the pointer to feed the guard is dead weight), the file-static
caches, their typedefs, their resolver blocks, the engine_funcs.{h,cpp} declaration/definition/binding/_DEBUG
print, and the alias lines (Harvest `s_fn_Serializer_SerializeBoolArray` alias + typedef + its "same fn,
different usage" comment in harvestarea.cpp / harvestorchard.cpp; SerializerEncodingVCall in engine_funcs.h).
RULES: (a) the EXE DISPATCH SLOT REGISTRATIONS STAY — 0x140152xxx are PE import slots other unported exe code
may still read; never remove a RegisterDispatchSlot / address_registry row in this pass; `python
tools/dispatch_slot_map.py --check` must stay at 0 mismatches. (b) A guard that has an ELSE arm with behaviour
(a fallback write, a desync warning) is not a readiness guard: decide per site and say what you did. (c) A guard
whose condition ALSO tests something else keeps the other conjunct. (d) Zero-caller means zero CALLs by the
sweep rules above — comments, strings, typedefs and the pointer's own definition are not callers; a call inside a
printf argument IS a caller. (e) Zero-CALL pointers already known: fn_Persistence_SetBool/SetInt, fn_ModEntry_
CopyCtor/Assign/Dtor, fn_ModArrayCopy, fn_Struct60B_Serialize, fn_TradeWork_impl_04d0/f1a0, fn_MemZeroFill,
fn_Ser_Slot_SerUShortArr_488, fn_Ser_Slot_SerByteArr_618, engine/types/Modification.h:111-121 ModificationAddr
block — verify each by binding before deleting; fn_ExternalPtr_Serialize3 is the last reader of exe 0x140007140
and STAYS (ExternalPtr doors are not this pass). Do this per directory if the diff gets large (several commits
"2a/2b/…" are fine); removed-line scan on each: every removed line is a guard condition, a pointer decl/def, a
typedef, a resolver statement, or a comment about one of those.

## Commit 3 — two retail-backed behaviour fixes (own commit, small)
code/Game/Component/ambientemitterdescription.cpp:57 and code/Game/Game/toolbardescription.cpp:156 (paths approximate
— find them) read `*(int*)(*(void**)(serializer + 8) + 8)` = ReaderWriter::_allowCustomSerialize (always 0) as the
"loading" flag, so `loading` is stuck TRUE even when saving. Retail reads the DIRECTION: AmbientEmitterDescription::
Serialize 0x1400431f0 `MOV RAX,[RSP+0x28]; CMP dword [RAX+8],0`, ToolbarDescription::Serialize 0x14012dacd same —
i.e. serializer+0x00 -> stream, +8 -> direction. Fix = `reinterpret_cast<IO::Serializer*>(ser)->GetDirection()`
compared as retail compares it (check the branch sense against the disassembly quoted in 6d1c0d28's message; if you
need Ghidra: ONE instance, call get_current_program_info first, Application-x64.exe = 11473 functions, switch back
to Runtime-x64.dll = 15290 afterwards). This CAN change saved bytes if either description is written on the Gate
save path: say in the commit what you expect the resave gate to show and why.

## Commit 4 — small receiver leftovers
 - IO::FileSystem::GetSaveDirectory (0x1800acfd0, runtime/src/io/filesystem_win.cpp): the only FileSystem member
   left; take the file whole in the established shape (member + delegating door, registrations byte-identical;
   by-value Filename return = hidden-retslot door, see how GetDataDirectory did it in cd4e8921). Then convert
   w2_takescreenshot.cpp's caller.
 - RtIo_PackageFile_GetFileInfo (0x180025400, filesystem_openfile_win.cpp) and RtIo_PackageFile_GetFileName
   (0x180025450, filesystem_packages.cpp) are IO::PackageFile members living in FileSystem's files. PackageFile is
   POLYMORPHIC: the door must delegate with the QUALIFIED call `obj->IO::PackageFile::X(...)`. They are on the §D
   (loader open/read) path: bodies move VERBATIM — same control flow, same callees, same order; prove it with a
   keyword/callee tally in the message. If in doubt, leave them and say so.
 - Stale comments: runtime/types/ui/ElementDescription.h:48-49 (names deleted SerializerLite / RtObject::kEncSerialize*);
   runtime/types/io/FileSystem.h (a banner line still says ??1FileSystem is in iobase_vtables.cpp — it is in
   filesystem_createfile_win.cpp); code/Game/Component/citizencomponent.cpp:43 (claims 0x140152a60 is RandFloat; it is
   Serializer::Serialize(float&)); gamegrid_serialize.cpp s_fn_SerSChar comment (says uchar 0x140152418, binds the
   schar export); code/Game/Game/gameupdateloop_load.cpp loader-guard region ~894-924 and
   code/Game/Game/menudialogloopt_funcs.h:157 still describe "our AsyncLoader worker": dispatcher (1) is GONE
   (standalone/async_loader.* deleted in 96c17f47; Engine::Loader is the only dispatcher; the guard is now an
   assertion — a "[GUL::Load] redundant dispatch" line reappearing means a second dispatcher is back). COMMENTS ONLY
   in those two files. docs/runtime_port/endgame_b/B34_BRIEFS.md §D wrongly calls the block-0 exe an "unchanged
   tree" and predates the loader fix: add a dated note at its top rather than rewriting history.
 - Widen B34_RULES §7.2's grep to: any BARE stream/serializer/FS class name inside a `namespace IO {` block outside
   tests/ (it fired three times, always as a compile error), and note the POD namespace spelling is not uniform
   (RtIO in FileSystem.h; RtIo in Persistence.h, Registry.h, Stream.h).

## Hard rules
Never fix a faithful crash with a null check. Never getenv; no new BANISHED_* flags. No unscoped filesystem
searches. No pacman. Never hand-edit .inc files (regenerate with the repo's tools; restore rt_export_addrs.inc if
a tool rewrites it spuriously). Never `git stash`, amend, rebase, push. No sub-agents. Build with
/c/msys64/ucrt64/bin/mingw32-make.exe. Report only what you verified in the FINAL file, not what you intended.

## Verification per commit
standalone-debug, standalone-perf, unit exit 0 (NOTE: `standalone` and `standalone-debug` share
standalone/build/obj — running one after the other compiles nothing; perf is the real NDEBUG build); zero new
warnings in changed files (A/B against HEAD~ compiled in place); dispatch_slot_map --check 0; cutover manifest gate
OK; rt_mint_gen --check UP TO DATE; tools/runtime_census.py numbers quoted. Do NOT run the game and do NOT take
the game-dir lock: report each hash to `main` as it lands and the coordinator gates the merged tree (3 routes +
resave x5 + type table). Commit messages end with
Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
