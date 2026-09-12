// ============================================================================
// standalone/matsetup_detour.cpp — Wave 24 lane GFXMAT
//   the detour that puts `?Setup@MaterialInstance@Graphics@@` on the DLL's OWN
//   render path.
// ============================================================================
//
// WHY THIS FILE EXISTS. Wave 23's handover item 13 says the row "needs a
// DETOUR, not a registration, to take retail's rel32 traffic — otherwise it and
// SetupShaders stay dead in play". The row itself has been PORTED and
// registered by NAME since Wave 21 (runtime/src/graphics/wave21_lane_gfxbody.cpp
// :349).
//
// ---------------------------------------------------------------------------
// THE INHERITED CLAIM WAS RE-CHECKED, AND IT IS HALF TRUE. THE HALF MATTERS.
// ---------------------------------------------------------------------------
// kickoff §5 — "re-check every stated blocker before acting on it". Measured
// here rather than quoted: a scan of the whole 1,349,120-byte `.text` of the
// shipped Runtime-x64.dll for `E8`/`E9` rel32 landing on 0x180102c90 finds
// EXACTLY TWO, and the scanner reproduces its positive control
// (`?Serialize@ElementDescription@UI@@`, documented at 21 internal callers)
// as exactly 21. The two are:
//
//   0x180105572   inside `Graphics::Renderer::DrawItemList` (0x180105240)
//   0x18011bb37   inside `UI::SpriteContext::Flush`         (0x18011ba40)
//
// `UI::SpriteContext::Flush` IS PORTED (runtime/src/ui/spritecontext_flush.cpp)
// and its port reaches Setup BY NAME (:267), so on any run where our Flush is
// live, one of retail's two callers ALREADY runs the port and the row is NOT
// dead. What is dead is the OTHER one — `Renderer::DrawItemList`, i.e. the
// every-frame 3D material bind, which is exactly the traffic the handover
// cared about and is unreachable by any registration.
//
// So the correct statement of the row is not "dead in play" but **"live on the
// UI sprite path, dead on the 3D render path"**, and this detour buys precisely
// the second one. Saying it the loose way would have made an ARMED run look
// like it had proved something it had not
// (reference_holdback_blockers_are_stale_check_them_first.md).
//
// ---------------------------------------------------------------------------
// OPT-IN, AND IT STAYS OPT-IN UNTIL SOMEONE WITH A RUN ARMS IT
// ---------------------------------------------------------------------------
// `BANISHED_RT_MATSETUP_DETOUR=1`. runtime_detour.h: "Every detour gets an env
// kill-switch at its call site... Nothing goes default-on unproven." This lane
// has NO game-directory allocation, so nothing here has ever executed inside a
// frame, and putting an unexecuted body on the every-frame material bind by
// default would be the opposite of that rule. With the var unset the DLL is not
// touched and the tree behaves exactly as every previous wave shipped.
//
// ---------------------------------------------------------------------------
// PROLOGUE, read out of the shipped DLL's FILE IMAGE (pefile), not off a
// mnemonic listing — this compiler emits redundant REX prefixes and a listing
// would have hidden the byte count:
//
//   180102c90  48 89 5C 24 08        MOV [RSP+0x08],RBX          (5)
//   180102c95  48 89 74 24 10        MOV [RSP+0x10],RSI          (5)
//   180102c9a  57                    PUSH RDI                    (1)
//   180102c9b  48 83 EC 20           SUB RSP,0x20                (4)
//   180102c9f  48 8B 41 28           MOV RAX,[RCX+0x28]   <- resumes here
//
// FIFTEEN bytes, an exact instruction boundary, and no RIP-relative operand, so
// the relocating installer has nothing to re-encode. The two `MOV [RSP+n]`
// stores ARE RSP-relative, and that is safe here for the reason it is safe in
// every other detour in this directory: the trampoline is entered by a CALL, so
// its `[RSP+8]`/`[RSP+0x10]` are the caller's home slots exactly as the
// original function's were. It is spelled out because an RSP-relative prologue
// is the one shape `elementdesc_detour.cpp` explicitly says it did NOT have to
// handle, so a reader coming from that file will be looking for this sentence.
//
// ---------------------------------------------------------------------------
// THE RECURSION HAZARD, AND WHY THIS ROW DOES NOT HAVE IT.
// `?Setup@MaterialInstance@` makes exactly ONE call, to
// `?SetupShaders@VideoProgram@Video@@` (0x1800f4520), which our port reaches by
// a DIFFERENT name. Nothing in the ported body resolves `?Setup@…` itself, so
// no trampoline door is needed and none is offered — deliberately, because an
// unused `…Trampoline()` accessor is a door a later reader assumes is load
// bearing. If a future oracle needs retail's body while the detour is in, it
// must add one; `RT_ORIGINAL_ONCE` would recurse until the stack dies.
//
// SAFETY RULE (kickoff): the installer must NOT resolve through the port
// registry and must NOT aim at our own prologue.
// `InstallRuntimeDetourChecked` resolves through `ResolveRuntimeRaw`
// (standalone/runtime_detour.cpp:685), which is the raw image, and the
// fifteen-byte expected-prologue check below refuses the patch if anything —
// a different build, a wrong ASLR delta, or ANOTHER DETOUR ALREADY THERE — has
// moved those bytes (reference_a_rule_on_the_callers_side_gets_broken.md is
// exactly this rule being broken 13 times from the caller's side, so it is
// enforced here on the callee's).
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uintptr_t kMatSetupAddr     = 0x180102c90;
constexpr size_t    kMatSetupPrologue = 15;

const uint8_t kMatSetupBytes[kMatSetupPrologue] = {
    0x48, 0x89, 0x5C, 0x24, 0x08,       // MOV [RSP+0x08],RBX
    0x48, 0x89, 0x74, 0x24, 0x10,       // MOV [RSP+0x10],RSI
    0x57,                               // PUSH RDI
    0x48, 0x83, 0xEC, 0x20              // SUB RSP,0x20
};

RtDetour g_detour = {};
bool     g_installed = false;

}  // namespace

extern "C" bool RtGraphicsMatSetupInstallDetour(void* ourFn) {
    if (g_installed) return true;
    if (!ourFn) return false;

    const char* e = std::getenv("BANISHED_RT_MATSETUP_DETOUR");
    if (!e || std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0)
        return false;   // opt-in; silence is the normal state

    if (!InstallRuntimeDetourChecked("Graphics::MaterialInstance::Setup[port]",
                                     kMatSetupAddr, ourFn, &g_detour,
                                     kMatSetupPrologue,
                                     kMatSetupBytes, kMatSetupPrologue)) {
        std::fprintf(stderr,
            "[MatSetupDetour] detour FAILED -- the DLL is UNTOUCHED and the port keeps "
            "only the callers a NAME registration reaches (ours, plus the DLL's "
            "SpriteContext::Flush caller whenever our Flush port is live). A safe "
            "failure, not a silent one: nothing was patched.\n");
        std::fflush(stderr);
        return false;
    }

    g_installed = true;
    std::fprintf(stderr,
        "[MatSetupDetour] DETOUR INSTALLED at 0x180102c90 -- the SECOND of retail's two "
        "rel32 callers, Graphics::Renderer::DrawItemList (0x180105240), now runs the "
        "port. That is the every-frame 3D material bind; the UI sprite path already "
        "reached the port by name.\n");
    std::fflush(stderr);
    return true;
}

// True only when this process actually patched 0x180102c90. Read it before
// believing an armed run measured the 3D path: the env var says what was ASKED
// FOR, this says what HAPPENED (reference_the_gate_was_never_a_control.md — a
// gate that is never read is not a control).
extern "C" bool RtGraphicsMatSetupDetourInstalled() { return g_installed; }
