#pragma once
// ============================================================================
// code/Game/Work/Work.h — ENDGAME B.3.8 block WRK (B38_RULES.md §2.1, D1-D5)
//
// `Work`, the exe's work-item base (a job a citizen can take: build a road,
// harvest a field, fight a fire …), as a REAL C++ class whose vtable the
// compiler generates. Global namespace, as in retail (reflected name "Work",
// parent literal "Object::Internal", abstract: create = 0x140003730).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1 / §6):
//   table 0x140161a80, 13 slots, ti 0x1401c6cf8
//   [0]  0x14002ca60 scalar deleting dtor: stamp 0x140161a80, `~Internal`,
//        `if (flags & 1) MemFree`. ~Work has no body.
//   [1]  0x14002ca50 TypeName (`MOV RAX,[ti+8]`)   [2] 0x14002ca40 Type
//   [3]  0x1400f7400 Persist (the Work base Serialize, version 3; ported in
//        work_base_reimpl.cpp; the leaves' Persist bodies call it QUALIFIED)
//   [4]  _purecall                                   (the tree's Execute)
//   [5]  0x1400f7540 `XOR AL,AL; RET` (in 22 tables) (the tree's Slot5)
//   [6]-[11] _purecall                               (OnComplete … GetPriority)
//   [12] 0x140006b10 `RET 0` (in 25 tables)
//   The shared base ctor 0x1400f72e0 (Work_Ctor25) and the default base init
//   every leaf ctor inlines are below.
//
// SLOT NAMES 4-12 have no retail symbol; they are the tree's (the old
// work_base_standalone.h). ARGUMENT LISTS: the union of the parameter lists the
// 21 leaves' C bodies declared (every C body took `self` first, i.e. the member
// ABI). The retail callers read so far: CitizenAI's work walk 0x1400f7b60
// dispatches [4] as `(work, &citizen, ctx+0x130)` and [9] with RCX only;
// the scheduler dispatches [10] as `(work, retbuf)`. The other slots' callers
// are NOT re-read (B38_RULES §6): a leaf ignores the registers it does not
// read, exactly as its C body did.
//
// LAYOUT: Object::Internal (0x10) + 0x38 = 0x48. The field names are the ones
// the leaves' PODs used (engine/types/BuildActions.h); +0x10 and +0x18 are
// InternalPtrs (Persist post-validates +0x10 as a ComponentEntity and +0x18 as
// a WorkComponent), +0x30/+0x38 are System::Time values (Persist v3+).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)

namespace WorkAddr {
    constexpr uintptr_t Vtable       = 0x140161a80;
}

class Work : public Object::Internal {
    DECLARE_EXE_TYPE(Work)

public:
    // ---- the 13 slots, in retail order -------------------------------------
    Object::Base* DeletingDtor(unsigned flags) override;             // [0]  0x14002ca60
    // [1]/[2]: the abstract base answers its own record (ENDGAME B.3.9 RF:
    // DEFINE_EXE_TYPE with retail's abstract factory); every leaf overrides both.
    // [1] TypeName 0x14002ca50 and [2] Type 0x14002ca40: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    void        Persist(void* stream) override;                       // [3]  0x1400f7400
    virtual int   Execute(void* citizen, void* context, uint64_t arg3) = 0;  // [4]  pure
    virtual int   Slot5(void* citizen, void* context);                // [5]  0x1400f7540 → 0
    virtual int   OnComplete(void* citizen, void* arg2, int arg3) = 0;// [6]  pure
    virtual int   OnCancel(void* citizen, void* arg2, int arg3) = 0;  // [7]  pure
    virtual int   Cleanup() = 0;                                      // [8]  pure
    virtual int   Validate(void* citizen) = 0;                        // [9]  pure
    // [10] the "who can do this" key (a u64 written through the hidden return
    // buffer, returned as the buffer): retail RCX = this, RDX = retbuf.
    virtual void* GetTarget(void* retbuf) = 0;                        // [10] pure
    virtual int   GetPriority() = 0;                                  // [11] pure
    virtual void  Slot12();                                           // [12] 0x140006b10 RET 0

    // ---- the 0x38 bytes Work adds, retail order ------------------------------
    // PUBLIC while unconverted bodies read them at raw offsets (decision 1).
    void*    field_10;   // +0x10 InternalPtr — the owning entity (a ComponentEntity)
    void*    field_18;   // +0x18 InternalPtr — the entity's WorkComponent
    int32_t  field_20;   // +0x20 work id (-1)
    int32_t  field_24;   // +0x24 (Persist copies +0x20 into it before v1)
    int32_t  field_28;   // +0x28 target map cell (-1)
    int32_t  field_2c;   // +0x2c
    void*    field_30;   // +0x30 System::Time
    void*    field_38;   // +0x38 System::Time
    int32_t  field_40;   // +0x40
    uint8_t  _pad44[4];  // +0x44 never written

    // The body of the shared base ctor 0x1400f72e0 MINUS its `??0Internal` call
    // and vptr store. `Work(void*)` runs it after the compiler's stamps (its
    // by-address door `Work_Ctor25_Impl` went at ENDGAME B.3.9 OBJ).
    void InitFromEntity(void* pEntityIBP);

protected:
    // The default base init every retail leaf ctor inlines after `??0Internal`
    // and the 0x140161a80 stamp: +0x10/+0x18 = 0, +0x20 = -1, +0x24 = 0,
    // +0x28 = -1, +0x2c = 0, +0x30/+0x38 = 0, +0x40 = 0 (e.g. BuildRoad_Ctor
    // 0x1400e4ad0). The vptr stores are the compiler's.
    Work();
    // 0x1400f72e0: the entity-seeded base ctor (Work_Ctor25).
    explicit Work(void* pEntityIBP);
    // No body in retail (slot 0 only re-stamps the table before ~Internal).
    ~Work() = default;

public:
    // Vtable() (DECLARE_EXE_TYPE) is the compiler's vtable (B38 D5): served at
    // 0x140161a80 while by-address readers (the unconverted dtors that re-stamp
    // the base) remain.
};

static_assert(sizeof(Work) == 0x48, "Work is 0x48 bytes (0x10 Internal + 0x38)");
static_assert(alignof(Work) == 8, "Work is 8-aligned");
static_assert(offsetof(Work, field_10) == 0x10, "Work +0x10 entity InternalPtr (Persist 0x1400f7400)");
static_assert(offsetof(Work, field_18) == 0x18, "Work +0x18 WorkComponent InternalPtr");
static_assert(offsetof(Work, field_20) == 0x20, "Work +0x20 work id, -1 (0x1400f72e0)");
static_assert(offsetof(Work, field_24) == 0x24, "Work +0x24");
static_assert(offsetof(Work, field_28) == 0x28, "Work +0x28 target cell, -1 (0x1400f72e0)");
static_assert(offsetof(Work, field_2c) == 0x2c, "Work +0x2c");
static_assert(offsetof(Work, field_30) == 0x30, "Work +0x30 System::Time (Persist v3)");
static_assert(offsetof(Work, field_38) == 0x38, "Work +0x38 System::Time (Persist v3)");
static_assert(offsetof(Work, field_40) == 0x40, "Work +0x40 (Persist v3)");

