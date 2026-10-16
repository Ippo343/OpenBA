// ============================================================================
// src/ext/foliage_aa/foliage_aa.cpp -- foliage_aa.h
// ============================================================================
// NOT RETAIL. ext.foliage_aa (docs/extensions/handins/FAA-CORE.md).
// ============================================================================
#include "ext/foliage_aa/foliage_aa.h"

#include "ext/registry.h"

#include "foliage_aa/foliage_aa_backend.h"   // src/render/dx11/foliage_aa: no D3D types

namespace Ext {
namespace FoliageAA {

namespace {

// Both are in the registry's tables (registry.cpp); the unit tests check the
// names, so a miss is a build-time typo, not a run-time state.
const ValueKey& ModeKey() { return *FindValueKey("foliage_aa_mode"); }
const Entry&    Row()     { return *FindEntry("foliage_aa"); }

}  // namespace

const Params& Current() {
    // Per thread (the AO precedent, ambient_occlusion/settings.cpp): rebuilt on
    // every call, so a reader never sees a half-written copy.
    thread_local Params p;
    p.mode = Ext::Ext().foliage_aa ? Val().foliage_aa_mode : kOff;
    return p;
}

void Set(const Params& p) {
    const bool on = p.mode > 0;
    if (on) SetValueAndRecord(ModeKey(), p.mode);
    const Entry& row = Row();
    if (Requested(row) != on) SetAndRecord(row, on);   // its Sync hooks run this file's Sync too
    Sync();                                            // a mode change under an unchanged row
}

void Sync() {
    const Params& p = Current();
    const bool fill = p.mode == kDistanceFill;
    FaaBackend::Request(p.mode, fill ? kDistanceFillK : 0.0f, fill ? kDistanceFillFloor : 0.0f);
}

}  // namespace FoliageAA
}  // namespace Ext
