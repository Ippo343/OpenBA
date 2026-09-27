// ============================================================================
// runtime/src/ui/containerdescription.cpp — UI::ContainerDescription, whole
// ============================================================================
//
// ??_7ContainerDescription@UI@@6B@ (retail 0x180156510), ten slots:
//   0  deleting dtor      0x180039c00  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x1800398d0  here
//   2  Type               0x1800398b0  here
//   3  GetFileVersion     0x1800ccfe0  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800cced0  here
//   6  Reconstruct        0x180039a50  here
//   7  Destruct           0x180039a80  here
//   8  CreateInstance     0x1800ccb90  here
//   9  Construct          0x180039a70  here
// plus ??1ContainerDescription@UI@@UEAA@XZ (0x1800ccff0).
//
// The constructor is OUT OF LINE (`??0ContainerDescription@UI@@QEAA@XZ`,
// 0x1800cd060) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp with the
// two factory steps; Construct and Reconstruct tail-jump into it.
//
// ----------------------------------------------------------------------------
// THIS IS THE ONLY CLASS IN THE BATCH WHOSE CreateInstance BUILDS **TWO**
// ELEMENTS.
// ----------------------------------------------------------------------------
// A ContainerElement (0xd8) and, nested inside it, a ClipRegionElement (0xd8) —
// the scroll viewport. The clip region gets the container's THIRD child
// description; the container itself then gets the other two. The two elements
// are kept in two registers (RSI = container, RDI = clip region) and NEITHER is
// re-read from the caller's slot, so the whole body is written with two locals.
//
// Layout: runtime/types/ui/ContainerDescription.h (the real class, B.3.6
// block C4; cross-asserted against DescriptionsBatch1.h's POD). sizeof 0x90.
//
// ENDGAME B.3.6 block C4: every body below is a MEMBER of
// UI::ContainerDescription; each registered door is a one-line QUALIFIED
// delegation (a live description wears the MINTED table, whose slots resolve
// back to these doors). RULE B6-5: the vtables, the type-info record and the
// ctor door the members reach are cached in namespace-scope std::atomic slots,
// never in a function-local static.
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/ContainerDescription.h"
#include "../../types/ui/ContainerElement.h"

// UI::ClipRegionElement is block C1's class. CreateInstance builds one inline
// (retail inlines its ctor); its own bytes are reached raw below, exactly as
// before, and the site is on the fragment list.
namespace UI { class ClipRegionElement; }

namespace {

using namespace rtdesc;

// RULE B6-5 door cache: RT_DATA_SLOT's never-cache-a-null contract, in a
// namespace-scope atomic instead of a function-local static.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtClipRegionElement{nullptr};
std::atomic<void*> g_typeInfo{nullptr};

void* VtClipRegionElement() { return AtomicExport(g_vtClipRegionElement, RtUI::kVtblClipRegionElement); }
const uint8_t* ContainerTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@ContainerDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t ContainerDescription::Type() const {
    const uint8_t* ti = ContainerTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* ContainerDescription::TypeName() const {
    const uint8_t* ti = ContainerTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// static 0x1800398c0 -- the same read as slot 2 (the minted record until W3).
uint32_t ContainerDescription::TypeId() {
    const uint8_t* ti = ContainerTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
int32_t ContainerDescription::GetFileVersion() const { return 0x204; }

// 0x180039a70 — the ctor, placement. (Retail's `TEST RCX,RCX` sat in the
// retired door; a virtual call always has a `this`.)
Object::External* ContainerDescription::Construct() {
    return ::new (this) ContainerDescription();             // 180039a68 JMP
}
// 0x180039a50 — slot 0 with flag 0 (destroy, do not free), then the ctor. The
// slot-0 call is VIRTUAL, as in retail.
Object::External* ContainerDescription::Reconstruct() {
    DeletingDtor(0);                                        // 180039a5e
    return ::new (this) ContainerDescription();             // 180039a68 JMP
}
// 0x180039a80
void ContainerDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@ContainerDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cced0
// ============================================================================
// THE FIELD ORDER IS NOT THE STRUCT ORDER, and it is taken from this body, not
// from the layout: three ints (+0x68, +0x6c, +0x74), then the three child
// descriptions, and only THEN the two bools at +0x70/+0x71 — which sit between
// the ints in memory. Sorting them into offset order would change the byte
// stream a .rsc parses.
bool ContainerDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);              // 1800cceea (qualified)

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt, &_requestedWidth);           // 1800ccf2e
    RwScalar(rw, kRwScalarInt, &_requestedHeight);          // 1800ccf43
    RwScalar(rw, kRwScalarInt, &_barSize);                  // 1800ccf58

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_partDescs[0], &ser);   // 1800ccf5e
    RtAddr<FieldSerFn>(0x180038f90ull)(&_partDescs[1], &ser);   // 1800ccf6c
    RtAddr<FieldSerFn>(0x180038f90ull)(&_partDescs[2], &ser);   // 1800ccf7d

    RwScalar(rw, kRwScalarBool, &_clampWidth);              // 1800ccf9d
    RwScalar(rw, kRwScalarBool, &_clampHeight);             // 1800ccfaf

    DestroyReaderWriter(rw);                                // 1800ccfc5
    return true;
}

// ============================================================================
// ?CreateInstance@ContainerDescription@UI@@...   retail 0x1800ccb90
// ============================================================================
// RCX = this (R14), RDX = the hidden return slot (R12), R8 = Dialog* (R15).
//
// The order retail builds things in, which is the order here:
//
//  1. ContainerElement (0xd8): ??0Element, vtable, +0xc8 dword 0,
//     +0xcc **WORD** 0 (`MOV word ptr`, two bytes, not four), +0xd0 dword 0.
//  2. slot = container ; ElementDescription::CreateInstance(self, &slot)
//  3. ClipRegionElement (0xd8), its constructor inlined exactly as in
//     runtime/src/ui/clipregiondescription.cpp, including the two UNALIGNED
//     qwords at +0xc4 and +0xcc.
//  4. child = CreateElement(dialog, &slot, _partDescs[2]) attached to the
//     CLIP REGION, not the container.
//  5. clip->ComputeSize() TWICE. Two identical virtual calls in a row; kept.
//  6. the size negotiation (see below)
//  7. the container's own five fields are copied from the description
//  8. slot = clip ; clip->_parent = container ; push clip into container
//  9. two more children from _partDescs[1] and _partDescs[0], in that order —
//     DESCENDING, and after the clip region, so the final child order is
//     [clip, desc1, desc0].
//
// STEP 6, TRANSCRIBED LITERALLY because its shape is not obvious:
//     w = clip->+0xc8
//     h = _requestedHeight ; if (h <= 0) h = clip->+0xcc
//     t = _requestedWidth  ; if (t >  0) w = t
//     if (clip->+0xc0 != w || clip->+0xc4 != h) { store both; mark the chain }
// i.e. the description's extents override the clip region's measured ones only
// when positive, and the mark is skipped when nothing changed.
//
// The ClipRegionElement's OWN fields (+0xc0..+0xd4) belong to block C1's
// header; they stay raw offsets here (fragment list).
Object::InternalPtr<Element>* ContainerDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    // ---- 1. the container ---------------------------------------------------
    // Retail inlines ??0ContainerElement (0x1800ccc06..ccc25): the real ctor.
    void* rawC = MemAlloc(0xd8, 8);                         // 1800ccbd6
    ContainerElement* container = nullptr;
    if (rawC != nullptr)                                    // 1800ccc01
        container = ::new (rawC) ContainerElement();        // 1800ccc06 .. 1800ccc25

    // ---- 2. the base applier ------------------------------------------------
    // ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@… (0x180115ad0)
    // is name-hidden by slot 8 and its body is still a dialog.cpp door, so it
    // is reached by address, as before.
    void* slot = container;                                 // 1800ccc31
    UI::ElementDescription::CreateInstance(                   // 1800ccc3e
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    // ---- 3. the clip region -------------------------------------------------
    void* rawR = MemAlloc(0xd8, 8);                         // 1800ccc60
    uint8_t* clip = nullptr;
    if (rawR != nullptr) {                                  // 1800ccc8b
        ElementCtor(rawR);                                  // 1800ccc90
        uint8_t* e = static_cast<uint8_t*>(rawR);
        *reinterpret_cast<void**>(e) = VtClipRegionElement();           // 1800ccc9d
        *reinterpret_cast<int32_t*>(e + 0xc0) = 0x64;                   // 1800ccca0
        const uint64_t k64 = 0x64, kZero = 0;
        __builtin_memcpy(e + 0xc4, &k64, 8);                            // 1800cccaa
        __builtin_memcpy(e + 0xcc, &kZero, 8);                          // 1800cccb5
        *reinterpret_cast<int32_t*>(e + 0xd4) = 0;                      // 1800cccc0
        clip = e;
    }
    Element* const clipElem = reinterpret_cast<Element*>(clip);         // B.3.6 bridge

    // ---- 4. the clip region's child -----------------------------------------
    void** made = DialogCreateElement(dialog, &slot, _partDescs[2]);    // 1800cccda
    AttachAndPushChild(clip, made);                         // 1800ccce7 .. 1800ccd24

    // ---- 5. two identical virtual calls (slot 4) ----------------------------
    clipElem->ComputeSize();                                // 1800ccd2c
    clipElem->ComputeSize();                                // 1800ccd35

    // ---- 6. the size negotiation --------------------------------------------
    {
        int32_t w = *reinterpret_cast<int32_t*>(clip + 0xc8);           // 1800ccd38
        int32_t h = _requestedHeight;                                   // 1800ccd3e
        if (h <= 0) h = *reinterpret_cast<int32_t*>(clip + 0xcc);       // 1800ccd46
        const int32_t t = _requestedWidth;
        if (t > 0) w = t;                                              // 1800ccd52 CMOVG
        if (*reinterpret_cast<int32_t*>(clip + 0xc0) != w ||            // 1800ccd55
            *reinterpret_cast<int32_t*>(clip + 0xc4) != h) {            // 1800ccd5d
            *reinterpret_cast<int32_t*>(clip + 0xc0) = w;               // 1800ccd65
            *reinterpret_cast<int32_t*>(clip + 0xc4) = h;               // 1800ccd6b
            clipElem->SetNeedsSizeCalc();                               // 1800ccd80
        }
    }

    // ---- 7. the container's own fields --------------------------------------
    container->_barSize         = _barSize;                 // 1800ccd96
    container->_requestedWidth  = _requestedWidth;          // 1800ccda0
    container->_requestedHeight = _requestedHeight;         // 1800ccdaa
    container->_clampWidth      = _clampWidth;              // 1800ccdb4 BYTE
    container->_clampHeight     = _clampHeight;             // 1800ccdbf BYTE

    // ---- 8. the clip region becomes the container's first child -------------
    slot = clip;                                            // 1800ccdca
    clipElem->_parent = container;                          // 1800ccdcf
    {
        using PushFn = void (*)(void*, int, const void*);
        RtAddr<PushFn>(0x180076900ull)(
            &container->_children, container->_children.GetSize() + 1, &slot);  // 1800ccdea
        container->SetNeedsSizeCalc();                      // 1800ccdf2
    }

    // ---- 9. the container's two remaining children, DESCENDING --------------
    uint8_t* const containerBytes = reinterpret_cast<uint8_t*>(container);
    made = DialogCreateElement(dialog, &slot, _partDescs[1]);           // 1800cce17
    AttachAndPushChild(containerBytes, made);               // 1800cce24 .. 1800cce54

    made = DialogCreateElement(dialog, &slot, _partDescs[0]);           // 1800cce62
    AttachAndPushChild(containerBytes, made);               // 1800cce6f .. 1800ccea4

    *reinterpret_cast<void**>(ret) = container;             // 1800ccea6
    return ret;
}

}  // namespace UI

namespace UI {

// ??1ContainerDescription@UI@@UEAA@XZ 0x1800ccff0 -- retail's body is the two
// restamps, ~ElementDescription's +0x60 String release and the tail into
// ~External: all the compiler's chain now.
ContainerDescription::~ContainerDescription() {}

// ?CreateNewObject@ContainerDescription@UI@@SAPEAVBase@Object@@XZ 0x1800399c0 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x90, align 8; retail's null
// branch; the out-of-line ctor. Replaces the FACTUI1 row (B39_RULES D2).
Object::Base* ContainerDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ContainerDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ContainerDescription();
}
// ?CreateInPlaceObject@ContainerDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180039a40
Object::Base* ContainerDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ContainerDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors (Type .. CreateInstance), the ??1 door and
// the FACTUI1 ??0 door are gone. Their only readers were the minted
// `??_7ContainerDescription@UI@@6B@`, its RT_DELDTOR row and the FACTUI1 row.
// ============================================================================
int RtUiContainerDescriptionRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@ContainerDescription` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@ContainerDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ContainerDescription::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x1800399c0ull, (void*)&UI::ContainerDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ContainerDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ContainerDescription::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x180039a40ull, (void*)&UI::ContainerDescription::CreateInPlaceObject);
    return n;
}
