// ============================================================================
// runtime/src/object/object_typeinfo_accessors.cpp — WAVE 11 lane OBJDATA
// ============================================================================
//
// THE SEVEN `?Type@` / `?TypeName@` ACCESSORS THAT WERE KEEPING FOUR MINTED
// `??_7…@Object@@6B@` VTABLE ROWS OUT OF standalone/reg_mint_promoted.cpp.
//
// Lane OBJDATA's brief said its ten rows were already generated into the mint
// and so its work was "promotion, not authoring". That is true of the two
// `?_typeInfo@` rows and false of the four vtable rows: a live
// BANISHED_RT_MINT_SHADOW=1 boot on fa4fa86 reported all four BLOCKED, with
// exactly seven traps between them —
//
//     BLOCKED  ??_7Base@Object@@6B@              slots= 3 ported= 1 TRAP= 1 purecall= 1
//     BLOCKED  ??_7CompiledExternal@Object@@6B@  slots= 8 ported= 2 TRAP= 2 purecall= 4
//     BLOCKED  ??_7External@Object@@6B@          slots= 8 ported= 2 TRAP= 2 purecall= 4
//     BLOCKED  ??_7Internal@Object@@6B@          slots= 4 ported= 2 TRAP= 2 purecall= 0
//
// — and the already-ported slots are, in every case, the deleting destructor
// (runtime/src/object/deleting_dtor.cpp) plus the one real body the class has
// (`?Compile@External@` for the two External rows, `?Persist@Internal@` for
// Internal). The traps are these seven accessors, and a grep of the whole tree
// by mangled name AND by address found no body for any of them anywhere:
// this file is the first.
//
// WHAT THEY ARE. Two instructions each, and every one of them a read of a
// `?_typeInfo@…@Object@@0VBaseTypeInfo@2@A` node's first two fields. `this` is
// never read by any of the seven (the disassembly is below, per body), which is
// what makes them shareable across every instance of the class.
//
//   ?Type@Base@Object@@UEBAIXZ              0x1800c0380  33 c0 c3
//                                           XOR EAX,EAX / RET  -> literal 0.
//     Object::Base has NO `_typeInfo` node of its own — it is the root of the
//     hierarchy and its type id is the reserved 0. That is not an inference
//     from the constant: `?Type@` on every OTHER class in this file loads from
//     that class's node, and Base is the one that does not have one to load.
//
//   ?Type@CompiledExternal@Object@@UEBAIXZ  0x18000b690  8b 05 6a e4 1e 00 c3
//                                           MOV EAX,[0x1801f9b00]  = node+0x00
//   ?TypeName@CompiledExternal@…            0x18000b6b0  48 8b 05 51 e4 1e 00 c3
//                                           MOV RAX,[0x1801f9b08]  = node+0x08
//   ?Type@External@Object@@UEBAIXZ          0x1800066d0  8b 05 62 34 1f 00 c3
//                                           MOV EAX,[0x1801f9b38]  = node+0x00
//   ?TypeName@External@Object@@UEBAPEBDXZ   0x1800066c0  48 8b 05 79 34 1f 00 c3
//                                           MOV RAX,[0x1801f9b40]  = node+0x08
//   ?Type@Internal@Object@@UEBAIXZ          0x1800064d0  8b 05 42 37 1f 00 c3
//                                           MOV EAX,[0x1801f9c18]  = node+0x00
//   ?TypeName@Internal@Object@@UEBAPEBDXZ   0x1800064c0  48 8b 05 59 37 1f 00 c3
//                                           MOV RAX,[0x1801f9c20]  = node+0x08
//
// The three target addresses are the `?_typeInfo@` rows the generator already
// carries, at exactly those VAs (standalone/rt_data_mint_tables.inc lines 1798,
// 1862, 1926), so the +0x00 / +0x08 reading is checked against the generated
// table and not only against the decode.
//
// THE DOOR. Each body reads its node through RT_DATA_SLOT on the node's
// exported name, which is the same door runtime/src/ui/spritesheet_vtable.cpp
// and runtime/src/io/package_file.cpp already use for the identical shape.
// `0V…A` is the PRIVATE static spelling and this DLL exports its private
// members, so a name door is the correct door and no address door is needed
// (reference_aeaa_is_private_not_unexported.md). Reading through the door — as
// opposed to pinning the DLL's node — is what makes these bodies follow a
// promotion of `?_typeInfo@External@Object@@`: promote that row and this file's
// `?Type@External@` reads OUR node, which is the arrangement cutover needs.
//
// ⚠ ONE OF THESE ORACLES REPORTS A PERMANENT POINTER DIVERGENCE ONCE
//   `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` IS PROMOTED, exactly as
//   `UI::SpriteSheet::TypeName` already does and for the same reason: our node
//   is in our image and the shipped export returns the DLL's, same characters
//   and a different object. Read the long note in
//   runtime/src/ui/spritesheet_vtable.cpp before treating it as a regression;
//   the pointer-identity question is retired from the binaries (zero consumers
//   of any node's +0x08 in either shipped binary — WAVE10C_LANE_TIDENT.md) and
//   the comparison is deliberately left AS A POINTER rather than relaxed to a
//   strcmp, because it is the one screen that can still see identity move.
//
// GATE (b) — AND WHY AN ORDINARY ATTENDANCE COUNTER WOULD NOT BE ONE HERE.
// These seven bodies are registered unconditionally, so a counter that only
// counts entries measures the BODIES and never varies with the ROW — the
// mistake WAVE10D lane VTFID found in `[DlgCtrl] ATTENDANCE` after that counter
// had already retired a gate (reference_the_gate_was_never_a_control.md). So
// the instrument below counts entries AND classifies, at each entry, which
// table the live object actually carries: our minted row, the DLL's row, or
// some third (derived-class) table. Only the FIRST of those three says the row
// is dispatched; the third is the ordinary case of a derived object inheriting
// an accessor, which promoting this row would not change. That classification
// does vary with the promotion, which is what makes it a gate rather than a
// tally.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"

// Forward declarations: the positive control at the bottom of the anonymous
// namespace calls these, and they are defined below it.
extern "C" uint32_t    RtObject_Base_Type(const void*);
extern "C" uint32_t    RtObject_CompiledExternal_Type(const void*);
extern "C" const char* RtObject_CompiledExternal_TypeName(const void*);
extern "C" uint32_t    RtObject_External_Type(const void*);
extern "C" const char* RtObject_External_TypeName(const void*);
extern "C" uint32_t    RtObject_Internal_Type(const void*);
extern "C" const char* RtObject_Internal_TypeName(const void*);

namespace {

// ---- the exported BaseTypeInfo nodes these bodies read ----------------------
const uint8_t* CompiledExternalTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A");
}
const uint8_t* ExternalTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@External@Object@@0VBaseTypeInfo@2@A");
}
const uint8_t* InternalTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A");
}

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);

// ============================================================================
// GATE (b) INSTRUMENT — see the banner. Per row: entries, and of those, how
// many arrived on an object whose +0x00 is OUR minted table, the DLL's table,
// or a third table entirely.
//
// `ours` is read through the PORT door and `theirs` through the RAW door, so
// the two differ exactly when the row is registered and are the same pointer
// when it is not. In the unregistered half every entry is therefore counted
// `dll` (or `other`) and `mint` is structurally zero — which is the point: the
// quantity varies with the thing it gates.
// ============================================================================
struct RowAttendance {
    const char* row;         // the ??_7 symbol
    unsigned    entries;
    unsigned    mint;        // object carries OUR minted table  -> row dispatched
    unsigned    dll;         // object carries the DLL's table   -> row NOT ours
    unsigned    other;       // object carries a third table     -> inherited body
    // Both doors resolved ONCE per row and cached. ResolveExportRaw IS
    // GetProcAddress (rt_port.h says so in capitals, and calling it per entry
    // once cost this project 5.6 points of sim time —
    // reference_rtoriginal_is_getprocaddress.md), and these accessors are on a
    // hot path the moment anything does dispatch them.
    bool        resolved;
    void*       ours;
    void*       theirs;
};

// ⚠ THE REPORT IS **NOT** PRINTED FROM A STATIC DESTRUCTOR, AND THE FIRST
// VERSION OF IT WAS. That version's own screen0 is what caught the mistake: at
// static-destructor time THE PORT REGISTRY HAS ALREADY BEEN TORN DOWN, so
// ResolveExport and ResolveExportRaw return the same DLL address for every
// symbol — including `??_7Element@UI@@6B@`, which is promoted unconditionally
// in standalone/reg_mint_promoted.cpp and therefore cannot be unregistered on
// any run. Every row printed `row=unregistered(DLL)` and the ARMED half of the
// A/B was indistinguishable from the control half, which would have been
// written up as "the row changes nothing" — a false negative produced entirely
// by WHEN the instrument ran.
//
// So Report() is called explicitly from standalone/main.cpp's shutdown block,
// beside the exit-phase mint shadow, where the registry is still answering.
// The `[DlgCtrl]` static-destructor idiom is fine for a counter that only
// TALLIES; it is wrong for one that has to ask the registry a question.
//
// These are NOT RtDiff `calls=` numbers: an RtDiff site increments `calls` only from
// inside a comparison, so a body that ran and never compared prints calls=0,
// which reads as "never ran" and means the opposite
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
struct RowAttendanceTable {
    RowAttendance row[4] = {
        { "??_7Base@Object@@6B@",             0, 0, 0, 0, false, nullptr, nullptr },
        { "??_7CompiledExternal@Object@@6B@", 0, 0, 0, 0, false, nullptr, nullptr },
        { "??_7External@Object@@6B@",         0, 0, 0, 0, false, nullptr, nullptr },
        { "??_7Internal@Object@@6B@",         0, 0, 0, 0, false, nullptr, nullptr },
    };
    // Declared here, defined after the bodies it calls. See the long note on
    // ObjTiPositiveControl().
    void RunPositiveControl();

    void Report() {
        for (const RowAttendance& a : row) {
            if (a.entries == 0) {
                printf("[ObjTI] ATTENDANCE %-34s entries=0 -- nothing dispatched "
                       "or inherited these bodies on this run\n", a.row);
            } else {
                printf("[ObjTI] ATTENDANCE %-34s entries=%u  minted-table=%u  "
                       "dll-table=%u  other-table=%u\n",
                       a.row, a.entries, a.mint, a.dll, a.other);
            }
        }
        // The control runs LAST and on its own counters, so it can never be
        // mistaken for, or contaminate, the measurement above.
        RunPositiveControl();
    }
};
RowAttendanceTable g_attTable;
RowAttendance* const g_att = g_attTable.row;

void Attend(int idx, const void* self) {
    RowAttendance& a = g_att[idx];
    ++a.entries;
    if (!self) { ++a.other; return; }
    void* vt = nullptr;
    std::memcpy(&vt, self, sizeof vt);
    if (!a.resolved) {
        // ResolveExportRaw always names the DLL's copy; ResolveExport names
        // ours when the row is promoted or armed and the DLL's otherwise. They
        // differ exactly when the row is registered, which is what makes this
        // classification vary with the thing it gates.
        a.ours     = ResolveExport(a.row);
        a.theirs   = ResolveExportRaw(a.row);
        a.resolved = true;
    }
    if (a.ours && vt == a.ours && a.ours != a.theirs) ++a.mint;
    else if (a.theirs && vt == a.theirs)              ++a.dll;
    else                                              ++a.other;
}

}  // namespace

// ---- ??_7Base@Object@@6B@  slot 2 ------------------------------------------
// ?Type@Base@Object@@UEBAIXZ   0x1800c0380   XOR EAX,EAX / RET.
extern "C" uint32_t RtObject_Base_Type(const void* self) {
    RT_DIFF_BEGIN("Object::Base::Type");
    Attend(0, self);
    const uint32_t ours = 0;
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn, "?Type@Base@Object@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7CompiledExternal@Object@@6B@  slots 2 and 1 -----------------------
// ?Type@CompiledExternal@Object@@UEBAIXZ  0x18000b690  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_CompiledExternal_Type(const void* self) {
    RT_DIFF_BEGIN("Object::CompiledExternal::Type");
    Attend(1, self);
    uint32_t ours = 0;
    std::memcpy(&ours, CompiledExternalTypeInfo(), sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@CompiledExternal@Object@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ?TypeName@CompiledExternal@Object@@UEBAPEBDXZ  0x18000b6b0  MOV RAX,[node+8].
extern "C" const char* RtObject_CompiledExternal_TypeName(const void* self) {
    RT_DIFF_BEGIN("Object::CompiledExternal::TypeName");
    Attend(1, self);
    const char* ours = nullptr;
    std::memcpy(&ours, CompiledExternalTypeInfo() + 8, sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@CompiledExternal@Object@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7External@Object@@6B@  slots 2 and 1 -------------------------------
// ?Type@External@Object@@UEBAIXZ  0x1800066d0  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_External_Type(const void* self) {
    RT_DIFF_BEGIN("Object::External::Type");
    Attend(2, self);
    uint32_t ours = 0;
    std::memcpy(&ours, ExternalTypeInfo(), sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@External@Object@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ?TypeName@External@Object@@UEBAPEBDXZ  0x1800066c0  MOV RAX,[node+0x08].
// THIS is the body that reports a permanent pointer divergence once
// ?_typeInfo@External@Object@@ is promoted — see the banner.
extern "C" const char* RtObject_External_TypeName(const void* self) {
    RT_DIFF_BEGIN("Object::External::TypeName");
    Attend(2, self);
    const char* ours = nullptr;
    std::memcpy(&ours, ExternalTypeInfo() + 8, sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@External@Object@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ---- ??_7Internal@Object@@6B@  slots 2 and 1 -------------------------------
// ?Type@Internal@Object@@UEBAIXZ  0x1800064d0  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_Internal_Type(const void* self) {
    RT_DIFF_BEGIN("Object::Internal::Type");
    Attend(3, self);
    uint32_t ours = 0;
    std::memcpy(&ours, InternalTypeInfo(), sizeof ours);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(U32ThisFn,
                       "?Type@Internal@Object@@UEBAIXZ")(self));
    RT_DIFF_WORK(true);
    return ours;
}

// ?TypeName@Internal@Object@@UEBAPEBDXZ  0x1800064c0  MOV RAX,[node+0x08].
extern "C" const char* RtObject_Internal_TypeName(const void* self) {
    RT_DIFF_BEGIN("Object::Internal::TypeName");
    Attend(3, self);
    const char* ours = nullptr;
    std::memcpy(&ours, InternalTypeInfo() + 8, sizeof ours);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RT_ORIGINAL_ONCE(CStrThisFn,
                           "?TypeName@Internal@Object@@UEBAPEBDXZ")(self)));
    RT_DIFF_WORK(true);
    return ours;
}

// ============================================================================
// THE POSITIVE CONTROL FOR THE GATE (b) INSTRUMENT — BANISHED_OBJTI_PROBE=1
// ============================================================================
//
// `entries=0` on all four rows is the finding this lane rests on, and a counter
// that CANNOT fire reports exactly the same thing as a counter that was never
// reached. This project has shipped an inert instrument before and read its
// silence as a result (reference_a_rule_on_the_callers_side_gets_broken.md:
// a disabled screen sat inert for months behind one unread line). So the
// instrument is scored against a subject built to make it fire.
//
// The subject is SYNTHETIC on purpose. There is no route in the game that
// dispatches these bodies — that IS the finding — so waiting for one would
// mean never running the control (reference_no_route_is_not_no_oracle.md,
// reference_synthetic_subject_beats_a_fake.md). A bare object whose +0x00 is
// stamped with each row's table is not a fake of anything: it is exactly what
// a leaf instance of that class would be, which is why the classification it
// exercises is the real one.
//
// IT SCORES TWO THINGS SEPARATELY, because a control scored on the union of
// two screens reads green while one of them is blind
// (reference_union_control_hides_a_blind_screen.md):
//   (1) does Attend() fire at all — `entries` must move by exactly one per call;
//   (2) can it TELL THE TABLES APART — the object is stamped with the port
//       door's table, so the classification must land in `minted` when the row
//       is registered and in `dll` when it is not. A counter that always said
//       `other` would pass (1) and fail (2), and (1) alone cannot see that.
// ============================================================================
namespace {
void RowAttendanceTable::RunPositiveControl() {
    const char* on = std::getenv("BANISHED_OBJTI_PROBE");
    if (!on || !*on || on[0] == '0') return;

    // SCREEN 0 — is the port registry still answering AT ALL at this point?
    // `??_7Element@UI@@6B@` is an UNCONDITIONALLY PROMOTED row
    // (standalone/reg_mint_promoted.cpp), so its two doors MUST differ on every
    // run. If they do not, the registry is gone and every "unregistered" verdict
    // printed below is an artefact of WHEN this control runs, not of the row.
    {
        void* const pEl = ResolveExport("??_7Element@UI@@6B@");
        void* const rEl = ResolveExportRaw("??_7Element@UI@@6B@");
        printf("[ObjTI] CONTROL screen0(registry-live) = %s  "
               "promoted ??_7Element@UI@@6B@ portDoor=%p rawDoor=%p\n",
               (pEl && rEl && pEl != rEl) ? "GREEN" : "RED -- doors agree on a "
               "row that is promoted on every run; the control is running after "
               "the registry",
               pEl, rEl);
    }

    struct Case { int idx; const char* label; };
    static const Case kCases[4] = {
        { 0, "Base" }, { 1, "CompiledExternal" }, { 2, "External" }, { 3, "Internal" },
    };

    for (const Case& c : kCases) {
        RowAttendance& a = row[c.idx];
        const unsigned e0 = a.entries, m0 = a.mint, d0 = a.dll, o0 = a.other;

        // A bare instance of the class: one qword of vtable is all these seven
        // bodies can read, and none of them reads `this` for its RESULT.
        void* subject[8] = { nullptr };
        subject[0] = ResolveExport(a.row);
        void* const rawTable = ResolveExportRaw(a.row);

        switch (c.idx) {
            case 0: RtObject_Base_Type(subject); break;
            case 1: RtObject_CompiledExternal_Type(subject);
                    RtObject_CompiledExternal_TypeName(subject); break;
            case 2: RtObject_External_Type(subject);
                    RtObject_External_TypeName(subject); break;
            case 3: RtObject_Internal_Type(subject);
                    RtObject_Internal_TypeName(subject); break;
        }

        const unsigned calls   = (c.idx == 0) ? 1u : 2u;
        const bool     armed   = (subject[0] != rawTable);   // is the row registered?
        const unsigned dEntry  = a.entries - e0;
        const unsigned dMint   = a.mint  - m0;
        const unsigned dDll    = a.dll   - d0;
        const unsigned dOther  = a.other - o0;

        const bool screen1 = (dEntry == calls);
        const bool screen2 = armed ? (dMint == calls) : (dDll == calls);

        printf("[ObjTI] CONTROL %-18s row=%s  calls=%u  "
               "screen1(fires)=%s d_entries=%u  "
               "screen2(discriminates)=%s d_mint=%u d_dll=%u d_other=%u  "
               "portDoor=%p rawDoor=%p\n",
               c.label, armed ? "REGISTERED(minted)" : "unregistered(DLL)",
               calls, screen1 ? "GREEN" : "RED", dEntry,
               screen2 ? "GREEN" : "RED", dMint, dDll, dOther,
               subject[0], rawTable);

        // Do not let the control's own entries pollute the measurement that was
        // already printed above; restore the counters exactly.
        a.entries = e0; a.mint = m0; a.dll = d0; a.other = o0;
    }
}
}  // namespace

// ============================================================================
// Registration — BOTH doors on every row, for the reason the mint itself
// gives: a minted vtable slot is filled BY NAME, but many sites in this tree
// reach exported code BY ADDRESS, and a name-only registration leaves those
// walking past the port into the DLL
// (reference_by_address_door_dies_at_cutover.md).
// ============================================================================
// Called from standalone/main.cpp's shutdown block, NOT from a static
// destructor. See the note above RowAttendanceTable for the measurement that
// forced that.
void RtObjectTypeInfoAccessorsReport() {
    g_attTable.Report();
}

int RtObjectTypeInfoAccessorsRegister() {
    int n = 0;

    RegisterRuntimeExport("?Type@Base@Object@@UEBAIXZ",
                          (void*)&RtObject_Base_Type); ++n;
    RegisterRuntimeAddr(0x1800c0380, (void*)&RtObject_Base_Type);

    RegisterRuntimeExport("?Type@CompiledExternal@Object@@UEBAIXZ",
                          (void*)&RtObject_CompiledExternal_Type); ++n;
    RegisterRuntimeAddr(0x18000b690, (void*)&RtObject_CompiledExternal_Type);

    RegisterRuntimeExport("?TypeName@CompiledExternal@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_CompiledExternal_TypeName); ++n;
    RegisterRuntimeAddr(0x18000b6b0, (void*)&RtObject_CompiledExternal_TypeName);

    RegisterRuntimeExport("?Type@External@Object@@UEBAIXZ",
                          (void*)&RtObject_External_Type); ++n;
    RegisterRuntimeAddr(0x1800066d0, (void*)&RtObject_External_Type);

    RegisterRuntimeExport("?TypeName@External@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_External_TypeName); ++n;
    RegisterRuntimeAddr(0x1800066c0, (void*)&RtObject_External_TypeName);

    RegisterRuntimeExport("?Type@Internal@Object@@UEBAIXZ",
                          (void*)&RtObject_Internal_Type); ++n;
    RegisterRuntimeAddr(0x1800064d0, (void*)&RtObject_Internal_Type);

    RegisterRuntimeExport("?TypeName@Internal@Object@@UEBAPEBDXZ",
                          (void*)&RtObject_Internal_TypeName); ++n;
    RegisterRuntimeAddr(0x1800064c0, (void*)&RtObject_Internal_TypeName);

    return n;
}
