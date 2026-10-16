// ============================================================================
// src/ext/foliage_aa/foliage_aa.h -- ext.foliage_aa (class A, default OFF)
// ============================================================================
// NOT RETAIL. docs/extensions/FOLIAGE_AA.md (the plan, §6 the spike's rulings),
// docs/extensions/handins/FAA-CORE.md.
//
// Smooth alpha-tested edges (leaves, needles, twigs): the six retail cutout
// programs (x 4 DETAIL variants) are rewritten at load, in memory, from the
// user's own installed bytecode (src/render/dx11/foliage_aa/): the discard
// becomes a sharpened alpha written to o0.w and drawn with alpha to coverage
// (MSAA on); mode 2 also scales the alpha up with the mip level (the distance
// fill). With MSAA off the discard stays and mode 2 keeps only the fill.
//
// Settings: the bool row `ext.foliage_aa` (the Extensions dialog's checkbox)
// and the valued key `ext.foliage_aa_mode` (1..2, default 2; registry.cpp). The
// Options dialog's combo (block FAA-MENU) is Off / Smooth edges / Smooth edges +
// distance fill = Current().mode 0 / 1 / 2, written through Set().
//
//   Sync()   boot (Ext::InitFromSettings), every Extensions-dialog toggle
//            (Ext::SetAndRecord) and every Set(): hands the mode and k to the
//            render side (FaaBackend::Request). A mode / k change re-creates the
//            transformed shaders (k is an immediate in the emitted code).
// ============================================================================
#pragma once

namespace Ext {
namespace FoliageAA {

enum Mode : int {
    kOff          = 0,   // the row is off (retail)
    kSmoothEdges  = 1,   // part 1: alpha to coverage + the sharpened alpha
    kDistanceFill = 2,   // parts 1 + 2: + alpha *= 1 + max(0, lod) * k
};

// Part 2's k (FOLIAGE_AA §3 "from ~0.25"; FAA-TUNE row 3 moved it to 0.15: at
// the overview zoom the canopy's covered samples are +6.3% over the mip-0
// reference at 0.15, +8.0% at 0.25, +9.6% at 0.40; smooth edges alone already
// +1.2%. The user's look ruling may move it). Baked into the emitted code as an
// immediate.
constexpr float kDistanceFillK = 0.15f;

// The fill floor (FAA-TUNE, FOLIAGE_AA §6.5; handins/FAA-TUNE.md row 1): where
// the sampled alpha is under this, mode 2 neither fills nor covers, so a low-alpha
// texel (whose filtered RGB is not leaf colour: the blue-grey speckles) is never
// pulled into coverage. Measured: under 0.15 the covered samples are 41-83%
// blue-over-green against 3% for the leaf; the floor costs 0.4% of the canopy's
// covered samples at the overview. Baked into the emitted code beside k.
constexpr float kDistanceFillFloor = 0.15f;

struct Params {
    int mode = kOff;   // 0 = the row is off, else ext.foliage_aa_mode (1..2)
};

// The live value: mode = row on ? ext.foliage_aa_mode : 0. Cheap; never read
// per draw (the render side holds what Sync last handed it).
const Params& Current();

// The menu's write (FAA-MENU): mode <= 0 turns the row off (the key keeps its
// stored mode); mode >= 1 stores clamp(mode, 1, 2) and turns the row on. Records
// the settings keys (the caller flushes, Ext::PersistSettings or retail's
// WriteSettings), then Sync()s.
void Set(const Params& p);

void Sync();

}  // namespace FoliageAA
}  // namespace Ext
