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
