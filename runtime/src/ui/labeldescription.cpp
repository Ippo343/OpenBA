// ============================================================================
// runtime/src/ui/labeldescription.cpp — UI::LabelDescription, the whole class
// ============================================================================
//
// ??_7LabelDescription@UI@@6B@ (retail 0x180156b30), ten slots:
//   0  deleting dtor      0x180042730  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x1800423b0  here
//   2  Type               0x180042390  here
//   3  GetFileVersion     0x1800d2950  here  (returns 0x2304)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d2820  here
//   6  Reconstruct        0x180042530  here
//   7  Destruct           0x180042560  here
//   8  CreateInstance     0x1800d26b0  here
//   9  Construct          0x180042550  here
// plus ??1LabelDescription@UI@@UEAA@XZ (0x1800d2960).
//
// DERIVES FROM UI::TextDescription, NOT directly from ElementDescription, and
// that base belongs to ANOTHER agent's batch. Three of the bodies below reach
// into it and all three do so by ADDRESS, through the registry-first door, so
// the moment TextDescription lands these call sites follow it:
//     0x18011ee30   TextDescription::Serialize        (Serialize's base call)
//     0x18011eae0   ~TextDescription                  (the dtor's TAIL JUMP —
//                                                      note: NOT ??1External)
//     0x18011ef70   the TextDescription applier       (CreateInstance)
// Nothing of TextDescription is ported here.
//
// The constructor is OUT OF LINE (`??0LabelDescription@UI@@QEAA@XZ`,
// 0x1800d29d0) and is ALREADY ported, by runtime/src/ui/ui_factory_wave28.cpp
// (LabelDescription sorts after ImageDescription, so it is in the SECOND
// factory lane's roster, not factory_ui1.cpp's).
//
// GetFileVersion is 0x2304 — the largest constant in this batch, and read from
// this class's own body. It is NOT its base's.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xb8.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtLabelDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblLabelDescription); }
const uint8_t* LabelTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@LabelDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* LabelCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0LabelDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

extern "C" uint32_t RtUi_LabelDescription_Type(const void*) {
    const uint8_t* ti = LabelTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_LabelDescription_TypeName(const void*) {
    const uint8_t* ti = LabelTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_LabelDescription_GetFileVersion(const void*) { return 0x2304; }

extern "C" void* RtUi_LabelDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                    // 180042553
    return LabelCtor(selfv);                                 // 180042559 JMP
}
extern "C" void* RtUi_LabelDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                     // 18004253e
    return LabelCtor(selfv);                                 // 180042548 JMP
}
extern "C" void RtUi_LabelDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1LabelDescription@UI@@UEAA@XZ   0x1800d2960
// ============================================================================
// Stamp own vtable, release the sprite name at +0xb0, TAIL-JUMP to
// ~TextDescription (0x18011eae0). It does NOT stamp a base vtable itself and it
// does NOT touch +0x60 — its base does both. The +0xa8 ExternalPtr is not
// released.
extern "C" void RtUi_LabelDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtLabelDescription();     // 1800d297d
    ReleaseStringField(p, 0xb0);                             // 1800d2980

    using BaseDtorFn = void (*)(void*);
    RtAddr<BaseDtorFn>(0x18011eae0ull)(p);                   // 1800d29be JMP
}

// ============================================================================
// ?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d2820
// ============================================================================
// Base is TextDescription::Serialize (0x18011ee30). THE SPRITE PAIR COMES
// FIRST, BEFORE THE SIX DWORDS — offset order would put +0x90..+0xa4 first, and
// the byte stream a .rsc parses is this order, not that one.
extern "C" bool RtUi_LabelDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x18011ee30ull)(p, stream);            // 1800d283a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0xa8, &ser);      // 1800d287b ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0xb0, &ser);      // 1800d288c System::String

    RwScalar(rw, kRwScalarUInt, p + 0x90);                   // 1800d28a3
    RwScalar(rw, kRwScalarUInt, p + 0x94);                   // 1800d28bb
    RwScalar(rw, kRwScalarInt,  p + 0x98);                   // 1800d28d3
    RwScalar(rw, kRwScalarInt,  p + 0x9c);                   // 1800d28eb
    RwScalar(rw, kRwScalarInt,  p + 0xa0);                   // 1800d2903
    RwScalar(rw, kRwScalarInt,  p + 0xa4);                   // 1800d291b

    DestroyReaderWriter(rw);                                 // 1800d2934
    return true;
}

// ============================================================================
// ?CreateInstance@LabelDescription@UI@@...   retail 0x1800d26b0
// ============================================================================
// RCX = this (RSI), RDX = the hidden return slot (R14), R8 = Dialog* (RBP).
//
// LabelElement is 0x130 and its constructor is OUT OF LINE (0x1800d2620).
// The base applier is TextDescription's (0x18011ef70) and it DOES take the
// Dialog — unlike ElementDescription::CreateInstance, which takes two args.
//
// THE SPRITE ASSIGNMENT IS GUARDED BY A CHANGE TEST, and the two halves of the
// pair are compared SEPARATELY (`CMP qword [RDI+0x100],RBX` then
// `CMP dword [RDI+0x108],EAX`) before the mark:
//     idx = sheet ? GetSpriteIndexFromName(sheet, &_spriteName) : -1
//     if (elem->+0x100 != sheet || elem->+0x108 != idx) mark the chain
//     elem->+0x100 = sheet ; elem->+0x108 = idx     <- STORED UNCONDITIONALLY,
//                                                      after the test
// and the index goes in as a QWORD read from a slot only a DWORD was written to
// (0x1800d275f writes `dword [RSP+0x30]`, 0x1800d279d reads `qword`).
//
// NOTE THE -1 ARM IS `OR EAX,0xffffffff` INTO THE REGISTER, not a store — both
// arms fall through to the same `MOV dword [RSP+0x30],EAX` at 0x1800d275f. The
// slot is written exactly once either way; the high dword still never is.
//
// The last copy is `self->+0x3c`, an ElementDescription field, into +0x128.
extern "C" void** RtUi_LabelDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0x130, 8);                          // 1800d26f2

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                    // 1800d271d
        using LabelElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<LabelElementCtorFn>(0x1800d2620ull)(raw));  // 1800d2722
    }

    void* slot = elem;                                       // 1800d272a
    using TextApplyFn = void (*)(const void*, void**, const void*);
    RtAddr<TextApplyFn>(0x18011ef70ull)(self, &slot, dialog);  // 1800d273a

    {
        void*   sheet = *reinterpret_cast<void* const*>(self + 0xa8);   // 1800d273f
        int32_t idx;
        if (sheet != nullptr) {                              // 1800d2749
            idx = GetSpriteIndexFromName(sheet, self + 0xb0);          // 1800d2755
        } else {
            idx = -1;                                        // 1800d275c OR EAX,-1
        }
        uint64_t idxSlot;                                    // [RSP+0x30]
        __builtin_memcpy(&idxSlot, &idx, sizeof idx);        // 1800d275f DWORD store

        if (*reinterpret_cast<void**>(elem + 0x100) != sheet ||         // 1800d2763
            *reinterpret_cast<int32_t*>(elem + 0x108) != idx) {         // 1800d276c
            MarkNeedsSizeCalc(elem);                                    // 1800d2780
        }
        *reinterpret_cast<void**>(elem + 0x100)    = sheet;             // 1800d2796
        *reinterpret_cast<uint64_t*>(elem + 0x108) = idxSlot;           // 1800d27a2 QWORD
    }

    static const int kSrc[6] = { 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4 };
    static const int kDst[6] = { 0x110, 0x114, 0x118, 0x11c, 0x120, 0x124 };
    for (int i = 0; i < 6; ++i)                              // 1800d27a9 .. 1800d27eb
        *reinterpret_cast<int32_t*>(elem + kDst[i]) =
            *reinterpret_cast<const int32_t*>(self + kSrc[i]);

    *reinterpret_cast<int32_t*>(elem + 0x128) =
        *reinterpret_cast<const int32_t*>(self + 0x3c);      // 1800d27f1

    *ret = elem;                                             // 1800d27fa
    return ret;
}

int RtUiLabelDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@LabelDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_LabelDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180042390ull, (void*)&RtUi_LabelDescription_Type);

    RegisterRuntimeExport("?TypeName@LabelDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LabelDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800423b0ull, (void*)&RtUi_LabelDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@LabelDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_LabelDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d2950ull, (void*)&RtUi_LabelDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LabelDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180042550ull, (void*)&RtUi_LabelDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LabelDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180042530ull, (void*)&RtUi_LabelDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@LabelDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_LabelDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180042560ull, (void*)&RtUi_LabelDescription_Destruct);

    RegisterRuntimeExport("??1LabelDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_LabelDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d2960ull, (void*)&RtUi_LabelDescription_Dtor);

    RegisterRuntimeExport("?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_LabelDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d2820ull, (void*)&RtUi_LabelDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@LabelDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_LabelDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d26b0ull, (void*)&RtUi_LabelDescription_CreateInstance);

    return n;
}
