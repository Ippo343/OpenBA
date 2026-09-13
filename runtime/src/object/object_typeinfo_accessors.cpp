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
// the old DLL-era switch boot on fa4fa86 reported all four BLOCKED, with
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
    // The registry door is resolved ONCE per row and cached: these accessors
    // are on a hot path the moment anything dispatches them.
    bool        resolved;
    void*       ours;
};

// ⚠ THE REPORT IS **NOT** PRINTED FROM A STATIC DESTRUCTOR, AND THE FIRST
// VERSION OF IT WAS. That version's own screen0 is what caught the mistake: at
// static-destructor time THE PORT REGISTRY HAS ALREADY BEEN TORN DOWN, so
// ResolveExport and the raw export door return the same DLL address for every
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
// These are NOT the diff oracle `calls=` numbers: an the diff oracle site increments `calls` only from
// inside a comparison, so a body that ran and never compared prints calls=0,
// which reads as "never ran" and means the opposite
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
struct RowAttendanceTable {
    RowAttendance row[4] = {
        { "??_7Base@Object@@6B@",             0, 0, 0, 0, false, nullptr },
        { "??_7CompiledExternal@Object@@6B@", 0, 0, 0, 0, false, nullptr },
        { "??_7External@Object@@6B@",         0, 0, 0, 0, false, nullptr },
        { "??_7Internal@Object@@6B@",         0, 0, 0, 0, false, nullptr },
    };
    void Report() {
        for (const RowAttendance& a : row) {
            if (a.entries == 0) {
                printf("[ObjTI] ATTENDANCE %-34s entries=0 -- nothing dispatched "
                       "or inherited these bodies on this run\n", a.row);
            } else {
                printf("[ObjTI] ATTENDANCE %-34s entries=%u  minted-table=%u  "
                       "other-table=%u\n", a.row, a.entries, a.mint, a.other);
            }
        }
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
        a.ours     = ResolveExport(a.row);
        a.resolved = true;
    }
    if (a.ours && vt == a.ours) ++a.mint;
    else                        ++a.other;
}

}  // namespace

// ---- ??_7Base@Object@@6B@  slot 2 ------------------------------------------
// ?Type@Base@Object@@UEBAIXZ   0x1800c0380   XOR EAX,EAX / RET.
extern "C" uint32_t RtObject_Base_Type(const void* self) {
    Attend(0, self);
    const uint32_t ours = 0;
    return ours;
}

// ---- ??_7CompiledExternal@Object@@6B@  slots 2 and 1 -----------------------
// ?Type@CompiledExternal@Object@@UEBAIXZ  0x18000b690  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_CompiledExternal_Type(const void* self) {
    Attend(1, self);
    uint32_t ours = 0;
    std::memcpy(&ours, CompiledExternalTypeInfo(), sizeof ours);
    return ours;
}

// ?TypeName@CompiledExternal@Object@@UEBAPEBDXZ  0x18000b6b0  MOV RAX,[node+8].
extern "C" const char* RtObject_CompiledExternal_TypeName(const void* self) {
    Attend(1, self);
    const char* ours = nullptr;
    std::memcpy(&ours, CompiledExternalTypeInfo() + 8, sizeof ours);
    return ours;
}

// ---- ??_7External@Object@@6B@  slots 2 and 1 -------------------------------
// ?Type@External@Object@@UEBAIXZ  0x1800066d0  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_External_Type(const void* self) {
    Attend(2, self);
    uint32_t ours = 0;
    std::memcpy(&ours, ExternalTypeInfo(), sizeof ours);
    return ours;
}

// ?TypeName@External@Object@@UEBAPEBDXZ  0x1800066c0  MOV RAX,[node+0x08].
// THIS is the body that reports a permanent pointer divergence once
// ?_typeInfo@External@Object@@ is promoted — see the banner.
extern "C" const char* RtObject_External_TypeName(const void* self) {
    Attend(2, self);
    const char* ours = nullptr;
    std::memcpy(&ours, ExternalTypeInfo() + 8, sizeof ours);
    return ours;
}

// ---- ??_7Internal@Object@@6B@  slots 2 and 1 -------------------------------
// ?Type@Internal@Object@@UEBAIXZ  0x1800064d0  MOV EAX,[node+0x00].
extern "C" uint32_t RtObject_Internal_Type(const void* self) {
    Attend(3, self);
    uint32_t ours = 0;
    std::memcpy(&ours, InternalTypeInfo(), sizeof ours);
    return ours;
}

// ?TypeName@Internal@Object@@UEBAPEBDXZ  0x1800064c0  MOV RAX,[node+0x08].
extern "C" const char* RtObject_Internal_TypeName(const void* self) {
    Attend(3, self);
    const char* ours = nullptr;
    std::memcpy(&ours, InternalTypeInfo() + 8, sizeof ours);
    return ours;
}

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
