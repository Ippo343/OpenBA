#pragma once
// ============================================================================
// runtime/types/io/Registry.h — IO::Registry (Wave 9 lane BOOT2)
// ============================================================================
//
// Retail Runtime-x64.dll, image base 0x180000000. Unlike most partial layouts
// in this tree, this one is COMPLETE and its total size is pinned by evidence
// rather than by the largest offset anybody happened to touch.
//
// THE SIZE — pinned by the copy constructor, not by a guess
// --------------------------------------------------------
// `??0Registry@IO@@QEAA@AEBV01@@Z` (ord 344, retail 0x18002b140) is 18
// instructions and copies the WHOLE object, field by field, and nothing else:
//
//     [RCX]      = 0
//     RAX = [RDX];      if (RAX) { [RCX]      = RAX; ++RAX[-1]; }   ; +0x00 String
//     RAX = [RDX+0x08]; [RCX+0x08] = RAX                            ; +0x08 qword
//     EAX = [RDX+0x10]; [RCX+0x10] = EAX                            ; +0x10 dword
//     EAX = [RDX+0x14]; [RCX+0x14] = EAX                            ; +0x14 dword
//     return RCX
//
// A member-wise copy ctor enumerates every member of the class, so the object
// is exactly 0x18 bytes: one refcounted `System::String`, one qword, two
// dwords. That is why this header may carry a `sizeof` assert where
// `runtime/types/io/Persistence.h` deliberately may not — there, no function
// enumerates the members; here, one does.
//
// THE FIELDS
// ----------
//   +0x00  System::String _path   — "Software\<company>\<product>[\<subkey>]",
//          built by the ctor with `String::Format`. Refcounted: the copy ctor
//          AddRefs it, and `IO::Persistence::ReadSettings` releases it inline
//          at 0x1800b8892 with the standard String release idiom.
//   +0x08  HKEY _key — the open handle. `void*` here rather than `HKEY` so this
//          header stays free of <windows.h> (PLAN.md §4.1 decision 1); the only
//          file that casts it is `persistence_registry_win.cpp`. ZERO means
//          "neither open nor create succeeded", which the ctor writes
//          explicitly at 0x1800b971c.
//   +0x10  uint32 _enumIndex — the running `dwIndex` for RegEnumValueW /
//          RegEnumKeyExW. Pinned by ReadSettings, which reads it as a DWORD
//          (`MOV EAX,[reg+0x10]`), post-increments it, and stores it back
//          (0x1800b851a-0x1800b8522), and resets it to 0 on the way out
//          (0x1800b8832).
//   +0x14  uint32 _enumGroupIndex — the SECOND enumeration cursor, copied as a
//          separate dword by the copy ctor and zeroed by the main ctor
//          together with +0x10 (one `MOV qword ptr [RCX+0x10],0`). Named for
//          the pair `?NextKey@Registry@IO@@` (0x1800b97c0) / `?NextGroup@…`
//          (0x1800b9480) that the two cursors must serve; NEITHER of those two
//          functions was read by this lane, so the NAME is an inference and
//          the OFFSET and WIDTH are not. A lane porting NextKey/NextGroup
//          should confirm which cursor each one drives before relying on it.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../system/String.h"

namespace RtIo {

struct Registry {
    RtSystem::String path;            // +0x00
    void*            key;             // +0x08 — HKEY, or null
    uint32_t         enumIndex;       // +0x10
    uint32_t         enumGroupIndex;  // +0x14 — see the banner: name inferred
};
static_assert(offsetof(Registry, key)            == 0x08, "IO::Registry::_key @ +0x08");
static_assert(offsetof(Registry, enumIndex)      == 0x10, "IO::Registry::_enumIndex @ +0x10");
static_assert(offsetof(Registry, enumGroupIndex) == 0x14, "IO::Registry::_enumGroupIndex @ +0x14");
static_assert(sizeof(Registry) == 0x18,
              "IO::Registry is exactly 0x18 bytes -- pinned by the member-wise copy "
              "ctor ??0Registry@IO@@QEAA@AEBV01@@Z (retail 0x18002b140)");

// The registry path the no-subkey constructor builds, split into the three
// literals retail's `String::Format` consumes. Read out of the shipped DLL's
// .rdata at the addresses named, not retyped from a decompile:
//   0x180155898  "Software\\%s\\%s"
//   0x180155878  "Shining Rock Software LLC"
//   0x180154858  "Banished"
// The result matches, independently, the live key this project's own harness
// already opens by hand in standalone/main.cpp:144
// (`HKCU\Software\Shining Rock Software LLC\Banished`).
constexpr const char* kRegistryPathFormat  = "Software\\%s\\%s";
constexpr const char* kRegistryCompanyName = "Shining Rock Software LLC";
constexpr const char* kRegistryProductName = "Banished";
// 0x1801557e4 — the separator the ctor appends before a non-empty subkey.
constexpr const char* kRegistryPathSeparator = "\\";

}  // namespace RtIo
