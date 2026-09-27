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


// ENDGAME B.3.9: ProgressElement/ComponentEntity/AttendanceDescription/AttendanceComponent
// all have a ported TypeId() now, so the old EnsureTypeIds()/Ready() lazy-cache over the
// exe .rdata globals (TypeId_ProgressElement etc.) is dead — the static always answers.

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
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(UI::ProgressElement::TypeId()));  // B.3.6 bridge
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = fn_IsDerivedFrom(foundElem, UI::ProgressElement::TypeId());
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(progressPtr));   // B.3.3 bridge
    this->data30 = *(uint64_t*)basePtrBuf;

    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    void* attendComp = nullptr;
    fn_FindComponent_Attendance(entityInternal, &attendComp);

    // Get sub-component at +0x18, check type
    void* subComp = *(void**)((uintptr_t)attendComp + 0x18);
    bool isType = fn_IsDerivedFrom(subComp, AttendanceDescription::TypeId());
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
    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, ComponentEntity::TypeId());
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
