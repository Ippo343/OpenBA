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
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0x90.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtContainerDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblContainerDescription); }
void* VtContainerElement()     { return RT_DATA_SLOT(void*, RtUI::kVtblContainerElement); }
void* VtClipRegionElement()    { return RT_DATA_SLOT(void*, RtUI::kVtblClipRegionElement); }
const uint8_t* ContainerTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ContainerDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* ContainerCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0ContainerDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

extern "C" uint32_t RtUi_ContainerDescription_Type(const void*) {
    const uint8_t* ti = ContainerTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ContainerDescription_TypeName(const void*) {
    const uint8_t* ti = ContainerTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ContainerDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_ContainerDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                   // 180039a73
    return ContainerCtor(selfv);                            // 180039a68 JMP
}
extern "C" void* RtUi_ContainerDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                    // 180039a5e
    return ContainerCtor(selfv);                            // 180039a68 JMP
}
extern "C" void RtUi_ContainerDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1ContainerDescription@UI@@UEAA@XZ   0x1800ccff0
// ============================================================================
extern "C" void RtUi_ContainerDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtContainerDescription();                          // 1800cd00d
    *v = VtElementDescription();                            // 1800cd017
    ReleaseStringField(p, 0x60);                            // 1800cd01a

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cd04c JMP
}

// ============================================================================
// ?Serialize@ContainerDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cced0
// ============================================================================
// THE FIELD ORDER IS NOT THE STRUCT ORDER, and it is taken from this body, not
// from the layout: three ints (+0x68, +0x6c, +0x74), then the three child
// descriptions, and only THEN the two bools at +0x70/+0x71 — which sit between
// the ints in memory. Sorting them into offset order would change the byte
// stream a .rsc parses.
extern "C" bool RtUi_ContainerDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                 // 1800cceea

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt, p + 0x68);                   // 1800ccf2e
    RwScalar(rw, kRwScalarInt, p + 0x6c);                   // 1800ccf43
    RwScalar(rw, kRwScalarInt, p + 0x74);                   // 1800ccf58

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x78, &ser);     // 1800ccf5e
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x80, &ser);     // 1800ccf6c
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x88, &ser);     // 1800ccf7d

    RwScalar(rw, kRwScalarBool, p + 0x70);                  // 1800ccf9d
    RwScalar(rw, kRwScalarBool, p + 0x71);                  // 1800ccfaf

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
//  4. child = CreateElement(dialog, &slot, self->_childDesc[2]) attached to the
//     CLIP REGION, not the container.
//  5. clip->vt[0x20](clip) TWICE. Two identical virtual calls in a row; kept.
//  6. the size negotiation (see below)
//  7. the container's own five fields are copied from the description
//  8. slot = clip ; clip->_parent = container ; push clip into container
//  9. two more children from _childDesc[1] and _childDesc[0], in that order —
//     DESCENDING, and after the clip region, so the final child order is
//     [clip, desc1, desc0].
//
// STEP 6, TRANSCRIBED LITERALLY because its shape is not obvious:
//     w = clip->+0xc8
//     h = self->_height ; if (h <= 0) h = clip->+0xcc
//     t = self->_width  ; if (t >  0) w = t
//     if (clip->+0xc0 != w || clip->+0xc4 != h) { store both; mark the chain }
// i.e. the description's extents override the clip region's measured ones only
// when positive, and the mark is skipped when nothing changed.
extern "C" void** RtUi_ContainerDescription_CreateInstance(const void* selfv,
                                                           void** ret,
                                                           const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    // ---- 1. the container ---------------------------------------------------
    void* rawC = MemAlloc(0xd8, 8);                         // 1800ccbd6
    uint8_t* container = nullptr;
    if (rawC != nullptr) {                                  // 1800ccc01
        ElementCtor(rawC);                                  // 1800ccc06
        uint8_t* e = static_cast<uint8_t*>(rawC);
        *reinterpret_cast<void**>(e)           = VtContainerElement();  // 1800ccc13
        *reinterpret_cast<int32_t*>(e + 0xc8)  = 0;                     // 1800ccc18
        *reinterpret_cast<uint16_t*>(e + 0xcc) = 0;                     // 1800ccc1e WORD
        *reinterpret_cast<int32_t*>(e + 0xd0)  = 0;                     // 1800ccc25
        container = e;
    }

    // ---- 2. the base applier ------------------------------------------------
    void* slot = container;                                 // 1800ccc31
    ElementDescriptionApply(self, &slot);                   // 1800ccc3e

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

    // ---- 4. the clip region's child -----------------------------------------
    void** made = DialogCreateElement(dialog, &slot,
                                      *reinterpret_cast<void* const*>(self + 0x88)); // 1800cccda
    AttachAndPushChild(clip, made);                         // 1800ccce7 .. 1800ccd24

    // ---- 5. two identical virtual calls -------------------------------------
    {
        using LayoutFn = void (*)(void*);
        reinterpret_cast<LayoutFn>(VTableOf(clip)[0x20 / 8])(clip);     // 1800ccd2c
        reinterpret_cast<LayoutFn>(VTableOf(clip)[0x20 / 8])(clip);     // 1800ccd35
    }

    // ---- 6. the size negotiation --------------------------------------------
    {
        int32_t w = *reinterpret_cast<int32_t*>(clip + 0xc8);           // 1800ccd38
        int32_t h = *reinterpret_cast<const int32_t*>(self + 0x6c);     // 1800ccd3e
        if (h <= 0) h = *reinterpret_cast<int32_t*>(clip + 0xcc);       // 1800ccd46
        const int32_t t = *reinterpret_cast<const int32_t*>(self + 0x68);
        if (t > 0) w = t;                                              // 1800ccd52 CMOVG
        if (*reinterpret_cast<int32_t*>(clip + 0xc0) != w ||            // 1800ccd55
            *reinterpret_cast<int32_t*>(clip + 0xc4) != h) {            // 1800ccd5d
            *reinterpret_cast<int32_t*>(clip + 0xc0) = w;               // 1800ccd65
            *reinterpret_cast<int32_t*>(clip + 0xc4) = h;               // 1800ccd6b
            MarkNeedsSizeCalc(clip);                                    // 1800ccd80
        }
    }

    // ---- 7. the container's own fields --------------------------------------
    *reinterpret_cast<int32_t*>(container + 0xc8) =
        *reinterpret_cast<const int32_t*>(self + 0x74);     // 1800ccd96
    *reinterpret_cast<int32_t*>(container + 0xc0) =
        *reinterpret_cast<const int32_t*>(self + 0x68);     // 1800ccda0
    *reinterpret_cast<int32_t*>(container + 0xc4) =
        *reinterpret_cast<const int32_t*>(self + 0x6c);     // 1800ccdaa
    *reinterpret_cast<uint8_t*>(container + 0xcc) =
        *reinterpret_cast<const uint8_t*>(self + 0x70);     // 1800ccdb4 BYTE
    *reinterpret_cast<uint8_t*>(container + 0xcd) =
        *reinterpret_cast<const uint8_t*>(self + 0x71);     // 1800ccdbf BYTE

    // ---- 8. the clip region becomes the container's first child -------------
    slot = clip;                                            // 1800ccdca
    *reinterpret_cast<void**>(clip + kElemParent) = container;   // 1800ccdcf
    {
        using PushFn = void (*)(void*, int, const void*);
        RtAddr<PushFn>(0x180076900ull)(
            container + kElemChildren,
            *reinterpret_cast<int32_t*>(container + kElemChildren + 8) + 1,
            &slot);                                         // 1800ccdea
        MarkNeedsSizeCalc(container);                       // 1800ccdf2
    }

    // ---- 9. the container's two remaining children, DESCENDING --------------
    made = DialogCreateElement(dialog, &slot,
                               *reinterpret_cast<void* const*>(self + 0x80));  // 1800cce17
    AttachAndPushChild(container, made);                    // 1800cce24 .. 1800cce54

    made = DialogCreateElement(dialog, &slot,
                               *reinterpret_cast<void* const*>(self + 0x78));  // 1800cce62
    AttachAndPushChild(container, made);                    // 1800cce6f .. 1800ccea4

    *ret = container;                                       // 1800ccea6
    return ret;
}

int RtUiContainerDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ContainerDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ContainerDescription_Type);            ++n;
    RegisterRuntimeAddr(0x1800398b0ull, (void*)&RtUi_ContainerDescription_Type);

    RegisterRuntimeExport("?TypeName@ContainerDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ContainerDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800398d0ull, (void*)&RtUi_ContainerDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ContainerDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ContainerDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800ccfe0ull, (void*)&RtUi_ContainerDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ContainerDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180039a70ull, (void*)&RtUi_ContainerDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ContainerDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ContainerDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180039a50ull, (void*)&RtUi_ContainerDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ContainerDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ContainerDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180039a80ull, (void*)&RtUi_ContainerDescription_Destruct);

    RegisterRuntimeExport("??1ContainerDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ContainerDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800ccff0ull, (void*)&RtUi_ContainerDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ContainerDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ContainerDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800cced0ull, (void*)&RtUi_ContainerDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ContainerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ContainerDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800ccb90ull, (void*)&RtUi_ContainerDescription_CreateInstance);

    return n;
}
