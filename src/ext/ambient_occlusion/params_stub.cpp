// ============================================================================
// src/ext/ambient_occlusion/params_stub.cpp -- TEMPORARY. AOMENU: DELETE THIS FILE.
// ============================================================================
// NOT RETAIL. AOCORE's placeholder for the two functions params.h declares and
// AOMENU owns (the settings keys ext.ao_quality / ext.ao_opacity, settings.cpp).
// It exists only so the AOCORE branch links on its own: Current() returns the
// defaults (quality 2 = Medium, opacity 100), Set() stores without recording.
// When AOMENU's settings.cpp lands, this file goes (a duplicate definition fails
// the link, so the merge cannot keep both).
// ============================================================================
#include "ext/ambient_occlusion/params.h"

namespace Ext {
namespace AO {

namespace {
Params g_stubParams;
}

const Params& Current() { return g_stubParams; }

void Set(const Params& p) { g_stubParams = p; }

}  // namespace AO
}  // namespace Ext
