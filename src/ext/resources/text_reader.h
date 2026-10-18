#pragma once
// ============================================================================
// src/ext/resources/text_reader.h -- build ::Object::External objects from our
// .rsc text (docs/extensions/KITFREE.md, block RSCTEXT)
// ============================================================================
// NOT RETAIL. The reader is an IO::ReaderWriter driven by the object's OWN
// Serialize body (the same slots the binary reader answers), so the engine does
// every store, SetSize, String assignment and ExternalPtr resolve itself; this
// code only decides, per slot, whether the text names that field and what
// value to answer. Every other member is left exactly as the base copy (or the
// constructor) set it. The details are in text_reader.cpp's banner.
// ============================================================================

#include "ext/resources/text_parser.h"

namespace Object { class External; }

namespace Ext {
namespace RscText {

// Builds the object `obj` of the parsed file `file`, whose resource path is
// `dir` + `fileName` (e.g. "Dialog/", "Extensions.rsc"), and gives it the path
// "<dir><fileName>:<obj.name>". Derivation (`obj.base`) goes through the live
// ExternalFactory (FindOrLoad, so a base of the same file re-enters the
// provider). Null on any failure, logged as `[ExtRes]` on stderr.
::Object::External* BuildObject(const File& file, const Object& obj,
                              const char* dir, const char* fileName);

// True when `typeName` has a schema here (the description classes our files use).
bool KnowsType(const char* typeName);

}  // namespace RscText
}  // namespace Ext
