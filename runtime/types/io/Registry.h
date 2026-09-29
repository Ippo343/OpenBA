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
//   +0x08  HKEY _key — the open handle, spelled `HKEY__*` (what <windows.h>'s
//          STRICT `DECLARE_HANDLE(HKEY)` makes HKEY) against a forward
//          declaration, so this header stays free of <windows.h> (PLAN.md §4.1
//          decision 1) and nothing casts it. ZERO means
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
#include <type_traits>   // B.3.4 PACKAGE+FS: the "IO::Registry has no vptr" pins
#include "../system/String.h"

// <windows.h>'s STRICT handle type: `DECLARE_HANDLE(HKEY)` is
// `struct HKEY__ { int unused; }; typedef struct HKEY__* HKEY;`.
struct HKEY__;

namespace RtIo {

// (B.4 R-IO: the `RtIo::Registry` POD is gone -- its last user,
// WriteSettings, holds the class.)

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

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — the REAL `IO::Registry`.
// ============================================================================
//
// Design: B34_RULES.md §1.5 rule 5 (the class keeps the `IO` namespace despite
// the name-adjacency to `System` — that is retail's placement, not a mistake to
// fix), §4.3 (the ctor-door question), §1.5 rule 3 (the POD stays).
//
// *** IT HAS NO VTABLE. ***
// -------------------------
// There is no `??_7Registry@IO@@6B@` anywhere: not in the retail export table,
// not in `standalone/rt_data_mint_tables.inc`, and `deleting_dtor.cpp` carries
// no row for it. Every exported member is `QEAA` — the constructor
// (0x1800b9500), the copy constructor (0x18002b140), `?NextKey@` (0x1800b97c0)
// and `?NextGroup@` (0x1800b9480). So, exactly as for `IO::Serializer`
// (B34_RULES §4.3), converting its CONSTRUCTOR is safe: a ctor door is
// dangerous only when the member form would change which vtable a live object
// wears, and this object wears none.
//
// *** ITS DESTRUCTOR IS RETAIL'S INLINED TEARDOWN. ***
// Retail exports no `??1Registry@IO@@`: every `~Registry` in the DLL is
// INLINED at its use site as `if (_key) RegCloseKey(_key); _key = 0;` followed
// by the String release idiom on `_path` (0x1800b8892 in `ReadSettings`,
// 0x1800b8af2 and 0x1800b8fc3 in `WriteSettings`). B.4 R-IO: that is this
// class's destructor -- the body closes the key, then `_path`'s own
// destructor releases the String -- and each of the three use sites is a
// local whose scope ends exactly where retail's inlined copy sits. All three
// are in persistence_registry_win.cpp; nothing else names this class.
// ============================================================================

namespace IO {

class Registry {
public:
    // ??0Registry@IO@@QEAA@PEBD@Z                            retail 0x1800b9500
    // Body: runtime/src/io/persistence_registry_win.cpp (`_win`, because the
    // whole tail is RegOpenKeyExW / RegCreateKeyExW).
    //
    // NOT `explicit`: retail's is a one-argument converting constructor and
    // every call site passes `nullptr` positionally. It CREATES the key when it
    // is absent — retail's behaviour, and the one registry mutation on the boot
    // path. `_key` is left null when both the open and the create fail
    // (0x1800b971c).
    Registry(const char* subKey);

    // Retail's inlined teardown (see the banner). Defined in
    // persistence_registry_win.cpp, which has <windows.h>.
    ~Registry();

    // Retail HAS a copy constructor (`??0Registry@IO@@QEAA@AEBV01@@Z`, ord 344,
    // 0x18002b140) and it AddRefs `_path` — see this header's top banner, where
    // its member-wise body is what pins the 0x18 size. It is NOT ported (no
    // call site in this tree reaches it, and an uncalled port is an unverified
    // port). Deleted rather than left implicit because the IMPLICIT one would
    // copy `_path.data` WITHOUT the AddRef and hand two owners one refcount.
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // ---- the 0x18 bytes, at retail's offsets -------------------------------
    // PUBLIC: `offsetof` on them has to be well formed for the pins below.
    System::String   _path;            // +0x00
    HKEY__*          _key;             // +0x08 — HKEY, or null
    uint32_t         _enumIndex;       // +0x10
    uint32_t         _enumGroupIndex;  // +0x14 — name inferred, offset is not
};

static_assert(sizeof(Registry) == 0x18,
              "IO::Registry is exactly 0x18 bytes -- pinned by the member-wise "
              "copy ctor ??0Registry@IO@@QEAA@AEBV01@@Z (retail 0x18002b140)");
static_assert(offsetof(Registry, _key)            == 0x08, "IO::Registry::_key @ +0x08");
static_assert(offsetof(Registry, _enumIndex)      == 0x10, "IO::Registry::_enumIndex @ +0x10");
static_assert(offsetof(Registry, _enumGroupIndex) == 0x14, "IO::Registry::_enumGroupIndex @ +0x14");
// The guard, stated as an assertion so a future `virtual` fails the build
// rather than the boot: a vptr would make this 0x20 bytes and push every field
// eight bytes down, under a door that is registered BY ADDRESS.
static_assert(!std::is_polymorphic<Registry>::value,
              "IO::Registry must never acquire a vtable: retail has none for it");
static_assert(std::is_standard_layout<Registry>::value,
              "the four fields sit at the offsets pinned above");

}  // namespace IO
