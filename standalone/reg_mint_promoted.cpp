// ============================================================================
// standalone/reg_mint_promoted.cpp — Wave 10 lane MINTSEL
//
// MINTED DATA ROWS THAT ARE LIVE ON EVERY RUN.
// ============================================================================
// A3-HOLD (ENDGAME A.3/A.4, 2026-09-13): the CODE below no longer reads any
// BANISHED_RT_* variable, but the per-row PROSE still quotes the flags and the
// diff oracle each promotion was measured with. That prose is the warrant -- the
// evidence record for the row -- and the RT_UNPROMOTABLE: tags in it are parsed
// by tools/cutover_manifest.py, which gates the build. Rewriting the warrants
// would destroy the provenance without changing any behaviour, so they stay.
// RtMintRegisterPromotedRows() is itself no longer called (see rt_data_mint.h).
//
//
// standalone/rt_data_mint.cpp mints 197 exported DATA symbols — 55 `??_7`
// vtables, 104 `?_typeInfo@` BaseTypeInfo and 38 `.data` statics — and until
// Wave 10 the only way to register any of them was `BANISHED_RT_MINT=1`, ONE
// switch over all 197. That switch is not a route to anything: with the DLL
// present it publishes our zeroed `?_singleton@MemoryAllocator@System@@` over
// the DLL's live one and dies in `EnterCriticalSection` on a null self before
// the UI exists. The blocker is in the 38 STORAGE rows and says nothing about
// the vtables or the BaseTypeInfos — but with one switch there was no way to
// say so with a measurement, and no way to act on it if you could.
//
// This file is the other end of that. A row listed here is registered
// unconditionally, on every run, with no env var — it is a PORT, by this
// project's own rule that a gated registration is not one. Getting onto this
// list is a claim, and the claim is paid for per row:
//
//   1. `BANISHED_RT_MINT_SHADOW=1` must report the row READY in the per-row
//      readiness tables (rt_data_mint.cpp). Those are LIVE screens: they ask
//      the port registry through the same OurPort() the mint fills with, so
//      they see a registration whose name arrives in a variable — the
//      literal-grep blindness this project has been burned by repeatedly.
//   2. The row must be run in a real boot and the run must reach the same
//      place the control reaches. `BANISHED_RT_MINT_ROWS=<csv>` registers a
//      candidate WITHOUT the storage rows that make the global switch fatal;
//      `BANISHED_RT_MINT_PROMOTED=0` turns this file off for the other half of
//      the A/B.
//   3. The warrant is written next to the row, naming what was measured.
//
// THE POINTER-IDENTITY QUESTION IS RETIRED — for all twelve rows, permanently.
// (WAVE 10C lane TIDENT; full census with addresses in
// docs/runtime_port/WAVE10C_LANE_TIDENT.md.)
//
// Lane DLGV declined to promote `?_typeInfo@DialogController@` because doing so
// makes `?TypeName@DialogController@` hand out OUR node's `name` POINTER, and
// noted the objection applied equally to the eleven rows below that were
// already live default-on — the screen that green-lit them, `strs=`, is a
// strcmp and is structurally incapable of expressing an identity answer. It is:
// `strs=0` is a true statement about the characters and says nothing about the
// object. That was the right thing to flag.
//
// It was answered from the binaries, not by argument:
//
//   * Runtime-x64.dll — 104 `?_typeInfo@` statics; the ONLY static reads of any
//     node's `+0x08` are 103 `?TypeName@` accessors, and ZERO of those 103 is
//     CALLED anywhere in the DLL (309 refs, all vtable slots + export entries).
//     `Factory::Startup` (0x1800c0630) CRCs the string — content.
//     `Factory::GetTypeName` (0x1800c0a60) has no caller in the DLL at all.
//   * Application-x64.exe — 525 references to the 42 imported `?_typeInfo@`
//     slots. Every consuming instruction reads the DWORD AT +0x00, the typeId.
//     NOT ONE reads +0x08. The exe does not import `?GetTypeName@Factory@`; it
//     imports `?GetTypeIdFromName@…@QEBAIPEBD_N@Z` (name -> CRC, content) and
//     `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` (by id).
//
// Zero pointer-identity consumers, in either binary. So the eleven rows below
// are SAFE AS SHIPPED and none is backed out; the question does not have to be
// re-asked per row.
//
// WHAT DID CHANGE. The difference is REAL, it is LIVE TODAY, and until this
// wave nothing in the tree said so: `UI::SpriteSheet::TypeName` compares the
// returned pointer and reports `calls=1 divergences=1`, which goes to 0 with
// `BANISHED_RT_MINT_PROMOTED=0` (calls=1 in both halves, so the VALUE moved and
// not the coverage), while the same code shape on the unpromoted
// `IO::PackageFile::TypeName` reads calls=4 divergences=0 in both. That is the
// string twin of the "Construct divergence is the vtable identity" paragraph
// further down, and it is now written beside the body in
// runtime/src/ui/spritesheet_vtable.cpp so the red line is not read as a
// regression. The shadow report also grew a `ptr=` column beside `strs=`,
// scored SEPARATELY and never folded into READY, with
// `BANISHED_RT_MINT_PTRALIAS=<n>` as its effectiveness control.
//
// WHY THE SYMBOL IS SPELLED TWICE PER ROW
//
// `RtMintPromotionStorage(sym, va)` does the checking — is it really a minted
// row, does the VA still match the generated table — and records the
// promotion. The two `Register*` calls are made HERE, with the symbol as a
// literal, because tools/runtime_census.py excludes rt_data_mint.cpp from the
// ported side entirely and its non-literal scan cannot see a name that arrives
// as `t.symbol` from a table walk. A promotion registered from inside the mint
// would be live in the process and invisible to the census — precisely
// reference_half_registered_port_reads_as_absent.md. The duplication is two
// adjacent tokens and it cannot survive a mismatch: the helper aborts if the
// symbol is not a minted row or if the VA disagrees with the generated table.
// ============================================================================

#include "resolve.h"
#include "rt_data_mint.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// THE TEN UI `?_typeInfo@` ROWS, PLUS AN ELEVENTH (Element) AND THREE `??_7`
// VTABLES — the vtables and the eleventh typeinfo were added by Wave 10B lanes
// SS, ELEM and DLGV, each with its own warrant below.
// ============================================================================
//
// These are the ten `?_typeInfo@…@UI@@` symbols that
// `tools/runtime_census.py --unported --namespace UI` lists as called-but-not-
// ported. They are promoted together because ONE fact makes all ten safe and
// it is the same fact for each, measured the same way.
//
// WHAT WAS WRONG WITH THEM, AND WHAT CHANGED
//
// Lane UIDATA's verdict was "blocked by a silent wrong value, and nothing
// short of cutover fixes it": the mint stamped `BaseTypeInfo::typeId` with the
// ctor sentinel 0xFFFFFFFF, and because our nodes hang off `g_mintTypeInfoHead`
// — a chain no `Factory::Startup` walks — the sentinel was not a temporary
// state, it was the final value. Every consumer of these symbols in this tree
// reads exactly that dword. Registering them would have handed all of them
// 0xFFFFFFFF with no crash and no log line.
//
// That reasoning was right about the consequence and wrong about the
// remedy. The id is not information the DLL holds that we cannot recover: it
// is a pure function of `name`, and we already own that function. Retail's
// private ?GetTypeFromNameCRC@Factory@Object@@AEBAIPEBD@Z is transcribed in
// runtime/src/object/type_system.cpp, and `Factory::Startup`'s own line is
// `info->typeId = TypeNameCRC(info->name)`. MintTypeInfos() now runs that same
// line (through RtObject::TypeIdFromName) when it lays each node out. Nothing
// new is claimed about retail; Startup's line runs earlier, on a node Startup
// will never reach, and the value it writes is the value Startup would write.
//
// THE MEASUREMENT (2026-09-08, debug build, DLL present, autostart + pause-menu
// quit, under tools/game_dir_lock.sh)
//
//  * `[RtMintShadow] BaseTypeInfo: 624 field comparisons, 0 MISMATCH,
//    0 typeId not-yet-comparable`. The typeId is no longer an INFO line: it is
//    compared against the loaded DLL's own node for all 104 rows, and the
//    comparison is skipped-and-counted (never passed) when the DLL's own
//    Startup has not run yet, so an early report cannot read as a pass.
//    All 104 agree. Ten of ten UI rows report
//    `READY  … idcmp=yes strs=0`.
//  * The negative control is built in and did not have to be invented: the
//    hash widens the name to UTF-16LE before CRC-ing it, and a CRC over plain
//    ASCII — the obvious wrong implementation — would have produced 104
//    MISMATCH lines rather than 104 silent passes. The instrument can fail.
//  * `standalone/reg_element_typeids.cpp` is a live consumer of six of the ten
//    and logs the pointer AND the id it publishes into each of the exe's 21
//    `.rdata` slots. Across the A/B the pointers move from the DLL's data into
//    our minted storage and every id is byte-identical. That is an end-to-end
//    check of the one field anybody reads, on a default-on consumer.
//
// WHAT IS STILL A TRAP HERE, STATED RATHER THAN HIDDEN
//
// Each minted BaseTypeInfo's `factoryFn` / `inPlaceFactoryFn` is a named abort
// trap, for all 104 rows — no factory in the mint is ported yet. Those two
// pointers are reachable only through a `Object::Factory` tree, and a minted
// node is in no tree: our `Factory::Startup` resolves `?_firstTypeInfo@` by
// name, that row is NOT promoted, so it still walks the DLL's chain. Every
// consumer of these ten symbols in this tree reads the first dword (and
// `dialog.cpp` / `spritesheet_vtable.cpp` also read `+0x08 name`, which the
// shadow compares and which agrees). So the traps are unreachable today, and
// if that ever changes they abort with the symbol in the first line rather
// than failing silently — which is the mint's own design rule, not an
// exception made for this file.
//
// WHY NOT ALL 104. The other 94 are equally READY on the instrument, and
// promoting them would be a port with no caller: nothing in this tree resolves
// them, so nothing would exercise them and the census would gain 94 rows that
// no run can verify (reference_uncalled_port_is_not_verified.md). These ten
// have live consumers; that is the whole difference.

// ===========================================================================
// THE GUARDED VTABLE PROMOTION  (Wave 25 lane SWEEP)
// ===========================================================================
// A promoted `??_7` row publishes WHATEVER THE LIVE REGISTRY HELD when
// RtDataMintInit() ran. If a wave kill switch (BANISHED_RT_GRAPHICS=0,
// BANISHED_RT_IO=0, ...) suppressed a slot's registration, the mint fills that
// slot with a named abort trap -- and the promotion then stamps a table of
// abort traps into a LIVE OBJECT'S VPTR. The build stays clean and links;
// kickoff §3.6 says exactly that a build check cannot see this.
//
// Wave 25 lane VTGFX added RtMintVtableTrapCount() and made its own new row
// refuse itself, and explicitly handed the extension to this lane: the five
// vtables promoted in Waves 19-20 (and the four UI rows from Wave 10B, and the
// Video one from Wave 18 -- ELEVEN rows in total, not five) carried the
// identical exposure with no guard at all.
//
// Every `??_7` promotion in this file now goes through here. A `?_typeInfo@`
// row does NOT: it is a single pointer, not a table of slots, and
// RtMintVtableTrapCount would answer -1 for it -- which is "not a vtable row",
// a different answer from "zero traps", and routing those through this helper
// would have refused all thirty of them.
//
// WHAT THIS GUARD DOES NOT DO: it does not check that a slot's body is CORRECT,
// only that a body was registered. A wrong port passes it.
static int g_vtGuardEvaluated = 0;
static int g_vtGuardRefused   = 0;

static bool PromoteVtableGuarded(const char* symbol, unsigned long long va, int* n) {
    ++g_vtGuardEvaluated;
    const int traps = RtMintVtableTrapCount(symbol);
    if (traps != 0) {
        ++g_vtGuardRefused;
        // traps < 0 is "not a minted VTABLE row" -- a typo or a table that was
        // never minted -- which must NOT read as a green light either.
        fprintf(stderr,
                "[RtMint] REFUSING to promote %s: RtMintVtableTrapCount()=%d "
                "(expected 0). %s Publishing this table would put named abort "
                "traps in a live object's vptr. The row stays on the DLL.\n",
                symbol, traps,
                traps < 0 ? "The symbol is not a minted VTABLE row at all."
                          : "A slot has no registered port -- a wave kill switch "
                            "has suppressed at least one registration.");
        fflush(stderr);
        return false;
    }
    void* st = RtMintPromotionStorage(symbol, va);
    RegisterRuntimeExport(symbol, st);
    RegisterRuntimeAddr(va, st);
    if (n) ++*n;
    return true;
}

int RtMintRegisterPromotedRows() {
    int n = 0;
    void* st = nullptr;

    st = RtMintPromotionStorage("?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801fa9a8ull);
    RegisterRuntimeExport("?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa9a8ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801f9d68ull);
    RegisterRuntimeExport("?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9d68ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A", 0x1801faa88ull);
    RegisterRuntimeExport("?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801faa88ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A", 0x1801faac0ull);
    RegisterRuntimeExport("?_typeInfo@DialogInstance@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801faac0ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801fac48ull);
    RegisterRuntimeExport("?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fac48ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801fa040ull);
    RegisterRuntimeExport("?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa040ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801fa190ull);
    RegisterRuntimeExport("?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa190ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801fad98ull);
    RegisterRuntimeExport("?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fad98ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A", 0x1801fadd0ull);
    RegisterRuntimeExport("?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fadd0ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A", 0x1801faee8ull);
    RegisterRuntimeExport("?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801faee8ull, st); ++n;

    // ------------------------------------------------------------------------
    // WAVE 10D lane VTFID — `?_typeInfo@DialogController@UI@@` (DLL 0x1801f9e48)
    //
    // Lane TIDENT left this READY-but-held with an expiry condition stated in
    // full: "it expires the moment `[TidStatic]` prints on any route … the
    // observable is the NODE POINTER on that line moving out of the DLL's data
    // into our image". Both halves of that condition are now measured.
    //
    //  1. THE CONSUMER IS LIVE. On wave10/base @190e91c, route
    //     BANISHED_AUTOSTART=1 BANISHED_LIVE_TOOLS=1
    //     BANISHED_TOOL_HARNESS=housepanel + pause-menu quit, `[TidStatic]`
    //     prints for this row. TIDENT recorded zero lines on five routes
    //     including housepanel, so the consumer became reachable between that
    //     tip and this one — NOT through this wave's vtable fidelity fix: the
    //     line prints identically with BANISHED_DLGCTRL_VT=0, so the fix is
    //     not what varies and is not offered as the warrant.
    //
    //  2. SOMETHING VARIES WITH THE ROW, and it is the thing TIDENT named.
    //     Same binary, same seed, same route, one variable:
    //        row off:  node=00007ffcab689e48  id=0xf2c192b8
    //        row on:   node=00007ff672f2d638  id=0xf2c192b8
    //     Runtime-x64.dll based at 0x7ffcab490000 that run, so the OFF node is
    //     DLL+0x1f9e48 — exactly the documented 0x1801f9e48 — and the ON node
    //     is in our image. The id is byte-identical, which is the correct
    //     outcome and is why the pointer, not the value, is the observable.
    //
    //  3. THE NEGATIVE CONTROL IS IN THE SAME LINE PAIR. `[TidStatic]` also
    //     prints `?_typeInfo@GroupElement@UI@@`, which is NOT armed: its node
    //     stays 00007ffcab68abd8 in both halves. So the pointer move follows
    //     the ARMED ROW and not the run, the route, or the environment — the
    //     distinction a single moving pointer could not make on its own.
    //
    // What this does NOT license: the row's own base bodies stay unentered
    // (`[DlgCtrl] ATTENDANCE … TypeName=0 Type=0`), and that is not a gap in
    // the evidence — retail OVERRIDES vtable slots 1 and 2 in all 39
    // DialogController subclasses (tools/dlgctrl_vtable_audit.py; no mask has
    // bit 1 or 2 set), so no subclass dispatch can ever reach them. This row's
    // consumer reaches the static a different way, by direct read in
    // TypeIdOfStatic().
    st = RtMintPromotionStorage("?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A", 0x1801f9e48ull);
    RegisterRuntimeExport("?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9e48ull, st); ++n;

    // =================================================================    // THE FIRST PROMOTED `??_7` VTABLES  —  WAVE 10B lanes SS and ELEM
    // (both landed the same day from separate worktrees, so each lane doc
    // calls itself "the first"; the integrator merged SS then ELEM.)
    //
    // FIRST: lane SS
    // ========================================================================
    //
    // `??_7SpriteSheet@UI@@6B@` (DLL 0x180154590, 9 slots). The paragraph that
    // used to sit at the bottom of this file said it was one trap away; lane SS
    // ported that trap (slot 5, `?Serialize@SpriteSheet@UI@@`, retail
    // 0x18011d730 — runtime/src/ui/spritesheet_vtable.cpp) and this row is what
    // the port was for.
    //
    // THE WARRANT — three measurements, debug build, DLL present,
    // BANISHED_AUTOQUIT=300, all under tools/game_dir_lock.sh (lane SS,
    // 2026-09-09; logs in the lane's snapshot dirs, doc
    // docs/runtime_port/WAVE10B_LANE_SS.md):
    //
    //  1. GATE (a) — `BANISHED_RT_MINT_SHADOW=1` reports, on both of the two
    //     readiness passes in the run:
    //        READY  ??_7SpriteSheet@UI@@6B@   slots= 9 ported= 9 TRAP= 0 purecall= 0
    //     The baseline at wave10/base @7358732 read `ported= 8 TRAP= 1` on the
    //     same instrument, so the screen moved for a reason and is not stuck.
    //
    //  2. GATE (b) — the A/B. `BANISHED_RT_MINT_ROWS=??_7SpriteSheet@UI@@6B@`
    //     against the same boot without it: both exit 0, no crash log, both end
    //     on the same two closing lines, and the normalised log diff is the two
    //     expected `[RtMint] ROW ON` lines plus splash-frame and camera-anim
    //     jitter. `Dialog/SpriteSheet.rsc` loads in both and both find
    //     `Sprite("SaveLoadEmpty")` at index 211.
    //
    //  3. THE EFFECTIVENESS CONTROL — because "the boot still works" is also
    //     what a promotion that did NOTHING would report
    //     (reference_probe_needs_an_effectiveness_control.md). With
    //     BANISHED_RT_DIFF=1 added to both halves, the promoted run and only
    //     the promoted run shows:
    //        * `UI::SpriteSheet::Construct  divergences=1`, and the divergence
    //          is the eight bytes at +0x00 — `ours` is an address in OUR image,
    //          `theirs` the DLL's 0x180154590+ASLR. That IS the promotion: our
    //          Construct now stamps the minted table.
    //        * a `UI::SpriteSheet` site (the deleting destructor,
    //          runtime/src/object/deleting_dtor.cpp) with `calls=2
    //          divergences=0`, which does not appear in the control at all —
    //          i.e. real dispatch went through slot 0 of the minted table and
    //          came back clean.
    //     So the row is live, it changes what runs, and what runs agrees.
    //
    // WHAT THIS WARRANT DOES **NOT** CLAIM. Slot 5 itself was never entered on
    // any of the four boots: the sheet that is actually deserialised at boot is
    // built by the DLL's own ExternalFactory and dispatches slot 5 off the
    // DLL's table. `?Serialize@SpriteSheet@UI@@` is therefore **PORTED, NOT
    // VERIFIED** — its write-path oracle (the emission oracle, wired in
    // spritesheet_vtable.cpp) reported no entry, which is the `calls=0` column
    // and not a pass (reference_uncalled_port_is_not_verified.md). Promoting is
    // still the right call and this is why: TRAP=0 means the slot can no longer
    // abort(), the other eight slots ARE exercised and clean, and leaving the
    // row unpromoted would not verify slot 5 either — it would only keep the
    // eight measured ones unreachable too.
    //
    // The `Construct` divergence above is expected and permanent for any
    // promoted vtable: it is the vtable identity, nothing else. Stated here so
    // a later reader does not find that red line and read it as a regression.
    PromoteVtableGuarded("??_7SpriteSheet@UI@@6B@", 0x180154590ull, &n);
    // ---- the ELEVENTH `?_typeInfo@` row  (Wave 10B lane ELEM) --------------
    //
    // Not promoted with the ten above for the reason that block's closing
    // paragraph gives — "these ten have live consumers; that is the whole
    // difference" — and `?_typeInfo@Element@UI@@` had none. It has one now:
    // runtime/src/ui/element_vtable.cpp's `?Type@Element@UI@@` and
    // `?TypeName@Element@UI@@` are the two shipped vtable slots that read this
    // BaseTypeInfo at +0x00 and +0x08, and they are ports as of this branch.
    //
    // MEASURED THE SAME WAY THE TEN WERE, and end to end rather than by
    // analogy. Control = the run above with only the vtable promoted; armed =
    // the same run plus `BANISHED_RT_MINT_ROWS=?_typeInfo@Element@UI@@…`.
    // `standalone/reg_element_typeids.cpp` is a live consumer that logs both
    // halves of what it publishes into the exe's `.rdata`, and across the A/B:
    //
    //   control  [ElemTid] Element (base)  slot=0x140151660
    //                        typeInfo=00007ffcab1cab30  id=2370443687
    //   armed    [ElemTid] Element (base)  slot=0x140151660
    //                        typeInfo=00007ff7ac54d9d8  id=2370443687
    //
    // The pointer moves out of the DLL's data into our minted storage and the
    // id — the ONE field anybody reads — is byte-identical (0x8D4A19A7, the
    // TypeNameCRC our MintTypeInfos() computes). The shadow's own field
    // comparison agrees independently: `READY … id=0x8d4a19a7 idcmp=yes`.
    // And the strongest form of it: with the row armed, `UI::Element::Type
    // calls=1 worked=1 divergences=0` — OUR body read OUR storage and still
    // matched the shipped export reading the DLL's.
    // 0-byte crash log, `[RtResolve] 0 misses`, clean `[Shutdown] done`, and a
    // divergence set identical to the control's twelve named entries.
    //
    // The `fac=2traps` caveat the ten carry applies here unchanged: this node's
    // factoryFn / inPlaceFactoryFn are named abort traps, unreachable because a
    // minted node is in no `Object::Factory` tree.
    st = RtMintPromotionStorage("?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A", 0x1801fab30ull);
    RegisterRuntimeExport("?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fab30ull, st); ++n;

    // ========================================================================
    // THE FIRST PROMOTED VTABLE — ??_7Element@UI@@6B@   (Wave 10B lane ELEM)
    // ========================================================================
    //
    // WHAT CHANGED. At the wave-10B baseline this row read `slots=12 ported=3
    // TRAP=8 purecall=1` and is listed two paragraphs below as NOT PROMOTED for
    // exactly that reason. runtime/src/ui/element_vtable.cpp ported the eight,
    // and the readiness table now reads, on a live boot:
    //
    //     READY  ??_7Element@UI@@6B@   slots=12 ported=11 TRAP= 0 purecall= 1
    //
    // THE MEASUREMENT (2026-09-09, debug build, DLL present, under
    // tools/game_dir_lock.sh, four runs; logs in docs/runtime_port/wave10b/).
    // The A/B is `BANISHED_TOOL_HARNESS=utility
    // BANISHED_UTILITY_TEMPLATE=Template\UtilityMap.rsc` on top of autostart —
    // the minimap route, because that is the one that actually CONSTRUCTS an
    // Element through our `??0Element` port. Control = the same run without
    // BANISHED_RT_MINT_ROWS; both sides had BANISHED_RT_DIFF=1 so only the row
    // differs.
    //
    //  * THE EFFECTIVENESS CONTROL FIRST, because the answer is not "nothing
    //    changed". A menu-only A/B of this row IS a no-op — logs identical bar
    //    two `[RtMint] ROW ON` lines — and promoting on that would have been a
    //    port with no caller (reference_uncalled_port_is_not_verified.md, in
    //    vtable form). On the minimap route the row is demonstrably LIVE:
    //      - `UI::Element::ctor calls=1` on both sides: our ported ctor builds
    //        the FrustumOutlineElement that code/Game/Component/
    //        frustumoutlineelement.cpp creates, and that object then KEEPS the
    //        base Element vtable for its whole life. It is the reader.
    //      - armed only: `UI::Element::Type calls=1 worked=1 divergences=0` —
    //        our minted slot 2 was DISPATCHED INTO and agreed with the DLL.
    //      - armed only: our minted slot 0 ran too, at teardown —
    //        `UI::Element::RemoveChildren calls=1`, `Collection::Array<8>::
    //        SetSize calls=1`, `~Array calls=1`. In the control that object was
    //        destroyed by the DLL's deleting destructor instead.
    //  * SAME PLACE REACHED: both runs `[Shutdown] done`, 0-byte crash log,
    //    `[RtResolve] 0 misses`, identical 21 lines of harness/MapProbe output.
    //  * THE WHOLE DIVERGENCE DELTA IS ONE LINE, and it is the promotion
    //    itself: `UI::Element::ctor call #1 ours=+0x0 <exe .data> theirs=+0x0
    //    <Runtime .rdata>` — the vtable pointer. The other twelve divergence
    //    lines are byte-identical between the two runs and are the tree's
    //    existing named negative controls. element.cpp's ctor oracle now
    //    excludes +0x00 with that argument written out, and checks the field
    //    with its own (labelled) self-check instead.
    //
    // WHAT IS STILL NOT MEASURED, PLAINLY. Only slots 0 and 2 were dispatched
    // in the verifying run. Slots 3, 7, 8, 9, 10, 11 are ported and READY but
    // were not entered by any route this lane found, so their oracles report
    // calls=0 — and slot 4 is `_purecall` in Element itself, which is why
    // frustumoutlineelement.cpp builds a patched COPY of this table rather than
    // using it directly. Promoting is still right: a READY row's whole claim is
    // "no abort trap in the table", and that claim is what the shadow measures
    // and what the boot confirms. It is NOT a claim that all eleven bodies are
    // differentially verified, and docs/runtime_port/WAVE10B_LANE_ELEM.md says
    // which are and which are not, per slot.
    PromoteVtableGuarded("??_7Element@UI@@6B@", 0x180156160ull, &n);
    // ========================================================================
    // THE FIRST MINTED **VTABLE** ROW  —  Wave 10B lane DLGV
    // ========================================================================
    //
    // `??_7DialogInstance@UI@@6B@` (DLL 0x180156d70, 10 slots). Until this wave
    // it read `8 ported, 2 TRAP` in the paragraph below. Lane DLGV ported both
    // traps — `?Type@` (runtime/src/ui/dialog.cpp) and `?OnOpened@`
    // (runtime/src/ui/dialog_settings.cpp) — and the row is now
    //
    //     READY  ??_7DialogInstance@UI@@6B@   slots=10 ported=10 TRAP= 0
    //
    // THE MEASUREMENT (2026-09-09, debug build, DLL present, under
    // tools/game_dir_lock.sh; logs in lane_dlgv_logs/ and the lane doc)
    //
    //  * Gate (a): the shadow readiness screen reports READY / TRAP=0 on a live
    //    boot. That screen asks the port registry through the same OurPort()
    //    the mint fills with, so it sees registrations whose name arrives in a
    //    variable.
    //  * Gate (b): TWO A/B pairs, each `BANISHED_RT_MINT_ROWS=<this row>`
    //    against the identical boot without it — autostart to gameplay, then a
    //    pause-menu Quit. Both halves of both pairs reached gameplay, quit
    //    cleanly and left a **0-byte crash log**. A whole-log frequency diff of
    //    the first pair finds exactly THREE differing lines, all three of them
    //    the `[RtMint] ROW ON (env)` lines the candidate half prints; everything
    //    else that moves is splash-loader tick and fade frame counts.
    //  * **The effectiveness control, which is the part that makes (b) mean
    //    anything.** Registering this row is not passive — our ported ctors
    //    stamp the table, so the DLL's own dispatch runs our slots. But our
    //    `?OnOpened@` writes nothing observable, so an A/B alone cannot tell a
    //    SAFE promotion from one that was never exercised. dialog_settings.cpp
    //    therefore carries an ATTENDANCE counter, and it moves:
    //
    //        control    [DlgSettings] ATTENDANCE: OnOpened was not entered
    //        candidate  [DlgSettings] ATTENDANCE ... entries=2 (pair 1)
    //        candidate  [DlgSettings] ATTENDANCE ... entries=3 (pair 2, with an
    //                                 in-game residence panel open)
    //
    //    0 -> 2 and 0 -> 3 on two independent routes. Our body really is what
    //    the DLL dispatches once the row is registered.
    //
    // PORTED, LIVE, AND ONLY PARTLY COVERED — stated rather than footnoted.
    // Every one of those five entries reported `noBuffer`: `_userData` was null,
    // so the body took its second-line early exit and `badMagic=0 records=0`.
    // The controller loop ran (over an empty list) and the settings-reading half
    // of `?OnOpened@` — MemoryStream, the magic, the Serializer, the per-record
    // loop — has NOT been exercised by any route this lane found. That is the
    // `calls=0`-inverts-the-conclusion shape caught early rather than late
    // (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md), and
    // the sub-counters exist precisely so the gap is visible in every run's log.
    PromoteVtableGuarded("??_7DialogInstance@UI@@6B@", 0x180156d70ull, &n);

    // ========================================================================
    // ??_7ToolTipDialog@UI@@6B@  —  WAVE 10D, warrant 7a
    // ========================================================================
    //
    // The row did not EXIST when Wave 10D opened. Lane TIP ported the three
    // slots that are ToolTipDialog's own (0 vector-deleting-dtor, 1 TypeName,
    // 2 Type) and PROVED BY ADDRESS that slots 3..9 are byte-identical to
    // ??_7DialogInstance@UI@@6B@'s targets — already ours, since that row is
    // 10/10 and promoted. Extent pinned independently by the adjacent export:
    // 0x180157880 + 10*8 == ??_7DialogLoaderLoopT@Engine@@6B@.
    //
    // The integrator regenerated rt_data_mint_tables.inc ONCE at wave close;
    // the vtable SET diff was exactly one entrant, this row.
    //
    // GATE (a) — shadow, live boot, DLL present:
    //     READY  ??_7ToolTipDialog@UI@@6B@  slots=10 ported=10 TRAP= 0
    //
    // GATE (b) — DISPATCH, not readiness. This is the distinction that has kept
    // ??_7Dialog and ??_7DialogController green-and-unpromoted for two waves,
    // so it was named IN ADVANCE with an abort condition rather than judged
    // afterwards: a ToolTipDialog cannot be constructed without this vtable
    // being stamped onto it, so `operatorNew` non-zero proves the table is
    // really dispatched. TIP wrote: "if operatorNew is zero while MOUSE's
    // BeginToolTip reads entries=2, that is a genuine finding — the DLL's
    // BeginToolTip ran and ours did not — and this warrant must NOT be applied."
    //
    // Measured on the merged tip via lane MOUSE's injector
    // (BANISHED_MOUSE_INJECT=hover,hover,off,hover,off MOUSE_TARGET=#time
    // RT_UIHOVER_DETOUR=1), one cell, exit 0, 0-byte crash log:
    //
    //   [RtPort] UI::ToolTipDialog ATTENDANCE: total=6 operatorNew=2 (bytes=192)
    //            GetToolTipString=2 (nonNull=0) SetText=2 (rootHit=0 found=2
    //            wrote=2) TypeName=0 Type=0
    //   [RtPort] UI::UserInterface::BeginToolTip  entries=2 rejected=0
    //            closedPrev=0 nonEmptyString=0 allocFailed=0
    //
    // Read as RATIOS against operatorNew, never as absolute counts — every
    // absolute scales with the injector script, so an absolute prediction turns
    // a script change into a false divergence:
    //     operatorNew        == BeginToolTip entries   (2 == 2)  -> DISPATCHED
    //     bytes              == 0xc0 (192)  CONSTANT, not a ratio -- see below
    //     GetToolTipString/operatorNew == 1  -> the EMPTY arm, which
    //         cross-checks BeginToolTip's own nonEmptyString=0 from the far end
    //     SetText/operatorNew == 1, with effectiveness controls found=2 wrote=2
    //     TypeName/Type == 0 — expected; they exist for TRAP=0, not this route
    //
    // NOT CLAIMED: the non-empty arm. `#time` has an empty +0x58
    // _toolTipString, so retail falls back to the string-table arm
    // (+0x48/+0x50 through 0x180035020) and GetToolTipString is called ONCE.
    // The tooltip is fully populated — what is empty is the +0x58 override, not
    // the tooltip. Covering the other arm needs an element some controller has
    // called ?SetToolTipString@Element@UI@@ on; see WAVE10D_LANE_MOUSE.md §9.
    PromoteVtableGuarded("??_7ToolTipDialog@UI@@6B@", 0x180157880ull, &n);

    // ---- ?_typeInfo@ToolTipDialog@  —  WAVE 10D, warrant 7b ----------------
    //
    // Does not depend on the run. Its evidence is measured shadow output
    // showing the row character-for-character the same shape as rows that
    // already ship promoted: `idcmp=yes strs=0 ptr=2 fac=2traps`.
    // Its live consumers are the two slots lane TIP ported — ?TypeName@ and
    // ?Type@ read this BaseTypeInfo at +0x00 and +0x08 — which is this file's
    // own "these have live consumers; that is the whole difference" rule.
    // The pointer-identity question that hung over every typeinfo promotion was
    // retired in Wave 10C by lane TIDENT: BaseTypeInfo::name is CONTENT-ONLY in
    // both shipped binaries (525 exe references to the imported ?_typeInfo@
    // slots all read +0x00; not one reads +0x08, and the DLL's 103 ?TypeName@
    // accessors are never called).
    st = RtMintPromotionStorage("?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A", 0x1801fa4a0ull);
    RegisterRuntimeExport("?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa4a0ull, st); ++n;

    // ========================================================================
    // NOT PROMOTED — THE STANDING CASE FOR PROMOTING AT CUTOVER
    //   ?_singleton@SaveLoad@Save@@0PEAV12@EA   0x1801fc6f0   ordinal 4817
    //                                             (Wave 14 lane SINGLETON)
    // ========================================================================
    //
    // This row is MINTED (rt_data_mint_tables.inc, 8 bytes) and deliberately
    // NOT registered here. The enumeration below is the finished work: it is
    // what a cutover lane needs in order to promote the row, and it is written
    // out in full so that lane inherits it instead of re-deriving it.
    //
    // WHY NOT NOW, decided against this lane's own first recommendation. With
    // the DLL present, promoting does not CLOSE a split brain, it CREATES one:
    // measured, our ctor already publishes into retail's own .data
    // (`arrangement=DLL-SLOT`, ourDoor == dllSlot), and registering the row
    // moves our writers and readers to our storage while `?Get@` keeps reading
    // the DLL slot through an in-DLL RIP-relative load that can never be
    // redirected. That is safe — see the deadness proof below — but "safe"
    // is not "worth it":
    //
    //   * It unblocks no lane and closes no route. The only thing it buys with
    //     the DLL present is a live detector for an ordering regression, via a
    //     log line nobody is watching.
    //   * Promotion is the arrangement CUTOVER needs, and cutover is the one
    //     configuration in which it has NOT been measured. Promoting now takes
    //     the risk where it buys least and defers the verification to where it
    //     matters.
    //   * Promoted-by-default makes every other lane in the wave carry a
    //     configuration resting on one lane's STATIC proof. If that proof is
    //     wrong, somebody else eats the crash and bisects onto this row, mid
    //     wave. A wave is the worst moment to spend that.
    //
    // TO PROMOTE IT (at cutover, or whenever the DLL-absent boot runs), add:
    //     st = RtMintPromotionStorage("?_singleton@SaveLoad@Save@@0PEAV12@EA",
    //                                 0x1801fc6f0ull);
    //     RegisterRuntimeExport("?_singleton@SaveLoad@Save@@0PEAV12@EA", st);
    //     RegisterRuntimeAddr(0x1801fc6f0ull, st); ++n;
    // That exact triple was run: `arrangement=MINTED`, the publish check passed
    // against our storage, 19 pass / 0 fail, and a real autostarted game drove
    // frame 600 -> SAVE OPERATION COMPLETE at 661 with an 8.6 MB Auto.sav. The
    // exe-side door followed it — `[SaveTest] g_pSaveLoadSingleton` read our
    // storage, and that value comes from import slot 0x140151610, not from the
    // probe. Reproduce either half without a code change:
    //     BANISHED_RT_ORPHAN_PROBE=1 [BANISHED_RT_MINT_ROWS=<this symbol>]
    //
    // WHAT MAKES IT SAFE WHEN IT IS PROMOTED — and this is the part not to
    // re-derive.
    //
    // Every row above this one is a vtable or a BaseTypeInfo — write-once data
    // whose worst case is a wrong constant. This one is a LIVE MUTABLE POINTER,
    // the same shape as `?_singleton@Application@1@`, which is tagged
    // UNPROMOTABLE | HARMFUL twenty lines below. So the difference between the
    // two has to be shown, not asserted, and it is the READER SET.
    //
    // Application::_singleton has 26 in-DLL RIP-relative references, 24 of them
    // READS in bodies nobody has ported. Promoting it would strand those 24 on
    // a slot our ctor no longer writes. SaveLoad::_singleton has FIVE, and they
    // were enumerated twice by routes that could have disagreed — Ghidra
    // get_xrefs_to, and a decoder-independent scan of every disp32 field in
    // .text whose RIP-relative target is the slot. (The scan first reported
    // FOUR: it assumed no trailing immediate, and `??1SaveLoad`'s reference is
    // `MOV qword ptr [rip+d],0`, where RIP is measured past the imm32.
    // Corrected, both routes give the same five. A second route only helps if
    // it can actually disagree, and this one did.)
    //
    //   0x1800c3d9b  W  ??0SaveLoad@Save@@QEAA@XZ            PORTED + registered
    //   0x1800c3ca6  W  ??1SaveLoad@Save@@QEAA@XZ            PORTED + registered
    //   0x1800dc3e9  R  ?OnVideoRestart@MainLoop@Engine@@    PORTED + registered
    //   0x1800dcc1d  R  ?OnVideoError@MainLoop@Engine@@      PORTED + registered
    //   0x180030d40  R  ?Get@SaveLoad@Save@@SAPEAV12@XZ      UNPORTED, and DEAD
    //
    // All four ported bodies reach the slot through `RT_DATA_SLOT` /
    // `ResolveExport`, i.e. through the door, so promoting moves them with it.
    // The fifth cannot be moved — an in-DLL RIP-relative load is not a door —
    // and that is the whole question, so `Get`'s deadness is the load-bearing
    // fact and it is established from the binaries, not from reading:
    //
    //   * Runtime-x64.dll: no call site. Ghidra's xrefs to 0x180030d40 are the
    //     export-directory entry and the entry-point sweep, nothing else.
    //   * Application-x64.exe: not among its 899 imports (833 from this DLL).
    //     The exe reaches this singleton at 20 .text sites, every one of them
    //     through import slot 0x140151610 — a DOOR, registered in
    //     reg_unreg_lane1.cpp — never through `Get`.
    //   * VideoDX11-x64.dll (163 imports) and VideoDX9-x64.dll (146): absent.
    //   * No pointer-valued reference to 0x1801fc6f0 exists in any of the four
    //     binaries, so there is no fifth way to reach it.
    //
    // ORDER. Everything that resolves this name does so after this file runs:
    // RtDataMintInit() is inside RuntimePortInit() (main.cpp:1731), and
    // AddressRegistryInit() (which computes RegisterDispatchSlot(0x140151610,
    // ResolveExport(...)) ) and EngineFuncsInit() both run after it. A
    // promotion that landed after either would have left the exe's 20 readers
    // pointed at a slot nothing writes; that is worth stating because it is the
    // failure this row would have had, and it is ordering, not luck.
    //
    // MEASURED, not argued: `ProbeSaveLoadSingleton` in rt_orphan_probe.cpp
    // (BANISHED_RT_ORPHAN_PROBE=1) sweeps six sentinels through the DLL slot to
    // prove retail's `Get` reads exactly this address, then runs our REGISTERED
    // ctor on a private object and checks where the pointer landed. The A/B is
    // the `arrangement=` line: DLL-SLOT without this row, MINTED with it, and
    // the publish check passes in BOTH halves — which is the point. The quantity
    // varies with the thing the gate gates. WITH the row registered, as it now
    // is, the probe reports MINTED — the true statement about this build.
    //
    // PROMOTED 2026-09-09, AFTER the wave, by explicit decision. The deferral
    // above was correct FOR THE WAVE and its reasoning is left standing rather
    // than deleted: promoting mid-wave would have made six sibling lanes carry
    // a configuration resting on one lane's static proof. That objection is
    // spent — every lane has reported, merged and stopped running, so a defect
    // here now lands on the integrator and bisects onto one commit.
    //
    // What is STILL not measured is the DLL-ABSENT boot. Promotion is the
    // arrangement cutover needs and this makes the tree carry it early, which
    // is a real (small) risk taken deliberately for a real gain: it is a LIVE
    // detector for the ordering regression described above, and the Save
    // namespace reaches its ceiling of 29/30. If the DLL-absent boot ever finds
    // this wrong, the fix is to delete the three lines below, not to re-derive
    // the enumeration.
    st = RtMintPromotionStorage("?_singleton@SaveLoad@Save@@0PEAV12@EA",
                                0x1801fc6f0ull);
    RegisterRuntimeExport("?_singleton@SaveLoad@Save@@0PEAV12@EA", st);
    RegisterRuntimeAddr(0x1801fc6f0ull, st); ++n;

    // ========================================================================
    // WAVE 11 LANE OBJDATA — the two Object `?_typeInfo@` rows
    // ========================================================================
    //
    // Both are READY on the exit-phase shadow, in the run where the DLL's own
    // Factory::Startup HAS executed, so the typeId is compared and not skipped:
    //     BaseTypeInfo : 624 field comparisons, 0 MISMATCH,
    //                    0 typeId not-yet-comparable
    //     typeinfo rows: 104 READY, 0 BLOCKED
    // (At phase=init the same two read `idcmp=NO` — that is the DLL still
    // holding the ctor sentinel, not a mismatch, and reading the init snapshot
    // as the verdict would have blocked both rows for nothing.)
    //
    // ORDERING, which is the failure mode this pair could have had. Both rows
    // have an EAGER consumer that resolves the node ONCE and republishes the
    // pointer: standalone/reg_unreg_lane1.cpp:128 does
    // `RegisterDispatchSlot(0x140152870, ResolveExport("?_typeInfo@External@…"))`,
    // filling an exe import slot. A promotion landing AFTER that call would
    // leave the exe's readers on the DLL's node and ours on the minted one.
    // It does not: `RuntimePortInit()` (standalone/main.cpp:1734) contains both
    // RtDataMintInit and this function, and `AddressRegistryInit()` — which
    // calls RegisterUnregLane1Batch — is at :1755, twenty-one lines later. The
    // exe slot therefore receives OUR node. Checked, not assumed, and it is the
    // same ordering argument the SaveLoad row above turns on.
    //
    // `?_typeInfo@ExternalList@Object@@` — GATE (b) IS SATISFIED BY A ROUTE
    // THAT ACTUALLY RUNS, which is what separates this row from the three
    // `?_typeInfo@` rows this file still holds back. Its consumer is the retail
    // type gate in code/Game/Game/gameupdateloop_load.cpp:281 and :355, which
    // reads the node's typeId and feeds it to `IsDerivedFrom`. A/B on the
    // autostart route, one variable
    // (`BANISHED_RT_MINT_ROWS=?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A`):
    //     armed    [GUL::DiseaseList] isExternalList=1 diseases=12
    //              [GUL::Help]        isExternalList=1 topics=66
    //     control  [GUL::DiseaseList] isExternalList=1 diseases=12
    //              [GUL::Help]        isExternalList=1 topics=66
    // Same verdict and same counts on both sides; only the per-run heap
    // addresses differ. The gate is exercised AND the promotion is neutral,
    // which is the pair of facts `??_7DialogController@UI@@6B@` could never
    // produce because nothing entered its consumer at all.
    //
    // `?_typeInfo@External@Object@@` — its exercised consumer is the eager
    // RegisterDispatchSlot above, which runs unconditionally on every boot, plus
    // `TypeIdExternal()` in runtime/src/engine/mainloop_video.cpp:333. Both read
    // the typeId dword, and `idcmp=yes` says that dword is the DLL's own value.
    // STATED PLAINLY BECAUSE IT IS THE WEAKER HALF OF THIS PAIR: the three
    // accessors in runtime/src/object/object_typeinfo_accessors.cpp that also
    // read this node were entered ZERO times in both halves of the A/B, so they
    // are NOT part of this row's warrant.
    //
    // Boot measured with both rows armed: `[Shutdown] done`, 0-byte crash log,
    // and no `[RtDiff]` site anywhere reporting a non-zero divergence count.
    st = RtMintPromotionStorage("?_typeInfo@External@Object@@0VBaseTypeInfo@2@A",
                                0x1801f9b38ull);
    RegisterRuntimeExport("?_typeInfo@External@Object@@0VBaseTypeInfo@2@A", st);
    RegisterRuntimeAddr(0x1801f9b38ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A",
                                0x1801f9be0ull);
    RegisterRuntimeExport("?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A", st);
    RegisterRuntimeAddr(0x1801f9be0ull, st); ++n;

    // ========================================================================
    // WAVE 18 lane VDATA — the ImageBuffer vtable and the TEN OnVideoError
    // BaseTypeInfo rows (six Video + four Graphics)
    // ========================================================================
    //
    // Doc: docs/runtime_port/WAVE18_LANE_VDATA.md. Predictions pre-registered
    // at 88f90b3, BEFORE any code was written and before any run.
    //
    // THE BLOCKER THESE ELEVEN WERE HELD BEHIND WAS STALE, WHICH IS THE WHOLE
    // STORY. Wave 17 lane CEIL §3.4 and the Wave 18 kickoff both record the ten
    // `?_typeInfo@` rows as "blocked on attendance": their only consumer in the
    // tree is the ten-element `ids[]` in `RtEngine_MainLoop_OnVideoError`
    // (runtime/src/engine/mainloop_video.cpp:487-499), that is the device-lost
    // recovery path, and `post=0` on all four of CEIL's routes. CEIL §5.4
    // therefore prescribed SYNTHESISING an `OnVideoError` call.
    //
    // No synthesis was needed. `BANISHED_FORCE_DEVICE_LOST=<frame>`
    // (dx11/src/contextdata.cpp:594-628, Wave 9 lane ENGIO) injects one
    // synthetic DXGI_ERROR_DEVICE_RESET into a Present result and lets the REAL
    // teardown and rebuild run. It was already in the tree, already documented
    // in WAVE9_LANE_VIDL.md §4.2, and it still works. That is the third stale
    // stated-blocker of this wave
    // (reference_holdback_blockers_are_stale_check_them_first.md).
    //
    // ---- THE WARRANT --------------------------------------------------------
    // Debug build, DLL present, `tools/game_dir_lock.sh run vdata` (never
    // piped, never backgrounded as a bare acquire). ONE control/armed pair
    // against ONE binary, identical but for `BANISHED_RT_MINT_ROWS`. Env:
    // BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_W17_ATTEND=1
    // BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_SAVE_TRACE=1
    // BANISHED_FORCE_DEVICE_LOST=400 BANISHED_PAUSEMENU=900
    // BANISHED_PAUSEMENU_QUIT=120. Both halves rc=0, both crash logs 0 bytes,
    // both `[RtResolve] 0 misses`. Logs: docs/runtime_port/wave18_lane_vdata/.
    //
    //  1. THE ROUTE FIRED. `[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET` ->
    //     `hr=0x887a0007 onLost=... -> calling` -> `recreate OK` ->
    //     `device-lost round trip COMPLETE`, and it reached the consumer:
    //     `Engine::MainLoop::OnVideoError calls=1 divergences=0` in BOTH halves.
    //
    //  2. ATTENDANCE. `BANISHED_W17_ATTEND=1` (CEIL's counter, unmodified —
    //     a fixed 63-entry watch list spelled as literals in
    //     standalone/wave17_door_attend.cpp, 62 subject rows plus the wiring
    //     control) reads all ten rows ATTENDED, `post=1`. Its two controls
    //     both hold: the wiring control `?_singleton@MainLoop@Engine@@` reads
    //     `init=1 post=3` GREEN (the table is SUPPRESSED, not printed, if it
    //     reads 0), and the phase marker sits AFTER `AddressRegistryInit()`,
    //     so a resolve during registration is not counted as attendance —
    //     exactly one of the ten (`GraphicsMesh`, eagerly resolved by
    //     reg_unreg_lane3.cpp) carries `init=1`, and the other nine `init=0`.
    //     `post=1` means the consumer RAN. It does NOT mean "ran once": every
    //     `TypeIdX()` caches in its own function-local static.
    //     This single route attends 49 of the 62 subject rows, where CEIL's
    //     union of four routes attended 36.
    //
    //  3. READINESS. At exit, all ten read
    //     `READY … idcmp=yes strs=0 ptr=2 fac=2traps`, inside
    //     `BaseTypeInfo: 624 field comparisons, 0 MISMATCH, 0 typeId
    //     not-yet-comparable`. `idcmp=yes` is the load-bearing one: our minted
    //     typeId is compared against the live DLL's own value and agrees.
    //     `ptr=2` is the pointer-identity column, scored SEPARATELY from
    //     `strs=` and permanently harmless here — `BaseTypeInfo::name` is
    //     content-only in both shipped binaries. `fac=2traps` does not bite:
    //     CEIL §2.6 re-measured the factory-trap hazard on exactly this
    //     subject and found 0 of 106,251 dispatches went through a minted
    //     BaseTypeInfo, because every dispatch site walks
    //     `?_singleton@Factory@Object@@`'s AVL tree, built from the DLL's
    //     `_firstTypeInfo` chain, while minted nodes hang off
    //     `g_mintTypeInfoHead` — which no `Factory::Startup` walks. Structural.
    //
    //  4. THE VALUE CHECK, and it is not a tautology.
    //     `Collection::Array::SetSize @0x18004f620` is the `_videoResources`
    //     sweep: `OnVideoError` walks `ExternalFactory::FindNext` for each of
    //     the ten type ids and appends every live resource it finds. It reads
    //     **calls=995 in BOTH halves.** In the armed half those ten ids came
    //     from OUR minted nodes and in the control from the DLL's statics. Had
    //     ANY of the ten minted ids been wrong, `FindNext` would have matched a
    //     different set and that count would have moved. (995 is also the
    //     number Wave 9 lane VIDL measured independently.)
    //
    //  5. THE VTABLE'S EFFECTIVENESS CONTROL — GREEN, and only the SET-DIFF
    //     could see it. "The boot still works" is also what a promotion that
    //     did NOTHING reports. The raw site counts are 601 control / 600 armed
    //     and say nothing; the set-diff of the site NAMES says everything:
    //         only in armed:   `Video::ImageBuffer`  calls=6 divergences=0
    //     That site is the scalar deleting destructor
    //     (runtime/src/object/deleting_dtor.cpp). It does not exist in the
    //     control run at all, so real dispatch went through slot 0 of the
    //     MINTED table and came back clean. Corroborated by a second counter
    //     nobody had to look for: `ImageBuffer::PlatformDestruct` moves 4 -> 10,
    //     +6, matching those six destructions exactly. This is the
    //     `??_7SpriteSheet@UI@@6B@` shape reproduced.
    //
    //     The two sites present only in the CONTROL (`Video::Context::
    //     SetInverseScale` calls=2383 and `Math::Transform::Identity()`
    //     calls=1) were CHECKED rather than dismissed as jitter:
    //     `SetInverseScale`'s only caller in the tree is the WEATHER particle
    //     draw (code/Game/Game/weather_particle_draw.cpp:274), reached only
    //     when weather is active with visible tiles, and the neighbouring
    //     draw-volume counter `Video::Context::SetColor` moves 195,598 ->
    //     170,022 in the same direction. Weather is simulation-state
    //     dependent; the promotion cannot reach it.
    //
    // ---- WHAT THIS WARRANT DOES **NOT** CLAIM -------------------------------
    // SEVEN of the ten ImageBuffer slots are **PORTED, NOT VERIFIED.** Slots
    // 1,2,3,5,6,7,8 (`TypeName`, `Type`, `GetFileVersion`, `Serialize`,
    // `Reconstruct`, `Destruct`, `Construct`) report NO `[RtDiff]` site in
    // EITHER half, because no site is registered until the body is entered —
    // they were never dispatched on this route. `Serialize`'s save-trace oracle
    // reported no entry at all (the only emission-oracle lines in either run
    // are `Achievement::Goal::Serialize`), which is the `calls=0` column and
    // not a pass (reference_uncalled_port_is_not_verified.md).
    //
    // Promoting is still right, and for the reason lane SS gave for SpriteSheet
    // slot 5: TRAP=0 means those seven slots can no longer `abort()`, slot 0 IS
    // exercised and clean, and leaving the row unpromoted would not verify the
    // seven either — it would only keep slot 0 unreachable too.
    PromoteVtableGuarded("??_7ImageBuffer@Video@@6B@", 0x180154fc0ull, &n);

    st = RtMintPromotionStorage("?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa548ull);
    RegisterRuntimeExport("?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa548ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@PixelProgram@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa580ull);
    RegisterRuntimeExport("?_typeInfo@PixelProgram@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa580ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa5f0ull);
    RegisterRuntimeExport("?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa5f0ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa660ull);
    RegisterRuntimeExport("?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa660ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@VertexProgram@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa698ull);
    RegisterRuntimeExport("?_typeInfo@VertexProgram@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa698ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@VideoProgram@Video@@0VBaseTypeInfo@Object@@A",
                                0x1801fa5b8ull);
    RegisterRuntimeExport("?_typeInfo@VideoProgram@Video@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa5b8ull, st); ++n;

    // The four Graphics rows ride the SAME consumer — they are elements 7..10
    // of the same `ids[]` — so one run decided all ten. They are promoted here
    // rather than left for the Graphics wave because the evidence is the same
    // evidence, and splitting it would mean re-running the device-lost drive to
    // re-derive numbers this run already produced.
    st = RtMintPromotionStorage("?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801f99b0ull);
    RegisterRuntimeExport("?_typeInfo@EdgeMesh@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f99b0ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801fa708ull);
    RegisterRuntimeExport("?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa708ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@ReflectionImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801f9a90ull);
    RegisterRuntimeExport("?_typeInfo@ReflectionImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9a90ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@FrameCopyImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801f99e8ull);
    RegisterRuntimeExport("?_typeInfo@FrameCopyImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f99e8ull, st); ++n;

    // ========================================================================
    // WAVE 21 LANE GFXDATA — the two Graphics `?_typeInfo@` rows WAVE17_LANE_CEIL
    // §3.3 named PROMOTABLE and explicitly left for a porting lane ("Both are the
    // shape the UI namespace promoted eleven of in Wave 10... it does not move
    // the ceiling, which is why this lane did not do it").
    //
    // Both cleared the same bar as the ten OnVideoError rows above:
    //   * READY — id, name, parentName, fileExtension all agree with the live
    //     DLL (0 MISMATCH across the whole 104-row `BaseTypeInfo` shadow this
    //     wave, re-measured live 2026-09-11).
    //   * fac=2traps is a non-issue for EVERY `?_typeInfo@` row structurally
    //     (WAVE17_LANE_CEIL §2.6: `[FactoryDispatch]` on 106,251 real dispatches
    //     found 0 that walk a minted node — the AVL tree Factory::CreateObject
    //     walks is built from the DLL's own `_firstTypeInfo` chain, which our
    //     minted nodes never join).
    //   * No consumer walks past the object: GeneratedImageBuffer's is a single
    //     `*typeIdPtr` read (runtime/src/graphics/wave9_lane_gfx.cpp); Material's
    //     is `IsDerivedFrom(ext, *g_TypeId_Material)` (code/Game/Game/
    //     gameupdateloop_load.cpp:250 and mapui.cpp/noiseui.cpp) — both dereference
    //     the scalar typeId once and stop, so GFXVT's "the harm can live in the
    //     READER" finding (a fixed-count loop reading past a shorter table) does
    //     not apply to either row.
    //   * ATTENDED, live, both rows, this wave: `BANISHED_AUTOSTART=1
    //     BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=60 BANISHED_W17_ATTEND=1`
    //     (a real New Game boot reaching gameplay, NOT the menu-only
    //     `BANISHED_AUTOQUIT` route the first attempt mistakenly used — see
    //     reference_autoquit_overrides_autostart_newgame.md) reported:
    //       ATTENDED init=0 post=1  ?_typeInfo@GeneratedImageBuffer@Graphics@@...
    //       ATTENDED init=1 post=1  ?_typeInfo@Material@Graphics@@...
    //     matching WAVE17_LANE_CEIL's own measurement for both rows exactly.
    //
    // `?_typeInfo@MaterialInstance@Graphics@@` and `?_typeInfo@Particle@Graphics@@`
    // are READY on the identical evidence (0 mismatch, same factory argument, same
    // single-dereference consumer shape — code/Game/Game/serialize_arrays.cpp and
    // engine_funcs.cpp's `g_ppTypeInfo_MaterialInstance`) but NOT promoted here:
    // the same live run reported `never init=1 post=0` for both, matching
    // WAVE17_LANE_CEIL's "blocked on attendance" verdict exactly, one wave later.
    // Per this file's own rule (item 2, top of file) and the precedent set by the
    // ten OnVideoError rows above (promoted only after a later wave SYNTHESISED
    // their route and measured attendance), readiness alone is not being taken as
    // sufficient here either. See docs/runtime_port/WAVE21_LANE_GFXDATA.md §3.
    st = RtMintPromotionStorage("?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801f9a20ull);
    RegisterRuntimeExport("?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9a20ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A",
                                0x1801fa740ull);
    RegisterRuntimeExport("?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801fa740ull, st); ++n;

    // ========================================================================
    // WAVE 19 LANE IOBASE — four IO vtables, unblocked by ONE tranche of bodies
    // ========================================================================
    //
    // WHAT CHANGED, as a set-diff against phase 0c's re-measurement at
    // `ebbec6e` (WAVE19_PHASE0C_TRAPS.md §4.1), NOT against CEIL's inherited
    // table:
    //
    //   ??_7BinaryReader@IO@@6B@    26/25 TRAP=1  ->  26/26 TRAP=0  READY
    //   ??_7Visitor@IO@@6B@         26/25 TRAP=1  ->  26/26 TRAP=0  READY
    //   ??_7MemoryStream@IO@@6B@     8/ 6 TRAP=2  ->   8/ 8 TRAP=0  READY
    //   ??_7VisitorStream@IO@@6B@    UNMINTED     ->   7/ 7 TRAP=0  READY
    //
    // and, not promoted here, two more rows the same tranche moved:
    //   ??_7Stream@IO@@6B@           7/ 1 TRAP=1  ->   7/ 2 TRAP=0  READY
    //       — READY and STILL UNPROMOTABLE(NO-OP). Phase 0a proved from the
    //         shipped bytes that nothing dispatches while that table is
    //         installed, and READY does not touch that: a green readiness row
    //         says SAFE TO STAMP, not that anything dispatches it
    //         (reference_a_green_readiness_screen_is_not_a_route.md). The tag
    //         stands and this lane deliberately does not promote it.
    //   ??_7CompressedMemoryStream@ 9/3 TRAP=6 -> 9/4 TRAP=5, and
    //   ??_7CompressedStream@       9/1 TRAP=8 -> 9/2 TRAP=7
    //       — lane CSTREAM's rows, moved by ?GetSource@Stream@IO@@ landing in
    //         slot 1 of both. Not this lane's to promote.
    //
    // The bodies are runtime/src/io/iobase_vtables.cpp: each row's slot-0
    // vector deleting destructor (all four unexported, registered by address —
    // which is the door the mint slot itself uses), plus
    // `?GetSource@Stream@IO@@` and the five VisitorStream stream methods.
    //
    // WHY THESE FOUR AND NOT ALL SIX: the two above are excluded on their own
    // grounds, stated rather than left implicit.
    //
    // ATTENDANCE, and where it is honest and where it is not. Measured with
    // `BANISHED_W17_ATTEND=1` on the gameplay route in the same run as the
    // readiness table (docs/runtime_port/wave19_iobase/run1/):
    //
    //   ??_7MemoryStream@IO@@6B@    ATTENDED  post=1
    //   ??_7VisitorStream@IO@@6B@   ATTENDED  post=2
    //   ??_7BinaryReader@IO@@6B@    never     post=0
    //   ??_7Visitor@IO@@6B@         NOT IN THE INSTRUMENT'S 62-ROW SUBJECT
    //
    // So two of the four are demonstrably live on the route this promotion was
    // measured on; one is demonstrably not; and for one the instrument has no
    // opinion at all, which is UNMEASURED and not a green cell. `post` counts
    // door opens, not dispatches, and `post==1` is not "ran once" — it is
    // "the symbol's door opened at least once after AddressRegistryInit".
    //
    // Promoting the two unattended rows is still right, and for the reason
    // lane SS gave for SpriteSheet slot 5 and lane VRES repeated for
    // ImageBuffer: TRAP=0 means no slot in the row can `abort()` any more, and
    // leaving a row unpromoted does not verify it — it only keeps the slots
    // unreachable. BinaryReader in particular is the READ half of the
    // save/load codec pair whose WRITE half (`??_7BinaryWriter@`) has been
    // READY and unpromoted since Wave 17; see WAVE19_LANE_IOBASE.md for the
    // route question, which is answered there and is NOT answered by this
    // comment.
    PromoteVtableGuarded("??_7BinaryReader@IO@@6B@", 0x1801552e0ull, &n);

    PromoteVtableGuarded("??_7Visitor@IO@@6B@", 0x1801558b0ull, &n);

    PromoteVtableGuarded("??_7MemoryStream@IO@@6B@", 0x1801555b0ull, &n);

    PromoteVtableGuarded("??_7VisitorStream@IO@@6B@", 0x180155a20ull, &n);

    // ---- ??_7BinaryWriter@IO@@6B@ — the row that was READY since Wave 17 ----
    //
    // This one is NOT unblocked by this lane's bodies: it has read 26/26 TRAP=0
    // since WAVE17_LANE_CEIL and phase 0c CONFIRMED it at `ebbec6e`. It has sat
    // unpromoted because nobody could show a route that dispatches it. Promoted
    // here, and the warrant is DELIBERATELY WEAKER than the four above — it
    // rests on TRAP=0 plus a door argument, NOT on a dispatch observation, and
    // that distinction is the point of writing it down.
    //
    // ATTENDANCE, on every route this lane could drive
    // (docs/runtime_port/wave19_iobase/, `BANISHED_W17_ATTEND=1`, control
    // `?_singleton@MainLoop@Engine@@` GREEN on all four so the zeros are
    // findings and not a dead instrument):
    //
    //   run1  gameplay                                post=0
    //   run2  gameplay + a COMPLETED in-game save      post=0
    //         ([SaveTest] frame 961: SAVE OPERATION COMPLETE), and
    //         `[Encoding] ATTENDANCE: none of the four bodies was entered`
    //   run4  same with BANISHED_RT_IOBASE=0           post=0
    //   run5  BANISHED_TOOL_HARNESS=dlgsettings        UNMEASURED, NOT post=0:
    //         the route did not fire. Its own line says why — "no entity with
    //         a UIComponent in this world (searched tid=0x988e1422 over 23002
    //         entities) -- the route has no subject here". An unmeasured cell
    //         is not a green cell.
    //   the MODS route was NOT ATTEMPTED: CLAUDE.md records it crashing in
    //   ModDialog_ToggleMod_Impl+0x43 and this lane did not re-test that.
    //
    // The strongest measured statement is therefore: **a full, successful
    // in-game save does not open this door.** That is worth more than it looks
    // — it settles that the save path does not run an IO::Encoding BinaryWriter
    // codec, which is what one would have assumed.
    //
    // WHY IT IS NOT THE PERMANENTLY-UNPROMOTABLE SHAPE. A minted row can be
    // ported in full and never promotable, because we intercept only at DOORS
    // and an in-DLL rel32 call or RIP-relative LEA stamp is not one
    // (reference_a_ported_row_can_be_permanently_unpromotable.md). That is NOT
    // this row. All three bodies that install this table are OURS and all three
    // stamp it through a NAME door:
    //   ??0BinaryWriter  -> RtIo_BinaryWriter_Ctor,        encoding.cpp:259
    //   ??1BinaryWriter  -> RtIo_BinaryWriter_Dtor,        binary_writer.cpp:187
    //   slot-0 deleting  -> RtIo_BinaryWriter_DeletingDtor, binary_writer.cpp:222
    // each reaching `VtblBinaryWriter()` == `ResolveExport("??_7BinaryWriter@…")`.
    // So ANY route that constructs a BinaryWriter arrives at whichever table
    // this promotion selects. The row is attendable in principle; what is
    // missing is a driver, not an interception point.
    //
    // If nothing ever dispatches it, promoting changes nothing. If something
    // does, it changes from the DLL's table to one with no traps in it. Neither
    // outcome is a regression, and leaving it unpromoted would not verify it.
    PromoteVtableGuarded("??_7BinaryWriter@IO@@6B@", 0x180155210ull, &n);

    // ========================================================================
    // WAVE 20 LANE PROMOTE — ??_7BinaryFileStream@IO@@6B@, and it is the ONLY
    // row of seven candidates that passes the §5.1 stamper gate
    // ========================================================================
    //
    // THE GATE, restated because it is the whole decision: a row is promotable
    // insofar as WE OWN THE FUNCTION THAT STAMPS IT (WAVE19_CLOSED.md §5.1).
    // Every stamp is an in-DLL RIP-relative LEA followed by a store, which is
    // NOT a door, so TRAP=0 says nothing at all about who installs the table.
    //
    // THE STAMPER CENSUS WAS ENUMERATED FROM THE BINARY, not read off our own
    // port list. `tools/mint_stamper_census.py` (new this lane; it BORROWS lane
    // EXTENT's writer channel rather than rebuilding one) groups every
    // rip-relative-LEA-then-store of each table address by the `.pdata`-bounded
    // function containing it, and joins that against a literal registration
    // scan of runtime/ engine/ code/ standalone/ dx11/ WITH COMMENTS STRIPPED.
    // Run on this tree, decoder control 3 of 3, ownership controls both green:
    //
    //   row                              stampers  ours-capable  retail  unattr
    //   ??_7BinaryFileStream@IO@@6B@            2          2         0       0   <- promoted here
    //   ??_7FileStream@IO@@6B@                 17          6        11       0
    //   ??_7CompressedStream@IO@@6B@            9          5         4       0
    //   ??_7CompressedMemoryStream@IO@@6B@      7          5         2       0
    //   ??_7CompressedFileStream@IO@@6B@        8          1         7       0
    //   ??_7ReaderWriter@IO@@6B@               20          7        10       3
    //   ??_7Stream@IO@@6B@                     50         22        25       3
    //
    // The two stampers of THIS row, with the instruction and the function each
    // sits in — both are ported and registered by name AND by address in
    // runtime/src/io/file_stream.cpp:
    //
    //   0x1800a8270  mov %rax,(%rdi)   ??0BinaryFileStream@IO@@QEAA@XZ +0x70
    //   0x180023cfe  mov %rax,(%rbx)   ??0BinaryFileStream@IO@@QEAA@AEBV01@@Z +0x5e
    //
    // That agrees with lane FILE's independent `get_xrefs_to` count (file_stream.cpp
    // header, "briefed as ONE stamper, measured TWO"). TWO DERIVATIONS BY
    // DIFFERENT MEANS, and the briefed number was wrong — on the briefed claim
    // the row would have been declared owned with the copy constructor's stamp
    // still in the DLL.
    //
    // `ours-capable` is what the tool prints and it is an UPPER BOUND: a
    // registration is a DOOR, not a guarantee that every caller uses it. For
    // this row the bound is tight in the only way that matters — both stampers
    // are EXPORTS registered under their own mangled names, so any caller that
    // resolves either by name or by address reaches our body, and the A/B below
    // MEASURES the result rather than inferring it.
    //
    // THE SLOTS: 8 of 8 registered, 0 purecall, TRAP=0 (census PROMOTION
    // BACKLOG on this tree reads `??_7BinaryFileStream@IO@@6B@  8  8  COMPLETE`).
    // Slot 0 is the unexported deleting-dtor thunk at 0x180023d80, registered by
    // ADDRESS; slots 1..7 are the seven exported FileStream methods.
    //
    // A/B AND ATTENDANCE. `BANISHED_RT_PROMOTE_BFS=0` turns THIS ROW off and
    // nothing else — a per-row switch, not the file-wide
    // BANISHED_RT_MINT_PROMOTED=0, so the comparison has ONE variable. Both
    // halves are printed by file_stream_cutover_probe.cpp
    // (BANISHED_RT_FILE_CUTOVER=1) as `[BfsPromote]`, with `stamps` as the
    // denominator and an explicit "I MEASURED NOTHING" line when it is zero.
    // The measured numbers are in docs/runtime_port/WAVE20_LANE_PROMOTE.md.
    //
    // WHAT THIS DOES NOT CLAIM. Attendance is not verification: the bodies'
    // oracle is runtime/src/io/file_stream_slots_probe.cpp and it is a separate
    // tier. This promotion says our table is installed and dispatched; that
    // probe says the bodies agree with retail's.
    PromoteVtableGuarded("??_7BinaryFileStream@IO@@6B@", 0x1801554d0ull, &n);

    // ========================================================================
    // WAVE 29 LANE IOLANE — ??_7PackageFile@IO@@6B@
    // ========================================================================
    //
    // IO's LAST PROMOTABLE VTABLE. (IO's other three gap rows are
    // `??_7ReaderWriter@` and `??_7Stream@`, both tagged UNPROMOTABLE, and
    // `??_7TextFileStream@`, which is 10/10 COMPLETE and measured NEVER
    // ATTENDED — see the block after this one.)
    //
    // ---- WHAT WAS BLOCKING IT, AND THAT BLOCKER HAS EXPIRED ---------------
    // The committed Wave 19 phase-0c evidence
    // (docs/runtime_port/wave19_phase0c/run2_attend/exit.tbl:39) reads
    //     BLOCKED ??_7PackageFile@IO@@6B@ slots= 9 ported= 5
    // i.e. FOUR TRAPS. Waves 23/25/27 closed all four (slot 0 lane VTIO;
    // slots 5/6/7 lane IOTEXT). The census on this tip prints the row
    // 9 slots / 9 ours / COMPLETE. The blocker named a MISSING CAPABILITY and
    // capability blockers expire (kickoff warning 3); this one has.
    // PromoteVtableGuarded re-asks the trap question AT RUNTIME against the
    // live registry, so this paragraph is not what the decision rests on.
    //
    // ---- ATTENDANCE: MEASURED TWICE ON THIS TIP, IN TWO ARMS --------------
    // `BANISHED_W17_ATTEND=1`, ordinary `BANISHED_AUTOSTART=1` run with a
    // clean self-quit, control `?_singleton@MainLoop@Engine@@` GREEN
    // (init=1 post=3) in both:
    //
    //   arm A (probe + RT_DIFF + RT_STRICT armed)  ATTENDED init=0 post=1
    //   arm B (NOTHING else armed)                 ATTENDED init=0 post=1
    //
    // ARM B IS THE ONE THAT COUNTS, and running it is not a formality. In the
    // SAME pair of runs the neighbouring row
    // `?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A` read post=1 in arm
    // A and **post=0 in arm B** — i.e. its attendance was manufactured by the
    // instrumentation, which is exactly the failure this file's own Wave 26
    // retraction (search `A CHEAPER CLOSE WAS PROPOSED`) says reads
    // indistinguishably from the real thing. Without arm B this lane would
    // have reported that row's route condition satisfied. It is NOT satisfied
    // and it is NOT promoted here.
    //
    // `post=1` is NOT "ran once" — it is "the door opened at least once after
    // init", because consumers cache in a function-local static. The dispatch
    // evidence is separate and much larger: in arm A,
    // `IO::PackageFile::PackageFile` reports calls=14 entered=14 worked=14
    // divergences=0, and the per-entry accessors 5836-9471 calls each, all
    // divergences=0, over 4 real mounted packages.
    //
    // ---- STAMPER OWNERSHIP: 3 OF 4, AND THE FOURTH IS NAMED ---------------
    // `python tools/mint_stamper_census.py --rows="??_7PackageFile@IO@@6B@"`
    // on this tip:
    //     STAMPERS 4 ; ours-capable 3 ; retail 1 ; unattributed 0
    //       ?RefreshList@Modification@IO@@   0x1800af846  OURS
    //       ??0PackageFile@IO@@QEAA@XZ       0x1800b447f  OURS
    //       ??1PackageFile@IO@@UEAA@XZ       0x1800b455b  OURS
    //       ??0PackageFile@IO@@QEAA@AEBV01@@Z 0x18002562c RETAIL (copy ctor,
    //                                                     UNPORTED)
    // So the population of live PackageFiles is MIXED: one built by retail's
    // copy constructor carries the DLL's table, one built by any of the other
    // three carries ours. That is NOT a hazard — both tables are correct and
    // the nine bodies behind ours are the ones measured above — but it is the
    // honest ROUTE SCOPE of this promotion and it is stated instead of a bare
    // verdict. At cutover the retail copy ctor does not exist at all, so the
    // mixture is a DLL-present phenomenon only. `ours-capable` is an UPPER
    // BOUND (the tool says so); it is not read here as "every caller uses it".
    //
    // ---- THE COST, ANNOTATED IN THIS COMMIT AND NOT AFTER IT --------------
    // Promoting this row changes what `PackageFileVtable()`
    // (runtime/src/io/package_file.cpp:318, an `RT_DATA_SLOT`, registry-first)
    // hands our constructor, so OUR ctor stamps the minted table while
    // retail's stamps the DLL's. `RtIo_PackageFile_Ctor`'s `RT_DIFF_BYTES`
    // oracle compares those two objects byte for byte and would therefore go
    // permanently red at the vtable field — the "Construct divergence IS the
    // vtable identity" shape this file already records for
    // `?TypeName@SpriteSheet@UI@@`. That site is updated in the SAME commit
    // (package_file.cpp) so no sweep reads it as a regression, and the
    // divergent offsets there were MEASURED, not assumed.
    //
    // ⚠ THE SENTENCE THAT USED TO STAND HERE WAS WRONG, AND THE CORRECTION IS
    // THE MOST USEFUL THING THIS LANE FOUND. It read: "The Destruct and
    // Reconstruct memcmp oracles in the same file are NOT affected and this was
    // checked rather than assumed: both build BOTH sides with
    // `RtIo_PackageFile_Ctor`, so both sides get whichever table is live."
    //
    // Destruct is unaffected and still reads MATCH. **RECONSTRUCT DIVERGED on
    // the very first promoted run**, and the reasoning above is where it broke:
    // retail's `?Reconstruct@` destructs in place through the object's own
    // vtable slot 0 — which with our table installed is OUR body — and then
    // TAIL-JUMPS retail's constructor at 0x1800b4440 by an **in-DLL rel32**.
    // An in-DLL rel32 is NOT A DOOR; no registration can intercept it; so
    // retail's ctor re-stamps the DLL's table over an object we built.
    //
    // Two things generalise, and neither is about PackageFile:
    //   * `reference_a_ported_row_can_be_permanently_unpromotable.md` is
    //     usually read as "we cannot get our table IN". This is the mirror:
    //     retail can put ITS table BACK, over an object of ours, at any time,
    //     through a call our tree cannot see. A promotion therefore has a
    //     LIFETIME as well as a route.
    //   * The claim was written as "checked rather than assumed" and it was
    //     assumed. What caught it was a print that ran regardless of what I
    //     believed. The correction is left in place rather than deleted
    //     (reference_a_prescription_read_as_a_completion.md).
    //
    // CONSEQUENCE FOR THE PROMOTION: none for correctness. The DLL's table and
    // ours hold the same nine behaviours (all nine are oracle-verified, 0
    // divergences), so an object retail re-stamps is not broken — it simply
    // stops dispatching OUR bodies. It is a COVERAGE fact, not a hazard, and
    // at cutover 0x1800b4440 is our body anyway. The Reconstruct oracle in
    // package_file.cpp now excuses exactly the derived vtable slot and compares
    // the other 232 bytes whole.
    //
    // A/B control: `BANISHED_RT_PROMOTE_PKGFILE=0`.
            PromoteVtableGuarded("??_7PackageFile@IO@@6B@", 0x180155700ull, &n);

    // ---- ??_7TextFileStream@IO@@6B@ — COMPLETE, and NOT PROMOTED ----------
    //
    // NOT an oversight and not a readiness problem. The row is 10/10 COMPLETE
    // on this tip (Waves 20 SAVETEXT, 21 ENC, 28 SLOTS closed every slot).
    // It is left on the DLL because BOTH halves of the case against it are
    // measured and neither is about readiness:
    //
    //   * ATTENDANCE: `never init=0 post=0`, in BOTH arms of the pair of runs
    //     described above, in a run whose control was GREEN and in which 40 of
    //     67 subject rows DID read post>0. So this is a real absence and not a
    //     dead instrument. It reproduces Wave 19 phase-0c exactly.
    //   * STAMPERS: `mint_stamper_census.py` reports
    //     `STAMPERS 8 ; ours-capable 3 ; retail 5 ; unattributed 0`. The five
    //     in the DLL are `??0TextFileStream@IO@@QEAA@AEBV01@@Z`,
    //     `??0TextFileStream@IO@@QEAA@XZ`, and TextFile's `Close` / `Create` /
    //     `Open`. Wave 19 §5.1's criterion — a row is promotable insofar as we
    //     own the function that STAMPS it — puts this row at 3/8, the weakest
    //     ratio of any IO vtable, where `??_7CompressedFileStream@` at 0/6
    //     drew "promote none".
    //
    // [ADDRESSEE: the lane that ports `??0TextFileStream@IO@@QEAA@XZ`
    //  (0x1800ba530) and `?Open@TextFile@IO@@QEAA_NAEBVString@System@@@Z`
    //  (0x1800b9fc0) — those two are the stampers on the `?SaveText@External@`
    //  path, which is the one route that could give this row a post>0.]
    // [DO NOT re-derive the attendance; it is measured twice above. What is
    //  missing is STAMPER OWNERSHIP and a ROUTE, not a slot.]

    // ========================================================================
    // WAVE 25 LANE VTGFX — ??_7MaterialInstance@Graphics@@6B@
    // ========================================================================
    //
    // THE FIRST GRAPHICS VTABLE PROMOTED. It is the ONLY one of the eight
    // Graphics `??_7` gap rows for which a promotion can change a dispatch,
    // and the sorting axis that says so is the PURECALL COUNT, not the
    // readiness screen (WAVE23_LANE_GFXVT.md §0: readiness and value are
    // ANTI-CORRELATED here). Re-derived this lane from the shipped bytes with
    // tools/vtable_dump.py, whose `_purecall` control printed FOUND:
    //
    //   AnimationEvent  9 slots, 5 purecall -> ABSTRACT, 4/4 COMPLETE, NO-OP
    //   Geometry        9 slots, 3 purecall -> ABSTRACT, 6/6 COMPLETE, NO-OP
    //   MaterialInstance 9 slots, 0 purecall -> CONCRETE, 9/9 COMPLETE  <- this
    //   the other five  incomplete (1 to 6 slots short)
    //
    // A class with a pure virtual cannot be instantiated, so its table is only
    // ever a base-ctor window that the derived ctor overwrites one statement
    // later. MaterialInstance has NO pure virtual, is LEAF in Graphics, and
    // its table IS a live object's final vptr.
    //
    // WHAT CHANGED SINCE WAVE 23 REFUSED IT. GFXVT's stated blocker was
    // "blocked, 4 slots short" and nothing else; its cutover verdict was
    // already REQUIRED. Wave 24 lane GFXMAT ported those four
    // (runtime/src/graphics/wave24_lane_gfxmat.cpp: Serialize, Reconstruct,
    // Destruct, Construct) and left the row COMPLETE and deliberately
    // unpromoted, with an explicit residual (WAVE24_LANE_GFXMAT.md §7 R1)
    // naming the promotion as the next step. All nine slot registrations were
    // re-verified BY LITERAL this lane, one grep per slot:
    //   slot 0 0x180002d60  runtime/src/object/deleting_dtor.cpp:844  (ADDR)
    //   slots 1,2,3         runtime/src/graphics/graphics_typeinfo_accessors.cpp
    //   slot 4              runtime/src/object/external.cpp:741 (shared Compile)
    //   slots 5,6,7,8       runtime/src/graphics/wave24_lane_gfxmat.cpp:791-797
    // and every one of those *_PortInit() runs BEFORE RtDataMintInit()
    // (resolve_standalone.cpp:318-406), so the mint fills all nine from the
    // live registry. TRAP=0 is not assumed — see the runtime guard below.
    //
    // STAMPER CENSUS, re-run this lane (tools/mint_stamper_census.py, decoder
    // controls 3 of 3, ownership pos+neg green):
    //
    //   ??_7MaterialInstance@Graphics@@6B@  10 stampers, 5 OURS-CAPABLE,
    //                                        5 retail, 0 unattributed
    //
    // **That is 3 -> 5 since Wave 23**, and the two new ones are GFXMAT's own
    // Reconstruct and the slot-0 deleting dtor. `ours-capable` is an UPPER
    // BOUND (a registration is a door, not a guarantee), so the honest reading
    // is: SOME MaterialInstance objects will carry our table and some will
    // carry retail's, because retail's five stampers are in-DLL and are not
    // doors. Both tables are semantically the same nine methods; a mixed
    // population is not a hazard, it is a coverage limit.
    //
    // ATTENDANCE IS MEASURED, NOT ASSUMED — and the measurement already
    // existed in the tree (reference_grep_the_harness_before_declaring_no_route).
    // standalone/wave17_door_attend.cpp watches this exact symbol, and Wave 17
    // lane CEIL's recorded runs read, with the MainLoop control GREEN in both:
    //     docs/runtime_port/wave17_lane_ceil/attend_load.txt:49
    //        ATTENDED init=0 post=1   ??_7MaterialInstance@Graphics@@6B@
    //     docs/runtime_port/wave17_lane_ceil/attend_save.txt:49
    //        ATTENDED init=0 post=1   ??_7MaterialInstance@Graphics@@6B@
    //     docs/runtime_port/wave17_lane_ceil/attend_mods.txt:49
    //        never    init=0 post=0   (the mods route does not reach it)
    // `post` counts DOOR OPENS, not dispatches, and post==1 is NOT "ran once"
    // — every consumer caches in a function-local static. So what this proves
    // is that a consumer of this symbol RAN on the save and load routes. The
    // consumers are exactly four, all re-grepped this lane:
    //   runtime/src/graphics/renderer.cpp:561       our ported ctor's stamp
    //   runtime/src/graphics/wave24_lane_gfxmat.cpp:244  Construct/Reconstruct
    //   runtime/src/save/saveload_draw.cpp:373/560  the SaveLoad thumbnail stamp
    //   standalone/wave17_door_attend.cpp           the instrument itself
    // It does NOT prove a slot body was dispatched. That is a separate tier and
    // it is NOT claimed here (§ "what this does not claim", below).
    //
    // THE CONSUMER THAT MADE ??_7Geometry@ HARMFUL DOES NOT APPLY HERE, and it
    // was checked rather than assumed, because it is the one failure mode that
    // has actually bitten a Graphics vtable promotion in this tree. Geometry is
    // HARMFUL because code/Game/Component/statusicon_geometry.cpp COPIES slots
    // out of the stamped table with a fixed bound. MaterialInstance has the
    // same shape of consumer — code/Game/Dialog/thumb_vtables.cpp builds the
    // four dialogs' thumbnail vtable by copying out of whatever the Runtime
    // ctor installed — and it is WIDTH-CORRECT:
    //     thumb_vtables.cpp:38   constexpr int kSlots = 9;
    //     thumb_vtables.cpp:74   for (int i = 1; i < kSlots; i++) ...
    //     rt_data_mint_tables.inc:1197  static void* kVTable38[9];
    // 9 against 9, copying indices 1..8. No overrun exists in either direction,
    // which is precisely the fact that is false for Geometry.
    //
    // NO CONSUMER READS THIS SYMBOL AS AN IDENTITY TOKEN. Grepped across
    // runtime/ code/ standalone/ engine/ dx11/: the four consumers above all
    // STAMP the pointer; none compares an object's vptr against it. That is the
    // `standalone/scene_watch.cpp` shape that makes ?AddDrawItems@MeshInstance@
    // permanently HARMFUL, and it is absent here.
    //
    // THE RUNTIME SELF-REFUSAL, and why it is new. A promoted vtable's slots
    // are filled from the LIVE port registry at RtDataMintInit(). Three wave
    // kill switches can empty this row's slots at runtime —
    // BANISHED_RT_GRAPHICS=0, BANISHED_RT_GFXTI=0, BANISHED_RT_GFXMAT=0 — and
    // the promotion would then publish a table of named abort traps into live
    // objects. The build is clean and links; kickoff §3.6 says exactly that a
    // build check cannot see this. RtMintVtableTrapCount() (added this lane,
    // standalone/rt_data_mint.cpp) answers it, and the row refuses ITSELF
    // rather than trusting the A/B operator. The five vtables promoted in
    // Waves 19-20 carry the same exposure unguarded; extending this guard to
    // them is a SWEEP-sized change and is NOT made here (§ addressee below).
    //
    // A/B CONTROL: BANISHED_RT_PROMOTE_MATINST=0 turns THIS ROW off and
    // nothing else, following the ??_7BinaryFileStream@ precedent above, so a
    // comparison has ONE variable rather than the file-wide
    // BANISHED_RT_MINT_PROMOTED=0.
    //
    // WHAT THIS PROMOTION DOES **NOT** CLAIM — stated before anyone reads a
    // green line as coverage:
    //   * NO LANE-VTGFX GAME RUN HAPPENED. Wave 25 gave this lane no
    //     game-directory lock by design (kickoff §1d). The attendance above is
    //     Wave 17's, on a Wave 17 tree; it establishes the ROUTE exists, not
    //     that it still runs identically today.
    //   * NO SLOT BODY IS VERIFIED BY THIS. GFXMAT's synthetic differential
    //     oracle (tools/gfxmat24_probe/, 5 of 6 controls CAUGHT) covers
    //     Serialize/Construct/Reconstruct/Destruct against the SHIPPED export;
    //     slots 1,2,3 (Type/TypeName/GetFileVersion) and slot 4 (the shared
    //     Compile@External) are covered by neither this promotion nor that
    //     probe.
    //   * `Graphics::MaterialInstance::RenderTextures` measured
    //     worked=201,103 / calls=0 / divergences=0 (Wave 24 lane DIFF). NO
    //     EXISTING MaterialInstance GREEN MAY BE READ AS COVERAGE OF THIS ROW.
    //   * A PERMANENT +0x00 DIVERGENCE IS THE PROMOTION'S SIGNATURE, NOT A
    //     REGRESSION. renderer.cpp's ctor and gfxmat's Construct[ORACLE] both
    //     byte-compare against retail; a promoted table makes the vptr differ
    //     by construction. GFXMAT wrote the Construct oracle to score the vptr
    //     on its OWN screen precisely so this shows as a pointer move
    //     (reference_a_control_can_corrupt_a_byte_that_should_not_be_compared).
    //
    // ADDRESSEE for the two things this lane could not do:
    //   [SCOPE: DLL-PRESENT] [ADDRESSEE: WAVE 25 lane SWEEP, which holds the
    //   second game-directory slot] — run one autostart boot with
    //   `BANISHED_W17_ATTEND=1 BANISHED_RT_DIFF=1` and a second with
    //   `BANISHED_RT_PROMOTE_MATINST=0`, and report (a) that this row still
    //   reads ATTENDED, (b) the `Graphics::MaterialInstance::Construct[ORACLE]`
    //   vptr screen, which must move and only move, (c) whether any slot body
    //   records calls>0, which is the dispatch evidence nobody has yet.
    //   [SCOPE: DLL-PRESENT] [ADDRESSEE: WAVE 25 lane SWEEP] — extend the
    //   RtMintVtableTrapCount() self-refusal to the five vtables promoted in
    //   Waves 19-20, which have the identical exposure and no guard.
    {
        const int traps = RtMintVtableTrapCount("??_7MaterialInstance@Graphics@@6B@");
        if (traps != 0) {
            // traps < 0 means "not a minted vtable row", which is a DIFFERENT
            // failure from "has traps" and must not read as a green light.
            fprintf(stderr,
                    "[RtMint] REFUSING to promote ??_7MaterialInstance@Graphics@@6B@: "
                    "RtMintVtableTrapCount()=%d (expected 0). %s Publishing this "
                    "table would put named abort traps in a live object's vptr. "
                    "The row stays on the DLL.\n",
                    traps,
                    traps < 0 ? "The symbol is not a minted VTABLE row at all."
                              : "At least one slot has no registered port.");
            fflush(stderr);
        } else {
            st = RtMintPromotionStorage("??_7MaterialInstance@Graphics@@6B@", 0x1801545e0ull);
            RegisterRuntimeExport("??_7MaterialInstance@Graphics@@6B@", st);
            RegisterRuntimeAddr(0x1801545e0ull, st); ++n;
        }
    }

    // ========================================================================
    // WAVE 26 LANE PROMOGFX — TWO OF WAVE 25 LANE DATA's SEVEN `?_typeInfo@`
    // ROWS. THE OTHER FIVE ARE **NOT** PROMOTED. Read this whole block before
    // adding a sixth.
    // ========================================================================
    //
    // Lane DATA left those seven rows READY and blocked on exactly ONE thing:
    // CRITERION 2 at the top of this file — "the row must be run in a real boot
    // and the run must reach the same place the control reaches". Wave 26 lane
    // ATTEND ran it. Its verdict decides each row individually, and it did NOT
    // come out the same for all seven.
    //
    // ATTEND's run: tip wave26/base 7de42dd1, exe md5
    // 45c72978bfbc80f17935d9701fe58977, BANISHED_W17_ATTEND=1
    // BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120,
    // clean exit 0, 0-byte crash log, CONTROL GREEN
    // (?_singleton@MainLoop@Engine@@ init=1 post=2). 66 subject rows.
    // Full table: docs/runtime_port/wave26_attend/run1_w17attend.txt.
    //
    //   ?_typeInfo@Achievement@1@                 init=0 post=1   ATTENDED
    //   ?_typeInfo@GoalInstance@Achievement@@     init=0 post=1   ATTENDED
    //   ?_typeInfo@AchievementInstance@Achievement@@  init=0 post=0   never
    //   ?_typeInfo@AnimationEvent@Graphics@@      init=0 post=0   never
    //   ?_typeInfo@MaterialInstance@Graphics@@    init=1 post=0   never
    //   ?_typeInfo@Particle@Graphics@@            init=1 post=0   never
    //   (?_typeInfo@PackageFile@IO@@ is lane PROMOIO's row, not decided here.)
    //
    // THE TWO BELOW ARE PROMOTED because post>0 IS criterion 2: a consumer in
    // this tree opened the door after init, on a real boot that reached the
    // control. The four `never` rows are NOT promoted and are NOT tagged
    // unpromotable either — see the ROUTE-UNDECIDED block after them.
    //
    // ONE ANNOTATION THAT LANE DATA REQUIRED IS **NOT APPLICABLE HERE**, and
    // that is worth stating rather than silently omitting. DATA's fourth bullet
    // says promoting AnimationEvent / MaterialInstance / Particle / PackageFile
    // makes their `?TypeName@` bodies hand out OUR node's `name` POINTER, so
    // their RT_DIFF_SCALAR sites read `divergences=N` forever after — annotate
    // beside each body IN THE SAME COMMIT. Neither row below has that exposure:
    // `?TypeName@Achievement@1@` (table slot 1, 0x180001040) and
    // `?TypeName@GoalInstance@Achievement@@` (table slot 1, 0x180001e60) are
    // UNREGISTERED — those slots are two of the reasons ??_7Achievement@0@6B@
    // reads 4/9 and ??_7GoalInstance@Achievement@@6B@ reads 3/7 — so there is
    // no ported body to hand out a pointer and no RT_DIFF_SCALAR site to
    // annotate. The obligation transfers with the slot: WHOEVER PORTS EITHER
    // `?TypeName@` MUST write that annotation then.
    st = RtMintPromotionStorage("?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A",
                                0x1801f9898ull);
    RegisterRuntimeExport("?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9898ull, st); ++n;

    st = RtMintPromotionStorage("?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A",
                                0x1801f9940ull);
    RegisterRuntimeExport("?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A", st);
    RegisterRuntimeAddr(0x1801f9940ull, st); ++n;

    // ------------------------------------------------------------------------
    // ROUTE-UNDECIDED — the four `?_typeInfo@` rows ATTEND measured `never`.
    // NOT promoted, NOT tagged RT_UNPROMOTABLE, and NOT dropped.
    // ------------------------------------------------------------------------
    //
    // A bare `never` cannot tell "this row is not ready" from "this route does
    // not reach this row", and those have OPPOSITE consequences. ATTEND was
    // asked to separate them and did: ALL FOUR are (b), route-never-reached,
    // and the zero is therefore NOT evidence about the row.
    //
    //   * The general (a)-refutation, applying to all four: in the SAME run,
    //     rows using the identical RT_DATA_SLOT -> RtResolveCached ->
    //     ResolveExport idiom read post>0 — ?_typeInfo@Material@Graphics@@
    //     post=1 and ?_typeInfo@GeneratedImageBuffer@Graphics@@ post=1, same
    //     file family. The idiom is proven live in that run, so a zero cannot
    //     be blamed on the door mechanism. ATTEND also checked every consumer
    //     literal byte-for-byte against its watch-table literal, which excludes
    //     spelling drift — the one (a)-shaped failure this instrument has.
    //   * MaterialInstance and Particle prove their OWN door: init=1 means the
    //     symbol resolved once, from reg_unreg_lane3.cpp:164/195, so post=0 can
    //     only mean no post-init consumer ran.
    //   * AnimationEvent, MaterialInstance and Particle are corroborated: their
    //     only consumers are the vtable-slot bodies at
    //     graphics_typeinfo_accessors.cpp:132/136/140, and in the same run
    //     ??_7AnimationEvent@, ??_7MaterialInstance@, ??_7Particle@,
    //     ??_7ParticleInstance@ and ??_7ParticleSystemInstance@ ALL read
    //     post=0 — nothing fetched any of those tables through a door, so those
    //     slot bodies never dispatched.
    //   * AchievementInstance's only consumer is gameupdateloop_serialize.cpp:195
    //     inside EnsureResolved(), and its "[GUL::Serialize] resolved:" line has
    //     ZERO occurrences in both logs. Corroborated twice more:
    //     "[SaveMetaOracle] bodyCalls=0 calls=0" and "[RefTable] calls=0".
    //     A New-Game-and-quit route neither saves nor loads.
    //
    // ATTEND'S OWN CAVEAT, carried here so it is not lost: this instrument
    // counts DOORS, not EXECUTIONS. "Route never reaches it" and "route reaches
    // those objects but only through in-DLL calls, which are not doors" are
    // BOTH consistent with post=0 and it cannot separate them. For a promotion
    // decision the consequence is identical — the zero is not evidence about
    // the row — but it is NOT proof the class is unused.
    //
    // [SCOPE: DLL-PRESENT, on the autostart New-Game-and-quit route only.]
    // [ADDRESSEE: the Wave 27 kickoff writer, who must NOT copy these four into
    //  a cheap-wins list as "ready, just promote them" — criterion 2 is still
    //  unmet for all four.]
    // [CONDITION, per row:
    //    ?_typeInfo@AchievementInstance@ — A RUN THAT SAVES. ATTEND named it
    //      exactly: re-run the W17 watch on a route that reaches
    //      GameUpdateLoop::Serialize. post>0 there is criterion 2 and the row
    //      can be promoted with no further argument.
    //    ?_typeInfo@AnimationEvent@ / @MaterialInstance@ / @Particle@ — these
    //      three are downstream of a DIFFERENT question: their consumers are
    //      vtable slot bodies whose TABLES are not fetched through a door on
    //      any route measured so far. Do NOT keep re-running the autostart
    //      route expecting a different answer. The prerequisite is an
    //      EXECUTION counter inside graphics_typeinfo_accessors.cpp:132/136/140
    //      (the accessor bodies themselves), which separates "no route" from
    //      "reached only by in-DLL call". Until someone builds that, these
    //      three rows are undecidable by the door instrument and no amount of
    //      booting changes it.
    //    AND WHEN ANY OF THE THREE IS PROMOTED: lane DATA's `?TypeName@`
    //      RT_DIFF_SCALAR annotation DOES apply to them, unlike the two rows
    //      promoted above. Write it in the same commit.]

    // ========================================================================
    // WAVE 26 LANE PROMOGFX — ??_7Geometry@Graphics@@6B@ (0x180154e60)
    // ========================================================================
    //
    // THIS ROW WAS HANDED TO THIS LANE BY NAME. Wave 21 lane GFXVT tagged it
    // HARMFUL; Wave 22 lane GEOM discharged the blocker and wrote, in the
    // RESOLVED note above, that "re-scoring and actually registering the
    // promotion is a SEPARATE, still-open step lane GEOM did not take". This
    // lane is that step, and it did not take GEOM's word for any of it.
    //
    // RE-DERIVED HERE, not re-quoted:
    //
    //   1. THE BLOCKER IS GONE. The whole HARMFUL verdict rested on ONE
    //      consumer reading past the end of the table:
    //      code/Game/Component/statusicon_geometry.cpp copied `i < 12` slots
    //      out of a NINE-slot table into a live exe-side vtable, so promoting
    //      would have moved a dormant overrun off the DLL's .rdata (three
    //      qwords of the "ImageBuffer\0..." string run) onto `kVTable28`, a
    //      `static void* kVTable28[9]`. READ ON THIS TIP: that loop now reads
    //      `if (i < 9) ... else s_siGeomVtable[i] = nullptr;` and never
    //      indexes baseVtbl at 9 or beyond. The overrun does not exist.
    //
    //   2. THE STATIC HALF. tools/mint_slot_sweep.py-equivalent slot scan on
    //      this tip: 9 slots, 3 RT_MINT_PURECALL, 6 non-purecall, 6 of 6
    //      registered -> COMPLETE. Slot 0 is address-registered
    //      (runtime/src/object/deleting_dtor.cpp:912, 0x180015e90); slots 4-8
    //      are name-registered in runtime/src/graphics/scene.cpp:1884-1899.
    //      PromoteVtableGuarded re-asks this at runtime through
    //      RtMintVtableTrapCount and refuses on any non-zero.
    //
    //   3. THE DYNAMIC HALF — WHO STAMPS IT. tools/mint_stamper_census.py on
    //      this tip (decoder control 3/3, positive control REGISTERED,
    //      negative control not-registered): 3 stamp sites in 3 functions,
    //      ours-capable 2, retail 1, unattributed 0.
    //        ??0Geometry@Graphics@@QEAA@XZ   0x1800a4281  OURS (name+addr)
    //        ??1Geometry@Graphics@@UEAA@XZ   0x1800a41d5  OURS (name+addr)
    //        ??0Geometry@Graphics@@QEAA@AEBV01@@Z 0x180015b7e  RETAIL
    //      So a COPY-CONSTRUCTED Geometry keeps retail's table. That is a
    //      SPLIT, not a trap: both tables dispatch the same six bodies, and at
    //      cutover there is no retail body left for the copy ctor to run.
    //
    //   4. THE DOOR EXISTS AND PROMOTION REACHES IT. runtime/src/graphics/
    //      scene.cpp:212 stamps ResolveAddr(Module::Runtime, 0x180154e60), and
    //      ResolveAddr consults the PORT REGISTRY FIRST, so registering by
    //      address here is what re-points that stamp. Registering by NAME as
    //      well matches the ??_7BinaryFileStream@ precedent above.
    //
    // WHAT THIS DOES NOT CLAIM. No A/B was run for this row in this lane, and
    // no attendance counter says a promoted Geometry vtable is ever
    // DISPATCHED. This promotion is the same tier as every other row in this
    // file: the table is installed. Whether our six bodies agree with retail's
    // is a separate tier and is not asserted here.
    //
    // A/B CONTROL: BANISHED_RT_PROMOTE_GEOM=0 leaves the row on the DLL.
            PromoteVtableGuarded("??_7Geometry@Graphics@@6B@", 0x180154e60ull, &n);

    // ========================================================================
    // WAVE 26 LANE PROMOGFX — ??_7AnimationEvent@Graphics@@6B@ (0x180154bf0)
    // ========================================================================
    //
    //   1. STATIC HALF: 9 slots, 5 RT_MINT_PURECALL, 4 non-purecall, 4 of 4
    //      registered -> COMPLETE. Slot 0 is the unnamed deleting dtor
    //      (0x180011800, address-registered); slots 1, 2 and 4 are
    //      ?TypeName@ / ?Type@ / ?Compile@External@Object@, name-registered.
    //
    //   2. DYNAMIC HALF: 5 stamp sites in 5 functions, ours-capable 3, retail
    //      2, unattributed 0 — ??1AnimationEvent (0x1800a2132) and
    //      ??0AnimationEvent@@QEAA@XZ (0x1800a218b) by NAME+ADDR, plus
    //      FUN_180011800 by ADDR; the two retail stampers are the copy ctor
    //      (0x180011777) and FUN_1800117b0. Same SPLIT shape as Geometry, and
    //      the split is over four bodies all of which we own.
    //
    //   3. THE DOOR: runtime/src/graphics/animation.cpp:156 resolves it by
    //      ADDRESS (ResolveAddr(Module::Runtime, 0x180154bf0)) and
    //      animation.cpp:240 and :276 stamp it. Registry-first, so the
    //      address registration below re-points both stamps.
    //
    //   4. NO OUT-OF-BOUNDS CONSUMER. The Geometry hazard was a copy loop over
    //      the table; grepped for on this tip and there is none for this row —
    //      the two consumers assign the pointer and nothing indexes it. The
    //      minted storage is `static void* kVTable5[9]`, the full extent.
    //
    // WHAT THIS DOES NOT CLAIM: as for Geometry, installation only. No A/B and
    // no attendance evidence that the table is dispatched.
    //
    // A/B CONTROL: BANISHED_RT_PROMOTE_ANIMEVT=0 leaves the row on the DLL.
            PromoteVtableGuarded("??_7AnimationEvent@Graphics@@6B@", 0x180154bf0ull, &n);

    // ========================================================================
    // WAVE 26 LANE PROMOGFX — ??_7UpdateLoop@Engine@@6B@ IS **HELD BACK**
    // ========================================================================
    //
    // It is the third COMPLETE table in this lane's namespaces (7/7 of its 12
    // slots non-purecall, every one registered) and it is DELIBERATELY NOT
    // PROMOTED HERE. Stated so the next lane does not read the census's
    // `7 7 COMPLETE` and promote it as the obvious cheap win it looks like.
    //
    // THE REASON IS THE STAMPER CENSUS, and it is a DIFFERENT SHAPE from the
    // two rows above. 8 stamp sites in 8 functions: ours-capable 2, retail 6,
    // unattributed 0. The two ours are ??0UpdateLoop@Engine@@QEAA@XZ
    // (0x1800ddd44) and ??1UpdateLoop@Engine@@UEAA@XZ (0x1800dddc6). The six
    // retail ones are NOT UpdateLoop functions at all — they are the
    // BASE-SUBOBJECT stamps in the ctors and dtors of DERIVED loop classes:
    //   ??0UpdateLoop@Engine@@QEAA@AEBV01@@Z  0x1800317ee  (copy ctor)
    //   FUN_180031880                         0x18003189d
    //   ??0LoaderLoop@Engine@@QEAA@AEBV01@@Z  0x18004d3be
    //   ??0DialogLoopT@Engine@@QEAA@AEBV01@@Z 0x18004d7f3
    //   ??1DialogLoopT@Engine@@UEAA@XZ        0x1800db394
    //   ??1LoaderLoop@Engine@@UEAA@XZ         0x1800dc190
    //
    // Geometry's single retail stamper is a copy ctor and the split it causes
    // is one object's vptr. This row's six are the base-class stamp of THREE
    // OTHER CLASSES whose own tables (??_7LoaderLoop@ 6/12,
    // ??_7DialogLoopT@ 10/12, ??_7DialogLoaderLoopT@ 8/12) are all INCOMPLETE
    // and unpromoted. So promotion here puts OUR table in an UpdateLoop and
    // RETAIL's in the base sub-object of every derived loop, across the
    // hierarchy that runs the entire front end.
    //
    // WHAT THIS LANE DID **NOT** DETERMINE, said plainly rather than guessed:
    // whether those six stamps are TRANSIENT (the ordinary MSVC sequence —
    // base ctor stamps the base table, the derived ctor immediately overwrites
    // it, so the split never outlives the constructor and is harmless) or
    // FINAL. That is decidable by disassembling the six, and it is the whole
    // question. Nothing above settles it, and a lane that promotes this row on
    // the strength of `COMPLETE` will not have asked it.
    //
    // RT_UNPROMOTABLE is deliberately NOT written for this row: it is not a
    // verdict that promotion is harmful, it is an admission that this lane did
    // not finish the check. Tagging it would file an unfinished measurement as
    // a permanent refusal, which is the error
    // reference_unpromotable_is_route_scoped_not_permanent.md warns about from
    // the other side.
    //
    // [SCOPE: DLL-PRESENT — at cutover there are no retail stampers and the
    //  question dissolves; the row is then REQUIRED, because
    //  runtime/src/engine/loops.cpp:313 resolves it BY NAME and ResolveExport
    //  returns NULL for an unpublished row, so loops.cpp:414 and :583 would
    //  stamp a NULL vptr.]
    // [ADDRESSEE: the next lane that owns runtime/src/engine/loops.cpp, or
    //  failing that the Wave 27 kickoff writer.]
    // [CONDITION: disassemble the six retail stamp sites listed above and
    //  record, per site, whether the UpdateLoop table is still in the object
    //  when that function returns. If all six are transient, promote. If any
    //  is final, write an RT_UNPROMOTABLE tag naming that site.]
    //
    // ========================================================================
    // ANSWERED 2026-09-12 — WAVE 27 lane LOOPS, the named addressee.
    // ========================================================================
    // The condition above was discharged: all six retail stamp sites were
    // disassembled. THE VERDICT IS **PROMOTE**, and the four loop tables below
    // are promoted on the same evidence. Full derivation in
    // docs/runtime_port/WAVE27_LANE_LOOPS.md §3; the load-bearing part:
    //
    // THE STAMPER CENSUS WAS RE-DERIVED WITHOUT GHIDRA. Two Ghidra MCP defects
    // were live during this wave (a wrong-program server, and
    // disassemble_function serving a DIFFERENT function's listing relabelled
    // with the requested address). So the census was retaken by scanning .text
    // in the shipped file for every RIP-relative LEA of each vtable VA, and
    // every body by `objdump -d --start-address=…`. The scan reproduces the
    // note above EXACTLY: 8 LEA sites for ??_7UpdateLoop@, at the same 8
    // addresses. Re-derive with the two scanners quoted in the lane doc §3.1.
    //
    // PER-SITE, `objdump` on the retail file (NOT Ghidra):
    //   0x1800317ee  ??0UpdateLoop@…@AEBV01@@Z  +0x1e  FINAL, no overwrite
    //                before `ret` at 0x180031830.  BUT: **0 direct callers.**
    //   0x18003189d  an UNREFERENCED deleting-dtor variant (FUN_180031880).
    //                Not vtable slot 0 — slot 0 is 0x180031900, which does not
    //                stamp. **0 direct callers**; dead code in the shipped .text.
    //   0x18004d3be  ??0LoaderLoop@…@AEBV01@@Z   TRANSIENT — overwritten with
    //                ??_7LoaderLoop@ at 0x18004d3f3, nothing virtual between.
    //   0x18004d7f3  ??0DialogLoopT@…@AEBV01@@Z  TRANSIENT — overwritten at
    //                0x18004d82f.
    //   0x1800db394  ??1DialogLoopT@  destruction-time (inlined ~UpdateLoop).
    //   0x1800dc190  ??1LoaderLoop@   destruction-time, 2 insns after the
    //                derived stamp, nothing between.
    //
    // WHERE THIS DIFFERS FROM THE CONDITION AS WRITTEN, SAID PLAINLY RATHER
    // THAN QUIETLY REDEFINED. The condition asks "is the UpdateLoop table still
    // in the object when that function returns", and answers PROMOTE only if
    // all six are transient. On that LITERAL test sites #1/#2/#5/#6 are "final"
    // and the answer would be a tag. That literal test is the wrong test, for a
    // reason the note's author could not see without the disassembly: it cannot
    // distinguish a ctor from a DTOR (every MSVC base dtor ends with the base
    // vptr in a dead object — #5/#6 — so the literal test can never pass for any
    // class with a virtual dtor), and it asks about the table rather than about
    // whether anything can DISPATCH through it. The substantive question is
    // "can a LIVE object end up holding retail's table", and the answer is no:
    // the two live-object-final sites have ZERO callers, measured twice (a raw
    // E8/E9 scan of .text, and Ghidra xrefs, agreeing).
    //
    // THE COPY-CTOR REACHABILITY RESULT, which is what clears the other three
    // tables too (raw E8/E9 scan of the whole .text):
    //   ??0UpdateLoop@…@AEBV01@@Z         0 callers
    //   FUN_180031880                     0 callers
    //   ??0DialogLoopT@…@AEBV01@@Z        0 callers
    //   ??0DialogLoaderLoopT@…@AEBV01@@Z  0 callers
    //   ??0LoaderLoop@…@AEBV01@@Z         1 caller — 0x18004d58e, which is
    //       INSIDE ??0DialogLoaderLoopT@…@AEBV01@@Z (the base-subobject copy),
    //       and that copy ctor has 0 callers. Transitively unreachable.
    // So no shipped code path constructs any of these four classes by copy, and
    // the "retail stamper" half of each table's census is unreachable code.
    //
    // LIMIT OF THAT SCAN, stated because it is the reason ??_7Loader@ is NOT
    // promoted below: a direct-E8/E9 scan cannot see a call made through the
    // CRT static-initialiser table, so a `0` for a STATIC object's constructor
    // is an artefact, not a finding. `??0Loader@Engine@@QEAA@XZ` scores 0 for
    // exactly that reason — `_loader` is a static — so it is NOT cleared.
    //
    // WHAT THIS DOES NOT CLAIM: that the twelve slot bodies these four tables
    // now route to are CORRECT. They are byte-derived and they run (measured
    // below), but eleven of the twelve have no oracle. This promotion is what
    // gives them a route at all; verifying them is separate work.
    //
    // A/B: each row has its own kill switch so a bisect can isolate one table.
    // ========================================================================
            PromoteVtableGuarded("??_7UpdateLoop@Engine@@6B@", 0x180155ba0ull, &n);
    // ========================================================================
    // MEASURED, AND IT CHANGED THIS LANE'S MIND — the three DERIVED loop tables
    // below are ARMED-OFF BY DEFAULT. Read this before flipping one on.
    // ========================================================================
    // This lane promoted all three, built, and RAN (autostart -> in-game ->
    // pause-menu quit, rc=0, crash log 0 bytes). The promotion WORKED: the guard
    // line went `17 row(s) evaluated` -> `21 ... 0 refused`, so all four tables
    // are COMPLETE with every slot registered. TWO THINGS WERE THEN MEASURED,
    // and the second is why these three are off:
    //
    //  1. ZERO of the twelve slot bodies these tables route to was CALLED. Not
    //     one registered an RtDiff site across a full boot-to-gameplay run.
    //     The reason is NOT the promotion and promoting cannot fix it: every
    //     loop our standalone constructs is an EXE SUBCLASS whose own table
    //     replaces these slots -- g_outerLoaderLoopVtable (12/12 Outer_* stubs,
    //     standalone/loader_loop_stubs.cpp:309), g_menuDialogLoopVtable (13/13
    //     Stub_*, standalone/menudialogloopt_stubs.cpp:757),
    //     g_splashDialogLoopVtable. Those stubs DELEGATE to the base for
    //     Startup/Load/Update (which is why ?Load@DialogLoopT@ and
    //     ?Update@DialogLoopT@ DO show attendance) and OVERRIDE
    //     Shutdown/Draw/OnVideoError outright. A base body the subclass
    //     overrides is unreachable no matter whose table is published.
    //
    //  2. It costs three permanently-RED ctor oracles. `Engine::LoaderLoop::ctor`,
    //     `Engine::DialogLoopT::ctor` and `Engine::DialogLoaderLoopT::ctor` are
    //     RT_DIFF_BYTES oracles comparing an object OUR ctor built against one
    //     the DLL's ctor built. Our ctor stamps the symbol BY NAME, so once the
    //     row is promoted ours holds OUR table and theirs holds the DLL's, and
    //     the objects differ -- at OFFSET +0x0 AND NOWHERE ELSE, verified from
    //     the per-call dumps:
    //         ours  =+0x0: 20 74 f9 fb f6 7f 00 00  (our minted table, in the exe)
    //         theirs=+0x0: 40 79 84 55 ff 7f 00 00  (0x...7940 == ??_7LoaderLoop@)
    //     divergences exactly equalled calls (3/3, 1/1, 3/3): every call, one
    //     qword. THIS IS NOT A PORT DEFECT, and it is a trap for the next reader,
    //     because the row then reads as a regression forever. GENERAL RULE, since
    //     it applies to every future vtable promotion and nothing else records it:
    //     PROMOTING A VTABLE ROW INVALIDATES ANY ctor BYTE-ORACLE FOR THAT CLASS.
    //     Not fixed here (Wave 27 standing warning #1, NO TOOLING).
    //
    // NET: all cost, no measured benefit, on the boot path the whole front end
    // runs through. So they are opt-in. The evidence that they are SAFE stands
    // (the stamper analysis above -- every retail stamper is an unreachable copy
    // ctor or a destruction-time stamp); what is missing is a REASON.
    //
    // [ADDRESSEE: the Wave 28 lane that owns standalone/loader_loop_stubs.cpp or
    //  standalone/menudialogloopt_stubs.cpp.]
    // [CONDITION: flip one on when you make the corresponding exe-subclass stub
    //  DELEGATE to the base body instead of overriding it -- then the base body
    //  has a route and the promotion is what carries it. Expect that class's ctor
    //  oracle to go red at +0x0 when you do; that is item 2, not a bug.]
    // ARM WITH: BANISHED_RT_PROMOTE_LOADERLOOP=1 / _DIALOGLOOPT=1 /
    //           _DIALOGLOADERLOOPT=1   (default OFF, unlike every other row here)
    // ??_7Loader@Engine@@6B@ (0x180157a08) IS **NOT** PROMOTED, and this is a
    // real blocker rather than an unfinished check.
    // RT_UNPROMOTABLE: ??_7Loader@Engine@@6B@ | 2 slots, both ours as of Wave 27 lane LOOPS (slot 0 ?Run@Loader@Engine@@ new this lane, slot 1 ?OnCompleted@Thread@System@@ ported earlier) | WAVE27_LANE_LOOPS.md §4 | HARMFUL while the DLL is present -- this table has exactly ONE stamper, `??0Loader@Engine@@QEAA@XZ` (0x1800dc0f0, stamp at +0x21), and it is RETAIL AND UNPORTED. Every other table this lane promoted has OUR ctor as its stamper, which is why promoting them is safe; this one does not. The single instance is the static `?_loader@LoaderLoop@Engine@@1VLoader@2@A`, constructed by the DLL's own CRT static-init before any of our code runs, so promoting the symbol would publish OUR table to name-resolving readers while the live object keeps the DLL's -- two tables, one object, and the loader thread is what dispatches through it. *** CORRECTED BEFORE THIS LANE CLOSED: THE REAL STAMPER IS NOT `??0Loader@`. *** The exported `??0Loader@Engine@@QEAA@XZ` (0x1800dc0f0) is NEVER CALLED: 0 direct E8/E9 sites AND its address occurs as a qword in NO PE section (so it is in no vtable, jump table or function-pointer global, which rules out an indirect call too; that absence screen was validated by handing it two known positives, ?Run@Loader@ and the UpdateLoop deleting dtor, each found at exactly its vtable slot 0). The code that actually builds `_loader` is an UNEXPORTED STATIC-INIT THUNK at 0x180148fe0 with the ctor INLINED: `mov $0x40000,%edx` / `lea rcx,[0x1801fe7b8]` / `call 0x1800ca4e0` (Thread::Thread, 256 KB stack), then the vtable stamp at 0x180148fff, then `movq $0x0,[0x1801fe7e8]` (_loop = nullptr, at _loader+0x30), then it registers the atexit dtor. IT RUNS AT DLL LOAD, BEFORE ANY OF OUR CODE EXISTS. *** CONSEQUENCE: THERE IS NO SYMBOL TO PORT THAT RETIRES THIS ROW. *** An expiry naming `??0Loader@` would be satisfiable and would change nothing -- the same defect this lane documented on ?_loader@LoaderLoop@ below, which this tag very nearly reproduced. [SCOPE: DLL-PRESENT. This is a ROUTE-SCOPED refusal, not a permanent one (reference_unpromotable_is_route_scoped_not_permanent.md): while the DLL is present NO port can retire it, because the constructor is DLL-load-time and unexported. At CUTOVER the thunk does not exist, we construct `_loader` ourselves, and the row becomes REQUIRED.] [ADDRESSEE: the lane that runs the next CUTOVER attempt -- not a porting lane.] [CONDITION: do NOT wait for a symbol. Promote this row as part of the cutover changeover, at the same moment our own code takes over constructing `_loader`.] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER

    // ========================================================================
    // ========================================================================
    //
    // SCOPE. This lane owned the IO / Video / System half of the promotion
    // backlog: 19 COMPLETE rows. FIFTEEN of them are not promoted and the
    // reason is a single structural fact stated once here rather than fifteen
    // times: see the "COMPLETE(abstract-base)" block in the NOT-PROMOTED list
    // below, and docs/runtime_port/WAVE26_LANE_PROMOIO.md §3.
    //
    // THE FOUR BELOW ARE THE ONLY ROWS IN THIS LANE'S SCOPE WHOSE TABLE IS THE
    // FINAL TABLE OF A LIVE OBJECT. Each is COMPLETE (every non-purecall slot
    // registered; PromoteVtableGuarded re-checks that at RUNTIME against the
    // live registry, so a wave kill switch cannot turn one of these into a
    // table of abort traps behind our back).
    //
    // THE THREE SCREENS A PROMOTION MUST PASS, all run this lane against all
    // four rows. They are the three hazards this file has already paid for:
    //
    //   (a) FIXED-BOUND SLOT COPY — the ??_7Geometry@Graphics@ shape, where a
    //       consumer memcpy's slots out of the stamped table with a hard-coded
    //       count. Grepped runtime/ code/ standalone/ engine/ dx11/ for a
    //       memcpy of any IO stream table: ZERO. No consumer copies slots out
    //       of any of these four.
    //
    //   (b) IDENTITY-TOKEN CONSUMER — the standalone/scene_watch.cpp shape,
    //       where something compares an object's vptr against the symbol.
    //       THIS SCREEN FOUND A LIVE DEFECT, and not in these four rows:
    //       runtime/src/io/file_stream_probe.cpp builds its subjects with
    //       ResolveExportRaw (RETAIL's ctor, retail's absolute-LEA stamp) and
    //       then compared the result against RT_DATA_SLOT, which consults the
    //       PORT REGISTRY FIRST. Two of its three vptr checks have therefore
    //       been structurally FALSE since `??_7VisitorStream@IO@@6B@` (Wave 19
    //       lane IOBASE) and `??_7BinaryFileStream@IO@@6B@` (Wave 20 lane
    //       PROMOTE) were promoted — and they do not read as "a promotion
    //       happened", they read as a LAYOUT defect, which is the one thing
    //       that probe exists to detect. It is opt-in
    //       (BANISHED_RT_IOSTREAM_PROBE) so nothing ran it. Fixed in the same
    //       commit as this block: those three checks now ask the DLL for the
    //       address, which is what the object in front of them was stamped
    //       with, and the claim they actually make (+0x00 and +0x68 are two
    //       separately stamped tables) is preserved exactly.
    //
    //   (c) A PORTED CTOR WHOSE ORACLE BYTE-COMPARES THE VPTR — the "Construct
    //       divergence is the vtable identity" shape. For three of these four
    //       the oracle is runtime/src/io/compressedstream.cpp and it is
    //       ALREADY IMMUNE: `TagVptrIdentity` normalises +0x00 on both sides,
    //       and the comment at compressedstream.cpp:1096 predicted this exact
    //       promotion in as many words. The fourth (CompressedFileStream) has
    //       no ported ctor at all — `??0CompressedFileStream@IO@@QEAA@XZ` is
    //       retail — so there is no oracle to redden.
    //
    // STAMPER OWNERSHIP, re-derived this lane with tools/mint_stamper_census.py
    // (decoder control 3 of 3, ownership pos+neg green). READ THE NEXT
    // PARAGRAPH BEFORE READING THESE NUMBERS:
    //
    //   ??_7FileStream@IO@@6B@              17 stampers,  6 ours, 11 retail
    //   ??_7CompressedStream@IO@@6B@         9 stampers,  5 ours,  4 retail
    //   ??_7CompressedMemoryStream@IO@@6B@   7 stampers,  5 ours,  2 retail
    //   ??_7CompressedFileStream@IO@@6B@     8 stampers,  1 ours,  7 retail
    //
    // NONE OF THESE IS "FULLY OWNED", AND THAT IS NOT THE GATE. The gate is
    // whether OUR OWN ported bodies reach the table through a DOOR, because
    // only those objects can carry our table; retail's in-DLL stampers keep
    // retail's, and both tables are the same nine (or eight) methods. A mixed
    // population is a coverage limit, not a hazard — the position lane VTGFX
    // took for MaterialInstance, restated rather than re-argued.
    //
    // THE DOORS ARE NAMED, not asserted. Each is an RT_DATA_SLOT /
    // ResolveExport of the mangled `??_7` string, so each picks up whatever
    // this file publishes:
    //   CompressedFileStream   runtime/src/object/external_load.cpp:278, 458
    //   CompressedStream       runtime/src/io/compressedstream.cpp:923
    //                          (VtblCompressedStream(), used by the ported ctor)
    //   CompressedMemoryStream runtime/src/io/compressedstream.cpp:162,
    //                          runtime/src/io/package_file_read_win.cpp:288,
    //                          runtime/src/save/saveload_meta.cpp:409, 602
    //   FileStream             runtime/src/io/file_stream.cpp:252, 437, 1250,
    //                          runtime/src/object/external_savetext_win.cpp:366
    //
    // THE WARRANTS ARE NOT EQUAL, and the difference is stated rather than
    // averaged away:
    //
    //   CompressedFileStream — THE STRONGEST. `Object::External::Load` (ours,
    //     ported, ~the whole object-load path) builds a CompressedFileStream as
    //     a MOST-DERIVED, LIVE object, stamps this table through the door at
    //     external_load.cpp:278, hands `&cfs.base.base` to a Serializer, and
    //     the BinaryReader/Encoding on the other side then DISPATCHES Read /
    //     GetCurrentPos / GetSize / SetCurrentPos VIRTUALLY through exactly
    //     this table. That is a dispatch route, not a door count.
    //
    //   CompressedMemoryStream — same shape, different route: our ported
    //     `??0CompressedMemoryStream@IO@@QEAA@XZ` stamps it, and
    //     SaveLoad::SaveMetaData / LoadMetaData / PackageFile::ReadPackfile
    //     (all ported) are the constructors' callers.
    //
    //   CompressedStream — our ported ctor stamps it; whether a MOST-DERIVED
    //     CompressedStream (rather than one of its two subclasses) is ever
    //     built on a live route is NOT MEASURED by this lane.
    //
    //   FileStream — DELIBERATELY THE WEAKEST WARRANT IN THIS BLOCK, and it is
    //     the ??_7BinaryWriter@ precedent above: TRAP=0 plus a door argument,
    //     with NO dispatch observation. Every stamp of this table our tree
    //     makes is a TRANSIENT base-class stamp that the derived constructor
    //     overwrites before anything can dispatch, or a destructor-chain stamp
    //     on a dying object. `??0FileStream@IO@@QEAA@XZ` — the only ctor that
    //     could leave this table FINAL in an object — is RETAIL and unported.
    //     So the honest claim is: **this promotion probably changes nothing**,
    //     and it is made anyway because TRAP=0 means no slot can abort, and
    //     leaving a row unpromoted does not verify it, it only keeps the slots
    //     unreachable. If a bare IO::FileStream is ever constructed through our
    //     door, it gets eight ported slots instead of retail's.
    //
    // ATTENDANCE — MEASURED, BY LANE ATTEND, NOT BY THIS LANE. This lane never
    // held the game-dir lock (ATTEND held it throughout), so the figures below
    // are ATTEND's run and are attributed to it rather than restated as ours.
    // Run: tip wave26/base 7de42dd1, exe md5 45c72978bfbc80f17935d9701fe58977,
    // BANISHED_W17_ATTEND=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
    // BANISHED_PAUSEMENU_QUIT=120, clean exit 0, 0-byte crash log. CONTROL
    // GREEN: ?_singleton@MainLoop@Engine@@ init=1 post=2, so the zeros in that
    // table are findings and not a dead instrument. Full table:
    // docs/runtime_port/wave26_attend/run1_w17attend.txt (branch wave26/attend).
    //
    //   ??_7FileStream@IO@@6B@              ATTENDED  post=2
    //   ??_7CompressedStream@IO@@6B@        ATTENDED  post=2
    //   ??_7CompressedMemoryStream@IO@@6B@  ATTENDED  post=2
    //   ??_7CompressedFileStream@IO@@6B@    ATTENDED  post=2
    //
    // All four doors open on an ordinary autostart route. `post` counts DOOR
    // OPENS, not dispatches, and post==2 is NOT "ran twice" — every consumer
    // caches in a function-local static. So this proves a consumer of each
    // symbol RAN; it does not prove a slot body was dispatched. That is a
    // separate tier and it is NOT claimed here.
    //
    // NOTE THIS CHANGES THE FileStream WARRANT'S STATUS, NOT ITS CONTENT: the
    // door demonstrably opens, which was never in doubt — what remains
    // unmeasured is whether any object keeps this table as its FINAL one.
    //
    // AND ONE CONSEQUENCE NOBODY PLANNED FOR, also ATTEND's measurement
    // (WAVE26_LANE_ATTEND.md §4). In ATTEND's cutover arms,
    // `??_7FileStream@IO@@6B@` and `??_7CompressedFileStream@IO@@6B@` resolve
    // NULL and KILL THE BOOT inside Game::LoadInputNameTables ->
    // ExternalFactory::FindOrLoad -> External::Load — but only when
    // BANISHED_RT_MINT=1 is absent, because they are minted-but-unpromoted rows
    // that nothing publishes except that blanket switch, and integrator item C3
    // says the blanket switch is not what cutover should use. **Promoting them
    // publishes both unconditionally**, which removes that particular null on
    // the cutover path. This lane did not set out to move the cutover wall and
    // does not claim to have moved it — attempt 27 is the only thing that can
    // say — but the two symbols ATTEND named are two of the four promoted here.
    // [ADDRESSEE: whoever runs cutover attempt 27] [CONDITION: if those two
    // symbols no longer appear as NULL, this block is why; if the boot dies
    // somewhere new, that is the expected shape of progress, not a regression.]
    //
    // A/B CONTROL: one env var per row, never the file-wide switch, so a
    // comparison has ONE variable.
    //
    // FOUR BLOCKS RATHER THAN A TABLE AND A LOOP, for the reason this file's
    // own header gives twice: a loop would pass `row.symbol`, and
    // tools/runtime_census.py reads a LITERAL argument. A promotion registered
    // through a variable is live in the process and invisible to the census —
    // reference_census_cannot_see_nonliteral_registrations.md, which this file
    // already cites and which a tidy loop here would have reproduced.
    // PromoteVtableGuarded re-checks TRAP==0 against the LIVE registry and
    // refuses loudly rather than publishing abort traps.
    PromoteVtableGuarded("??_7CompressedFileStream@IO@@6B@",   0x1801555f0ull, &n);
    PromoteVtableGuarded("??_7CompressedMemoryStream@IO@@6B@", 0x180155560ull, &n);
    PromoteVtableGuarded("??_7CompressedStream@IO@@6B@",       0x180155640ull, &n);
    PromoteVtableGuarded("??_7FileStream@IO@@6B@",             0x180154960ull, &n);

    // ========================================================================
    // RT_UNPROMOTABLE — rows that are PORTED IN FULL and can NEVER be promoted
    // ========================================================================
    //
    // These are NOT unfinished work, and they are not the same thing as the
    // "NOT PROMOTED, and why" list below — that list is rows a future lane
    // could still move. A row appears HERE only when a lane has PROVED that no
    // promotion could change behaviour on any route that could ever exist.
    //
    // This block is the SOURCE OF TRUTH for tools/runtime_census.py's
    // UNPROMOTABLE tag. The script parses the `RT_UNPROMOTABLE:` lines below by
    // literal text. It changes no count in the census: it tags the row and
    // derives a per-namespace ceiling. A tag is a WRITTEN VERDICT from a named
    // lane doc, never a heuristic, and a row loses it the moment the stated
    // expiry condition is met.
    //
    // FORMAT (one per line, parsed):
    //   RT_UNPROMOTABLE: <symbol> | <slots ported> | <doc §> | <expiry> [| <class>]
    //
    // DECLARING THE DENOMINATOR CONVENTION — REQUIRED ON EVERY NEW TAG.
    // (WAVE20_CLOSED §6.1; ENFORCED since Wave 21 by
    // tools/unpromotable_slot_check.py, which is a NEW CHECK, not a switch
    // that was turned on.) `<slots ported>` of the form N/M is ambiguous: two
    // conventions are in use here and they disagree by the purecall count K —
    // see the WAVE 20 LANE DOORS note further down for the history and why
    // the existing tags were NOT rewritten. A tag written from now on must
    // carry exactly one of these tokens anywhere in the `<slots ported>`
    // field:
    //
    //   [CONV: M=TABLE]        M is the WHOLE table
    //   [CONV: M=NONPURECALL]  M counts only non-purecall slots; table = M + K
    //
    //   e.g. (the marker is spelled RT_UNPROMOTABLE followed by a colon; it is
    //   written apart here so this EXAMPLE cannot be parsed as a real tag by
    //   this or any future reader):
    //      <marker> ??_7Foo@Bar@@6B@ | 3/9 (2 purecall) [CONV: M=NONPURECALL]
    //               | WAVE21_LANE_X.md §2 | NONE, EVER
    //
    // Declaring is not paperwork: the checker verifies a DECLARED tag under
    // that reading ONLY, instead of accepting either, so a declaration is what
    // retires the accept-either blind spot for that row.
    //
    // The twenty tags that predate this rule are exempt and are FROZEN in the
    // checker as (symbol, exact field text) pairs. Editing one of those fields
    // — which §6.1 permits when the owning lane re-measures — makes that tag
    // require a declaration, by design.
    //
    // DECLARING THE ROUTE SCOPE AND AN ADDRESSEE — REQUIRED ON EVERY NEW TAG.
    // (Wave 21; same enforcement, CHECK 3.) The <expiry> field must carry
    // both of:
    //
    //   [SCOPE: PERMANENT]    true on EVERY route that could ever exist,
    //                         cutover included
    //   [SCOPE: DLL-PRESENT]  true only while Runtime-x64.dll is loaded; at
    //                         cutover the verdict CHANGES
    //   [SCOPE: CONDITIONAL]  lapses on the stated condition
    //   [ADDRESSEE: <who or what will evaluate this>]
    //
    // WHY BOTH FIELDS EXIST, each paid for once:
    //   SCOPE     — Wave 21 lane SHOT proved ?_singleton@FileSystem@IO@@'s
    //               "NONE, EVER" is really DLL-PRESENT: the harm is real while
    //               the DLL is loaded (89 reads across 44 functions, 26 still
    //               retail, against 2 writes, both ours) and FALSE at cutover,
    //               where the row is REQUIRED. ??_7Stream@IO@@6B@ is spelled
    //               identically and IS permanent. Of the forty existing
    //               expiries, 36 say "NONE, EVER" — the phrase had become the
    //               default spelling, which is what a required choice stops.
    //   ADDRESSEE — an expiry naming a CONDITION but nobody to evaluate it
    //               never fires (WAVE20_CLOSED §5.7). ??_7ReaderWriter@'s `?`
    //               waited a full wave for "the next lane that counts ported
    //               slots on this row"; naming an addressee in the Wave 21
    //               kickoff closed it in one lane, and the work was small.
    //
    // WHAT THE CHECK DOES NOT DO, so a green is not over-read: it enforces
    // PRESENCE and a closed vocabulary, nothing more. It cannot tell a correct
    // scope from a wrong one, a false condition from a true one, or a
    // RELEVANT condition from an irrelevant one — Wave 21 lane ENC found a
    // TextFileStream expiry that was both false AND beside the point, since
    // the deleting-dtor thunk pattern would not have required the callee to be
    // ported at all. All four tag defects found in Wave 21 sat in tags that
    // were perfectly well formed. THE FIELDS MAKE THE AUTHOR CHOOSE; they do
    // not make anyone right.
    //
    // The forty tags that predate this rule are frozen the same way and do not
    // fail the check. They declare NEITHER field; that is a backlog, printed
    // as a counted NOTE by the tool, not a clean bill.
    //
    // <class> is OPTIONAL and defaults to NO-OP, so every line written before
    // Wave 12 keeps its exact meaning. There are TWO reasons a row cannot be
    // promoted and they are near-opposites, so they must not share one word:
    //
    //   NO-OP    Ported in full, and a lane proved no route could EVER dispatch
    //            through it. Promoting would change nothing. "Done."
    //   HARMFUL  Promoting would change behaviour FOR THE WORSE. NOT done --
    //            deliberately left on the DLL, and a future lane that promotes
    //            it would introduce a defect. The expiry for these is normally
    //            NONE, EVER, and the note must say what breaks.
    //
    // RT_UNPROMOTABLE: ??_7DialogController@UI@@6B@ | 11/11 | WAVE10E_LANE_FINAL.md §2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- verdict UNCHANGED.] NO-OP here is a property of C++ (the base table is stamped and then overwritten unread), so it survives cutover intact and this row is NOT re-scoped. But NO-OP is not the same as NOT-PUBLISHING: 1 file(s) reach this symbol BY NAME, and with no DLL loaded ResolveExport returns NULL for an unpublished row -- so the row is PUBLISH-AT-CUTOVER while its verdict stands. [SCOPE: PERMANENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted]
    // RT_UNPROMOTABLE: ??_7Dialog@UI@@6B@ | 9/9 | WAVE10E_LANE_FINAL.md §3 + WAVE11 lane OBJINT (runtime/src/ui/dialog_factory.cpp banner) + WAVE23_LANE_TAIL.md §2 | NOT the factory alone -- ALL THREE of (a) a factory body REGISTERED UNGATED, (b) a creator holding a BaseTypeInfo whose factoryFn is OUR pointer, and (c) that minted node present in the AVL Factory::Startup walks; (c) is blocked by the ?_firstTypeInfo split brain [SCOPE: PERMANENT] (lane TAIL wrote 'DLL-PRESENT and cutover alike'; PERMANENT is the canonical token for that meaning) [ADDRESSEE: the Wave 24 kickoff writer -- do not re-scope a lane onto this row until the _firstTypeInfo split brain is closed] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Application@1@0PEAV11@EA | n/a (scalar ptr) | WAVE12_APPLICATION.md §ceiling + WAVE23_LANE_TAIL.md §5 | NONE WHILE THE DLL IS PRESENT -- **INVERTS AT CUTOVER**, where the row is REQUIRED [SCOPE: DLL-PRESENT] (lane TAIL wrote 'the HARMFUL verdict is DLL-PRESENT ONLY'; same meaning, canonical token) [ADDRESSEE: the lane that first boots without Runtime-x64.dll -- this row must be minted then, and the 'EXPIRY: none' prose below was written before route scoping existed] | HARMFUL (DLL-present) / REQUIRED (cutover)
    //
    // ---- WAVE 23 LANE TAIL: the four Audio/Input singletons -------------
    //
    // These four carried NO TAG AT ALL before this wave, which is how each
    // successive lane rediscovered them from scratch. Each was decided on ITS
    // OWN xrefs and NOT by analogy with its sibling one namespace over -- the
    // rule Wave 22 lane ERRCFG paid for by refuting the general split-brain
    // story for 3 of its own 5 rows.
    //
    // METHOD, identical for all four and stated so the numbers can be
    // re-derived: Ghidra get_xrefs_to on the slot, PLUS a decoder-independent
    // scan of every disp32 window in .text whose RIP-relative target is the
    // slot (imm widths 0/1/2/4, because `MOV qword ptr [rip+d],0` measures RIP
    // past the imm32 -- the encoding that made the same scan under-report in
    // reg_mint_promoted's SaveLoad section). THE TWO ROUTES AGREE EXACTLY on
    // all four counts, and on Application's 26 as well. Readers were then
    // classified by grepping RegisterRuntimeExport in runtime/ -- NOT by the
    // census CSV, which is keyed on `called` and therefore reported SEVEN
    // registered-but-uncalled bodies as absent (PauseEffect, StopEffect,
    // OnFocusMessage, OnLostFocusMessage, OnMouseWheelMessage, WindowProc and
    // Get@Application). That blind spot is recorded because it inflates the
    // unported-reader count in the HARMFUL direction, i.e. towards the
    // conclusion reached anyway -- it had to be removed before the conclusion
    // could be trusted.
    //
    // EVERY ONE STILL HAS UNPORTED IN-DLL READERS after that correction, so
    // every one is HARMFUL to promote today; and every one INVERTS at cutover,
    // where those readers do not exist and nothing else supplies the storage.
    //
    // RT_UNPROMOTABLE: ?_singleton@Configuration@Audio@@0PEAV12@EA | n/a (scalar ptr) | WAVE23_LANE_TAIL.md §1 | 9 refs = 7 W + 2 R, and BOTH readers are unported (?Get@Configuration@Audio@@ has no body at all; ?Update@Scene@Audio@@ is held back this wave and runs every frame) [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that first boots without Runtime-x64.dll] | HARMFUL (DLL-present) / REQUIRED (cutover)
    // RT_UNPROMOTABLE: ?_singleton@Device@Audio@@0PEAV12@EA | n/a (scalar ptr) | WAVE23_LANE_TAIL.md §1 | 8 refs = 1 W + 7 R; 3 readers ported (PauseEffect/StopEffect/StopAll), 4 NOT (??1SoundBuffer, ?Run@Stream, ?Get@Device@Audio, ?Update@Scene@Audio) [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that first boots without Runtime-x64.dll] | HARMFUL (DLL-present) / REQUIRED (cutover)
    // RT_UNPROMOTABLE: ?_singleton@Configuration@Input@@0PEAV12@EA | n/a (scalar ptr) | WAVE23_LANE_TAIL.md §3 | 24 refs = 7 W + 16 R over 13 distinct readers; only 4 ported, and THREE of the unported readers are UNEXPORTED DLL INTERNALS (FUN_18000db80 / FUN_18000dd60 / FUN_1800a1dc0) which have no door and can never be ported by name [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that first boots without Runtime-x64.dll] | HARMFUL (DLL-present) / REQUIRED (cutover)
    // RT_UNPROMOTABLE: ?_singleton@Device@Input@@0PEAV12@EA | n/a (scalar ptr) | WAVE23_LANE_TAIL.md §3 | 17 refs = 1 W + 16 R over 14 distinct readers; 4 ported, 7 not (incl. ?Released@KeyMap and ?GetNormalizedValue@KeyMap, whose SIBLINGS ?Pressed@ and ?GetValue@ ARE ported -- a half-ported reader set, which is why this row cannot be decided by looking at the class) [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that first boots without Runtime-x64.dll] | HARMFUL (DLL-present) / REQUIRED (cutover)
    // RT_UNPROMOTABLE: ??_7Base@Object@@6B@ | 2/2 (1 purecall) [CONV: M=NONPURECALL] [RE-DERIVED WAVE23 LANE OBJ: 3-slot table, adjacent-export UPPER bound, corroborated by layout congruence with the hard-bounded External/CompiledExternal rows] | WAVE11_LANE_OBJDATA.md §2 + WAVE23_LANE_OBJ.md §1 | NO-OP is a property of C++ and survives cutover, but NOT-PUBLISHING does not: 13 files in this tree reach this symbol by NAME and ResolveExport returns NULL for an unpublished row with no DLL loaded [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    // RT_UNPROMOTABLE: ??_7External@Object@@6B@ | 4/4 (4 purecall) [CONV: M=NONPURECALL] [RE-DERIVED WAVE23 LANE OBJ: 8-slot table, HARD bound - slot 8 is the ASCII tail of "External", not a pointer] | WAVE11_LANE_OBJDATA.md §2 + WAVE23_LANE_OBJ.md §1 | as ??_7Base: NO-OP on every route, 13 by-name doors resolve NULL at cutover unless published [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    // RT_UNPROMOTABLE: ??_7CompiledExternal@Object@@6B@ | 4/4 (4 purecall) [CONV: M=NONPURECALL] [RE-DERIVED WAVE23 LANE OBJ: 8-slot table, HARD bound - the qword after slot 7 is string data] | WAVE11_LANE_OBJDATA.md §2 + WAVE23_LANE_OBJ.md §1 | NO-OP on every route; ONE by-name door (runtime/src/video/buffers.cpp:445) resolves NULL at cutover unless published [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    // RT_UNPROMOTABLE: ?_singleton@Factory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 + WAVE23_LANE_OBJ.md §2 | HARMFUL while the DLL is loaded - 18 in-DLL reads against 2 in-DLL writes (re-derived, both writers are the ctor we have ported); at cutover every one of those 20 sites is GONE and our ctor is the only writer, so the row is then REQUIRED, not harmful [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@ExternalFactory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 + WAVE23_LANE_OBJ.md §2 | HARMFUL while the DLL is loaded - 149 in-DLL reads against 2 in-DLL writes (re-derived); at cutover all 151 sites are gone and the row is REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@InternalFactory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 + WAVE23_LANE_OBJ.md §2 | HARMFUL while the DLL is loaded - 5 in-DLL reads against 1 in-DLL write, and the harm is the WRITER not the readers (our ported ctor never runs on a DLL-present boot, so a promoted slot stays zero); at cutover our ctor is the only ctor and the row is REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff] | HARMFUL
    // RT_UNPROMOTABLE: ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA | n/a (chain head) | WAVE11_LANE_OBJDATA.md §4 + WAVE23_LANE_OBJ.md §2 | HARMFUL while the DLL is loaded - 106 in-DLL reads and 105 in-DLL writes (re-derived; ~104 of the pairs are the self-linking static initialiser of one type each), so promoting splits ONE chain into two; at cutover none of those initialisers exists, there is only our chain, and the row is REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff] | HARMFUL
    // RT_UNPROMOTABLE: ??_7Internal@Object@@6B@ | 4/4 (0 purecall) [CONV: M=NONPURECALL] [RE-DERIVED WAVE23 LANE OBJ: 4-slot table, adjacent-export UPPER bound] | WAVE11F_LANE_TINFO.md §2 + WAVE23_LANE_OBJ.md §1 | NONE while the DLL is present - and at cutover the row is REQUIRED, which runtime/src/object/internal.cpp:256-258 already states in as many words (VtblInternal falls back to ResolveExport, which is NULL unless this row is published) [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    // RT_UNPROMOTABLE: ?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A | n/a (BaseTypeInfo) | WAVE11F_LANE_TINFO.md §3 + WAVE23_LANE_OBJ.md §2 | NONE while the DLL is present - re-derived: 2 in-DLL readers, BOTH of them the two-instruction typeId accessors in the row's own vtable, plus 1 LEA in the static initialiser; no third consumer exists [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    // RT_UNPROMOTABLE: ?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A | n/a (BaseTypeInfo) | WAVE11F_LANE_TINFO.md §3 + WAVE23_LANE_OBJ.md §2 | NONE while the DLL is present - re-derived: 5 in-DLL readers, and only TWO are the vtable accessors TINFO reasoned about; 0x18002fb35, 0x1800bfb74 and 0x1800bff92 load this node inside ordinary in-DLL bodies. NO-OP still holds because the two nodes are content-identical after Factory::Startup, NOT because nothing reads it [SCOPE: DLL-PRESENT] [ADDRESSEE: the Wave 23 integrator, who carries the ten-row Object cutover-publish list from WAVE23_LANE_OBJ.md §6 into the Wave 24 kickoff]
    //
    // ---- WAVE 17 LANE CEIL: Video / IO / Graphics ------------------------
    //
    // The twelve Video companion-base vtables. ONE mechanism, measured the
    // same way for all twelve, and it is the `??_7DialogController@UI@@6B@`
    // shape: the stamp is overwritten, or freed, UNREAD.
    //
    // RT_UNPROMOTABLE: ??_7ConstantBufferData@Video@@6B@ | 1/1 (3 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7ContextData@Video@@6B@ | 1/1 (19 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7DeviceData@Video@@6B@ | 1/1 (19 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7ImageBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7IndexBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7PixelProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7RenderBufferData@Video@@6B@ | 1/1 (3 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7RenderStateData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7VertexBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7VertexLayoutData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7VertexProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    // RT_UNPROMOTABLE: ??_7VideoProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER [SCOPE: PERMANENT] [ADDRESSEE: nobody need act -- Wave 23 lane VIDVT re-measured all 12 from the shipped bytes (WAVE23_LANE_VIDVT.md S2); a lane that adds a virtual call on a companion BASE between the two stamps must re-open it]
    //
    // The thirteen mutable-storage rows. NOT no-ops: promoting any of them
    // would split the brain exactly as `?_singleton@Application@1@` would.
    //
    // RT_UNPROMOTABLE: ?_singleton@Configuration@Video@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Device@Video@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_firstSurface@ImageBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE while the DLL is present -- at cutover this row is REQUIRED, because the storage it names ceases to exist and the mint is the only thing that owns it (runtime/src/video/buffers.cpp's "at cutover that inverts" and runtime/src/video/dynamic_resources.cpp:144 both already say so) [SCOPE: DLL-PRESENT] [ADDRESSEE: the cutover lane that stops loading Runtime-x64.dll; re-derive the reference table with tools/video_data_refs.py] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@FileSystem@IO@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 25 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Modification@IO@@2PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 6 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Persistence@IO@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 13 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    //
    // ---- WAVE 19 PHASE 0a LANE NOOP: the two IO abstract bases -------------
    //
    // CEIL declined to tag these two because the claim rested on a COMMENT in
    // encoding.cpp. Lane NOOP settled it from the shipped bytes: both tables
    // are abstract (25 purecall of 26 [was written 30 of 36 — see the note
    // below the tags], and 5 of 7, read out of .rdata), every
    // reference to either VA in the whole DLL is an in-DLL RIP-relative LEA
    // (20 and 56, agreed by a decoder route AND a decoder-independent disp32
    // scan), no shipped image imports either symbol or holds its VA as a
    // value, and NOTHING dispatches while either table is installed: 17 and 58
    // stamp-windows contain zero calls through the stamped object. Our own
    // eight door-stamps of these two symbols all mirror retail's
    // stamp-then-overwrite or stamp-then-free.
    //
    // RT_UNPROMOTABLE: ??_7ReaderWriter@IO@@6B@ | 1/26 ported [RE-DERIVED, WAVE21_LANE_ENC.md item 12: the `?` numerator resolved by lane ENC, the assigned addressee -- slot 0 (0x180023bd0, the deleting dtor) is now runtime/src/object/deleting_dtor.cpp row 42, verified against tools/rt_deleting_dtor_verify.py], 25 purecall [RE-DERIVED] [CONV: M=TABLE] | WAVE19_PHASE0A_NOOP.md §3.1 + WAVE19_INTEGRATOR_CORRECTIONS.md C30 + WAVE21_LANE_ENC.md item 12 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- verdict UNCHANGED.] NO-OP here is a property of C++ (the base table is stamped and then overwritten unread), so it survives cutover intact and this row is NOT re-scoped. But NO-OP is not the same as NOT-PUBLISHING: 4 file(s) reach this symbol BY NAME, and with no DLL loaded ResolveExport returns NULL for an unpublished row -- so the row is PUBLISH-AT-CUTOVER while its verdict stands. [SCOPE: PERMANENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted]
    // RT_UNPROMOTABLE: ??_7Stream@IO@@6B@ | 2/7 ported [RE-DERIVED, WAVE23_LANE_IOPORT.md §2: the `1` was tagged [INHERITED] and was STALE BY ONE. tools/vtable_dump.py 0x1801549a0 against retail reads 5 purecall and TWO concrete slots -- slot 0 0x18000c610 (deleting-dtor thunk, runtime/src/object/deleting_dtor.cpp, registered by address) and slot 1 0x18000c6a0 ?GetSource@Stream@IO@@ (runtime/src/io/iobase_vtables.cpp:522, registered by address). BOTH are ours, so the non-purecall set is CLOSED and runtime_census.py prints `2 2 COMPLETE` under its M=NONPURECALL convention], 5 purecall [RE-DERIVED] [CONV: M=TABLE] | WAVE19_PHASE0A_NOOP.md §3.2 + WAVE23_LANE_IOPORT.md §2 | NONE while the DLL is present [SCOPE: DLL-PRESENT] [ADDRESSEE: Wave 23 lane IOPORT re-derived the STAMPER census on this tip -- 50 stampers, 22 ours-capable, 25 RETAIL, 3 unattributed -- so the block is stamper ownership, NOT slots. Re-open only when a lane drives retail's 25 to 0; until then this row is COMPLETE-and-unpromotable, which is a different state from incomplete]
    //
    // WAVE 20 LANE DOORS — the ReaderWriter denominator, and a convention split
    // these tags never declared.
    //
    // `2/36 ported, 30 purecall` was honestly marked [INHERITED] and had gone
    // stale: WAVE19_INTEGRATOR_CORRECTIONS C30 re-derived the row as **26
    // slots**, not 36 — TWO unexported tables sit between it and the next
    // export, one of them stamped by a three-instruction leaf ctor with no
    // .pdata entry — and lane EXTENT re-derived 26 again this wave
    // (docs/runtime_port/wave20_extent_audit.txt, CORROBORATED-EXACT). The
    // slot array in this tree agrees without being asked: `kVSlots46` in
    // standalone/rt_data_mint_tables.inc is 26 entries, 25 of them
    // RT_MINT_PURECALL. So the denominator is 26 and the purecall count is 25.
    //
    // THE NUMERATOR IS NOT FIXED, AND IS WRITTEN `?` RATHER THAN GUESSED. The
    // `2` came from WAVE17_LANE_CEIL §3.1's `36 / 2 / 4 / 30` row, counted over
    // the 36-slot extent — so its two ported slots may have been inside the ten
    // slots that turned out to belong to other tables, and nothing cheap
    // decides which. EXPIRY: the next lane that counts ported slots on this row
    // writes the number; until then `?` is the honest value and it is not a
    // count anything reads (tools/runtime_census.py parses this field as free
    // text and derives no number from it).
    //
    // AND A SPLIT NOBODY DECLARED: these tags use TWO conventions for the
    // denominator. The Object/Video rows above are `N/M (K purecall)` where
    // M is the NON-purecall slots and M+K is the table (Base 2/2 (1) = 3
    // slots, ContextData 1/1 (19) = 20); the two IO rows here are `N/M ...,
    // K purecall` where M is the WHOLE table (Stream 1/7, 5 purecall = 7
    // slots). Both readings are self-consistent and they disagree by K, so a
    // reader who assumes the wrong one mis-sizes the row. Declared here rather
    // than normalised, because rewriting eighteen lanes' numbers to one
    // convention is a change to eighteen written verdicts, not a tidy-up.
    // `tools/unpromotable_slot_check.py` checks every tag under BOTH readings
    // and reports a row only when NEITHER fits — which is how the 36 was found
    // and how the other nineteen rows were cleared.
    //
    // RT_UNPROMOTABLE: ?_singleton@AnimationManager@Graphics@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 2 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RT_UNPROMOTABLE: ?_frameIndex@ParticleInstance@Graphics@@1HA | n/a (int counter) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 3 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RT_UNPROMOTABLE: ?_rng@Particle@Graphics@@1VRandom@System@@A | n/a (RNG state) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 2 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RT_UNPROMOTABLE: ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA | n/a (bool flag) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER [ROUTE SCOPE ADDED, WAVE 25 lane MANIFEST -- 'NONE, EVER' predates route scoping and is preserved above as written; it is read here as 'no expiry ON THE DLL-PRESENT ROUTE'. The HARMFUL verdict itself is UNCHANGED.] At cutover the in-DLL RIP-relative readers that make promotion harmful cease to exist and the minted storage is this symbol's only owner, so the row INVERTS to REQUIRED; 4 by-name door(s) in this tree resolve NULL without it. [SCOPE: DLL-PRESENT] [ADDRESSEE: whoever runs cutover attempt 26 -- this row is carried by the GENERATED publish manifest standalone/cutover_manifest.inc; re-derive with `python tools/cutover_manifest.py --check`, which FAILS if the row is REQUIRED and unminted] | HARMFUL
    // RESOLVED by Wave 22 lane GEOM (docs/runtime_port/WAVE22_LANE_GEOM.md):
    // this row's expiry condition FIRED. code/Game/Component/statusicon_geometry.cpp:341
    // now reads `i < 9` (re-derived independently this lane by reading the raw
    // bytes of both 0x180154e60 in Runtime-x64.dll and 0x14015f0c0 in
    // Application-x64.exe via pefile, not just re-quoting WAVE21_LANE_GFXVT.md
    // §5.2 -- both extents confirmed 9), so the below is HISTORY, not a live
    // tag, and there is deliberately no `RT_UNPROMOTABLE:` line here for
    // tools/unpromotable_slot_check.py to score. The row is NOT re-promoted by
    // this commit -- lane GEOM's brief was the cutover blocker in
    // code/Game/Component/, not a decision to add a
    // RegisterRuntimeExport("??_7Geometry@Graphics@@6B@", ...) call in this
    // file, which is a separate, still-open step for whichever lane next
    // re-scores this row from scratch (its stamper census was already green
    // before this fix: 3 stampers, 2 ours-capable, 0 unattributed, 1 retail
    // copy-ctor stamper that is moot at cutover).
    //
    // WAVE 22 LANE ALLOC. The row this whole FILE has cited as its canonical
    // HARMFUL example since Wave 10 -- see the banner at the top, and line 1834
    // -- and which had no tag of its own. Both halves of its verdict are
    // OBSERVED here rather than derived, which is unusual and worth saying:
    //
    //   DLL PRESENT, promoting is HARMFUL: publishing our zeroed storage over
    //   the DLL's live `_singleton` dies in EnterCriticalSection on a null self
    //   before the UI exists. That is what makes BANISHED_RT_MINT=1 fatal and
    //   is why per-row selectivity had to be built at all.
    //
    //   AT CUTOVER, the row is REQUIRED: engine/funcs/engine_funcs.cpp:1629
    //   dereferences `ResolveAddr(Module::Runtime, 0x1801fc718)` with no guard,
    //   and the first controlled DLL-absent boot faulted there precisely
    //   because this row was unregistered (standalone/resolve_standalone.cpp
    //   records it).
    //
    // So it is the same shape as ?_singleton@FileSystem@IO@@, and independently
    // confirms Wave 22 lane TIMER's rule on this lane's own subject: unpromoted,
    // our writer (rtalloc::Construct) publishes through RT_DATA_SLOT ->
    // ResolveExport -> the DLL's real slot, so the many unported DLL internals
    // that read 0x1801fc718 as a hard-coded RVA (`MOV RCX,[0x1801fc718]`,
    // documented at runtime/src/collection/{array,linkedlist,avl_trees,
    // set_avl_e24}.cpp and runtime/src/ui/dialog_factory.cpp) stay correct BY
    // CONSTRUCTION. Promoting redirects our write to our own storage and
    // freezes theirs. For an allocator that is not a clean crash: a DLL
    // internal freeing against retail's arena a pointer our arena handed out is
    // silent corruption.
    // RT_UNPROMOTABLE: ?_singleton@MemoryAllocator@System@@0PEAV12@EA | n/a (scalar ptr) | WAVE22_LANE_ALLOC.md §5 | NONE while Runtime-x64.dll is present; REQUIRED at cutover, where the verdict INVERTS [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that next attempts a DLL-absent cutover boot -- it must register this row, and engine_funcs.cpp:1629 faults immediately if it does not] | HARMFUL
    //
    //      ⚠ THIS TAG IS ROUTE-SCOPED AND ITS EXPIRY IS **NOT** "NONE, EVER".
    //      Wave 21 lane SHOT established that the existing vocabulary spells two
    //      opposite instructions the same way: ??_7Stream@IO@@6B@ is
    //      PERMANENTLY unpromotable (a no-op on every route), while
    //      ?_singleton@FileSystem@IO@@ is harmful only WITH THE DLL PRESENT and
    //      is REQUIRED at cutover. This row is the second kind, and writing a
    //      bare "NONE, EVER" here would have inherited exactly that ambiguity.
    //
    //        WITH THE DLL PRESENT — HARMFUL, and promotion is OPTIONAL, so
    //          don't. Retail's table answers the door; promoting buys nothing
    //          and moves the overrun below onto our storage.
    //        AT CUTOVER — REQUIRED, not merely safe. scene.cpp:176 reaches this
    //          table through ResolveAddr(Module::Runtime, 0x180154e60), and
    //          standalone/resolve_standalone.cpp returns 0 for an unmapped
    //          module ("Return 0 when the module is not mapped", guarding on the
    //          handle). Unpromoted, the ported Geometry ctor stamps a NULL
    //          vtable into every Geometry and the first virtual call is an
    //          EXEC-of-0 — the identical argument mesh.cpp:295-305 already makes
    //          for its own three rows.
    //
    //      THE CONSEQUENCE, and it is the reason this scoping matters: at
    //      cutover the `i < 12` overrun below reads OUR storage no matter what
    //      we choose, because the DLL's table is not available to read instead.
    //      So statusicon_geometry.cpp:341 is NOT a tidy-up that would be nice to
    //      have before promoting this row — it is ON THE CUTOVER CRITICAL PATH.
    //
    //      AND IT IS PRICED — the cheap kind, established from the shipped bytes
    //      and NOT from the comment above that loop, which is WRONG.
    //      WAVE21_LANE_GFXVT.md §5.2a: retail's billboard vtable at 0x14015f0c0
    //      in Application-x64.exe is NINE pointers, then a NULL pad, then a
    //      DIFFERENT CLASS's table at 0x14015f110 — proved by xrefs (0x14015f0c0
    //      is stamped by StatusIconComponent_Ctor at 0x14012c0fe; 0x14015f110 by
    //      TradeWork_impl_ed50 and FUN_1400b00e0). So the loop's "12 real slots"
    //      has no basis in either binary, and slots 10 and 11 it copies are the
    //      first two slots of somebody else's vtable. Nothing can dispatch slots
    //      9-11: both classes in play are nine slots, so no slot needs SOURCING
    //      and this is not hidden port work.
    //
    //  ??_7Geometry@Graphics@@6B@ — HARMFUL, and it is the ONE Graphics vtable
    //      row that is COMPLETE, which is exactly why it needed a verdict: the
    //      next lane to read the PROMOTION BACKLOG sees `6 6 COMPLETE` and a
    //      stamper census that is 2-of-3 ours, and promotes it.
    //
    //      COMPLETE is the static half of readiness and the stamper gate is the
    //      dynamic half; this row passes BOTH and is still harmful, for a third
    //      reason neither gate asks about: A CONSUMER READS PAST THE END OF THE
    //      TABLE, and how bad that is depends on WHOSE storage it overruns.
    //
    //      The chain, every link measured on this tree at wave21/gfxvt:
    //        1. code/Game/Component/statusiconcomponent.cpp:611 calls the
    //           Geometry base ctor on `s+0x28`, then reads back the vtable it
    //           stamped as `baseV` and hands it to StatusIconGeometry_BuildVtable.
    //           The path is gated on BANISHED_LIVE_STATUSICON_DRAW, which is in
    //           the default-on table (component_factory_base.cpp:189) — DEFAULT ON.
    //        2. code/Game/Component/statusicon_geometry.cpp:341 copies
    //           `i < 12` slots out of that table into the billboard's own.
    //        3. The table is NINE slots. Read from the shipped DLL at
    //           0x180154e60 this lane: slots 0..8 are image pointers and
    //           0x180154ea8 is `49 6d 61 67 65 42 75 66` = "ImageBuf" — the
    //           start of the "ImageBuffer\0GeneratedImageBuffer\0" string run.
    //           So baseVtbl[9..11] are 0x6675426567616d49, 0x726566 and
    //           0x65746172656e6547: all non-zero, so the `baseVtbl[i]` test
    //           passes and all three are installed as function pointers.
    //           (WAVE6_LANE_D.md §6.1 found this and prescribed `i < 9`; the
    //           prescription was never applied — a prescription read as a
    //           completion, reference_a_prescription_read_as_a_completion.md.)
    //        4. Our ported Geometry ctor stamps
    //           ResolveAddr(Module::Runtime, 0x180154e60)
    //           (runtime/src/graphics/scene.cpp:176), and ResolveAddr consults
    //           the PORT REGISTRY FIRST. Promoting this row — which, following
    //           the ??_7BinaryFileStream@ precedent above, registers by name AND
    //           by address — therefore redirects that stamp to `kVTable28`, a
    //           `static void* kVTable28[9]` in standalone/rt_data_mint_tables.inc.
    //        5. Step 2 then reads 24 bytes PAST the end of that 9-element array,
    //           out of whatever our image's linker placed next, and installs it
    //           as three function pointers in a live exe-side vtable.
    //
    //      TODAY the same out-of-bounds read is dormant: the three values are
    //      fixed ASCII from the DLL's own .rdata and nothing dispatches a
    //      Geometry past slot 8 (WAVE6_LANE_D.md §6.1, measured). Promotion does
    //      not create the overrun — it moves it off known-dormant DLL bytes onto
    //      unanalysed storage of ours. That is "changes behaviour for the worse"
    //      in the sense this tag exists for, and the direction matters more than
    //      the current dormancy: reference_vtable_slot_count_from_the_name_string
    //      is precisely the case where fixing one dormant slot unleashes the rest.
    //
    //      ⚠ THE A/B WAS NOT RUN. This verdict is derived — from the shipped
    //      bytes, from the two source sites, and from ResolveAddr's documented
    //      registry-first order — and it was NOT confirmed by promoting the row
    //      and observing a failure. A lane that wants to overturn it must run
    //      that A/B, not re-read this comment.
    //
    //      EXPIRY, WITH AN ADDRESSEE (reference_expiry_clause_needs_an_addressee)
    //      -- FIRED, by the named addressee. The blocker was ONE CHARACTER:
    //      statusicon_geometry.cpp:341's `i < 12` needed to read `i < 9`, the
    //      fix WAVE6_LANE_D prescribed in Wave 6 and Wave 21 lane GFXVT
    //      re-confirmed. The addressee was THE NEXT LANE THAT OWNS
    //      code/Game/Component/ (Wave 22 lane GEOM) — explicitly not "whoever
    //      next counts slots on this row", because GFXVT could not make that
    //      edit and verify it inside a vtable-scoped brief. That line now
    //      reads `i < 9`, so this tag is void and the row is eligible to be
    //      re-scored from scratch (its stamper census was already green: 3
    //      stampers, 2 ours-capable, 0 unattributed) — but re-scoring and
    //      actually registering the promotion is a SEPARATE, still-open step
    //      lane GEOM did not take (see the RESOLVED note above this essay).
    //
    //      THE PRICE OF THIS PROMOTION, in lane SHOT's units — stampers split by
    //      owner, with the unattributed bucket kept SEPARATE rather than folded
    //      into either side: 3 stampers = 2 ours-capable / 1 retail / 0
    //      unattributed. The one retail stamper is the copy ctor
    //      ??0Geometry@Graphics@@QEAA@AEBV01@@Z at 0x180015b7e, so a
    //      copy-constructed Geometry keeps retail's table after a promotion.
    //      That is a SPLIT, not a trap — and at cutover it is moot, because
    //      there is no retail body left to run.
    //
    // ---- WAVE 21 LANE GFXBODY (row 11), routed through lane CLASS ----------
    //
    // The FIRST tag written under the Wave 21 form: it declares a route SCOPE
    // and an ADDRESSEE. GFXBODY supplied the row and the reason and correctly
    // declined to write the tag itself.
    //
    // ?AddDrawItems@MeshInstance@Graphics@@ (retail 0x180100f20, ordinal 1550)
    // is PORTED AND UNDERSTOOD (body transcribed in WAVE6_LANE_E.md) and must
    // NOT be registered, for a reason the census cannot see: our tree's only
    // use of that address is NOT A CALL. `standalone/scene_watch.cpp:401`
    // resolves it as an IDENTITY TOKEN and compares it against `vtable[3]` of a
    // live object to decide whether the object is a MeshInstance. Register the
    // address and ResolveAddr hands back OUR body, the compare fails for every
    // object, and BANISHED_LOD_CENSUS / BANISHED_FORCE_LOD0 silently observe an
    // EMPTY SCENE -- a wrong answer with no error, which is the worst shape.
    // Same mechanism as `0x1400f2f00` in PLAN §1.1. The fuller write-up is at
    // runtime/src/graphics/mesh.cpp:1822 and is not repeated here.
    //
    // SCOPE is DLL-PRESENT, not PERMANENT, and the distinction is load-bearing
    // exactly as lane SHOT showed for ?_singleton@FileSystem@IO@@: the harm is
    // that a retail vtable slot is the thing being compared against. At cutover
    // there is no retail slot, scene_watch's identity test must be rebuilt
    // whatever we decide here, and this row stops being harmful. Writing
    // "NONE, EVER" would have filed a cutover task as a permanent refusal.
    //
    // RT_UNPROMOTABLE: ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z | n/a (function, not a table) | WAVE21_LANE_GFXBODY.md row 11 + WAVE21_LANE_CLASS.md §2 | *** THIS EXPIRY HAS FIRED. WAVE 27 lane GFXMESH. *** The original clause was: registering it is harmful while the DLL supplies vtable[3]; voids when scene_watch.cpp:401 stops using this address as an identity token. scene_watch.cpp:401 NO LONGER USES IT AS A TOKEN -- IsMeshInstance now matches vtable[3] against ResolveRuntimeRaw(0x180100f20) (the registry-FREE door, what the DLL ctor stamped, unmovable by any registration) OR RtPortLookupAddr(0x180100f20) (our port, if ever registered). So the HARMFUL verdict is RETIRED and this row is no longer a refusal: registering AddDrawItems is now SAFE on the DLL-present route. It remains unregistered only because THE BODY IS NOT PORTED -- lane GFXMESH removed the blocker and did not write the ~0xd0-byte LOD-selection body, which needs Renderer::AddDrawItem's 9-argument frame. [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that PORTS ?AddDrawItems@MeshInstance@ -- it needs no scene_watch change of its own, that debt is paid; it must only re-run `grep -rn 0x180100f20 runtime/ code/ standalone/ engine/ dx11/` and confirm no NEW identity-token consumer has appeared since wave27/gfxmesh, where the only live one was scene_watch.cpp] | HARMFUL
    //
    // ==== WAVE 27 LANE GFXMESH: THE CONDITION ABOVE IS NOW FALSE. ============
    // The tag's own voiding clause -- "voids when scene_watch.cpp:401 stops
    // using this address as an identity token" -- HAS FIRED. Lane GFXMESH was
    // the named addressee, it re-verified the clause was still true before
    // acting (it was: the line still called ResolveAddr, which consults the port
    // registry first), and it then did exactly what the tag prescribed, FIRST:
    //
    //   standalone/scene_watch.cpp `IsMeshInstance` no longer asks ResolveAddr.
    //   It now matches vtable[3] against EITHER ResolveRuntimeRaw(0x180100f20) --
    //   the registry-FREE door, which is what the DLL's constructor actually
    //   stamped and which no registration can move -- OR
    //   RtPortLookupAddr(0x180100f20), our own port if one is ever registered.
    //   Two tokens, so the test is right for objects built before and after any
    //   future promotion of ??_7MeshInstance@. The full write-up is at that call
    //   site, in the SYMBOL / CONDITION / CONSEQUENCE shape.
    //
    // SO THE HARMFUL VERDICT ABOVE IS RETIRED. It is left standing rather than
    // deleted because a refusal that vanishes teaches nothing, and because its
    // reasoning is the clearest example in this file of a port being harmful for
    // a reason the census cannot see.
    //
    // WHAT IS *NOT* RETIRED, and lane GFXMESH says so rather than letting the
    // silence read as a completion: ?AddDrawItems@MeshInstance@ IS STILL NOT
    // REGISTERED, because THIS LANE DID NOT PORT THE BODY. Nothing above changes
    // that -- it removes the BLOCKER, not the work. The body is 0xd0 bytes of
    // LOD selection (a squared-distance compare against the union of
    // _worldBounds and _prevWorldBounds, walking _detailLevels at stride 0xb0)
    // followed by one ?AddDrawItem@Renderer@ call per TriangleList, and it needs
    // Renderer::AddDrawItem's 9-argument frame reproduced exactly.
    //
    // [ADDRESSEE for the remaining work: the lane that ports
    //  ?AddDrawItems@MeshInstance@Graphics@@ itself. It may now register the
    //  symbol with no scene_watch change of its own -- that debt is paid. It
    //  must still check, at that time, that no OTHER identity-token consumer of
    //  0x180100f20 has appeared: `grep -rn 0x180100f20 runtime/ code/ standalone/
    //  engine/ dx11/` returned exactly the two sites named here on wave27/base
    //  (this file and scene_watch.cpp) plus rt_data_mint_tables.inc, which
    //  STAMPS the address rather than comparing it.]
    //
    // ==== WAVE 28 LANE SLOTS: THE REMAINING WORK IS DONE. ===================
    // *** THE PARAGRAPH IMMEDIATELY ABOVE IS NOW FALSE AND MUST NOT BE READ AS
    // CURRENT. *** Lane SLOTS was the named addressee. It did what the addressee
    // clause required, in order:
    //
    //   1. RE-RAN THE GREP on the wave28 tip, before registering anything:
    //      `grep -rn 0x180100f20 runtime/ code/ standalone/ engine/ dx11/`
    //      returns this file, scene_watch.cpp (the rebuilt two-token test, which
    //      no longer goes through ResolveAddr), scene_watch.h (a comment),
    //      rt_data_mint_tables.inc (which STAMPS the address) and mesh.cpp.
    //      **NO NEW IDENTITY-TOKEN CONSUMER HAS APPEARED.**
    //   2. PORTED BOTH BODIES -- ?AddDrawItems@MeshInstance@ AND its sibling
    //      ?AddDrawItems@EdgeMeshInstance@ (0x1800a2fd0), together, because
    //      WAVE27_LANE_GFXMESH.md §7 warned that porting one without the other
    //      leaves two overrides of one slot derived from different readings of
    //      the same nine-argument call. runtime/src/graphics/mesh.cpp, Wave 28
    //      section.
    //   3. REGISTERED BOTH, by NAME and by ADDRESS, in
    //      RtGraphicsMeshVtableRegister().
    //
    // A CORRECTION TO THE SIZE CLAIM ABOVE, because it made the job look cheap:
    // "0xd0 bytes" UNDERSTATES it. The body runs 0x180100f20..0x1801011ef =
    // **0x2d0 bytes**, measured from the instruction listing (the next function,
    // ?GetSubTransform@MeshInstance@, starts at 0x180101200). 0xd0 is roughly
    // the LOD-selection PREAMBLE alone. This is the `.pdata`-understates shape
    // (reference_pdata_is_a_lower_bound_on_function_extent.md) in its
    // consequence rather than its mechanism: an understated extent makes work
    // look cheap, and this one sat in a tag for a wave.
    //
    // THE ROW IS STILL TAGGED HARMFUL ABOVE AND THAT TAG IS DEAD IN BOTH ITS
    // CLAUSES. It is left standing, per lane GFXMESH's own reasoning, because a
    // refusal that vanishes teaches nothing.
    // ========================================================================
    //
    // WHY EACH, in one line — the proofs are in the lane doc:
    //
    //  ?_singleton@Application@1@0PEAV11@EA — HARMFUL, not no-op. This is a
    //      mutable pointer, not a vtable, and Ghidra get_xrefs_to(0x1801fc678)
    //      reports 26 in-DLL RIP-relative references to it: 2 WRITE (the
    //      Application ctor 0x18009f5b8 and a Startup 0x1800dbbe2) and 24 READ
    //      across Application, UI and Input (WindowProc, Get, EnableCursor,
    //      Input::PlatformStartup / PlatformShutdown / PlatformUpdate /
    //      PlatformGetKeyboardState / PlatformGetMouseState, two Update bodies).
    //      WE INTERCEPT ONLY AT DOORS AND A RIP-RELATIVE MOV IS NOT ONE, so
    //      minting our own storage would SPLIT THE BRAIN -- our readers on our
    //      copy, those 26 sites on theirs, the ctor publishing into one of the
    //      two. The correct arrangement is the current one: RtApp_Get resolves
    //      the EXPORTED NAME through RT_DATA_SLOT and therefore reads the very
    //      slot retail writes (swept 6/6 against the shipped ?Get@ by
    //      rt_orphan_probe, which refuses to run under BANISHED_RT_MINT=1 and
    //      reports SPLIT instead).
    //      EXPIRY: none. A future lane that promotes this introduces a defect.
    //
    //  ??_7DialogController@UI@@6B@ — its single consumer stamps the pointer
    //      and then calls ??1Internal@Object@@, WHOSE FIRST STATEMENT
    //      OVERWRITES THE SAME QWORD. The value is written and overwritten
    //      UNREAD. No route could ever observe a different table.
    //      EXPIRY: none. No future lane should hunt a route.
    //
    //  ??_7Dialog@UI@@6B@ — complete consumer set is 4 stamps / 0 reads, all
    //      onto stack temporaries destroyed by a DIRECT ??1Dialog call. The
    //      Dialogs the framework dispatches on are built by
    //      ?CreateNewObject@Dialog@UI@@ (0x180038530) and
    //      ?CreateInPlaceObject@Dialog@UI@@ (0x180038610), both UNPORTED, so
    //      retail's factory stamps its own table through an unredirectable
    //      in-DLL LEA.
    //      EXPIRY: the moment either factory is ported and registered. One grep
    //      to check; [DlgTrip] foreignDispatch > 0 to confirm.
    //
    //  ??_7Base@Object@@6B@, ??_7External@Object@@6B@,
    //  ??_7CompiledExternal@Object@@6B@ — NO-OP, and the proof is C++'s, not
    //      this tree's. Each of the three carries PURECALL slots (1, 4 and 4 of
    //      them), so each class is ABSTRACT and no leaf instance of it can
    //      exist; a base table is therefore only ever the live +0x00 for the
    //      instant between the base constructor and the derived one, and no
    //      virtual call is made in that window. Two measurements close it:
    //        * Each of the seven `?Type@`/`?TypeName@` bodies in
    //          runtime/src/object/object_typeinfo_accessors.cpp appears in
    //          EXACTLY ONE vtable in the whole of Runtime-x64.dll — its own base
    //          row. Every derived class overrides both, as it must to carry its
    //          own typeId, so no derived table can dispatch into these rows.
    //        * No qword in ANY of the four shipped binaries holds any of these
    //          three table addresses. The only writer of one is an in-DLL
    //          RIP-relative LEA, which is not a door.
    //      Gate (a) passes on all three (`slots=3 ported=2 TRAP=0 purecall=1`
    //      and `slots=8 ported=4 TRAP=0 purecall=4` twice) since lane OBJDATA
    //      ported the seven accessors; gate (b) reports
    //      `[ObjTI] ATTENDANCE … entries=0` in BOTH halves of the A/B.
    //      EXPIRY: none. Do not spend a lane hunting a route to an abstract
    //      class's own vtable.
    //
    //  ??_7Internal@Object@@6B@ — READY (`slots=4 ported=4 TRAP=0 purecall=0`)
    //      and NOT PROMOTED. It is NOW TAGGED, which reverses lane OBJDATA's
    //      deliberate decision to leave it untagged; the reversal is WAVE 11
    //      follow-up 5 (lane TINFO) and it rests on a measurement OBJDATA could
    //      not make, not on a re-reading of OBJDATA's evidence.
    //
    //      OBJDATA was right that the purecall argument does not settle this
    //      row: `Object::Internal` is CONCRETE, so a bare instance genuinely
    //      could carry this table. Its expiry was therefore left LIVE —
    //      "the moment `[ObjTI] ATTENDANCE … entries>0`" — and the advisor then
    //      AMENDED it to require gate (b) as well (WAVE11.md §ADVISOR
    //      ADJUDICATION), because `VtblInternal()` was registry-first and a
    //      promotion would have split the class's vptr.
    //
    //      GATE (b) IS NOW IMPLEMENTED (runtime/src/object/internal.cpp:
    //      `VtblInternal`/`VtblBase` pin the DLL's table while the DLL is
    //      present and fall back to the registry only when it is absent), and
    //      implementing it is what settles this row — because THE TWO
    //      CONDITIONS ARE MUTUALLY EXCLUSIVE. The only thing that could ever
    //      have made `entries>0` on a DLL-present run is our own ctor stamping
    //      the MINTED table; gate (b) is precisely the change that stops it
    //      doing so. Measured both ways, one variable, with the row armed:
    //
    //        BANISHED_RT_INTERNAL_VTBL_RAW=0  stamped=MINTED (SPLIT), ctorRuns=124395
    //        default (pin armed)              stamped=DLL, no split,  ctorRuns=124680
    //
    //      and `[ObjTI] ATTENDANCE ??_7Internal@Object@@6B@ entries=0` in BOTH
    //      halves — including the half where 124,395 live objects really did
    //      carry the minted table. So nothing dispatches slots 2/3 of this
    //      table on any route this project can drive, split or not. The ObjTI
    //      control was armed for that run and screen0/screen1/screen2 all read
    //      GREEN, with screen2 discriminating `d_mint=2` on the armed row — so
    //      the zero is the instrument's answer and not its silence.
    //
    //      EXPIRY: NONE while Runtime-x64.dll is present. At cutover the DLL is
    //      gone, `VtblInternal` falls through to the registry by design, and the
    //      minted table is the only one there is — but cutover registers every
    //      minted row anyway, so it is not a promotion and this tag does not
    //      stand in its way.
    //
    //  ?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A,
    //  ?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A — NO-OP, and it is the
    //      SAME mechanism as the row above, one level up. Lane TINFO measured
    //      two independent things:
    //
    //        1. FOLLOW-UP 2'S BLOCKER IS CLEARED FOR THESE TWO ROWS. Both carry
    //           abort-trap factory pointers, and follow-up 2 made those
    //           reachable in principle by porting `?Load@External@Object@@`.
    //           runtime/src/object/factory_dispatch_probe.cpp instruments ALL
    //           THREE `BaseTypeInfo::factoryFn` dispatch sites in this tree
    //           (External::Load, Factory::CreateObject, DialogInstance::
    //           AddControllerTab) and reports, over a full autostart→gameplay
    //           run: 0 of 102,454 dispatches through a minted node, and the two
    //           subject types created through a factory pointer ZERO times —
    //           on a cap-immune counter, with the predicate control and the
    //           liveness control both GREEN. The reason is structural and the
    //           mint's own banner already stated it: every dispatch site reads
    //           `info` out of `?_singleton@Factory@Object@@`'s AVL tree, and
    //           the minted nodes hang off `g_mintTypeInfoHead`, a private chain
    //           no `Factory::Startup` walks.
    //
    //        2. AND THERE IS STILL NO ROUTE. The ONLY consumers of either
    //           symbol in the whole tree are the two accessors in
    //           runtime/src/object/object_typeinfo_accessors.cpp (checked by
    //           grep on the mangled name; neither symbol is among the exe's 42
    //           imported `?_typeInfo@` slots, checked with pefile). Those
    //           accessors are reachable only through the four
    //           `??_7…@Object@@6B@` vtables, and every one of those four is now
    //           proved routeless — three by C++ (abstract), the fourth by the
    //           measurement above. `[ObjTI] ATTENDANCE` reads `entries=0` for
    //           both rows on every route driven, including a `BANISHED_AUTOLOAD`
    //           run and a run with both rows armed.
    //
    //      So promoting either would be SAFE and would change NOTHING, which is
    //      the definition of the NO-OP class.
    //      EXPIRY: NONE while the DLL is present. It moves only if some future
    //      port acquires a by-name reader of one of these nodes — which is a
    //      grep, not a hunt.
    //
    // AND THE FACT THAT SETTLES THE EXE HALF OF BOTH: Application-x64.exe
    // imports 899 symbols, 833 of them from Runtime-x64.dll, and ZERO ??_7.
    // No exe code can ever consume a Runtime vtable row — in our build or in
    // retail's. That is a property of the shipped binary, not of our porting.
    // ========================================================================

    // ------------------------------------------------------------------------
    // NOT PROMOTED, and why. Each of these was measured this run, not assumed.
    //
    //      WARNING THESE COUNTS GO STALE FAST. They were 27 across four
    //      vtables when lane UIDATA counted; lane UI4 closed seven of
    //      SpriteSheet's eight, lane SS the ninth, and lane ELEM all eight of
    //      Element's — three separate lanes inside two waves. Re-read the
    //      readiness table from a live BANISHED_RT_MINT_SHADOW=1 run rather
    //      than trusting this line. That is exactly how the ??_7Element and
    //      ??_7SpriteSheet entries that used to sit here went stale, and how
    //      the ??_7Dialog one below cost a lane a whole dispatch.
    //  ??_7Dialog@UI@@6B@   9 slots, **9 ported, TRAP = 0** as of Wave 10C lane
    //      DLG8 — and still NOT PROMOTED, for DialogController's reason below
    //      and measured the same way. This is now the SECOND green-screen
    //      refusal in this file, which makes it a pattern rather than an
    //      exception, so read the two together.
    //
    //      The eight traps this paragraph used to list are closed:
    //      runtime/src/ui/dialog_vtable.cpp has the seven code slots (1,2,3,5,
    //      6,7,8) behind BANISHED_RT_DLGVTABLE, and slot 0 — the unexported
    //      vector deleting destructor at 0x180038b20 — is a 38th row of the
    //      family table in runtime/src/object/deleting_dtor.cpp, under
    //      BANISHED_RT_DELDTOR. **THE SWITCH COUPLING IS REAL**:
    //      BANISHED_RT_DELDTOR=0 re-traps slot 0 and makes this row unsafe.
    //      That is not new — ??_7SpriteSheet@UI@@6B@, promoted last wave, has
    //      exactly the same dependency on exactly the same switch.
    //
    //      Gate (a) PASSES, on a live boot (lane_dlg8_logs/shadow2):
    //          READY  ??_7Dialog@UI@@6B@  slots= 9 ported= 9 TRAP= 0 purecall= 0
    //
    //      **Gate (b) says the row is LIVE FOR THE STAMP AND DEAD FOR THE
    //      DISPATCH**, which is a distinction lane DLGV's row could not draw
    //      and this one can. Three A/B routes, each `BANISHED_RT_MINT_ROWS=
    //      ??_7Dialog@UI@@6B@` against the identical boot without it, all three
    //      reaching `[Shutdown] done` with a 0-byte crash log:
    //
    //        * THE EFFECTIVENESS CONTROL FIRES, and it is not a no-op. Lane
    //          DLG2's ctor differential moves `divergences=0 -> 3` the moment
    //          the row is armed: our ported `??0Dialog` stamps the MINTED table
    //          where retail's stamps the DLL's, so +0x00 differs. The row
    //          genuinely reaches live objects — `DialogTempCtor`
    //          (runtime/src/ui/dialog.cpp) stamps `ResolveExport(
    //          "??_7Dialog@UI@@6B@")` on the element path of every dialog the
    //          game builds, and lane DLG2 already measured that path as firing
    //          in every run.
    //        * AND NOTHING EVER DISPATCHES IT. dialog_vtable.cpp's seven-way
    //          ATTENDANCE counter prints, in EVERY half of every pair —
    //          autostart-to-gameplay, autostart with BANISHED_RT_DIFF, and the
    //          front-end Mods route that builds and tears down the whole main
    //          menu:
    //              [DlgVtbl] ATTENDANCE: none of the seven bodies was entered
    //          The temp Dialog is destroyed by a DIRECT call to our ported
    //          `??1Dialog`, never through slot 0, and no DLL body makes a
    //          virtual call on it.
    //
    //      So promoting would put a different pointer in every Dialog's +0x00
    //      and buy nothing measurable, while making DLG2's oracle carry three
    //      permanent divergences. A run in which our bodies are entered zero
    //      times cannot distinguish a safe promotion from an unexercised one —
    //      this file's own "WHY NOT ALL 104" rule.
    //
    //      **THE CENSUS AGREES, INDEPENDENTLY.** Of the eight names this lane
    //      registered, exactly ONE entered the census (`?Serialize@Dialog@UI@@`,
    //      ported=1) — the other seven are reached only through
    //      `RT_ORIGINAL_ONCE`, which is a DIFF-ONLY door that disappears at
    //      cutover and which the called-scan correctly does not count. Two
    //      instruments, different mechanisms, same answer: nothing in this tree
    //      calls these for real yet.
    //
    //      It expires the moment `[DlgVtbl] ATTENDANCE` prints a non-zero
    //      total. The instrument is in the tree and prints on every run, so
    //      nobody has to rediscover the question.
    //
    //      THE OLD NOTE HERE SAID "NOT MINTED AT ALL … expires by re-running
    //      tools/rt_mint_gen.py". Both halves were misleading and cost a lane
    //      a dispatch. The generator did not merely need re-running: it was
    //      REFUSING to emit, on one unrelated missing size
    //      (?_disableHandler@Debug@System@@0_NA, now pinned at 1 in its
    //      EXTRA_SIZES with the DLL-side proof recorded). And re-running it was
    //      never going to "expire" this entry the way the note implied — a mint
    //      is not a promotion. Minting Dialog moved it from "no row exists" to
    //      "a row exists with 8 traps"; it did not move the census, which still
    //      lists ??_7Dialog@UI@@6B@ as called-but-not-ported and will until
    //      those eight bodies are ported and registered.
    //  ??_7DialogController@UI@@6B@   11 slots, 11 ported, **TRAP = 0**, and
    //      still NOT PROMOTED. This row is the one place in this file where the
    //      readiness screen is GREEN and the answer is still no, so the reason
    //      is worth stating exactly.
    //
    //      Lane DLGV ported all nine remaining slots
    //      (runtime/src/ui/dialog_controller_base.cpp — seven of them are
    //      genuinely `RET 0`, checked byte-wise AND one disassembly at a time),
    //      so gate (a) passes: the shadow reports
    //          READY  ??_7DialogController@UI@@6B@  slots=11 ported=11 TRAP= 0
    //
    //      **Gate (b) is UNINFORMATIVE, on two different routes.** That file
    //      carries a nine-way ATTENDANCE counter for exactly this question, and
    //      on both A/B pairs it printed, in BOTH halves,
    //
    //          [DlgCtrl] ATTENDANCE: none of the nine bodies was entered
    //
    //      — including the pair whose candidate half opened an in-game
    //      RESIDENCE INFO PANEL and left it open
    //      (BANISHED_TOOL_HARNESS=housepanel; the log line
    //      `[HousePanel] OPEN house[0]=... dialog=...` is in that run). The
    //      panel's DialogInstance really did reach our `?OnOpened@` — its
    //      entries counter went 2 -> 3 — and its `_controllers` list was empty,
    //      so not one controller virtual was dispatched. Every DialogController
    //      SUBCLASS in code/Game/Dialog/ (DestroyUI, CitizenUI, MapUI, BuildUI,
    //      CropUI, DebugUI, TutorialUI, …) has its own exe-side vtable; nothing
    //      this lane could drive constructs an object carrying the RUNTIME
    //      BASE table.
    //
    //      A run in which registering the row changes nothing and our bodies
    //      are entered zero times cannot distinguish a safe promotion from an
    //      unexercised one. That is this file's own "WHY NOT ALL 104" rule
    //      (a port with no caller, that no run can verify), applied to a row
    //      whose readiness number says go.
    //
    //      *** WAVE 10D lane VTFID — THE STATED EXPIRY WAS THE WRONG TEST, AND
    //      THE ROW'S BLOCKER IS NOW A PERMANENT FACT, NOT A MISSING ROUTE. ***
    //
    //      The expiry above ("the moment any route makes `[DlgCtrl]
    //      ATTENDANCE` print a non-zero total") is now SATISFIED — this wave's
    //      vtable fidelity fix takes it to `total=1699` on an ordinary run with
    //      no switch. It changed nothing about this row, exactly as lane ROUTES
    //      predicted when it retired the same gate: that counter measures the
    //      nine BODIES, which are registered unconditionally, so it never
    //      varied with the row at all.
    //
    //      Measured on the fixed tree — same binary, same seed, same route, one
    //      variable (`BANISHED_RT_MINT_ROWS=??_7DialogController@UI@@6B@`) —
    //      the normalised log diff is the two `[RtMint] ROW ON` lines the armed
    //      half prints, plus the splash/camera/town-name jitter that differs in
    //      every pair. Nothing varies with the row.
    //
    //      And now the reason is a BINARY FACT rather than a state of our tree.
    //      This row's only consumer is the ported base destructor
    //      `??1DialogController` (runtime/src/ui/dialog.cpp:1227), reachable
    //      only through vtable SLOT 0. tools/dlgctrl_vtable_audit.py reads all
    //      39 retail subclass vtables out of Application-x64.exe: **not one of
    //      them inherits slot 0** — every subclass overrides its own deleting
    //      destructor, as C++ requires. So retail itself never dispatches the
    //      base destructor from a subclass, and no amount of extra fidelity in
    //      our tables can ever reach it. Exercising this row needs a bare
    //      `UI::DialogController` instance, which nothing constructs.
    //
    //      That is a stronger statement than the one it replaces: not "no
    //      subclass currently calls it" but "retail's own tables make it
    //      unreachable by dispatch". Do not spend another lane hunting a route.
    //
    //      The nine BODIES are ported and registered under both doors
    //      regardless, so `runtime_census.py --namespace UI` moves for them;
    //      only the DATA row waits — and after Wave 10D seven of the nine are
    //      also EXERCISED (`OnOpened=9 OnClosed=8 OnUpdateLayout=841
    //      OnUpdate=841`), which the row's status does not depend on.
    //
    //  ?_singleton@Factory@Object@@, ?_singleton@ExternalFactory@Object@@ and
    //  ?_singleton@InternalFactory@Object@@ — HARMFUL, all three, and the
    //      reason is NOT the one the Wave 15 phase-2 reference table predicts.
    //      That table (docs/runtime_port/wave15/rt_singleton_refs.csv) asks "of
    //      this row's in-DLL references, how many are READS in bodies we have
    //      NOT ported?", and on that screen the three look completely different:
    //          ExternalFactory  151 refs, 143 unported/unknown-owner reads
    //          Factory           20 refs,  13 unported/unknown-owner reads
    //          InternalFactory    6 refs,   1 unported read
    //      — and that ONE unported InternalFactory reader is
    //      `?Get@InternalFactory@Object@@SAPEAV12@XZ`, which this lane then
    //      proved DEAD by three independent routes (Ghidra xrefs: export table
    //      only; a rel32/jmp scan of every byte of .text: zero call sites; a
    //      whole-image qword scan: zero pointer-valued references; and it is not
    //      among the 899 / 163 / 146 named imports of the exe, DX11 or DX9).
    //      Five refs ported-and-registered plus one dead reader is EXACTLY the
    //      shape that made `?_singleton@SaveLoad@Save@@` safe, and on that
    //      screen alone InternalFactory should have been promoted.
    //
    //      IT IS UNSAFE ANYWAY, and one line of this file's own exit-phase
    //      shadow says why:
    //          storage ?_singleton@InternalFactory@Object@@  DLL=WRITTEN  minted=zero
    //          storage ?_singleton@Factory@Object@@          DLL=WRITTEN  minted=zero
    //          storage ?_singleton@ExternalFactory@Object@@  DLL=WRITTEN  minted=zero
    //          storage ?_singleton@SaveLoad@Save@@           DLL=still zero minted=zero
    //      The three Object factories ARE constructed on every boot, and the
    //      constructor that does it is reached by an in-DLL rel32 from
    //      Engine::Startup, which is not a door. Our ported ctors publish
    //      through RT_DATA_SLOT and would follow a promotion, but they never
    //      RUN, so the minted storage stays zero for the entire process.
    //      Promoting any of the three hands every one of our readers a NULL
    //      factory pointer while the live factory sits in the DLL's slot — the
    //      `?_singleton@MemoryAllocator@System@@` failure mode that makes the
    //      global BANISHED_RT_MINT=1 switch fatal, reached by a different road.
    //      SaveLoad is safe precisely because its DLL slot is ALSO still zero:
    //      nothing constructs one on this route, so promoting it moved nothing.
    //
    //      THE GENERAL LESSON, because it is not written down anywhere else:
    //      the reference-ownership table is NECESSARY AND NOT SUFFICIENT. It
    //      answers "who READS this slot", and a storage row also has to answer
    //      "does anything ever WRITE OUR copy". A row whose only writer is an
    //      in-DLL call to a ctor we have ported but nothing invokes is unsafe no
    //      matter how its reads are distributed. Read the `minted=` column of
    //      the exit-phase shadow BEFORE the reference CSV.
    //      EXPIRY: none by promotion. The condition that would change it is a
    //      route that makes OUR ctor run — at which point `minted=written`
    //      appears in that shadow line and the question can be re-asked.
    //
    //  ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA (0x1801fc6e0) — HARMFUL,
    //      and the highest-risk row in lane OBJDATA's ten. This is the head of
    //      the reflection chain, and unlike the three factories above BOTH
    //      copies are written on every run (`DLL=WRITTEN  minted=written`) —
    //      which is worse, not better, because the two heads then hold DIFFERENT
    //      chains. A decoder-independent disp32 scan of .text finds its
    //      references cluster in one shape: ~104 read/write PAIRS at
    //      0x180147xxx-0x18014axxx on a regular stride (the inlined
    //      `??0BaseTypeInfo@Object@@` in one static initialiser per type, each
    //      linking itself onto the head), the out-of-line ctor itself at
    //      0x180006320, and `Factory::Startup`'s own read at 0x1800c065e. Every
    //      one of those is an unredirectable RIP-relative access to the DLL's
    //      cell. Our side already writes ours: `[FactoryReg] Registered 312
    //      types into Runtime _firstTypeInfo` and `[GameMain] Post-Startup
    //      _firstTypeInfo: 416 entries`. Today those are the SAME cell, because
    //      the row is unpromoted and our resolver hands us the DLL's — which is
    //      the arrangement that keeps one chain with 416 entries in it.
    //      Promoting splits it into our 312 and their 104, with retail's
    //      Startup walking the half that does not contain the exe's types.
    //      EXPIRY: none before cutover. At cutover there is no second chain and
    //      the row becomes the only head, which is a different question from
    //      this one.
    //
    //  ?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A  (0x1801f9e48)
    //      READY on the shadow — `idcmp=yes strs=0 ptr=2 fac=2traps` — and
    //      still NOT PROMOTED. WAVE 10C lane TIDENT examined it and the verdict
    //      changed REASON, which is the part worth recording:
    //
    //      * DLGV's stated blocker — pointer identity — is RETIRED, and not
    //        only for this row (see the header). It was never the reason to
    //        hold this one back.
    //      * The reason it is held back is gate (b), and it is the same
    //        "a port with no caller" rule as the 94 rows above: this row's ONE
    //        consumer in the tree is `TypeIdOfStatic(...)` in
    //        runtime/src/ui/dialog_create.cpp, reached from
    //        `RtUiDialogInstance_AddControllerTab` (0x1801133b0), and it is
    //        NEVER ENTERED. That file now carries a `[TidStatic]` attendance
    //        line printing the node pointer and the id it reads, and across
    //        FIVE routes — autostart+quit, and BANISHED_TOOL_HARNESS in
    //        housepanel / pinpanel / tradepanel — it printed ZERO lines, in
    //        both halves of the armed/unarmed A/B. Same root as the
    //        `[DlgCtrl] ATTENDANCE: none of the nine bodies was entered` above:
    //        nothing drivable constructs a controller through the runtime base
    //        path.
    //
    //      So arming it is measurably a no-op, and an A/B in which nothing is
    //      entered cannot distinguish a safe promotion from an unexercised one.
    //      It expires the moment `[TidStatic]` prints on any route — the
    //      instrument is in the tree and prints on every run, so nobody has to
    //      rediscover the question. NOTE that when it does, the id will be
    //      IDENTICAL either way (idcmp=yes); the observable is the NODE POINTER
    //      on that line moving out of the DLL's data into our image, which is
    //      why the line prints the pointer and not just the value.
    // ------------------------------------------------------------------------
    // WAVE 25 LANE DATA — the fourteen UNTAGGED DATA gap rows, adjudicated.
    // ------------------------------------------------------------------------
    //
    // Full table, per-row evidence and the instrument's own controls are in
    // docs/runtime_port/WAVE25_LANE_DATA.md.  Re-derive any row's counts with
    //
    //   python tools/runtime_census.py --unported --csv c.csv
    //   python tools/rt_data_row_refs.py --census c.csv          # scalar rows
    //   python tools/rt_data_row_refs.py --census c.csv --width 0x38   # aggregates
    //   python tools/rt_indll_callers.py <name>=<va> ...
    //
    // THE MEASUREMENT THAT DECIDED TWELVE OF THESE FOURTEEN, and the one this
    // lane would most like the next lane to inherit:
    //
    //   THE CENSUS `ported` COLUMN DOES NOT MEAN "EVERY READER GOES THROUGH OUR
    //   BODY".  It means a door is registered for that export.  The DLL's own
    //   copy of the same function still runs for every in-DLL rel32 call site,
    //   and it still touches the DLL's statics.  `?SetText@Error@System@@` is
    //   `ported`, and tools/rt_indll_callers.py finds **18 in-DLL rel32 call
    //   sites** that reach retail's SetText without passing a door; `?Display@`
    //   is `ported` and has 2.  So for the three Error/Debug rows, "all my
    //   readers and writers are ported" was TRUE on the census and FALSE in the
    //   process, and promoting them would have split the error-reporting state
    //   exactly on the out-of-memory path that needs it.  A reader/writer
    //   census over .text is necessary and NOT sufficient; the rel32 caller
    //   count is the other half.
    //
    // WHY THE CLASS FIELD BELOW IS ONE HYPHENATED TOKEN AND NOT
    // "HARMFUL (DLL-present) / REQUIRED (cutover)", WHICH READS BETTER.
    // tools/runtime_census.py parses the class as `(\S+)` in an optional group
    // ANCHORED AT END OF LINE, so a class field containing a space does not
    // merely lose its class -- the whole regex fails and the ENTIRE TAG IS
    // DROPPED.  Five tags already spell it the readable way
    // (?_singleton@Application@1@, Configuration/Device @Audio@ and @Input@) and
    // have therefore been invisible to the census for several waves while
    // tools/unpromotable_slot_check.py counted them the whole time; the two
    // instruments disagreed by five with no detector, because neither prints the
    // other's count.  Found by Wave 25 lane MANIFEST.  These fourteen are spelled
    // to be parsed by BOTH tools rather than to look nice in one of them.  If a
    // later lane widens the census regex, the five may be re-spelled and so may
    // these -- but not before, or the count silently moves again.
    // RT_UNPROMOTABLE: ?_errorString@Error@System@@0PADA | n/a (char[0x2000] buffer) | WAVE25_LANE_DATA.md §3.1 | NONE while the DLL is present -- all 5 in-DLL refs sit in ?SetText@ and ?Display@, which the census calls PORTED, but tools/rt_indll_callers.py finds 18 in-DLL rel32 sites reaching retail's SetText and 2 reaching retail's Display, none of them a door; promoting moves our copy and leaves every one of those 20 writing/reading retail's buffer. At cutover all 20 are gone and the row is REQUIRED (error.cpp:175 resolves it by name and ResolveExport is NULL for an unpublished row) [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25, which carries the cutover publish list; and any later lane that drives the 18 rel32 SetText sites to 0 -- that, not the census `ported` column, is this tag's expiry condition] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_stopApplication@Error@System@@0_NA | n/a (bool) | WAVE25_LANE_DATA.md §3.1 | NONE while the DLL is present -- identical argument to ?_errorString@ above and the SAME 18+2 rel32 sites; the two writers are ?SetText@ (18 unreachable-by-door callers) and the reader is ?Display@ (2). At cutover REQUIRED, error.cpp:178 [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = the 18 rel32 SetText sites reaching 0] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_hideDisplay@Error@System@@0_NA | n/a (bool) | WAVE25_LANE_DATA.md §3.1 | NONE while the DLL is present -- same 18+2 rel32 argument. NOTE the blocker that is NOT the reason: its one unported writer ?HideErrorDisplay@Error@System@@ is DEAD (Wave 22 ERRCFG proved no call site in any of the four shipped binaries, four screens each with a live positive control), so the dependency-shaped objection is discharged and the row still fails on the rel32 sites. At cutover REQUIRED, error.cpp:184 and engine_lifecycle.cpp:289 [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = the 18 rel32 SetText sites reaching 0] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_disableHandler@Debug@System@@0_NA | n/a (bool) | WAVE25_LANE_DATA.md §3.2 | NONE while the DLL is present -- 1 in-DLL reader, ?ExceptionHandler@Debug@System@@ (UNPORTED and LIVE: it is the DLL's own top-level exception filter), against 5 writers of which only ?EnumerateVideoModes@Device@Video@@ is ours. Promoting makes our EnumerateVideoModes raise a flag retail's filter never sees, so retail swallows the probe exception it is supposed to re-raise. At cutover the filter is gone and the row is REQUIRED (enumerate_video_modes.cpp:112) [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ?ExceptionHandler@Debug@System@@ ported AND registered] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_secondsPerTick@Timer@System@@0NA | n/a (double) | WAVE25_LANE_DATA.md §3.3 | NONE while the DLL is present -- 96 in-DLL reads against 2 writes, and the readers are spread across the whole engine (ParticleSystemInstance::Update, four UI element Update bodies, Audio::Scene::Update, Device::Input::UpdateState, MainLoop::Frame/Draw, SaveLoad's five Update bodies), a MAJORITY of them unported. This is the ?_firstTypeInfo@ shape at 96:2. At cutover all 96 are gone and our timer.cpp is the only consumer, so REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = every in-DLL reader listed in the lane doc ported, which is engine-wide work and not a DATA-lane task] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_ticksPerSecond@Timer@System@@0_JA | n/a (int64) | WAVE25_LANE_DATA.md §3.3 | NONE while the DLL is present -- 12 in-DLL reads / 2 writes; 6 of the readers are unported (MainLoop::MarkFrameUpdateTime, Timer::CurrentTime, Timer::CurrentTimeTicks, MainLoop::Draw, MainLoop::Frame, Context::Finish). At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = those 6 readers ported] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_systemInitialTick@Timer@System@@0_JA | n/a (int64) | WAVE25_LANE_DATA.md §3.3 | NONE while the DLL is present -- 12 in-DLL reads, ZERO in-DLL writes through a RIP store (the only writer is ?PlatformStartup@Timer@System@@, which takes its ADDRESS by LEA and writes through the register -- a form no RIP-relative scan can see, recorded here so a later lane does not read `W=0` as `never written`); 6 readers unported, same six as ?_ticksPerSecond@. At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = those 6 readers ported] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_systemTicksPerSecond@Timer@System@@0_JA | n/a (int64) | WAVE25_LANE_DATA.md §3.3 | NONE while the DLL is present, and this row is HARMFUL IN BOTH DIRECTIONS AT ONCE: its ONE writer ?PlatformStartup@Timer@System@@ is UNPORTED, so a promoted slot is never written and stays 0, while 12 in-DLL readers reach it as `idiv qword ptr [rip]` -- a zero divisor is #DE, not a wrong number. This is the ?_singleton@InternalFactory@Object@@ shape (the harm is the WRITER) stacked on the ?_firstTypeInfo@ shape (the harm is the readers). At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ?PlatformStartup@Timer@System@@ ported AND registered, which must come FIRST for this row] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_systemSecondsPerTick@Timer@System@@0NA | n/a (double) | WAVE25_LANE_DATA.md §3.3 | NONE while the DLL is present -- exactly ONE in-DLL reference in the whole of .text and it is a WRITE, by the UNPORTED ?PlatformStartup@Timer@System@@. Zero in-DLL readers, so there is no split-brain to fear and the harm is purely the writer: a promoted slot is never written and runtime/src/system/timer.cpp reads 0.0. A one-reference row is the easiest kind to misread as trivially safe [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ?PlatformStartup@Timer@System@@ ported AND registered] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_singleton@MainLoop@Engine@@1PEAV12@EA | n/a (scalar ptr) | WAVE25_LANE_DATA.md §3.4 | NONE while the DLL is present -- 19 in-DLL reads against 1 write (the MainLoop ctor, which IS ported), the textbook Wave 23 GFXDATA shape: a ported writer is not progress toward promotability while readers remain. Unported readers include ?Draw@MainLoop@Engine@@ and ?Finish@Context@Video@@. At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = every in-DLL reader ported] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_singleton@Engine@1@0PEAV11@EA | n/a (scalar ptr) | WAVE25_LANE_DATA.md §3.4 | NONE while the DLL is present -- 4 in-DLL reads / 2 writes; both writers are ours (Engine ctor and dtor) and at least one reader, ?Run@Loader@Engine@@, is not. Same shape as ?_singleton@AnimationManager@Graphics@@, which Wave 23 GFXDATA re-derived and left HARMFUL on identical grounds. At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ?Run@Loader@Engine@@ and the other in-DLL readers ported] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // *** WAVE 27 lane LOOPS, 2026-09-12: THE EXPIRY CONDITION ABOVE IS NOW
    // *** SATISFIED AND THE ROW IS STILL NOT PROMOTABLE. `?Startup@LoaderLoop@`
    // *** IS ported and registered (runtime/src/engine/loops.cpp,
    // *** RtEngine_LoaderLoop_Startup). So the condition was MIS-SPECIFIED: it
    // *** named the wrong fact. Owning the CODE that takes `_loader`'s address
    // *** was never the blocker -- owning the code that CONSTRUCTS the object
    // *** is. `??0Loader@Engine@@QEAA@XZ` (0x1800dc0f0) is retail and unported,
    // *** `_loader` is a STATIC built by the DLL's CRT static-init before any of
    // *** our code runs, and the DLL's loader thread was created on THAT object
    // *** and waits on ITS events. A promoted row would hand our name-resolving
    // *** doors a second Loader that nothing constructed and no thread watches;
    // *** the loading screen would hang rather than crash. The verdict was RIGHT
    // *** on a PREMISE THAT HAS NOW CHANGED, which is the shape that lets a
    // *** satisfied expiry read as a green light.
    // *** CORRECTED CONDITION -- AND THE FIRST CORRECTION THIS LANE WROTE HERE
    // *** WAS ITSELF WRONG, FIXED BEFORE THE LANE CLOSED. It said 'expiry =
    // *** ??0Loader@Engine@@QEAA@XZ ported AND registered'. That is the SAME
    // *** defect it was correcting: a condition naming a symbol that is not the
    // *** blocker. The exported ??0Loader@ is NEVER CALLED (0 direct E8/E9
    // *** sites, and its address occurs as a qword in NO PE section, so no
    // *** indirect route either). `_loader` is really built by an UNEXPORTED
    // *** STATIC-INIT THUNK at 0x180148fe0 with the ctor inlined -- it calls
    // *** Thread::Thread(&_loader, 0x40000), stamps ??_7Loader@ at 0x180148fff,
    // *** and nulls _loop -- AT DLL LOAD, before any of our code exists.
    // *** THERE IS THEREFORE NO SYMBOL TO PORT THAT RETIRES THIS ROW.
    // *** CORRECTED CONDITION: none. [SCOPE: DLL-PRESENT] is route-scoped: while
    // *** the DLL is present this row can never be promoted; at CUTOVER it is
    // *** REQUIRED and we construct `_loader` ourselves.
    // *** CORRECTED ADDRESSEE: the lane that runs the next CUTOVER attempt --
    // *** NOT a porting lane, because no amount of porting reaches it. (The old
    // *** addressee, 'lane MANIFEST of Wave 25', is a closed wave.)
    // *** Same blocker and same disposition as ??_7Loader@Engine@@6B@ above;
    // *** the two rows must be retired together, at cutover.
    // RT_UNPROMOTABLE: ?_loader@LoaderLoop@Engine@@1VLoader@2@A | n/a (a Loader OBJECT, not a pointer) | WAVE25_LANE_DATA.md §3.4 | NONE while the DLL is present -- this row is an AGGREGATE and its consumers take its ADDRESS: 5 LEA and 2 stores, and ?Startup@LoaderLoop@Engine@@ (UNPORTED) does `lea rcx,[0x1801fe7b8]` and then works through the register. Promoting gives our doors a different object from the one that unported body constructs and runs. NOTE FOR ANY LANE RE-DERIVING THIS: a RIP-relative READ/WRITE census reports `R=0` here and that 0 is an ARTEFACT of the access pattern, not evidence of disuse. At cutover REQUIRED (engine/funcs/engine_funcs.cpp and runtime/src/engine/loops.cpp both resolve it by name) [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ?Startup@LoaderLoop@Engine@@ ported AND registered] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_singleton@Configuration@System@@0PEAV12@EA | n/a (scalar ptr) | WAVE25_LANE_DATA.md §3.4 | NONE while the DLL is present -- 4 in-DLL reads / 2 writes, and BOTH halves are split: the reader ?Get@Configuration@System@@ is unported AND the writer ??0Configuration@System@@ is unported, so a promoted slot is neither written by retail's ctor nor read by retail's accessor. At cutover REQUIRED [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ??0Configuration@System@@ ported AND registered, then ?Get@] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    // RT_UNPROMOTABLE: ?_singleton@AchievementManager@Achievement@@0PEAV12@EA | n/a (scalar ptr) | WAVE25_LANE_DATA.md §3.5 | NONE while the DLL is present -- an INVERTED-SHAPE row: only 1 in-DLL read against 5 writes, and the harm is on the writer side. Two of the five writers, ??0AchievementManager@Achievement@@ and a store inside ??0DialogLoopT@Engine@@'s range, are UNPORTED, so retail constructs the manager into retail's slot and a promoted slot would hold whatever our ported Engine::Startup/Shutdown last wrote -- two owners, one object. A LOW READ COUNT IS NOT A SAFE ROW. At cutover REQUIRED (gameupdateloop_serialize.cpp and achievement_manager.cpp both resolve it by name) [SCOPE: DLL-PRESENT] [ADDRESSEE: lane MANIFEST of Wave 25; expiry = ??0AchievementManager@Achievement@@ ported AND registered] | HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER
    //   [WAVE 27 lane ACHIEVE -- THIS TAG'S REASONING IS WRONG IN BOTH HALVES.
    //    ITS COUNT IS RIGHT. RE-DERIVED FROM THE SHIPPED RETAIL FILE, NOT FROM
    //    GHIDRA: `objdump -d "C:/GOG Games/Banished/Runtime-x64.dll"` and grep
    //    the resolved-target comment `# 0x1801fc670`. Complete site set, 6 of
    //    them, exactly the 1 READ + 5 WRITES this tag claims:
    //      R 0x1800079d0 ?Get@AchievementManager@   PORTED+registered
    //                                               (achievement_manager.cpp:1143)
    //      W 0x18004d946 FUN_18004d930              UNPORTED -- AND DEAD CODE
    //      W 0x18009eac6 ??1AchievementManager@     PORTED (achievement_state.cpp)
    //      W 0x18009eb35 ??0AchievementManager@     PORTED (engine_lifecycle.cpp:1101)
    //      W 0x1800db698 ?Shutdown@Engine@1@        PORTED
    //      W 0x1800dbe83 ?Startup@Engine@1@         PORTED
    //
    //    (a) "a store inside ??0DialogLoopT@Engine@@'s range" NAMES THE WRONG
    //        FUNCTION. There is no store to this cell anywhere in that ctor.
    //        Found by Wave 27 lane LOOPS, which read 0x1800db3f0..0x1800db452
    //        instruction by instruction; the tag's attribution came from a
    //        function extent stretched to the next export
    //        (reference_adjacent_export_is_an_upper_bound.md). The fifth writer
    //        is real but lives at 0x18004d930, ~0x8e000 away.
    //    (b) BUT THE FIFTH WRITER IS NOT MISSING, AND A REFUTATION THAT COUNTED
    //        ONLY 3 WRITES IS ALSO WRONG. A scan matching only MOV r/m,r and
    //        MOV r,r/m (opcodes 89/8B) is BLIND to `MOV r/m, imm32` (opcode C7),
    //        which is how BOTH 0x18004d946 and 0x18009eac6 spell `= nullptr`.
    //        Two of the five writes are C7 stores.
    //
    //    THE ROW'S HAZARD IS NEVERTHELESS GONE, on a third set of facts:
    //    FUN_18004d930 is the compiler-generated SCALAR DELETING DESTRUCTOR
    //    (~AchievementManager then operator delete; byte-identical to the ported
    //    0x18009eab0 plus a trailing CALL 0x180006a10) and it is UNREACHABLE --
    //    ZERO rel32 call/jmp sites, and an 8-byte scan of EVERY section finds
    //    its address nowhere, so NO IN-DLL DISPATCH STRUCTURE names it -- no
    //    vtable, jump table, function-pointer global or static-init table,
    //    which is where an indirect in-DLL call must get an address from. It is
    //    ALSO absent from the export directory (checked separately, with a
    //    control: ?Get@AchievementManager@ is found there at ordinal 2403 of
    //    4926). So nothing reaches it by any route: it cannot run, and it
    //    cannot write this cell.
    //    GENERAL FORM, because the narrow form does not travel: "absent from
    //    every section" licenses "no in-DLL dispatch structure names it" and
    //    NOT "nothing names it" -- an 8-byte VA scan cannot see the export
    //    directory, which stores 32-bit RVAs. Reading it as "not exported" is
    //    reference_aeaa_is_private_not_unexported.md from its other side. The
    //    export check above is a SECOND screen for that reason, not a
    //    restatement of the first. ?Get@ likewise has ZERO in-DLL callers.
    //    So NO in-DLL code path touches this cell at all, and the "two owners,
    //    one object" hazard has no owner on the DLL side.
    //    THAT LIMIT WAS THEN CLOSED, because "is this symbol the thing that
    //    actually RUNS?" is the check these tags keep failing. It is not enough
    //    that our copies of the four live writers are ported: if the DLL calls
    //    its OWN copy of any of them, a promoted cell and the DLL's cell are
    //    two owners again. Measured, both ways, for all four:
    //      DIRECT   rel32 call/jmp sites in .text      ctor 0, dtor 0,
    //                                                  Engine::Startup 0,
    //                                                  Engine::Shutdown 0
    //      INDIRECT 8-byte scan of EVERY PE section    all four: absent
    //    (Engine::Startup 0x1800db8a0 and Engine::Shutdown 0x1800db4f0 were
    //     confirmed as true function entries via get_function_by_address, not
    //     derived by subtracting an offset.)
    //
    //    BOTH SCREENS WERE HANDED KNOWN POSITIVES FIRST, because a screen whose
    //    normal answer is 0 is indistinguishable from a dead parser
    //    (reference_absence_screen_must_be_handed_a_shape.md):
    //      direct-call screen: 0x180009700 -> 5 sites, 0x1800ca4e0 -> 5 sites
    //      address-presence  : ?Run@Loader@ 0x1800dbfc0 found once at
    //                          0x180157a08, and the UpdateLoop deleting dtor
    //                          0x180031900 found once at 0x180155ba0 -- the two
    //                          positives Wave 27 lane LOOPS used, reproduced
    //                          here independently at the same two addresses.
    //    REMAINING LIMIT, stated: the address-presence screen matches 64-bit
    //    VAs, so it cannot see the PE export table (which stores 32-bit RVAs).
    //    That is not the question being asked -- the question is in-DLL dispatch
    //    tables -- but "absent" here does not mean "not exported".
    //
    //    LANE ACHIEVE DID NOT PROMOTE THIS ROW and is not asking for it to be:
    //    promotion is the mint/manifest owner's call and needs the cutover
    //    manifest regenerated with it.
    //    [ADDRESSEE: the Wave 28 kickoff writer, or whichever lane next owns the
    //     mint tables. Re-run the two commands above before acting -- this note
    //     is a measurement, and the thing it replaces was also once a
    //     measurement.]
    //
    // THE SEVEN `?_typeInfo@` GAP ROWS ARE DELIBERATELY NOT TAGGED HERE.
    // They are NOT unpromotable -- they are READY and BLOCKED ON ONE THING
    // THIS LANE WAS NOT ALLOCATED, a run.  Stated so the next lane does not
    // re-derive it:
    //
    //   ?_typeInfo@Achievement@1@ / @AchievementInstance@ / @GoalInstance@
    //   ?_typeInfo@AnimationEvent@ / @MaterialInstance@ / @Particle@Graphics@
    //   ?_typeInfo@PackageFile@IO@
    //
    //   * SAFE.  Each has ZERO mutable in-DLL state: measured at --width 0x38,
    //     each node carries exactly TWO in-DLL writes, both in the CRT static
    //     initialiser (`parentName` +0x10 and `next` +0x30), which run once at
    //     DLL load into the DLL's own node and never again.  Every other in-DLL
    //     reference is a `MOV r32, dword ptr [rip]` of +0x00 `typeId`, whose
    //     minted value is measured EQUAL to the DLL's on 104/104 rows
    //     (RtMintShadowReport("exit"), 624 field comparisons, 0 MISMATCH).
    //     There is no chain to split.
    //   * HAS A CONSUMER.  All seven have a live name door: package_file.cpp:327,
    //     goal.cpp:169, achievement_manager.cpp:293, graphics_typeinfo_accessors
    //     .cpp:132/136/140, gameupdateloop_load.cpp:75,
    //     gameupdateloop_serialize.cpp:195, reg_unreg_lane3.cpp:164/195.
    //   * BLOCKED ON CRITERION 2 AT THE TOP OF THIS FILE -- a real boot.  The
    //     committed evidence says `never` for three of them
    //     (docs/runtime_port/wave19_phase0c/run2_attend/: PackageFile init=0
    //     post=0, MaterialInstance and Particle init=1 post=0) and says nothing
    //     at all about the other four, because three of the four were not in
    //     the watch table.  This lane added them (wave17_door_attend.cpp) so
    //     the next armed run measures all seven.
    //   * AND ONE THING TO ANNOTATE WITH THE PROMOTION, NOT AFTER IT: promoting
    //     AnimationEvent / MaterialInstance / Particle / PackageFile makes their
    //     `?TypeName@` bodies hand out OUR node's `name` POINTER, and those four
    //     bodies compare that pointer against the DLL's with RT_DIFF_SCALAR.
    //     They will read `divergences=N` forever after, exactly as
    //     ?TypeName@SpriteSheet@UI@@ already does (see the top of this file).
    //     That is the string twin of the vtable-identity divergence and it is
    //     NOT a regression -- but an unannotated red line is how a sweep gets
    //     misread.  Annotate beside each body in the same commit as the
    //     promotion.
    //
    // ---- WAVE 26 LANE PROMOIO: the IO one of those seven, adjudicated -------
    //
    //   ?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A
    //       NOT PROMOTED.  STATUS: **ROUTE-UNDECIDED**, which is a THIRD state
    //       and is neither "ready" nor "not ready".
    //
    //   THE MEASUREMENT (lane ATTEND, not this lane; same run and same GREEN
    //   MainLoop control as the four vtable promotions above):
    //       ?_typeInfo@PackageFile@IO@@   init=0  post=0   -- never
    //
    //   WHY THAT ZERO IS NOT A RED LIGHT, and this is the whole point of the
    //   entry.  ATTEND was asked to separate (a) "the row is not ready" from
    //   (b) "the route never reached it, so nothing ever fetched the table and
    //   the silence is not evidence about the row at all".  It reported (b),
    //   with evidence rather than assertion:
    //     * the symbol's only consumers are the three reflection bodies
    //       RtIo_PackageFile_Type/_TypeId/_TypeName (package_file.cpp:531/542/
    //       552), all reading through PackageFileTypeInfo() at :325;
    //     * in the SAME run `??_7PackageFile@IO@@6B@` reads ATTENDED post=1 —
    //       so PackageFile objects ARE built through our door on that route,
    //       and nothing on it asks one for its type;
    //     * sibling `?_typeInfo@` rows read post>0 in the same run
    //       (Material@Graphics, GeneratedImageBuffer@Graphics), so the zero
    //       cannot be blamed on the RT_DATA_SLOT door mechanism;
    //     * the consumer literal is byte-identical to the watch-table literal,
    //       which excludes the one (a)-shaped failure available here — spelling
    //       drift reading as post=0 while the door worked.
    //   ATTEND's own caveat is kept rather than dropped: this counts DOORS, not
    //   executions, and "route never reaches it" and "route reaches those
    //   objects only through in-DLL calls that are not doors" are both
    //   consistent with post=0 and it cannot separate them.
    //
    //   SO WHY NOT PROMOTE ANYWAY, given the four vtables above were promoted
    //   on weaker attendance?  Because the cost is not symmetric.  A `?_typeInfo@`
    //   promotion makes `?TypeName@PackageFile@IO@@` hand out OUR node's `name`
    //   POINTER, and that body compares the pointer against the DLL's with
    //   RT_DIFF_SCALAR — it would read `divergences=N` forever after, exactly as
    //   ?TypeName@SpriteSheet@UI@@ already does.  That annotation must land in
    //   the SAME COMMIT as the promotion (lane DATA's instruction, and the
    //   reason is that an unannotated red line is how a sweep gets misread).
    //   Writing a permanent red line into a body on the strength of a
    //   measurement that is explicitly NOT ABOUT THE ROW is the wrong trade.
    //   The vtable rows carry no such cost: their oracles are already immune.
    //
    //   [ADDRESSEE: the Wave 26 integrator.]
    //   [CONDITION, and it is a ROUTE condition, not a readiness one: promote
    //   this row when ONE run reads it post>0 on a route THE GAME DRIVES.  The
    //   candidate is a route that asks a PackageFile for its type — the mods
    //   route would, and CLAUDE.md records that route crashing in
    //   ModDialog_ToggleMod_Impl+0x43, so this row is blocked on THAT CRASH and
    //   not on anything about itself.]
    //   [DO NOT re-derive the (a)/(b) question — it is measured above.  What is
    //   missing is a ROUTE, not an analysis.]
    //
    //   A CHEAPER CLOSE WAS PROPOSED — BY ME — AND IT IS WRONG.  RETRACTED HERE
    //   RATHER THAN QUIETLY DELETED, because the recipe is more dangerous than
    //   the row.  The first version of this block named "call
    //   RtIo_PackageFile_TypeName once from standalone/tool_harness.cpp, which
    //   needs no new route" and said it SETTLES IT.  It does not.  Lane ATTEND
    //   refused it and is right:
    //
    //     Criterion 2 is "a real boot", and what that criterion BUYS is *did a
    //     consumer run on the route*.  A harness call makes OUR OWN TEST CODE
    //     the consumer.  The counter would read post=1, the row would look
    //     promoted-on-evidence, and the evidence would be the measurement
    //     calling itself — and in the log it is INDISTINGUISHABLE from the real
    //     thing.  That is what makes it dangerous rather than merely useless.
    //     It is this project's self-check-read-as-an-oracle family
    //     (reference_shadow_marker_is_not_an_oracle.md,
    //     reference_a_probe_whose_ours_side_reaches_the_body_by_address…) in its
    //     cheapest and most persuasive form.
    //
    //   WHAT SUCH A RUN WOULD LEGITIMATELY ESTABLISH, if anyone still wants it:
    //   that the row's DOOR RESOLVES — the name is not a phantom and
    //   RT_DATA_SLOT returns non-null.  Record that as a DOOR-WORKS check under
    //   that name and NEVER in the attendance column.  It is also largely
    //   redundant: ATTEND's (a)-refutation already covers it, because sibling
    //   `?_typeInfo@` rows on the identical RT_DATA_SLOT idiom read post>0 in
    //   the same run.
    //
    //   THE GENERAL RULE, which is the part worth keeping: AN ATTENDANCE
    //   COUNTER MEASURES WHETHER THE GAME REACHED A SYMBOL.  Any close that
    //   works by adding a caller does not satisfy it — it defeats it, and
    //   leaves a number that reads exactly like the one it replaced.
    //
    //   AND THE REASON THIS RETRACTION IS SPELLED OUT HERE, IN SOURCE, RATHER
    //   THAN LEFT IN THE COMMIT THAT FIXED IT: **a commit message is a
    //   permanently uncorrectable artefact.**  The bad recipe went out in
    //   `d668f883`'s message and is in that message forever; `c9a56cb8`
    //   retracts it and is the correction of record, but a later reader
    //   grepping `?_typeInfo@PackageFile@IO@@` through the history will meet
    //   the two messages in either order and has no way to tell which one the
    //   tree agrees with.  This project knows two neighbours of this family — a
    //   generated file that goes stale with no detector, and a prescription
    //   read as a completion — and this is the third and worst, because it is
    //   not stale-with-no-detector but stale-with-NO-POSSIBLE-REPAIR.
    //
    //   SO THE SOURCE IS THE ONLY SURFACE THAT CAN CARRY THE CORRECTION, and
    //   the two defences are both taken here: the correction NAMES the
    //   uncorrectable artefact by hash, and it lives beside the row rather than
    //   only in a lane doc, because the row's name is what gets grepped.
    //   (Framing due to lane ATTEND, Wave 26.)
    // ------------------------------------------------------------------------

    printf("[RtMint] promoted %d minted DATA row(s) -- live on every run\n", n);
    printf("[RtMint] vtable promotion guard: %d row(s) evaluated, %d refused\n",
           g_vtGuardEvaluated, g_vtGuardRefused);
    return n;
}
