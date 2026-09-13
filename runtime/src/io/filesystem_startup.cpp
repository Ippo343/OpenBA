// ============================================================================
// runtime/src/io/filesystem_startup.cpp — Wave 9 lane ENG, iteration 2
// IO::FileSystem::Startup — the row behind SetCommandLine in the boot queue
// ============================================================================
//
//   | Export                            | Retail      | Ord  | Instrs |
//   |-----------------------------------|-------------|------|--------|
//   | ?Startup@FileSystem@IO@@QEAA_NXZ  | 0x1800abc50 | 4097 |   38   |
//
// Name checked against Runtime-x64.dll's export directory with pefile before a
// line was written. No new Win32 API and no new import library.
//
// It lives in its own file rather than in `filesystem.cpp` (Wave 4 lane D)
// purely so that two lanes are not editing one file while both are in flight;
// it registers through its own `RtIoFileSystemStartupRegister()`, wired into
// `rt_io_port.cpp` beside lane D's.
//
// ---------------------------------------------------------------------------
// THE BODY, AND THE ONE THING THAT IS NOT OBVIOUS IN IT
// ---------------------------------------------------------------------------
//   0x1800abc67  SetupDirectories(this)                     ; ord 4029
//   0x1800abc6c  this->existsInPackfileGate = 1             ; +0x18
//   0x1800abc79  String tmp; String::Set(&tmp, "onlypkg")   ; literal 0x1801556e8
//   0x1800abc8b  found = Find(Configuration::_singleton + 0x10, &tmp) != null
//   0x1800abca7  ~tmp
//   0x1800abcd4  if (found) this->platformExistsGate = 0    ; +0x19
//   0x1800abcdb  LoadPackages(this)                         ; ord 3209
//   0x1800abce0  return true                                ; MOV AL,1 -- no failure path
//
// The literal was read with `read_memory` at 0x1801556e8 and is the seven bytes
// `onlypkg`, NUL-terminated. So the two gate bytes at +0x18/+0x19 are the
// `-onlypkg` command-line switch: with it set, `FileSystem::Exists` stops
// consulting the real filesystem and answers only out of the mounted `.pkg`
// files (runtime/types/io/FileSystem.h documents both gates from `Exists`'s own
// two `CMP byte` tests). On this project's boots the command line is empty
// (see runtime/src/system/configuration.cpp §2), so `found` is false and the
// gate keeps the value `SetupDirectories` left in it.
//
// The unguarded `*(Configuration**)0x1801fc6f8` is retail's and is KEPT. What is
// guarded is the SLOT itself -- see the comment at the call.
//
// ---------------------------------------------------------------------------
// THIS ROW CLOSES; ITS TWO CALLEES DO NOT
// ---------------------------------------------------------------------------
// Per the triage rule, state it plainly: porting this does NOT make
// `Engine::Startup` get past step 3. It moves the fault from
// `?Startup@FileSystem@IO@@QEAA_NXZ` to
// **`?SetupDirectories@FileSystem@IO@@QEAAXXZ`** (0x1800abd00, ord 4029), which
// is 722 instructions with thirteen distinct callees, six of them large and
// unported. That is the next real wall and it wants a lane, not a tail-end.
// `?LoadPackages@FileSystem@IO@@QEAAXXZ` (0x1800ab460, ord 3209) is behind it.
//
// Porting it anyway is worth one iteration's cost: it converts an anonymous
// "the boot stops in FileSystem" into a named 722-instruction target with a
// measured callee list, which is the whole value of this queue.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (that header drags in <windows.h>, and PLAN §4.1 requires runtime/src/ to stay
// platform-neutral). Must stay byte-identical to resolve.h's, and at FILE scope.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtIO::FileSystem;

namespace {

typedef void  (*VoidThisFn)(void*);
typedef void* (*MapFindFn)(void* map, const System::String* wanted);

// `?_singleton@Configuration@System@@0PEAV12@EA`, ord 4803, retail 0x1801fc6f8.
// Written by `??0Configuration@System@@QEAA@XZ`, which lane ENG ported in
// runtime/src/engine/engine_lifecycle.cpp; read here through the PORT-AWARE
// door so it follows lane C2's mint at cutover.
void** ConfigurationSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@System@@0PEAV12@EA");
}

}  // namespace

// ============================================================================
// ?Startup@FileSystem@IO@@QEAA_NXZ                          retail 0x1800abc50
// ============================================================================
// ORACLE: none. It runs once, from `Engine::Startup`, and both of its callees
// mutate the process's file-system state (directories created, `.pkg` files
// opened and indexed) -- a replay would open every package twice. The shadow
// below checks the two gate bytes, which are the only thing this body writes
// that nothing else in it wrote, plus a did-work counter.
extern "C" bool RtIo_FileSystem_Startup(FileSystem* self) {

    RT_EXPORT_ONCE(VoidThisFn, "?SetupDirectories@FileSystem@IO@@QEAAXXZ")(self);

    self->existsInPackfileGate = 1;

    // ENDGAME B.3.1 step 2: retail's `String::Set(&tmp, "onlypkg")`
    // (0x1800abc79, the literal at 0x1801556e8) followed by the dtor at
    // 0x1800abca7 is exactly the `const char*` ctor and RAII destruction --
    // the two RT_EXPORT_ONCE calls that used to spell ctor/dtor by name are
    // gone; `tmp`'s destructor is the retail dtor now.
    System::String tmp("onlypkg");

    // Retail: `MOV RCX,[0x1801fc6f8]; ADD RCX,0x10` -- the map lives at
    // Configuration + 0x10, and retail dereferences the singleton without a
    // check. The DEREFERENCE stays unguarded, faithfully. What is guarded is
    // whether the SLOT EXISTS: with no DLL the variable at 0x1801fc6f8 does not
    // exist at all unless lane C2's mint provides it, and as of this commit
    // `?_singleton@Configuration@System@@0PEAV12@EA` is NOT in
    // rt_data_mint_tables.inc. That is our difference from retail, not a guard
    // over retail's logic; when the mint row lands this branch stops being
    // reachable, because lane ENG's ported ctor writes the slot.
    //
    // The find itself (0x18002ace0) has NO export name in either build, so it
    // goes by address. `ResolveAddr(Module::Runtime, ...)` consults the port
    // registry FIRST (reference_resolveaddr_consults_port_registry_first.md),
    // and runtime/src/system/configuration.cpp registers that address, so this
    // reaches OUR port both today and at cutover.
    bool found = false;
    if (void** cfgSlot = ConfigurationSingletonSlot()) {
        void* map = static_cast<uint8_t*>(*cfgSlot) + 0x10;
        found = reinterpret_cast<MapFindFn>(
                    ResolveAddr(Module::Runtime, 0x18002ace0))(map, &tmp) != nullptr;
    }

    if (found) self->platformExistsGate = 0;

    RT_EXPORT_ONCE(VoidThisFn, "?LoadPackages@FileSystem@IO@@QEAAXXZ")(self);


    return true;
}

// ============================================================================
// Registration
// ============================================================================
int RtIoFileSystemStartupRegister() {
    int n = 0;
    RegisterRuntimeExport("?Startup@FileSystem@IO@@QEAA_NXZ", (void*)&RtIo_FileSystem_Startup); ++n;
    RegisterRuntimeAddr(0x1800abc50, (void*)&RtIo_FileSystem_Startup);

    // DELIBERATELY ABSENT -- the two callees, both exported, both unported:
    //   ?SetupDirectories@FileSystem@IO@@QEAAXXZ  0x1800abd00  ord 4029  722 instrs
    //   ?LoadPackages@FileSystem@IO@@QEAAXXZ      0x1800ab460  ord 3209
    return n;
}
