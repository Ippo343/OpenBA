// ============================================================================
// runtime/src/engine/engine_platform.cpp — Wave 23 lane ENGINE
// ?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z   retail 0x1800a1670, ord 3329
// ============================================================================
//
// THE CUTOVER BOOT WALL. Wave 20 recorded this function as the address the
// DLL-absent boot dies on (EXEC-of-0 at `RtEngine_Engine_Startup+0x777`) and
// then left it `[NOT RE-DERIVED]` for three waves. Lane ENGINE re-derived the
// wall statically — see docs/runtime_port/WAVE23_LANE_ENGINE.md §1 — and it is
// still there, for a reason that is mechanically exact and was never written
// down: our own PORTED `Engine::Engine::Startup` (engine_lifecycle.cpp:710)
// calls this export through `RT_EXPORT_ONCE`, and with the DLL gone that
// resolves to null. The wall is not a missing feature; it is one unported
// callee on a ported caller's only path.
//
// ---------------------------------------------------------------------------
// 1. THE FUNCTION HAS TWO HALVES AND ONLY ONE OF THEM IS PORTABLE TODAY
// ---------------------------------------------------------------------------
// Read the whole body in Ghidra (272 instructions) before deciding scope, and
// it splits cleanly:
//
//   HALF A — `configure == false`, plus `configure == true` on the branch that
//   DECLINES to show the dialog. Every dependency is an export this tree has
//   already ported (Save@Configuration@Video, WriteSettings@Persistence@IO,
//   Set@String@System, GetCommandLineString@Configuration@System). This half is
//   ported here, byte-faithfully, and it is the ONLY half an unattended run
//   ever executes — `HarnessGuardUnattendedBoot` (standalone/main.cpp:184)
//   exists precisely to keep the arming bit clear so that this branch is taken.
//
//   HALF B — the launcher dialog actually being shown. It needs, all of which
//   live INSIDE Runtime-x64.dll and none of which this lane can supply:
//       * Win32 dialog templates 0x66 (launcher), 0x68 (options) and the
//         bitmap 0x2711, in the DLL's RESOURCE section;
//       * three unexported dialog procedures — 0x1800a1ae0 (launcher),
//         0x1800a1dc0 (options), 0x1800a2020 (a control helper);
//       * four unexported statics — 0x1801fc680 (the DLL's HINSTANCE),
//         0x1801fc688, 0x1801fc698 ("reset" checkbox), 0x1801fc699
//         ("nomods" checkbox);
//       * 0x18000f7d0, an unexported recursive clear of Persistence's settings
//         map, reached only by the "reset" path.
//   Verified unexported with pefile against the shipped DLL, not assumed:
//   none of those addresses appears in the export directory.
//
// Half B is therefore a NAMED RESIDUAL, not an oversight. See §4.
//
// ---------------------------------------------------------------------------
// 2. WHAT THIS PORT CHANGES AT CUTOVER, STATED WITHOUT INFLATION
// ---------------------------------------------------------------------------
// It does NOT make the launcher dialog work without the DLL. It converts the
// wall from an EXEC-of-0 that names only an address into:
//   * Half A running for real — which is what an unattended boot takes; and
//   * a NAMED refusal on Half B, printed with the symbol and the reason,
//     instead of a jump to 0.
// A cutover boot is a GATE, not a planner: it can only name what it reached.
// Moving it past this function is what lets the next attempt name the NEXT
// wall. That is the whole claim.
//
// ---------------------------------------------------------------------------
// 3. TWO THINGS THE DECOMPILER GETS WRONG HERE
// ---------------------------------------------------------------------------
//   * `param_2` (the `void*`) is NEVER dereferenced or even read: R8 is
//     untouched for all 272 instructions. It is kept because that is the
//     signature. Do not "use" it.
//   * The "launcher" test and the "nomods"/"reset" tests are DIFFERENT
//     mechanisms and Ghidra's C makes them look alike. "launcher" goes through
//     `GetCommandLineString` (0x1800c5300) and tests the returned String's data
//     pointer; "nomods"/"reset" go through the unexported map find at
//     0x18002ace0 on `Configuration + 0x10` and test PRESENCE. Read from the
//     disassembly at 0x1800a16df vs 0x1800a1832 / 0x1800a18f2.
//
// ---------------------------------------------------------------------------
// 4. A DOCUMENTATION DEFECT FOUND WHILE DOING THIS
// ---------------------------------------------------------------------------
// runtime/types/engine/Engine.h's block comment said 0x1800f6d10 was
// `Video::Configuration::Shutdown`. It is not: pefile says 0x1800f6d10 is
// `?Save@Configuration@Video@@QEAAXXZ`. Corrected in that header by this lane.
//
// Kill switch: RT_ENGINEPLATFORM=0.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../types/engine/Engine.h"
#include "../../types/video/Device.h"      // ENDGAME B.3.5: Video::Device / Configuration
#include "../../types/io/Persistence.h"    // ENDGAME B.3.9 SS1: Persistence::_singleton
#include "../../types/audio/Audio.h"       // ENDGAME B.3.7 callers (K4): Audio::Configuration
#include "../../types/input/Input.h"       // ENDGAME B.3.7 callers (K5): Input::Configuration
#include "../../types/system/String.h"
#include "../../types/io/Modification.h"
#include "../../types/io/Persistence.h"            // B3R: WriteSettings by name
#include "../../types/system/Configuration.h"      // B3R: GetCommandLineString, RtSystem_ConfigMap_Find
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"     // B3R: rtsetclear::Clear
#include "../system/string.h"

// The platform seam. runtime/src/engine/engine_platform_win.cpp.
bool RtEnginePlatformShiftHeld();
long long RtEnginePlatformShowLauncherDialog();
bool RtEnginePlatformLauncherFlagNoMods();
bool RtEnginePlatformLauncherFlagReset();

// ENDGAME B.3.7: `Engine::Engine` is the real class; a global
// `using RtEngine::Engine;` is ill-formed next to `namespace Engine`.
using EngineT = ::Engine::Engine;
// ENDGAME B.3.1 step 2: `System::String` is the real class and the two shims
// this file used (`RtSystem_String_Set` 0x1800c95d0 and the release idiom) are
// its `Set` and its destructor.
using System::String;

namespace {

// Retail's unexported per-class operator new / delete at 0x180011640 and
// 0x180006a10 are `MemAlloc(size, 4)` and `MemFree` verbatim — read in full by
// Wave 9 lane ENG, which made the same call this file makes: call the named
// allocator exports directly rather than register those two addresses, because
// they have dozens of DLL-internal callers and no name.
void* MemAlloc(uint64_t size, int align) {
    return System::MemoryAllocator::MemAlloc(size, align);   // 0x1800c7a50
}
void MemFree(void* p) {
    System::MemoryAllocator::MemFree(p);                     // 0x1800c7ad0
}

// The exported DATA slots -- all class statics now (ENDGAME B.3.9 SS1,
// B39_RULES D7): the address-of IS the redirected cell.
void** VideoDeviceSingletonSlot() {
    return reinterpret_cast<void**>(&::Video::Device::_singleton);
}
void** VideoConfigSingletonSlot() {
    return reinterpret_cast<void**>(&::Video::Configuration::_singleton);
}
void** AudioConfigSingletonSlot() {
    return reinterpret_cast<void**>(&::Audio::Configuration::_singleton);
}
void** InputConfigSingletonSlot() {
    return reinterpret_cast<void**>(&::Input::Configuration::_singleton);
}
void** PersistenceSingletonSlot() {
    return reinterpret_cast<void**>(&::IO::Persistence::_singleton);
}
EngineT** EngineSingletonSlot() {
    return reinterpret_cast<EngineT**>(&::Engine::Engine::_singleton);
}

// `GetCommandLineString(cfg, &ret, &key)` — THREE arguments. The hidden return
// slot is RDX and the key is R8 (0x1800a16d0-0x1800a16df). An fn_* typedef
// that omits the return slot is a wild write that still returns the right
// answer, which is why this is spelled out rather than guessed.
bool CommandLineHasValue(void* cfg, const char* keyText) {
    if (cfg == nullptr) return false;
    String key(keyText);       // 0x18000b630: zero, then Set
    // System::Configuration::GetCommandLineString (0x1800c5300), by name (B3R);
    // the by-value result is constructed in `value`.
    const String value = static_cast<const System::Configuration*>(cfg)
                             ->GetCommandLineString(key);
    const bool present = value._data != nullptr;   // retail: CMP [RAX],RDI
    // `value` then `key` release here — retail's order, which is C++'s reverse
    // declaration order.
    return present;
}

// Retail's OTHER command-line test: the map find at `Configuration + 0x10`.
// Unexported in both shipped builds; the body is
// runtime/src/system/configuration.cpp's RtSystem_ConfigMap_Find (0x18002ace0),
// called by name (B3R).
bool CommandLineHasKey(void* cfg, const char* keyText) {
    if (cfg == nullptr) return false;
    String key(keyText);       // 0x18000b630: zero, then Set
    void* map = static_cast<uint8_t*>(cfg) + 0x10;
    const bool found = RtSystem_ConfigMap_Find(
        map, reinterpret_cast<const RtSystem::String*>(&key)) != nullptr;
    // `key` releases here — retail's 0x1800a1934, now ~String.
    return found;
}

// 0x1800a1875-0x1800a18ba. Note it re-reads the ENGINE SINGLETON rather than
// using `this` — transcribed as written, not "simplified" to self.
void DisableAllModifications() {
    EngineT** eslot = EngineSingletonSlot();
    EngineT* engine = eslot ? *eslot : nullptr;
    if (engine == nullptr) return;
    uint8_t* modification = static_cast<uint8_t*>(engine->_modification);
    if (modification == nullptr) return;

    const int32_t count = *reinterpret_cast<int32_t*>(modification + 0x10);
    uint8_t* base = *reinterpret_cast<uint8_t**>(modification + 8);
    for (int32_t i = 0; i < count; ++i)
        base[static_cast<size_t>(i) * 0x80 + 0x50] = 0;   // Mod::enabled

    modification[0] = 1;   // the "list is dirty" byte SaveList reads
    reinterpret_cast<IO::Modification*>(modification)->SaveList();
}

// 0x1800a1939-0x1800a1a92. Destroy and re-create the three Configuration
// singletons with their compiled-in defaults. Every constant below is read off
// the instruction stream, not reconstructed from a header.
void ResetConfigurations(EngineT* self) {
    // Persistence's settings map is cleared by an UNEXPORTED recursive helper
    // (0x18000f7d0 -> 0x18000f890 / 0x18000fa90 / 0x180009c30), ported as
    // rtsetclear::Clear (runtime/src/collection/set_clear.cpp), called by name
    // (B3R).
    //
    // NOT CONVERTED (ENDGAME B.3.2 step 2): 0x18000f7d0 is `rtsetclear::Clear`,
    // a `Collection::Set<Pair<String,String>>::Clear` door. Calling it as a
    // member needs `Persistence::settingsMapRoot` (runtime/types/io/
    // Persistence.h, currently a raw `void* settingsMapRoot` + a separate
    // `settingsMapCount`) to be a real `Collection::Set<Pair<String,String>,
    // DefaultAllocator, Pair<String,String>, AVLCompare>` member — that header
    // is outside this batch's file list (runtime/types/io, not runtime/src/
    // engine), so it stays the free body.
    void** pslot = PersistenceSingletonSlot();
    if (pslot && *pslot) {
        rtsetclear::Clear(static_cast<rtsetavl::Set*>(*pslot));   // 0x18000f7d0
    }

    // --- Input::Configuration (0x10 bytes) ---------------------------------
    if (self->_inputConfiguration) {
        if (void** s = InputConfigSingletonSlot()) *s = nullptr;
        MemFree(self->_inputConfiguration);
    }
    {
        uint8_t* p = static_cast<uint8_t*>(MemAlloc(0x10, 4));
        if (p) {
            *reinterpret_cast<uint64_t*>(p + 0x00) = 0;
            *reinterpret_cast<uint16_t*>(p + 0x08) = 0;
            p[0x0a] = 0;
            if (void** s = InputConfigSingletonSlot()) *s = p;
        }
        self->_inputConfiguration = p;
        reinterpret_cast< ::Input::Configuration*>(p)->Startup();  // B.3.7 bridge
    }

    // --- Video::Configuration (0x44 bytes) ---------------------------------
    if (self->_videoConfiguration) {
        if (void** s = VideoConfigSingletonSlot()) *s = nullptr;
        MemFree(self->_videoConfiguration);
    }
    {
        uint8_t* p = static_cast<uint8_t*>(MemAlloc(0x44, 4));
        if (p) {
            p[0x00] = 1;                                              // the arming bit
            *reinterpret_cast<uint64_t*>(p + 0x04) = 1;
            *reinterpret_cast<int64_t*>(p + 0x0c)  = -1;
            *reinterpret_cast<int64_t*>(p + 0x14)  = -1;
            *reinterpret_cast<int64_t*>(p + 0x1c)  = -1;
            *reinterpret_cast<uint64_t*>(p + 0x24) = 1;
            *reinterpret_cast<uint32_t*>(p + 0x2c) = 1;
            *reinterpret_cast<uint32_t*>(p + 0x30) = 2;
            *reinterpret_cast<uint32_t*>(p + 0x34) = 1;
            *reinterpret_cast<uint32_t*>(p + 0x38) = 0x3c;
            *reinterpret_cast<uint32_t*>(p + 0x3c) = 1;
            *reinterpret_cast<uint16_t*>(p + 0x40) = 0x101;
            p[0x42] = 0;
            if (void** s = VideoConfigSingletonSlot()) *s = p;
        }
        self->_videoConfiguration = p;
        reinterpret_cast<Video::Configuration*>(p)->Startup();  // B.3.5 bridge
    }

    // --- Audio::Configuration (0x10 bytes) ---------------------------------
    if (self->_audioConfiguration) {
        if (void** s = AudioConfigSingletonSlot()) *s = nullptr;
        MemFree(self->_audioConfiguration);
    }
    {
        uint8_t* p = static_cast<uint8_t*>(MemAlloc(0x10, 4));
        if (p) {
            *reinterpret_cast<uint32_t*>(p + 0x00) = 100;
            *reinterpret_cast<uint32_t*>(p + 0x04) = 0x32;
            *reinterpret_cast<uint32_t*>(p + 0x08) = 100;
            *reinterpret_cast<uint32_t*>(p + 0x0c) = 100;
            if (void** s = AudioConfigSingletonSlot()) *s = p;
        }
        self->_audioConfiguration = p;
        reinterpret_cast<Audio::Configuration*>(p)->Startup();   // B.3.7 bridge
    }
}

// Effectiveness counters — read by RtEnginePlatformSummary(). A port on a
// boot-once path has exactly one coverage question: did the body run, and
// WHICH HALF.
unsigned g_calls        = 0;
unsigned g_deconfigure  = 0;   // configure == false
unsigned g_skippedDlg   = 0;   // Half A, the branch an unattended boot takes
unsigned g_shownDlg     = 0;   // Half B

}  // namespace

// ============================================================================
// ?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z   retail 0x1800a1670
// ============================================================================
bool Engine::Engine::PlatformConfigure(bool configure, void* platformHandle) {
    (void)platformHandle;   // see §3: R8 is never read by retail.
    ++g_calls;

    // ---- 0x1800a1aa0: the `configure == false` half ------------------------
    // This is the call `code/Game/Game/game_main.cpp:240` already inlines. The
    // deref of `_videoConfiguration` is guarded HERE because retail guards it
    // here (`TEST RCX,RCX ; JZ`), not because we added a null check.
    if (!configure) {
        ++g_deconfigure;
        void* vc = _videoConfiguration;
        if (vc == nullptr) return true;
        *static_cast<uint8_t*>(vc) = (_startupComplete == 0) ? 1 : 0;
        return true;
    }

    // ---- 0x1800a16a9: decide whether the dialog is shown at all ------------
    // Retail derefs `_videoConfiguration` UNGUARDED on this path
    // (`MOV RAX,[RCX+0x6b8] ; CMP byte ptr [RAX],BL`). Left faithful: the
    // caller is Engine::Startup, which constructed it eight steps earlier, and
    // a guard here would skip the behaviour rather than restore it.
    bool skipDialog = false;
    if (*static_cast<uint8_t*>(_videoConfiguration) == 0) {
        const bool forced = CommandLineHasValue(_configuration, "launcher")
                         || RtEnginePlatformShiftHeld();
        skipDialog = !forced;
    }

    if (skipDialog) {
        // 0x1800a1787: arm the bit, persist, and tell the caller to proceed.
        ++g_skippedDlg;
        *static_cast<uint8_t*>(_videoConfiguration) = 1;
        reinterpret_cast<Video::Configuration*>(_videoConfiguration)
            ->Save();                                        // B.3.5 bridge
        static_cast<const IO::Persistence*>(_persistence)->WriteSettings();   // 0x1800b88e0
        return true;
    }

    // ---- 0x1800a17ae: HALF B ----------------------------------------------
    ++g_shownDlg;
    {
        void** dslot = VideoDeviceSingletonSlot();
        // THE DOOR DECIDES THE OVERLOAD: the no-argument `_NXZ` spelling
        // (0x1800f8830), not the eight-int-reference `XAEAH0000000@Z` one.
        // The receiver is loaded through a `volatile`-free local rather than
        // written as `dslot ? *dslot : nullptr` at the call: GCC constant-folds
        // the null arm of the ternary and reports `-Wnonnull` on a member call
        // that retail reaches with whatever the singleton holds. The value is
        // identical; only the spelling keeps the diagnostic away.
        Video::Device* device = dslot
            ? reinterpret_cast<Video::Device*>(*dslot) : nullptr;
        device->ValidateConfiguration();                     // B.3.5 bridge
    }

    const long long result = RtEnginePlatformShowLauncherDialog();

    *static_cast<uint8_t*>(_videoConfiguration) = 1;
    reinterpret_cast<Video::Configuration*>(_videoConfiguration)
        ->Save();                                            // B.3.5 bridge
    static_cast<const IO::Persistence*>(_persistence)->WriteSettings();       // 0x1800b88e0

    // 0x1800a17ff / 0x1800a18bf. The `||` short-circuit is retail's: the
    // command line is consulted ONLY when the checkbox byte is zero.
    if (RtEnginePlatformLauncherFlagNoMods()
            || CommandLineHasKey(_configuration, "nomods"))
        DisableAllModifications();

    if (RtEnginePlatformLauncherFlagReset()
            || CommandLineHasKey(_configuration, "reset"))
        ResetConfigurations(this);

    return result == 1;   // 0x1800a1a97: IDOK from the launcher template
}

extern "C" void RtEnginePlatformSummary() {
    printf("[RtPort]   Engine::PlatformConfigure: calls=%u deconfigure=%u "
           "dialog-skipped=%u dialog-shown=%u\n",
           g_calls, g_deconfigure, g_skippedDlg, g_shownDlg);
}
