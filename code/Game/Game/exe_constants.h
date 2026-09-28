#pragma once
// code/Game/Game/exe_constants.h -- the Application-x64.exe .rdata constants that
// code/Game reads BY RETAIL ADDRESS today (ENDGAME B.3R, block W0-P0c, B3R_RULES R7).
//
// One `inline constexpr` per .rdata address, named after the address. VALUE AND TYPE
// are copied from the storage the registering file hands to RegisterExeAddr (those
// values were verified against the image when they were registered): a bit pattern
// where the file stores a bit pattern (uint32_t / uint64_t, so the float is exact),
// the literal where it stores a float or double. The meaning is the registering
// file's own, where it states one.
//
// Five addresses have two storage cells holding the same value; each is ONE constant
// here, typed like the cell that is live today (the one registered LAST in
// AddressRegistryInit), the other cell named in its comment.
//
// Nothing reads these yet: the registration rows stay, and the readers (the
// `p ? *p : fallback` sites) convert in W1.

#include <cstdint>

// ---- 0x1401720xx
inline constexpr uint32_t kRdata_1401720b0 = 0x3991A2B4U;  // 0.000277777785 = 1/3600, one second of a game year (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_1401720b8 = 0x3A83126FU;  // 0.001 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_1401720bc = 0x3B800000U;  // 1/256 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_1401720c0 = 0x3C23D700u;  // 0.01f, growth new-width scale (reg_unreg_lane4.cpp)
inline constexpr uint32_t kRdata_1401720cc = 0x3C8EFA35U;  // pi/180, degrees -> radians (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_1401720d4 = 0.05f;        // MoveTo minimum arrival distance, squared (reg_action_move.cpp)
inline constexpr uint32_t kRdata_1401720e4 = 0x3DCCCCCDU;  // 0.1 (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_1401720ec = 0.125f;       // ConsumeFuel heating-demand scale (reg_component_gap_A.cpp)
inline constexpr uint32_t kRdata_1401720f4 = 0x3E19999Au;  // 0.15f, crop spread divisor (reg_unreg_lane4.cpp)
inline constexpr float    kRdata_1401720fc = 0.2f;         // boat state 1->2 dock range (reg_boat_ctrl.cpp)

// ---- 0x1401721xx
inline constexpr float    kRdata_140172100 = 0.25f;        // ConsumeFood stale-variety penalty (reg_component_gap_A.cpp)
inline constexpr uint32_t kRdata_14017210c = 0x3EA8F5C3U;  // 0.33 (reg_rdata_float_consts.cpp; also reg_boat_ctrl.cpp k_rdata_17210c = 0.33f, boat state-1 speed clamp, low)
inline constexpr float    kRdata_140172110 = 0.4f;         // HerdComponent_Populate sub-tile spawn jitter (reg_herd_live.cpp)
inline constexpr float    kRdata_140172114 = 0.5f;         // msg-3 happiness increment (reg_citizen_needsupdate.cpp)
inline constexpr uint32_t kRdata_140172120 = 0x3F2B851EU;  // 0.67 (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_140172124 = 0.75f;        // (reg_citizenai_actionctors.cpp)
inline constexpr uint32_t kRdata_14017213c = 0x3F7851ECU;  // 0.97 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_140172140 = 0x3f7ae148u;  // 0.98f, UV / spawn scale (reg_component_nature.cpp)
inline constexpr float    kRdata_140172144 = 0.99f;        // crop maturity threshold (reg_food_typeid.cpp)
inline constexpr uint32_t kRdata_140172148 = 0x3F7FBE77U;  // 0.999 (reg_rdata_float_consts.cpp; the row is DEAD)
inline constexpr float    kRdata_14017214c = 1.0f;         // (gameupdateloop_load.cpp s_fConst_1_0)
inline constexpr double   kRdata_140172168 = 0.5;          // warmth decay step when demand < current, a double (reg_component_gap_A.cpp)
inline constexpr float    kRdata_140172174 = 2.0f;         // camera parameter (gameupdateloop_load.cpp s_fCamParam2)
inline constexpr float    kRdata_140172178 = 2.1f;         // plant / hunt PerformWork time (reg_work_actionctor.cpp)
inline constexpr float    kRdata_140172180 = 3.0f;         // anim-time base (reg_citizenai_handlers.cpp)
inline constexpr float    kRdata_140172184 = 3.14159274f;  // pi, search-area term sqrtf(radiusSq * PI) (reg_citizenai_handlers.cpp)
inline constexpr uint32_t kRdata_140172188 = 0x40600000U;  // 3.5 (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_140172194 = 4.0f;         // (reg_citizenai_actionctors.cpp)
inline constexpr float    kRdata_140172198 = 5.0f;         // (reg_citizenai_actionctors.cpp)
inline constexpr float    kRdata_14017219c = 6.0f;         // audio time scale (gameupdateloop_load.cpp s_fAudioTimeScale; also reg_rdata_float_consts.cpp 0x40C00000)
inline constexpr float    kRdata_1401721a0 = 7.0f;         // anim-time scale (reg_citizenai_handlers.cpp)
inline constexpr float    kRdata_1401721a8 = 8.0f;         // warmth flag-bit-1 penalty (reg_component_gap_A.cpp)
inline constexpr float    kRdata_1401721b0 = 10.0f;        // grazing PerformWork time (reg_work_actionctor.cpp)
inline constexpr uint32_t kRdata_1401721b8 = 0x41400000u;  // 12.0f, months per year (reg_component_fruit.cpp)
inline constexpr uint32_t kRdata_1401721cc = 0x41F00000u;  // 30.0f, event-notify timer / autosave interval entry (reg_unreg_lane4.cpp)
inline constexpr uint32_t kRdata_1401721d0 = 0x42000000u;  // 32.0f, freeze temperature (reg_component_nature.cpp)
inline constexpr float    kRdata_1401721d4 = 33.0f;        // planting temperature, one degree above freezing (reg_citizenai_handlers.cpp)
inline constexpr float    kRdata_1401721dc = 40.0f;        // camera parameter (gameupdateloop_load.cpp s_fCamParam1; also reg_rdata_float_consts.cpp 0x42200000)
inline constexpr uint32_t kRdata_1401721e4 = 0x42400000U;  // 48 (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_1401721f4 = 64.0f;        // boat state 0->1 gate, squared (8.0) (reg_boat_ctrl.cpp)
inline constexpr float    kRdata_1401721fc = 75.0f;        // CollectStats interval (reg_citizenbehavior_tick.cpp)

// ---- 0x1401722xx
inline constexpr float    kRdata_140172204 = 90.0f;        // quarter turn of a dropped produce pile (reg_pasture_live.cpp)
inline constexpr uint32_t kRdata_140172208 = 0x42C80000U;  // 100 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_140172214 = 0x42FF0000U;  // 127.5 (reg_rdata_float_consts.cpp; the row is DEAD)
inline constexpr float    kRdata_140172220 = 180.0f;       // herd spawn spread (reg_component_animal.cpp)
inline constexpr uint32_t kRdata_140172228 = 0x43480000U;  // 200 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_14017222c = 0x437f0000u;  // 255.0f, byte <-> float scale (reg_component_nature.cpp)
inline constexpr uint32_t kRdata_140172230 = 0x43800000U;  // 256 (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_140172234 = 300.0f;       // msg-6/7 Time scale (reg_citizen_needsupdate.cpp)
inline constexpr uint32_t kRdata_140172238 = 0x43B40000U;  // 360, a full turn in degrees (reg_rdata_float_consts.cpp)
inline constexpr float    kRdata_140172240 = 512.0f;       // camera parameter (gameupdateloop_load.cpp s_fCamParam3; also reg_rdata_float_consts.cpp 0x44000000)
inline constexpr uint32_t kRdata_140172248 = 0x447a0000u;  // 1000.0f, spawn-distance scale (reg_component_nature.cpp)
inline constexpr uint32_t kRdata_140172254 = 0x44E10000U;  // 1800, half a game year (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_14017225c = 0x45610000u;  // 3600.0f, seconds per year (reg_component_fruit.cpp)
inline constexpr uint32_t kRdata_140172264 = 0x46800000U;  // 16384 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_14017226c = 0xB8D1B717U;  // -1e-4 (reg_rdata_float_consts.cpp; the row is DEAD)
inline constexpr float    kRdata_140172280 = -1.0f;        // MoveTo speed sentinel -> randomized default (reg_citizenai_handlers.cpp)
inline constexpr uint32_t kRdata_140172288 = 0xc1200000u;  // -10.0f, growth rate at freeze (reg_component_nature.cpp)
inline constexpr uint32_t kRdata_1401722a0 = 0xC3340000U;  // -180 (reg_rdata_float_consts.cpp)
inline constexpr uint32_t kRdata_1401722a8 = 0xC47A0000U;  // -1000, PerformWork "no heading" sentinel (reg_rdata_float_consts.cpp)
inline constexpr uint64_t kRdata_1401722b0 = 0x7fffffffffffffffull;  // double fabs mask (reg_component_nature.cpp)
inline constexpr uint32_t kRdata_1401722c0 = 0x80000000U;  // -0.0, float sign-flip mask (reg_rdata_float_consts.cpp; also reg_fire.cpp g_DAT_1401722c0 = 0x80000000u)
