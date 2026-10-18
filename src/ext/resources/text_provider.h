#pragma once
// ============================================================================
// src/ext/resources/text_provider.h -- the provider for our embedded .rsc text
// ============================================================================
// NOT RETAIL (docs/extensions/KITFREE.md, block RSCTEXT). Resources.h is the
// shared contract; this header is RSCTEXT's own surface: the boot call, and
// what the cross-check harness (src/diag/rsc_text_check.cpp) needs.
// ============================================================================

#include <string>

namespace Ext {
namespace Resources {

// Parses the embedded files (tools/ext_rsc_embed.py -> ext_rsc_text.h) and
// registers the provider for the prefixes "Dialog/Extensions" and
// "Dialog/EnhancedOptions". Called once from the ext boot
// (registry_boot.cpp), never under the faithful profile. Logs `[ExtRes]`.
void InitText();

// How many objects every provider has handed the factory so far (the seam's
// count): the run log's proof that the exe built them, not a loose file.
int ProvidedCount();

// The embedded text's objects, as factory paths ("Dialog/Extensions.rsc:dialogRoot"),
// for the cross-check: `index` in [0, TextObjectCount()).
int  TextObjectCount();
bool TextObjectAt(int index, std::string& path);

}  // namespace Resources
}  // namespace Ext
