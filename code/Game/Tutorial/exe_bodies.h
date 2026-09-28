#pragma once
// code/Game/Tutorial/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Tutorial/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
//
// Every body the registry reaches by retail address, declared once, copied
// from its DEFINITION (linkage included), grouped by defining file; the
// retail address(es) it is registered under are the trailing comment. The
// defining .cpp includes this header so the compiler checks each pair.
// Readers call these by name (W1); nothing here changes what runs. Making
// them members is B.4's work.
//
// Also here: the declarations and inline bodies of this module that lived in
// engine/funcs/engine_funcs.h, moved verbatim with their comments (that
// header includes this one, so none of its includers changes).

#include <cstdint>
#include "engine_funcs.h"   // the globals the moved inline body still reads

// ---- find_ui_element_by_path.cpp
extern "C" void* FindUIElementByPath_Impl(void* outElementIBP, void* pathStr, uint64_t* outDialog);  // 0x1400e1870

// ---- tutorial_base.cpp
extern "C" void TutBase_SerializeStepArray(void* serializer, int count, void* arrayData, void*);  // 0x140026fb0
extern "C" void TutBase_SerializeImageArray(void* serializer, int count, void* arrayData, void*);  // 0x140028cf0
extern "C" void TutBase_SerializeSetupArray(void* serializer, int count, void* arrayData, void*);  // 0x140028f40
extern "C" void TutBase_SerializeGoalArray(void* serializer, int count, void* arrayData, void*);  // 0x140029190
extern "C" void TutBase_SerializeImageRef(void* fieldPtr, void* serializer);  // 0x140027ad0
extern "C" void TutBase_ResolveToolbarRef(void* stringField);  // 0x140027de0
extern "C" uint32_t TutBase_CountBuildingsByType(uintptr_t worldData, uintptr_t typePtr);  // 0x140092010
extern "C" uint32_t TutBase_CountProfessionsByType(uintptr_t worldData, uintptr_t typePtr);  // 0x1400910a0
extern "C" void* TutBase_CastToSliderElement(void* element);  // 0x140013920
extern "C" int WorkComponent_CalcMaxWorkers(uintptr_t workComp);  // 0x14012e5c0
extern "C" int WorkPlaceComponent_GetMaxWorkers(uintptr_t workplaceComp);  // 0x14012fbb0

// ---- tutorial_wave.cpp
extern "C" void TutWave_SerializeTutorialRef(void* field, void* serializer);  // 0x1400262b0
extern "C" void TutWave_SerializeDialogRef(void* field, void* serializer);  // 0x140028650
extern "C" void TutWave_TutorialTreeInsert(void* treeOwner, uint64_t tutorialId, void* , uint32_t value);  // 0x1401001f0
extern "C" void TutWave_SetAllDialogFlags(void* self, char show);  // 0x1400e2330
extern "C" void* TutWave_FindHighlightDialog(void* self, void* outIBP);  // 0x1400e24f0
extern "C" void TutWave_OpenHighlightDialog(void* self, char checkExisting);  // 0x1400e2700
extern "C" void TutWave_ShowListedElements(void* self);  // 0x1400e29f0
extern "C" void TutWave_StepExecute(void* self, void* param2, void* param3, uint64_t param4);  // 0x1400e1db0

// ---- inline bodies (moved from engine/funcs/engine_funcs.h; no .cpp)

// --- FindDialogByName (FUN_1400e1600) — REIMPLEMENTED inline ---
// Parses dialog name string (splits on ':'), gets type IDs via Factory::GetTypeIdFromName,
// walks UI::UserInterface dialog linked list with IsA checks, optionally matches controller type.
// Non-trivial return: RCX=retval(InternalBasePtr*), RDX=name(String*)
// NOTE: the body of exe 0x1400e1600 (the two door pointers once bound to it are gone).
inline void* FindDialogByName_Impl(void* retval, void* nameStr) {
    uintptr_t ret = (uintptr_t)retval;
    uintptr_t namePtr = *(uintptr_t*)nameStr;

    // Empty string or null -> return null
    if (namePtr == 0 || *(int*)(namePtr - 8) == 0) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    // Split string on ':'
    // Build a temporary array of substrings
    const char* str = (const char*)namePtr;
    int strLen = *(int*)(namePtr - 8);

    // Count parts (max 2 — "DialogType" or "DialogType:ControllerType")
    int colonPos = -1;
    for (int i = 0; i < strLen; i++) {
        if (str[i] == ':') { colonPos = i; break; }
    }

    // Extract part1 (and optionally part2)
    char part1[256] = {};
    char part2[256] = {};
    int partCount = 1;

    if (colonPos >= 0) {
        if (colonPos < 255) {
            memcpy(part1, str, colonPos);
            part1[colonPos] = 0;
        }
        int part2Len = strLen - colonPos - 1;
        if (part2Len > 0 && part2Len < 255) {
            memcpy(part2, str + colonPos + 1, part2Len);
            part2[part2Len] = 0;
        }
        partCount = 2;
    } else {
        int copyLen = (strLen < 255) ? strLen : 255;
        memcpy(part1, str, copyLen);
        part1[copyLen] = 0;
    }

    // Get type IDs from Factory (retail: **import slot 0x140152760)
    void* factoryObj = ::Object::Factory::_singleton;
    if (!factoryObj) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t dialogTypeId =
        reinterpret_cast<const ::Object::Factory*>(factoryObj)   // B.3.3 bridge
            ->GetTypeIdFromName(part1, true);
    if (dialogTypeId == 0) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t controllerTypeId = 0;
    if (partCount == 2 && part2[0] != 0) {
        controllerTypeId =
            reinterpret_cast<const ::Object::Factory*>(factoryObj)   // B.3.3 bridge
                ->GetTypeIdFromName(part2, true);
    }

    // Get UI singleton
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) {
        *(uintptr_t*)ret = 0;
        return retval;
    }
    UI::UserInterface* ui =
        *reinterpret_cast<UI::UserInterface**>((uintptr_t)gameWorld + 0x98);
    if (!ui || !fn_IsA) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    // Walk the dialog list.
    // `_dialogs.GetFirst()` IS retail's UI::UserInterface::GetFirstDialog
    // (0x180031150, literally `*retval = this->_dialogs._head`), reached here
    // through dispatch slot 0x140151ca0; the Iterator is the bare node pointer,
    // so `it.node` is exactly what retail hands to CheckPtr, and `++it` is
    // retail's `node = *(node + 0x10)`.
    // Original: LEA RDX,[RBP+0x48]; MOV RCX,RSI; CALL [0x140151ca0]
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        void* node = it.node;
        // CheckPtr on the iterator node (InternalBasePtr) before dereferencing.
        // Original: MOV RCX,RSI; CALL [0x140152ab0] (fn_InternalBasePtr_CheckPtr)

        // Deref node to get DialogInstance*
        void* dialog = *(void**)node;
        if (dialog) {
            // Check IsA against dialogTypeId
            if (fn_IsA(dialog, dialogTypeId)) {
                if (controllerTypeId == 0 || partCount != 2) {
                    // Match found, no controller check needed
                    *(uintptr_t*)ret = (uintptr_t)dialog;
                    return retval;
                }
                // Need to check controller type too.
                // Original reloads node and calls CheckPtr again before GetController:
                //   MOV RSI,[RBP+0x48]; MOV RCX,RSI; CALL CheckPtr
                //   MOV R8D,EBX(=controllerTypeId); LEA RDX,[retbuf]; MOV RCX,[RSI]; CALL GetController
                // GetController takes controllerTypeId as R8 — it does the type matching
                // internally and returns a matching controller or null.
                {
                    uintptr_t controllerBuf[2] = {};
                    ::new (static_cast<void*>(controllerBuf)) ::Object::InternalPtr<UI::DialogController>(
                        reinterpret_cast<UI::DialogInstance*>(*(void**)node)->GetController(controllerTypeId));  // B.3.6 bridge
                    if (controllerBuf[0] != 0) {
                        // Controller matched — return this dialog
                        *(uintptr_t*)ret = (uintptr_t)*(void**)node;
                        return retval;
                    }
                    // Controller didn't match — continue searching
                }
            }
        }
        // `++it` is retail's next-node step:
        // MOV RAX,[RBP+0x48]; MOV RSI,[RAX+0x10]; MOV [RBP+0x48],RSI
    }

    // Not found
    *(uintptr_t*)ret = 0;
    return retval;
}
