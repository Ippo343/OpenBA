// code/Game/Component/tradedescription.cpp
// TradeDescription (entity description block) — trading post / merchant config.
// Extends Object::External (0x20 base). Vtable 0x14015ad60. Parent "Description".
// Size 0x70. GetVersion=7 (slot +0x18 @ 0x14005ca00: mov eax,7; ret).
//
// Ctor defaults (TradeDescription_sub_caa0): all fields zero except
//   float @ +0x68 = 0x3eaa7efa (~0.333)
//   int   @ +0x6c = 3
// Serialize (vtable +0x28, 0x14005c7f0) — Serializer ctor with version=1, stream order
// (verified disasm CALLs at 0x14005c7f0):
//   ExternalPtr @ +0x30  (FUN_140002340 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x50 -> resize +0x48 merchant array (element 0x48), per merchant:
//        ExternalPtr @+0x00 (FUN_14000bf40)
//        String      @+0x08
//        int count   @+0x28 -> resize ext-array @+0x20 -> Desc_LoadExternalArray
//        int count   @+0x38 -> resize int-array @+0x30 -> per-elem enum vcall +0x98
//        int         @+0x40
//        enum @+0x10, enum @+0x14, enum @+0x18 (encoding vcall +0x98)
//   int count   @ +0x60 -> resize +0x58 array (element stride 0x14), per element:
//        int @+0x00, float @+0x04, float @+0x08, int @+0x0c, int @+0x10
//   ExternalPtr @ +0x20  (FUN_14000bd30 -> fn_ExternalPtr_BuildSerialize)
//   ExternalPtr @ +0x28  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x40 -> resize +0x38 array (element stride 0x18), per element:
//        enum @+0x00 (encoding vcall +0x98), String @+0x08, String @+0x10
//   float @ +0x68
//   int   @ +0x6c
// ToolKit fields: merchants (ships / goods / prices).
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (TradeDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015ad60) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/TradeDescription.h"
#include "Component/TradeComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstring>
#include <cstdint>
#include <new>

namespace {
// The retail ctor stores raw IEEE bit patterns; this keeps them exact.
inline float FloatBits(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

namespace {
// serializer+0x00 is the IO::Stream; stream+0x08 is its error/direction word
// (0 == ok on the read leg). The tree's check before it allocates an array.
inline int SerStreamErr(IO::Serializer& ser) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&ser)) + 8);
}
}  // namespace

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);  // componentdescription.cpp

namespace {
// Allocate + zero a count*stride array into *basePtr. Returns the buffer.
void* TD_AllocArray(void* basePtr, int n, uint64_t stride) {
    void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * stride, 8);
    *(void**)basePtr = buf;
    if (buf) for (uint64_t b = 0; b < (uint64_t)n * stride; ++b) ((uint8_t*)buf)[b] = 0;
    return buf;
}
}  // namespace

// ctor (inlined into create 0x14000dbd0 / createInPlace 0x14000dc10):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
TradeDescription::TradeDescription()
    : Description(),
      _ref20(nullptr),
      _ref28(nullptr),
      _ref30(nullptr),
      _pairs(nullptr),
      _pairCount(0),
      _pairCap(0),
      _merchants(nullptr),
      _merchantCount(0),
      _merchantCap(0),
      _prices(nullptr),
      _priceCount(0),
      _priceCap(0),
      _f68(FloatBits(0x3eaa7efa)),
      _i6c(3) {}

// dtor (slot 0 0x14000dc50 minus the stamps): frees the merchant (and its inner arrays/Strings), string-pair and price arrays.
// The compiler then chains ~Description -> ~External.
TradeDescription::~TradeDescription() {
    if (fn_MemFreeStatic) {
        // merchant array (free its inner arrays + strings first; the tree walks
        // the CAPACITY field, which the load path never sets)
        int merchN = _merchantCap;
        uint8_t* m = static_cast<uint8_t*>(_merchants);
        if (m) {
            for (int i = 0; i < merchN; ++i, m += 0x48) {
                if (*(void**)(m + 0x20)) fn_MemFreeStatic(*(void**)(m + 0x20));
                if (*(void**)(m + 0x30)) fn_MemFreeStatic(*(void**)(m + 0x30));
                reinterpret_cast<System::String*>(m + 0x08)->~String();  // B.3.1 bridge
            }
            fn_MemFreeStatic(_merchants);
        }
        // string-pair array (free strings first)
        int spN = _pairCap;
        uint8_t* sp = static_cast<uint8_t*>(_pairs);
        if (sp) {
            for (int i = 0; i < spN; ++i, sp += 0x18) {
                reinterpret_cast<System::String*>(sp + 0x08)->~String();  // B.3.1 bridge
                reinterpret_cast<System::String*>(sp + 0x10)->~String();  // B.3.1 bridge
            }
            fn_MemFreeStatic(_pairs);
        }
        if (_prices) fn_MemFreeStatic(_prices);
    }
    _merchants = nullptr;
    _pairs = nullptr;
    _prices = nullptr;
}

// [0] 0x14000dc50: ~TradeDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* TradeDescription::DeletingDtor(unsigned flags) {
    this->~TradeDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14005c7f0: stream order as retail.
bool TradeDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize || !fn_MemAlloc || !g_pMemAllocSingleton) return true;   // standalone: unwired doors
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_ref30, &ser);   // ExternalPtr +0x30
    
    // merchant array @ +0x48 (count @ +0x50, element stride 0x48)
    ser.Serialize(_merchantCount, nullptr);
    int merchN = _merchantCount;
    if (SerStreamErr(ser) == 0 && merchN > 0) TD_AllocArray(&_merchants, merchN, 0x48);
    if (merchN > 0) {
        uint8_t* m = static_cast<uint8_t*>(_merchants);
        for (int i = 0; i < merchN && m; ++i, m += 0x48) {
            fn_ExternalPtr_BuildSerialize(m + 0x00, &ser);                      // ExternalPtr
            reinterpret_cast<System::String*>(m + 0x08)->Serialize(&ser);      // B.3.1 bridge: String
            // ext-ref array @ m+0x20 (count @ m+0x28, stride 8)
            ser.Serialize(*reinterpret_cast<int*>(m + 0x28), nullptr);
            int extN = *(int32_t*)(m + 0x28);
            if (SerStreamErr(ser) == 0 && extN > 0) TD_AllocArray(m + 0x20, extN, 8);
            Desc_LoadExternalArray(&ser, extN, *(void**)(m + 0x20));
            // int array @ m+0x30 (count @ m+0x38, stride 4) per-elem enum vcall +0x98
            ser.Serialize(*reinterpret_cast<int*>(m + 0x38), nullptr);
            int intN = *(int32_t*)(m + 0x38);
            if (SerStreamErr(ser) == 0 && intN > 0) TD_AllocArray(m + 0x30, intN, 4);
            uint8_t* ia = *(uint8_t**)(m + 0x30);
            for (int j = 0; j < intN && ia; ++j, ia += 4)
                ser.Serialize(*reinterpret_cast<unsigned int*>(ia), nullptr);
            ser.Serialize(*reinterpret_cast<int*>(m + 0x40), nullptr);           // int @+0x40
            ser.Serialize(*reinterpret_cast<unsigned int*>(m + 0x10), nullptr);  // enum @+0x10
            ser.Serialize(*reinterpret_cast<unsigned int*>(m + 0x14), nullptr);  // enum @+0x14
            ser.Serialize(*reinterpret_cast<unsigned int*>(m + 0x18), nullptr);  // enum @+0x18
        }
    }
    
    // price array @ +0x58 (count @ +0x60, element stride 0x14)
    ser.Serialize(_priceCount, nullptr);
    int priceN = _priceCount;
    if (SerStreamErr(ser) == 0 && priceN > 0) TD_AllocArray(&_prices, priceN, 0x14);
    if (priceN > 0) {
        uint8_t* p = static_cast<uint8_t*>(_prices);
        for (int i = 0; i < priceN && p; ++i, p += 0x14) {
            ser.Serialize(*reinterpret_cast<int*>(p + 0x00), nullptr);
            ser.Serialize(*reinterpret_cast<float*>(p + 0x04), nullptr);
            ser.Serialize(*reinterpret_cast<float*>(p + 0x08), nullptr);
            ser.Serialize(*reinterpret_cast<int*>(p + 0x0c), nullptr);
            ser.Serialize(*reinterpret_cast<int*>(p + 0x10), nullptr);
        }
    }
    
    fn_ExternalPtr_BuildSerialize(&_ref20, &ser);   // ExternalPtr +0x20
    fn_ExternalPtr_BuildSerialize(&_ref28, &ser);   // ExternalPtr +0x28
    
    // string-pair array @ +0x38 (count @ +0x40, element stride 0x18)
    ser.Serialize(_pairCount, nullptr);
    int spN = _pairCount;
    if (SerStreamErr(ser) == 0 && spN > 0) TD_AllocArray(&_pairs, spN, 0x18);
    if (spN > 0) {
        uint8_t* sp = static_cast<uint8_t*>(_pairs);
        for (int i = 0; i < spN && sp; ++i, sp += 0x18) {
            ser.Serialize(*reinterpret_cast<unsigned int*>(sp + 0x00), nullptr);   // enum @+0x00
            reinterpret_cast<System::String*>(sp + 0x08)->Serialize(&ser);        // B.3.1 bridge: String @+0x08
            reinterpret_cast<System::String*>(sp + 0x10)->Serialize(&ser);        // B.3.1 bridge: String @+0x10
        }
    }
    ser.Serialize(_f68, nullptr);
    ser.Serialize(_i6c, nullptr);
    return true;
}

// [6] 0x14000dc20: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* TradeDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) TradeDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void TradeDescription::Destruct() { DeletingDtor(0); }

// [8] 0x14000dc40: TradeComponent's type id through the hidden return slot.
Object::Type* TradeDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = TradeComponent::TypeId();
    return ret;
}

// [9] 0x14000dc10: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* TradeDescription::Construct() {
    return new (static_cast<void*>(this)) TradeDescription();
}

// create 0x14000dbd0: MemAlloc(0x70, 8), then the (inlined) ctor.
void* TradeDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TradeDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TradeDescription());
}
// createInPlace 0x14000dc10.
void* TradeDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TradeDescription());
}

DEFINE_EXE_TYPE(TradeDescription, "Description")
