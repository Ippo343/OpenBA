// standalone/reg_player_render.cpp
// Player registration wave, round 2d — batch "render" registrar.
//
// Entity SELECTION-OUTLINE (the retail yellow outline around a selected/followed
// entity). The full HighlightComponent EdgeMesh tessellation chain is ported in
// code/Game/Player/tool_base_render.cpp (b930 tessellator + c4a0 render mutation
// + the c9e0/cb40/7080/b440/b10e0/74a0 closure). This registrar wires the entry
// points our other reimplemented code reaches by exe address:
//
//   0x1400fc9e0  Entity_impl_c9e0  -> HighlightSelect_Impl   (select finalize; the
//                one edittool_select.cpp resolves as s_fn_c9e0 to paint the outline)
//   0x14004c4a0  Entity_impl_c4a0  -> HighlightRepaint_Impl  (rebuild+resubmit mesh)
//   0x1400fcb40  Entity_impl_cb40  -> HighlightClear_Impl    (clear highlight flag)
//
// *** DEFAULT OFF — gated on BANISHED_SELECTION_OUTLINE ***
// When the env var is unset (the shipping default) NOTHING is registered, so
// s_fn_c9e0/s_fn_cc90 in edittool_select.cpp stay null and the select path is
// byte-identical to today (info dialog opens, no overlay). This is deliberate:
// b930 mutates the live Graphics::Scene, so a wrong tessellation would be silent
// corruption with no A/B signal on this DORMANT path — keeping it opt-in means
// the default build is provably unaffected. Enable with
//   BANISHED_SELECTION_OUTLINE=1 make sa-run   (or add to a test env combo).
//
// The internal closure (b930, b440, b10e0, 74a0, 7080, cb40) is NOT registered
// separately — those are direct C++ calls within the ported TU. (b10e0 is also
// registered by RegisterPlayerTileRenderBatch for the placement footprint grid;
// registering it here too would double-register.)
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>
#include <cstdlib>

extern "C" void HighlightSelect_Impl(void*);    // 0x1400fc9e0
extern "C" void HighlightClear_Impl(void*);     // 0x1400fcb40
extern "C" void HighlightRepaint_Impl(void*);   // 0x14004c4a0

void RegisterPlayerRenderBatch() {
    const char* e = std::getenv("BANISHED_SELECTION_OUTLINE");
    if (!e || !e[0] || e[0] == '0') return;   // default OFF -> leave everything unregistered

    RegisterExeAddr(0x1400fc9e0, reinterpret_cast<uintptr_t>(&HighlightSelect_Impl));
    RegisterExeAddr(0x1400fcb40, reinterpret_cast<uintptr_t>(&HighlightClear_Impl));
    RegisterExeAddr(0x14004c4a0, reinterpret_cast<uintptr_t>(&HighlightRepaint_Impl));
}
#endif
