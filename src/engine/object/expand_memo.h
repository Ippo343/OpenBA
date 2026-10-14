#pragma once
// ============================================================================
// src/engine/object/expand_memo.h -- the ExpandRelativePath memo
//                                     (block SAVELOAD, NOT RETAIL, exact)
// ============================================================================
//
// WHY. `ExternalFactory::FindOrLoad` expands every request through
// `ExpandRelativePath` (src/engine/io/external_path.cpp, 0x1800bd360): four
// path splits, two six-term concatenations, a package-directory probe and the
// answer's concatenation -- ~25 String allocations and ~10 AdjustDelimiters
// per call. A save load asks it once per resource REFERENCE (every entity's
// Description, MaterialInstance, ...), ~100k times for a few thousand distinct
// names; the census (LOADPLAN §2.2) put FindOrLoad at 73% of the 785 ms
// `ser.entities` stage. The answer for one request never changes while its
// inputs do not, so the second and later identical requests take it from here.
//
// WHAT THE ANSWER DEPENDS ON (read off the body, every input):
//   1. the request's characters                      -> the KEY
//   2. `appendIdentifier`                            -> the KEY
//   3. the factory's path-stack top (filename, directory) -> the memo is used
//      ONLY while the stack is EMPTY (the caller tests it); a request made
//      while an .rsc is being parsed always runs the body
//   4. `FileSystem::_singleton` and its `_dataDirectory` characters, which
//      build the probed candidate                    -> the CONTEXT
//   5. whether the candidate exists. Three probes decide it: the gated package
//      probe, GetFileAttributesW (gated), the ungated package probe. The memo
//      STORES ONLY an answer a PACKAGE probe gave (the caller passes that
//      flag). If the package directory holds the candidate, the outcome is
//      "found" whatever the two gate bytes and the disk say (gate set: the
//      first probe finds it; gate clear: the disk probe finds it or the retry
//      does), so such an answer depends on the package directory alone
//                                                    -> the CONTEXT's generation
//      (src/engine/io/packfile_generation.h, bumped on every change of
//      `FileSystem::_packfileIndex`). A "not found" answer, or one the disk
//      probe gave, is never stored: those requests run the body every time.
// When the context differs from the one the entries were made under, every
// entry is dropped before the lookup. So an entry can only be returned under
// the inputs that produced it.
//
// WHAT A HIT SKIPS. Only work with no effect outside the body: its own
// allocations, the probes (reads; GetFileAttributesW is a stat), the `g_pc`
// counters (nothing reads them), and AdjustDelimiters calls on fresh buffers or
// on already-normalised shared ones (the data directory, a no-op: AdjustDelimiters
// is idempotent, tests/unit/filename_adjust_test.cpp). The request's own buffer
// is never written by the body (its one copy, in GetPathIdentifer, happens only
// for a request with no '/' or '\', where AdjustDelimiters writes nothing).
// The returned Filename is a fresh, unshared buffer holding the same
// characters, as the body's is. `Find`, `Add` and `External::Load` in
// FindOrLoad run exactly as before: the memo is below them.
//
// THREADS. Every call is made under `ResLoadGuard` (ExpandRelativePath takes
// it first), so the state below is never touched by two threads at once.
// ============================================================================

#include <cstdint>
#include <string>

namespace RtObject {

// The cached answer for (path[0..len), appendIdentifier) under the context
// (fs, dataDir, generation), or null. A context that differs from the stored
// one drops every entry first. `dataDir` may be null.
const std::string* ExpandMemoFind(const char* path, int len, bool appendIdentifier,
                                  const void* fs, const char* dataDir, uint32_t generation);

// Remember `result` (resultLen characters) for that request under that context.
void ExpandMemoStore(const char* path, int len, bool appendIdentifier,
                     const void* fs, const char* dataDir, uint32_t generation,
                     const char* result, int resultLen);

// For the unit tests: the entry count, and a full reset.
size_t ExpandMemoSize();
void   ExpandMemoClear();

}  // namespace RtObject
