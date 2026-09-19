#pragma once
// ============================================================================
// runtime/src/system/pod_bridge.h — the ONE transitional POD<->class bridge
//                                   (ENDGAME B.3.1 step 2)
// ============================================================================
//
// B.3.1 step 1 introduced `System::String` / `System::Filename` as real classes
// laid over the SAME bytes as the pre-existing PODs (`RtSystem::String`,
// `RtSystem::Filename`). Step 2 converts call sites to the classes, but two
// things still speak POD and cannot move in the same commit:
//
//   * the registered C entry points, whose signatures are the retail ABI shape
//     (`T* self, T* ret, ...`) and are forward-declared in files owned by other
//     batches — changing one would be a cross-batch signature break;
//   * members of not-yet-converted structs (`RtSystem::String path;` inside
//     `IO::FileSystem`, `Object::ExternalFactory`, ...), which B31_RULES says
//     keep their POD type until THAT class is converted.
//
// B31_RULES' answer for both is `reinterpret_cast<System::String&>(field)` at
// the use site. Spelling that cast once, here, instead of several hundred times
// across the tree is the same transitional cast with one place to delete: when
// the last POD user is gone this header goes with it, and until then the B.4
// cast metric counts these eight lines and not the call sites.
//
// The casts are exact by construction: every static_assert in String.h and
// Filename.h pins the two spellings to the same size and the same single field,
// and retail itself converts String<->Filename by pointer cast throughout
// (Filename.h header note).
//
// Spelled `Sys(x)` rather than `AsSys(x)` deliberately: runtime/src/system/
// filename.cpp already has a file-local `AsSys` of its own (the class's own
// file, not converted by step 2), and two spellings of the same idea that are
// only sometimes both visible is exactly the ambiguity this header exists to
// avoid.
// ============================================================================

#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../types/system/WideString.h"   // ENDGAME B.3.4b SYSTEM (4a)

namespace RtSystem {

// ---- String ---------------------------------------------------------------
inline System::String&       Sys(String& s)        { return reinterpret_cast<System::String&>(s); }
inline const System::String& Sys(const String& s)  { return reinterpret_cast<const System::String&>(s); }
inline System::String*       Sys(String* s)        { return reinterpret_cast<System::String*>(s); }
inline const System::String* Sys(const String* s)  { return reinterpret_cast<const System::String*>(s); }

// ---- WideString (ENDGAME B.3.4b SYSTEM (4a)) -------------------------------
// The same transitional cast one type over. `RtSystem::WideString` is the POD
// three `runtime/src/` files still hold by value and seed through the ctor
// door; `System::WideString` is the class the doors now delegate to. Both are
// one `wchar_t*`, pinned field for field in WideString.h.
inline System::WideString&       Sys(WideString& w)       { return reinterpret_cast<System::WideString&>(w); }
inline const System::WideString& Sys(const WideString& w) { return reinterpret_cast<const System::WideString&>(w); }
inline System::WideString*       Sys(WideString* w)       { return reinterpret_cast<System::WideString*>(w); }
inline const System::WideString* Sys(const WideString* w) { return reinterpret_cast<const System::WideString*>(w); }

// ---- Filename -------------------------------------------------------------
inline System::Filename&       Sys(Filename& f)       { return reinterpret_cast<System::Filename&>(f); }
inline const System::Filename& Sys(const Filename& f) { return reinterpret_cast<const System::Filename&>(f); }
inline System::Filename*       Sys(Filename* f)       { return reinterpret_cast<System::Filename*>(f); }
inline const System::Filename* Sys(const Filename* f) { return reinterpret_cast<const System::Filename*>(f); }

// ---- a String field read AS a Filename ------------------------------------
// Retail does this at real call sites, not as a convenience: `IO::FileSystem`
// declares `sourceDirectory`/`dataDirectory` as Strings and `GetSourceDirectory`
// (0x180007b90) / `GetDataDirectory` (0x180007bd0) pass their addresses straight
// into `??0Filename@System@@QEAA@AEBV01@@Z`. Same eight bytes, same single
// field; the conversion retail performs by pointer cast, spelled once.
inline System::Filename&       SysFile(String& s)       { return reinterpret_cast<System::Filename&>(s); }
inline const System::Filename& SysFile(const String& s) { return reinterpret_cast<const System::Filename&>(s); }

}  // namespace RtSystem
