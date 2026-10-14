// ============================================================================
// tests/unit/ext_render_pipe_test.cpp -- RENDERPIPE-0: ext.render_pipe (class A)
// ============================================================================
// The pure parts of src/ext/render_pipe on their own:
//   * packet.h's Emit: the second half's steps in retail's order (MainLoop::Frame
//     tail -> MainLoop::Draw -> GameUpdateLoop::Draw's context half, pass 2, the
//     post-scene draws -> the post-draw callbacks, EndCommands, the frame index,
//     Finish), and the BeginCommands failure path stopping where MainLoop::Draw's
//     does;
//   * pipe_state.h's PipeState: the ping-pong of the two Renderer slots, the
//     first frame holding nothing, the fence before a step, every "drain, then
//     run the frame unpipelined" transition (toggle off + uninstall, resize, the
//     minimap paint, the eye camera, a screenshot, a quit, a loop change, a
//     diagnostic), the device-lost discard, and renderIdle (fixes 4 and 5);
//   * a scripted run of the state machine over many frames: never two packets
//     held, pass 1 never fills the held packet's slot, every held packet emitted
//     before any step;
//   * the registry row.
// docs/extensions/handins/RENDERPIPE0.md
// ============================================================================
#include "unit.h"
#include "ext/render_pipe/packet.h"
#include "ext/render_pipe/pipe_state.h"
#include "ext/registry.h"

#include <random>
#include <string>
#include <vector>

namespace {

using RenderPipe::Packet;
using RenderPipe::PipeState;
using RenderPipe::TailInputs;
using RenderPipe::TailPlan;

struct RecordOps {
    std::vector<std::string> log;
    bool begin = true;
    void FrameTailTime(const Packet&)  { log.push_back("FrameTailTime"); }
    bool BeginCommands()               { log.push_back("BeginCommands"); return begin; }
    void BeginFailed()                 { log.push_back("BeginFailed"); }
    void ClearResourcesIfDirty()       { log.push_back("ClearResourcesIfDirty"); }
    void ViewportReconcile()           { log.push_back("ViewportReconcile"); }
    void PacketBegin()                 { log.push_back("PacketBegin"); }
    void RenderDocFrame()              { log.push_back("RenderDocFrame"); }
    void SetTime(const Packet&)        { log.push_back("SetTime"); }
    void WeatherContext(const Packet&) { log.push_back("WeatherContext"); }
    void ForceSnow(const Packet&)      { log.push_back("ForceSnow"); }
    void Pass2(const Packet&)          { log.push_back("Pass2"); }
    void SaveLoadDraw(const Packet&)   { log.push_back("SaveLoadDraw"); }
    void SetCamera(const Packet&)      { log.push_back("SetCamera"); }
    void ToolDraw(const Packet&)       { log.push_back("ToolDraw"); }
    void UIDraw(const Packet&)         { log.push_back("UIDraw"); }
    void PacketEnd()                   { log.push_back("PacketEnd"); }
    void PostDraw()                    { log.push_back("PostDraw"); }
    void EndCommands()                 { log.push_back("EndCommands"); }
    void FrameUpdateTime()             { log.push_back("FrameUpdateTime"); }
    void IncFrameIndex()               { log.push_back("IncFrameIndex"); }
    void Finish()                      { log.push_back("Finish"); }
};

TailInputs Clear() { return TailInputs{}; }

}  // namespace

UNIT_TEST(ext_render_pipe_emit_replays_in_retail_order) {
    RecordOps ops;
    Packet p;
    RenderPipe::Emit(ops, p);
    // Retail's frame from the Frame tail on, minus the main half's steps (3, 4,
    // the sun half of 5, 6 and pass 1): mainloop_frame.cpp tail, loops.cpp
    // MainLoop::Draw, gameupdateloop.cpp Draw steps 2, 5, 7 (pass 2), 8, 10-12.
    const std::vector<std::string> want = {
        "FrameTailTime", "BeginCommands", "ClearResourcesIfDirty", "ViewportReconcile",
        "PacketBegin", "RenderDocFrame", "SetTime", "WeatherContext", "ForceSnow", "Pass2",
        "SaveLoadDraw", "SetCamera", "ToolDraw", "UIDraw", "PacketEnd",
        "PostDraw", "EndCommands", "FrameUpdateTime", "IncFrameIndex", "Finish",
    };
    CHECK(ops.log == want);
}

UNIT_TEST(ext_render_pipe_emit_stops_after_a_failed_begin) {
    RecordOps ops;
    ops.begin = false;
    Packet p;
    RenderPipe::Emit(ops, p);
    const std::vector<std::string> want = { "FrameTailTime", "BeginCommands", "BeginFailed" };
    CHECK(ops.log == want);
}

UNIT_TEST(ext_render_pipe_first_frame_holds_nothing_then_ping_pongs) {
    PipeState s;
    CHECK(!s.Pending());
    TailPlan plan = s.PlanTail(Clear());
    CHECK(plan.pipelined);
    CHECK(!plan.drain);
    CHECK(plan.renderIdle);              // nothing held: SunLight::Set / UpdateCamera run
    const int32_t first = plan.pass1Slot;
    CHECK(first == 0 || first == 1);
    s.Hold(first);
    CHECK(s.Pending());
    CHECK(s.PendingSlot() == first);

    for (int frame = 0; frame < 8; ++frame) {
        plan = s.PlanTail(Clear());
        CHECK(plan.pipelined);
        CHECK(!plan.renderIdle);         // a packet is held: fixes 4 and 5 skip
        CHECK(plan.pass1Slot == 1 - s.PendingSlot());
        const int32_t slot = plan.pass1Slot;
        s.Drained();                     // the held packet emitted after pass 1
        s.Hold(slot);
        CHECK(s.PendingSlot() == slot);
    }
}

UNIT_TEST(ext_render_pipe_fence_before_a_step) {
    PipeState s;
    CHECK(!s.DrainBeforeSteps(true));    // nothing held
    s.Hold(s.FreeSlot());
    CHECK(!s.DrainBeforeSteps(false));   // no step this Frame: keep it
    CHECK(s.DrainBeforeSteps(true));
    s.Drained();
    CHECK(!s.Pending());
    // The first frame after the step: nothing held, so the sun / camera writes run.
    const TailPlan plan = s.PlanTail(Clear());
    CHECK(plan.pipelined);
    CHECK(plan.renderIdle);
}

UNIT_TEST(ext_render_pipe_every_transition_drains_and_runs_unpipelined) {
    using Field = bool TailInputs::*;
    const Field fields[] = { &TailInputs::quitting, &TailInputs::settingsChanged,
                             &TailInputs::paintTerrain, &TailInputs::eyeCamera,
                             &TailInputs::screenshot, &TailInputs::diagnostic };
    for (Field f : fields) {
        PipeState s;
        s.Hold(1);
        TailInputs in = Clear();
        in.*f = true;
        const TailPlan plan = s.PlanTail(in);
        CHECK(plan.drain);
        CHECK(!plan.pipelined);
        CHECK(!plan.uninstall);
        // With nothing held there is nothing to drain, and the frame is still sequential.
        PipeState empty;
        const TailPlan plan2 = empty.PlanTail(in);
        CHECK(!plan2.drain);
        CHECK(!plan2.pipelined);
    }
    {   // the loop is not the game's (menu loop, a load, the world not built)
        PipeState s;
        s.Hold(0);
        TailInputs in = Clear();
        in.gameLoop = false;
        const TailPlan plan = s.PlanTail(in);
        CHECK(plan.drain);
        CHECK(!plan.pipelined);
    }
    {   // toggle off: drain, the faithful tail, then the seam is removed
        PipeState s;
        s.Hold(0);
        TailInputs in = Clear();
        in.want = false;
        const TailPlan plan = s.PlanTail(in);
        CHECK(plan.drain);
        CHECK(!plan.pipelined);
        CHECK(plan.uninstall);
    }
}

UNIT_TEST(ext_render_pipe_faithful_frame_and_discard_pick_the_other_slot_next) {
    PipeState s;
    s.FaithfulFrame();                    // the loop's own Renderer (slot 0) drew
    CHECK(s.FreeSlot() == 1);
    s.Hold(1);
    s.Drained();
    s.Discard(0);                         // device lost: the new packet in slot 0 is dropped
    CHECK(!s.Pending());
    CHECK(s.FreeSlot() == 1);
    const TailPlan plan = s.PlanTail(Clear());
    CHECK(plan.renderIdle);
}

UNIT_TEST(ext_render_pipe_scripted_frames_keep_the_ownership_rules) {
    std::mt19937 rng(20261014u);
    PipeState s;
    int presented = 0, gathered = 0, discarded = 0;
    for (int frame = 0; frame < 20000; ++frame) {
        // A Frame: maybe steps (fence), then the tail.
        const bool steps = (rng() % 7) == 0;
        if (s.DrainBeforeSteps(steps)) { s.Drained(); ++presented; }
        if (steps) CHECK(!s.Pending());   // the sim never runs with a packet held

        TailInputs in = Clear();
        const uint32_t r = rng() % 100;
        if (r == 0) in.settingsChanged = true;
        else if (r == 1) in.eyeCamera = true;
        else if (r == 2) in.screenshot = true;
        else if (r == 3) in.gameLoop = false;
        const TailPlan plan = s.PlanTail(in);
        if (plan.drain) { s.Drained(); ++presented; }
        if (!plan.pipelined) {
            CHECK(!s.Pending());
            s.FaithfulFrame();
            ++presented;
            continue;
        }
        CHECK(plan.renderIdle == !s.Pending());
        CHECK(!s.Pending() || plan.pass1Slot != s.PendingSlot());   // never the held packet's slot
        ++gathered;
        bool keep = true;
        if (s.Pending()) {
            s.Drained();
            ++presented;
            keep = (rng() % 997) != 0;      // a device loss in the emit
        }
        if (keep) s.Hold(plan.pass1Slot);
        else { s.Discard(plan.pass1Slot); ++discarded; }
    }
    CHECK(gathered > 0);
    CHECK(presented > 0);
    (void)discarded;
}

UNIT_TEST(ext_render_pipe_registry_row) {
    const Ext::Entry* e = Ext::FindEntry("render_pipe");
    CHECK(e != nullptr);
    if (e == nullptr) return;
    CHECK(e->tier == Ext::Tier::Extension);
    CHECK(e->cls == Ext::Class::A);
    CHECK(!e->defaultOn);
    CHECK(e->formatVersion == 0);
    CHECK(!Ext::IsLockedClass(e->cls));
    CHECK(Ext::ValidId(e->id));
    CHECK(std::string(e->displayName) == "Render thread");
}
