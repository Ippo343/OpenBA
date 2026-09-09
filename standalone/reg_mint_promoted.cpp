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
// THE TEN UI `?_typeInfo@` ROWS
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
    //  ??_7Element@UI@@6B@            12 slots, 3 ported, 8 TRAP, 1 purecall.
    //  ??_7DialogController@UI@@6B@   11 slots, 2 ported, 9 TRAP.
    //      Same rule, further away. 20 trap slots between the four.
    //  ??_7Dialog@UI@@6B@             9 slots, 1 ported, 8 TRAP.
    //      MINTED as of Wave 10B lane MINT (2026-09-09) — 0x1801562a0,
    //      sizeProof "tail-not-a-pointer", and its 9 slots are among the
    //      536 the shadow's ground-truth screen compares against the live
    //      DLL with 0 MISMATCH. So it is a row now, and the reason it is
    //      still not promoted is the ordinary one: eight trap slots.
    //          slot 0  (unexported; known by address only) DLL 0x180038b20
    //          slot 1  ?TypeName@Dialog@UI@@UEBAPEBDXZ
    //          slot 2  ?Type@Dialog@UI@@UEBAIXZ
    //          slot 3  ?GetFileVersion@Dialog@UI@@UEBAHXZ
    //          slot 5  ?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z
    //          slot 6  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
    //          slot 7  ?Destruct@Dialog@UI@@UEAAXXZ
    //          slot 8  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
    //      The one ported slot is 4, ?Compile@External@Object@@. It is the
    //      FURTHEST of the five UI vtables from promotion, not the closest.
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
    // ------------------------------------------------------------------------

    printf("[RtMint] promoted %d minted DATA row(s) -- live on every run\n", n);
    return n;
}
