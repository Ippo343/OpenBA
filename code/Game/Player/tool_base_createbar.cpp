// code/Game/Player/tool_base_createbar.cpp
// ============================================================================
// CreateBar populate + position (GUI Keystone F, Phase 3B item 2)
// ============================================================================
// Faithful 1:1 reimplementation of the in-game building-placement "CreateBar"
// update path. PlacementTool::Serialize (and the Drag tools) call this every
// frame while a tool is active; it fills the CreateBar dialog's "textSize" /
// "textStatus" / "imageIcon" elements with the footprint size + cost/status
// text + red/white validity tint, projects the building footprint to screen
// space, and repositions (smoothed) the CreateBar dialog over that footprint.
//
// Ported exe functions (all UNCONDITIONAL — compile in BOTH dll + standalone):
//   operator_impl_fc60          0x1400ffc60  -> UpdateToolUI_fc60   (main)
//   operator_impl_fc60_sub_e600 0x14005e600  -> UpdateToolUI_e600   ("textSize")
//   operator_impl_e420          0x14005e420  -> UpdateToolUI_e420   ("textStatus")
//   FUN_14005e700               0x14005e700  -> UpdateToolUI_e700   (size-region elems)
//   FUN_14005e360               0x14005e360  -> UpdateToolUI_e360   ("imageIcon" tint)
//   operator_helper_ebb0        0x1400febb0  -> UpdateToolUI_ebb0   (box corners)
//
// Sub-callees resolved (not re-ported here):
//   CharacterComponent_impl_ec70 0x1400fec70  (REG) — build 4 footprint corners
//   FUN_1400fe960                0x1400fe960  (REG) — project corners -> screen rect
//   Runtime-x64.dll exports (engine_funcs + local ResolveExport for the by-value
//   Math/Volume/String helpers not present in engine_funcs.h).
//
// ABI NOTES (verified against the Ghidra disassembly — get these wrong and it
// hardfaults on the Microsoft x64 calling convention):
//   * MEMBER functions that return a struct BY VALUE use a hidden return buffer:
//     RCX=this, RDX=hidden_retbuf, R8+=args. This applies to:
//        Vector3::Min          ?Min@Vector3@Math@@QEBA?AV12@AEBV12@@Z
//        Transform::operator*  ??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z
//        UI::String -> System::String  ??BString@UI@@QEBA?AV0System@@XZ
//        DialogInstance::GetController (engine_funcs, retval-2nd)
//     -> the returned value is written to the retbuf AND returned in RAX.
//   * fn_UIString_Ctor ALWAYS takes 3 args (retbuf, stringTable, stringName);
//     Ghidra renders it with 2. e420 supplies R8 = the string-name arg.
//   * UI::String is 16 bytes; System::String is 8 bytes (COW refcounted). The
//     empty-string idiom is (*p==0 || *(int*)(*p-8)==0).
//   * type-id .rdata globals (DAT_1401c4d40 = "BuildRequirementUI" controller,
//     DWORD_1401c7f58 = "ToolbarDescription" component, ImageElement typeinfo)
//     read as 0 in standalone — filled lazily via ResolveAddr-then-GetTypeIdByName.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (both modes)
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace {

// ---------------------------------------------------------------------------
// By-value Math/Volume/String helpers not present in engine_funcs.h. Resolved by
// mangled export name from Runtime-x64.dll (ASLR handled by ResolveExport). Names
// recovered from the exe dispatch-slot name tables (0x140152900 / 0x140152950 /
// 0x140151470 / 0x1401528a8 / 0x140151308).
// ---------------------------------------------------------------------------
// Vector3::Min(this, retbuf, other) -> retbuf (member, hidden retbuf, componentwise min)
typedef float* (*Vec3Min_t)(const void* self, void* retbuf, const void* other);
// Transform::operator*(this, retbuf, vec) -> retbuf (member, transforms a point)
typedef float* (*TransformMul_t)(const void* self, void* retbuf, const void* vec);
// Box::GetCorners(this, outVec3x8) — fills 8 Vector3 (member, no retval)
typedef void (*BoxGetCorners_t)(const void* self, void* outCorners);
// Rectangle::Rectangle(this) — default ctor
typedef void* (*RectCtor_t)(void* self);
// UI::String::operator System::String(this, retbuf) -> retbuf (member, hidden retbuf)
typedef void* (*UIStrToSys_t)(const void* self, void* retbuf);
// FUN_1400fec70 (REG) — build 4 footprint corners from a tile rect
typedef uint32_t (*Ec70_t)(uint64_t, int, int, int, int, float*);
// FUN_1400fe960 (REG) — project N corners into a screen rectangle
typedef uint8_t (*Fe960_t)(uint64_t, uint32_t, float*, void*);

Vec3Min_t        s_fn_Vec3Min      = nullptr;
TransformMul_t   s_fn_TransformMul = nullptr;
BoxGetCorners_t  s_fn_BoxGetCorners = nullptr;
RectCtor_t       s_fn_RectCtor     = nullptr;
UIStrToSys_t     s_fn_UIStrToSys   = nullptr;
Ec70_t           s_fn_ec70         = nullptr;
Fe960_t          s_fn_fe960        = nullptr;

void EnsureCreateBarExports() {
    static bool s_ready = false;
    if (s_ready) return;
    s_fn_Vec3Min       = reinterpret_cast<Vec3Min_t>(ResolveExport("?Min@Vector3@Math@@QEBA?AV12@AEBV12@@Z"));
    s_fn_TransformMul  = reinterpret_cast<TransformMul_t>(ResolveExport("??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z"));
    s_fn_BoxGetCorners = reinterpret_cast<BoxGetCorners_t>(ResolveExport("?GetCorners@Box@Volume@@QEBAXPEAVVector3@Math@@@Z"));
    s_fn_RectCtor      = reinterpret_cast<RectCtor_t>(ResolveExport("??0Rectangle@Volume@@QEAA@XZ"));
    s_fn_UIStrToSys    = reinterpret_cast<UIStrToSys_t>(ResolveExport("??BString@UI@@QEBA?AV0System@@XZ"));
    // ec70 / fe960 are exe code (registered in standalone; real exe code in dll).
    s_fn_ec70  = reinterpret_cast<Ec70_t>(ResolveAddr(Module::Exe, 0x1400fec70));
    s_fn_fe960 = reinterpret_cast<Fe960_t>(ResolveAddr(Module::Exe, 0x1400fe960));
    // Latch once everything critical resolved (retry next call otherwise). The
    // by-value math/rect helpers + fe960 are the ones that would crash if null.
    if (s_fn_Vec3Min && s_fn_TransformMul && s_fn_BoxGetCorners && s_fn_RectCtor &&
        s_fn_UIStrToSys && s_fn_ec70 && s_fn_fe960)
        s_ready = true;
}

// DAT_1401c4d40 — "BuildRequirementUI" (parent UI::DialogController). The CreateBar
// controller type used to filter GetController. RegisterType_BuildRequirementUI.
uint32_t BuildReqUICtrlTypeId() {
    static uint32_t t = 0;
    if (!t) {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4d40));
        if (p) t = *p;                                        // dll: real .rdata type-id
        if (!t) t = ::Object::GetTypeIdByName("BuildRequirementUI");  // standalone fallback
    }
    return t;
}

// DWORD_1401c7f58 — "ToolbarDescription" (parent Description). The component on the
// tool's description entity that supplies textStatus content. ToolbarDescription_RegisterType.
uint32_t ToolbarDescTypeId() {
    static uint32_t t = 0;
    if (!t) {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7f58));
        if (p) t = *p;
        if (!t) t = ::Object::GetTypeIdByName("ToolbarDescription");
    }
    return t;
}

// e360 IsDerivedFrom target — UI::ImageElement type-id. In the exe this is read as
// *(uint32_t*)*(void**)0x140151500 (slot holds the ImageElement _typeInfo pointer).
uint32_t ImageElementTypeId() {
    static uint32_t t = 0;
    if (!t) {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151500));
        if (slot && *slot) t = *reinterpret_cast<uint32_t*>(*slot);   // dll path
        if (!t) {
            void* ti = ResolveExport("?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A");
            if (ti) t = *reinterpret_cast<uint32_t*>(ti);
        }
        if (!t) t = ::Object::GetTypeIdByName("ImageElement");
    }
    return t;
}

// System::String empty test: (*p==0) || (*(int*)(*p - 8) == 0).
inline bool StringIsEmpty(const void* sysStr) {
    const uint8_t* p = *reinterpret_cast<const uint8_t* const*>(sysStr);
    return (p == nullptr) || (*reinterpret_cast<const int*>(p - 8) == 0);
}

}  // namespace

// ============================================================================
// UpdateToolUI_e600 (0x14005e600) — set the "textSize" element
// ============================================================================
// param_1 = the BuildRequirementUI controller; param_2 = a System::String*.
// Finds "textSize" on the controller's DialogInstance (controller+0x18), sets it
// as text-parameter 0, and hides/shows the element by whether the string is empty.
extern "C" void UpdateToolUI_e600(void* ctrl, void* str) {
    uint8_t nameStr[8] = {};                          // System::String "textSize"
    if (fn_String_Ctor_CStr) fn_String_Ctor_CStr(nameStr, "textSize");

    void* dlgIBP = reinterpret_cast<uint8_t*>(ctrl) + 0x18;   // controller+0x18 = DialogInstance IBP
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(dlgIBP);
    void* dialog = *reinterpret_cast<void**>(dlgIBP);

    uint64_t elemBuf = 0;                              // InternalPtr retbuf
    if (fn_DialogInstance_FindElement) fn_DialogInstance_FindElement(dialog, &elemBuf, nameStr);
    void* elem = reinterpret_cast<void*>(elemBuf);     // *retbuf = element ptr
    if (fn_String_Dtor) fn_String_Dtor(nameStr);

    if (elem) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
        if (fn_TextElement_SetParameter) fn_TextElement_SetParameter(elem, 0, str);

        // hide/show by empty string
        uint8_t tmp[8] = {};
        if (fn_String_Ctor_Copy) fn_String_Ctor_Copy(tmp, str);
        bool isEmpty = StringIsEmpty(tmp);
        if (fn_String_Dtor) fn_String_Dtor(tmp);

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
        if (fn_Element_SetFlagsOnOff)
            fn_Element_SetFlagsOnOff(elem, isEmpty ? 1u : 0u, isEmpty ? 0u : 1u);
    }
}

// ============================================================================
// UpdateToolUI_e420 (0x14005e420) — set the "textStatus" element
// ============================================================================
// Scans the controller's description entity (controller+0x30) component list for
// the ToolbarDescription component, then finds "textStatus" and sets:
//   SetText(elem, UIString(comp.stringTable, costString))
//   SetParameter(elem, 0, (System::String)UIString(comp.stringTable, comp+0x40))
//   hide/show by whether costString is empty.
extern "C" void UpdateToolUI_e420(void* ctrl, void* costStr) {
    void* descEntity = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ctrl) + 0x30);
    if (!descEntity) return;

    // Find the ToolbarDescription-typed component (base +0x28, count +0x30).
    void* comp = nullptr;
    {
        int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(descEntity) + 0x30);
        uint32_t tid = ToolbarDescTypeId();
        for (int i = 0; i < count; i++) {
            uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(descEntity) + 0x28);
            void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(c) + 0x10))(c);
            if (t == tid) {
                void* obj = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                comp = (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, tid)) ? obj : nullptr;
                break;
            }
        }
    }

    uint8_t nameStr[8] = {};                           // System::String "textStatus"
    if (fn_String_Ctor_CStr) fn_String_Ctor_CStr(nameStr, "textStatus");
    void* dlgIBP = reinterpret_cast<uint8_t*>(ctrl) + 0x18;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(dlgIBP);
    void* dialog = *reinterpret_cast<void**>(dlgIBP);
    uint64_t elemBuf = 0;
    if (fn_DialogInstance_FindElement) fn_DialogInstance_FindElement(dialog, &elemBuf, nameStr);
    void* elem = reinterpret_cast<void*>(elemBuf);
    if (fn_String_Dtor) fn_String_Dtor(nameStr);
    if (!elem) return;

    // Defensive: the original assumes the ToolbarDescription component always
    // exists (comp != null) and unconditionally derefs comp+0x30. Guard it so a
    // standalone description entity that lacks the component leaves textStatus
    // empty instead of null-derefing. (The retail invariant holds; this only
    // matters for degraded standalone data.)
    if (!comp) return;

    void* stringTable = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(comp) + 0x30);

    // UIString_A = UIString(stringTable, costString); SetText(elem, UIString_A).
    uint8_t uiA[16] = {};
    if (fn_UIString_Ctor) fn_UIString_Ctor(uiA, reinterpret_cast<uint64_t>(stringTable), costStr);
    uint8_t uiA_copy[16];
    std::memcpy(uiA_copy, uiA, 16);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
    if (fn_TextElement_SetText) fn_TextElement_SetText(elem, uiA_copy);

    // UIString_B = UIString(stringTable, comp+0x40); SetParameter(elem, 0, (SysStr)UIString_B).
    uint8_t uiB[16] = {};
    if (fn_UIString_Ctor)
        fn_UIString_Ctor(uiB, reinterpret_cast<uint64_t>(stringTable),
                         reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(comp) + 0x40));
    uint8_t sysB[8] = {};
    if (s_fn_UIStrToSys) s_fn_UIStrToSys(uiB, sysB);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
    if (fn_TextElement_SetParameter) fn_TextElement_SetParameter(elem, 0, sysB);
    if (fn_String_Dtor) fn_String_Dtor(sysB);

    // hide/show by whether costString (UIString_A) is empty.
    uint8_t sysA[8] = {};
    if (s_fn_UIStrToSys) s_fn_UIStrToSys(uiA, sysA);
    uint8_t sysA_copy[8] = {};
    if (fn_String_Ctor_Copy) fn_String_Ctor_Copy(sysA_copy, sysA);
    bool isEmpty = StringIsEmpty(sysA_copy);
    if (fn_String_Dtor) fn_String_Dtor(sysA_copy);
    if (fn_String_Dtor) fn_String_Dtor(sysA);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
    if (fn_Element_SetFlagsOnOff)
        fn_Element_SetFlagsOnOff(elem, isEmpty ? 1u : 0u, isEmpty ? 0u : 1u);
}

// ============================================================================
// UpdateToolUI_e700 (0x14005e700) — set the per-axis size-region elements
// ============================================================================
// controller+0x38 = the size multiplier (param_8). For each of controller+0x58
// size-region elements (element IBP array at +0x40, size int array at +0x50):
//   text = "%i"  formatted from  sizeArray[i] * multiplier.
extern "C" void UpdateToolUI_e700(void* ctrl, uint32_t param_8) {
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(ctrl) + 0x38) = param_8;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(ctrl) + 0x58);
    if (count <= 0) return;
    for (int i = 0; i < count; i++) {
        uintptr_t sizeArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(ctrl) + 0x50);
        int mult = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(ctrl) + 0x38);
        int val = *reinterpret_cast<int*>(sizeArr + static_cast<int64_t>(i) * 4) * mult;

        uint8_t s[8] = {};
        if (fn_String_Format) fn_String_Format(s, "%i", static_cast<uint64_t>(static_cast<uint32_t>(val)));

        uintptr_t elemArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(ctrl) + 0x40);
        void* elemIBP = reinterpret_cast<void*>(elemArr + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
        void* elem = *reinterpret_cast<void**>(elemIBP);
        if (fn_TextElement_SetParameter) fn_TextElement_SetParameter(elem, 0, s);
        if (fn_String_Dtor) fn_String_Dtor(s);
    }
}

// ============================================================================
// UpdateToolUI_e360 (0x14005e360) — tint the "imageIcon" element
// ============================================================================
// Finds "imageIcon" on the controller's DialogInstance, type-checks it as a
// UI::ImageElement, and calls its virtual slot +0x50 (SetColor) with the tint
// (0xffffffff valid / 0xffff0000 invalid).
extern "C" void UpdateToolUI_e360(void* ctrl, uint32_t color) {
    uint8_t nameStr[8] = {};                           // System::String "imageIcon"
    if (fn_String_Ctor_CStr) fn_String_Ctor_CStr(nameStr, "imageIcon");

    void* dlgIBP = reinterpret_cast<uint8_t*>(ctrl) + 0x18;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(dlgIBP);
    void* dialog = *reinterpret_cast<void**>(dlgIBP);
    uint64_t elemBuf = 0;
    if (fn_DialogInstance_FindElement) fn_DialogInstance_FindElement(dialog, &elemBuf, nameStr);
    void* elemIBP = &elemBuf;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(elemIBP);
    void* obj = *reinterpret_cast<void**>(elemIBP);

    uint32_t imgTid = ImageElementTypeId();
    void* ptr = (obj && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, imgTid)) ? obj : nullptr;

    uint64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, ptr);
    if (fn_String_Dtor) fn_String_Dtor(nameStr);

    if (ibp != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibp);
        void* elem = reinterpret_cast<void*>(ibp);
        // virtual slot +0x50 = ImageElement::SetColor(this, color).
        void* vtbl = *reinterpret_cast<void**>(elem);
        auto setColor = *reinterpret_cast<void(**)(void*, uint32_t)>(reinterpret_cast<uintptr_t>(vtbl) + 0x50);
        setColor(elem, color);
    }
}

// ============================================================================
// UpdateToolUI_ebb0 (0x1400febb0) — box corners -> transformed Vector3[8]
// ============================================================================
// Fills outCorners[0..23] (8 Vector3) from the placement Box, clamping each
// corner componentwise to (1e6, 1e6, 2.5) then applying the mesh Transform.
// Returns 8 (the corner count). param_1 is a dummy the original never reads.
extern "C" uint32_t UpdateToolUI_ebb0(void* /*dummy*/, void* box, void* transform, float* outCorners) {
    if (s_fn_BoxGetCorners) s_fn_BoxGetCorners(box, outCorners);
    const float clampConst[3] = { 1000000.0f, 1000000.0f, 2.5f };  // 0x49742400,0x49742400,0x40200000
    float* corner = outCorners;
    for (int i = 0; i < 8; i++) {
        if (s_fn_Vec3Min) {
            float ret[3];
            float* r = s_fn_Vec3Min(corner, ret, clampConst);
            corner[0] = r[0]; corner[1] = r[1]; corner[2] = r[2];
        }
        if (s_fn_TransformMul) {
            float ret2[3];
            float* r2 = s_fn_TransformMul(transform, ret2, corner);
            corner[0] = r2[0]; corner[1] = r2[1]; corner[2] = r2[2];
        }
        corner += 3;
    }
    return 8;
}

// ============================================================================
// UpdateToolUI_fc60 (0x1400ffc60) — populate + position the CreateBar
// ============================================================================
// Signature MUST match placementtool.cpp's fn_operator_fc60_t exactly:
//   (dialogPtr, W, H, costStringPtr, valid, param_6, param_7, param_8,
//    startCell, endCell, box, transform)
// dialogPtr = the tool's DialogInstance IBP slot (first 8 bytes = DialogInstance*,
// +0x08/+0x0c = the cached smoothed x/y coords seeded to -1.0 by OpenToolDialog).
extern "C" void UpdateToolUI_fc60(void* param_1, int param_2, int param_3, void* param_4,
                                  char param_5, char param_6, char param_7, int param_8,
                                  int param_9, int param_10, void* param_11, void* param_12) {
    EnsureCreateBarExports();

    // No dialog open -> nothing to update.
    if (*reinterpret_cast<void**>(param_1) == nullptr) return;
    void* dialog = *reinterpret_cast<void**>(param_1);

    // Resolve the CreateBar controller (BuildRequirementUI) from the dialog.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    uint32_t ctrlTid = BuildReqUICtrlTypeId();
    uint64_t ctrlBuf = 0;
    if (fn_DialogInstance_GetController) fn_DialogInstance_GetController(dialog, &ctrlBuf, ctrlTid);
    void* ctrl = nullptr;
    if (ctrlBuf != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlBuf);
        void* c = reinterpret_cast<void*>(ctrlBuf);
        if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, ctrlTid)) ctrl = c;
    }
    uint64_t ctrlIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ctrlIBP, ctrl);
#ifdef STANDALONE_EXE
    // One-shot diagnostic: confirm the CreateBar controller resolved (type-ids correct)
    // — else the populate silently no-ops here. Logs once; A/B-safe (only when fc60 runs).
    {
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            fprintf(stderr, "[fc60] ctrlTid=%u ctrl=%p ctrlIBP=%llx (0 => wrong type-id, populate skipped)\n",
                    ctrlTid, ctrl, (unsigned long long)ctrlIBP);
            fflush(stderr);
        }
    }
#endif
    if (ctrlIBP == 0) return;  // no live controller

    // --- Size text (controller-cached) ---
    if (param_6 == 0) {
        uint8_t fmtBuf[8] = {};                        // System::String from Format
        if (param_7 == 0) {
            if (fn_String_Format)
                fn_String_Format(fmtBuf, "%ix%i",
                                 static_cast<uint64_t>(static_cast<uint32_t>(param_2)),
                                 static_cast<uint64_t>(static_cast<uint32_t>(param_3)));
        } else {
            if (fn_String_Format)
                fn_String_Format(fmtBuf, "%i",
                                 static_cast<uint64_t>(static_cast<uint32_t>(param_2)));
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlIBP);
        UpdateToolUI_e600(reinterpret_cast<void*>(ctrlIBP), fmtBuf);
        if (fn_String_Dtor) fn_String_Dtor(fmtBuf);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlIBP);
        UpdateToolUI_e700(reinterpret_cast<void*>(ctrlIBP), static_cast<uint32_t>(param_8));
    } else {
        uint8_t emptyStr[8] = {};                      // null System::String -> clears "textSize"
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlIBP);
        UpdateToolUI_e600(reinterpret_cast<void*>(ctrlIBP), emptyStr);
        if (fn_String_Dtor) fn_String_Dtor(emptyStr);
    }

    // --- Status text (cost) + icon tint ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlIBP);
    UpdateToolUI_e420(reinterpret_cast<void*>(ctrlIBP), param_4);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlIBP);
    uint32_t color = (param_5 != 0) ? 0xffffffffu : 0xffff0000u;
    UpdateToolUI_e360(reinterpret_cast<void*>(ctrlIBP), color);

    // --- Build the 8 footprint corners (default-constructed = zero) ---
    float corners[24] = {};   // 8 * Vector3
    uint32_t cornerCount = 0;
    const bool boxPath = (param_9 < 0) || (param_10 < 0);
    if (boxPath) {
        // Single building / no drag: use the mesh Box, clamped + transformed, then
        // an extra componentwise Min against (1e6, 1e6, 3.0).
        cornerCount = UpdateToolUI_ebb0(nullptr, param_11, param_12, corners);
        const float clampConst[3] = { 1000000.0f, 1000000.0f, 3.0f };  // 0x49742400,0x49742400,0x40400000
        for (int i = 0; i < 8; i++) {
            if (s_fn_Vec3Min) {
                float ret[3];
                float* r = s_fn_Vec3Min(&corners[i * 3], ret, clampConst);
                corners[i * 3 + 0] = r[0];
                corners[i * 3 + 1] = r[1];
                corners[i * 3 + 2] = r[2];
            }
        }
    } else {
        // Drag rectangle: project start/end cells to world corners.
        int mapW = *reinterpret_cast<int*>(
            *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8));
        if (s_fn_ec70 && mapW != 0) {
            cornerCount = s_fn_ec70(
                static_cast<uint64_t>(static_cast<uint32_t>(param_9 % mapW)),  // p1 (unused by ec70)
                param_9 % mapW,     // col_start
                param_9 / mapW,     // row_start
                param_10 % mapW,    // col_end
                param_10 / mapW,    // row_end
                corners);
        }
    }

    // --- Project corners into a screen rectangle ---
    float rect[4] = {};   // Volume::Rectangle {minX, minY, maxX, maxY}
    if (s_fn_RectCtor) s_fn_RectCtor(rect);
    uint8_t ok = s_fn_fe960 ? s_fn_fe960(0, cornerCount, corners, rect) : 0;
    if (ok == 0) return;

    // --- Read the dialog root bounds (+0x68 left, +0x6c top, +0x70 right, +0x74 bottom) ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    void* rootIBP = fn_DialogInstance_GetRoot ? fn_DialogInstance_GetRoot(dialog) : nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(rootIBP);
    void* rootElem = rootIBP ? *reinterpret_cast<void**>(rootIBP) : nullptr;
    if (!rootElem) return;
    int left   = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rootElem) + 0x68);
    int top    = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rootElem) + 0x6c);
    int right  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rootElem) + 0x70);
    int bottom = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rootElem) + 0x74);

    // --- Compute the centered target position from the projected rect ---
    const float minX = rect[0], minY = rect[1], maxX = rect[2], maxY = rect[3];
    float cx = (minX + maxX) * 0.5f;                   // DAT_140172114 = 0.5
    float cy = boxPath ? minY                          // box branch: cy = rect min-y
                       : (minY + maxY) * 0.5f;         // cells branch: cy = rect centroid-y
    int halfW = (right - left) >> 1;
    int halfH = (bottom - top) >> 1;
    int screenX = static_cast<int>(cx) - halfW;
    int screenY = static_cast<int>(cy) - halfH;

    // --- Smooth toward the target (0.25 new / 0.75 cached); first frame (cached<0) snaps ---
    float* cachedX = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(param_1) + 0x08);
    float* cachedY = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(param_1) + 0x0c);
    if (*cachedX >= 0.0f) {
        *cachedX = static_cast<float>(screenX) * 0.25f + *cachedX * 0.75f;  // DAT_140172100 / DAT_140172124
        *cachedY = static_cast<float>(screenY) * 0.25f + *cachedY * 0.75f;
    } else {
        *cachedX = static_cast<float>(screenX);
        *cachedY = static_cast<float>(screenY);
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    if (fn_DialogInstance_SetUpperLeft)
        fn_DialogInstance_SetUpperLeft(dialog, static_cast<int>(*cachedX),
                                       static_cast<int>(*cachedY), true);
#ifdef STANDALONE_EXE
    {
        static bool s_posLogged = false;
        if (!s_posLogged) {
            s_posLogged = true;
            fprintf(stderr, "[fc60] positioned CreateBar: rect=(%.1f,%.1f,%.1f,%.1f) "
                    "root=(%d,%d,%d,%d) -> upperLeft=(%d,%d)\n",
                    rect[0], rect[1], rect[2], rect[3], left, top, right, bottom,
                    static_cast<int>(*cachedX), static_cast<int>(*cachedY));
            fflush(stderr);
        }
    }
#endif
}
