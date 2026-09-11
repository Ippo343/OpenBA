// ============================================================================
// standalone/reg_mint_promoted.cpp — Wave 10 lane MINTSEL
//
// MINTED DATA ROWS THAT ARE LIVE ON EVERY RUN.
// ============================================================================
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

int RtMintRegisterPromotedRows() {
    // Kill switch, for the A/B this file's warrants were written from and for
    // anyone bisecting a UI regression onto it. DEFAULT-ON: these rows are
    // ports, and a port that only runs when asked is not one.
    const char* off = getenv("BANISHED_RT_MINT_PROMOTED");
    if (off && strcmp(off, "0") == 0) {
        printf("[RtMint] BANISHED_RT_MINT_PROMOTED=0 -- no minted row registered\n");
        return 0;
    }
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
    // VERIFIED** — its write-path oracle (RtSaveTrace, wired in
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
    st = RtMintPromotionStorage("??_7SpriteSheet@UI@@6B@", 0x180154590ull);
    RegisterRuntimeExport("??_7SpriteSheet@UI@@6B@", st);
    RegisterRuntimeAddr(0x180154590ull, st); ++n;
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
    st = RtMintPromotionStorage("??_7Element@UI@@6B@", 0x180156160ull);
    RegisterRuntimeExport("??_7Element@UI@@6B@", st);
    RegisterRuntimeAddr(0x180156160ull, st); ++n;
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
    st = RtMintPromotionStorage("??_7DialogInstance@UI@@6B@", 0x180156d70ull);
    RegisterRuntimeExport("??_7DialogInstance@UI@@6B@", st);
    RegisterRuntimeAddr(0x180156d70ull, st); ++n;

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
    st = RtMintPromotionStorage("??_7ToolTipDialog@UI@@6B@", 0x180157880ull);
    RegisterRuntimeExport("??_7ToolTipDialog@UI@@6B@", st);
    RegisterRuntimeAddr(0x180157880ull, st); ++n;

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
    // reported no entry at all (the only `[RtSaveTrace]` lines in either run
    // are `Achievement::Goal::Serialize`), which is the `calls=0` column and
    // not a pass (reference_uncalled_port_is_not_verified.md).
    //
    // Promoting is still right, and for the reason lane SS gave for SpriteSheet
    // slot 5: TRAP=0 means those seven slots can no longer `abort()`, slot 0 IS
    // exercised and clean, and leaving the row unpromoted would not verify the
    // seven either — it would only keep slot 0 unreachable too.
    st = RtMintPromotionStorage("??_7ImageBuffer@Video@@6B@", 0x180154fc0ull);
    RegisterRuntimeExport("??_7ImageBuffer@Video@@6B@", st);
    RegisterRuntimeAddr(0x180154fc0ull, st); ++n;

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
    st = RtMintPromotionStorage("??_7BinaryReader@IO@@6B@", 0x1801552e0ull);
    RegisterRuntimeExport("??_7BinaryReader@IO@@6B@", st);
    RegisterRuntimeAddr(0x1801552e0ull, st); ++n;

    st = RtMintPromotionStorage("??_7Visitor@IO@@6B@", 0x1801558b0ull);
    RegisterRuntimeExport("??_7Visitor@IO@@6B@", st);
    RegisterRuntimeAddr(0x1801558b0ull, st); ++n;

    st = RtMintPromotionStorage("??_7MemoryStream@IO@@6B@", 0x1801555b0ull);
    RegisterRuntimeExport("??_7MemoryStream@IO@@6B@", st);
    RegisterRuntimeAddr(0x1801555b0ull, st); ++n;

    st = RtMintPromotionStorage("??_7VisitorStream@IO@@6B@", 0x180155a20ull);
    RegisterRuntimeExport("??_7VisitorStream@IO@@6B@", st);
    RegisterRuntimeAddr(0x180155a20ull, st); ++n;

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
    st = RtMintPromotionStorage("??_7BinaryWriter@IO@@6B@", 0x180155210ull);
    RegisterRuntimeExport("??_7BinaryWriter@IO@@6B@", st);
    RegisterRuntimeAddr(0x180155210ull, st); ++n;

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
    {
        const char* off = getenv("BANISHED_RT_PROMOTE_BFS");
        if (off && strcmp(off, "0") == 0) {
            printf("[RtMint] BANISHED_RT_PROMOTE_BFS=0 -- "
                   "??_7BinaryFileStream@IO@@6B@ left on the DLL (A/B control)\n");
        } else {
            st = RtMintPromotionStorage("??_7BinaryFileStream@IO@@6B@", 0x1801554d0ull);
            RegisterRuntimeExport("??_7BinaryFileStream@IO@@6B@", st);
            RegisterRuntimeAddr(0x1801554d0ull, st); ++n;
        }
    }

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
    // RT_UNPROMOTABLE: ??_7DialogController@UI@@6B@ | 11/11 | WAVE10E_LANE_FINAL.md §2 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7Dialog@UI@@6B@ | 9/9 | WAVE10E_LANE_FINAL.md §3 | either Dialog factory ported+registered
    // RT_UNPROMOTABLE: ?_singleton@Application@1@0PEAV11@EA | n/a (scalar ptr) | WAVE12_APPLICATION.md §ceiling | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ??_7Base@Object@@6B@ | 2/2 (1 purecall) | WAVE11_LANE_OBJDATA.md §2 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7External@Object@@6B@ | 4/4 (4 purecall) | WAVE11_LANE_OBJDATA.md §2 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7CompiledExternal@Object@@6B@ | 4/4 (4 purecall) | WAVE11_LANE_OBJDATA.md §2 | NONE, EVER
    // RT_UNPROMOTABLE: ?_singleton@Factory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@ExternalFactory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@InternalFactory@Object@@0PEAV12@EA | n/a (scalar ptr) | WAVE11_LANE_OBJDATA.md §3 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA | n/a (chain head) | WAVE11_LANE_OBJDATA.md §4 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ??_7Internal@Object@@6B@ | 4/4 (0 purecall) | WAVE11F_LANE_TINFO.md §2 | NONE while the DLL is present
    // RT_UNPROMOTABLE: ?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A | n/a (BaseTypeInfo) | WAVE11F_LANE_TINFO.md §3 | NONE while the DLL is present
    // RT_UNPROMOTABLE: ?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A | n/a (BaseTypeInfo) | WAVE11F_LANE_TINFO.md §3 | NONE while the DLL is present
    //
    // ---- WAVE 17 LANE CEIL: Video / IO / Graphics ------------------------
    //
    // The twelve Video companion-base vtables. ONE mechanism, measured the
    // same way for all twelve, and it is the `??_7DialogController@UI@@6B@`
    // shape: the stamp is overwritten, or freed, UNREAD.
    //
    // RT_UNPROMOTABLE: ??_7ConstantBufferData@Video@@6B@ | 1/1 (3 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7ContextData@Video@@6B@ | 1/1 (19 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7DeviceData@Video@@6B@ | 1/1 (19 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7ImageBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7IndexBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7PixelProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7RenderBufferData@Video@@6B@ | 1/1 (3 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7RenderStateData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7VertexBufferData@Video@@6B@ | 1/1 (6 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7VertexLayoutData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7VertexProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7VideoProgramData@Video@@6B@ | 1/1 (2 purecall) | WAVE17_LANE_CEIL.md §3.1 | NONE, EVER
    //
    // The thirteen mutable-storage rows. NOT no-ops: promoting any of them
    // would split the brain exactly as `?_singleton@Application@1@` would.
    //
    // RT_UNPROMOTABLE: ?_singleton@Configuration@Video@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Device@Video@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_firstSurface@ImageBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA | n/a (list head) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@FileSystem@IO@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Modification@IO@@2PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_singleton@Persistence@IO@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
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
    // RT_UNPROMOTABLE: ??_7ReaderWriter@IO@@6B@ | 1/26 ported [RE-DERIVED, WAVE21_LANE_ENC.md item 12: the `?` numerator resolved by lane ENC, the assigned addressee -- slot 0 (0x180023bd0, the deleting dtor) is now runtime/src/object/deleting_dtor.cpp row 42, verified against tools/rt_deleting_dtor_verify.py], 25 purecall [RE-DERIVED] [CONV: M=TABLE] | WAVE19_PHASE0A_NOOP.md §3.1 + WAVE19_INTEGRATOR_CORRECTIONS.md C30 + WAVE21_LANE_ENC.md item 12 | NONE, EVER
    // RT_UNPROMOTABLE: ??_7Stream@IO@@6B@ | 1/7 ported [INHERITED], 5 purecall [MEASURED] | WAVE19_PHASE0A_NOOP.md §3.2 | NONE, EVER
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
    // RT_UNPROMOTABLE: ?_singleton@AnimationManager@Graphics@@0PEAV12@EA | n/a (scalar ptr) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_frameIndex@ParticleInstance@Graphics@@1HA | n/a (int counter) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_rng@Particle@Graphics@@1VRandom@System@@A | n/a (RNG state) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA | n/a (bool flag) | WAVE17_LANE_CEIL.md §3.2 | NONE, EVER | HARMFUL
    // RT_UNPROMOTABLE: ??_7Geometry@Graphics@@6B@ | 6/6 (3 purecall) [CONV: M=NONPURECALL] | WAVE21_LANE_GFXVT.md §5.2 | ROUTE-SCOPED (DLL-PRESENT ONLY); voids when statusicon_geometry.cpp:341 reads `i < 9`; REQUIRED at cutover [SCOPE: DLL-PRESENT] [ADDRESSEE: Wave 22 lane GEOM, which owns the statusicon_geometry.cpp:341 cutover blocker -- WAVE21_CLOSED.md §7 item 1] | HARMFUL
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
    //           kDefaultOnFlags (component_factory_base.cpp:189) — DEFAULT ON.
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
    //      EXPIRY, WITH AN ADDRESSEE (reference_expiry_clause_needs_an_addressee).
    //      The blocker is ONE CHARACTER: statusicon_geometry.cpp:341's `i < 12`
    //      must read `i < 9`, the fix WAVE6_LANE_D prescribed in Wave 6. The
    //      addressee is THE NEXT LANE THAT OWNS code/Game/Component/ —
    //      explicitly not "whoever next counts slots on this row", because this
    //      lane could not make that edit and verify it inside a vtable-scoped
    //      brief. When that line reads `i < 9`, this tag is void and the row
    //      should be re-scored from scratch (its stamper census is already
    //      green: 3 stampers, 2 ours-capable, 0 unattributed).
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
    // RT_UNPROMOTABLE: ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z | n/a (function, not a table) | WAVE21_LANE_GFXBODY.md row 11 + WAVE21_LANE_CLASS.md §2 | registering it is harmful while the DLL supplies vtable[3]; voids when scene_watch.cpp:401 stops using this address as an identity token [SCOPE: DLL-PRESENT] [ADDRESSEE: the lane that next needs AddDrawItems registered -- it must give scene_watch a different identity token first; if no lane does, the Wave 22 kickoff writer] | HARMFUL
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

    printf("[RtMint] promoted %d minted DATA row(s) -- live on every run\n", n);
    return n;
}
