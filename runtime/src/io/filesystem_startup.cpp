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

#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Configuration.h"   // ENDGAME B.3.9 SS1: Configuration::_singleton

using RtIO::FileSystem;

// ============================================================================
// ?Startup@FileSystem@IO@@QEAA_NXZ                          retail 0x1800abc50
// ============================================================================

// ============================================================================
// `IO::FileSystem::Startup` — the member body
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2).
//
// Same callees in the same order, the same unguarded Configuration
// dereference, the same single `return true`. B3R RT-IO: SetupDirectories and
// LoadPackages are member calls and the map find is RtSystem_ConfigMap_Find by
// name (they were two RT_EXPORT_ONCE doors and a by-address resolve).
namespace IO {

bool FileSystem::Startup() {

    SetupDirectories();

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
    // The find itself (0x18002ace0) has NO export name in either build; it is
    // `RtSystem_ConfigMap_Find` (runtime/src/system/configuration.cpp).
    void* map = reinterpret_cast<uint8_t*>(::System::Configuration::_singleton) + 0x10;
    bool found = RtSystem_ConfigMap_Find(
                map, reinterpret_cast<const RtSystem::String*>(&tmp)) != nullptr;   // B.3.x bridge

    if (found) _platformExistsGate = 0;

    LoadPackages();


    return true;
}

}  // namespace IO

