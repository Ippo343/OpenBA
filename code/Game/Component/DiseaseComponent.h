#pragma once
// ============================================================================
// code/Game/Component/DiseaseComponent.h — ENDGAME B.3.8 block CIT
//
// `DiseaseComponent` — the PER-BUILDING crop infestation / livestock disease
// component (NOT the citizen plague: that is the `Disease` external) — as a real
// C++ class with a compiler-generated vtable (B38_RULES.md D1-D4). The
// engine/types POD is a forwarder to this file (it keeps `DiseaseDescription`'s
// POD, which is block K1's class).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "DiseaseComponent", parent "Component", ti 0x1401c4170
//   create 0x140004db0: MemAlloc(0x58, 8) + ctor 0x140048db0
//   ctor   0x140048db0: Component ctor, stamp 0x140159290, +0x28..+0x40 (4
//                       qwords) = 0, +0x48 (dword) = 0, +0x50 = 0, flags |= 0x8b
//   dtor   0x140004f80 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140159290, 16 slots:
//     [0] 0x140004f80 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140048ca0 Persist                [4] 0x140048c50 Startup
//     [6] 0x140048c00 OnAdded (registry append)
//     [7] 0x140048bb0 OnRemove (registry erase + Cure)
//     [9] 0x140048b90 Update                 every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class DiseaseComponent final : public Component {
    DECLARE_EXE_TYPE(DiseaseComponent)

public:
    DiseaseComponent();                                           // 0x140048db0

    // no own work
    ~DiseaseComponent() override = default;                       // [0] 0x140004f80
    void Persist(void* stream) override;                          // [3] 0x140048ca0
    void Startup() override;                                      // [4] 0x140048c50
    void OnAdded() override;                                      // [6] 0x140048c00
    void OnRemove() override;                                     // [7] 0x140048bb0
    void Update(void* time) override;                             // [9] 0x140048b90

    // PUBLIC while the ported bodies read them (B38 decision 1). All four
    // timers are System::Time (int64 ticks) — never narrow them to int32.
    int64_t  rollAccum;     // +0x28 time accumulated toward the next infection roll
    int64_t  duration;      // +0x30 infestation time REMAINING (>0 == infested)
    int64_t  killAccum;     // +0x38 time accumulated toward the next kill tick
    int64_t  spreadAccum;   // +0x40 time accumulated toward the next spread attempt
    int32_t  isPasture;     // +0x48 1 when the entity has a PastureComponent
    uint8_t  _pad4C[4];     // +0x4C
    uint64_t curedIdentity; // +0x50 ExternalPtr: what was growing at the last Cure
};

static_assert(sizeof(DiseaseComponent) == 0x58, "DiseaseComponent is 0x58 (create 0x140004db0)");
static_assert(alignof(DiseaseComponent) == 8, "DiseaseComponent alignment");
static_assert(offsetof(DiseaseComponent, rollAccum)     == 0x28, "");
static_assert(offsetof(DiseaseComponent, duration)      == 0x30, "");
static_assert(offsetof(DiseaseComponent, killAccum)     == 0x38, "");
static_assert(offsetof(DiseaseComponent, spreadAccum)   == 0x40, "");
static_assert(offsetof(DiseaseComponent, isPasture)     == 0x48, "");
static_assert(offsetof(DiseaseComponent, curedIdentity) == 0x50, "");
