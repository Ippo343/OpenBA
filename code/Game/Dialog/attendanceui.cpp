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
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// AttendanceUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds ProgressElement by type in dialog, stores at +0x30.
// Gets AttendanceComponent, reads max capacity from sub-component,
// sets as progress maximum.
//
// Original: 0x14005DD40.

void AttendanceUI_Slot4(AttendanceUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    fn_Element_FindElementType(dialogElem, &result, *g_TypeId_ProgressElement);
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = fn_IsDerivedFrom(foundElem, *g_TypeId_ProgressElement);
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(progressPtr));   // B.3.3 bridge
    self->data30 = *(uint64_t*)basePtrBuf;

    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
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
    void* progress = (void*)self->data30;
    *(float*)((uintptr_t)progress + 0x124) = (float)maxVal;
}

// ============================================================
// AttendanceUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Gets current attendance count from component, updates progress
// bar value and text.
//
// Original: 0x14005DC50.

void AttendanceUI_Slot7(AttendanceUI* self) {
    // Get entity, find AttendanceComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
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
    void* progress = (void*)self->data30;
    *(float*)((uintptr_t)progress + 0x128) = (float)currentCount;

    // Format count as text and set on progress element
    System::String countStr = System::String::Format("%d", (unsigned int)currentCount);
    fn_ProgressElement_SetProgressTextParameter(progress, 0, &countStr);
}
