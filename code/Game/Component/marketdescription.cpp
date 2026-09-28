// code/Game/Component/marketdescription.cpp
// MarketDescription (entity description block) — market fill-ratio parameters.
// Extends Object::External (0x20 base). Vtable 0x140159cf0. Parent "Description".
// Size 0x38. GetVersion=3 (slot +0x18 @ 0x14004ec80: mov eax,3; ret).
//
// Ctor defaults (MarketDescription_Factory):
//   float @ +0x20 = 1.0 (0x3f800000)   (global fill ratio)
//   array @ +0x28 = {ptr=0, count/cap=0 @ +0x30}  (per-resource fill ratios)
// Serialize (vtable +0x28, 0x14004ebb0), stream order:
//   float @ +0x20
//   int   count @ +0x30 -> resize +0x28 array (element stride 8: {enum @+0, float @+4})
//   per-element: enum @+0 (serializer encoding vcall +0x98), float @+4
// ToolKit fields: fill ratios (global + per-resource).
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (MarketDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159cf0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/MarketDescription.h"
#include "Component/MarketComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
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

// ctor (inlined into create 0x140008290 / createInPlace 0x140008300):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
MarketDescription::MarketDescription()
    : Description(),
      _fillRatio(1.0f),
      _ratios(nullptr),
      _ratioCount(0),
      _ratioCap(0) {}

// dtor (slot 0 0x1400083d0 minus the stamps): frees the ratio array buffer.
// The compiler then chains ~Description -> ~External.
MarketDescription::~MarketDescription() {
    if (_ratios && fn_MemFreeStatic) fn_MemFreeStatic(_ratios);
    _ratios = nullptr;
}

// [0] 0x1400083d0: ~MarketDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* MarketDescription::DeletingDtor(unsigned flags) {
    this->~MarketDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004ebb0: stream order as retail.
bool MarketDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_fillRatio, nullptr);
    // array @ +0x28 (count @ +0x30, element stride 8: {enum @+0, float @+4})
    ser.Serialize(_ratioCount, nullptr);
    int n = _ratioCount;
    if (SerStreamErr(ser) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        _ratios = buf;
        _ratioCap = n;
        if (buf) for (int i = 0; i < n; ++i) static_cast<uint64_t*>(buf)[i] = 0;
    }
    if (n > 0) {
        uint8_t* e = static_cast<uint8_t*>(_ratios);
        for (int i = 0; i < n && e; ++i, e += 8) {
            ser.Serialize(*reinterpret_cast<unsigned int*>(e + 0), nullptr);   // enum @+0
            ser.Serialize(*reinterpret_cast<float*>(e + 4), nullptr);          // float @+4
        }
    }
    return true;
}

// [6] 0x140008360: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* MarketDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) MarketDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void MarketDescription::Destruct() { DeletingDtor(0); }

// [8] 0x1400083c0: MarketComponent's type id through the hidden return slot.
Object::Type* MarketDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = MarketComponent::TypeId();
    return ret;
}

// [9] 0x140008300: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* MarketDescription::Construct() {
    return new (static_cast<void*>(this)) MarketDescription();
}

// create 0x140008290: MemAlloc(0x38, 8), then the (inlined) ctor.
void* MarketDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MarketDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MarketDescription());
}
// createInPlace 0x140008300.
void* MarketDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MarketDescription());
}

DEFINE_EXE_TYPE(MarketDescription, "Description")
