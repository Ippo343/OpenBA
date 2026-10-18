#pragma once
// ============================================================================
// src/diag/rsc_text_check.h -- the KITFREE cross-check (KITFREE.md section 5)
// ============================================================================
// NOT RETAIL. Builds every object of the embedded .rsc text through the
// provider (Ext::Resources::Provide, bypassing the factory's cache so a loose
// .crs cannot stand in), writes each one with the ported IO::BinaryWriter in
// the layout External::Load reads (magic, type, version, an EMPTY dependency
// list, the object) to <outDir>/<File>_<name>.bin, and logs `[RscCheck]`.
// tools/ext_rsc_check.py compares the files with the kit's compiled .crs.
// Armed by `/rsccheck:1` on the command line (registry_boot.cpp).
// ============================================================================

namespace Diag {

void RscTextCheck(const char* outDir);

// RscTextCheck(outDir) once per outDir when `/rsccheck:1` is on the command
// line (System::Configuration), else nothing. Called at the ext boot
// (registry_boot.cpp, "standalone_rsccheck") and when the main menu attaches
// the Extensions button ("standalone_rsccheck_menu"): the two moments differ
// in what the factory can load.
void RscTextCheckIfArmed(const char* outDir);

}  // namespace Diag
