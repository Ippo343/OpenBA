#pragma once
// ============================================================================
// code/Game/Work/AutoPickupWork.h — ENDGAME B.3.8 block WRK (B38_RULES.md §2.1)
//
// `AutoPickupWork`, the abstract Work subclass of the six gatherer works
// (ForesterWork, HarvestArea, HarvestOrchard, HerbalistWork, HuntWork,
// PastureWork): a work whose produce is carried off by the building's
// AutoPickupComponent. Reflected "AutoPickupWork", parent literal "Work",
// abstract (create = 0x140003730). Adds no bytes.
//
// RETAIL (B38_SLOTS.md §1):
//   ctor 0x1400e4050: `??0Internal`, stamp 0x140161a80, the Work base init,
//        stamp 0x140160fa0
//   table 0x140160fa0, 13 slots, ti 0x1401c6828
//   [0]  0x1400294e0 scalar deleting dtor: stamp 0x140160fa0, stamp
//        0x140161a80, `~Internal`, MemFree on &1 — no body of its own
//   [1]  0x1400294d0 TypeName   [2] 0x1400294c0 Type
//   [3]  0x1400e3f80 Persist: CALL 0x1400f7400 (Work::Persist, a direct call),
//        then one EMPTY Serializer(stream, 0) scope
//   [8]  0x1400e3f30 Cleanup: frees the AutoPickupComponent's item list
//   [4]-[7], [9]-[12] = Work's (pure / 0x1400f7540 / 0x140006b10)
// ============================================================================

#include <cstdint>
#include "Work/Work.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)


class AutoPickupWork : public Work {
    DECLARE_EXE_TYPE(AutoPickupWork)

public:
    ~AutoPickupWork() override = default;                  // [0] 0x1400294e0
    // [1] TypeName 0x1400294d0 and [2] Type 0x1400294c0: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    void          Persist(void* stream) override;          // [3] 0x1400e3f80
    int           Cleanup() override;                      // [8] 0x1400e3f30

protected:
    // 0x1400e4050 (the default init) and the entity-seeded form the component
    // CreateWork bodies use (Work_Ctor25, then the leaf stamp over this one).
    AutoPickupWork() = default;
    explicit AutoPickupWork(void* pEntityIBP) : Work(pEntityIBP) {}

public:
    // Vtable() (DECLARE_EXE_TYPE): served at 0x140160fa0 (B38 D5)
};

static_assert(sizeof(AutoPickupWork) == 0x48, "AutoPickupWork adds no bytes to Work");
static_assert(alignof(AutoPickupWork) == 8, "AutoPickupWork is 8-aligned");
