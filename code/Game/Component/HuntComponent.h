#pragma once
// ============================================================================
// code/Game/Component/HuntComponent.h — ENDGAME B.3.8 block MI-B
//
// `HuntComponent` (the hunting cabin) as a real C++ class with a
// compiler-generated vtable (B38_RULES.md D1-D6). The engine/types POD is a
// forwarder.
//
// RETAIL (B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "HuntComponent", parent "ResourceProduceComponent", ti 0x1401c7890
//   create 0x140034f30: MemAlloc(0x58, 8) + ctor
//   ctor 0x1401228e0: RP ctor 0x14012b3d0; stamp; `MOV qword [RBX+0x50],0`;
//     `_flags |= 8` (opts into the per-frame Update).
//   [0] 0x140035100: restamps + ~Internal + MemFree on &1 (no body of its own).
//   table 0x140163770, 22 slots: [3] 0x140122820 Persist, [9] 0x140122740
//     Update, [18] 0x1401226a0 CreateWork; every other slot = RP's.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ResourceProduceComponent.h"
#include "Object/exe_type.h"

class HuntComponent final : public ResourceProduceComponent {
    DECLARE_EXE_TYPE(HuntComponent)

public:
    HuntComponent();                                                     // 0x1401228e0

    // 0x140035100: no body
    ~HuntComponent() override = default;                                // [0]  0x140035100
    void        Persist(void* stream) override;                         // [3]  0x140122820
    // [9] receives the System::Time BY VALUE (an 8-byte tick count in RDX); the
    // base declares the register as `void*`, so the body reads its bits.
    void        Update(void* time) override;                            // [9]  0x140122740
    uintptr_t*  CreateWork(uintptr_t* out) override;                    // [18] 0x1401226a0

    // +0x50 the hunt-progress accumulator (a System::Time, in ticks).
    void*    huntData;          // +0x50
};

static_assert(sizeof(HuntComponent) == 0x58, "HuntComponent is 0x58 (create 0x140034f30)");
static_assert(alignof(HuntComponent) == 8, "HuntComponent alignment");
static_assert(offsetof(HuntComponent, huntData) == 0x50, "HuntComponent +0x50 (ctor 0x14012290b)");
