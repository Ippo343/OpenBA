// code/Game/Component/consumeproducedescription.cpp
// ConsumeProduceDescription (entity description block) — production-building recipe.
// Extends Object::External (0x20 base). Vtable 0x140162b30. Parent "Description".
// Size 0x58. GetVersion=12 (slot +0x18 @ 0x140119200: mov eax,0xC; ret).
//
// Ctor defaults (ConsumeProduceDescription_sub_9270):
//   ExternalPtr  @ +0x20 = 0
//   array        @ +0x28 = {ptr=0, count/cap=0 @ +0x30}   (recipe entries)
//   Type         @ +0x38 = 0   (tool typeId; serialized as a name)
//   int          @ +0x3c = 0
//   int/enum     @ +0x40 = 1
//   float        @ +0x44 = 4.0 (0x40800000)
//   int          @ +0x48 = -1  (0xffffffff)
//   int          @ +0x4c = 0
//   int/enum     @ +0x50 = 0xb (11)
// Serialize (vtable +0x28, 0x1401190e0), stream order (verified disasm CALLs):
//   ExternalPtr @ +0x20  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x30 -> resize +0x28 outer array (stride 0x28), per outer element:
//        inner int count @+0x08 -> resize inner array @+0x00 (stride 0x10):
//             per inner element: ExternalPtr @+0x00 (FUN_140002340), int @+0x08
//        ExternalPtr @+0x10 (FUN_140002340)
//        enum @+0x18 (encoding vcall +0x98)
//        String @+0x20
//   int   @ +0x40
//   enum  @ +0x50 (encoding vcall +0x98)
//   Type  @ +0x38 (Object::Type::Serialize, dispatch slot 0x1401516b0)
//   int   @ +0x3c
//   float @ +0x44
//   int   @ +0x48
//   int   @ +0x4c
// ToolKit fields: inputs / outputs / work time / tool.
// Decompiled READ-ONLY from Application-x64.exe.
//
// NOTE for orchestrator: the Type field @ +0x38 uses Object::Type::Serialize via
// IO::Serializer dispatch slot 0x1401516b0. We resolve that slot inline below; it
// is filled from Runtime-x64.dll at startup (same mechanism as g_fn_String_Serialize
// = slot 0x140151238). If it is unfilled in standalone the Type read is skipped,
// which would desync the stream — register/wire slot 0x1401516b0 if needed.
//
// ENDGAME B.3.8 K1: a REAL class (ConsumeProduceDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140162b30) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ConsumeProduceDescription.h"
#include "Component/ConsumeProduceComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include <cstdint>
#include <new>

namespace {
// serializer+0x00 is the IO::Stream; stream+0x08 is its error/direction word
// (0 == ok on the read leg). The tree's check before it allocates an array.
inline int SerStreamErr(IO::Serializer& ser) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&ser)) + 8);
}
}  // namespace

// ctor (inlined into create 0x140032360 / createInPlace 0x1400323a0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ConsumeProduceDescription::ConsumeProduceDescription()
    : Description(),
      _ref20(nullptr),
      _recipes(nullptr),
      _recipeCount(0),
      _recipeCap(0),
      _toolType(0),
      _i3c(0),
      _i40(1),
      _f44(4.0f),
      _i48(-1),
      _i4c(0),
      _u50(0xb) {}

// dtor (slot 0 0x1400323e0 minus the stamps): frees each recipe's inner array and String, then the recipe array.
// The compiler then chains ~Description -> ~External.
ConsumeProduceDescription::~ConsumeProduceDescription() {
    // free inner arrays then the outer recipe array (the tree walks the CAPACITY)
    int outerN = _recipeCap;
    uint8_t* e = static_cast<uint8_t*>(_recipes);
    if (e && fn_MemFreeStatic) {
        for (int i = 0; i < outerN; ++i, e += 0x28) {
            void* inner = *(void**)(e + 0x00);
            if (inner) fn_MemFreeStatic(inner);
            reinterpret_cast<System::String*>(e + 0x20)->~String();  // B.3.1 bridge
        }
        fn_MemFreeStatic(_recipes);
    }
    _recipes = nullptr;
}

// [5] 0x1401190e0: stream order as retail.
bool ConsumeProduceDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize || !fn_MemAlloc || !g_pMemAllocSingleton) return true;   // standalone: unwired doors
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_ref20, &ser);
    // outer recipe array @ +0x28 (count @ +0x30, element stride 0x28)
    ser.Serialize(_recipeCount, nullptr);
    int outerN = _recipeCount;
    if (SerStreamErr(ser) == 0 && outerN > 0) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)outerN * 0x28, 8);
        _recipes = buf;
        _recipeCap = outerN;
        if (buf) for (uint64_t b = 0; b < (uint64_t)outerN * 0x28; ++b) ((uint8_t*)buf)[b] = 0;
    }
    if (outerN > 0) {
        uint8_t* e = static_cast<uint8_t*>(_recipes);
        for (int i = 0; i < outerN && e; ++i, e += 0x28) {
            // inner array @ e+0x00 (count @ e+0x08, element stride 0x10)
            ser.Serialize(*reinterpret_cast<int*>(e + 0x08), nullptr);
            int innerN = *(int32_t*)(e + 0x08);
            if (SerStreamErr(ser) == 0 && innerN > 0) {
                void* ibuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)innerN * 0x10, 8);
                *(void**)(e + 0x00) = ibuf;
                if (ibuf) for (uint64_t b = 0; b < (uint64_t)innerN * 0x10; ++b) ((uint8_t*)ibuf)[b] = 0;
            }
            uint8_t* ie = *(uint8_t**)(e + 0x00);
            for (int j = 0; j < innerN && ie; ++j, ie += 0x10) {
                fn_ExternalPtr_BuildSerialize(ie + 0x00, &ser); // ExternalPtr
                ser.Serialize(*reinterpret_cast<int*>(ie + 0x08), nullptr);
            }
            fn_ExternalPtr_BuildSerialize(e + 0x10, &ser);      // ExternalPtr +0x10
            ser.Serialize(*reinterpret_cast<unsigned int*>(e + 0x18), nullptr);   // enum +0x18
            reinterpret_cast<System::String*>(e + 0x20)->Serialize(&ser);        // B.3.1 bridge: String +0x20
        }
    }
    ser.Serialize(_i40, nullptr);
    ser.Serialize(_u50, nullptr);
    {
        // Type @ +0x38 via Object::Type::Serialize dispatch slot 0x1401516b0
        uintptr_t slotAddr = ResolveAddr(Module::Exe, 0x1401516b0);
        typedef void (*TypeSer_t)(void* typeField, void* serializer);
        TypeSer_t fnTypeSer = slotAddr ? *reinterpret_cast<TypeSer_t*>(slotAddr) : nullptr;
        if (fnTypeSer) fnTypeSer(&_toolType, &ser);
        // else: slot unwired in standalone — Type read skipped (would desync)
    }
    ser.Serialize(_i3c, nullptr);
    ser.Serialize(_f44, nullptr);
    ser.Serialize(_i48, nullptr);
    ser.Serialize(_i4c, nullptr);
    return true;
}

// [6] 0x1400323b0: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* ConsumeProduceDescription::Reconstruct() {
    this->~ConsumeProduceDescription();
    return new (static_cast<void*>(this)) ConsumeProduceDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void ConsumeProduceDescription::Destruct() { this->~ConsumeProduceDescription(); }

// [8] 0x1400323d0: ConsumeProduceComponent's type id through the hidden return slot.
Object::Type* ConsumeProduceDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ConsumeProduceComponent::TypeId();
    return ret;
}

// [9] 0x1400323a0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ConsumeProduceDescription::Construct() {
    return new (static_cast<void*>(this)) ConsumeProduceDescription();
}

// create 0x140032360: MemAlloc(0x58, 8), then the (inlined) ctor.
void* ConsumeProduceDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeProduceDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeProduceDescription());
}
// createInPlace 0x1400323a0.
void* ConsumeProduceDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeProduceDescription());
}

DEFINE_EXE_TYPE(ConsumeProduceDescription, "Description")
