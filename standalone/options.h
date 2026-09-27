// ============================================================================
// standalone/options.h — THE inventory of BANISHED_* environment flags.
//
// ENDGAME C.3. Every environment flag the exe honours is a field of the one
// `Options` struct below, filled ONCE at boot by `OptionsInit()` (called from
// the top of WinMain, before anything else looks at the environment) and read
// everywhere else through `Opt()`. NOTHING in the tree calls getenv outside
// standalone/options.cpp — code/, runtime/, dx11/, engine/ and standalone/ all
// read `Opt()`.
//
// HOW TO READ THIS FILE. Each field carries a one-line comment naming WHO SETS
// IT — a Makefile target, a script under tools/, perf/, .vscode/launch.json,
// or a route documented in CLAUDE.md / docs/runtime_port/STATUS.md. A field
// marked `[no tool reader]` is a diagnostic nothing outside the tree sets: it
// is an ENDGAME C.4 delete candidate, kept here only so the inventory is
// complete and the deletion can be decided from one list rather than by
// grepping 50 files.
//
// TYPED vs RAW. Flags a tool or a documented route sets get a typed field, and
// options.cpp owns their parse. The per-wave gameplay harnesses keep a RAW
// `const char*` (the exact value the variable had, null when unset) and parse
// it where they always did: those parses are quirky one-offs, and the point of
// this pass is to move the READ, not to rewrite the harnesses that are on
// their way out.
//
// ADDING A FLAG. Don't, unless the feature is unproven (DIRECTION.md §2: a
// verified feature is unconditional code, not a flag). If you must: one field,
// one comment naming the reader, one line in OptionsInit.
//
// STRING FIELDS point into the CRT environment block and stay valid for the
// life of the process. The one place the environment is written is the
// stress-town default block in OptionsInit (see options.cpp), and that runs
// before any reader.
// ============================================================================
#pragma once
#include <cstdint>

struct Options {
    // ── harness drivers: menu → game (standalone/autostart.cpp) ──────────────
    bool        autostart            = false;  // Makefile sa-run, tools/cutover_run.sh, perf/mintsel, CLAUDE.md
    int         autostartFrames      = 0;      //   numeric value > 1 overrides the 60-frame menu settle; 0 = default
    int         autostartGames       = 0;      // BANISHED_AUTOSTART_GAMES — quit-to-menu → New Game repeats [no tool reader]
    bool        autoquit             = false;  // CLAUDE.md / STATUS.md quit route (settings round-trip)
    int         autoquitFrames       = 0;      //   value > 1 overrides the settle frames
    bool        autoload             = false;  // tools/resave_gate/resave_gate.sh (BANISHED_AUTOLOAD=Gate)
    const char* autoloadName         = nullptr;//   save name; null when the value was "1" (= the autosave "Auto")
    bool        autotutorial         = false;  // CLAUDE.md unattended-boot list
    int         autotutorialIndex    = 0;      //   scenario entry index; 0 when the value was "1"
    int         automenuButton       = 0;      // CLAUDE.md / STATUS.md front-end button routes (Mods=7, Load=2, …)
    const char* automenuClicks       = nullptr;// CLAUDE.md follow-up click feed: <id>|d<id>|m<id>, comma separated

    // ── harness drivers: in-game pause menu (standalone/pausemenu_harness.cpp)
    int         pausemenu            = 0;      // tools/resave_gate, perf/mintsel, CLAUDE.md — open frame (clamped up to 600)
    int         pausemenuQuit        = 120;    // tools/resave_gate, CLAUDE.md — frames between open and the click
    int         pausemenuButton      = 6;      // CLAUDE.md / perf README — 6 = Quit Game, 3 = Save, 7 = Mods
    const char* pausemenuClicks      = nullptr;// CLAUDE.md follow-up click feed (tokens `w`, `p` too)

    // ── modal dialogs ────────────────────────────────────────────────────────
    bool        modalDriver          = false;  // standalone/modal_driver.cpp — presence arms it; 0 is a legal answer [no tool reader]
    int         modalDriverAnswer    = 0;      //   the answer it substitutes ("<answer>[,<ticks>]")
    int         modalDriverTicks     = 0;      //   nested ticks before answering; 0 = the site's default
    const char* modalDriverRaw       = nullptr;//   the raw value, for the [ModalDriver] banner
    bool        modalAnswer          = false;  // BANISHED_MODAL_ANSWER — CLAUDE.md Mods route (code/Game/Dialog/*)
    int         modalAnswerValue     = 0;

    // ── front end / save-load routes ─────────────────────────────────────────
    int         menuScene            = -1;     // tools/resave_gate/resave_gate.sh pins the menu scene (0..9); -1 = unpinned
    const char* saveTest             = nullptr;// tools/resave_gate/resave_gate.sh (value "roundtrip" = the in-process gate)
    int         saveTestRepeat       = 0;      // standalone/saveload_harness.cpp re-arm period [no tool reader]
    const char* userProfile          = nullptr;// %USERPROFILE% — where Documents\Banished\Save lives (not a BANISHED_* flag)

    // ── world setup (read in code/Game; those sites migrate in the code/ pass) ─
    const char* seed                 = nullptr;// tools/stress_run.sh, tools/dx11_state_diff, tools/trace_oracle, CLAUDE.md
    const char* mapSize              = nullptr;// tools/stress_run.sh, perf/README.md, CLAUDE.md — small|medium|large
    const char* climate              = nullptr;// perf/README.md, CLAUDE.md — mild|fair|harsh
    const char* difficulty           = nullptr;// perf/README.md stress command line
    const char* startCondition       = nullptr;// documented world-setup lever (ENDGAME C.3 tier 3)
    const char* cameraPin            = nullptr;// CLAUDE.md "Camera Pinning" — x,y[,dist] or town[,dist]
    const char* forceSpeed           = nullptr;// perf/README.md — game speed override
    const char* simprof              = nullptr;// perf/README.md — SIMPROF phase-split interval

    // ── founding-town flags the stress town needs (read in code/Game) ────────
    const char* buildDemoFishingDock = nullptr;// perf/README.md stress town
    const char* buildDemoTradingPost = nullptr;// perf/README.md stress town
    const char* buildDemoCropField   = nullptr;// perf/README.md stress town
    const char* buildDemoOrchard     = nullptr;// perf/README.md stress town
    const char* buildDemoPasture     = nullptr;// perf/README.md stress town
    const char* buildDemoHunterLodge = nullptr;// perf/README.md stress town
    const char* buildDemoGathererHut = nullptr;// perf/README.md stress town
    const char* pastureAutoType      = nullptr;// perf/README.md stress town — pick a herd type for every pasture
    const char* cropFieldForceStart  = nullptr;// perf/README.md — "2" = pick a crop AND post the plant command
    const char* orchardForceStart    = nullptr;// perf/README.md — pick a tree type for every orchard
    bool        forceConstruct       = false;  // code/Game/Game/construction_force_harness.cpp [no tool reader]
    bool        buildSyncLabor       = true;   //   default ON; only "=0" disables

    // ── perf: the stress town (standalone/stress_harness.cpp, perf/README.md) ─
    bool        stress               = false;  // tools/stress_run.sh / make sa-stress
    int         stressCitizens       = 800;    // STRESS_ARGS --citizens
    int         stressSets           = 3;      // STRESS_ARGS --sets
    int         stressFrames         = 6000;   // STRESS_ARGS --frames
    int         stressInterval       = 600;    // CSV row interval (clamped to >= 60)
    int         stressSpeed          = 10;     // STRESS_ARGS --speed
    int         stressFoodMult       = 0;      // 0 = derive from the citizen count
    int         stressStaff          = 1;      // staff every workplace through the real seat cascade
    int         stressCompProf       = 1;      // 0 off, 1 bucket Updates, 2 + per-frame slot-8 ticks (perf/README.md)
    const char* stressCsv            = nullptr;// CSV path override; null = standalone_stress.csv
    bool        stressHomeWatch      = false;  // perf/README.md — the opt-in [HomeLoss] per-Update watch

    // ── perf: profiling and frame pacing ─────────────────────────────────────
    int         sample               = 0;      // tools/sample_report.py, perf/README.md — sampling profiler Hz; 0 = off
    const char* fpsCap               = nullptr;// perf/README.md — "off"/"false"/0 disables the limiter, else a target FPS
    const char* fpsHeadroom          = nullptr;// standalone/frame_limiter.cpp — % above the refresh rate [no tool reader]
    bool        perfTerrainVerify    = false;  // perf/README.md — check every sidecar terrain read against the real byte

    // ── perf: the experimental knobs (standalone/perf_experimental.cpp) ───────
    // One row each in perf/README.md. The master's default is the build's
    // -DBANISHED_PERF_EXPERIMENTAL; the rest default on under the master.
    bool        perfExperimental     = false;
    bool        perfAnimCull         = false;
    bool        perfGeomXfLastOnly   = false;
    bool        perfIconLastOnly     = false;
    int         perfAmbientThrottle  = 0;
    bool        perfTreeSnapshot     = false;
    bool        perfParticleCull     = false;
    bool        perfPickCull         = false;
    bool        perfGeomCull         = false;
    bool        perfIconCull         = false;
    bool        perfAnimBatchCulled  = false;
    bool        perfAnimBatchAll     = false;
    bool        perfTerrainSidecar   = false;
    float       perfCullMargin       = 6.0f;
    bool        perfTrace            = false;
    int         perfBuildDefault     = 0;      //   what -DBANISHED_PERF_EXPERIMENTAL compiled in (for the banner)

    // ── heap: the redzone allocator / watchdog (standalone/redzone_alloc.cpp) ─
    const char* redzone              = nullptr;// perf/README.md — "watch" = watchdog only, other truthy = full shim
    bool        redzoneAbort         = true;   //   "=0" keeps running past the first corruption
    int         redzoneSelfTestMs    = 0;      //   clobber one tag after N ms (self-test of the instrument)
    int         redzoneWatchMs       = -1;     //   watchdog interval; -1 = leave the built-in default
    int         redzoneScan          = 0;      //   full-scan every N allocations; 0 = leave the default
    int         redzonePad           = 0;      //   guard bytes per side; 0 = leave the default
    bool        redzoneZero          = false;  //   zero every allocation
    bool        teardownHeapCheck    = false;  //   per-op heap check during teardown

    // ── boot / engine plumbing ───────────────────────────────────────────────
    bool        unattendedBoot       = false;  // standalone/main.cpp — any of AUTOSTART/AUTOQUIT/AUTOLOAD/
                                               //   AUTOTUTORIAL/AUTOMENU_BUTTON/PAUSEMENU_BUTTON says nobody is watching
    bool        deferWindowShow      = true;   // standalone/boot_window.cpp — default ON, "0"/"false" shows the window early
    bool        skipEngineLoader     = false;  // standalone/loader_loop_stubs.cpp [no tool reader]
    bool        syncEngineLoader     = true;   // standalone/loader_loop_stubs.cpp — default ON, "0"/"f" goes async
    bool        noFolLock            = false;  // standalone/res_load_lock.cpp — kill switch for the resource-load lock
    bool        unregCensus          = false;  // tools/unreg_full.py — census of unregistered addresses

    // ── opt-in LIVE_* gates deliberately NOT in the default-on table ──────────────
    // (ENDGAME C.2 decides these: graduate with the run that proves them, or delete.)
    bool        liveSeatShuffle      = false;  // standalone/reg_town_workplace_registry.cpp


    // ── code/Game readers migrated off getenv (ENDGAME C.2/C.4 close) ────────
    // RAW values, parsed at the reader exactly as the getenv call used to be.
    // Every one is [no tool reader] unless the comment says otherwise.
    const char* buildDemoForce       = nullptr;// BANISHED_BUILD_DEMO_FORCE — code/Game/Component/buildcomponent_state8.cpp
    const char* buildDemoFrames      = nullptr;// BANISHED_BUILD_DEMO_FRAMES — buildcomponent_state8.cpp, construction_force_harness.cpp
    const char* buildDemoIndex       = nullptr;// BANISHED_BUILD_DEMO_INDEX — code/Game/Game/starttown.cpp
    const char* buildDemoHouse       = nullptr;// BANISHED_BUILD_DEMO_HOUSE — code/Game/Game/starttown.cpp
    const char* buildDemoForesterLodge = nullptr;// starttown.cpp ringPlace
    const char* buildDemoMarket      = nullptr;// starttown.cpp ringPlace
    const char* buildDemoSchoolHouse = nullptr;// starttown.cpp ringPlace
    const char* buildDemoTavern      = nullptr;// starttown.cpp ringPlace
    const char* buildDemoWell        = nullptr;// starttown.cpp ringPlace
    const char* buildDemoHospital    = nullptr;// starttown.cpp ringPlace (Template\DoctorHouse.rsc)
    const char* buildDemoCemetery    = nullptr;// starttown.cpp ringPlace
    const char* buildDemoChapel      = nullptr;// starttown.cpp ringPlace
    const char* buildDemoQuarry      = nullptr;// starttown.cpp ringPlace — CLAUDE.md camera-pin example
    const char* stressAllowPockets   = nullptr;// BANISHED_STRESS_ALLOW_POCKETS — starttown.cpp siting
    const char* stressKindMap        = nullptr;// BANISHED_STRESS_KINDMAP — starttown.cpp siting dump
    const char* forceWork            = nullptr;// BANISHED_FORCE_WORK — citizenaicomponent.cpp
    const char* forceHarvest         = nullptr;// BANISHED_FORCE_HARVEST — code/Game/Work/foresterwork.cpp
    const char* forceForesterWork    = nullptr;// BANISHED_FORCE_FORESTERWORK — forester_force_harness.cpp
    const char* forceSnow            = nullptr;// BANISHED_FORCE_SNOW — gameupdateloop.cpp
    const char* forceDeviceLost      = nullptr;// BANISHED_FORCE_DEVICE_LOST — dx11/src/contextdata.cpp
    const char* screenshotAt         = nullptr;// BANISHED_SCREENSHOT_AT — gameupdateloop.cpp
    const char* captureIngame        = nullptr;// BANISHED_CAPTURE_INGAME — gameupdateloop.cpp
    const char* achIgnoreMods        = nullptr;// BANISHED_ACH_IGNORE_MODS — gameupdateloop.cpp achievements
    const char* factoryEnum          = nullptr;// BANISHED_FACTORY_ENUM — factory_registrations_bulk.cpp
    const char* fastTrig             = nullptr;// BANISHED_FAST_TRIG — code/Game/Component/character_trig.cpp
    const char* treeGuards           = nullptr;// BANISHED_TREE_GUARDS — code/Game/Entity/entitylist.cpp
    const char* treePrefetch         = nullptr;// BANISHED_TREE_PREFETCH — code/Game/Entity/entitylist.cpp
    int         unregCensusFrames    = 0;      //   BANISHED_UNREG_CENSUS=<n> dump frame (gameupdateloop.cpp)

    // ── diagnostic traces still read from code/Game ──────────────────────────
    const char* mapTrace             = nullptr;// BANISHED_MAP_TRACE — minimap / map UI / tool UI
    const char* movetickTrace        = nullptr;// BANISHED_MOVETICK_TRACE — charactercomponent.cpp, citizenai_executor.cpp
    const char* plagueTrace          = nullptr;// BANISHED_PLAGUE_TRACE — citizencomponent.cpp, citizen_updatehealth.cpp
    const char* cropDiseaseTrace     = nullptr;// BANISHED_CROPDISEASE_TRACE — diseasecomponent.cpp
    const char* tornadoTrace         = nullptr;// BANISHED_TORNADO_TRACE — tornadocomponent.cpp
    const char* serializeTrace       = nullptr;// BANISHED_SERIALIZE_TRACE — componententity.cpp
    const char* selectDiag           = nullptr;// BANISHED_SELECT_DIAG — code/Game/Player/edittool.cpp

    // ── diagnostics: allocator / memory ──────────────────────────────────────
    bool        allocStats           = false;  // standalone/alloc_stats.cpp [no tool reader]
    uint64_t    allocBig             = 0;      //   report allocations at or above this size

    // ── diagnostics: censuses and checkers ───────────────────────────────────
    const char* achTimeScale         = nullptr;// standalone/ach_clock.cpp [no tool reader]
    const char* achYearBoost         = nullptr;// standalone/ach_clock.cpp [no tool reader]
    const char* achYearMax           = nullptr;// standalone/ach_clock.cpp [no tool reader]
    bool        compValidate         = false;  // standalone/entity_comp_validator.cpp [no tool reader]
    bool        sceneWatch           = false;  // standalone/scene_watch.cpp [no tool reader]
    bool        sceneAddTally        = false;  // standalone/scene_watch.cpp [no tool reader]
    bool        sceneCensus          = false;  // standalone/scene_watch.cpp [no tool reader]
    bool        forceLod0            = false;  // standalone/scene_watch.cpp [no tool reader]
    bool        lodCensus            = false;  // standalone/scene_watch.cpp [no tool reader]
    bool        citizenVitals        = true;   // standalone/citizen_vitals.cpp — default ON, "0" disables [no tool reader]

    // ── gameplay harnesses: RAW values, parsed at the harness ────────────────
    // Every one is `[no tool reader]` except BANISHED_PANEL_HOLD
    // (tools/capture_window.ps1) and BANISHED_KILL_AT_FRAME (named in
    // tools/game_dir_lock.sh's walk-away advice). Kept as one block so ENDGAME
    // C.4 can retire the gameplay harnesses together.
    const char* builders             = nullptr;// BANISHED_BUILDERS=<n>[,<settleFrame>]
    const char* workValidateTrace    = nullptr;// BANISHED_WORKVALIDATE_TRACE — roster dump for the ValidateTask crash
    const char* finishBuilds         = nullptr;// BANISHED_FINISH_BUILDS — stamp pending state 2 every n frames
    const char* autoStaff            = nullptr;// BANISHED_AUTOSTAFF — interval; unset lets the driver decide
    const char* autoStaffTrace       = nullptr;// BANISHED_AUTOSTAFF_TRACE
    const char* autoStaffUnseat      = nullptr;// BANISHED_AUTOSTAFF_UNSEAT — soak the workerCountDown click
    const char* killCitizens         = nullptr;// BANISHED_KILL_CITIZENS
    const char* killAtFrame          = nullptr;// BANISHED_KILL_AT_FRAME — tools/game_dir_lock.sh names it
    const char* killWhenCemeteryReady= nullptr;// BANISHED_KILL_WHEN_CEMETERY_READY
    const char* familyCensus         = nullptr;// BANISHED_FAMILY_CENSUS
    const char* killFamily           = nullptr;// BANISHED_KILL_FAMILY
    const char* marryHarness         = nullptr;// BANISHED_MARRY_HARNESS
    const char* birthTrace           = nullptr;// BANISHED_BIRTH_TRACE
    const char* forceBirth           = nullptr;// BANISHED_FORCE_BIRTH
    const char* forceBirthFrame      = nullptr;// BANISHED_FORCE_BIRTH_FRAME
    const char* foodPanel            = nullptr;// BANISHED_FOOD_PANEL — which food panel to open
    const char* foodPanelFrame       = nullptr;// BANISHED_FOOD_PANEL_FRAME
    const char* foodPanelClose       = nullptr;// BANISHED_FOOD_PANEL_CLOSE ("2" = close through the active tool)
    const char* foodPanelClick       = nullptr;// BANISHED_FOOD_PANEL_CLICK — fire the panel's command element
    const char* foodPanelSeat        = nullptr;// BANISHED_FOOD_PANEL_SEAT — seat a worker from the panel
    const char* foodPanelArmBuildTool= nullptr;// BANISHED_FOOD_PANEL_ARM_BUILDTOOL
    const char* foodPlace            = nullptr;// BANISHED_FOOD_PLACE
    const char* foodPlaceFrame       = nullptr;// BANISHED_FOOD_PLACE_FRAME
    const char* foodDiag             = nullptr;// BANISHED_FOOD_DIAG — dump period in frames
    const char* foodStock            = nullptr;// BANISHED_FOOD_STOCK — census period in frames
    const char* foodWorkers          = nullptr;// BANISHED_FOOD_WORKERS
    const char* marketOverstock      = nullptr;// BANISHED_MARKET_OVERSTOCK — percentage
    const char* marketOverstockFrame = nullptr;// BANISHED_MARKET_OVERSTOCK_FRAME
    const char* marketOverstockPeriod= nullptr;// BANISHED_MARKET_OVERSTOCK_PERIOD
    const char* orchardCut           = nullptr;// BANISHED_ORCHARD_CUT
    const char* orchardDiag          = nullptr;// BANISHED_ORCHARD_DIAG
    const char* orchardGrow          = nullptr;// BANISHED_ORCHARD_GROW (a growth multiplier)
    const char* orchardWorkers       = nullptr;// BANISHED_ORCHARD_WORKERS
    const char* pastureTend          = nullptr;// BANISHED_PASTURE_TEND — interval
    const char* pastureProduce       = nullptr;// BANISHED_PASTURE_PRODUCE — interval
    const char* pastureFill          = nullptr;// BANISHED_PASTURE_FILL — first frame
    const char* tradeSell            = nullptr;// BANISHED_TRADE_SELL — interval
    const char* tradeSellPerRow      = nullptr;// BANISHED_TRADE_SELL_PERROW
    const char* tradeSellSeed        = nullptr;// BANISHED_TRADE_SELL_SEED
    const char* panelHold            = nullptr;// BANISHED_PANEL_HOLD — tools/capture_window.ps1 opens one building's panel
    const char* panelHoldFrame       = nullptr;// BANISHED_PANEL_HOLD_FRAME
    const char* panelHoldClick       = nullptr;// BANISHED_PANEL_HOLD_CLICK — "<control>:<id>"
    const char* panelHoldClickEvery  = nullptr;// BANISHED_PANEL_HOLD_CLICK_EVERY
    const char* dlgCtrlRoute         = nullptr;// BANISHED_DLGCTRL_ROUTE ("probe" = observe only)

    // ── the mouse injector (standalone/mouse_inject.cpp) [no tool reader] ─────
    const char* mouseInject          = nullptr;// comma-separated step script
    const char* mouseHold            = nullptr;
    const char* mouseStart           = nullptr;
    const char* mouseTrace           = nullptr;// presence alone arms the trace
    const char* mouseTipStr          = nullptr;
    const char* mouseControl         = nullptr;// none|nowrite|badscale|offtarget|notip
    const char* mouseTarget          = nullptr;// #name | @x,y | an element id

    // ── the tool/placement harness (standalone/tool_harness.cpp) [no tool reader]
    const char* toolHarness          = nullptr;
    const char* placementTemplate    = nullptr;
    const char* placementCell        = nullptr;
    const char* placementRadius      = nullptr;
    const char* roadTemplate         = nullptr;
    const char* roadRuns             = nullptr;
    const char* roadLen              = nullptr;
    const char* pastureOverNaturals  = nullptr;
    const char* fencedRuns           = nullptr;
    const char* fencedSide           = nullptr;
    const char* utilityTemplate      = nullptr;
    const char* mapTestPattern       = nullptr;
    const char* dlgSettingsRoute     = nullptr;
    const char* housePanelIndex      = nullptr;
    const char* uiCompCensusShow     = nullptr;
    const char* pasturePanelSelect   = nullptr;
    const char* orchardPanelSelect   = nullptr;
    const char* upgradeWaitFrames    = nullptr;
    const char* upgradeForceFrame    = nullptr;
    const char* upgradeClickDelay    = nullptr;
    const char* bridgeDragFrames     = nullptr;
    const char* bridgePoll           = nullptr;
    const char* bridgeWaitFrames     = nullptr;
    const char* bridgeTemplate       = nullptr;
    const char* bridgeLongest        = nullptr;
    const char* bridgeSelect         = nullptr;
    const char* pastureFullDelay     = nullptr;

    // ── the disaster harness (standalone/disaster_harness.cpp) [no tool reader]
    // Four arms, indexed FIRE=0, TORNADO=1, SICK=2, INFEST=3:
    // BANISHED_FORCE_<ARM>[_FRAME|_PERIOD], plus BANISHED_FORCE_SICK_DISEASE.
    const char* disasterCount[4]     = {nullptr, nullptr, nullptr, nullptr};
    const char* disasterFrame[4]     = {nullptr, nullptr, nullptr, nullptr};
    const char* disasterPeriod[4]    = {nullptr, nullptr, nullptr, nullptr};
    const char* forceSickDisease     = nullptr;

    // ── NOT read by the exe ──────────────────────────────────────────────────
    // BANISHED_DIR      — Makefile only (where sa-deploy copies the exe).
    // BANISHED_GAME_DIR — tools/game_dir_lock.sh only (the shared game directory).
    // BANISHED_STRIP_LOGS is not an environment flag at all: it is the
    // -DBANISHED_STRIP_LOGS compile define (see standalone/log_strip.h).
};

// Fill the struct from the environment. Call ONCE, first thing in WinMain.
void OptionsInit();

// The options for this run. Valid from OptionsInit() onward; before that it
// reads as the defaults above.
const Options& Opt();
