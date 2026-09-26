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
// plus ??1DocumentTextDescription@UI@@UEAA@XZ (0x1800cfb30) and the applier
// 0x1800cf520 (the non-virtual CreateInstance(InternalPtr<DocumentTextElement>,
// const Dialog*) const), registered BY ADDRESS ONLY.
//
// B.3.6 block C5: slots 1-3 and 5-9 and the applier are members of
// UI::DocumentTextDescription (runtime/types/ui/DocumentTextDescription.h); the
// registered names are one-line qualified doors.
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
// Layout: runtime/types/ui/DocumentTextDescription.h (cross-asserted against
// DescriptionsBatch1.h's POD). sizeof 0xe8.
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/DocumentTextDescription.h"
#include "../../types/ui/DocumentTextElement.h"

namespace {

using namespace rtdesc;

// RULE B6-5: the members below run on the loader thread. Every export they
// resolve lives in a namespace-scope atomic, never a function-local static.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};
std::atomic<void*> g_ctor{nullptr};

void* VtDocumentTextDescription() {
    return AtomicExport(g_vtable, RtUI::kVtblDocumentTextDescription);
}
const uint8_t* DocumentTextTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@DocumentTextDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// ??0DocumentTextDescription@UI@@QEAA@XZ (0x1800cfc30) — the ctor DOOR (B4b-2).
void* DocumentTextCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(
        AtomicExport(g_ctor, "??0DocumentTextDescription@UI@@QEAA@XZ"))(self);
}

}  // namespace

namespace UI {

// [1] 0x18003b900 / [2] 0x18003b8e0 — the BaseTypeInfo's +0x08 name and +0x00 id.
const char* DocumentTextDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, DocumentTextTypeInfo() + 8, sizeof v); return v;
}
uint32_t DocumentTextDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, DocumentTextTypeInfo(), sizeof v); return v;
}
// [3] 0x1800cfb20
int32_t DocumentTextDescription::GetFileVersion() const { return 0x204; }

// [9] 0x18003baa0 — the null-`this` test stays in the door; the member is the JMP.
Object::External* DocumentTextDescription::Construct() {
    return reinterpret_cast<Object::External*>(DocumentTextCtor(this));   // 18003baa9 JMP
}
// [6] 0x18003ba80 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* DocumentTextDescription::Reconstruct() {
    DeletingDtor(0);                                                        // 18003ba8e slot 0
    return reinterpret_cast<Object::External*>(DocumentTextCtor(this));   // 18003ba98 JMP
}
// [7] 0x18003bab0
void DocumentTextDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// [5] ?Serialize@DocumentTextDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800cf8a0
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
bool DocumentTextDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                  // 1800cf8c6

    Serializer ser = { stream, nullptr };                   // [RBP-0x18], [RBP-0x10]
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn   = void (*)(void*, Serializer*);
    using SetSizeFn    = void (*)(void*, int32_t);
    using ArraySerFn   = void (*)(Serializer*, int32_t, void*);

    RtAddr<FieldSerFn>(0x1800042a0ull)(&_documentStringTable, &ser);   // 1800cf8ff ExternalPtr<StringTable>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_documentTextName, &ser);      // 1800cf90c System::String
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);           // 1800cf919 ExternalPtr<SpriteSheet>

    int32_t n;                                              // [RBP+0x10], the scratch

    n = _frameNamesCount;                                   // 1800cf91e
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cf935
    if (ser.stream->mode == 0)                              // 1800cf93b re-read
        RtAddr<SetSizeFn>(0x180029a80ull)(&_frameNamesData, n);        // 1800cf94f
    RtAddr<ArraySerFn>(0x180029bb0ull)(&ser, n, _frameNamesData);

    RtAddr<FieldSerFn>(0x1800c9930ull)(&_underlineSpriteName, &ser);   // 1800cf972 System::String

    RwScalar(rw, kRwScalarUInt, &_linkColor);               // 1800cf988
    RwScalar(rw, kRwScalarUInt, &_linkHoverColor);          // 1800cf99f
    RwScalar(rw, kRwScalarUInt, &_linkPressedColor);        // 1800cf9b6
    RwScalar(rw, kRwScalarInt,  &_tabWidth);                // 1800cf9cd
    RwScalar(rw, kRwScalarInt,  &_wrapWidth);               // 1800cf9e4

    n = _fontsCount;                                        // 1800cf9ea
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa01
    if (ser.stream->mode == 0)                              // 1800cfa07
        RtAddr<SetSizeFn>(0x18003e600ull)(&_fontsData, n);  // 1800cfa1b
    RtAddr<ArraySerFn>(0x18003e710ull)(&ser, n, _fontsData);

    n = _sheetsCount;                                       // 1800cfa33
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa4a
    if (ser.stream->mode == 0)                              // 1800cfa50
        RtAddr<SetSizeFn>(0x18003ea50ull)(&_sheetsData, n); // 1800cfa64
    RtAddr<ArraySerFn>(0x18003eb60ull)(&ser, n, _sheetsData);

    n = _colorsCount;                                       // 1800cfa7c
    RwScalar(rw, kRwScalarInt, &n);                         // 1800cfa93
    if (ser.stream->mode == 0)                              // 1800cfa99
        RtAddr<SetSizeFn>(0x18003eea0ull)(&_colorsData, n); // 1800cfaad
    {
        // The hand-rolled int loop, walking the array's data pointer read
        // AFTER the SetSize call (`MOV RBX,[RSI+0xd0]` at 0x1800cfab2).
        int32_t* q = _colorsData;                              // 1800cfab2
        for (int32_t k = n; k > 0; --k) {                      // 1800cfabc / 1800cfadc
            RwScalar(rw, kRwScalarUInt, q);                    // 1800cfacf
            ++q;                                               // 1800cfad5
        }
    }

    RtAddr<FieldSerFn>(0x18003dc90ull)(&_resourceList, &ser);   // 1800cfae9

    DestroyReaderWriter(rw);                                // 1800cfb00
    return true;
}

// ============================================================================
// The applier, retail 0x1800cf520 — the NON-virtual
// `CreateInstance(InternalPtr<DocumentTextElement>, const Dialog*) const`
// (B36_SLOTS §6: exported, not the "UNEXPORTED" this file used to say). The
// registration stays address-only, as before.
// ============================================================================
// R8 IS NEVER READ by this body — it creates no children.
//
//   local = *elemPtr ; ElementDescription::CreateInstance(self, &local)
//   idx   = table ? 0x180002470(table, &_documentTextName) : -1
//   e->_text = { table, idx }                            <- QWORD from a DWORD slot
//   e->_spriteSheet = _spriteSheet
//   Array::SetSize(&e->_frameTable, _frameNamesCount)    0x1800370c0
//   for i in 0.._frameNamesCount-1:
//       sheet = _spriteSheet                             (RE-READ every iteration)
//       idx   = sheet ? GetSpriteIndexFromName(sheet, &_frameNames[i]) : -1
//       row   = e->_frameTable                           (RE-READ every iteration)
//       row[i].sheet = sheet ; row[i].index = idx        <- 16-byte stride
//   e->_underlineSprite = { _spriteSheet, index of _underlineSpriteName }
//   five dwords (link colours, tab width, wrap width), each through a fresh *elemPtr
//   0x18003ce30(&e->_fonts, &_fonts) ; 0x18003cf20(&e->_sheets, &_sheets)
//   THE Array<int> COPY, open-coded (see below)
//   e->_resourceList = _resourceList
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
void DocumentTextDescription::CreateInstance(Object::InternalPtr<DocumentTextElement>* element,
                                             const Dialog* /*dialog*/) const {
    void* local = element->Get();                           // 1800cf548
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 1800cf550
        reinterpret_cast<Object::InternalPtr<Element>*>(&local));  // B.3.6 bridge

    {
        void* table = _documentStringTable;                 // 1800cf555
        uint64_t idxSlot;                                   // [RSP+0x30], see below
        if (table != nullptr) {                             // 1800cf559
            using FontIdxFn = int32_t (*)(const void*, const void*);
            const int32_t idx = RtAddr<FontIdxFn>(0x180002470ull)(table, &_documentTextName);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);   // 1800cf56a DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);   // 1800cf570
        }
        DocumentTextElement* e = element->Get();            // 1800cf578
        e->_text.table = table;                             // 1800cf57b
        // 1800cf587: a QWORD store over index AND its pad.
        __builtin_memcpy(reinterpret_cast<uint8_t*>(&e->_text) + 8, &idxSlot, sizeof idxSlot);
    }

    element->Get()->_spriteSheet = _spriteSheet;            // 1800cf58e / 1800cf595
    {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x1800370c0ull)(
            &element->Get()->_frameTableData, _frameNamesCount);        // 1800cf59c / 1800cf5ac
    }

    // The bound is RE-READ from _frameNamesCount at the bottom of every
    // iteration (0x1800cf60b), not hoisted. Transcribed that way.
    for (int32_t i = 0; i < _frameNamesCount; ++i) {        // 1800cf5b5 / 1800cf60b
        const uint8_t* names = static_cast<const uint8_t*>(_frameNamesData);
        void* sheet = _spriteSheet;                         // 1800cf5ce
        const uint64_t idxSlot = SpriteIndexSlot(sheet, names + i * 8);   // 1800cf5da
        uint8_t* row = static_cast<uint8_t*>(element->Get()->_frameTableData) + i * 0x10;  // 1800cf5ed
        *reinterpret_cast<void**>(row)        = sheet;      // 1800cf5f7
        *reinterpret_cast<uint64_t*>(row + 8) = idxSlot;    // 1800cf600 QWORD store
    }

    {
        void* sheet = _spriteSheet;                                     // 1800cf613
        const uint64_t idxSlot = SpriteIndexSlot(sheet, &_underlineSpriteName);  // 1800cf626
        uint8_t* sprite = reinterpret_cast<uint8_t*>(&element->Get()->_underlineSprite);  // 1800cf639
        *reinterpret_cast<void**>(sprite)        = sheet;   // 1800cf63c
        *reinterpret_cast<uint64_t*>(sprite + 8) = idxSlot; // 1800cf648 QWORD store
    }

    element->Get()->_linkColor        = _linkColor;         // 1800cf64f .. 1800cf694
    element->Get()->_linkHoverColor   = _linkHoverColor;
    element->Get()->_linkPressedColor = _linkPressedColor;
    element->Get()->_tabWidth         = _tabWidth;
    element->Get()->_wrapWidth        = _wrapWidth;

    {
        using AssignFn = void (*)(void*, const void*);
        RtAddr<AssignFn>(0x18003ce30ull)(&element->Get()->_fontsData, &_fontsData);    // 1800cf69a / 1800cf6ab
        RtAddr<AssignFn>(0x18003cf20ull)(&element->Get()->_sheetsData, &_sheetsData);  // 1800cf6b0 / 1800cf6c1
    }

    // ---- the open-coded Array<int> copy -------------------------------------
    {
        DocumentTextElement* e = element->Get();            // 1800cf6c6
        if (static_cast<const void*>(&e->_colorsData) !=
            static_cast<const void*>(&_colorsData)) {       // 1800cf6d7
            if (e->_colorsData != nullptr) MemFree(e->_colorsData);     // 1800cf70f (locked Free)

            e->_colorsData     = nullptr;                   // 1800cf71e
            e->_colorsCount    = 0;                         // 1800cf721  size+capacity, one qword
            e->_colorsCapacity = 0;
            const int32_t n = _colorsCount;                 // 1800cf725
            e->_colorsCount = n;                            // 1800cf72a
            if (n > 0) {                                    // 1800cf730
                e->_colorsCapacity = n;                     // 1800cf732 capacity
                e->_colorsData = MemAlloc(static_cast<uint64_t>(n) * 4, 4);   // 1800cf76a
            }
            if (e->_colorsCount > 0) {                      // 1800cf77e
                for (int32_t i = 0; i < e->_colorsCount; ++i) {
                    int32_t* out = static_cast<int32_t*>(e->_colorsData) + i;   // 1800cf788
                    if (out != nullptr)                     // 1800cf78e
                        *out = _colorsData[i];              // 1800cf797
                }
            }
        }
    }

    element->Get()->_resourceList = _resourceList;          // 1800cf7a6 / 1800cf7b0
}

// ============================================================================
// [8] ?CreateInstance@DocumentTextDescription@UI@@...   retail 0x1800cf7e0
// ============================================================================
// DocumentTextElement is 0x1c8 and its constructor is OUT OF LINE
// (`??0DocumentTextElement@UI@@QEAA@XZ`, 0x1800cf410, already ported by
// runtime/src/ui/factory_ui1.cpp); retail takes its RETURN value as the element.
Object::InternalPtr<Element>* DocumentTextDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0x1c8, 8);                         // 1800cf822

    DocumentTextElement* elem = nullptr;
    if (raw != nullptr) {                                   // 1800cf84d
        using DtElementCtorFn = void* (*)(void*);
        elem = static_cast<DocumentTextElement*>(
            RtAddr<DtElementCtorFn>(0x1800cf410ull)(raw));  // 1800cf852
    }

    Object::InternalPtr<DocumentTextElement> slot(elem);    // 1800cf85e
    CreateInstance(&slot, dialog);                          // 1800cf86e the applier, now a member

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800cf873
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_DocumentTextDescription_Type(const void* self) {
    return reinterpret_cast<const UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::Type();
}
extern "C" const char* RtUi_DocumentTextDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::TypeName();
}
extern "C" int32_t RtUi_DocumentTextDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::GetFileVersion();
}
extern "C" void* RtUi_DocumentTextDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                    // 18003baa3 (retail null test)
    return reinterpret_cast<UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::Construct();
}
extern "C" void* RtUi_DocumentTextDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::Reconstruct();
}
extern "C" void RtUi_DocumentTextDescription_Destruct(void* self) {
    reinterpret_cast<UI::DocumentTextDescription*>(self)->UI::DocumentTextDescription::Destruct();
}
extern "C" bool RtUi_DocumentTextDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::Serialize(stream);
}
extern "C" void RtUi_DocumentTextDescription_ApplyToElement(const void* self, void** elemPtr,
                                                            const void* dialog) {
    reinterpret_cast<const UI::DocumentTextDescription*>(self)
        ->UI::DocumentTextDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::DocumentTextElement>*>(elemPtr),  // B.3.6 bridge
            static_cast<const UI::Dialog*>(dialog));
}
extern "C" void** RtUi_DocumentTextDescription_CreateInstance(const void* self, void** ret,
                                                              const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::DocumentTextDescription*>(self)
            ->UI::DocumentTextDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1DocumentTextDescription@UI@@UEAA@XZ   0x1800cfb30 — a DOOR (B4b-2), unchanged.
// ============================================================================
// Four array destructors in DESCENDING offset order, then two String releases,
// then the base vtable and the base's _textName, then the tail JMP.
// The +0x68 ExternalPtr<StringTable> and the +0x78 ExternalPtr<SpriteSheet> are
// NOT released — the same asymmetry ~ImageDescription has.
extern "C" void RtUi_DocumentTextDescription_Dtor(void* selfv) {
    using namespace rtdesc;
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
