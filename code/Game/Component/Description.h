#pragma once
// ============================================================================
// code/Game/Component/Description.h — ENDGAME B.3.8 (B38_RULES.md §2.4)
//
// `Description`, the exe's per-component resource base (`Object::External`),
// as a real C++ class with a compiler-generated vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1):
//   reflected "Description", parent "Object::External", ti 0x1401c72e0,
//   abstract (create/createInPlace 0x140003730 `XOR EAX,EAX; RET`).
//   table 0x1401629a0, 9 slots (B38_SLOTS; INVENTORY_A's "10" ran into the next
//   string):
//   [0] 0x140031c70 scalar deleting dtor: stamp 0x1401629a0, `~External`
//       (IAT), MemFree on &1 — no body of its own
//   [1] 0x140031c60 TypeName   [2] 0x140031c50 Type
//   [3] PURE  GetFileVersion   (External's)
//   [4] IMPORT `Object::External::Compile` (inherited)
//   [5] PURE  Serialize        [6] PURE Reconstruct   [7] PURE Destruct (External's)
//   [8] PURE  NEW in Description: every leaf returns the TypeId of the component
//       the description builds, BY VALUE through a hidden return slot —
//       WellDescription 0x14000fde0 is `MOV EAX,[0x1401c4c28]; MOV [RDX],EAX;
//       MOV RAX,RDX; RET` (RCX = this, RDX = the caller's `Object::Type`).
//       B5-12 / B7-4 shape: `Object::Type* GetComponentType(Object::Type* ret) const`.
//   Every LEAF adds slot 9, its own `Construct()` (placement-new of itself on
//   `this`; WellDescription 0x14000fd40, the same bytes as its createInPlace
//   because MSVC folded the identical bodies). Leaf tables are 10 slots.
//   Description adds no bytes: sizeof == sizeof(Object::External) == 0x20.
// ============================================================================

#include <cstdint>
#include "object/External.h"
#include "object/Type.h"

class Description : public Object::External {
public:
    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140031c70
    const char*   TypeName() const override;                      // [1] 0x140031c60
    uint32_t      Type() const override;                          // [2] 0x140031c50
    // [3] GetFileVersion, [5] Serialize, [6] Reconstruct, [7] Destruct: pure (External's)
    virtual Object::Type* GetComponentType(Object::Type* ret) const = 0;   // [8] pure

protected:
    // Retail inlines this into each leaf ctor: `??0External@Object@@` then the
    // 0x1401629a0 stamp (the compiler's).
    Description() = default;
    ~Description() = default;
};

static_assert(sizeof(Description) == 0x20, "Description adds no bytes to Object::External");
