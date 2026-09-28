#pragma once
// ============================================================================
// code/Game/Dialog/ControlUIConfig.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// The config resource behind every generic ControlUI button (pasture
// work/split/empty, any .rsc button bound to a component command).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ControlUIConfig", parent "UI::DialogControllerConfig",
//   ti 0x1401c4e20, table 0x14015b3e0, 9 slots, sizeof 0x28.
//   create 0x140010cd0: MemAlloc(0x28, 8), ??0DialogControllerConfig, stamp,
//     ONE qword zero store at +0x20 (covers both fields).
//   createInPlace 0x140010d30 = slot 8 (the same body, no alloc).
//   [0] 0x140010dd0 scalar deleting dtor   [1] 0x140010cc0 TypeName
//   [2] 0x140010cb0 Type                   [3] 0x14005fbd0 GetFileVersion -> 1
//   [4] External::Compile (IMPORT, inherited — not declared)
//   [5] 0x14005fb70 Serialize   [6] 0x140010d80 Reconstruct
//   [7] 0x140001640 Destruct (= `~T()` (slot 0, flags 0))   [8] 0x140010d30 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogControllerConfig.h"
#include "Object/exe_type.h"

class ControlUIConfig final : public UI::DialogControllerConfig {
    DECLARE_EXE_TYPE(ControlUIConfig)

public:
    ControlUIConfig() = default;                                   // 0x140010cd0 (inlined)

    ~ControlUIConfig() override = default;                         // [0] 0x140010dd0
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x14005fbd0
    bool              Serialize(void* stream) override;            // [5] 0x14005fb70
    Object::External* Reconstruct() override;                      // [6] 0x140010d80
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x140010d30

    // +0x20 COMMAND id handed to the target component's slot 13/14 (pasture:
    // work 2, split 1, empty 0). +0x24 the TARGET component's Object::Type.
    int32_t  _command       = 0;                                   // +0x20
    uint32_t _componentType = 0;                                   // +0x24
};

static_assert(sizeof(ControlUIConfig) == 0x28, "ControlUIConfig is 0x28 (create 0x140010cd0 MemAlloc(0x28))");
static_assert(alignof(ControlUIConfig) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ControlUIConfig, _command) == 0x20, "command @ +0x20 (Serialize 0x14005fb70)");
static_assert(offsetof(ControlUIConfig, _componentType) == 0x24, "target type @ +0x24 (Serialize 0x14005fb70)");
