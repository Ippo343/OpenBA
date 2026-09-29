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
// ABI), corrected from retail for [4], [9] and [12] (B3X W0 G-B). The retail
// callers read so far: CitizenAI's work walk 0x1400f7b60 and 0x1400f8620
// dispatch [4] as `(work, &citizen, ctx+0x130)` and [9] with RCX only;
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
#include "system/Value.h"      // System::Time (+0x30 / +0x38; trivial, 8 bytes)

class ComponentEntity;   // B4 WORK-a: field_10 typed (H4)
class WorkComponent;     // B4 WORK-c: field_18 typed (H4)
class CitizenComponent;
class CompleteTask;


class Work : public Object::Internal {
    DECLARE_EXE_TYPE(Work)

public:
    // ---- the 13 slots, in retail order -------------------------------------
    // No body in retail (slot 0 only re-stamps the table before ~Internal).
    ~Work() override = default;                                      // [0]  0x14002ca60
    // [1]/[2]: the abstract base answers its own record (ENDGAME B.3.9 RF:
    // DEFINE_EXE_TYPE with retail's abstract factory); every leaf overrides both.
    // [1] TypeName 0x14002ca50 and [2] Type 0x14002ca40: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    void        Persist(void* stream) override;                       // [3]  0x1400f7400
    // [4] retail passes TWO arguments: RDX = &citizen copy, R8 = scheduler
    // +0x130 (0x1400f7bdd/0x1400f7be4 and 0x1400f86c7/0x1400f86ce; R9 is never
    // set, and no body reads it -- BuildRoad 0x1400e4190 overwrites R8 unread).
    virtual int   Execute(void* citizen, void* context) = 0;          // [4]  pure
    virtual int   Slot5(void* citizen, void* context);                // [5]  0x1400f7540 → 0
    virtual int   OnComplete(void* citizen, void* arg2, int arg3) = 0;// [6]  pure
    virtual int   OnCancel(void* citizen, void* arg2, int arg3) = 0;  // [7]  pure
    virtual int   Cleanup() = 0;                                      // [8]  pure
    // [9] retail passes `this` only: 0x1400f7bc3, 0x1400f86ad, 0x1400f86f4,
    // 0x1400f82b1, 0x1400f99a4 set RCX and nothing else. Bodies (BuildRoad
    // 0x1400e40c0, Pickup 0x1400f2e20) forward the stale RDX to
    // Work::ValidateTask 0x1400f7100, which spills it to its home slot and
    // overwrites it unread (0x1400f71a4) -- ClearArea 0x1400e5b10 never sets it.
    virtual int   Validate() = 0;                                     // [9]  pure
    // [10] the "who can do this" key (a u64 written through the hidden return
    // buffer, returned as the buffer): retail RCX = this, RDX = retbuf.
    virtual void* GetTarget(void* retbuf) = 0;                        // [10] pure
    virtual int   GetPriority() = 0;                                  // [11] pure
    // [12] retail passes EDX = the serializer version (Goals_Serialize
    // 0x1400f9335..0x1400f9340: `CALL GetVersion; MOV EDX,EAX; MOV RCX,RDI;
    // CALL [RSI+0x60]`). Every body in the tree is an empty `RET 0`.
    virtual void  Slot12(int version);                                // [12] 0x140006b10 RET 0

    // ---- the 0x38 bytes Work adds, retail order ------------------------------
    // PUBLIC while unconverted bodies read them at raw offsets (decision 1).
    ComponentEntity* field_10;   // +0x10 InternalPtr — the owning entity (a ComponentEntity)
    WorkComponent* field_18;   // +0x18 InternalPtr — the entity's WorkComponent (Persist validates it)
    int32_t  field_20;   // +0x20 work id (-1)
    int32_t  field_24;   // +0x24 (Persist copies +0x20 into it before v1)
    int32_t  field_28;   // +0x28 target map cell (-1)
    int32_t  field_2c;   // +0x2c
    System::Time field_30;   // +0x30 System::Time (Persist v3, ?Serialize@Time@System@@)
    System::Time field_38;   // +0x38 System::Time (Persist v3)
    int32_t  field_40;   // +0x40
    uint8_t  _pad44[4];  // +0x44 never written

    // The body of the shared base ctor 0x1400f72e0 MINUS its `??0Internal` call
    // and vptr store. `Work(void*)` runs it after the compiler's stamps (its
    // by-address door `Work_Ctor25_Impl` went at ENDGAME B.3.9 OBJ).
    void InitFromEntity(void* pEntityIBP);

    // 0x1400f7100: is `citizen` still working this job? True when one of the
    // citizen's actions up to the running one (CharacterComponent +0x48 array,
    // +0x60 index) is a CompleteTask whose task index is this work's id (+0x20);
    // that task is written to `out` when `out` is non-null. RDX is spilled and
    // never read (0x1400f7100 stores it, 0x1400f71a4 overwrites the slot).
    bool ValidateTask(void* unusedRdx, CitizenComponent** citizen, CompleteTask** out);

protected:
    // The default base init every retail leaf ctor inlines after `??0Internal`
    // and the 0x140161a80 stamp: +0x10/+0x18 = 0, +0x20 = -1, +0x24 = 0,
    // +0x28 = -1, +0x2c = 0, +0x30/+0x38 = 0, +0x40 = 0 (e.g. BuildRoad_Ctor
    // 0x1400e4ad0). The vptr stores are the compiler's.
    Work();
    // 0x1400f72e0: the entity-seeded base ctor (Work_Ctor25).
    explicit Work(void* pEntityIBP);

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

