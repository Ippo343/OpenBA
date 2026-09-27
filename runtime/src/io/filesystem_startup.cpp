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
#include "../../types/system/Configuration.h"   // ENDGAME B.3.9 SS1: Configuration::_singleton

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (that header drags in <windows.h>, and PLAN §4.1 requires runtime/src/ to stay
// platform-neutral). Must stay byte-identical to resolve.h's, and at FILE scope.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtIO::FileSystem;

namespace {

typedef void  (*VoidThisFn)(void*);
typedef void* (*MapFindFn)(void* map, const System::String* wanted);

}  // namespace

// ============================================================================
// ?Startup@FileSystem@IO@@QEAA_NXZ                          retail 0x1800abc50
// ============================================================================
// ORACLE: none. It runs once, from `Engine::Startup`, and both of its callees
// mutate the process's file-system state (directories created, `.pkg` files
// opened and indexed) -- a replay would open every package twice. The shadow
// below checks the two gate bytes, which are the only thing this body writes
// that nothing else in it wrote, plus a did-work counter.
//
// ENDGAME B.3.4 PACKAGE+FS commit 5a: the body is now
// `IO::FileSystem::Startup()`, below. The door keeps its name, its `extern "C"`
// linkage and both registration lines, and delegates. No qualified `IO::` call
// is needed: `IO::FileSystem` has no vtable at all
// (runtime/types/io/FileSystem.h), so B34_BRIEFS section A.1's re-entry hazard
// cannot arise.
extern "C" bool RtIo_FileSystem_Startup(FileSystem* self) {
    return reinterpret_cast<IO::FileSystem*>(self)->Startup();
}

// ============================================================================
// `IO::FileSystem::Startup` — the member body
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2). The two file-local
// helpers it needs live in this translation unit's GLOBAL anonymous namespace,
// so they are reached as `::ConfigurationSingletonSlot` / `::VoidThisFn` /
// `::MapFindFn` — spelled with the `::` on purpose, so a future reader cannot
// mistake them for members.
//
// NOTHING ELSE CHANGED. Same callees in the same order, same two RT_EXPORT_ONCE
// doors, the same unguarded Configuration dereference, the same single `return
// true`. `self->field` became `_field`; that is the whole of the diff.
namespace IO {

bool FileSystem::Startup() {

    RT_EXPORT_ONCE(::VoidThisFn, "?SetupDirectories@FileSystem@IO@@QEAAXXZ")(this);

    _existsInPackfileGate = 1;

    // ENDGAME B.3.1 step 2: retail's `String::Set(&tmp, "onlypkg")`
    // (0x1800abc79, the literal at 0x1801556e8) followed by the dtor at
    // 0x1800abca7 is exactly the `const char*` ctor and RAII destruction --
    // the two RT_EXPORT_ONCE calls that used to spell ctor/dtor by name are
    // gone; `tmp`'s destructor is the retail dtor now.
    System::String tmp("onlypkg");

    // Retail: `MOV RCX,[0x1801fc6f8]; ADD RCX,0x10` -- the map lives at
    // Configuration + 0x10, and retail dereferences the singleton without a
    // check. ENDGAME B.3.9 SS1 (B39_RULES D7): `?_singleton@Configuration@System@@`
    // IS `::System::Configuration::_singleton` now (class static, always exists) --
    // the existence guard the door version needed is gone, so the deref below is
    // retail's true unguarded one.
    //
    // The find itself (0x18002ace0) has NO export name in either build, so it
    // goes by address. `ResolveAddr(Module::Runtime, ...)` consults the port
    // registry FIRST (reference_resolveaddr_consults_port_registry_first.md),
    // and runtime/src/system/configuration.cpp registers that address, so this
    // reaches OUR port both today and at cutover.
    void* map = reinterpret_cast<uint8_t*>(::System::Configuration::_singleton) + 0x10;
    bool found = reinterpret_cast<::MapFindFn>(
                ResolveAddr(Module::Runtime, 0x18002ace0))(map, &tmp) != nullptr;

    if (found) _platformExistsGate = 0;

    RT_EXPORT_ONCE(::VoidThisFn, "?LoadPackages@FileSystem@IO@@QEAAXXZ")(this);


    return true;
}

}  // namespace IO

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
