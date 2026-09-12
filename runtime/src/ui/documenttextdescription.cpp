// ============================================================================
// runtime/src/ui/documenttextdescription.cpp — UI::DocumentTextDescription
// ============================================================================
//
// ??_7DocumentTextDescription@UI@@6B@ (retail 0x180156750), ten slots:
//   0  deleting dtor      0x18003bd90  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x18003b900  here
//   2  Type               0x18003b8e0  here
//   3  GetFileVersion     0x1800cfb20  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800cf8a0  here
//   6  Reconstruct        0x18003ba80  here
//   7  Destruct           0x18003bab0  here
//   8  CreateInstance     0x1800cf7e0  here
//   9  Construct          0x18003baa0  here
// plus ??1DocumentTextDescription@UI@@UEAA@XZ (0x1800cfb30) and the UNEXPORTED
// applier 0x1800cf520, registered BY ADDRESS ONLY.
//
// The constructor is OUT OF LINE (`??0DocumentTextDescription@UI@@QEAA@XZ`,
// 0x1800cfc30) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp.
//
// This is the widest class in the batch: four separate arrays, each with its
// own SetSize/Serialize pair, plus a fifth container. Every one of those pairs
// belongs to Collection::Array, not to this class, and is reached by address
// through the registry-first door rather than copied:
//     +0x88  SetSize 0x180029a80   Serialize 0x180029bb0   Array<System::String>
//     +0xb0  SetSize 0x18003e600   Serialize 0x18003e710
//     +0xc0  SetSize 0x18003ea50   Serialize 0x18003eb60
//     +0xd0  SetSize 0x18003eea0   (ints serialized INLINE by this body)
//     +0xe0  whole-container       0x18003dc90
// and the four array destructors ~ at +0xd0/+0xc0/+0xb0/+0x88 likewise
// (0x18003db40, 0x18003da00, 0x18003d8c0, 0x180010ab0).
//
// NOTE ON THE +0x88 DESTRUCTOR: it is 0x180010ab0, the ELEMENT-DESTROYING
// Array dtor, not the trivial 0x180039440. Read from the call at 0x1800cfb7e.
// (The same distinction bit ~AnimatedImageDescription; see
// runtime/src/ui/animatedimagedescription.cpp.)
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xe8.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtDocumentTextDescription() {
    return RT_DATA_SLOT(void*, RtUI::kVtblDocumentTextDescription);
}
const uint8_t* DocumentTextTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@DocumentTextDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* DocumentTextCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0DocumentTextDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

extern "C" uint32_t RtUi_DocumentTextDescription_Type(const void*) {
    const uint8_t* ti = DocumentTextTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_DocumentTextDescription_TypeName(const void*) {
    const uint8_t* ti = DocumentTextTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_DocumentTextDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_DocumentTextDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                   // 18003baa3
    return DocumentTextCtor(selfv);                         // 18003baa9 JMP
}
extern "C" void* RtUi_DocumentTextDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                    // 18003ba8e
    return DocumentTextCtor(selfv);                         // 18003ba98 JMP
}
extern "C" void RtUi_DocumentTextDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1DocumentTextDescription@UI@@UEAA@XZ   0x1800cfb30
// ============================================================================
// Four array destructors in DESCENDING offset order, then two String releases,
// then the base vtable and the base's _textName, then the tail JMP.
// The +0x68 ExternalPtr<StringTable> and the +0x78 ExternalPtr<SpriteSheet> are
// NOT released — the same asymmetry ~ImageDescription has.
extern "C" void RtUi_DocumentTextDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtDocumentTextDescription();                       // 1800cfb4d

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x18003db40ull)(p + 0xd0);          // 1800cfb57
    RtAddr<ArrayDtorFn>(0x18003da00ull)(p + 0xc0);          // 1800cfb64
    RtAddr<ArrayDtorFn>(0x18003d8c0ull)(p + 0xb0);          // 1800cfb71
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(p + 0x88);          // 1800cfb7e  element-destroying

    ReleaseStringField(p, 0x80);                            // 1800cfb84
    ReleaseStringField(p, 0x70);                            // 1800cfbba

    *v = VtElementDescription();                            // 1800cfbeb
    ReleaseStringField(p, 0x60);                            // 1800cfbee

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cfc20 JMP
}

// ============================================================================
// ?Serialize@DocumentTextDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800cf8a0
// ============================================================================
// Four array round-trips, each with the same three-step shape:
//     n = <the size field>              copied to a SCRATCH DWORD on the frame
//     rw->kScalarInt(&n)                (so a READ overwrites the scratch)
//     if (pair.stream->mode == 0) Array::SetSize(&field, n)
//     Array::Serialize(&pair, n, field._data)
//
// TWO TRANSCRIPTION FACTS:
//  * THE SIZE GOES THROUGH A SCRATCH SLOT ([RBP+0x10], the frame's fourth home
//    slot), not through the field itself. On the write path that is invisible;
//    on the read path it means the field's size is set by SetSize, not by the
//    serializer, and the two can disagree if SetSize clamps.
//  * THE MODE IS RE-READ FROM THE PAIR'S COPY (`MOV RAX,[RBP-0x18] ; CMP dword
//    [RAX+8],0`, e.g. 0x1800cf93b), one deref deeper than the first test in
//    MakeReaderWriter. The two are the same Stream today; the re-read is
//    transcribed rather than folded because folding it would assert they must
//    be. Identical finding in runtime/src/ui/groupdescription.cpp.
//
// The FOURTH array has no Array::Serialize at all: its ints are pushed one at a
// time through the kScalarUInt slot in a hand-rolled loop (0x1800cfac2).
extern "C" bool RtUi_DocumentTextDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                 // 1800cf8c6

    Serializer ser = { stream, nullptr };                   // [RBP-0x18], [RBP-0x10]
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn   = void (*)(void*, Serializer*);
    using SetSizeFn    = void (*)(void*, int32_t);
    using ArraySerFn   = void (*)(Serializer*, int32_t, void*);

    RtAddr<FieldSerFn>(0x1800042a0ull)(p + 0x68, &ser);     // 1800cf8ff ExternalPtr<StringTable>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x70, &ser);     // 1800cf90c System::String
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x78, &ser);     // 1800cf919 ExternalPtr<SpriteSheet>

    int32_t n;                                              // [RBP+0x10], the scratch

    n = *reinterpret_cast<int32_t*>(p + 0x90);              // 1800cf91e
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cf935
    if (ser.stream->mode == 0)                              // 1800cf93b re-read
        RtAddr<SetSizeFn>(0x180029a80ull)(p + 0x88, n);     // 1800cf94f
    RtAddr<ArraySerFn>(0x180029bb0ull)(&ser, n, *reinterpret_cast<void**>(p + 0x88));

    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x80, &ser);     // 1800cf972 System::String

    RwScalar(rw, kRwScalarUInt, p + 0x98);                  // 1800cf988
    RwScalar(rw, kRwScalarUInt, p + 0x9c);                  // 1800cf99f
    RwScalar(rw, kRwScalarUInt, p + 0xa0);                  // 1800cf9b6
    RwScalar(rw, kRwScalarInt,  p + 0xa4);                  // 1800cf9cd
    RwScalar(rw, kRwScalarInt,  p + 0xa8);                  // 1800cf9e4

    n = *reinterpret_cast<int32_t*>(p + 0xb8);              // 1800cf9ea
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa01
    if (ser.stream->mode == 0)                              // 1800cfa07
        RtAddr<SetSizeFn>(0x18003e600ull)(p + 0xb0, n);     // 1800cfa1b
    RtAddr<ArraySerFn>(0x18003e710ull)(&ser, n, *reinterpret_cast<void**>(p + 0xb0));

    n = *reinterpret_cast<int32_t*>(p + 0xc8);              // 1800cfa33
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa4a
    if (ser.stream->mode == 0)                              // 1800cfa50
        RtAddr<SetSizeFn>(0x18003ea50ull)(p + 0xc0, n);     // 1800cfa64
    RtAddr<ArraySerFn>(0x18003eb60ull)(&ser, n, *reinterpret_cast<void**>(p + 0xc0));

    n = *reinterpret_cast<int32_t*>(p + 0xd8);              // 1800cfa7c
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa93
    if (ser.stream->mode == 0)                              // 1800cfa99
        RtAddr<SetSizeFn>(0x18003eea0ull)(p + 0xd0, n);     // 1800cfaad
    {
        // The hand-rolled int loop, walking the array's data pointer read
        // BEFORE SetSize could have moved it (`MOV RBX,[RSI+0xd0]` at
        // 0x1800cfab2 — after the SetSize call, so the reload is retail's).
        uint8_t* q = *reinterpret_cast<uint8_t**>(p + 0xd0);   // 1800cfab2
        for (int32_t k = n; k > 0; --k) {                      // 1800cfabc / 1800cfadc
            RwScalar(rw, kRwScalarUInt, q);                    // 1800cfacf
            q += 4;                                            // 1800cfad5
        }
    }

    RtAddr<FieldSerFn>(0x18003dc90ull)(p + 0xe0, &ser);     // 1800cfae9

    DestroyReaderWriter(rw);                                // 1800cfb00
    return true;
}

// ============================================================================
// The applier, retail 0x1800cf520 — UNEXPORTED, address-only.
// ============================================================================
// `void Apply(const DocumentTextDescription* self, InternalPtr<Element>* elemPtr,
//             const Dialog*)`. R8 IS NEVER READ by this body — it creates no
// children — so the third parameter is declared and ignored, exactly as the
// bytes do; it is declared at all because the ONE call site (0x1800cf86e)
// passes it.
//
//   local = *elemPtr ; ElementDescription::CreateInstance(self, &local)
//   font  = self->+0x68 (the StringTable/Font external)
//   idx   = font ? 0x180002470(font, &self->_textName) : -1
//   e = *elemPtr ; e->+0xc0 = font ; e->+0xc8 = idx      <- QWORD from a DWORD slot
//   e = *elemPtr ; e->+0xd0 = self->_spriteSheet
//   e = *elemPtr ; Array::SetSize(&e->+0xe8, self->_namesSize)      0x1800370c0
//   for i in 0..namesSize-1:
//       sheet = self->_spriteSheet                       (RE-READ every iteration)
//       idx   = sheet ? GetSpriteIndexFromName(sheet, &self->_names[i]) : -1
//       row   = (*elemPtr)->+0xe8                        (RE-READ every iteration)
//       row[i].sheet = sheet ; row[i].index = idx        <- 16-byte stride
//   sheet = self->_spriteSheet ; idx = ... (&self->_spriteName)
//   e->+0xd8 = sheet ; e->+0xe0 = idx
//   five dwords +0xf8..+0x108 <- self +0x98..+0xa8, each through a fresh *elemPtr
//   0x18003ce30(e+0x110, self+0xb0) ; 0x18003cf20(e+0x120, self+0xc0)
//   THE Array<int> COPY, open-coded (see below)
//   e->+0x140 = self->+0xe0
//
// *** `*elemPtr` IS RE-READ BEFORE EVERY SINGLE USE. *** Thirteen times. It is
// transcribed that way for the reason imagedescription.cpp gives: folding the
// re-reads would assert that nothing can change the caller's slot between them,
// and retail does not assert that.
//
// THE Array<int> COPY AT +0x130 IS OPEN-CODED, NOT A CALL, and it is NOT
// `Array::SetSize` — it is an assignment operator MSVC inlined:
//     if (&dst == &src) skip everything
//     if (dst._data) Free(dst._data)
//     dst._data = 0 ; dst._size = 0 and dst._capacity = 0 in ONE qword store
//     dst._size = (int)src._size          (sign-extended through RAX)
//     if (dst._size > 0) { dst._capacity = dst._size ; dst._data = Allocate(n*4, 4) }
//     if (dst._size > 0) for i: if (dst._data + i*4 != 0) dst._data[i] = src._data[i]
// The last line's per-element null test is retail's and is kept: it tests the
// computed ADDRESS, not the pointer, so it can only fire when _data is null and
// i is 0 — a dead branch that costs one instruction and is still in the bytes.
extern "C" void RtUi_DocumentTextDescription_ApplyToElement(const void* selfv,
                                                            void** elemPtr,
                                                            const void* /*dialog*/) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* local = *elemPtr;                                 // 1800cf548
    ElementDescriptionApply(self, &local);                  // 1800cf550

    {
        void* font = *reinterpret_cast<void* const*>(self + 0x68);   // 1800cf555
        uint64_t idxSlot;                                   // [RSP+0x30], see below
        if (font != nullptr) {                              // 1800cf559
            using FontIdxFn = int32_t (*)(const void*, const void*);
            const int32_t idx = RtAddr<FontIdxFn>(0x180002470ull)(font, self + 0x70);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);   // 1800cf56a DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);   // 1800cf570
        }
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr); // 1800cf578
        *reinterpret_cast<void**>(e + 0xc0)    = font;      // 1800cf57b
        *reinterpret_cast<uint64_t*>(e + 0xc8) = idxSlot;   // 1800cf587 QWORD store
    }

    {
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr); // 1800cf58e
        *reinterpret_cast<void**>(e + 0xd0) =
            *reinterpret_cast<void* const*>(self + 0x78);   // 1800cf595
    }
    {
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr); // 1800cf59c
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x1800370c0ull)(
            e + 0xe8, *reinterpret_cast<const int32_t*>(self + 0x90));  // 1800cf5ac
    }

    // The bound is RE-READ from self->_namesSize at the bottom of every
    // iteration (0x1800cf60b), not hoisted. Transcribed that way.
    for (int32_t i = 0;
         i < *reinterpret_cast<const int32_t*>(self + 0x90);
         ++i) {                                             // 1800cf5b5 / 1800cf60b
        void* const* names = *reinterpret_cast<void* const* const*>(self + 0x88);
        void*  sheet = *reinterpret_cast<void* const*>(self + 0x78);   // 1800cf5ce
        const uint64_t idxSlot = SpriteIndexSlot(
            sheet, reinterpret_cast<const uint8_t*>(names) + i * 8);   // 1800cf5da
        uint8_t* e   = *reinterpret_cast<uint8_t**>(elemPtr);          // 1800cf5ed
        uint8_t* row = *reinterpret_cast<uint8_t**>(e + 0xe8) + i * 0x10;
        *reinterpret_cast<void**>(row)         = sheet;     // 1800cf5f7
        *reinterpret_cast<uint64_t*>(row + 8)  = idxSlot;   // 1800cf600 QWORD store
    }

    {
        void* sheet = *reinterpret_cast<void* const*>(self + 0x78);   // 1800cf613
        const uint64_t idxSlot = SpriteIndexSlot(sheet, self + 0x80); // 1800cf626
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr);           // 1800cf639
        *reinterpret_cast<void**>(e + 0xd8)    = sheet;     // 1800cf63c
        *reinterpret_cast<uint64_t*>(e + 0xe0) = idxSlot;   // 1800cf648 QWORD store
    }

    static const int kSrc[5] = { 0x98, 0x9c, 0xa0, 0xa4, 0xa8 };
    static const int kDst[5] = { 0xf8, 0xfc, 0x100, 0x104, 0x108 };
    for (int i = 0; i < 5; ++i) {                           // 1800cf64f .. 1800cf694
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr);
        *reinterpret_cast<int32_t*>(e + kDst[i]) =
            *reinterpret_cast<const int32_t*>(self + kSrc[i]);
    }

    {
        using AssignFn = void (*)(void*, const void*);
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr); // 1800cf69a
        RtAddr<AssignFn>(0x18003ce30ull)(e + 0x110, self + 0xb0);     // 1800cf6ab
        e = *reinterpret_cast<uint8_t**>(elemPtr);          // 1800cf6b0
        RtAddr<AssignFn>(0x18003cf20ull)(e + 0x120, self + 0xc0);     // 1800cf6c1
    }

    // ---- the open-coded Array<int> copy -------------------------------------
    {
        uint8_t* dst = *reinterpret_cast<uint8_t**>(elemPtr) + 0x130;  // 1800cf6c6
        const uint8_t* src = self + 0xd0;                              // 1800cf6c9
        if (dst != src) {                                              // 1800cf6d7
            void* old = *reinterpret_cast<void**>(dst);                // 1800cf6e0
            if (old != nullptr) MemFree(old);                          // 1800cf70f (locked Free)

            *reinterpret_cast<uint64_t*>(dst)     = 0;                 // 1800cf71e
            *reinterpret_cast<uint64_t*>(dst + 8) = 0;                 // 1800cf721  size+capacity
            const int32_t n = *reinterpret_cast<const int32_t*>(src + 8);  // 1800cf725
            *reinterpret_cast<int32_t*>(dst + 8) = n;                  // 1800cf72a
            if (n > 0) {                                               // 1800cf730
                *reinterpret_cast<int32_t*>(dst + 0xc) = n;            // 1800cf732 capacity
                *reinterpret_cast<void**>(dst) =
                    MemAlloc(static_cast<uint64_t>(n) * 4, 4);         // 1800cf76a
            }
            if (*reinterpret_cast<int32_t*>(dst + 8) > 0) {            // 1800cf77e
                for (int32_t i = 0; i < *reinterpret_cast<int32_t*>(dst + 8); ++i) {
                    uint8_t* out = *reinterpret_cast<uint8_t**>(dst) + i * 4;  // 1800cf788
                    if (out != nullptr) {                              // 1800cf78e
                        const uint8_t* in = *reinterpret_cast<const uint8_t* const*>(src);
                        *reinterpret_cast<int32_t*>(out) =
                            *reinterpret_cast<const int32_t*>(in + i * 4);     // 1800cf797
                    }
                }
            }
        }
    }

    {
        uint8_t* e = *reinterpret_cast<uint8_t**>(elemPtr); // 1800cf7a6
        *reinterpret_cast<void**>(e + 0x140) =
            *reinterpret_cast<void* const*>(self + 0xe0);   // 1800cf7b0
    }
}

// ============================================================================
// ?CreateInstance@DocumentTextDescription@UI@@...   retail 0x1800cf7e0
// ============================================================================
// DocumentTextElement is 0x1c8 and its constructor is OUT OF LINE
// (`??0DocumentTextElement@UI@@QEAA@XZ`, 0x1800cf410, already ported by
// runtime/src/ui/factory_ui1.cpp); retail takes its RETURN value as the element.
extern "C" void** RtUi_DocumentTextDescription_CreateInstance(const void* selfv,
                                                              void** ret,
                                                              const void* dialog) {
    void* raw = MemAlloc(0x1c8, 8);                         // 1800cf822

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                   // 1800cf84d
        using DtElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<DtElementCtorFn>(0x1800cf410ull)(raw));   // 1800cf852
    }

    void* slot = elem;                                      // 1800cf85e
    RtUi_DocumentTextDescription_ApplyToElement(selfv, &slot, dialog);   // 1800cf86e

    *ret = elem;                                            // 1800cf873
    return ret;
}

int RtUiDocumentTextDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@DocumentTextDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_DocumentTextDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18003b8e0ull, (void*)&RtUi_DocumentTextDescription_Type);

    RegisterRuntimeExport("?TypeName@DocumentTextDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_DocumentTextDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18003b900ull, (void*)&RtUi_DocumentTextDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@DocumentTextDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_DocumentTextDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800cfb20ull, (void*)&RtUi_DocumentTextDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_DocumentTextDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18003baa0ull, (void*)&RtUi_DocumentTextDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@DocumentTextDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_DocumentTextDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18003ba80ull, (void*)&RtUi_DocumentTextDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@DocumentTextDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_DocumentTextDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18003bab0ull, (void*)&RtUi_DocumentTextDescription_Destruct);

    RegisterRuntimeExport("??1DocumentTextDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_DocumentTextDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800cfb30ull, (void*)&RtUi_DocumentTextDescription_Dtor);

    RegisterRuntimeExport("?Serialize@DocumentTextDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_DocumentTextDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800cf8a0ull, (void*)&RtUi_DocumentTextDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@DocumentTextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_DocumentTextDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800cf7e0ull, (void*)&RtUi_DocumentTextDescription_CreateInstance);

    // ADDRESS ONLY: the applier has no export name.
    RegisterRuntimeAddr(0x1800cf520ull,
                        (void*)&RtUi_DocumentTextDescription_ApplyToElement);    ++n;

    return n;
}
