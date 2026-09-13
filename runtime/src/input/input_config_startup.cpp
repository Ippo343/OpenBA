// ============================================================================
// runtime/src/input/input_config_startup.cpp — Wave 9 lane ENG, iteration 3
// Input::Configuration::Startup + the unexported Load it tail-calls
// ============================================================================
//
//   | Export                                | Retail      | Ord  | Instrs |
//   |---------------------------------------|-------------|------|--------|
//   | ?Startup@Configuration@Input@@QEAA_NXZ | 0x1800e0d30 | 4085 |   43   |
//   | ?Load@Configuration@Input@@QEAAXXZ     | 0x1800e0c80 | 3199 |   25   |
//
// CORRECTED, Wave 20 Phase 0c. This table used to call 0x1800e0c80
// "(unexported)" and the paragraph below used to say it "has no name in either
// build and is registered by ADDRESS only". BOTH WERE FALSE, and re-running the
// very check they claim to have run is what showed it:
//
//   ?Load@Configuration@Input@@QEAAXXZ -> ordinal 3199, 0x1800e0c80
//
// It is a plain public export (QEAA). This is the documented trap -- this DLL
// exports its private members, so "unexported" is a conclusion that has to be
// MEASURED, never inferred from a mangling or from a name not being to hand
// (reference_aeaa_is_private_not_unexported.md). The cost was real: input.cpp
// had already ported and NAME-registered the same function, so the false belief
// produced a second body, and the export's two doors reached one each. input.cpp
// now registers both doors at THIS body; see the note there.
//
// `Engine::Startup` step 14 — the last of the four `Configuration::Startup`
// calls, and the last thing before `PlatformConfigure`. Off the current reach
// path (lane FS's `SetupDirectories` is the wall) and chosen because among the
// candidates that are off it, this is the one whose whole transitive closure is
// already ported bar ONE function.
//
// Kill switch: `RT_INPUTCFGSTART=0`.
//
// ---------------------------------------------------------------------------
// STARTUP (0x1800e0d30)
// ---------------------------------------------------------------------------
//   if (this->keyMap) DeletingDtor(this->keyMap);     ; 0x180053440, unexported
//   p = MemAlloc(0x18, 4);                            ; operator new, INLINED --
//                                                       lock, Allocate(0x18,4),
//                                                       unlock. `MOV EDX,0x18`
//                                                       then `LEA R8D,[RDX-0x14]`
//                                                       is how the 4 is built.
//   if (p) { *(int*)p = 0; q = p + 8; *(void**)q = 0; *(int*)(q+8) = 0; }
//   this->keyMap = p;                                 ; the NULL is stored too
//   Load(this);                                       ; 0x1800e0c80
//   return true;                                      ; MOV AL,1 -- no failure arm
//
// The `if (this->keyMap)` arm is DEAD on the boot path: `Engine::Startup` calls
// this immediately after `??0Configuration@Input@@`, which zeroes the field
// (lane X). It is kept because `Startup` is a public export and nothing stops a
// second call, which is exactly the case the arm exists for. 0x180053440 is an
// unnamed scalar deleting destructor reached by address — the SECOND such site
// in this lane, and like the first it sits on an arm no boot takes.
//
// The inlined `operator new` is `MemAlloc(0x18, 4)` and nothing else: the same
// 20-instruction body as the unexported 0x180011640 (see
// runtime/src/engine/engine_lifecycle.cpp, where it was read in full), spelled
// out at this call site rather than called.
//
// ---------------------------------------------------------------------------
// LOAD (0x1800e0c80) — three registry reads
// ---------------------------------------------------------------------------
//   KeyMap::Load(this->keyMap);                       ; ord 3203, UNPORTED
//   Persistence::Get("Input", "MouseClipping", &this->mouseClipping);
//   Persistence::Get("Input", "Legacy",        &this->legacy);
//   Persistence::Get("Input", "MouseDisplay",  &this->mouseDisplay);  ; tail call
//
// All four literals were read with `read_memory`, not inferred from the header:
// 0x180155200 = `Input`, 0x180157d30 = `MouseClipping`, 0x180157d40 = `Legacy`,
// 0x180157d48 = `MouseDisplay`. They agree with runtime/types/input/Input.h,
// which derived them from this function and from `Save`.
//
// `?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z` is the BOOL overload and is already
// PORTED (runtime/src/io/persistence.cpp:309). Retail does not test any of the
// three return values: `Get`'s contract is to leave the out-parameter untouched
// when the key is absent, so a missing key keeps the constructor's value.
//
// `KeyMap::Load` is passed `this->keyMap` UNGUARDED (0x1800e0c8d is a bare
// `MOV RCX,[RCX]`), on the one path where the allocation above may have failed.
// Kept faithful.
//
// ---------------------------------------------------------------------------
// THIS ROW CLOSES COMPLETELY — the only one in this lane that does
// ---------------------------------------------------------------------------
// Per the triage rule, and this time the answer is unqualified: EVERY callee in
// the closure is already ported.
//
//   ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  Wave 9 lane E
//   ?Load@KeyMap@Input@@QEAAXXZ                    Wave 9 lane IOA,
//                                                  runtime/src/input/keymap_load.cpp:303
//   ?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z        Wave 4 lane C,
//                                                  runtime/src/io/persistence.cpp:309
//
// `KeyMap::Load` was UNPORTED when this lane enumerated the queue in iteration
// 1; lane IOA landed it since. RE-CHECKING the stated blocker instead of
// trusting a two-iteration-old list is what turned this from "closes and
// exposes one row" into "closes" — the lesson this wave keeps paying for, in
// the cheap direction for once.
//
// So `Engine::Startup` step 14 is DONE: nothing behind it is null at cutover.
// The one address with no name (0x180053440) sits on an arm no boot takes.
//
// ORACLE: none for either. `Startup` allocates and publishes into the live
// configuration object and `Load` reads the registry; a replay would allocate a
// second KeyMap and leak the first. Both get a shadow on what their callees
// produced, plus a did-work counter.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/input/Input.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtInput::Configuration;

namespace {

typedef void* (*MemAllocFn)(uint64_t, int);
typedef void  (*VoidThisFn)(void*);
typedef bool  (*GetBoolFn)(void* self, const char* section, const char* key, bool* out);

// ?_singleton@Persistence@IO@@0PEAV12@EA, ord 4816, retail 0x1801fc6b8.
void** PersistenceSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Persistence@IO@@0PEAV12@EA");
}

// The four literals, read out of the DLL rather than retyped from a header.
const char kSection[]      = "Input";          // 0x180155200
const char kMouseClipping[] = "MouseClipping";  // 0x180157d30
const char kLegacy[]        = "Legacy";         // 0x180157d40
const char kMouseDisplay[]  = "MouseDisplay";   // 0x180157d48

}  // namespace

// ============================================================================
// ?Load@Configuration@Input@@QEAAXXZ — retail 0x1800e0c80, ordinal 3199.
// (Was labelled UNEXPORTED here. It is not -- see the banner.)
// ============================================================================
extern "C" void RtInput_Configuration_Load(Configuration* self) {

    // Unguarded, exactly as retail: `MOV RCX,[RCX]` then straight into the call.
    RT_EXPORT_ONCE(VoidThisFn, "?Load@KeyMap@Input@@QEAAXXZ")(self->keyMap);

    // Retail reads Persistence::_singleton ONCE into RBX and uses it for all
    // three calls; the slot guard is ours (the variable does not exist without
    // the DLL unless the mint provides it), the null `this` it may hold is
    // retail's and stays unguarded.
    if (void** pSlot = PersistenceSingletonSlot()) {
        void* persistence = *pSlot;
        GetBoolFn get = RT_EXPORT_ONCE(GetBoolFn,
            "?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z");
        get(persistence, kSection, kMouseClipping, &self->mouseClipping);
        get(persistence, kSection, kLegacy,        &self->legacy);
        get(persistence, kSection, kMouseDisplay,  &self->mouseDisplay);
    }

}

// ============================================================================
// ?Startup@Configuration@Input@@QEAA_NXZ                   retail 0x1800e0d30
// ============================================================================
extern "C" bool RtInput_Configuration_Startup(Configuration* self) {

    if (self->keyMap != nullptr) {
        // 0x1800e0d54. An unnamed scalar deleting destructor; no export in
        // either build, so it can only be reached by address, and that address
        // is unmapped at cutover. Dead on the boot path (the ctor just zeroed
        // this field) -- see the banner.
        if (uintptr_t fn = ResolveAddr(Module::Runtime, 0x180053440))
            reinterpret_cast<VoidThisFn>(fn)(self->keyMap);
    }

    void* keyMap = RT_EXPORT_ONCE(MemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(0x18, 4);
    if (keyMap) {
        uint8_t* p = static_cast<uint8_t*>(keyMap);
        *reinterpret_cast<int32_t*>(p + 0x00) = 0;
        *reinterpret_cast<void**>(p + 0x08)   = nullptr;
        *reinterpret_cast<int32_t*>(p + 0x10) = 0;
    }
    // The null is STORED, not rejected -- 0x1800e0db6/0x1800e0db9.
    self->keyMap = keyMap;

    RtInput_Configuration_Load(self);


    return true;
}

// ============================================================================
// Registration
// ============================================================================
int RtInputConfigStartupRegister() {
    int n = 0;
    RegisterRuntimeExport("?Startup@Configuration@Input@@QEAA_NXZ", (void*)&RtInput_Configuration_Startup); ++n;
    RegisterRuntimeAddr(0x1800e0d30, (void*)&RtInput_Configuration_Startup);

    // 0x1800e0c80 / ?Load@Configuration@Input@@QEAAXXZ is registered -- BOTH
    // doors, at this body -- by input.cpp, whose registration is ungated. It
    // was registered here too, by address, on the false premise that the
    // function had no export name; that duplicate is what made the two doors
    // disagree, so it is gone rather than merely repointed.

    // NOTHING is deliberately absent here -- see the banner. Every callee in
    // the closure is ported: MemAlloc (lane E), ?Load@KeyMap@Input@@ (lane IOA,
    // keymap_load.cpp:303) and ?Get@Persistence@IO@@…AEA_N@Z (Wave 4 lane C,
    // persistence.cpp:309).
    return n;
}
