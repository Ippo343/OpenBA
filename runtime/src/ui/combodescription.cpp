// ============================================================================
// runtime/src/ui/combodescription.cpp — UI::ComboDescription, the whole class
// ============================================================================
//
// ??_7ComboDescription@UI@@6B@ (retail 0x180159090), ten slots:
//   0  deleting dtor      0x180074e70  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x180074ae0  here
//   2  Type               0x180074ac0  here
//   3  GetFileVersion     0x1801129b0  here  (returns 0x2104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1801129c0  here
//   6  Reconstruct        0x180074cc0  here
//   7  Destruct           0x180074d70  here
//   8  CreateInstance     0x180112a70  here
//   9  Construct          0x180074d20  here
// plus ??1ComboDescription@UI@@UEAA@XZ (0x180112370).
//
// DERIVES FROM UI::ButtonDescription, exactly as CheckDescription does, and
// reaches the same three places in runtime/src/ui/buttondescription.cpp:
// ??0ButtonDescription (0x180110b10), ButtonDescription::Serialize
// (0x1801113e0) and the shared applier (0x1801108d0). There is no out-of-line
// ??0ComboDescription; the three stores it adds are open-coded in Construct.
//
// ----------------------------------------------------------------------------
// WHAT IS **NOT** PORTED HERE, AND WHY
// ----------------------------------------------------------------------------
// `CreateInstance` calls six functions that belong to other classes, and all
// six are reached through the port-aware door rather than copied:
//   0x180035020  UI::String::operator System::String   PORTED (ui/sprite.cpp)
//   0x180034860  Array<System::String>::SetSize(n,fill) PORTED (collection/array.cpp
//                kSpec_StringRef)
//   0x18004bfc0  Array<{ptr,int}>::SetSize(n,fill)      PORTED (collection/array.cpp)
//   0x180024fe0  Array<void*>::SetSize(n,fill)          PORTED (collection/array.cpp)
//   0x180120f20  UI::UserInterface::CloseDialog         PORTED (ui/userinterface_close.cpp)
//   0x180112180  a UI::ComboElement member              NOT ported — it belongs
//                to ComboElement, not to this class, and porting it here would
//                give that class's body two owners. It is reached by address,
//                which is registry-first, so the moment ComboElement lands this
//                call site follows it.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xb8.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtComboDescription()  { return RT_DATA_SLOT(void*, RtUI::kVtblComboDescription); }
void* VtButtonDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblButtonDescription); }
const uint8_t* ComboTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ComboDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* ButtonCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0ButtonDescription@UI@@QEAA@XZ")(self);
}

// The four stores ComboDescription adds on top of its base's constructor,
// transcribed from 0x180074d42..0x180074d63.
void ConstructTail(uint8_t* p) {
    *reinterpret_cast<void**>(p)           = VtComboDescription();  // 180074d49
    *reinterpret_cast<uint64_t*>(p + 0xa0) = 0;                     // 180074d4e
    *reinterpret_cast<uint64_t*>(p + 0xa8) = 0;                     // 180074d55
    *reinterpret_cast<uint64_t*>(p + 0xb0) = 0;                     // 180074d5c
}

}  // namespace

extern "C" uint32_t RtUi_ComboDescription_Type(const void*) {
    const uint8_t* ti = ComboTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ComboDescription_TypeName(const void*) {
    const uint8_t* ti = ComboTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ComboDescription_GetFileVersion(const void*) { return 0x2104; }

extern "C" void* RtUi_ComboDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                   // 180074d3a
    uint8_t* p = static_cast<uint8_t*>(selfv);
    ButtonCtor(p);                                          // 180074d3c
    ConstructTail(p);
    return p;                                               // 180074d63
}
extern "C" void* RtUi_ComboDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                    // 180074cd7
    uint8_t* p = static_cast<uint8_t*>(selfv);
    ButtonCtor(p);                                          // 180074ce1
    ConstructTail(p);
    return p;                                               // 180074d08
}
extern "C" void RtUi_ComboDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1ComboDescription@UI@@UEAA@XZ   0x180112370
// ============================================================================
extern "C" void RtUi_ComboDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtComboDescription();                              // 18011238d
    *v = VtButtonDescription();                             // 180112397
    *v = VtElementDescription();                            // 1801123a1
    ReleaseStringField(p, 0x60);                            // 1801123a4

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1801123d6 JMP
}

// ============================================================================
// ?Serialize@ComboDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801129c0
// ============================================================================
// Base is ButtonDescription::Serialize. Two ExternalPtr<ElementDescription>
// then ONE ExternalPtr<StringTable> — a DIFFERENT serializer (0x1800042a0, not
// 0x180038f90), which is how the +0xb0 field's type is known.
extern "C" bool RtUi_ComboDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x1801113e0ull)(p, stream);           // 1801129da

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0xa0, &ser);     // 180112a1b
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0xa8, &ser);     // 180112a2c
    RtAddr<FieldSerFn>(0x1800042a0ull)(p + 0xb0, &ser);     // 180112a3d

    DestroyReaderWriter(rw);                                // 180112a55
    return true;
}

// ============================================================================
// ?CreateInstance@ComboDescription@UI@@...   retail 0x180112a70
// ============================================================================
// RCX = this (RBP), RDX = the hidden return slot (RSI), R8 = Dialog* (R14).
//
// ComboElement is 0x128 and its constructor is OUT OF LINE
// (`??0ComboElement@UI@@QEAA@XZ`, 0x1801123f0, already ported by
// runtime/src/ui/factory_ui1.cpp); retail takes its RETURN value as the element.
//
// After the shared ButtonDescription applier, the body POPULATES THE COMBO FROM
// ITS STRING TABLE: for every entry i of `self->_stringTable` it builds a
// `UI::String{table, i}` ON THE STACK, converts it to a System::String, and
// pushes one entry into each of THREE parallel element arrays:
//     +0xe8 / +0xf0   Array<System::String>   the label          (0x180034860)
//     +0xf8 / +0x100  Array<{ptr,int}>        the sprite, {0,-1} (0x18004bfc0)
//     +0x108 / +0x110 Array<void*>            the user data, 0   (0x180024fe0)
// and then releases the temporary string with retail's usual asymmetry.
//
// THREE TRANSCRIPTION FACTS, none of them inferable from the C:
//   1. `self->_stringTable` is RE-READ at the BOTTOM of every iteration to
//      recompute the loop bound (`MOV RAX,[RBP+0xb0]` at 0x180112c08). The
//      bound is not hoisted.
//   2. THE WHOLE TAIL — the CloseDialog and the +0x118 reset — IS INSIDE the
//      `if (_stringTable != nullptr)` arm. The `JZ 0x180112c5c` at 0x180112b37
//      jumps past all of it, straight to `*ret = elem`.
//   3. The three fills are three DIFFERENT stack objects and one of them, the
//      {0, -1} pair at [RSP+0x38], is built INSIDE the loop even though it is
//      loop-invariant.
extern "C" void** RtUi_ComboDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0x128, 8);                         // 180112ab3

    uint8_t* elem = nullptr;                                // R12 == 0
    if (raw != nullptr) {                                   // 180112ae4
        using ComboElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<ComboElementCtorFn>(0x1801123f0ull)(raw));  // 180112ae9
    }

    void* slot = elem;                                      // 180112af6  [RSP+0x90]
    using ApplyFn = void (*)(const void*, void**, const void*);
    RtAddr<ApplyFn>(0x1801108d0ull)(self, &slot, dialog);   // 180112b0c

    *reinterpret_cast<void**>(elem + 0xd8) =
        *reinterpret_cast<void* const*>(self + 0xa0);       // 180112b18 QWORD
    *reinterpret_cast<void**>(elem + 0xe0) =
        *reinterpret_cast<void* const*>(self + 0xa8);       // 180112b26 QWORD

    const uint8_t* table = *reinterpret_cast<const uint8_t* const*>(self + 0xb0);
    if (table != nullptr) {                                 // 180112b37
        int32_t i = 0;
        if (*reinterpret_cast<const int32_t*>(table + 0x28) > 0) {   // 180112b40
            void* userDataFill = nullptr;                   // 180112b4a  [RSP+0x98]
            do {
                // UI::String{ StringTable*, index } built in place, then
                // converted; the System::String lands in the SAME slot the
                // applier used, which is why it has to be one variable.
                struct UiString { const void* table; int32_t index; int32_t _pad; };
                UiString key = { table, i, 0 };             // [RSP+0x28]/[RSP+0x30]
                using ToSysStringFn = void** (*)(const void*, void**);
                RtAddr<ToSysStringFn>(0x180035020ull)(&key, &slot);  // 180112b68

                struct SpriteFill { void* sheet; int32_t index; int32_t _pad; };
                SpriteFill spriteFill = { nullptr, -1, 0 }; // 180112b6e / 180112b73

                using FillSetSizeFn = void (*)(void*, int, const void*);
                RtAddr<FillSetSizeFn>(0x180034860ull)(
                    elem + 0xe8, *reinterpret_cast<int32_t*>(elem + 0xf0) + 1,
                    &slot);                                 // 180112b92
                RtAddr<FillSetSizeFn>(0x18004bfc0ull)(
                    elem + 0xf8, *reinterpret_cast<int32_t*>(elem + 0x100) + 1,
                    &spriteFill);                           // 180112bab
                RtAddr<FillSetSizeFn>(0x180024fe0ull)(
                    elem + 0x108, *reinterpret_cast<int32_t*>(elem + 0x110) + 1,
                    &userDataFill);                         // 180112bc7

                // The temporary System::String, released in place in the slot.
                {
                    char*& s = *reinterpret_cast<char**>(&slot);
                    if (s != nullptr) {                     // 180112bd5
                        --*reinterpret_cast<int32_t*>(s - 4);
                        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {
                            s = s - 8;                      // 180112beb
                            MemFree(s);
                        } else {
                            s = nullptr;                    // 180112bfe
                        }
                    }
                }

                ++i;                                        // 180112c06
                table = *reinterpret_cast<const uint8_t* const*>(self + 0xb0);  // 180112c08
            } while (i < *reinterpret_cast<const int32_t*>(table + 0x28));
        }

        // 180112c18: close any dialog the element is still showing.
        void* open = *reinterpret_cast<void**>(elem + 0x120);
        if (open != nullptr) {                              // 180112c1f
            slot = open;                                    // 180112c24
            using CloseDialogFn = void (*)(void*, void**);
            RtAddr<CloseDialogFn>(0x180120f20ull)(
                *reinterpret_cast<void**>(static_cast<uint8_t*>(open) + 0x10),
                &slot);                                     // 180112c38
            *reinterpret_cast<void**>(elem + 0x120) = nullptr;          // 180112c3d
        }

        if (*reinterpret_cast<int32_t*>(elem + 0x118) != 0) {           // 180112c44
            *reinterpret_cast<int32_t*>(elem + 0x118) = 0;              // 180112c4d
            // A UI::ComboElement member (0x180112180), not this class's — see
            // the banner. Reached by address, which is registry-first.
            using ComboElemFn = void (*)(void*);
            RtAddr<ComboElemFn>(0x180112180ull)(elem);                  // 180112c57
        }
    }

    *ret = elem;                                            // 180112c5c
    return ret;
}

int RtUiComboDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ComboDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ComboDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180074ac0ull, (void*)&RtUi_ComboDescription_Type);

    RegisterRuntimeExport("?TypeName@ComboDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ComboDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180074ae0ull, (void*)&RtUi_ComboDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ComboDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ComboDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1801129b0ull, (void*)&RtUi_ComboDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ComboDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180074d20ull, (void*)&RtUi_ComboDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ComboDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ComboDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180074cc0ull, (void*)&RtUi_ComboDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ComboDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ComboDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180074d70ull, (void*)&RtUi_ComboDescription_Destruct);

    RegisterRuntimeExport("??1ComboDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ComboDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180112370ull, (void*)&RtUi_ComboDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ComboDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ComboDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1801129c0ull, (void*)&RtUi_ComboDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ComboDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ComboDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180112a70ull, (void*)&RtUi_ComboDescription_CreateInstance);

    return n;
}
