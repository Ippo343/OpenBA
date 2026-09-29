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

// `GetCommandLineString(cfg, &ret, &key)` — THREE arguments. The hidden return
// slot is RDX and the key is R8 (0x1800a16d0-0x1800a16df). An fn_* typedef
// that omits the return slot is a wild write that still returns the right
// answer, which is why this is spelled out rather than guessed.
bool CommandLineHasValue(const System::Configuration* cfg, const char* keyText) {
    if (cfg == nullptr) return false;
    String key(keyText);       // 0x18000b630: zero, then Set
    // System::Configuration::GetCommandLineString (0x1800c5300), by name (B3R);
    // the by-value result is constructed in `value`.
    const String value = cfg->GetCommandLineString(key);
    const bool present = value._data != nullptr;   // retail: CMP [RAX],RDI
    // `value` then `key` release here — retail's order, which is C++'s reverse
    // declaration order.
    return present;
}

// Retail's OTHER command-line test: the map find at `Configuration + 0x10`.
// Unexported in both shipped builds; the body is
// runtime/src/system/configuration.cpp's RtSystem_ConfigMap_Find (0x18002ace0),
// called by name (B3R).
bool CommandLineHasKey(System::Configuration* cfg, const char* keyText) {
    if (cfg == nullptr) return false;
    String key(keyText);       // 0x18000b630: zero, then Set
    // The map sub-object at +0x10 starts with `_mapRoot`. LEFT: the find takes
    // the RtSystem::String POD (configuration.cpp, R-CORE).
    const bool found = RtSystem_ConfigMap_Find(
        &cfg->_mapRoot, reinterpret_cast<const RtSystem::String*>(&key)) != nullptr;
    // `key` releases here — retail's 0x1800a1934, now ~String.
    return found;
}

// 0x1800a1875-0x1800a18ba. Note it re-reads the ENGINE SINGLETON rather than
// using `this` — transcribed as written, not "simplified" to self.
void DisableAllModifications() {
    EngineT* engine = ::Engine::Engine::_singleton;
    if (engine == nullptr) return;
    IO::Modification* modification = engine->_modification;
    if (modification == nullptr) return;

    const int32_t count = modification->_count;          // +0x10
    RtIO::Mod* base = modification->_mods;               // +0x08
    for (int32_t i = 0; i < count; ++i)
        base[i].enabled = 0;                             // Mod +0x50, stride 0x80

    modification->_dirty = 1;   // the "list is dirty" byte SaveList reads
    modification->SaveList();
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
    // LEFT: the Set helper takes the rtsetavl::Set POD over the Persistence
    // object's settings map (Persistence.h / Collection.h, not this block's).
    if (void* ps = ::IO::Persistence::_singleton) {
        rtsetclear::Clear(static_cast<rtsetavl::Set*>(ps));       // 0x18000f7d0
    }

    // Each Configuration below is freed without its destructor and rebuilt with
    // its constructor's stores inlined (retail's merged qword / word stores are
    // written as the named fields they cover, in address order).

    // --- Input::Configuration (0x10 bytes) ---------------------------------
    if (self->_inputConfiguration) {
        ::Input::Configuration::_singleton = nullptr;
        MemFree(self->_inputConfiguration);
    }
    {
        ::Input::Configuration* p =
            static_cast< ::Input::Configuration*>(MemAlloc(0x10, 4));
        if (p) {
            p->_keyMap        = nullptr;                  // +0x00
            p->_mouseClipping = false;                    // +0x08
            p->_legacy        = false;                    // +0x09
            p->_mouseDisplay  = false;                    // +0x0a
            ::Input::Configuration::_singleton = p;
        }
        self->_inputConfiguration = p;
        p->Startup();
    }

    // --- Video::Configuration (0x44 bytes) ---------------------------------
    if (self->_videoConfiguration) {
        ::Video::Configuration::_singleton = nullptr;
        MemFree(self->_videoConfiguration);
    }
    {
        Video::Configuration* p = static_cast<Video::Configuration*>(MemAlloc(0x44, 4));
        if (p) {
            p->settingsValid      = 1;                    // the arming bit
            p->interfaceIndex     = 1;                    // +0x04
            p->adapterIndex       = 0;                    // +0x08
            p->width              = -1;                   // +0x0c
            p->height             = -1;                   // +0x10
            p->windowX            = -1;                   // +0x14
            p->windowY            = -1;                   // +0x18
            p->windowWidth        = -1;                   // +0x1c
            p->windowHeight       = -1;                   // +0x20
            p->multisample        = 1;                    // +0x24
            p->textureFilter      = 0;                    // +0x28
            p->shadowScalar       = 1;                    // +0x2c
            p->shaderDetail       = 2;                    // +0x30
            p->reflectionDetail   = 1;                    // +0x34
            p->refreshNumerator   = 0x3c;                 // +0x38
            p->refreshDenominator = 1;                    // +0x3c
            p->fullscreen         = 1;                    // +0x40
            p->vsync              = 1;                    // +0x41
            p->field42            = 0;                    // +0x42
            ::Video::Configuration::_singleton = p;
        }
        self->_videoConfiguration = p;
        p->Startup();
    }

    // --- Audio::Configuration (0x10 bytes) ---------------------------------
    if (self->_audioConfiguration) {
        ::Audio::Configuration::_singleton = nullptr;
        MemFree(self->_audioConfiguration);
    }
    {
        Audio::Configuration* p = static_cast<Audio::Configuration*>(MemAlloc(0x10, 4));
        if (p) {
            p->uiVolume      = 100;                       // +0x00
            p->musicVolume   = 0x32;                      // +0x04
            p->ambientVolume = 100;                       // +0x08
            p->effectVolume  = 100;                       // +0x0c
            ::Audio::Configuration::_singleton = p;
        }
        self->_audioConfiguration = p;
        p->Startup();
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
        Video::Configuration* vc = _videoConfiguration;
        if (vc == nullptr) return true;
        vc->settingsValid = (_startupComplete == 0) ? 1 : 0;
        return true;
    }

    // ---- 0x1800a16a9: decide whether the dialog is shown at all ------------
    // Retail derefs `_videoConfiguration` UNGUARDED on this path
    // (`MOV RAX,[RCX+0x6b8] ; CMP byte ptr [RAX],BL`). Left faithful: the
    // caller is Engine::Startup, which constructed it eight steps earlier, and
    // a guard here would skip the behaviour rather than restore it.
    bool skipDialog = false;
    if (_videoConfiguration->settingsValid == 0) {
        const bool forced = CommandLineHasValue(_configuration, "launcher")
                         || RtEnginePlatformShiftHeld();
        skipDialog = !forced;
    }

    if (skipDialog) {
        // 0x1800a1787: arm the bit, persist, and tell the caller to proceed.
        ++g_skippedDlg;
        _videoConfiguration->settingsValid = 1;
        _videoConfiguration->Save();
        _persistence->WriteSettings();                        // 0x1800b88e0
        return true;
    }

    // ---- 0x1800a17ae: HALF B ----------------------------------------------
    ++g_shownDlg;
    {
        // THE DOOR DECIDES THE OVERLOAD: the no-argument `_NXZ` spelling
        // (0x1800f8830), not the eight-int-reference `XAEAH0000000@Z` one.
        Video::Device* device = ::Video::Device::_singleton;
        device->ValidateConfiguration();
    }

    const long long result = RtEnginePlatformShowLauncherDialog();

    _videoConfiguration->settingsValid = 1;
    _videoConfiguration->Save();
    _persistence->WriteSettings();                            // 0x1800b88e0

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
