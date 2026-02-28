#pragma once
// ControllerComponent — ABSTRACT intermediate base for components with a secondary vtable.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Not a real class with its own constructor — this is a pattern observed in:
//   WorkPlaceComponent, StorageLocationComponent, MarketLocationComponent,
//   AbsorbResourceComponent, ResourceProduceComponent (and its derivatives).
//
// These all share the same initialization pattern after Component_Ctor:
//   1. Set vtable to ControllerComponent base (0x140162b80), then override with derived
//   2. Initialize secondary "controller vtable" sub-object at +0x28:
//      - +0x28: secondary vtable (initially purecall 0x14015f070, then real)
//      - +0x30: data field (zeroed)
//      - +0x38: data field (zeroed)
//   3. Set final secondary vtable at +0x28
//   4. Set back-pointer to self at +0x40
//
// Layout extends Component (0x28) with controller sub-object:
//   +0x28: controllerVtable (secondary vtable for controller interface)
//   +0x30: controllerData0 (zeroed)
//   +0x38: controllerData1 (zeroed)
//   +0x40: selfPtr (back-pointer to this component)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ControllerComponentAddr {
    constexpr uintptr_t BaseVtable     = 0x140162b80; // intermediate vtable (overwritten by derived)
    constexpr uintptr_t PurecallVtable = 0x14015f070; // purecall placeholder for secondary vtable
}

struct ControllerComponent {
    // Component base (0x28 bytes)
    void*    vtable;            // +0x00: derived vtable pointer
    void*    next;              // +0x08: next component in entity's component list
    void*    owner;             // +0x10: owning entity
    void*    _unk18;            // +0x18: unknown, zeroed
    uint32_t flags;             // +0x20: component flags
    uint8_t  _pad24[4];         // +0x24: padding

    // Controller sub-object (0x20 bytes)
    void*    controllerVtable;  // +0x28: secondary controller vtable
    void*    controllerData0;   // +0x30: zeroed by ctor
    void*    controllerData1;   // +0x38: zeroed by ctor
    void*    selfPtr;           // +0x40: back-pointer to this component
};

static_assert(sizeof(ControllerComponent) == 0x48, "ControllerComponent size mismatch");
static_assert(offsetof(ControllerComponent, vtable) == 0x00, "");
static_assert(offsetof(ControllerComponent, next) == 0x08, "");
static_assert(offsetof(ControllerComponent, owner) == 0x10, "");
static_assert(offsetof(ControllerComponent, flags) == 0x20, "");
static_assert(offsetof(ControllerComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ControllerComponent, controllerData0) == 0x30, "");
static_assert(offsetof(ControllerComponent, controllerData1) == 0x38, "");
static_assert(offsetof(ControllerComponent, selfPtr) == 0x40, "");
