// code/Game/Goal/finishtutorialinstance.cpp
// Clean reimplementation of FinishTutorialInstance vtable methods.
//
// FinishTutorialInstance (0x28 bytes) — goal instance for tutorial completion.
// Vtable: 0x140160478
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint32_t field08        — GoalInstance base (Internal base)
//   +0x0C: uint32_t field0C        — GoalInstance base
//   +0x10: int32_t  currentStep    — current tutorial step index
//   +0x18: void*    goalPtr        — InternalBasePtr to parent Goal
//   +0x20: void*    tutorialInst   — InternalBasePtr to TutorialInstance object
//
// Decompiled from Application-x64.exe using Ghidra.

#include <cstdio>
#include "Tutorial/FinishTutorialInstance.h"
#include "Tutorial/FinishTutorialGoal.h"
#include "Tutorial/TutorialInstance.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7 callers (K2): QUALIFIED base calls
#include "object/TypeInfo.h"    // ENDGAME B.3.7 callers (K2): Object::Base::IsDerivedFrom
#include <new>
#include <atomic>

// Startup/Update use function pointers from engine_funcs.h (resolved at startup).
// ENDGAME B.3.8 (B6-5): the one-shot diagnostic counters are namespace-scope
// atomics, not function-local statics — goal instances are built on the loader
// thread (GameUpdateLoop::Load) and ticked on the main thread.
namespace {
std::atomic<int> s_logStartup{0};
std::atomic<int> s_logUpdate{0};
std::atomic<int> s_logCreate{0};
}  // namespace

// ============================================================
// FinishTutorialInstance::Slot3 (vtable +0x18, 0x1400d7bf0)
// ============================================================
// Serialize/Persist: calls GoalInstance::Persist(base), creates Serializer,
// serializes InternalBasePtr at +0x20, then type-checks against TutorialInstance.
//
// Decompiled:
//   GoalInstance::Persist(this, stream);
//   Serializer local_20;
//   IO::Serializer::Serializer(local_20, stream, 0);
//   Object::InternalBasePtr::Serialize(&this->+0x20, local_20);
//   if (*(void**)(this+0x20) != NULL) {
//       if (!IsDerivedFrom(*(void**)(this+0x20), TypeInfo_TutorialInstance)) {
//           *(uint64_t*)(this+0x20) = 0;
//       }
//   }
//   IO::Serializer::~Serializer(local_20);
void FinishTutorialInstance::Persist(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Achievement::GoalInstance::Persist (slot 3, 0x18009ef70) — QUALIFIED (B7-13):
    // an unqualified call would dispatch back into this slot.
    Achievement::GoalInstance::Persist(stream);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x20
    reinterpret_cast<Object::InternalBasePtr*>((void*)(s + 0x20))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Type-check: if deserialized object is not a TutorialInstance, null it out
    void* obj = *(void**)(s + 0x20);
    if (obj != nullptr) {
        // Object::Base::IsDerivedFrom (import slot 0x140152a98) against the id in
        // TypeInfo_TutorialInstance (dword 0x1401c6668, TutorialInstance's cell —
        // Table 1), exactly retail's 0x1400d7c3a `MOV EDX,[0x1401c6668]` /
        // 0x1400d7c40 `CALL [0x140152a98]`.
        const uint32_t tidTutorialInstance = TutorialInstance::TypeId();
        bool isDerived = reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(tidTutorialInstance);   // B.3.7 bridge
        if (!isDerived) {
            *(uint64_t*)(s + 0x20) = 0;
        }
    }

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// FinishTutorialInstance::Slot4 (vtable +0x20, 0x1400d7a90)
// ============================================================
// Startup/Load: allocates a TutorialInstance (0x38-byte Internal object),
// initializes its fields, stores it in self+0x20, copies the tutorial
// definition reference from the parent FinishTutorialGoal, then calls
// the tutorial tree insert helper to register the tutorial dialog,
// and sets game world flags to enable tutorial UI.
//
// Decompiled from disassembly:
//   1. Allocate 0x38 bytes, call Object::Internal::Internal()
//   2. Set vtable to TutorialInstance vtable (0x140160bf8)
//   3. Set field+0x10 = -1, IBP::Ctor(+0x18, NULL), IBP::Ctor(+0x20, NULL), +0x30 = 0
//   4. IBP::Ctor(&local, newObj), store in self+0x20
//   5. IsDerivedFrom(self+0x18, FinishTutorialGoal) -> get goal pointer
//   6. CheckPtr(self+0x20), copy goal->+0x30 to tutInst->+0x30, set tutInst->+0x10 = 0
//   7. Call FinishTutorialGoal_helper_01f0 (tree insert) with:
//      - gameWorld+0x120 (tutorial tree root)
//      - tutDef->+0x20 (tutorial external ID)
//      - param3 (forwarded)
//      - *(int*)(*(void**)singleton_exref + 4) - 0x1d0
//   8. OR gameWorld+0xd0 flags with 0x5f0
//
// Retail passes R8 through to 0x1401001f0 untouched (Startup takes no
// argument; `?Startup@GoalInstance@Achievement@@UEAAXXZ`); the tree's
// TutWave_TutorialTreeInsert ignores that parameter, so null is passed.
void FinishTutorialInstance::Startup() {
    FinishTutorialInstance* self = this;
    void* p3 = nullptr;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Step 1-3: MemAlloc(0x38, 8) and the TutorialInstance ctor retail inlined
    // here (Internal(), stamp 0x140160bf8, +0x10 = -1, two null
    // InternalBasePtrs, +0x30 = 0). ENDGAME B.3.8: the real class's ctor —
    // the tree stamped `g_TutorialInstanceVtable` only when the opt-in
    // BANISHED_LIVE_TUTORIALINSTANCE_VT registration made 0x140160bf8 resolve,
    // and otherwise left Object::Internal's minted table on the object.
    void* newObj = Object::detail::InternalMemAlloc(sizeof(TutorialInstance), 8);
    if (newObj != nullptr) {
        newObj = static_cast<Object::Base*>(::new (newObj) TutorialInstance());
    }

    // Step 4: Create IBP from newObj, store in self+0x20
    // Use a local IBP buffer (8 bytes), then copy to self+0x20
    uint64_t localIBP = 0;
    if (fn_InternalBasePtr_Ctor) {
        new (&localIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(newObj));   // B.3.3 bridge
    }
    *(uint64_t*)(s + 0x20) = localIBP;

    // Step 5: IsDerivedFrom check on self+0x18 (goal pointer) against FinishTutorialGoal
    void* goalObj = *(void**)(s + 0x18);
    void* ftGoal = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_FinishTutorialGoal && goalObj) {
        bool isDerived = fn_IsDerivedFrom(goalObj, *g_TypeId_FinishTutorialGoal);
        if (isDerived) {
            ftGoal = goalObj;
        }
    }

    // Step 6: CheckPtr on self+0x20, then copy goal fields to tutInst

    uintptr_t tutInst = *(uintptr_t*)(s + 0x20);
    if (tutInst && ftGoal) {
        // Copy tutorial definition reference: tutInst->+0x30 = goal->+0x30
        *(uint64_t*)(tutInst + 0x30) = *(uint64_t*)((uintptr_t)ftGoal + 0x30);
        // Reset current step to 0
        *(int32_t*)(tutInst + 0x10) = 0;
    }

    { if (s_logStartup.fetch_add(1) == 0) {
        std::fprintf(stderr, "[FinTutInst] Slot4 INIT self=%p tutInst=%p goalObj=%p ftGoal=%p "
                             "tutDef=%p (goalTid=%u) treeInsert=%p\n",
                (void*)self, (void*)tutInst, goalObj, ftGoal,
                tutInst ? *(void**)(tutInst + 0x30) : nullptr,
                g_TypeId_FinishTutorialGoal ? *g_TypeId_FinishTutorialGoal : 0u,
                (void*)fn_TutorialTreeInsert);
        std::fflush(stderr); } }

    // Step 7: Call FinishTutorialGoal_helper_01f0 (tutorial tree insert)
    if (fn_TutorialTreeInsert && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        void* treeRoot = *(void**)(gameWorld + 0x120);

        // Read tutorial external ID from tutDef->+0x20
        uintptr_t tutDef = *(uintptr_t*)(tutInst + 0x30);
        uint64_t tutorialId = 0;
        if (tutDef) {
            tutorialId = *(uint64_t*)(tutDef + 0x20);
        }

        // Read screen height param from singleton exref
        int heightParam = 0;
        if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
            uintptr_t singletonObj = *(uintptr_t*)*g_pSingletonExref_151a80;
            if (singletonObj) {
                heightParam = *(int32_t*)(singletonObj + 4) - 0x1d0;
            }
        }

        fn_TutorialTreeInsert(treeRoot, tutorialId, p3, heightParam);
    }

    // Step 8: OR game world flags at +0xd0 with 0x5f0
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        *(uint32_t*)(gameWorld + 0xd0) |= 0x5f0;
    }
}

// ============================================================
// FinishTutorialInstance::Slot5 (vtable +0x28, 0x1400d7a50)
// ============================================================
// Execute/Update: checks the InternalBasePtr at +0x20 (TutorialInstance),
// then delegates to the tutorial step execution helper.
//
// Decompiled from disassembly:
//   1. CheckPtr(&self->+0x20)
//   2. Tail-call FinishTutorialInstance_helper_1db0(*(self+0x20), p2, p3, p4)
//
// Note: R8 (p3) and R9 (p4) are implicitly forwarded through the tail JMP.
//
// Update(const Time&, const GoalList&): RDX / R8 reach the helper unchanged;
// R9 is whatever the caller left (TutWave_StepExecute ignores it and R8).
void FinishTutorialInstance::Update(const System::Time& time, const Achievement::GoalList& list) {
    FinishTutorialInstance* self = this;
    void* p2 = const_cast<void*>(static_cast<const void*>(&time));
    void* p3 = const_cast<void*>(static_cast<const void*>(&list));
    void* p4 = nullptr;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // CheckPtr on IBP at +0x20

    // Read the TutorialInstance pointer
    void* tutInst = *(void**)(s + 0x20);
    { if (s_logUpdate.fetch_add(1) < 3) {
        std::fprintf(stderr, "[FinTutInst] Slot5 TICK self=%p tutInst=%p exec=%p\n",
                (void*)self, tutInst, (void*)fn_TutorialStepExecute); std::fflush(stderr); } }

    // Delegate to the tutorial step execution helper
    if (fn_TutorialStepExecute && tutInst) {
        fn_TutorialStepExecute(tutInst, p2, p3, (uint64_t)(uintptr_t)p4);
    }
}

// ============================================================
// FinishTutorialInstance::Slot6 (vtable +0x30, 0x1400d7bb0)
// ============================================================
// Check/Shutdown: checks whether the tutorial is complete.
// Reads the TutorialInstance via IBP at +0x20, compares:
//   tutorial->+0x30 (External* to Tutorial definition)
//   tutorialDef->+0x30 (total step count)
//   vs tutorialInst->+0x10 (current step)
//
// Returns: currentStep >= totalSteps
//
// Decompiled:
//   fn_InternalBasePtr_CheckPtr(&this->+0x20);
//   lVar1 = *(this+0x20);  // TutorialInstance*
//   return *(int*)(*(int64_t*)(lVar1+0x30) + 0x30) <= *(int*)(lVar1+0x10);
Achievement::Goal::GoalState FinishTutorialInstance::CheckGoalState() const {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // CheckPtr on IBP at +0x20

    // Read the TutorialInstance pointer
    uintptr_t tutInst = *(const uintptr_t*)(s + 0x20);
    if (tutInst == 0) return static_cast<Achievement::Goal::GoalState>(0);

    // Read tutorial definition pointer at tutInst+0x30
    uintptr_t tutDef = *(uintptr_t*)(tutInst + 0x30);
    if (tutDef == 0) return static_cast<Achievement::Goal::GoalState>(0);

    // Compare: totalSteps (tutDef+0x30) <= currentStep (tutInst+0x10)
    int totalSteps = *(int*)(tutDef + 0x30);
    int currentStep = *(int*)(tutInst + 0x10);

    return static_cast<Achievement::Goal::GoalState>((totalSteps <= currentStep) ? 1 : 0);
}

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// create 0x140026070: MemAlloc(0x28, 8), GoalInstance() (import slot
// 0x140152858), the vptr, +0x20 = 0 (the base ctor zeroes +0x10 and +0x18).
FinishTutorialInstance::FinishTutorialInstance() : _field20(nullptr) {}

// The dtor inside [0] 0x140026200 (and 0x1400d7c70): stamp, DeletePtr on the
// TutorialInstance handle at +0x20; the compiler then runs ~GoalInstance
// (re-stamp + ~Internal, the InternalFactory swap-remove).
FinishTutorialInstance::~FinishTutorialInstance() {
    if (fn_InternalBasePtr_DeletePtr) {
        reinterpret_cast< ::Object::InternalBasePtr*>(&_field20)->DeletePtr();   // B.3.3 bridge
    }
}

// [0] 0x140026200: ~FinishTutorialInstance, MemFree on &1.
Object::Base* FinishTutorialInstance::DeletingDtor(unsigned flags) {
    this->~FinishTutorialInstance();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

void* FinishTutorialInstance::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FinishTutorialInstance), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) FinishTutorialInstance());
}

// Retail's createInPlace is the imported Internal::CreateInPlaceObject (null);
// the tree's in-place build is kept (B38_RULES B8-13).
void* FinishTutorialInstance::CreateInPlaceObject(void* memory) {
    { if (s_logCreate.fetch_add(1) < 3) {
        std::fprintf(stderr, "[FinTutInst] CREATED (in place) mem=%p\n", memory); std::fflush(stderr); } }
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) FinishTutorialInstance());
}

DEFINE_EXE_VTABLE(FinishTutorialInstance, "_ZTV22FinishTutorialInstance")
DEFINE_EXE_TYPE(FinishTutorialInstance, "Achievement::GoalInstance")
