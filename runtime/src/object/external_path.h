#pragma once
// ============================================================================
// runtime/src/object/external_path.h — Wave 9 lane RRES
//   `Object::ExternalFactory`'s PATH half: the two path-stack readers and
//   `ExpandRelativePath`, plus the `System::Filename::GetPathExtension` those
//   two need.
// ============================================================================
//
// WHY THIS FILE EXISTS AS ITS OWN TRANSLATION UNIT
//
// Lane RES landed the resource TREE (`external_set.cpp`) and declined
// `FindOrLoad` with a measured blocker: `ExpandRelativePath` (834 instructions)
// could not be written because `ExternalFactory + 0x30 / +0x38` were recorded
// as NOT PINNED in `runtime/types/object/External.h`. That is now pinned — see
// the `PathStackEntry` block in that header — and this file is the port that
// the pin makes possible.
//
// It is a separate file from `external_set.cpp` and from `filename.cpp` for the
// reason `rt_object_port.cpp` states at the top: one lane owns one file, so two
// lanes never edit the same source. The ONE shared edit is the three-line hook
// in `rt_object_port.cpp`.
//
// WHAT IS DELIBERATELY *NOT* HERE
//
//   `PushPath` (0x1800bd0e0) and `PopPath` (0x1800bd040) are the WRITERS of the
//   path stack this file reads, and they are NOT ported. They need
//   `0x18002e220` — `Collection::Array<PathStackEntry>::Resize(n, const T&)`,
//   the 0x10-element instantiation, which exists nowhere in the tree — plus its
//   element copy-ctor and destroy-range. Porting the readers today closes four
//   exported names and opens ZERO new address doors; porting the writers as
//   well would have opened at least three. See WAVE9_LANE_RRES.md.
//
// GetPathExtension LIVES HERE, NOT IN filename.cpp
//
//   `?GetPathExtension@Filename@System@@QEBA?AV12@XZ` (0x1800c64f0) is a
//   `System::Filename` method and its natural home is
//   `runtime/src/system/filename.cpp`. It is here instead because
//   `ExpandRelativePath` calls it twice and `PushPath` calls it once, it had no
//   caller in the tree before this lane (so porting it alone would have been a
//   `calls=0` port — reference_uncalled_port_is_not_verified.md), and
//   `filename.cpp` is a file other lanes edit.
//
//   **If a later lane ports it in `filename.cpp`, DELETE ONE OF THE TWO.** A
//   duplicate `RegisterRuntimeExport` is last-binding-wins and silently makes
//   one body dead code (reference_grep_for_the_port_before_writing_it.md).
// ============================================================================

#include <cstdint>

#include "../../types/object/External.h"
#include "../../types/system/Filename.h"

namespace RtObject {

using RtSystem::Filename;

// ?GetPathExtension@Filename@System@@QEBA?AV12@XZ           retail 0x1800c64f0
// Hidden return slot: RCX = self, RDX = ret.
Filename* RtSystem_Filename_GetPathExtension(const Filename* self, Filename* ret);

// ?GetPathStackFilename@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ
//                                                          retail 0x1800bcf60
Filename* RtObject_ExternalFactory_GetPathStackFilename(const ExternalFactory* self,
                                                        Filename* ret);

// ?GetPathStackDirectory@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ
//                                                          retail 0x1800bcfd0
Filename* RtObject_ExternalFactory_GetPathStackDirectory(const ExternalFactory* self,
                                                         Filename* ret);

// ?ExpandRelativePath@ExternalFactory@Object@@QEBA?AVFilename@System@@AEBV34@_N@Z
//                                                          retail 0x1800bd360
// RCX = this, RDX = hidden return slot, R8 = path, R9B = appendIdentifier.
Filename* RtObject_ExternalFactory_ExpandRelativePath(const ExternalFactory* self,
                                                      Filename* ret,
                                                      const Filename* path,
                                                      bool appendIdentifier);

// The effectiveness control. Every counter is a branch taken INSIDE a ported
// body, not a call to one: a large `calls` with an untaken branch is the
// `worked=0` trap one level down (rt_diff.h, lane M's Finding 1).
struct PathCounters {
    uint64_t expandCalls;
    uint64_t expandEmptyPath;     // the `!path->data || len == 0` early-out
    uint64_t identDefaulted;      // identifier was empty -> "resource"
    uint64_t identPresent;
    uint64_t filenameFromStack;   // filename was empty -> taken from the stack
    uint64_t filenamePresent;
    uint64_t stackEmpty;          // a GetPathStack* call on a 0-deep stack
    uint64_t stackNonEmpty;
    uint64_t hitPackfile;         // found through the GATED ExistsInPackfile
    uint64_t attrProbeEntered;    // the GetFileAttributesW arm was REACHED
    uint64_t hitAttributes;       // ... and it said the file exists
    uint64_t hitPackfileRetry;    // found through the UNGATED second probe
    uint64_t notFound;            // all three probes missed
    uint64_t appendedIdentifier;  // the R9B == true tail
    uint64_t noIdentifierTail;
    uint64_t extCalls;
    uint64_t extNullNoData;       // GetPathExtension: self->data == null
    uint64_t extNoDot;            // ... no '.' at or before the scan start
    uint64_t extEmptyAfterDot;    // ... '.' is the last character
    uint64_t extColonTrim;        // ... a ':' shortened the scan window
    uint64_t extOk;
};
extern PathCounters g_pc;

}  // namespace RtObject
