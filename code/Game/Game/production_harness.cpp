// code/Game/Game/production_harness.cpp
//
// RETIRED 2026-09-13 (ENDGAME C.2): this file carried a one-shot harness that drove the
// real production-ACCOUNTING tail (RegisterRawMaterial 0x140088ef0) once for the
// force-worked ForesterLodge, to prove the town stock count increments. It only ever ran
// under BANISHED_LIVE_PRODUCTION, a default-OFF diagnostic flag nothing set; the flag and
// the harness went together. The accounting path it exercised is unaffected and still
// lives in code/Game/Action/action_base_rawmat.cpp.
//
// The entry point is kept as a no-op because CitizenAIComponent's tick still calls it
// (code/Game/Component/citizenaicomponent.cpp); the call and this file can go together in
// a later pass.

extern "C" void StartTown_DriveProduction() {
}
