#pragma once
// code/Game/Tutorial/exe_bodies.h -- the exe bodies DEFINED in code/Game/Tutorial/ that the registry
// reaches by retail address (B3R_RULES R5, block W0-P0c).
//
// One declaration per body, copied from its DEFINITION (linkage included),
// grouped by defining file; the retail address(es) the body is registered
// under are the trailing comment. The defining .cpp includes this header so
// the compiler checks each pair. Readers call these by name (W1); nothing
// here changes what runs. Making them members is B.4's work.

#include <cstdint>

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
