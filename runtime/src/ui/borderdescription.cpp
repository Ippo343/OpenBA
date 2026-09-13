// ============================================================================
// runtime/src/ui/borderdescription.cpp — UI::BorderDescription, the whole class
// ============================================================================
//
// ??_7BorderDescription@UI@@6B@ (retail 0x180156980), ten slots:
//   0  deleting dtor      0x180040ff0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x1800409e0  here
//   2  Type               0x1800409c0  here
//   3  GetFileVersion     0x18010fe10  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x18010fe20  here
//   6  Reconstruct        0x180040b60  here
//   7  Destruct           0x180040b90  here
//   8  CreateInstance     0x18010ff40  here
//   9  Construct          0x180040b80  here
// plus ??1BorderDescription@UI@@UEAA@XZ (0x18010fb20).
//
// NOT HERE, and deliberately: `??0BorderDescription@UI@@QEAA@XZ` (0x18010fa50)
// and the two factory steps `?CreateNewObject@` (0x180040ad0) /
// `?CreateInPlaceObject@` (0x180040b50) are ALREADY ported and registered,
// table-driven, by runtime/src/ui/factory_ui1.cpp (the kBorderDescription row
// in factory_ui1_tables.inc). Unlike GroupDescription, whose constructor is
// inlined everywhere and has no out-of-line `??0`, this class HAS one and both
// `Construct` and `Reconstruct` tail-jump straight into it — so there is no
// ctor body to transcribe here at all, only the two wrappers.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xb8.
//
// THE +0x68 ExternalPtr IS NOT RELEASED BY THE DESTRUCTOR — only the nine
// System::Strings and the base's _textName. That is retail's, transcribed, not
// an omission; the same is true of ~ImageDescription.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtBorderDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblBorderDescription); }
const uint8_t* BorderTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@BorderDescription@UI@@0VBaseTypeInfo@Object@@A");
}

// ??0BorderDescription@UI@@QEAA@XZ — ported by factory_ui1.cpp, reached here
// through the port-aware door so `Construct`'s tail jump lands on OUR copy.
void* BorderCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0BorderDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

// ============================================================================
// The trivial slots
// ============================================================================
// `Type` returns the FIRST DWORD of the BaseTypeInfo static (0x1801fa890) and
// `TypeName` the pointer at +0x08 — retail reads them as two separate globals,
// which are the same two fields runtime/types/object/TypeInfo.h pins.
extern "C" uint32_t RtUi_BorderDescription_Type(const void*) {
    const uint8_t* ti = BorderTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_BorderDescription_TypeName(const void*) {
    const uint8_t* ti = BorderTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x18010fe10: `MOV EAX,0x104 ; RET`. Per-class constant; read from this body,
// not inherited from a sibling.
extern "C" int32_t RtUi_BorderDescription_GetFileVersion(const void*) { return 0x104; }

// ============================================================================
// Construct / Reconstruct / Destruct
// ============================================================================
// 0x180040b80: `TEST RCX,RCX ; JNZ <ctor> ; XOR EAX,EAX ; RET` — a null test
// and a TAIL JUMP into the constructor, so the ctor's return value (this) is
// this function's.
extern "C" void* RtUi_BorderDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                  // 180040b83
    return BorderCtor(selfv);                              // 180040b78 JMP
}
// 0x180040b60: slot 0 with flag 0 (destroy WITHOUT freeing), then the ctor
// again, with NO null test — retail derefs `this` to reach the vtable before it
// could test it, so a null here faults in retail too.
extern "C" void* RtUi_BorderDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                   // 180040b6e
    return BorderCtor(selfv);                              // 180040b78 JMP
}
// 0x180040b90: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_BorderDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1BorderDescription@UI@@UEAA@XZ   0x18010fb20
// ============================================================================
// Re-stamp own vtable, release the NINE sprite names in DESCENDING order
// (+0xb0 down to +0x70 — the order matters only as a transcription fact, the
// releases are independent), re-stamp the BASE vtable, release
// `ElementDescription::_textName` at +0x60 and TAIL-JUMP to
// `??1External@Object@@` (0x1800bc950 — `JMP`, not `CALL`, at 0x18010fd2d).
extern "C" void RtUi_BorderDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBorderDescription();                            // 18010fb42

    ReleaseStringField(p, 0xb0);                           // 18010fb45
    ReleaseStringField(p, 0xa8);                           // 18010fb78
    ReleaseStringField(p, 0xa0);                           // 18010fbaa
    ReleaseStringField(p, 0x98);                           // 18010fbdc
    ReleaseStringField(p, 0x90);                           // 18010fc0e
    ReleaseStringField(p, 0x88);                           // 18010fc40
    ReleaseStringField(p, 0x80);                           // 18010fc72
    ReleaseStringField(p, 0x78);                           // 18010fca4
    ReleaseStringField(p, 0x70);                           // 18010fcca

    *v = VtElementDescription();                           // 18010fcf0
    ReleaseStringField(p, 0x60);                           // 18010fcfa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 18010fd2d JMP
}

// ============================================================================
// ?Serialize@BorderDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x18010fe20
// ============================================================================
//   ElementDescription::Serialize(this, stream)         0x180116b00, ported
//   pair = { stream, nullptr }
//   rw   = stream->mode == 0 ? enc->vt[0x18] : enc->vt[0x20]
//   ExternalPtr<SpriteSheet>::Serialize(&_spriteSheet, &pair)   0x180004590
//   System::String::Serialize over +0x70 .. +0xb0, ascending   0x1800c9930
//   if (rw) rw->vt[0](rw, 1)
//   return true
//
// NO `if (reading)` arm and no array: every field is fixed. The nine field
// serializers take the PAIR, not the reader (`LEA RDX,[RSP+0x28]`), which is
// why it has to live on this frame.
extern "C" bool RtUi_BorderDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                // 18010fe3a

    Serializer ser = { stream, nullptr };                  // [RSP+0x28], [RSP+0x30]
    void* rw = MakeReaderWriter(stream, &ser);             // 18010fe62 / 18010fe67

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x68, &ser);    // 18010fe78 ExternalPtr<SpriteSheet>
    for (int i = 0; i < 9; ++i)                            // 18010fe7d .. 18010ff0b
        RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x70 + i * 8, &ser);

    DestroyReaderWriter(rw);                               // 18010ff23
    return true;                                           // MOV AL,1
}

// ============================================================================
// ?CreateInstance@BorderDescription@UI@@...   retail 0x18010ff40
// ============================================================================
// *** THE HIDDEN RETURN SLOT. *** This returns `Object::InternalPtr<UI::Element>`
// BY VALUE, so the Microsoft x64 ABI inserts a slot pointer as the FIRST
// argument after `this`: RCX = this, RDX = the return slot. Ghidra drops it.
// R8 (the Dialog*) is never read by this body — it creates no children — so the
// third parameter is declared and ignored, exactly as the bytes do.
//
// BorderElement is 0x150 bytes and its constructor is OUT OF LINE
// (`??0BorderElement@UI@@QEAA@XZ`, 0x18010fd40, already ported by
// factory_ui1.cpp), unlike ImageDescription's, which is inlined at its call
// site. Retail takes the ctor's RETURN value as the element, not the raw
// allocation (`MOV RDI,RAX` at 0x18010ffb4).
//
// Then NINE identical blocks, one per sprite name:
//     sheet = this->_spriteSheet                    (RE-READ every iteration)
//     idx   = sheet ? GetSpriteIndexFromName(sheet, &_spriteName[i]) : -1
//     elem->[0xc0 + 0x10*i] = sheet
//     elem->[0xc8 + 0x10*i] = idx        <- QWORD store from a DWORD slot
// ONE STACK SLOT SERVES ALL NINE ([RBP-0x10]); its high dword is never written
// by anything, so every one of the nine qword stores carries four arbitrary
// bytes. See `SpriteIndexSlot` in description_common.h — the WIDTH is what is
// faithful.
extern "C" void** RtUi_BorderDescription_CreateInstance(const void* selfv,
                                                        void** ret,
                                                        const void* /*dialog*/) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0x150, 8);                        // 18010ff80

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                  // 18010ffaa
        using BorderElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<BorderElementCtorFn>(0x18010fd40ull)(raw));   // 18010ffaf
    }

    void* slot = elem;                                     // 18010ffb7  [RBP+0x20]
    ElementDescriptionApply(self, &slot);                  // 18010ffc2

    for (int i = 0; i < 9; ++i) {                          // 18010ffc7 .. 1801101a0
        void* sheet = *reinterpret_cast<void* const*>(self + 0x68);
        const uint64_t idxSlot = SpriteIndexSlot(sheet, self + 0x70 + i * 8);
        *reinterpret_cast<void**>(elem + 0xc0 + i * 0x10)     = sheet;
        *reinterpret_cast<uint64_t*>(elem + 0xc8 + i * 0x10)  = idxSlot;
    }

    *ret = elem;                                           // 1801101a7
    return ret;                                            // MOV RAX,R14
}

// ============================================================================
// Registration. Both doors for every row: the mangled name (which is what the
// minted BaseTypeInfo and the minted vtable ask for first) and the address
// (which is what a ported caller's `ResolveAddr` literal asks for).
// ============================================================================
int RtUiBorderDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@BorderDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_BorderDescription_Type);            ++n;
    RegisterRuntimeAddr(0x1800409c0ull, (void*)&RtUi_BorderDescription_Type);

    RegisterRuntimeExport("?TypeName@BorderDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_BorderDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800409e0ull, (void*)&RtUi_BorderDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@BorderDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_BorderDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18010fe10ull, (void*)&RtUi_BorderDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_BorderDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180040b80ull, (void*)&RtUi_BorderDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@BorderDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_BorderDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180040b60ull, (void*)&RtUi_BorderDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@BorderDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_BorderDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180040b90ull, (void*)&RtUi_BorderDescription_Destruct);

    RegisterRuntimeExport("??1BorderDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_BorderDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18010fb20ull, (void*)&RtUi_BorderDescription_Dtor);

    RegisterRuntimeExport("?Serialize@BorderDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_BorderDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x18010fe20ull, (void*)&RtUi_BorderDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@BorderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_BorderDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x18010ff40ull, (void*)&RtUi_BorderDescription_CreateInstance);

    return n;
}
