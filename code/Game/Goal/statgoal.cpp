// code/Game/Goal/statgoal.cpp
// StatGoal (0x80 bytes) — achievement goal requiring a stat comparison.
// ENDGAME B.3.8 block GOAL: a REAL class (Goal/StatGoal.h) with a
// compiler-generated vtable; the retail evidence per slot is in the header.
//
// Field layout (from StatGoal_Ctor at 0x1400daa80 and Serialize at 0x1400da8b0):
//   +0x30: int32  statTypeA           — stat type enum for first operand (default 5)
//   +0x34: int32  statTypeB           — stat type enum for second operand (default 5)
//   +0x38: int32  comparisonType      — comparison enum (0-11: EQ/NE/LT/LE/GT/GE, even=A, odd=B)
//   +0x3c: int32  fixedValue          — fixed comparison value when not using statB (default 1)
//   +0x40: bool   requireOtherGoalsMet — adds 1 to result if true
//   +0x48: String statAString         — resource name for stat A
//   +0x50: int32  statAGeneration     — ExternalFactory gen counter
//   +0x58: void*  statAExternal       — resolved External* for stat A
//   +0x60: String statBString         — resource name for stat B
//   +0x68: int32  statBGeneration     — ExternalFactory gen counter
//   +0x70: void*  statBExternal       — resolved External* for stat B
//   +0x78: int32  statFlags           — flags/category (default 0x20)
//   +0x7c: int32  statSubType         — sub-type index (default 1)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/StatGoal.h"
#include "Goal/goal_exe_helpers.h"
#include "Goal/statvaluedispatch.h"
#include "engine_funcs.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "resolve.h"
#include "system/String.h"
#include "system/Filename.h"
#include <new>
#include <cstdint>
#include <cstring>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7: QUALIFIED base calls

// The two exe helpers the slots call — FUN_1400265b0 (the lazy ExternalPtr
// resolve) and FUN_1400da4e0 (the stat value switch) — are ported
// (goal_exe_helpers.cpp, statvaluedispatch.cpp) and registered at those
// addresses by standalone/reg_goal.cpp. ENDGAME B.3.8 (B6-5): the members call
// them directly instead of a lazily filled ResolveAddr cache.

// ============================================================
// Construction / destruction
// ============================================================
// ctor 0x1400daa80: Goal(), the vptr, then the stat defaults.
StatGoal::StatGoal()
    : statTypeA(5), statTypeB(5), comparisonType(10), fixedValue(1),
      requireOtherGoalsMet(0), _pad41{}, statAString{}, statAGeneration(-1), _pad54{},
      statAExternal(nullptr), statBString{}, statBGeneration(-1), _pad6c{},
      statBExternal(nullptr), statFlags(0x20), statSubType(1) {}

// The dtor inside [0] 0x140026540: the two name Strings (B then A); the
// compiler then runs ~Goal. B.3.1 bridge: the ExternalPtr triples are bytes.
StatGoal::~StatGoal() {
    reinterpret_cast<System::String*>(statBString)->~String();   // B.3.1 bridge
    reinterpret_cast<System::String*>(statAString)->~String();   // B.3.1 bridge
}

// [0] 0x140026540: ~StatGoal -> ~Goal -> ~External, MemFree on &1.
Object::Base* StatGoal::DeletingDtor(unsigned flags) {
    this->~StatGoal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// StatGoal::Serialize (vtable +0x28, 0x1400da8b0)
// ============================================================
// Calls base Goal::Serialize first, then serializes the stat-specific fields.
// Retail ends in `MOV AL,1` (0x1400da9ee).
bool StatGoal::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Achievement::Goal::Serialize (slot 5, 0x18009ee20) — QUALIFIED (B7-13).
    Achievement::Goal::Serialize(stream);

    // Create serializer (stack-local, 24 bytes)
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // statTypeA / statTypeB / comparisonType, each through the UINT door
    // (rw_vtbl::kScalarUInt, readerWriter vtable +0x98). Retail 0x1400da8e2.
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x30), nullptr);   // B.3.4 bridge  (0x98 = uint door)  statTypeA
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x34), nullptr);   // B.3.4 bridge  (0x98 = uint door)  statTypeB
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x38), nullptr);   // B.3.4 bridge  (0x98 = uint door)  comparisonType

    // Serialize fixedValue (+0x3c) via Serialize_Int
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(fixedValue, nullptr);   // B.3.4 bridge

    // Serialize requireOtherGoalsMet (+0x40) via Serialize_Bool
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<bool*>(s + 0x40), nullptr);   // B.3.4 bridge

    // Serialize statA string (+0x48) via String::Serialize. Each leg is wrapped
    // in the Filename stack temporary retail's compiler emitted.
    {
        System::Filename filenameBuf;
        (void)filenameBuf;
        reinterpret_cast<System::String*>(s + 0x48)->Serialize(serializerBuf);   // B.3.1 bridge

        // If loading (mode == 0), resolve the ExternalPtr. Original (0x1400da960):
        // RAX = *(serializer+0) [the reader/writer], then *(int*)(RAX+8) == 0.
        void* reader = *(void**)(serializerBuf + 0);
        if (reader && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
            StatGoal_ResolveExternalPtr((void*)(s + 0x48));
        }
    }

    // Serialize statB string (+0x60)
    {
        System::Filename filenameBuf;
        (void)filenameBuf;
        reinterpret_cast<System::String*>(s + 0x60)->Serialize(serializerBuf);   // B.3.1 bridge

        void* reader = *(void**)(serializerBuf + 0);
        if (reader && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
            StatGoal_ResolveExternalPtr((void*)(s + 0x60));
        }
    }

    // statFlags (+0x78), the same UINT door. Retail 0x1400da9bb.
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x78), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    // Serialize statSubType (+0x7c) via Serialize_Int
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(statSubType, nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// StatGoal::Reconstruct [6] 0x140026520 / Destruct [7] 0x140001640
// ============================================================
// Retail: DeletingDtor(0) (destroy in place), then the ctor on `this`.
Object::External* StatGoal::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) StatGoal();
}
void StatGoal::Destruct() { DeletingDtor(0); }

// ============================================================
// StatGoal::CheckGoalState (vtable +0x40, 0x1400da720)
// ============================================================
// 1. Copies statA's ExternalPtr, resolves it, looks the stat value up
// 2. If comparisonType is odd, the same for statB; else fixedValue (+0x3c)
// 3. Switch on comparisonType (even/odd pairs share the comparison):
//    0,1 = EQ; 2,3 = NE; 4,5 = LT; 6,7 = LE; 8,9 = GT; 10,11 = GE
// 4. requireOtherGoalsMet: comparisonMet ? 2 : 1, otherwise comparisonMet ? 1 : 0

// Operand computation, split out unchanged so a diagnostic can read the two
// numbers the comparison is about to use. Retail has no such split; the
// instruction order inside is untouched.
void StatGoal::ComputeOperands(int& statValueA, int& comparisonValue) const {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    void* self = const_cast<StatGoal*>(this);

    // Local ExternalPtr struct layout matching original:
    //   [0x00..0x07]: String (8 bytes, copy)
    //   [0x08..0x0B]: int32 generation
    //   [0x10..0x17]: void* external ptr
    //   [0x28..0x2F]: parent ptr (points back to start)

    // --- Resolve statA ---
    uint8_t localA[0x30];
    memset(localA, 0, sizeof(localA));
    *(uintptr_t*)(localA + 0x28) = (uintptr_t)localA;  // parent ptr

    // B.3.1 bridge: the local ExternalPtr is still untyped bytes.
    new (localA) System::String(*reinterpret_cast<const System::String*>(s + 0x48));
    *(int32_t*)(localA + 0x08) = statAGeneration;          // generation
    *(void**)(localA + 0x10)   = statAExternal;            // external ptr
    *(uintptr_t*)(localA + 0x28) = (uintptr_t)localA;      // parent ptr

    StatGoal_ResolveExternalPtr(localA);
    void* extA = *(void**)(localA + 0x10);

    // Destroy string copy
    reinterpret_cast<System::String*>(localA)->~String();

    // Look up stat value A
    statValueA = static_cast<int>(StatGoal_StatValueDispatch(self, static_cast<uint32_t>(statTypeA), extA));

    // --- Determine comparison value ---
    comparisonValue = 0;
    if ((comparisonType & 1) != 0) {
        // Odd comparisonType: use statB
        uint8_t localB[0x30];
        memset(localB, 0, sizeof(localB));
        *(uintptr_t*)(localB + 0x28) = (uintptr_t)localB;

        new (localB) System::String(*reinterpret_cast<const System::String*>(s + 0x60));
        *(int32_t*)(localB + 0x08) = statBGeneration;
        *(void**)(localB + 0x10)   = statBExternal;
        *(uintptr_t*)(localB + 0x28) = (uintptr_t)localB;

        StatGoal_ResolveExternalPtr(localB);
        void* extB = *(void**)(localB + 0x10);

        reinterpret_cast<System::String*>(localB)->~String();

        comparisonValue = static_cast<int>(StatGoal_StatValueDispatch(self, static_cast<uint32_t>(statTypeB), extB));
    } else {
        // Even comparisonType: use fixedValue
        comparisonValue = fixedValue;
    }
}

Achievement::Goal::GoalState StatGoal::CheckGoalState() const {
    int statValueA = 0;
    int comparisonValue = 0;
    ComputeOperands(statValueA, comparisonValue);

    // --- Evaluate comparison ---
    bool comparisonMet = false;
    int cmpType = comparisonType;
    if (cmpType >= 0 && cmpType <= 11) {
        switch (cmpType) {
            case 0: case 1:   comparisonMet = (statValueA == comparisonValue); break;
            case 2: case 3:   comparisonMet = (statValueA != comparisonValue); break;
            case 4: case 5:   comparisonMet = (statValueA <  comparisonValue); break;
            case 6: case 7:   comparisonMet = (statValueA <= comparisonValue); break;
            case 8: case 9:   comparisonMet = (statValueA >  comparisonValue); break;
            case 10: case 11: comparisonMet = (statValueA >= comparisonValue); break;
        }
    }

    // Apply requireOtherGoalsMet modifier
    if (requireOtherGoalsMet != 0) {
        return static_cast<GoalState>(comparisonMet ? 2 : 1);
    }
    return static_cast<GoalState>(comparisonMet ? 1 : 0);
}

// ============================================================
// StatGoal_ProbeOperands — diagnostic accessor (not retail)
// ============================================================
// Returns the two numbers CheckGoalState compares. Uses the same member, so it
// can never disagree with the verdict.
extern "C" void StatGoal_ProbeOperands(void* goal, int* outA, int* outB) {
    int a = 0, b = 0;
    if (goal) reinterpret_cast<const StatGoal*>(goal)->ComputeOperands(a, b);   // B.3.8 bridge
    if (outA) *outA = a;
    if (outB) *outB = b;
}

// ============================================================
// StatGoal::Construct (vtable +0x50, 0x140026510)
// ============================================================
Object::External* StatGoal::Construct() {
    return ::new (static_cast<void*>(this)) StatGoal();
}

// create 0x1400264d0: MemAlloc(0x80, 8), then the ctor (0x1400daa80).
void* StatGoal::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatGoal), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) StatGoal());
}
// createInPlace 0x140026510.
void* StatGoal::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) StatGoal());
}

DEFINE_EXE_VTABLE(StatGoal, "_ZTV8StatGoal")

// B38_RULES D5: standalone/main.cpp's factory self-test (RunFactorySelfTest
// "StatGoal", 0x1401604c0) still reads the retail table address back and
// compares it with a factory-built object's vptr, so the exe address serves
// the COMPILER's table.
extern "C" void RegisterStatGoalVtable() {
    RegisterExeAddr(0x1401604c0, reinterpret_cast<uintptr_t>(StatGoal::Vtable()));
}

DEFINE_EXE_TYPE(StatGoal, "Achievement::Goal")
