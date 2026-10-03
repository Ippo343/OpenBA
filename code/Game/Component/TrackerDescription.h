#pragma once
// ============================================================================
// code/Game/Component/TrackerDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TrackerDescription", parent "Description", ti 0x1401c4b48
//   create 0x14000d830: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x14000d890.
//   table 0x14015ac00, 10 slots:
//     [0] 0x14000d940 scalar deleting dtor   [1] 0x14000d820 TypeName   [2] 0x14000d810 Type
//     [3] 0x140059400 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x1400593c0 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x14000d8e0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000d930 GetComponentType: TrackerComponent's TypeId
//     [9] 0x14000d890 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class TrackerDescription final : public Description {
    DECLARE_EXE_TYPE(TrackerDescription)

public:
    TrackerDescription() = default;

    ~TrackerDescription() override = default;                              // [0] 0x14000d940
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140059400
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x1400593c0
    Object::External* Reconstruct() override;                              // [6] 0x14000d8e0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000d930
    virtual Object::External* Construct();                                 // [9] 0x14000d890
};

static_assert(sizeof(TrackerDescription) == 0x20, "TrackerDescription is 0x20 (create 0x14000d830 MemAlloc(0x20))");
static_assert(alignof(TrackerDescription) == 8, "TrackerDescription alignment");
