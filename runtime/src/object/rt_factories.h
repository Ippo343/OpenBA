#pragma once
// ============================================================================
// runtime/src/object/rt_factories.h — WAVE 28 LANE FACTREST
//
// The shared shape of the `?Create{New,InPlace}Object@<C>@<NS>@@` family, for
// the 35 NON-UI classes of the absent-factory roster.
//
// WHY A TABLE AND NOT 70 HAND-WRITTEN BODIES
// ------------------------------------------
// Every member of this family has one of exactly three shapes, and Wave 26 lane
// FACTORY measured that before porting the first one:
//
//   (a) `xor eax,eax ; ret`                   -- the class is not instantiable
//                                                through the object model.
//   (b) locked MemAlloc(N,8), retail's null check, an INLINED constructor
//       (a straight run of stores, no control flow), return.
//   (c) the same, with the constructor OUT OF LINE as a `??0C@NS@@QEAA@XZ`
//       call (and the in-place half a `test rcx,rcx / jne <ctor>` tail call).
//
// Only three things vary within (b) and (c): the allocation size, the sequence
// of vptr stamps, and which bytes the constructor writes. All three are read
// mechanically from the disassembly, so the port stores them AS DATA and runs
// one interpreter over them. Every step carries the retail instruction address
// it was transcribed from, so a reviewer can check any single row against
// `objdump -d Runtime-x64.dll` without reading this file's logic at all.
//
// The step lists in the per-namespace files were produced by a scratchpad
// transcription script reading `objdump -d` output from the retail DLL on disk
// — NOT typed by hand, and NOT read out of Ghidra's decompiled C
// (reference_ghidra_c_output_vs_disassembly.md). The script is not delivered:
// Wave 28 carries no tooling (WAVE28_KICKOFF.md §5.1). What is delivered is its
// output plus the two derived checks in WAVE28_LANE_FACTREST.md §3.
//
// WHAT THIS IS WORTH, AND WHEN
// ----------------------------
// Nothing on an ordinary run. The DLL reaches its own factories through an
// in-module pointer stamped by a static initialiser, which is not a door
// (Wave 11 lane OBJINT). Every row here registers with ATTENDANCE 0 while the
// DLL is present, and that is EXPECTED, not a defect. The value is entirely at
// cutover, where the minted BaseTypeInfo is the only node and an unported
// factory is the mint's own per-slot FATAL.
// ============================================================================

#include <cstdint>

enum class RtFactOp : uint8_t {
    End = 0,
    Vptr,   // store a `??_7...@6B@` vtable pointer at +off (8 bytes)
    Imm,    // store an immediate of `size` bytes at +off
    Self,   // store (this + imm) at +off (8 bytes)
    Call,   // call `name(this)` -- an out-of-line constructor
};

struct RtCtorStep {
    RtFactOp    kind;
    uint8_t     size;   // Imm: 1/2/4/8. Vptr/Self: 8. Call: ignored.
    uint16_t    off;    // byte offset into the object
    uint64_t    imm;    // Imm: the value. Self: the displacement.
    const char* name;   // Vptr: the ??_7 export. Call: the ??0 export.
    uint64_t    va;     // retail instruction address this step came from
};

struct RtFactoryClass {
    const char*       typeName;     // "Graphics::AnimationGroup"
    uint32_t          allocSize;    // 0 => CreateNewObject is the null stub
    uint32_t          allocAlign;
    const RtCtorStep* newSteps;     // nullptr => CreateNewObject returns null
    const RtCtorStep* inPlaceSteps; // nullptr => CreateInPlaceObject returns null
    uint64_t          newVa;
    uint64_t          inPlaceVa;
    uint64_t*         counters;     // [0]=New entries [1]=InPlace entries [2]=nulls
};

struct RtFactoryReg {
    const char*           newName;
    uint64_t              newVa;
    void*                 newFn;
    const char*           inPlaceName;
    uint64_t              inPlaceVa;
    void*                 inPlaceFn;
    const RtFactoryClass* cls;
};

// The two entry shapes. Both are shared by every class in the roster.
void* RtFactoryNew(const RtFactoryClass& c);
void* RtFactoryInPlace(const RtFactoryClass& c, void* p);

// Registers both doors (NAME and ADDRESS) for every row of `regs`, and returns
// the number of ROWS registered (not slots). Callers are the per-namespace
// registrars below.
int RtFactoryRegisterAll(const RtFactoryReg* regs, size_t n);

// WAVE 29 LANE VERIFY — enumerate the registered roster.
// Fills `out` with up to `cap` row pointers and returns the TRUE row count, so
// a caller that passed too small a buffer can SAY SO rather than silently
// report on a prefix. Valid only after RtFactoriesPortInit() has run; returns 0
// when FACTREST is disabled, which is the correct reading (nothing registered).
size_t RtFactoryRoster(const RtFactoryReg** out, size_t cap);

// Per-namespace registrars, all called from RtFactoriesPortInit().
int RtFactoriesObjectRegister();
int RtFactoriesVideoRegister();
// (RtFactoriesGraphicsRegister retired with its last row, ENDGAME B.3.9 GFX.)
// (RtFactoriesIoRegister retired with its last row, ENDGAME B.3.9 IO.)
// (RtFactoriesPickingRegister retired with its last row, ENDGAME B.3.9 pilot.)
