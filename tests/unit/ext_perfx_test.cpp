// ============================================================================
// tests/unit/ext_perfx_test.cpp -- PERFXEXT: PerfX as ext.render_culling (class A)
// and ext.anim_batch (class B)
// ============================================================================
// src/ext/perfx/perfx_rules.h on its own (the pure decisions) and the two registry
// rows (src/ext/registry.cpp): the flags each extension state gives the seams, the
// particle exactness rule (a frozen emitter is one whose retail tick could not call
// StopEmitter, checked against a literal copy of ParticleComponent::Tick's opacity
// branch over a grid of inputs), the IsAnimating-at-clock rule behind the anim_batch
// shadow, and the rows' class / lock / save behaviour.
// docs/extensions/handins/PERFXEXT.md
// ============================================================================
#include "unit.h"
#include "ext/perfx/perfx_rules.h"
#include "ext/registry.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

const Ext::Entry& E(const char* id) {
    const Ext::Entry* e = Ext::FindEntry(id);
    if (e == nullptr) { std::printf("  no entry %s\n", id); std::abort(); }
    return *e;
}

// ParticleComponent::Tick (src/game/Component/particlecomponent.cpp, retail 0x140052b30),
// the opacity branch of one live emitter, copied literally: does this tick call
// StopEmitter? (`op` is the emitter's opacity, `fadeRate` its +0x10.)
bool TickCallsStopEmitter(float op, float fadeRate, float dt, float uvScale) {
    if (fadeRate != 0.0f) {
        op += dt * fadeRate;
        if (op > uvScale) {
            return false;
        } else if (op < 0.0f) {
            return true;
        } else if (op <= 0.0f) {
            return true;
        }
    } else if (op <= 0.0f) {
        return true;
    }
    return false;
}

}  // namespace

UNIT_TEST(ext_perfx_flags_for_each_extension_state) {
    const PerfX::Flags off = PerfX::FlagsFor(false, false);
    CHECK(!off.any && !off.animCull && !off.geomXfLastOnly && !off.geomCull && !off.iconLastOnly &&
          !off.iconCull && !off.pickCull && !off.particleCull && !off.animBatch);
    CHECK(!PerfX::SideTableNeeded(off));   // the faithful profile builds no side table

    const PerfX::Flags cull = PerfX::FlagsFor(true, false);
    CHECK(cull.any && cull.animCull && cull.geomXfLastOnly && cull.geomCull && cull.iconLastOnly &&
          cull.iconCull && cull.pickCull && cull.particleCull);
    CHECK(!cull.animBatch);
    CHECK(PerfX::SideTableNeeded(cull));   // particle verdicts

    const PerfX::Flags batch = PerfX::FlagsFor(false, true);
    CHECK(batch.any && batch.animBatch);
    CHECK(!batch.animCull && !batch.geomXfLastOnly && !batch.geomCull && !batch.iconLastOnly &&
          !batch.iconCull && !batch.pickCull && !batch.particleCull);
    CHECK(PerfX::SideTableNeeded(batch));  // batch counts

    const PerfX::Flags both = PerfX::FlagsFor(true, true);
    CHECK(both.any && both.animBatch && both.animCull && both.particleCull);
    // The two culls that a later flush repairs ride the anim cull (FlushCulled pushes the
    // geometry, ModelCulled needs the frustum the anim cull keeps).
    for (int bits = 0; bits < 4; ++bits) {
        const PerfX::Flags f = PerfX::FlagsFor((bits & 1) != 0, (bits & 2) != 0);
        CHECK(!f.geomCull || f.animCull);
        CHECK(!f.iconCull || f.animCull);
        CHECK(f.any == (f.animCull || f.animBatch || f.particleCull || f.pickCull));
    }
    CHECK(PerfX::kParticleMargin > PerfX::kCullMargin);
}

UNIT_TEST(ext_perfx_particle_rule_matches_the_tick) {
    // RetailStopsEmitter is the tick's own decision, over a grid that includes the
    // boundaries (0, the UV scale, a fade that lands exactly on 0).
    const float ops[]   = {-1.0f, -0.0f, 0.0f, 1e-7f, 0.25f, 0.5f, 1.0f, 1.5f};
    const float fades[] = {-4.0f, -2.0f, -0.5f, -1e-7f, 0.0f, 1e-7f, 0.5f, 2.0f};
    const float dts[]   = {0.0f, 1.0f / 60.0f, 0.125f, 0.25f, 0.5f, 1.0f};
    const float uvs[]   = {1.0f, 0.5f};
    int checked = 0, quiet = 0;
    for (float op : ops)
        for (float fade : fades)
            for (float dt : dts)
                for (float uv : uvs) {
                    const bool tick = TickCallsStopEmitter(op, fade, dt, uv);
                    CHECK(PerfX::RetailStopsEmitter(fade, op, dt, uv) == tick);
                    // The exactness rule: a QUIET emitter's tick never stops it, whatever
                    // the dt (so freezing it hides no sim-visible write).
                    if (PerfX::EmitterQuiet(fade, op)) { CHECK(!tick); ++quiet; }
                    ++checked;
                }
    CHECK(checked == 8 * 8 * 6 * 2);
    CHECK(quiet > 0);
    // Not quiet: fading either way, or invisible.
    CHECK(PerfX::EmitterQuiet(0.0f, 0.5f));
    CHECK(!PerfX::EmitterQuiet(-1.0f, 0.5f));    // fading out: may reach 0 this tick
    CHECK(!PerfX::EmitterQuiet(1.0f, 0.5f));     // fading in: frozen, it would lag
    CHECK(!PerfX::EmitterQuiet(0.0f, 0.0f));     // retail stops it on this tick
    CHECK(!PerfX::EmitterQuiet(0.0f, -0.25f));
    // A fade-out that reaches exactly 0 stops (the `<= 0` arm).
    CHECK(PerfX::RetailStopsEmitter(-2.0f, 0.5f, 0.25f, 1.0f));
}

UNIT_TEST(ext_perfx_is_animating_at_clock) {
    // No layer: not animating, however many ticks are pending.
    CHECK(!PerfX::IsAnimatingAt(false, true, 0, 100, 0));
    // A looping clip animates at any clock: batching cannot change the answer.
    CHECK(PerfX::IsAnimatingAt(true, true, 99, 100, 0));
    CHECK(PerfX::IsAnimatingAt(true, true, 99, 100, 1000));
    // A non-looping clip: the batched clock (pending ticks not applied) still animates
    // while retail's per-tick clock has reached the end -- the class B divergence.
    CHECK(PerfX::IsAnimatingAt(true, false, 90, 100, 0));
    CHECK(!PerfX::IsAnimatingAt(true, false, 90, 100, 10));
    CHECK(PerfX::IsAnimatingAt(true, false, 90, 100, 9));
    CHECK(!PerfX::IsAnimatingAt(true, false, 100, 100, 0));
}

UNIT_TEST(ext_perfx_registry_rows) {
    const Ext::Entry& cull = E("render_culling");
    CHECK(cull.tier == Ext::Tier::Extension);
    CHECK(cull.cls == Ext::Class::A);
    CHECK(cull.defaultOn);      // class A ships ON
    CHECK(cull.formatVersion == 0);
    const Ext::Entry& batch = E("anim_batch");
    CHECK(batch.tier == Ext::Tier::Extension);
    CHECK(batch.cls == Ext::Class::B);
    CHECK(!batch.defaultOn);
    CHECK(batch.formatVersion >= 1);

    // Defaults and the faithful profile: both off.
    Ext::State s{};
    CHECK(!s.ext.render_culling && !s.ext.anim_batch);
    Ext::State all{};
    cull.slot(all) = true;
    batch.slot(all) = true;
    Ext::Install(all, Ext::Profile::PerEntry, false);
    CHECK(Ext::Ext().render_culling && Ext::Ext().anim_batch);

    // Under a game's lock: render_culling (A) toggles live, anim_batch (B) waits.
    int owner = 0;
    Ext::LockForGame(&owner);
    CHECK(Ext::SetRequested(cull, false));          // took effect now
    CHECK(!Ext::Ext().render_culling);
    CHECK(!Ext::SetRequested(batch, false));        // locked: the game keeps it
    CHECK(Ext::Ext().anim_batch);
    CHECK(Ext::EntryLocked(batch) && !Ext::EntryLocked(cull));
    // The save carries anim_batch as the pseudo-mod ext:anim_batch, never render_culling.
    std::vector<Ext::PseudoMod> mods(Ext::EntryCount());
    const int n = Ext::PseudoModsForSave(mods.data(), (int)mods.size());
    bool sawBatch = false, sawCull = false;
    for (int i = 0; i < n; ++i) {
        if (mods[i].path == "ext:anim_batch") { sawBatch = true; CHECK(mods[i].formatVersion == batch.formatVersion); }
        if (mods[i].path == "ext:render_culling") sawCull = true;
    }
    CHECK(sawBatch && !sawCull);
    Ext::UnlockForGame(&owner);
    CHECK(!Ext::Ext().anim_batch);                  // back at the menu: the request (off)

    Ext::Install(Ext::State{}, Ext::Profile::PerEntry, false);   // leave the defaults behind
}
