#pragma once
// ============================================================================
// code/Game/Component/HighlightComponent.h — ENDGAME B.3.8 block VIS
//
// `HighlightComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HighlightComponent", parent "Component", ti 0x1401c4410
//   create 0x140006e40: MemAlloc(0x128, 8) + ctor 0x14004c930
//   ctor   0x14004c930: Component ctor, stamp 0x140159990,
//                       Graphics::EdgeMeshInstance ctor on +0x28, +0xf8 = 0,
//                       Math::Transform ctor on +0x100, +0x11c = 0 (byte),
//                       +0x120 = -1; no flag OR (flags stay 0x80)
//   dtor   0x14004c8c0: deleting-destroy the object at +0xf8, +0xf8 = 0,
//                       ~EdgeMeshInstance(+0x28), restamp, ~Internal
//   table  0x140159990, 16 slots:
//     [0]  0x140007040 scalar deleting dtor  [1]/[2] TypeName/Type
//     [3]  0x14004c880 Persist               [8] 0x14004c850 Tick (23 B) PORT GAP
//     [12] 0x14004c870 OnEvent (10 B) PORT GAP
//     every other slot = Component's
//
// TREE DIVERGENCES kept (behaviour of the tree's table, not retail's):
//   * the embedded EdgeMeshInstance / Transform are NOT constructed here: the
//     tree zero-fills +0x28..+0x11f and the tool side (Entity_impl_c4a0,
//     tool_base_render.cpp) constructs the outline lazily; a null +0x28 vptr is
//     the "never outlined" tell OnRemove tests. The members stay byte arrays.
//   * flags are 0x82, not 0x80: bit 1 enrols the component in the
//     MarkForRemoval (0x1400c2ea0) +0x38 broadcast, and
//   * slot 7 (Component's `RET` in retail) removes a live outline from the
//     Scene at mark time — required because the standalone EditTool notices a
//     dead selection only after the entity is freed (live crash 2026-07-20).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class HighlightComponent final : public Component {
    DECLARE_EXE_TYPE(HighlightComponent)

public:
    HighlightComponent();                                         // 0x14004c930

    // PORT GAP (retail 0x14004c8c0) — B38 D2: the retail dtor deletes the
    // +0xf8 object and destroys the embedded EdgeMeshInstance; the tree never
    // did (Inert_ScalarDtor) and never constructs it here. Kept as the tree's.
    ~HighlightComponent() override = default;                     // [0]  0x140007040
    void Persist(void* stream) override;                          // [3]  0x14004c880
    void OnRemove() override;                                     // [7]  = Component's in retail; TREE DIVERGENCE
    void Tick(void* time) override;                               // [8]  0x14004c850 PORT GAP
    void OnEvent(int eventId) override;                           // [12] 0x14004c870 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint8_t  edgeMeshInstance[0xD0]; // +0x28 Graphics::EdgeMeshInstance (built lazily by the tool side)
    void*    fieldF8;                // +0xF8 (ctor 0)
    uint8_t  transform[0x1C];        // +0x100 Math::Transform
    uint8_t  highlightFlag;          // +0x11C (ctor 0)
    uint8_t  _pad11D[3];             // +0x11D
    int32_t  highlightId;            // +0x120 Scene object id of the outline (ctor -1)
    uint8_t  _pad124[4];             // +0x124
};

static_assert(sizeof(HighlightComponent) == 0x128, "HighlightComponent is 0x128 (create 0x140006e40)");
static_assert(alignof(HighlightComponent) == 8, "HighlightComponent alignment");
static_assert(offsetof(HighlightComponent, edgeMeshInstance) == 0x28, "");
static_assert(offsetof(HighlightComponent, fieldF8) == 0xF8, "");
static_assert(offsetof(HighlightComponent, transform) == 0x100, "");
static_assert(offsetof(HighlightComponent, highlightFlag) == 0x11C, "");
static_assert(offsetof(HighlightComponent, highlightId) == 0x120, "");
