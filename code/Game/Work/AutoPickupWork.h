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

namespace AutoPickupWorkAddr {
    constexpr uintptr_t Ctor         = 0x1400e4050;
    constexpr uintptr_t Vtable       = 0x140160fa0;
    constexpr uintptr_t RegisterType = 0x14014dc10;
    constexpr uintptr_t Factory_Abstract = 0x140003730; // shared factory (45 xrefs)
}

class AutoPickupWork : public Work {
public:
    Object::Base* DeletingDtor(unsigned flags) override;   // [0] 0x1400294e0
    const char*   TypeName() const override;               // [1] 0x1400294d0
    uint32_t      Type() const override;                   // [2] 0x1400294c0
    void          Persist(void* stream) override;          // [3] 0x1400e3f80
    int           Cleanup() override;                      // [8] 0x1400e3f30

protected:
    // 0x1400e4050 (the default init) and the entity-seeded form the component
    // CreateWork bodies use (Work_Ctor25, then the leaf stamp over this one).
    AutoPickupWork() = default;
    explicit AutoPickupWork(void* pEntityIBP) : Work(pEntityIBP) {}
    ~AutoPickupWork() = default;

public:
    static const void* Vtable();   // served at 0x140160fa0 (B38 D5)
};

static_assert(sizeof(AutoPickupWork) == 0x48, "AutoPickupWork adds no bytes to Work");
static_assert(alignof(AutoPickupWork) == 8, "AutoPickupWork is 8-aligned");
