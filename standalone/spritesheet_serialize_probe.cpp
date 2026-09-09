// ============================================================================
// standalone/spritesheet_serialize_probe.cpp — Wave 10C lane ROUTES
//
// A WRITE-PATH REPLAY for `?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z`,
// the last "ported, calls=0" row of the Wave 10B UI set.
//
// OFF BY DEFAULT. Prints nothing unless BOTH switches are set:
//     BANISHED_SPRITESHEET_SERIALIZE_PROBE=1       arms this probe (sound
//                                                  side-swapped arrangement)
//     BANISHED_SPRITESHEET_SERIALIZE_PROBE=naive   the original arrangement,
//                                                  kept only to reproduce the
//                                                  re-entrancy artefact
//     BANISHED_RT_SAVE_TRACE=1                 arms the oracle it reports through
//
// ----------------------------------------------------------------------------
// WHY THE ROW WAS calls=0, AND WHY THAT NUMBER IS TRUSTWORTHY HERE
// ----------------------------------------------------------------------------
// Lane SS ported the body and promoted `??_7SpriteSheet@UI@@6B@`, expecting the
// promotion to supply the caller: "until ??_7SpriteSheet@UI@@6B@ is promoted this
// port genuinely has no caller — which is the whole point of the promotion"
// (runtime/src/ui/spritesheet_vtable.cpp). Measured on the promoted tip with the
// oracle armed, it still does not:
//
//     [RtSaveTrace] ==== 22 compared, 0 diverged ====
//
// and not one of those 22 is `UI::SpriteSheet::Serialize`. The boot-time sheet
// is built by the DLL's own `ExternalFactory`, which dispatches slot 5 off the
// DLL's table, so the promotion changed nothing for this row.
//
// **That `calls=0` is a real measurement and not a dark instrument**, which is
// the distinction this project keeps paying for. The same run's oracle made 22
// comparisons on two other bodies, so it was demonstrably live and reporting
// while SpriteSheet::Serialize contributed nothing. Compare that with
// `Element::Update`, whose published `calls=0` turned out to be the ABSENCE of a
// site (there was no counter that could have printed anything else) — the two
// zeros look identical in a lane doc and mean opposite things
// (reference_diagnostic_field_cannot_express_the_answer.md).
//
// ----------------------------------------------------------------------------
// THE ROUTE: REPLAY, NOT A HUNT FOR A CALLER
// ----------------------------------------------------------------------------
// On the WRITE path a Serialize is a pure read of the object, so ours and
// retail's can BOTH be run over the same object provided they emit into
// DIFFERENT sinks, and the two emission sequences compared
// (reference_write_path_serialize_is_replayable.md). That is cheaper and
// stronger than finding a reader: it does not depend on the game happening to
// save a sprite sheet, and it compares against the shipped export rather than
// against our own verdict (`RT_DIFF_SHADOW` is a self-check, not an oracle —
// reference_shadow_marker_is_not_an_oracle.md).
//
// `RtSaveTrace::Compare` already builds the two sinks and hands each side a
// `Serializer*`; the body's own probe block would do exactly this if anything
// ever called it. This file supplies the one thing missing — a CALL, on a real
// subject.
//
// THE SUBJECT IS THE REAL BOOT-TIME SHEET, not a constructed one. `Dialog/
// SpriteSheet.rsc` is loaded through the DLL's ExternalFactory during boot and
// parked at KeyMap+0xa0 by StandaloneEnsureSpriteSheetLoaded()
// (standalone/menudialogloopt_stubs.cpp:184). It is fully populated — the same
// run logs `Sprite("SaveLoadEmpty") -> idx=211`, so there are hundreds of sprite
// entries for the count/entries arm of the body to walk, plus both Strings and
// the MaterialInstance ExternalPtr. Using it means every arm of the body is
// driven by real data (reference_synthetic_subject_beats_a_fake). A hand-built
// SpriteSheet would have tested our idea of a sheet.
//
// SAFETY: nothing on this body's write path stores into the object — retail's
// own branch guards the Array SetSize to the READING side, and the ExternalPtr
// write arm touches only its own locals — so replaying the whole body is sound
// and `stopAfter` is deliberately 0 (no prefix truncation, everything compared).
// That is lane SS's reading of the disassembly, recorded in the body's ORACLE
// banner; this probe relies on it and says so rather than re-deriving it.
//
// WHAT A PASS DOES AND DOES NOT LICENCE. It says our body emits the identical
// sequence of scalars, strings and nested records as the shipped export, for
// this sheet, in the writing direction. It says nothing about the READING
// direction, whose `Array::SetSize` arm is the one retail guards off the write
// path — that arm stays unverified, and the count below reports the write side
// only.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "resolve.h"
#include "engine_funcs.h"

namespace RtSaveTrace {
bool Enabled();
bool Compare(const char* name, void* obj,
             void (*ours)(void*, void*), void (*theirs)(void*, void*),
             int stopAfter);
}  // namespace RtSaveTrace

// Our ported body. Declared rather than included: this file must not pull the
// runtime UI type headers into the standalone link for two pointers.
extern "C" bool RtUi_SpriteSheet_Serialize(void* self, void* stream);

namespace {

const char* const kSerializeMangled =
    "?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z";

using StreamMemberFn = bool (*)(void*, void*);

// Serializer's first field IS the Stream* (runtime/types/io/Serializer.h:241,
// static_assert'd there), which is what lets both lambdas take the Serializer*
// RtSaveTrace hands them and pass the body the Stream it wants.
inline void* StreamOf(void* serializer) {
    return *reinterpret_cast<void**>(serializer);
}

StreamMemberFn RetailBody() {
    // The RAW export, deliberately: ResolveExport would hand back OUR port,
    // which turns the oracle into a comparison of the port with itself -- the
    // vacuous pass this project has already paid for
    // (reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md).
    static StreamMemberFn f =
        reinterpret_cast<StreamMemberFn>(ResolveExportRaw(kSerializeMangled));
    return f;
}

}  // namespace

void SpriteSheetSerializeProbe_Run() {
    static bool s_done = false;
    if (s_done) return;

    const char* e = std::getenv("BANISHED_SPRITESHEET_SERIALIZE_PROBE");
    if (!e || !*e || std::strcmp(e, "0") == 0) return;   // silent when unset
    s_done = true;

    if (!RtSaveTrace::Enabled()) {
        printf("[SheetSer] BANISHED_RT_SAVE_TRACE is not set -- the oracle "
                    "this probe reports through is off; nothing measured.\n");
        return;
    }

    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) {
        printf("[SheetSer] KeyMap singleton not ready -- no subject.\n");
        return;
    }
    void* const sheet = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton) + 0xa0);
    if (!sheet) {
        printf("[SheetSer] KeyMap+0xa0 is null -- Dialog/SpriteSheet.rsc "
                    "was never loaded, so there is no subject.\n");
        return;
    }

    StreamMemberFn theirs = RetailBody();
    void* ours = reinterpret_cast<void*>(&RtUi_SpriteSheet_Serialize);
    if (!theirs) {
        printf("[SheetSer] the shipped export %s did not resolve -- cannot "
                    "compare against retail.\n", kSerializeMangled);
        return;
    }
    // The check that makes a pass mean something: if the two sides are the same
    // pointer the comparison is retail against itself and always passes.
    if (reinterpret_cast<void*>(theirs) == ours) {
        printf("[SheetSer] ours == theirs (%p) -- the raw export resolved to "
                    "OUR port, so this would compare the port with itself. "
                    "Refusing to report a pass.\n", ours);
        return;
    }

    // ------------------------------------------------------------------
    // THE SIDE-SWAP CONTROL, and why the first reading needed one.
    // ------------------------------------------------------------------
    // The first run of this probe reported:
    //
    //   [RtSaveTrace] pass UI::SpriteSheet::Serialize: 7151 emissions identical
    //   [RtSaveTrace] DIVERGE ...: same prefix but ours emitted 7151 and theirs
    //                 14302 -- a length difference IS the desync
    //
    // 14302 is EXACTLY 2 x 7151. A clean factor of two over an identical prefix
    // is not a port defect -- it is one side running the body twice. Two causes
    // fit equally well and they invert the conclusion:
    //
    //   (a) RE-ENTRANCY. The ported body carries its OWN probe block
    //       (spritesheet_vtable.cpp:519) which fires whenever
    //       BANISHED_RT_SAVE_TRACE is set and `g_probing` is false. Calling it
    //       from inside an OUTER Compare sets neither, so the inner Compare runs
    //       a whole nested pair while the outer sinks are recording.
    //   (b) The oracle's own sink handling not being re-entrant, so whichever
    //       body runs second is double-counted regardless of which it is.
    //
    // BANISHED_SPRITESHEET_SERIALIZE_PROBE=swap runs the identical comparison
    // with the two sides EXCHANGED. If the doubling follows the PORTED body onto
    // the `theirs` slot, the cause is (a) -- our body, and our body alone,
    // re-enters. If the doubling stays on whichever side is passed as `theirs`,
    // the cause is (b) and the instrument is what is wrong.
    //
    // This is the "could this field express the answer if the answer were
    // different?" test applied to the probe itself: the first reading could not
    // separate a port that emits half of retail's records from an instrument that
    // counts one side twice, and those are opposite verdicts.
    // THE ANSWER, and why `swap` is now the DEFAULT arrangement.
    //
    //   arrangement            ours side        theirs side      verdict
    //   naive  (port as ours)  port    7151     retail  14302    DIVERGE
    //   swap   (port as theirs) retail 7151     port     7151    MATCH
    //
    // The doubling did NOT follow the port -- in the swapped run the port sits
    // on the `theirs` slot and emits 7151, not 14302. It follows the
    // ARRANGEMENT: it appears only when the body carrying its own probe block
    // runs in the `ours` position, where its nested Compare re-enters the oracle
    // while the outer sinks are live and retail's emissions are recorded twice.
    // So 14302 was never a property of either body. A port that emitted half of
    // retail's records and an oracle that counted one side twice produce the
    // same DIVERGE line, and only the side swap separates them.
    //
    // The swapped arrangement is the SOUND one and is therefore the default:
    // retail's body contains no probe block, so putting it in the `ours`
    // position means nothing re-enters. `=naive` still selects the original
    // arrangement, kept so the artefact can be reproduced rather than taken on
    // trust.
    //
    // Read the [RtSaveTrace] labels with the swap in mind: on the `[swapped]`
    // line, "ours" is RETAIL and "theirs" is OUR PORT. The line is named
    // `[swapped]` for exactly that reason.
    const bool swap = (std::strcmp(e, "naive") != 0);
    printf("[SheetSer] replaying the WRITE path on the boot-time sheet %p "
                "(port=%p retail=%p)%s\n", sheet, ours, (void*)theirs,
                swap ? "  [SIDE-SWAP CONTROL: retail is `ours`, the port is `theirs`]" : "");
    const bool ok = swap
        ? RtSaveTrace::Compare(
              "UI::SpriteSheet::Serialize[swapped]", sheet,
              [](void* obj, void* ser) { RetailBody()(obj, StreamOf(ser)); },
              [](void* obj, void* ser) { RtUi_SpriteSheet_Serialize(obj, StreamOf(ser)); },
              0)
        : RtSaveTrace::Compare(
              "UI::SpriteSheet::Serialize", sheet,
              [](void* obj, void* ser) { RtUi_SpriteSheet_Serialize(obj, StreamOf(ser)); },
              [](void* obj, void* ser) { RetailBody()(obj, StreamOf(ser)); },
              0);
    printf("[SheetSer] replays=1 swap=%d verdict=%s (read the [RtSaveTrace] "
                "line above for the emission COUNTS on both sides -- a pass over "
                "ZERO emissions is not a verification, and a clean factor of two "
                "is a re-entrancy artefact, not a divergence)\n",
                swap ? 1 : 0, ok ? "MATCH" : "DIVERGED");
    fflush(stdout);
}
