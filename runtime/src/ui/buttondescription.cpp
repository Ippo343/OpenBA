// ============================================================================
// runtime/src/ui/buttondescription.cpp — UI::ButtonDescription, the whole class
// ============================================================================
//
// ??_7ButtonDescription@UI@@6B@ (retail 0x180157370), ten slots:
//   0  deleting dtor      0x1800481a0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x180047e30  here
//   2  Type               0x180047e10  here
//   3  GetFileVersion     0x1801113d0  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1801113e0  here
//   6  Reconstruct        0x180047fb0  here
//   7  Destruct           0x180047fe0  here
//   8  CreateInstance     0x180111510  here
//   9  Construct          0x180047fd0  here
// plus ??1ButtonDescription@UI@@UEAA@XZ (0x180110bd0) and the UNEXPORTED
// shared applier 0x1801108d0, registered BY ADDRESS ONLY.
//
// THE APPLIER IS THE REASON THIS CLASS IS THE ROOT OF A SUBTREE. 0x1801108d0 is
// called by THREE CreateInstance bodies — this one (0x1801115b6),
// CheckDescription's (0x180111a77) and ComboDescription's (0x180112b0c) — which
// is why it is a separate function in the shipped binary and is one here,
// owned by the base class rather than copied into each subclass file.
//
// The constructor is OUT OF LINE (`??0ButtonDescription@UI@@QEAA@XZ`,
// 0x180110b10) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp, with
// the two factory steps. `Construct` and `Reconstruct` tail-jump into it.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xa0.
//
// ~ButtonDescription RELEASES NOTHING BUT THE BASE'S _textName. The five
// ExternalPtr<ElementDescription> at +0x68..+0x88 are left alone, and so are
// the four ints. Retail's, transcribed.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtButtonDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblButtonDescription); }
void* VtButtonElement()     { return RT_DATA_SLOT(void*, RtUI::kVtblButtonElement); }
const uint8_t* ButtonTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ButtonDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* ButtonCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0ButtonDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

extern "C" uint32_t RtUi_ButtonDescription_Type(const void*) {
    const uint8_t* ti = ButtonTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ButtonDescription_TypeName(const void*) {
    const uint8_t* ti = ButtonTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ButtonDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_ButtonDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                  // 180047fd3
    return ButtonCtor(selfv);                              // 180047fc8 JMP
}
extern "C" void* RtUi_ButtonDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                   // 180047fbe
    return ButtonCtor(selfv);                              // 180047fc8 JMP
}
extern "C" void RtUi_ButtonDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1ButtonDescription@UI@@UEAA@XZ   0x180110bd0
// ============================================================================
// TWO vtable stamps back to back — own, then ElementDescription's — with
// nothing between them. Both are kept: a port that writes only the second is a
// different function under any interruption (the reason
// runtime/src/graphics/wave24_lane_gfxmat.cpp gives for the same choice).
extern "C" void RtUi_ButtonDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtButtonDescription();                            // 180110bed
    *v = VtElementDescription();                           // 180110bf7
    ReleaseStringField(p, 0x60);                           // 180110bfa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180110c2c JMP
}

// ============================================================================
// ?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801113e0
// ============================================================================
//   ElementDescription::Serialize(this, stream)               0x180116b00
//   five ExternalPtr<ElementDescription>::Serialize over +0x68..+0x88 (0x180038f90)
//   four kScalarUInt (+0x98 in the ReaderWriter vtable) over +0x90..+0x9c
//
// THE FOUR DWORDS GO THROUGH THE UNSIGNED SLOT (+0x98), NOT THE INT SLOT
// (+0xa0). Read off 0x18011148d/0x1801114a5/0x1801114bd/0x1801114d5; the
// sibling classes in this batch use both slots and the choice is per field.
extern "C" bool RtUi_ButtonDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                // 1801113fa

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    for (int i = 0; i < 5; ++i)                            // 18011142f .. 180111476
        RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x68 + i * 8, &ser);

    for (int i = 0; i < 4; ++i)                            // 18011147b .. 1801114d5
        RwScalar(rw, kRwScalarUInt, p + 0x90 + i * 4);

    DestroyReaderWriter(rw);                               // 1801114ee
    return true;
}

// ============================================================================
// The shared applier, retail 0x1801108d0 — UNEXPORTED, address-only.
// ============================================================================
// `void Apply(const ButtonDescription* self, InternalPtr<Element>* elemPtr,
//             const Dialog* dialog)`.
//
//   local = *elemPtr                                     ; 1801108f5
//   ElementDescription::CreateInstance(self, &local)      ; 1801108fa
//   for i in 0..4:
//       parent = *elemPtr           (RE-READ every iteration, into RBX)
//       made   = Dialog::CreateElement(dialog, &local, self->_childDesc[i])
//       if (*made) (*made)->_parent = parent
//       Array<Element*>::SetSize(&parent->_children, size+1, made)
//       mark parent and every ancestor kElementFlagNeedsSizeCalc
//   e = *elemPtr ; c = e->_children._data[4]
//   if (c) c->vt[0x50](c, self->_word90)
//   e->+0xc8 = self->+0x90 ; e->+0xcc = +0x94 ; e->+0xd0 = +0x98 ; e->+0xd4 = +0x9c
//       (each through a FRESH `*elemPtr` read)
//
// TWO THINGS HERE LOOK LIKE BUGS AND ARE NOT:
//
// 1. ONE STACK SLOT DOES TWO JOBS. [RSP+0x30] holds the new element when it is
//    handed to `ElementDescription::CreateInstance`, and is then REUSED as
//    `Dialog::CreateElement`'s return slot inside the loop. Splitting it into
//    two locals would be tidier and would stop being retail.
// 2. `*elemPtr` IS RE-READ from the CALLER's slot before every single use —
//    five times in the loop and five more at the tail — after
//    ElementDescription::CreateInstance has run on a LOCAL COPY of it. Folding
//    the re-reads into one variable would assert that nothing can change the
//    caller's slot between them. Retail does not assert that.
//
// THE FIFTH CHILD IS SPECIAL: `[RCX+0x20]` at 0x180110aa0 indexes the children
// array by 0x20 bytes, i.e. element [4], the last one this loop pushed — and
// only if the array pointer itself is non-null is its vt[0x50] called.
extern "C" void RtUi_ButtonDescription_ApplyToElement(const void* selfv,
                                                      void** elemPtr,
                                                      const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* local = *elemPtr;                                // 1801108f5
    ElementDescriptionApply(self, &local);                 // 1801108fa

    for (int i = 0; i < 5; ++i) {                          // 1801108ff .. 180110a94
        uint8_t* parent = *reinterpret_cast<uint8_t**>(elemPtr);
        void*    desc   = *reinterpret_cast<void* const*>(self + 0x68 + i * 8);
        void**   made   = DialogCreateElement(dialog, &local, desc);
        AttachAndPushChild(parent, made);
    }

    {
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr);          // 180110a96
        // `MOV RCX,[RAX+0xa0] ; MOV RCX,[RCX+0x20]` — the second deref is
        // UNGUARDED in retail, so an empty children array faults there too.
        // No null check is added here: the loop above always pushed five.
        void* const* children = *reinterpret_cast<void* const* const*>(e + kElemChildren);
        void* c = children[4];                                       // 180110aa0
        if (c != nullptr) {                                          // 180110aa4
            using SlotFn = void (*)(void*, int32_t);
            reinterpret_cast<SlotFn>(VTableOf(c)[0x50 / 8])(
                c, *reinterpret_cast<const int32_t*>(self + 0x90));  // 180110ab2
        }
    }

    // Four dwords, each preceded by its own `MOV RCX,[RDI]`.
    static const int kSrc[4] = { 0x90, 0x94, 0x98, 0x9c };
    static const int kDst[4] = { 0xc8, 0xcc, 0xd0, 0xd4 };
    for (int i = 0; i < 4; ++i) {                          // 180110ab5 .. 180110afa
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr);
        *reinterpret_cast<int32_t*>(e + kDst[i]) =
            *reinterpret_cast<const int32_t*>(self + kSrc[i]);
    }
}

// ============================================================================
// ?CreateInstance@ButtonDescription@UI@@...   retail 0x180111510
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*.
// ButtonElement is 0xd8 bytes and its constructor is INLINED here rather than
// called: ??0Element (0x180115e50), then the ButtonElement vtable, then two
// DWORD stores (+0xc0 = -1, +0xc4 = 0). Note the widths: both are `MOV dword`,
// not one qword, because the values differ.
extern "C" void** RtUi_ButtonDescription_CreateInstance(const void* selfv,
                                                        void** ret,
                                                        const void* dialog) {
    void* raw = MemAlloc(0xd8, 8);                         // 180111552

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                  // 18011157d
        ElementCtor(raw);                                  // 180111582
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e)          = VtButtonElement();  // 18011158f
        *reinterpret_cast<int32_t*>(e + 0xc0) = -1;                 // 180111592
        *reinterpret_cast<int32_t*>(e + 0xc4) = 0;                  // 18011159c
        elem = e;
    }

    void* slot = elem;                                     // 1801115a6
    RtUi_ButtonDescription_ApplyToElement(selfv, &slot, dialog);    // 1801115b6

    *ret = elem;                                           // 1801115bb
    return ret;
}

int RtUiButtonDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ButtonDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ButtonDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180047e10ull, (void*)&RtUi_ButtonDescription_Type);

    RegisterRuntimeExport("?TypeName@ButtonDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ButtonDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180047e30ull, (void*)&RtUi_ButtonDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ButtonDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ButtonDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1801113d0ull, (void*)&RtUi_ButtonDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ButtonDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047fd0ull, (void*)&RtUi_ButtonDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ButtonDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180047fb0ull, (void*)&RtUi_ButtonDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ButtonDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ButtonDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047fe0ull, (void*)&RtUi_ButtonDescription_Destruct);

    RegisterRuntimeExport("??1ButtonDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ButtonDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180110bd0ull, (void*)&RtUi_ButtonDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ButtonDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1801113e0ull, (void*)&RtUi_ButtonDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ButtonDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180111510ull, (void*)&RtUi_ButtonDescription_CreateInstance);

    // ADDRESS ONLY: the applier has no export name.
    RegisterRuntimeAddr(0x1801108d0ull, (void*)&RtUi_ButtonDescription_ApplyToElement); ++n;

    return n;
}
