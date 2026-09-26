#pragma once
// ============================================================================
// code/Game/Component/WellDescription.h — ENDGAME B.3.8 pilot (B38_RULES.md D1-D5)
//
// The first exe `Description` leaf with a compiler-generated vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "WellDescription", parent "Description", ti 0x1401c4c60
//   create 0x14000fce0: MemAlloc(0x20, 8); `??0External@Object@@`; stamp
//     0x1401629a0; stamp 0x14015af20 (the ctor 0x14005d200 inlined)
//   createInPlace 0x14000fd40: the same on `storage`, null-checked
//   dtor 0x14005d1c0: stamp 0x14015af20, stamp 0x1401629a0, tail-JMP ~External
//   table 0x14015af20, 10 slots:
//     [0] 0x14000fdf0 scalar deleting dtor (the dtor + MemFree on &1)
//     [1] 0x14000fcd0 TypeName  [2] 0x14000fcc0 Type
//     [3] 0x14005d1b0 GetFileVersion `MOV EAX,1`
//     [4] Object::External::Compile (import)
//     [5] 0x14005d170 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x14000fd90 Reconstruct: `CALL [vptr+0] (this, 0)` (destroy in place),
//         then the ctor on `this` again, returns this
//     [7] 0x140001640 Destruct: `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` —
//         DeletingDtor(0); the one shared body in 119 tables
//     [8] 0x14000fde0 GetComponentType: WellComponent's TypeId through the
//         hidden return slot (Description.h)
//     [9] 0x14000fd40 Construct: placement-new on `this` (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class WellDescription final : public Description {
    DECLARE_EXE_TYPE(WellDescription)

public:
    WellDescription() = default;                                   // 0x14005d200

    Object::Base*  DeletingDtor(unsigned flags) override;          // [0] 0x14000fdf0
    int32_t        GetFileVersion() const override { return 1; }   // [3] 0x14005d1b0
    bool           Serialize(void* stream) override;               // [5] 0x14005d170
    Object::External* Reconstruct() override;                      // [6] 0x14000fd90
    void           Destruct() override;                            // [7] 0x140001640
    Object::Type*  GetComponentType(Object::Type* ret) const override;   // [8] 0x14000fde0
    virtual Object::External* Construct();                         // [9] 0x14000fd40

protected:
    ~WellDescription() = default;                                  // 0x14005d1c0
};

static_assert(sizeof(WellDescription) == 0x20, "WellDescription is 0x20 (create 0x14000fce0 MemAlloc(0x20))");
