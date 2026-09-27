// engine/src/ui/attendanceui.cpp
// Clean reimplementation of AttendanceUI game logic.
//
// AttendanceUI (0x38 bytes) — attendance progress bar panel.
// Extends DialogController. Fields:
//   +0x30 progressElement (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ProgressElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/AttendanceComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/AttendanceDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)

// ---- AttendanceUI: PROMOTED to the real reimpl (2026-08-15) ------------------
// Symptom: the school panel read "students" with no number in front of it.
//
// Both bodies have been ported in code/Game/Dialog/attendanceui.cpp since that
// file was written, and both were wired into NOTHING — the generic stub's 48
// no-op slots. This is the "stub sweep is a WIRING job" shape again
// (project_next_stub_controller_sweep): a ported body is not a reached body.
//
// The count is not a label of its own; it is TEXT PARAMETER 0 of the panel's
// ProgressElement ("{0} students" in the .rsc). With idx7 a no-op the parameter
// was never set, so the element rendered its literal remainder — the number was
// not wrong, it was absent, which is exactly what "students" with nothing in
// front of it looks like.
//
// Slot indices verified from the vtable BYTES at 0x14015b070 (11 slots wide; the
// RTTI name string "Attendan..." starts immediately after idx10):
//   idx4 (+0x20) 0x14005dd40 Slot4 — find the ProgressElement, set its MAXIMUM
//                                    from AttendanceDescription+0x20 (capacity)
//   idx7 (+0x38) 0x14005dc50 Slot7 — per-frame: set current value + text param 0
// Every other slot is one of the shared 0x14003a9xx no-op thunks, so wiring
// these two is full behavioural coverage for this class.


namespace attendanceui_sa {
    // ProgressElement's type-id .rdata slot is null unless BANISHED_LIVE_ELEMENT_TIDS
    // is on, and IsDerivedFrom(elem, 0) is always false — which would leave +0x30 null
    // and turn Slot4's +0x124 write into a null-deref. Same private by-name resolve
    // orchardui.cpp and cropui_promote.cpp use, for the same reason. See
    // reference_gated_element_typeid_disables_controller.
    static uint32_t s_progress = 0;
    void EnsureTypeIds() {
        static uint32_t s_entity=0, s_attDesc=0, s_attComp=0;
        if (!g_TypeId_ProgressElement || !*g_TypeId_ProgressElement) {
            if (!s_progress) s_progress = ::Object::ResolveTypeIdByNameLazy("ProgressElement");
            if (s_progress) g_TypeId_ProgressElement = &s_progress;
        }
        if (!g_TypeId_ComponentEntity) {
            s_entity = ComponentEntity::TypeId();
            if (s_entity) g_TypeId_ComponentEntity = &s_entity;
        }
        if (!g_TypeId_AttendanceDescription || !*g_TypeId_AttendanceDescription) {
            s_attDesc = AttendanceDescription::TypeId();
            if (s_attDesc) g_TypeId_AttendanceDescription = &s_attDesc;
        }
        // fn_FindComponent_Attendance searches by this one. Its .rdata slot
        // (0x1401c3f40) went unregistered until 2026-08-15 — the 2026-07-19 audit
        // registered the Description 0x38 bytes later and missed the Component — so
        // the finder ran with a null id, matched nothing, and Slot4's faithful
        // `attendComp+0x18` read [0+0x18] the first time anyone clicked a school.
        // reg_economy_typeids.cpp now supplies the storage; this keeps a resolve
        // failure a skip rather than a repeat of that crash.
        if (!g_TypeId_AttendanceComponent || !*g_TypeId_AttendanceComponent) {
            s_attComp = AttendanceComponent::TypeId();
            if (s_attComp) g_TypeId_AttendanceComponent = &s_attComp;
        }
    }
    // Both reimpls dereference these globals unconditionally, as retail does. Skip
    // rather than fault if any failed to resolve — a standalone-only failure mode
    // (retail's .rdata is always populated), so the guard is here at the boundary
    // rather than inside the faithful bodies.
    bool Ready() {
        return g_TypeId_ProgressElement && *g_TypeId_ProgressElement
            && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
            && g_TypeId_AttendanceDescription && *g_TypeId_AttendanceDescription
            && g_TypeId_AttendanceComponent && *g_TypeId_AttendanceComponent;
    }
}  // namespace uistub_AttendanceUI

// ============================================================
// AttendanceUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds ProgressElement by type in dialog, stores at +0x30.
// Gets AttendanceComponent, reads max capacity from sub-component,
// sets as progress maximum.
//
// Original: 0x14005DD40.

// [4] 0x14005dd40 — find the ProgressElement, set its maximum (capacity).
void AttendanceUI::OnCreated() {
    attendanceui_sa::EnsureTypeIds();
    if (!attendanceui_sa::Ready()) return;
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(*g_TypeId_ProgressElement));  // B.3.6 bridge
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = fn_IsDerivedFrom(foundElem, *g_TypeId_ProgressElement);
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(progressPtr));   // B.3.3 bridge
    this->data30 = *(uint64_t*)basePtrBuf;

    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    void* attendComp = nullptr;
    fn_FindComponent_Attendance(entityInternal, &attendComp);

    // Get sub-component at +0x18, check type
    void* subComp = *(void**)((uintptr_t)attendComp + 0x18);
    bool isType = fn_IsDerivedFrom(subComp, *g_TypeId_AttendanceDescription);
    void* typedComp = isType ? subComp : nullptr;

    // Set progress maximum from sub-component's capacity field (+0x20)
    int maxVal = *(int*)((uintptr_t)typedComp + 0x20);
    void* progress = (void*)this->data30;
    *(float*)((uintptr_t)progress + 0x124) = (float)maxVal;
}

// ============================================================
// AttendanceUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Gets current attendance count from component, updates progress
// bar value and text.
//
// Original: 0x14005DC50.

// [7] 0x14005dc50 — per-frame: current value + text parameter 0.
void AttendanceUI::OnUpdateLayout() {
    attendanceui_sa::EnsureTypeIds();
    if (!attendanceui_sa::Ready()) return;
    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    void* attendComp = nullptr;
    fn_FindComponent_Attendance(entityInternal, &attendComp);

    // Get current count from component +0x30
    int currentCount = *(int*)((uintptr_t)attendComp + 0x30);

    // Set progress current value
    void* progress = (void*)this->data30;
    *(float*)((uintptr_t)progress + 0x128) = (float)currentCount;

    // Format count as text and set on progress element
    System::String countStr = System::String::Format("%d", (unsigned int)currentCount);
    reinterpret_cast<UI::ProgressElement*>(progress)->SetProgressTextParameter(0, countStr);  // B.3.6 bridge
}
