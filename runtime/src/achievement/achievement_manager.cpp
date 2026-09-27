// ============================================================================
// runtime/src/achievement/achievement_manager.cpp — Wave 5, lane B
// Achievement::Achievement / AchievementInstance / AchievementManager
// ============================================================================
//
// Ports 18 of the 19 CODE exports our tree calls on these three classes
// (docs/runtime_port/WAVE5_LANE_B.md has the per-function table). Retail
// addresses are in Runtime-x64.dll, image base 0x180000000; profile addresses
// are in Runtime-x64-profile.dll, the ToolKit build, which is the source of
// NAMES only (PLAN.md §6 step 1). Both were read in Ghidra on 2026-09-06.
//
// HELD BACK by Wave 5, deliberately, with the reasoning in the wave doc:
//   ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z (0x18009e910)
//     -- it inlines an IO::Serializer construction, and Wave 4 lane A measured
//       that a Serializer ctor CONSUMES OR EMITS A 12-BYTE STREAM HEADER
//       (runtime/types/io/Serializer.h). So it has no runnable oracle of any
//       kind: a second construction desyncs the save.
//
// *** THAT HOLDBACK IS DISCHARGED. WAVE 23 LANE ACHIEVE, 2026-09-11. ***
// The blocker was real when written and stopped being true in Wave 9, when lane
// S built the save-stream emission oracle: it ran each side against its OWN
// INDEPENDENT SYNTHETIC SINK, so there is no shared stream to desync and each
// side constructs its own codec. Lane P discharged the IDENTICAL blocker, in
// the identical words, for Goal::Serialize and GoalInstance::Persist
// (runtime/src/achievement/goal_serialize.cpp). Persist is now ported in
// runtime/src/achievement/achievement_serialize.cpp, under a PREFIX oracle
// (stopAfter = 1) because its element loop mutates the global Object::Factory.
// The paragraph above is kept rather than deleted: it is the evidence for
// PLAN rule "re-check a stated blocker before acting on it", and this one
// survived fourteen waves because nobody re-read it.
//   ?_singleton@AchievementManager@Achievement@@0PEAV12@EA and the two
//   ?_typeInfo@… symbols are DATA exports; they cannot be registered before
//   cutover (same finding as Wave 4 lane D's FileSystem::_singleton).
//
// STATE. Only nine of these functions are pure reads; the other nine mutate
// the live singleton, a live instance, or the Windows registry.
//
// NO ADDED NULL GUARDS. Retail derefs `_achievements` in GetAchievementCount,
// `_completed._data` in Update and `_instances._data` in Reset without
// checking any of them, and those faults are load-bearing: they are how a
// manager that never got SetAchievements announces itself
// (reference_achievementmanager_has_no_list_until_setachievements.md, whose
// "faults on a READ of 0x28" is re-derived and confirmed in the header). The
// ports below fault in exactly the same places, now at an address inside
// reimpl-x64.exe rather than inside Runtime-x64.dll.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>          // placement new into the retslot doors

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/FileSystem.h"
#include "../../types/object/External.h"   // Object::ExternalPtr<T>, ExternalList
#include "../../types/object/Internal.h"   // Object::InternalPtr<T>

// ENDGAME B.3.7 block B3: every body below is a MEMBER of
// `Achievement::AchievementManager`, `AchievementInstance` or `Achievement`
// (runtime/types/achievement/AchievementManager.h); every registered door is a
// one-line QUALIFIED delegation. ENDGAME B.3.9 ENG: the two CONSTRUCTOR doors
// (`??0Achievement`, `??0AchievementInstance`), which stamped the MINTED
// tables, are gone with those rows -- the classes' real constructors are the
// only bodies, and the factory pairs are the classes' own statics below.
//
// The callers inside this file that used to reach sibling members through
// ResolveExport name doors (`ReadAchievements`, `SetAchievementMet`,
// `CreateInstance`, `GoalList::Initialize`, `Base::IsDerivedFrom`,
// `~External`) and by address (`Array<ExternalPtr<Goal>>::operator=`
// 0x180003970) now call the members directly.

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ?_singleton@AchievementManager@Achievement@@0PEAV12@EA  (0x1801fc670) — stays a
// data slot until the class-static conversion at the end of B.3 (carry-over 5).
Achievement::AchievementManager** ManagerSingletonCell() {
    return RT_DATA_SLOT(Achievement::AchievementManager**,
        "?_singleton@AchievementManager@Achievement@@0PEAV12@EA");
}

// ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A  (0x1801f9898). The FIRST
// DWORD is the type id — GetAchievement's own `MOV EDX,dword ptr [0x1801f9898]`
// — i.e. retail's static accessor `Achievement::TypeId()` (0x180001030).
uint32_t AchievementTypeId() { return Achievement::Achievement::TypeId(); }

// ?_singleton@FileSystem@IO@@0PEAV12@EA  (0x1801fc6a8)
RtIO::FileSystem** FileSystemSingletonCell() {
    return RT_DATA_SLOT(RtIO::FileSystem**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

// AchievementManager::Update's gate, inlined in retail as `CMP dword
// [RAX+0x38],0 / JG done` — `IO::FileSystem::AreUserPackFilesEnabled`
// (0x1800acfc0): the user (mod) pack-file COUNT is positive.
bool UserPackFilesEnabled(const RtIO::FileSystem* fs) {
    return fs->userPackFilenameCount > 0;
}

}  // namespace

namespace Achievement {

// ############################################################################
// #  AchievementManager
// ############################################################################

// static AchievementManager* Get()  retail 0x1800079d0 — `return _singleton;`
AchievementManager* AchievementManager::Get() {
    return *ManagerSingletonCell();
}

// int GetAchievementCount() const  retail 0x1800079b0 — `_achievements->_count`,
// UNGUARDED: a manager with no SetAchievements faults on a read of 0x28, the
// recorded symptom and the only signal that the list is missing.
int AchievementManager::GetAchievementCount() const {
    return _achievements->_count;
}

// bool IsAchievementAwarded(int) const  retail 0x1800079c0 — one byte of
// `_completed`, no bounds check.
bool AchievementManager::IsAchievementAwarded(int index) const {
    return _completed._data[index];
}

// ExternalPtr<Achievement> GetAchievement(int) const  retail 0x18009e8a0
// (door: RCX=this, RDX=retval, R8D=index). `_resources[index]` if it derives
// from Achievement, else null — Base::IsDerivedFrom (0x1800c0390).
Object::ExternalPtr<Achievement> AchievementManager::GetAchievement(int index) const {
    Object::External* obj = _achievements->_resources[index];
    return Object::ExternalPtr<Achievement>(
        obj->IsDerivedFrom(AchievementTypeId()) ? static_cast<Achievement*>(obj) : nullptr);
}

// InternalPtr<AchievementInstance> GetAchievementInstance(int) const
// retail 0x18009e8f0 — one move, no bounds check, no handle validation.
Object::InternalPtr<AchievementInstance> AchievementManager::GetAchievementInstance(int index) const {
    return _instances._data[index];
}

// void SetEventHandler(void (*)(int))  retail 0x18009e6b0 — one store to +0x28.
void AchievementManager::SetEventHandler(EventHandler handler) {
    _eventHandler = handler;
}

// ============================================================================
// void SetAchievements(ExternalPtr<ExternalList>)  retail 0x18009e860
//     _achievements = list;                         ; `MOV [RCX],RDX` — BARE (B7-5)
//     _updateTimes.SetSize(list->_count);           ; 0x180008ff0 kSpec_U64Zero
//     _completed.SetSize(_achievements->_count);    ; 0x180008b90 kSpec_Bool, RE-READ
//     ReadAchievements();
// It does NOT size `_instances` — that is Reset's job on a new game and
// Persist's on a load.
// ============================================================================
void AchievementManager::SetAchievements(Object::ExternalPtr<Object::ExternalList> list) {
    _achievements = list.Get();
    _updateTimes.SetSize(_achievements->_count);
    _completed.SetSize(_achievements->_count);
    ReadAchievements();
}

// ============================================================================
// void Reset()  retail 0x18009e5c0 — THE INSTANCE BUILDER.
// Sizes `_instances` to the achievement count (0x180008d60, kSpec_PtrZero) and
// fills every slot with Achievement::CreateInstance. The per-element type test
// is retail's INLINED Base::IsDerivedFrom — the member here. A missing
// registration still faults rather than yielding null (retail derefs the
// found node unconditionally, and so does the port).
// ============================================================================
void AchievementManager::Reset() {
    _instances.SetSize(_achievements->_count);

    const uint32_t achievementTypeId = AchievementTypeId();
    for (int i = 0; i < _instances._size; ++i) {
        Object::External* obj = _achievements->_resources[i];
        Achievement* achievement =
            obj->IsDerivedFrom(achievementTypeId) ? static_cast<Achievement*>(obj) : nullptr;
        _instances._data[i] = achievement->CreateInstance();
    }
}

// ============================================================================
// void Update(const System::Time&)  retail 0x18009e6c0 / profile 0x180028cd0
//
// The rotating updater. Once per tick:
//   0. If any user pack file (mod) is mounted, do nothing at all.
//   1. Accumulate the delta into _updateTimes[i] for every not-yet-completed
//      achievement; hard-zero the slot of every completed one.
//   2. Walk the cursor past completed achievements; if it skips all, return.
//   3. Update the ONE instance at the cursor with its accumulated time, zero
//      that slot, and if the instance is now met, award it and fire the handler.
//   4. Advance the cursor, wrapping (on a RE-READ `_instances._size`).
// Retail INLINES AchievementInstance::Update and ::IsMet; the members are
// called. The unguarded `_completed._data[_lastAchievementUpdate]` read is
// retail's and faults when SetAchievements never ran.
// ============================================================================
void AchievementManager::Update(const System::Time& time) {
    if (UserPackFilesEnabled(*FileSystemSingletonCell())) return;

    const int count = _instances._size;
    for (int i = 0; i < count; ++i) {
        if (!_completed._data[i]) _updateTimes._data[i]._ticks += time._ticks;
        else                      _updateTimes._data[i]._ticks = 0;
    }

    int skipped = 0;
    while (_completed._data[_lastAchievementUpdate]) {
        if (skipped >= count) break;
        ++_lastAchievementUpdate;
        ++skipped;
        if (_lastAchievementUpdate >= count) _lastAchievementUpdate = 0;
    }
    if (skipped == count) return;

    int index = _lastAchievementUpdate;
    _instances._data[index]->Update(_updateTimes._data[index]);
    _updateTimes._data[index]._ticks = 0;

    index = _lastAchievementUpdate;
    AchievementInstance* inst = _instances._data[index].Get();
    if (inst->IsMet()) {
        if (!_completed._data[index] && SetAchievementMet(index)) {
            if (_eventHandler) _eventHandler(_lastAchievementUpdate);
        }
    }

    ++_lastAchievementUpdate;
    if (_lastAchievementUpdate >= _instances._size) _lastAchievementUpdate = 0;
}

// ############################################################################
// #  AchievementInstance
// ############################################################################

// ??1AchievementInstance@Achievement@@UEAA@XZ  0x18009e400 — EMPTY: the
// compiler emits the re-stamp, `~GoalList()` on `_goalList` and `~Internal()`.
AchievementInstance::~AchievementInstance() {}

// ============================================================================
// bool IsMet() const  retail 0x18009e2f0
// An EMPTY goal list is NOT met (retail's explicit `count == 0` test).
// Otherwise every goal must have bit 0 set and bit 1 clear — tested in that
// order, as retail's two branches do. Elements dereferenced UNGUARDED.
// ============================================================================
bool AchievementInstance::IsMet() const {
    const int count = _goalList._goals._size;
    if (count == 0) return false;
    for (int i = 0; i < count; ++i) {
        const uint32_t flags = _goalList._goals._data[i]->_flags;
        if ((flags & GoalInstance::kFlagMet) == 0)    return false;
        if ((flags & GoalInstance::kFlagFailed) != 0) return false;
    }
    return true;
}

// bool HasFailed() const  retail 0x18009e2b0 — true if ANY goal has bit 1.
bool AchievementInstance::HasFailed() const {
    const int count = _goalList._goals._size;
    for (int i = 0; i < count; ++i) {
        if ((_goalList._goals._data[i]->_flags & GoalInstance::kFlagFailed) != 0)
            return true;
    }
    return false;
}

// ============================================================================
// void Update(const System::Time&)  retail 0x18009e340
// Forwards the tick to every goal instance through GoalInstance SLOT 5 —
// `(this, const Time&, const GoalList&)`, passing THIS INSTANCE'S OWN list
// (`LEA R8,[RDI+0x10]`). A VIRTUAL call, on purpose: retail dispatches, and
// the instance is an exe subclass (TimeGoalInstance, FinishTutorialInstance).
// ============================================================================
void AchievementInstance::Update(const System::Time& time) {
    const int count = _goalList._goals._size;
    for (int i = 0; i < count; ++i)
        _goalList._goals._data[i]->Update(time, _goalList);
}

// ============================================================================
// void SetAchievement(ExternalPtr<Achievement>)  retail 0x18009e440
//     MOV [RCX+0x20],RDX            ; store FIRST (the argument is BARE, B7-5)
//     temp = achievement->_goals    ; 0x180003970, a deep copy into a fresh buffer
//     _goalList.Initialize(temp)    ; by value — the CALLEE tears `temp` down
// A null achievement faults at `achievement + 0x48`, as retail does.
// ============================================================================
void AchievementInstance::SetAchievement(Object::ExternalPtr<Achievement> achievement) {
    _achievement = achievement;

    GoalArray temp;
    temp.Assign(achievement->_goals);
    _goalList.Initialize(&temp);
}

// ############################################################################
// #  Achievement (the External resource)
// ############################################################################

// ??1Achievement@0@UEAA@XZ  0x18009e180 — EMPTY: see the header. The
// compiler's member destruction order (`_goals`, `_spriteName`,
// `_description`, `_title`) is retail's, then ~External().
Achievement::~Achievement() {}

// ?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ  0x180001130
// The FACTREST row this replaces: MemAlloc of 0x58 bytes, align 8 (the locked
// `?MemAlloc@MemoryAllocator@System@@` Object::detail::InternalMemAlloc
// reaches); retail's null branch; then the inlined constructor.
Object::Base* Achievement::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Achievement), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) Achievement();
}
// ?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z  0x1800011f0
Object::Base* Achievement::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) Achievement();
}

// ?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ
// 0x1800018b0 — MemAlloc(0x28, 8); retail's null branch; the inlined ctor
// (CALL ??0Internal, the stamp, three zero qwords). The in-place half is the
// shared `?CreateInPlaceObject@Internal@Object@@`.
Object::Base* AchievementInstance::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AchievementInstance), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) AchievementInstance();
}

// int GetFileVersion() const  retail 0x18009e170 — `return 3;` Scenario's
// override ORs 0x600 onto it (Scenario_Slot3, exe 0x1400a9460).
int32_t Achievement::GetFileVersion() const { return 3; }

// String GetTitle() / GetDescription() / GetSpriteName() const
// retail 0x1800013e0 / 0x180001420 / 0x1800013a0 — the same three-instruction
// body on a different field: the String copy constructor (an ADDREF).
System::String Achievement::GetTitle() const       { return _title; }
System::String Achievement::GetDescription() const { return _description; }
System::String Achievement::GetSpriteName() const  { return _spriteName; }

}  // namespace Achievement

// ############################################################################
// The registered doors — one-line QUALIFIED delegations. Retslot doors keep
// `(this, ret, …)` and placement-new the member's prvalue (B5-12).
// ############################################################################

Achievement::AchievementManager* RtAch_AchievementManager_Get() {
    return Achievement::AchievementManager::Get();
}

int RtAch_AchievementManager_GetAchievementCount(const Achievement::AchievementManager* self) {
    return self->Achievement::AchievementManager::GetAchievementCount();
}

bool RtAch_AchievementManager_IsAchievementAwarded(const Achievement::AchievementManager* self, int index) {
    return self->Achievement::AchievementManager::IsAchievementAwarded(index);
}

void* RtAch_AchievementManager_GetAchievement(const Achievement::AchievementManager* self,
                                              void* retval, int index) {
    new (retval) Object::ExternalPtr<Achievement::Achievement>(
        self->Achievement::AchievementManager::GetAchievement(index));
    return retval;
}

void* RtAch_AchievementManager_GetAchievementInstance(const Achievement::AchievementManager* self,
                                                      void* retval, int index) {
    new (retval) Object::InternalPtr<Achievement::AchievementInstance>(
        self->Achievement::AchievementManager::GetAchievementInstance(index));
    return retval;
}

void RtAch_AchievementManager_SetEventHandler(Achievement::AchievementManager* self, void (*handler)(int)) {
    self->Achievement::AchievementManager::SetEventHandler(handler);
}

// The ExternalPtr argument is one pointer in RDX (B7-5).
void RtAch_AchievementManager_SetAchievements(Achievement::AchievementManager* self,
                                              Object::ExternalList* list) {
    self->Achievement::AchievementManager::SetAchievements(
        Object::ExternalPtr<Object::ExternalList>(list));
}

void RtAch_AchievementManager_Reset(Achievement::AchievementManager* self) {
    self->Achievement::AchievementManager::Reset();
}

void RtAch_AchievementManager_Update(Achievement::AchievementManager* self, const System::Time* time) {
    self->Achievement::AchievementManager::Update(*time);
}

const Achievement::GoalList* RtAch_AchievementInstance_GetGoalList(const Achievement::AchievementInstance* self) {
    return &self->Achievement::AchievementInstance::GetGoalList();
}

bool RtAch_AchievementInstance_IsMet(const Achievement::AchievementInstance* self) {
    return self->Achievement::AchievementInstance::IsMet();
}

bool RtAch_AchievementInstance_HasFailed(const Achievement::AchievementInstance* self) {
    return self->Achievement::AchievementInstance::HasFailed();
}

void RtAch_AchievementInstance_Update(Achievement::AchievementInstance* self, const System::Time* time) {
    self->Achievement::AchievementInstance::Update(*time);
}

// The ExternalPtr argument is one pointer in RDX (B7-5).
void RtAch_AchievementInstance_SetAchievement(Achievement::AchievementInstance* self,
                                              Achievement::Achievement* achievement) {
    self->Achievement::AchievementInstance::SetAchievement(
        Object::ExternalPtr<Achievement::Achievement>(achievement));
}

void* RtAch_Achievement_GetTitle(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetTitle());
    return retval;
}

void* RtAch_Achievement_GetDescription(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetDescription());
    return retval;
}

void* RtAch_Achievement_GetSpriteName(const Achievement::Achievement* self, void* retval) {
    new (retval) System::String(self->Achievement::Achievement::GetSpriteName());
    return retval;
}

// ============================================================================
// Registration (unchanged). Every name is ONE string literal on ONE line —
// the census sees no other spelling. Address rows registered alongside every
// name (scenario.cpp reaches the Achievement lifecycle BY ADDRESS).
// ============================================================================
int RtAchManagerRegister() {
    int n = 0;

    // ---- AchievementManager, pure reads ------------------------------------
    RegisterRuntimeExport("?Get@AchievementManager@Achievement@@SAPEAV12@XZ", (void*)&RtAch_AchievementManager_Get); ++n;
    RegisterRuntimeAddr(0x1800079d0, (void*)&RtAch_AchievementManager_Get);

    RegisterRuntimeExport("?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ", (void*)&RtAch_AchievementManager_GetAchievementCount); ++n;
    RegisterRuntimeAddr(0x1800079b0, (void*)&RtAch_AchievementManager_GetAchievementCount);

    RegisterRuntimeExport("?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z", (void*)&RtAch_AchievementManager_IsAchievementAwarded); ++n;
    RegisterRuntimeAddr(0x1800079c0, (void*)&RtAch_AchievementManager_IsAchievementAwarded);

    RegisterRuntimeExport("?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z", (void*)&RtAch_AchievementManager_GetAchievement); ++n;
    RegisterRuntimeAddr(0x18009e8a0, (void*)&RtAch_AchievementManager_GetAchievement);

    RegisterRuntimeExport("?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z", (void*)&RtAch_AchievementManager_GetAchievementInstance); ++n;
    RegisterRuntimeAddr(0x18009e8f0, (void*)&RtAch_AchievementManager_GetAchievementInstance);

    // ---- AchievementManager, mutators --------------------------------------
    RegisterRuntimeExport("?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z", (void*)&RtAch_AchievementManager_SetEventHandler); ++n;
    RegisterRuntimeAddr(0x18009e6b0, (void*)&RtAch_AchievementManager_SetEventHandler);

    RegisterRuntimeExport("?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z", (void*)&RtAch_AchievementManager_SetAchievements); ++n;
    RegisterRuntimeAddr(0x18009e860, (void*)&RtAch_AchievementManager_SetAchievements);

    RegisterRuntimeExport("?Reset@AchievementManager@Achievement@@QEAAXXZ", (void*)&RtAch_AchievementManager_Reset); ++n;
    RegisterRuntimeAddr(0x18009e5c0, (void*)&RtAch_AchievementManager_Reset);

    RegisterRuntimeExport("?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z", (void*)&RtAch_AchievementManager_Update); ++n;
    RegisterRuntimeAddr(0x18009e6c0, (void*)&RtAch_AchievementManager_Update);

    // ---- AchievementInstance ------------------------------------------------
    // ---- the factory pairs (B39_RULES D2): the classes' OWN statics. The
    // minted `?_typeInfo@` records resolve them by name (MintTypeInfos runs
    // after every *PortInit). AchievementInstance's in-place half is the shared
    // `?CreateInPlaceObject@Internal@Object@@` (Object::Internal's).
    RegisterRuntimeExport("?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ", (void*)&Achievement::Achievement::CreateNewObject); ++n;
    RegisterRuntimeAddr(0x180001130, (void*)&Achievement::Achievement::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z", (void*)&Achievement::Achievement::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr(0x1800011f0, (void*)&Achievement::Achievement::CreateInPlaceObject);
    RegisterRuntimeExport("?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ", (void*)&Achievement::AchievementInstance::CreateNewObject); ++n;
    RegisterRuntimeAddr(0x1800018b0, (void*)&Achievement::AchievementInstance::CreateNewObject);

    RegisterRuntimeExport("?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z", (void*)&RtAch_AchievementInstance_SetAchievement); ++n;
    RegisterRuntimeAddr(0x18009e440, (void*)&RtAch_AchievementInstance_SetAchievement);

    RegisterRuntimeExport("?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z", (void*)&RtAch_AchievementInstance_Update); ++n;
    RegisterRuntimeAddr(0x18009e340, (void*)&RtAch_AchievementInstance_Update);

    RegisterRuntimeExport("?IsMet@AchievementInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_AchievementInstance_IsMet); ++n;
    RegisterRuntimeAddr(0x18009e2f0, (void*)&RtAch_AchievementInstance_IsMet);

    RegisterRuntimeExport("?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ", (void*)&RtAch_AchievementInstance_HasFailed); ++n;
    RegisterRuntimeAddr(0x18009e2b0, (void*)&RtAch_AchievementInstance_HasFailed);

    RegisterRuntimeExport("?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ", (void*)&RtAch_AchievementInstance_GetGoalList); ++n;
    RegisterRuntimeAddr(0x180001960, (void*)&RtAch_AchievementInstance_GetGoalList);

    // ---- Achievement (the External resource) --------------------------------
    RegisterRuntimeExport("?GetTitle@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetTitle); ++n;
    RegisterRuntimeAddr(0x1800013e0, (void*)&RtAch_Achievement_GetTitle);

    RegisterRuntimeExport("?GetDescription@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetDescription); ++n;
    RegisterRuntimeAddr(0x180001420, (void*)&RtAch_Achievement_GetDescription);

    RegisterRuntimeExport("?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ", (void*)&RtAch_Achievement_GetSpriteName); ++n;
    RegisterRuntimeAddr(0x1800013a0, (void*)&RtAch_Achievement_GetSpriteName);

    printf("[RtPort]   Achievement::AchievementManager/Instance/Achievement: %d exports "
           "registered here\n", n);
    return n;
}
