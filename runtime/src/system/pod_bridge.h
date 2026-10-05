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
// (B.4 Z LAST IO: the four String overloads are gone: no POD String is read as
// the class any more outside the graphics files, which spell their own.)

// (B.4 Z MIRROR2: the four WideString overloads are gone with the
// `RtSystem::WideString` POD.)

// ---- Filename -------------------------------------------------------------
inline System::Filename&       Sys(Filename& f)       { return reinterpret_cast<System::Filename&>(f); }  // LEFT: audit DEFERRED:RtSystem::String (50 files) / RtSystem::Filename (36 files) still named; POD String / Filename slot read as the class (H10 bridge)
inline const System::Filename& Sys(const Filename& f) { return reinterpret_cast<const System::Filename&>(f); }  // LEFT: audit DEFERRED:RtSystem::String (50 files) / RtSystem::Filename (36 files) still named; POD String / Filename slot read as the class (H10 bridge)
inline System::Filename*       Sys(Filename* f)       { return reinterpret_cast<System::Filename*>(f); }  // LEFT: audit DEFERRED:RtSystem::String (50 files) / RtSystem::Filename (36 files) still named; POD String / Filename slot read as the class (H10 bridge)
inline const System::Filename* Sys(const Filename* f) { return reinterpret_cast<const System::Filename*>(f); }  // LEFT: audit DEFERRED:RtSystem::String (50 files) / RtSystem::Filename (36 files) still named; POD String / Filename slot read as the class (H10 bridge)

// (B.4 Z LAST IO: the two `SysFile` overloads are gone: the FileSystem
// directories, FileInfo's names and the package entry's name are real
// Filenames, so no String field is read as a Filename any more.)

}  // namespace RtSystem
