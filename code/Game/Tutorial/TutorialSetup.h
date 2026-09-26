#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialSetup.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// The abstract tutorial setup: game state a TutorialStep puts in place while
// it runs (camera, speed, restrictions, a dialog).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md sections 1 and 6):
//   reflected "TutorialSetup", parent "Object::External", ti 0x1401c66a0,
//   abstract (create/createInPlace 0x140003730); the registration stays
//   REGISTER_ABSTRACT_FACTORY in factory_registrations_bulk.cpp (D4).
//   table 0x140160c30, 11 slots:
//     [0] 0x140028390 scalar deleting dtor: stamp, ~External, MemFree on &1
//     [1] 0x140028380 TypeName  [2] 0x140028370 Type
//     [3] 0x1400e2040 GetFileVersion `MOV EAX,1`
//     [4] Object::External::Compile (import) — inherited
//     [5] 0x1400e2000 Serialize: an EMPTY Serializer(stream, 0) scope, MOV AL,1
//     [6] Reconstruct PURE  [7] Destruct PURE (External's)
//     [8] PURE Begin   — TutWave_RefreshStepUI calls `(this)` when a step opens
//     [9] PURE Update  — TutWave_StepExecute (0x1400e1db0) calls `(this, time)`
//     [10] PURE End    — StepExecute calls `(this)` when the step completes
//     (names ours; the exe has no symbols)
//   Every leaf adds slot 11, its own Construct(). sizeof == External's 0x20.
// ============================================================================

#include <cstdint>
#include "object/External.h"

class TutorialSetup : public Object::External {
public:
    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140028390
    const char*   TypeName() const override;                       // [1] 0x140028380
    uint32_t      Type() const override;                           // [2] 0x140028370
    int32_t       GetFileVersion() const override { return 1; }    // [3] 0x1400e2040
    bool          Serialize(void* stream) override;                // [5] 0x1400e2000
    // [6] Reconstruct, [7] Destruct: pure (External's)
    virtual void  Begin() = 0;                                     // [8] pure
    virtual void  Update(void* time) = 0;                          // [9] pure
    virtual void  End() = 0;                                       // [10] pure

protected:
    // Retail inlines this into each leaf ctor: `??0External@Object@@` then the
    // 0x140160c30 stamp (the compiler's).
    TutorialSetup() = default;
    ~TutorialSetup() = default;
};

static_assert(sizeof(TutorialSetup) == 0x20, "TutorialSetup adds no bytes to Object::External");
