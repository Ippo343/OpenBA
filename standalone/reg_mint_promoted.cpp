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
// THE TEN UI `?_typeInfo@` ROWS  (an ELEVENTH, Element, was added below by
// Wave 10B lane ELEM once it acquired a live consumer; see its own warrant)
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

    // ------------------------------------------------------------------------
    // NOT PROMOTED, and why. Each of these was measured this run, not assumed.
    //
    //  ??_7SpriteSheet@UI@@6B@        9 slots, 8 ported, 1 TRAP.
    //      Lane UI4 took it from 1/8 to 8/1 and it is the CLOSEST UI vtable in
    //      the mint, but one trap is disqualifying: our ctors stamp this table
    //      into every SpriteSheet, so the DLL's own dispatch reaches the trap
    //      and abort()s. The surviving slot is named by the readiness table
    //      every run; today it is
    //          slot 5  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z
    //      Expires when that one body is ported and registered.
    //  ??_7DialogInstance@UI@@6B@     10 slots, 8 ported, 2 TRAP.
    //  ??_7DialogController@UI@@6B@   11 slots, 2 ported, 9 TRAP.
    //      Same rule, further away. 11 trap slots between the two.
    //      ⚠ THESE COUNTS ARE THE WAVE-10 BASELINE'S. Wave 10B is running
    //      lanes on both of them concurrently with lane ELEM; re-read the
    //      readiness table from a live BANISHED_RT_MINT_SHADOW=1 run rather
    //      than trusting this line, which is exactly how the `??_7Element@`
    //      entry that used to sit here went stale.
    //  ??_7Element@UI@@6B@            PROMOTED 2026-09-09 by lane ELEM; the
    //      warrant is above. It was `12 slots, 3 ported, 8 TRAP, 1 purecall`
    //      when this paragraph was written.
    //  ??_7Dialog@UI@@6B@             NOT MINTED AT ALL — it is not a row in
    //      standalone/rt_data_mint_tables.inc, so there is nothing to promote.
    //      It became a census row when runtime/src/ui/dialog.cpp started
    //      resolving it, after the tables were last generated. Expires by
    //      re-running tools/rt_mint_gen.py, which is the generator's lane.
    // ------------------------------------------------------------------------

    printf("[RtMint] promoted %d minted DATA row(s) -- live on every run\n", n);
    return n;
}
