#!/usr/bin/env python3
"""Runtime-x64.dll port census: called vs ported vs total, per namespace.

The exe port's progress question was "how many of the addresses our tree asks
for are registered". The runtime port's is the same shape, with three numbers
instead of two:

    total   — functions the DLL actually contains, per namespace
              (from docs/runtime_port/survey_ghidra_census.md, a live Ghidra
              count; this script does not talk to Ghidra)
    called  — the distinct functions OUR tree calls today, i.e. the dependency
              surface we must satisfy before cutover
              (scanned live from code/, engine/, runtime/, standalone/, dx11/)
    ported  — of those, the ones a RegisterRuntimeExport / RegisterRuntimeAddr /
              RegisterRuntimeOrdinal call has taken over

"called" is the honest denominator for the port: it is what has to work for
`reimpl-x64.exe` to boot without the DLL. "total" is the denominator for a
complete DLL reimplementation, which is not the stated goal (PLAN.md §4.1
decision 1: static link, no drop-in DLL).

Usage:
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py
    ... --csv docs/runtime_port/census_<stamp>.csv    write the per-function rows
    ... --unported                                    list what is called but not ported
    ... --namespace UI                                restrict to one namespace
"""

import argparse
import collections
import csv
import os
import re
import sys
import textwrap

# Set from --allow-degraded in main(); see load_export_addrs().
ALLOW_DEGRADED = False

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ["code", "engine", "runtime", "standalone", "dx11"]

# ===========================================================================
# ORPHAN_VERDICTS — the per-row triage of "ports with no caller in our tree"
# ===========================================================================
# This list exists because reference_uncalled_port_is_not_verified.md: a port
# with zero callers reports `calls=0 divergences=0` and reads as green. A WRONG
# Quaternion::operator* hid in exactly that state for a whole wave.
#
# UNTIL 2026-09-09 THE TRIAGE WAS PROSE, and prose rotted in the specific way
# prose does: the paragraph opened "All 10 of these have a WRITTEN VERDICT"
# while the list above it had grown past 500 rows, so a reader who trusted the
# sentence believed every printed row was accounted for. The count was stale
# for several waves and nothing could detect that, because a sentence cannot be
# checked against data.
#
# It is now a dict keyed by SYMBOL (a mangled export name) or by ADDRESS (an
# int), which has three properties prose did not:
#
#   1. A row either has a verdict or prints as UNTRIAGED. There is no third
#      state in which prose implies coverage it does not have.
#   2. STALENESS IS DETECTED. A verdict whose key is no longer in the orphan
#      list is reported as STALE at the bottom of the section -- that is the
#      self-check the old paragraph could not have. A key leaves this list when
#      the port gains a caller, which is good news, but a verdict left behind
#      claiming to describe a caller-less port is then wrong.
#   3. Adding a wave's triage is a data edit, not a prose rewrite.
#
# STATUS values, ordered from strongest evidence to weakest:
#
#   VERIFIED    Swept against the SHIPPED DLL by an oracle or probe: ours and
#               theirs ran on the same inputs and the results were compared.
#   EXERCISED   The BODY runs, via a direct C++ call from a sibling port,
#               exactly as retail calls it. Only the export REGISTRATION is
#               idle. The body's own diff site is the oracle.
#   STRUCTURAL  Verified WITHOUT executing -- e.g. decoding the DLL's own
#               tail-jump and asserting it targets what our port targets.
#               Weaker than VERIFIED and deliberately labelled differently.
#   PARTIAL     Some branches verified against the DLL, with a NAMED residue
#               that is not. The residue belongs in `note`, not rounded away.
#   NO_ORACLE   No comparison is possible and the reason is recorded. This is
#               a verdict, not an excuse -- it must say WHY, and a row whose
#               reason is merely "hard" does not qualify.
#   UNTRIAGED   Default for anything absent from this table.
#
# Each value is (STATUS, wave, note). Keep `note` to one line: it is printed.
ORPHAN_VERDICTS = {
    # GRADUATED -- do not re-add. Listed here so the next reader knows these
    # left the table on purpose rather than by accident:
    #
    #   ??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z (Quaternion::operator*)
    #     Retired 2026-09-09, on the FIRST run of this table's stale-verdict
    #     self-check, which flagged it immediately. It is no longer caller-less:
    #     runtime/src/graphics/mesh.cpp:261 resolves it with RT_EXPORT_ONCE --
    #     which consults the port registry, unlike RT_ORIGINAL_ONCE -- and
    #     RtQuatMul() is called at mesh.cpp:629 and :816. So the Wave 1 port
    #     that reference_uncalled_port_is_not_verified.md was written about is
    #     finally exercised, and a verdict describing it as caller-less would
    #     now be wrong. The stale source comment that still claimed otherwise
    #     (mesh.cpp, "keeps its zero-caller status ... still on the census
    #     orphan list") was corrected at the same time.
    #
    # --- Wave 8.5, docs/runtime_port/WAVE8_5.md section 2 -------------------
    "??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z": (
        "EXERCISED", "8.5", "called from RtMath_Transform_FromDirection (transform.cpp:770)"),
    "?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z": (
        "EXERCISED", "8.5",
        "called from transform.cpp:798; Wave 1 found a reassociated float sum through it"),
    "?Serialize@Serializer@IO@@QEAAXHPEACPEBD@Z": (
        "STRUCTURAL", "8.5",
        "schar[] member of the 18-member forwarder family; deliberately dead, "
        "RtIoSerializerRegister decodes the DLL tail-jump and asserts the vtable slot"),
    "?Serialize@Serializer@IO@@QEAAXHPEADPEBD@Z": (
        "STRUCTURAL", "8.5", "char[] member of the same family; same structural check"),
    "?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z": (
        "STRUCTURAL", "8.5", "short[] member of the same family; same structural check"),
    "?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z": (
        "VERIFIED", "8.5",
        "rt_orphan_probe: 10 cases (zero vector, unit axes, denormals, "
        "overflow-to-inf, -0.0), buffers poisoned 0xCD on both sides"),
    "??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z": (
        "STRUCTURAL", "8.5",
        "cannot be probed (a valid source's copy would run Geometry::~Geometry "
        "over an attach array); verified by disassembly comparison"),
    "?PauseEffect@Scene@Audio@@QEAAXI@Z": (
        "EXERCISED", "8.5",
        "Scene::Pause calls it as retail does (not inlined); also swept by ProbeAudioScene"),
    "?StopEffect@Scene@Audio@@QEAAXI@Z": (
        "EXERCISED", "8.5", "called from Scene::StopEffect3D; also swept by ProbeAudioScene"),

    # --- Wave 12, docs/runtime_port/WAVE12_APPLICATION.md ------------------
    # These eleven are caller-less for ONE shared reason worth stating once:
    # our tree calls the Platform* siblings directly, so the public wrappers
    # and the accessors never get a call site. That is not vestigiality.
    # Evidence for every row: docs/runtime_port/wave12_evidence/run_*.txt.
    "?Get@Application@1@SAPEAV11@XZ": (
        "VERIFIED", "12",
        "6 sentinels written into the DLL's own _singleton slot 0x1801fc678, "
        "all agreed, slot restored; refuses to run under BANISHED_RT_MINT=1"),
    "?GetWindow@Application@1@QEBAPEAUHWND__@@XZ": (
        "VERIFIED", "12", "part of the 216-state x 5-reader sweep (1081 comparisons)"),
    "?IsCursorEnabled@Application@1@QEAA_NXZ": (
        "VERIFIED", "12",
        "216-state sweep; FOUND A REAL DEFECT -- returned a normalised bool where "
        "retail returns the raw byte (144 divergences), now uint8_t both sides"),
    "?PlatformIsPaused@Application@1@AEAA_NXZ": (
        "VERIFIED", "12",
        "216-state sweep; also proved DISTINCT from the identical-bodied IsPaused"),
    "?PlatformDestruct@Application@1@AEAAXXZ": (
        "VERIFIED", "12",
        "retail body is `ret 0x0`; 4 fill patterns x 0x100 bytes bracketed on BOTH "
        "sides, neither wrote a byte -- verified to do NOTHING, which is the whole row"),
    "?Update@Application@1@QEAA_NXZ": (
        "STRUCTURAL", "12",
        "NOT executed (it pumps a message loop): retail's jmp target decoded AND our "
        "compiled body scanned for the call to our own PlatformUpdate"),
    "?Startup@Application@1@QEAA_NXZ": (
        "STRUCTURAL", "12", "same two-sided jmp/body check, target PlatformStartup"),
    "?Shutdown@Application@1@QEAAXXZ": (
        "STRUCTURAL", "12",
        "same two-sided check, target PlatformShutdown; this screen is what proves the "
        "integration seam was re-pointed and lane B's body is not bypassed"),
    "?EnableCursor@Application@1@QEAAX_N@Z": (
        "VERIFIED", "12",
        "30 rounds through rt_predict.h's PREDICTING oracle (a replay would fire a "
        "second live cursor change): 20 changed, 0 wrong, self-test control 20/20"),
    "?LaunchProcess@Application@1@QEAA_NPEBD_N@Z": (
        "PARTIAL", "12",
        "5 wait=true CreateProcessW FAILURE cases across retail's 0x104 clamp; the "
        "ShellExecute arm, the success path, the leaked hThread and the actual command "
        "line are UNVERIFIED and not probeable without spawning a real process"),
    "?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z": (
        "UNTRIAGED", "12",
        "explicitly left untriaged by Wave 12 rather than assumed fine along with "
        "its ten neighbours"),
}

# Printed in this order, strongest evidence first. UNTRIAGED is last on
# purpose: it is the actionable bucket and should be the reader's last word.
ORPHAN_STATUS_ORDER = ["VERIFIED", "EXERCISED", "STRUCTURAL", "PARTIAL",
                       "NO_ORACLE", "UNTRIAGED"]
SKIP_DIR_NAMES = {"build", "build-og", "build-perf", "obj", ".git", "__pycache__", ".venv"}
SOURCE_EXT = (".cpp", ".h", ".hpp", ".c")

# --- call sites into the DLL -------------------------------------------------
# These match only up to the OPENING quote; the mangled name itself is read by
# concat_literal() below, which absorbs any further string literals that
# follow separated only by whitespace. A mangled name split across two source
# lines is one C++ string literal after preprocessing, not two -- a plain
# `"([^"]+)"` capture reads only the first fragment and silently truncates it.
# (Confirmed live: gameupdateloop_load.cpp:71 SetAchievement, and
# pickingcomponent.cpp:171 AddMesh, both reported as phantom exports by
# export_name_audit.py before this fix.)
STRING_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')


def concat_literal(text, pos):
    """Read the C++ string literal starting at `pos` and absorb any further
    literals that immediately follow (separated only by whitespace) into one
    joined name, exactly as the C++ preprocessor concatenates adjacent string
    literals. Returns the joined content with quotes stripped."""
    parts = []
    i = pos
    while True:
        m = STRING_LITERAL.match(text, i)
        if not m:
            break
        parts.append(m.group(0)[1:-1])
        i = m.end()
        j = i
        while j < len(text) and text[j] in " \t\r\n":
            j += 1
        if j < len(text) and text[j] == '"':
            i = j
            continue
        break
    return "".join(parts)


RE_EXPORT = re.compile(r'ResolveExport(?:Raw)?\s*\(\s*(?=")')

# A ResolveExport whose argument is a NAMED CONSTANT rather than a literal --
# the SIXTH blind spot, and the first one this script found by being used
# rather than by an incident.
#
# Wave 10 hit it while porting the static GetAlignedPosition overload. The port
# spelled the mangled name once as `const char* const kGetAlignedStatic = ...`
# and used it at BOTH the ResolveExport site and the RegisterRuntimeExport site
# -- which is the practice the RE_NAME_CONST comment explicitly ENCOURAGES,
# because it stops the door and the registration drifting apart. The
# registration side resolved it. The CALL side did not, so the function fell out
# of `called` entirely, and since `ported` is a subset of `called` the freshly
# landed port read as a REGRESSION: UI called 161->160, ported flat.
#
# So the same `name_consts` table both sides already needed is now applied to
# both sides, and an unresolvable identifier is reported instead of dropped.
RE_EXPORT_NONLITERAL = re.compile(
    r'ResolveExport(?:Raw)?\s*\(\s*(?!")([A-Za-z_][A-Za-z0-9_:]*)\s*\)')

# THE MACRO DOORS -- the fifth blind spot, and the most expensive one this
# script has had. `called` matched a LITERAL `ResolveExport(` only, so the two
# macros in runtime/funcs/rt_port.h were invisible:
#
#   RT_EXPORT_ONCE(FnType, "?Mangled@...")   -- name is the SECOND argument
#   RT_DATA_SLOT(T,        "?Mangled@...")   -- name is the SECOND argument
#   RtResolveCached("?Mangled@...", &cache)  -- name is the FIRST argument
#
# All three end in RtResolveCached and are therefore live cutover dependencies
# in exactly the sense this column exists to count. 113 of them were unseen --
# including `?SetupDirectories@FileSystem@IO@@` and `?LoadPackages@FileSystem@IO@@`,
# the two symbols the DLL-less boot spent attempts 09 through 12 dying on. The
# census could not see the wall it was being used to look for.
#
# The two macros take a TYPE as their first argument, which can itself contain
# commas inside angle brackets or a function-pointer parameter list, so the name
# is found by scanning for the first string literal rather than by splitting on
# commas.
RE_MACRO_DOOR = re.compile(r'\b(?:RT_EXPORT_ONCE|RT_DATA_SLOT)\s*\(')
RE_RESOLVE_CACHED = re.compile(r'\bRtResolveCached\s*\(\s*(?=")')

# ---- THE WRAPPER-DOOR SCREEN (Wave 21; found by lane GFXVT) --------------
#
# THE SIXTH INSTANCE OF ONE FAMILY, AND THE REASON THIS IS A SCREEN RATHER
# THAN A SEVENTH PATTERN.  Every previous fix above added one more literal to
# match -- which is precisely why there was a sixth.  GFXVT found three live
# cutover dependencies (the EdgeMesh, EdgeMeshInstance and MeshInstance
# vtables) that never enter `called` at all, because they are reached through
# `RtVtable("...")` at runtime/src/graphics/mesh.cpp:305 -- a project-local
# one-line wrapper around ResolveExport that no pattern here recognises.
#
# GFXVT's control is what makes that a diagnosis rather than a guess: RtVtable
# has exactly five call sites, and the other two rows it names (Base@Object,
# External@Object) appear in the census ONLY because a second, known door also
# names them.  The wrapper door is invisible; the two survivors are
# coincidences.
#
# So this does not match a door.  It matches ANY identifier called with a
# mangled-name string literal, and prints the ones it does NOT recognise, BY
# NAME, with counts.  An instrument that says "I found N doors and could not
# classify these M identifiers" is worth more than one that silently matches N
# -- which is this wave's own doctrine (a screen must name what it could not
# examine) applied to the census itself.
#
# IT CHANGES NO COUNT.  Deliberately: deciding that a given wrapper IS a door
# is a judgement about that wrapper's semantics, and making it here would move
# numbers other lanes are quoting mid-wave.  The screen hands the list forward.
RE_MANGLED_CALL = re.compile(
    r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*"(\?[^"]{4,})"')

# Identifiers that ARE known doors, or are known NOT to be (a registration
# side, a probe, a log line).  An identifier absent from both lists is
# reported as UNCLASSIFIED -- never silently assumed to be either.
WRAPPER_KNOWN_DOOR = {
    "ResolveExport", "ResolveExportRaw", "RtResolveCached",
    "RT_EXPORT_ONCE", "RT_DATA_SLOT", "InstallRuntimeDetourExport",
}
WRAPPER_KNOWN_NOTDOOR = {
    # registration / minting sides: they publish, they do not resolve
    "RegisterRuntimeExport", "RegisterRuntimeAddr", "RegisterDispatchSlot",
    "RegisterExeAddr", "RtMintPromotionStorage", "RtMintVtable",
    # diagnostics and string handling
    "printf", "fprintf", "sprintf", "snprintf", "strcmp", "strncmp", "strstr",
    "puts", "fputs", "TRACE", "LOG", "Log", "assert",
}


def first_literal_after(text, pos, limit=400):
    """Position of the first `"` within `limit` chars of `pos`, or None.

    Used for the two macro doors, whose first argument is a TYPE that may
    contain commas (`Collection::Array<A, B>`, a function-pointer signature).
    Splitting on commas would pick the wrong argument; the name is simply the
    first string literal in the call, so find that.
    """
    stop = min(len(text), pos + limit)
    i = text.find('"', pos)
    if i == -1 or i >= stop:
        return None
    # A `)` before the literal means the call had no string argument at all.
    if ')' in text[pos:i]:
        return None
    return i
RE_ORDINAL = re.compile(r'ResolveOrdinal(?:Raw)?\s*\(\s*(\d+)')
RE_ADDR = re.compile(r'ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)')
RE_ADDR_RAW = re.compile(r'ResolveRuntimeRaw\s*\(\s*(0x[0-9a-fA-F]+)')

# dx11/ reaches Runtime-x64.dll through its OWN resolver, not ResolveExport:
#   Dx11ResolveRuntimeImport(mangled, iatGhidraAddress)
# and it almost never passes the name at the call site -- the names live in
# `{ &g_im.Field, "?Mangled@...", 0x1800134xx }` import tables that are then
# walked in a loop. RE_EXPORT sees none of it, so every one of dx11's Runtime
# imports was missing from `called` until the BANISHED_RT_STRICT residue census
# caught 86 of them arriving at the DLL during a plain autostart run.
#
# That gap matters more than its size: dx11/ IS our VideoDX11 replacement, the
# one subsystem that already survives its DLL being absent, and its Runtime
# imports are part of the cutover surface exactly like the exe's are.
#
# Restricted to dx11/ and to names that look like MSVC C++ manglings (leading
# '?', an '@@' somewhere) so an ordinary string literal cannot become a phantom
# call row -- the failure strip_comments() exists to prevent.
RE_DX11_MANGLED = re.compile(r'"(\?[A-Za-z0-9_?$@]*@@[A-Za-z0-9_?$@]*)"')

# --- ports -------------------------------------------------------------------
RE_REG_EXPORT = re.compile(r'RegisterRuntimeExport\s*\(\s*(?=")')

# A RegisterRuntimeExport whose first argument is NOT a string literal. This
# regex exists because of a real, silent under-count: Wave 4 lane D registered
# its six FileSystem/Modification ports through `kName*` constants, which
# RE_REG_EXPORT cannot see through, and the census reported IO 30/41 while 36
# were in fact registered. Nothing failed and nothing warned -- the ports were
# live in the process and invisible to the tool the project treats as its
# authority over any agent's "all N ported" claim.
#
# This is the mirror image of reference_uncalled_port_is_not_verified.md: there
# a port is counted but never runs; here a port runs but is never counted. Both
# end in a census that does not describe reality, so both must be loud.
RE_REG_EXPORT_NONLITERAL = re.compile(
    r'RegisterRuntimeExport\s*\(\s*(?!")([A-Za-z_][A-Za-z0-9_:]*)')

# A file-scope `const char* const kName = "?Mangled@...";` definition. Spelling
# the name once and using it at BOTH the RT_ORIGINAL_ONCE site and the
# RegisterRuntimeExport site is good practice -- the diff oracle and the
# registration cannot then drift apart -- so the census resolves these instead
# of punishing them. Only same-file, single-literal definitions are resolved;
# anything else still raises the NON-LITERAL warning above, because a name this
# script cannot follow is a name it must not silently drop.
#
# BOTH SPELLINGS. `const char* const kX` and `constexpr const char* kX` are the
# same idiom and the tree uses both -- runtime/types/ui/ElementDescription.h and
# code/Game/Dialog/mod_dialog_vtables.cpp use the constexpr form. Matching only
# the first meant a file could name a symbol once, correctly, and still be
# invisible to this script.
RE_NAME_CONST = re.compile(
    r'\b(?:(?:static\s+)?const\s+char\s*\*\s*const|constexpr\s+const\s+char\s*\*)\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?=")')
RE_REG_ORDINAL = re.compile(r'RegisterRuntimeOrdinal\s*\(\s*(\d+)')
RE_REG_ADDR = re.compile(r'RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)')

# --- detours (a port that also took over the DLL's own internal callers) -----
RE_DETOUR_ADDR = re.compile(r'InstallRuntimeDetour(?:Checked)?\s*\(\s*"([^"]*)"\s*,\s*(0x[0-9a-fA-F]+)')

# The SAME blind spot as RE_REG_EXPORT_NONLITERAL, one column over, and it bit
# immediately: Wave 10's first UI detour passes its target as
# `constexpr uintptr_t kUpdateContainersAddr = 0x1800d9710;` and RE_DETOUR_ADDR
# -- which demands a literal -- could not see it. The detour was installed, the
# audit tool confirmed it, the process logged it, and this census still printed
# `detour 0` for UI.
#
# A tool that already documents a blind spot for one column and then repeats it
# in the next is the shape of reference_literal_grep_blind_to_variables.md, so
# both halves of the cure are here: same-file `constexpr uintptr_t kX = 0x...;`
# definitions are RESOLVED, and anything still unresolved is reported LOUDLY
# rather than dropped.
RE_DETOUR_NONLITERAL = re.compile(
    r'InstallRuntimeDetour(?:Checked)?\s*\(\s*"([^"]*)"\s*,\s*(?!0x)([A-Za-z_][A-Za-z0-9_:]*)')

# BLIND SPOT SEVEN, found by lane UIDRAW (Wave 10). Everything above matches the
# `InstallRuntimeDetour*` FRONT DOORS only. The tree has a SECOND installer --
# `RuntimeDetourInstall`, the deliberately dumb byte-copy one -- and the census
# could not see a single one of its call sites.
#
# That is not a random gap, which is why it is worth a paragraph. The front doors
# route through `InstallDetour`, whose automatic `MOV RAX,RSP -> ADD RAX,8`
# correction ("Bug Class 3") is WRONG for a prologue that saves registers
# RAX-relative and restores them RSP-relative. So `RuntimeDetourInstall` is
# exactly the installer the HARDEST subjects are required to use --
# `UI::UserInterface::Update` (lane UI1) and `UI::UserInterface::Draw` (lane
# UIDRAW) both are -- and the column was therefore blind precisely where a
# detour is most delicate. UI printed `detour 2` while four `[port]` detours
# were installed and logging.
#
# The argument order is INVERTED relative to the front doors (address first,
# label last), which is why a copy of the patterns above would not have worked.
# The label still decides port-vs-instrument through _add_detour, so the two
# unlabelled instrument users of this installer (persistobject_probe.cpp,
# video_interface.cpp) stay instruments.
RE_DETOUR_BYTECOPY = re.compile(
    r'RuntimeDetourInstall\s*\(\s*(0x[0-9a-fA-F]+|[A-Za-z_][A-Za-z0-9_:]*)\s*,')
RE_LAST_STRING = re.compile(r'"([^"]*)"')


def bytecopy_detour_calls(text):
    """(target, label) for every `RuntimeDetourInstall(target, ..., "label")`.

    Parenthesis-MATCHED rather than regex-terminated. The first version of this
    ended the argument list at the first `)` a non-greedy `[^;]*?` could reach,
    which is the one inside `static_cast<int>(kUiDrawPrologue)` -- so it found
    the call, found no label, and filed it as an unlabelled instrument. A silent
    downgrade, in the exact column the function was added to fix.
    """
    out = []
    for m in RE_DETOUR_BYTECOPY.finditer(text):
        i, depth, n = m.end(), 1, len(text)   # just past the first comma
        while i < n and depth:
            c = text[i]
            if c == '"':                          # skip a string literal whole
                i += 1
                while i < n and text[i] != '"':
                    i += 2 if text[i] == '\\' else 1
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if not depth:
                    break
            i += 1
        labels = RE_LAST_STRING.findall(text[m.end():i])
        out.append((m.group(1), labels[-1] if labels else ""))
    return out

# A file-scope `constexpr uintptr_t kX = 0x...;` (or const/static uintptr_t).
# Mirrors RE_NAME_CONST, which resolves the same shape for mangled names.
RE_ADDR_CONST = re.compile(
    r'\b(?:static\s+)?(?:constexpr|const)\s+(?:uintptr_t|unsigned\s+long\s+long|uint64_t)\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9a-fA-F]+)')
RE_DETOUR_EXPORT = re.compile(r'InstallRuntimeDetourExport\s*\(\s*"([^"]*)"\s*,\s*"([^"]+)"')

# BLIND SPOT SEVEN (Wave 10 lane CLOSEDLG) -- THE OTHER INSTALLER.
#
# All three regexes above match `InstallRuntimeDetour*`, the relocating front
# doors. The tree has a SECOND installer, `RuntimeDetourInstall` -- the older
# byte-copy one runtime_detour.h calls "deliberately dumb" -- and NOTHING here
# matched it, so every detour installed through it was invisible. Not
# "unresolved and reported": invisible. `nonliteral_detours` came back EMPTY
# while four subjects were detoured, because an unmatched call site is not an
# unresolved one.
#
# That is not a marginal installer. Lane UI1 established that it is the CORRECT
# one for any prologue ending in `MOV RAX,RSP`, because the front doors apply an
# `ADD RAX,8` trampoline fixup ("Bug Class 3") that desynchronises an
# RAX-relative save from an RSP-relative restore. So the census was blind to
# exactly the installer the hardest subjects are obliged to use, and two PORT
# detours were missing from the board:
#
#     0x1801213c0  UI::UserInterface::Update[port]      (lane UI1, five runs)
#     0x180120f20  UI::UserInterface::CloseDialog[port] (lane CLOSEDLG)
#
# The argument order differs -- `RuntimeDetourInstall(addr, wrapper, len, bytes,
# len, "label")` puts the label LAST -- which is why a copy of the pattern above
# would not have worked. The negative lookahead rejects the DEFINITION in
# standalone/filename_runtime_hook.cpp, whose first parameter is a declaration
# (`uintptr_t addr`) rather than an argument; without it the scanner reads a
# function signature as a call site.
#
# The port/instrument split needs no new rule: `_add_detour` already treats an
# UNLABELLED detour as an instrument, which is the right answer for the two
# other users of this installer (`persistobject_probe.cpp` and
# `video_interface.cpp`, both wrappers, neither carrying `[port]`).
RE_DETOUR_INSTALL = re.compile(
    r'\bRuntimeDetourInstall\s*\(\s*'
    r'(?!(?:const\s+|unsigned\s+)?[A-Za-z_]\w*\s*\*?\s+[A-Za-z_]\w*\s*[,)])'  # not a declaration
    r'(0x[0-9a-fA-F]+|[A-Za-z_][A-Za-z0-9_]*)\s*,[^;{}]*?"([^"]*)"\s*\)')

# Function counts per namespace, from the live Ghidra census recorded in
# docs/runtime_port/PLAN.md §3.1. Update both together.
TOTAL_FUNCTIONS = {
    "UI": 1625, "Graphics": 599, "Video": 573, "IO": 407, "System": 262,
    "Math": 228, "Object": 191, "Audio": 137, "Achievement": 118, "Engine": 108,
    "Collision2D": 76, "Volume": 64, "Input": 63, "Picking": 49,
    "Pathfinding": 45, "Save": 37, "Application": 26, "Utility": 16, "Debug": 8,
}


# Absolute path of the shipped DLL, used ONLY to turn a raw address back into
# the exported name at that address. Everything else in this script is a source
# scan.
RUNTIME_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"


def load_export_addrs():
    """{ image address -> mangled name } for every export in the shipped DLL.

    Added 2026-09-06 (Wave 5), because the census had a third blind spot that
    both lane B and lane D hit independently:

        A function our tree reaches only by RAW ADDRESS was bucketed under the
        pseudo-namespace "internal", never into the namespace it belongs to.

    So a namespace row UNDER-COUNTED any class our tree calls by address --
    "Picking 8/8 100%" while a ninth Picking function (RemoveMesh, reached only
    at 0x180125290) sat in the internal bucket -- and, worse, the same function
    could be counted TWICE when one call site used the name and another used
    the address. Both directions corrupt the number this project treats as its
    authority over any agent's "all N ported" claim.

    FAILS LOUDLY if pefile or the DLL is unavailable.

    It used to `return {}` and let the census "degrade to its old address-blind
    behaviour rather than failing". That was wrong, and Wave 9 lane 0 measured
    exactly how wrong: run under the project venv (which has pefile) the census
    reports **830 called / 512 ported**; run under system Python 3.12 (which does
    not) the SAME tree reports **1016 / 616**. Same command, same source, two
    different authorities, no warning either time. It produced a wrong
    intermediate finding inside lane 0's own work (57 phantom "census blind
    spots") before the cause was found.

    That is this project's most-repeated failure mode, and this function was an
    instance of it: `reference_truncated_tool_output_manufactures_calls_zero.md`
    (a `tail -80` faked sixteen sites disappearing) and WAVE9.md's three census
    defects are the same shape -- a tool that answers when it cannot answer. The
    census is the number this project treats as authority over any agent's "all N
    ported" claim, so it must refuse to produce one it cannot stand behind.

    `--allow-degraded` restores the old behaviour for the one case that deserves
    it (a machine with no pefile that only wants the name-keyed rows), and says so
    on stderr every time rather than silently.
    """
    def _bail(why):
        if ALLOW_DEGRADED:
            sys.stderr.write(
                "WARNING: census running DEGRADED (%s).\n"
                "         Address-keyed rows are invisible; 'called' and 'ported'\n"
                "         are BOTH understated and the namespace split is wrong.\n"
                "         Do not publish this number.\n" % why)
            return {}
        sys.stderr.write(
            "FATAL: %s\n"
            "       The census cannot see functions our tree reaches by ADDRESS\n"
            "       without it, which changes the answer materially -- measured\n"
            "       830/512 (correct) vs 1016/616 (degraded) on the same tree.\n"
            "       Run it with the project venv:\n"
            "         tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py\n"
            "       or pass --allow-degraded if you really want the partial view.\n"
            % why)
        sys.exit(2)

    try:
        import pefile
    except ImportError:
        return _bail("pefile is not importable in this interpreter")
    if not os.path.exists(RUNTIME_DLL):
        return _bail("the shipped DLL is missing: %s" % RUNTIME_DLL)
    try:
        pe = pefile.PE(RUNTIME_DLL, fast_load=True)
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        base = pe.OPTIONAL_HEADER.ImageBase
        out = {}
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if e.name:
                out[base + e.address] = e.name.decode("utf-8", "replace")
        if not out:
            return _bail("the DLL's export directory parsed EMPTY")
        return out
    except SystemExit:
        raise
    except Exception as exc:
        return _bail("reading the DLL's export directory failed: %r" % exc)


def load_export_ordinals():
    """{ ordinal -> (mangled name, image address) } for the shipped DLL.

    BLIND SPOT NINE (Wave 25 lane INTERNAL42). The census used to give every
    `ResolveOrdinal(N)` call site a row of its own, keyed by the NUMBER, in a
    pseudo-namespace called `ordinal`, and it could never mark such a row ported
    unless somebody had also called `RegisterRuntimeOrdinal(N)`. That is the
    same defect `load_export_addrs()` fixes for the ADDRESS door, one door
    later: an ordinal, an address and a mangled name can all denote the SAME
    function, and counting it once per door inflates `called` and deflates
    `ported` simultaneously.

    It is not hypothetical and it is not one row. On the Wave 25 base tip four
    of the six `ordinal` rows printed as a gap:

        443  -> ??0Thread@System@@QEAA@I@Z                     already ported
        3214 -> ?Lock@Device@Video@@QEAAXXZ                    already ported
        4462 -> ?Unlock@Device@Video@@QEAAXXZ                  already ported
        2990 -> ?GetTypeIdForFileExtension@Factory@Object@@... ported Wave 24

    The first three are PHANTOMS -- a second row for a function that already has
    a ported `export` row. The fourth is a real function that belongs in
    `Object`, ported and registered under both doors by Wave 24 lane INTERNAL8,
    with no `export` row of its own because nothing in the tree resolves it BY
    NAME: the two exe call sites use the ordinal.

    And the ordinal door is not a dead end at runtime either --
    `standalone/resolve_standalone.cpp:ResolveOrdinal` misses `RtOrdinalReg()`
    and then falls through to `MangledForOrdinal()` + `RtExportReg()`, so a port
    registered by NAME is reached by an ordinal caller. All four rows were live
    ports printing as gap rows.

    Degrades exactly like `load_export_addrs()` (same interpreter, same DLL,
    same `--allow-degraded` contract): an empty map means every ordinal keeps
    its own row, i.e. the old behaviour.
    """
    if ALLOW_DEGRADED:
        try:
            import pefile
        except ImportError:
            return {}
        if not os.path.exists(RUNTIME_DLL):
            return {}
    else:
        import pefile          # load_export_addrs() has already bailed if absent
    try:
        pe = pefile.PE(RUNTIME_DLL, fast_load=True)
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        base = pe.OPTIONAL_HEADER.ImageBase
        out = {}
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            # A NONAME export has an ordinal and no name; it cannot be folded,
            # and it is left out so the caller's `unresolved` list stays honest.
            if e.name:
                out[e.ordinal] = (e.name.decode("utf-8", "replace"),
                                  base + e.address)
        return out
    except Exception:
        return {}


def load_section_map():
    """[(name, lo, hi)] for the shipped DLL's sections, or None.

    SECONDARY evidence only, and deliberately kept apart from the name-based
    classifier below. It exists for one population the classifier CANNOT
    reach: a row keyed by a raw address has no mangled name, so no amount of
    name grammar will say whether it is code or data. Section membership does
    say that, from a different piece of evidence, so it is printed as a HINT
    beside the UNKNOWN count and never folded into a named bucket.

    Returns None (not {}) when it cannot read the DLL, so the caller prints
    "not probed" rather than a zero. A zero here would be indistinguishable
    from "every address was unclassifiable", which is the failure this whole
    section exists to avoid.
    """
    try:
        import pefile
    except ImportError:
        return None
    if not os.path.exists(RUNTIME_DLL):
        return None
    try:
        pe = pefile.PE(RUNTIME_DLL, fast_load=True)
        base = pe.OPTIONAL_HEADER.ImageBase
        out = []
        for s in pe.sections:
            lo = base + s.VirtualAddress
            out.append((s.Name.rstrip(b"\x00").decode("ascii", "replace"), lo,
                        lo + max(s.Misc_VirtualSize, s.SizeOfRawData)))
        return out or None
    except Exception:
        return None


def section_of(secmap, va):
    if secmap is None:
        return None
    for name, lo, hi in secmap:
        if lo <= va < hi:
            return name
    return "(outside every section)"


# ---- THE SYMBOL-KIND CLASSIFIER (Wave 21 lane CLASS) ---------------------
#
# WHY IT EXISTS. A namespace's gap (called - ported) was being read as "N
# functions left to port" and it is not one population. Wave 10 discovered
# this for UI the expensive way -- 13 of its 31 were minted DATA with no body
# to reimplement (memory project_ui_namespace_gap_decomposition) -- and Wave
# 21's kickoff discovered it again, from scratch, for Graphics: 29 = 13 code
# rows + 8 vtable rows + 8 data rows. The fact was written down both times and
# NOTHING IN THE CENSUS ASKED FOR IT, so each wave paid for it again.
#
# WHAT IT READS. The MSVC mangled name, and nothing else. It does not consult
# the DLL, the mint tables, or our source. It is therefore blind to everything
# a name does not carry -- including how big a body is, whether it is
# reachable, and whether a DATA row is minted already.
#
# WHAT IT CANNOT CLASSIFY, stated here rather than discovered later:
#   * an ADDRESS-KEYED row (our tree reaches it by VA; it is unexported and
#     has no name at all) -> UNKNOWN, with a section hint from load_section_map
#   * an ORDINAL row (the export directory gives a number, not a name)
#     -> UNKNOWN, no hint available
#   * any name whose grammar this walker cannot finish -> UNKNOWN, printed by
#     name so it can be fixed rather than absorbed
# An identifier a tool cannot resolve is UNKNOWN, not ABSENT
# (memory reference_a_screen_that_cannot_name_its_subject): collapsing the
# first bucket into BODY would silently claim 29 unexported .text rows and 12
# unexported DATA rows are all port work, which is the INFLATING direction and
# the one that manufactures work.

KIND_ORDER = ["BODY", "CTOR", "DTOR", "OPNEWDEL", "OPERATOR", "VTABLE",
              "DATA", "UNKNOWN"]


def _type_code_char(mangled):
    """The MSVC type-code character that follows the qualified-name list.

    Walks the same grammar as namespace_of() -- fragments terminated by '@',
    single-digit back-references that are NOT terminated, list closed by an
    empty component -- and returns the first character after that closing '@'.
    In MSVC's grammar that character is a DIGIT for static/global DATA
    (0..5 = member data by access, 3 = plain global) and a LETTER for a
    function (A..Z member access + calling convention, Y = free function).
    Returns None if the name cannot be walked to that point.
    """
    if not mangled.startswith("?"):
        return None
    i, n = 1, len(mangled)
    if i < n and mangled[i] == "?":
        # operator/special name: "?X" or "?_X"; not a name fragment
        i += 1
        if i < n and mangled[i] == "_":
            i += 1
        i += 1
    while i < n:
        c = mangled[i]
        if c == "@":                     # empty component: list closed
            return mangled[i + 1] if i + 1 < n else None
        if c.isdigit():                  # back-reference, no terminator
            i += 1
            continue
        j = mangled.find("@", i)
        if j < 0:
            return None
        i = j + 1
    return None


def classify_symbol(key, kind):
    """(bucket, why) for one census row key. `why` is set only for UNKNOWN."""
    if kind == "ordinal":
        return "UNKNOWN", "ordinal row: the binary gives a number, not a name"
    if not key.startswith("?"):
        return "UNKNOWN", "address-keyed row: unexported, no mangled name"
    if key.startswith("??_7") or key.startswith("??_8"):
        return "VTABLE", ""              # ??_7 vftable, ??_8 vbtable
    if key.startswith("??_R") or key.startswith("??_C"):
        return "DATA", ""                # RTTI records, string literals
    if key.startswith("??0"):
        return "CTOR", ""
    if key.startswith("??1") or key.startswith("??_E") or key.startswith("??_G"):
        return "DTOR", ""                # dtor, vector/scalar deleting dtor
    if key.startswith("??2") or key.startswith("??3"):
        return "OPNEWDEL", ""
    if key.startswith("??"):
        return "OPERATOR", ""
    tc = _type_code_char(key)
    if tc is None:
        return "UNKNOWN", "mangled name this walker could not finish"
    if tc.isdigit():
        return "DATA", ""
    if tc.isalpha():
        return "BODY", ""
    return "UNKNOWN", "type code %r is neither a digit nor a letter" % tc


CLASSIFIER_FIXTURES = [
    # (row kind, key, expected bucket, what this case exists to pin)
    ("export", "?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z",
     "BODY", "ordinary virtual member function"),
    ("export", "?Foo@@YAXXZ", "BODY", "free function (type code Y, not a member)"),
    ("export", "?GetDescription@Achievement@1@QEBA?AVString@System@@XZ",
     "BODY", "innermost scope is a BACK-REFERENCE, so the list ends on a "
             "SINGLE '@' -- the case that once filed three Achievement "
             "methods under System"),
    ("export", "?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A",
     "DATA", "private static member data (type code 0)"),
    ("export", "?_frameIndex@ParticleInstance@Graphics@@1HA",
     "DATA", "protected static member data (type code 1)"),
    ("export", "?_singleton@Modification@IO@@2PEAV12@EA",
     "DATA", "public static member data (type code 2)"),
    ("export", "??_7Light@Graphics@@6B@", "VTABLE", "vftable"),
    ("export", "??0FrameCopyImageBuffer@Graphics@@QEAA@XZ", "CTOR", "constructor"),
    ("export", "??1FrameCopyImageBuffer@Graphics@@UEAA@XZ", "DTOR", "destructor"),
    ("export", "??_GTextFileStream@IO@@UEAAPEAXI@Z", "DTOR",
     "scalar DELETING destructor -- still a dtor, not an operator"),
    ("export", "??2MaterialInstance@Graphics@@SAPEAX_K@Z", "OPNEWDEL",
     "operator new"),
    ("export", "??4Vector3@Math@@QEAAAEAV01@AEBV01@@Z", "OPERATOR",
     "operator= -- an operator that is NOT new/delete/ctor/dtor"),
    ("addr", "0x180003970", "UNKNOWN", "address-keyed row: there is no name"),
    ("ordinal", "1234", "UNKNOWN", "ordinal row: there is no name"),
    # THE NEGATIVE CONTROL. Nothing in the live tree currently trips the
    # grammar branch, so without this line "0 unparseable names" would be
    # indistinguishable from a classifier that had stopped parsing at all
    # (memory reference_absence_screen_must_be_handed_a_shape).
    ("export", "?BrokenName@NoTerminator", "UNKNOWN",
     "DELIBERATELY MALFORMED: an unterminated final fragment. This is the "
     "synthetic shape handed to the UNKNOWN branch."),
]


def selftest_promoted_doors():
    """Control for mint_promoted_symbols(): BOTH promotion doors must be seen.

    WAVE 25 INTEGRATION, and the fifth instance of a literal-grep instrument
    going blind to a fact spelled through a variable. Lane SWEEP's
    PromoteVtableGuarded() refactor is an IMPROVEMENT -- it makes a row refuse
    itself rather than stamp abort traps -- and it silently cut 111 rows out
    of the census denominator by moving the symbol behind a parameter.

    The screen is ABSENCE-shaped: when a door stops being seen the result is a
    SMALLER SET, never an error. So it is HANDED a synthetic buffer holding
    both spellings and requires BOTH, and it then corroborates against the
    real file -- a fixture alone would pass while the thing it guards drifted.
    """
    fixture = "\n".join([
        "void f() {",
        '    st = RtMintPromotionStorage("?_typeInfo@Direct@UI@@0V@A", 0x1ull);',
        '    PromoteVtableGuarded("??_7Guarded@UI@@6B@", 0x2ull, &n);',
        '    // RtMintPromotionStorage("?_typeInfo@InAComment@UI@@0V@A", 0x3ull);',
        "}",
    ])
    src = strip_comments(fixture)
    direct = set(re.findall(r'RtMintPromotionStorage\(\s*"([^"]+)"', src))
    guard = set(re.findall(r'PromoteVtableGuarded\(\s*"([^"]+)"', src))
    got = direct | guard
    checks = [
        ("direct door seen", "?_typeInfo@Direct@UI@@0V@A" in got),
        ("GUARDED door seen (the Wave 25 blind spot)", "??_7Guarded@UI@@6B@" in got),
        ("comment stripped, not counted", "?_typeInfo@InAComment@UI@@0V@A" not in got),
        ("EFFECTIVENESS: both doors yielded something", bool(direct) and bool(guard)),
    ]
    bad = 0
    for name, ok in checks:
        print(("  GREEN   " if ok else "  RED     ") + name)
        bad += 0 if ok else 1
    live = mint_promoted_symbols(REPO)
    nvt = len([s for s in (live or ()) if s.startswith("??_7")])
    nti = len([s for s in (live or ()) if s.startswith("?_typeInfo@")])
    print("  LIVE FILE: %d vtable + %d typeInfo promoted symbol(s)" % (nvt, nti))
    if live is None or nvt == 0 or nti == 0:
        print("RESULT   : RED -- a live door yields ZERO; the scan has drifted")
        return 1
    print("RESULT   : %d of %d checks correct" % (len(checks) - bad, len(checks)))
    return 1 if bad else 0


def selftest_unpromotable_key():
    """Regression guard for BOTH defects in load_unpromotable()'s one regex.

    WAVE 25. The REASON CLASS field was `(\\S+)` (lane MANIFEST: a class naming
    two routes dropped the whole tag, 5 rows live) and the SYMBOL field was
    `(\\?\\S+)` (lane INTERNAL42: an address-keyed row cannot match at all,
    0 rows live but the `internal` bucket is 190 address-keyed rows).

    Both were invisible because a failed match drops the tag SILENTLY and the
    row then prints as ordinary unported work. Neither had a regression guard,
    and the address half is ABSENCE-shaped -- there is no live specimen, so it
    must be HANDED a positive case or it cannot be distinguished from a dead
    parser. That is what the two synthetic address rows below are for.
    """
    import re as _re
    src = open(__file__, encoding="utf-8").read()
    pat = _re.search(r'm = re\.search\((r"RT_UNPROMOTABLE.*?), line\)', src, _re.S)
    if not pat:
        print("RESULT   : CANNOT LOCATE THE REGEX -- this guard is DEAD"); return 1
    rx = _re.compile(eval("(" + pat.group(1).replace("\n", "") + ")"))

    def parse(line):
        m = rx.search(line)
        if not m:
            return None
        k = m.group(1)
        return ("0x%x" % int(k, 16)) if k[:2].lower() == "0x" else k

    T = "// RT_UNPROMOTABLE: %s | n/a | DOC.md | reason | %s"
    cases = [
        ("POSITIVE symbol, single-token class",
         T % ("?_singleton@Foo@@0PEAV12@EA", "HARMFUL"), "?_singleton@Foo@@0PEAV12@EA"),
        ("POSITIVE symbol, MULTI-token class (MANIFEST's live defect)",
         T % ("?_x@Bar@@0PEAV12@EA", "HARMFUL (DLL-present) / REQUIRED (cutover)"),
         "?_x@Bar@@0PEAV12@EA"),
        ("POSITIVE address, lowercase (INTERNAL42's latent twin)",
         T % ("0x18002ac40", "HARMFUL"), "0x18002ac40"),
        ("POSITIVE address, UPPERCASE -- must normalise to the row-set spelling",
         T % ("0x18002AC40", "NO-OP"), "0x18002ac40"),
        ("POSITIVE address, zero-padded -- must normalise",
         T % ("0x018002ac40", "NO-OP"), "0x18002ac40"),
        ("NEGATIVE no pipe fields -- must NOT match",
         "// RT_UNPROMOTABLE: 0x18002ac40 no pipes at all", None),
        ("NEGATIVE neither a symbol nor an address -- must NOT match",
         T % ("banana", "HARMFUL"), None),
    ]
    bad = 0
    parsed = 0
    for name, line, want in cases:
        got = parse(line)
        ok = got == want
        if want is not None and got is not None:
            parsed += 1
        print(("  GREEN   " if ok else "  RED     ") + name + " -> " + repr(got))
        bad += 0 if ok else 1
    # EFFECTIVENESS: a run where nothing parsed would print all-GREEN on the
    # negatives alone. Five positives MUST have produced a key.
    print("  EFFECTIVENESS: %d of 5 positive cases actually parsed a key" % parsed)
    if parsed != 5:
        print("RESULT   : DEAD PARSER -- negatives passed but positives parsed nothing"); return 1
    print("RESULT   : %d of %d cases correct" % (len(cases) - bad, len(cases)))
def selftest_ordinal_fold():
    """Controls for the ORDINAL FOLD-IN (Wave 25 lane INTERNAL42).

    SUBJECT: `load_export_ordinals()` and the fold rule that consumes it.
    It reads the shipped DLL's export directory and NOTHING from the tree.

    Why this exists at all: the fold's "unresolved" list is ABSENCE-SHAPED.
    Every ordinal this tree passes to ResolveOrdinal is nameable, so a real run
    prints `UNNAMEABLE: (none)` -- which is exactly what a DEAD LOOKUP prints
    too. Control 3 hands that path a positive shape so the two are
    distinguishable, per the rule a Wave 22 screen was caught by.
    """
    print("ordinal fold-in controls")
    print("SUBJECT : load_export_ordinals() + the fold rule. Reads the DLL's")
    print("          export directory only; reads NO source file.")
    print("=" * 78)
    omap = load_export_ordinals()
    bad = 0
    eff = 0                      # EFFECTIVENESS: assertions that ran at all

    def check(label, ok, detail):
        nonlocal bad, eff
        eff += 1
        if not ok:
            bad += 1
        print("%-5s %-46s %s" % ("ok" if ok else "FAIL", label, detail))

    # 1. POSITIVE, in-population. Each of these is an ordinal our own tree
    #    actually passes to ResolveOrdinal, with the name taken from
    #    standalone/resolve_standalone.cpp's kOrdinalNames -- an INDEPENDENT
    #    transcription of the same export directory, written three waves
    #    earlier by a different lane. If the two disagree, one is wrong and
    #    this control says which pair.
    expect = {
        443:  "??0Thread@System@@QEAA@I@Z",
        798:  "??2GroupDescription@UI@@SAPEAX_KPEAX@Z",
        2990: "?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z",
        2991: "?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z",
        3089: "?IsA@Base@Object@@QEBA_NI@Z",
        3214: "?Lock@Device@Video@@QEAAXXZ",
        4462: "?Unlock@Device@Video@@QEAAXXZ",
    }
    for ordn in sorted(expect):
        got = omap.get(ordn)
        check("ordinal %d resolves" % ordn,
              got is not None and got[0] == expect[ordn],
              (got[0] if got else "<NO ROW>"))

    # 2. The fold must MERGE, not duplicate: the name an ordinal resolves to
    #    has to be the same string `namespace_of` sees for the export row, or
    #    the fold would silently mint a second row instead of joining one.
    check("2990 folds into Object",
          omap.get(2990) and namespace_of(omap[2990][0]) == "Object",
          "namespace_of(...) = %s"
          % (namespace_of(omap[2990][0]) if omap.get(2990) else "n/a"))
    check("3214 folds into Video",
          omap.get(3214) and namespace_of(omap[3214][0]) == "Video",
          "namespace_of(...) = %s"
          % (namespace_of(omap[3214][0]) if omap.get(3214) else "n/a"))

    # 3. NEGATIVE / HANDED-IN SHAPE. The unresolved path has no natural
    #    positive case in this binary, so hand it one: an ordinal far past the
    #    end of the export directory MUST come back unnameable. If this
    #    returned a name, the map is fabricating rows; if the lookup were dead
    #    it would return None for control 1 too, so the pair separates
    #    "dead lookup" from "genuinely absent".
    check("synthetic ordinal 65535 is UNNAMEABLE", 65535 not in omap,
          "kept as its own `ordinal` row, as the fold's else-branch does")

    # 4. The map must be big enough to be the real directory and not a stub.
    check("export directory is fully read", len(omap) >= 4000,
          "%d named ordinals" % len(omap))

    print("=" * 78)
    print("EFFECTIVENESS : %d assertions evaluated, %d failed" % (eff, bad))
    print("NOT ESTABLISHED : this says nothing about whether any ordinal's")
    print("                  function is ported -- only about the NAME it")
    print("                  denotes. Portedness is decided by the three-door")
    print("                  test in the fold block, which this does not run.")
    return 1 if bad else 0


def selftest_classifier():
    print("classify_symbol() fixture table")
    print("SUBJECT : the classifier ONLY. Nothing here reads the tree or the DLL.")
    print("=" * 78)
    bad = 0
    seen = collections.Counter()
    for kind, key, want, why in CLASSIFIER_FIXTURES:
        got = classify_symbol(key, kind)[0]
        seen[got] += 1
        ok = got == want
        bad += 0 if ok else 1
        print("%-5s %-9s %-9s %s" % ("ok" if ok else "FAIL", want, got, key))
        print("          %s" % why)
    covered = sorted(seen)
    missing = [k for k in KIND_ORDER if k not in seen]
    print("=" * 78)
    print("COVERAGE : %d fixtures exercise %d of the %d buckets (%s)"
          % (len(CLASSIFIER_FIXTURES), len(covered), len(KIND_ORDER),
             ", ".join(covered)))
    if missing:
        print("           UNEXERCISED BUCKETS: %s -- no fixture reaches them, so"
              % ", ".join(missing))
        print("           this run says nothing about whether they work.")
    print("NEGATIVE : the malformed name must land in UNKNOWN -> %s"
          % ("GREEN" if classify_symbol("?BrokenName@NoTerminator",
                                        "export")[0] == "UNKNOWN" else "RED"))
    print("RESULT   : %d of %d fixtures correct"
          % (len(CLASSIFIER_FIXTURES) - bad, len(CLASSIFIER_FIXTURES)))
    return 1 if bad else 0


def namespace_of(mangled):
    """Outermost namespace of an MSVC mangled name: ?Method@Class@Namespace@@...

    The qualified name is a list of components, inner-to-outer, and the LAST
    one is the namespace. Parsing it needs the actual MSVC grammar, not a
    split on "@@", because of two rules that interact:

      * A NAME FRAGMENT is terminated by '@'.
      * A BACK-REFERENCE is a single digit indexing the fragments already seen
        in this name, and it is NOT terminated by '@'.
      * The component list itself is terminated by an empty component, i.e. a
        bare '@'.

    So the familiar "@@" is fragment-terminator plus list-terminator, and a
    name whose innermost component is a back-reference ends its scope with a
    SINGLE '@'. `?GetDescription@Achievement@1@QEBA?AVString@System@@XZ` is
    Achievement::Achievement::GetDescription returning a System::String — and
    splitting it on "@@" hands back "System", filing three Achievement methods
    under System. This walks the grammar instead.
    """
    if not mangled.startswith("?"):
        return "?"
    i, n = 1, len(mangled)
    if i < n and mangled[i] == "?":
        # Operator name: "?X" (??0 = ctor, ??1 = dtor, ??Y = operator+ ...) or
        # the extended "?_X" form. It is not a name fragment, so it must not
        # enter the back-reference table — skip it and start the scope list at
        # the next component.
        i += 1
        if i < n and mangled[i] == "_":
            i += 1
        i += 1
        fragments, resolved = [], [""]   # placeholder for the "method name"
    else:
        fragments, resolved = [], []
    while i < n:
        c = mangled[i]
        if c == "@":            # empty component: end of the qualified name
            break
        if c.isdigit():         # back-reference, one character, no terminator
            idx = int(c)
            resolved.append(fragments[idx] if idx < len(fragments) else c)
            i += 1
            continue
        j = mangled.find("@", i)
        if j < 0:
            return "?"          # unterminated fragment: not a name we can read
        frag = mangled[i:j]
        fragments.append(frag)
        resolved.append(frag)
        i = j + 1

    # resolved[0] is the method name; the rest are scopes, outermost last.
    scopes = resolved[1:]
    if not scopes:
        return "?"
    if scopes[-1] in TOTAL_FUNCTIONS:
        return scopes[-1]
    for comp in reversed(scopes):
        if comp in TOTAL_FUNCTIONS:
            return comp
    return "?"


def iter_sources():
    for top in SCAN_DIRS:
        root_dir = os.path.join(REPO, top)
        if not os.path.isdir(root_dir):
            continue
        for dirpath, dirnames, filenames in os.walk(root_dir):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
            for fn in filenames:
                if fn.endswith(SOURCE_EXT):
                    yield os.path.join(dirpath, fn)


# Files whose RegisterRuntime* calls are OPT-IN: their Register function only runs
# when an env var arms it and returns 0 otherwise, so the registrations are
# present in the source but NOT live. A census that counted them would say
# "System 95% ported" for a wave in which the allocator has never executed a
# single instruction -- the same overstatement as a diff site reporting
# divergences=0 on calls=0 (WAVE1.md defect 4). They are reported separately.
#
# WAVE 22 LANE ALLOC: this is now a MAPPING rather than a set, so the gate that
# holds a file back can be NAMED in the output. The membership tests around it
# (`rel in GATED_OFF_FILES`, `for f in sorted(GATED_OFF_FILES)`) read keys and
# are unchanged by the switch.
GATED_OFF_FILES = {
    "runtime/src/system/allocator.cpp":     "BANISHED_RT_ALLOC",
    "runtime/src/system/allocator_win.cpp": "BANISHED_RT_ALLOC",
}

# A GATED FILE CAN STILL HOLD AN UNGATED REGISTRATION, and this one does.
# Found 2026-09-11 by Wave 22 lane ALLOC, and found by the instrument that lane
# was asked to BUILD rather than by reading: the new GATED-BUT-WRITTEN join
# printed `internal / 0x180009c30 / BANISHED_RT_ALLOC`, which is a row this
# lane knew to be live on every run. The file-level rule above is COARSE -- it
# asks "which file?" and cannot ask "which Register function?" -- so it was
# wrong by one row, in the UNDERSTATING direction, for as long as that
# consolidation has existed.
#
#   0x180009c30 is FreeLockedIgnoringSelf, the two-argument Free wrapper whose
#   first argument is ignored. Wave 9 lane VID2 found it registered THREE times
#   by three lanes; Wave 10 consolidated it to one owner in allocator.cpp and
#   registered it DELIBERATELY UNCONDITIONALLY, from
#   RtSystemAllocatorRegisterSharedAddresses(), which rt_system_port.cpp:84
#   calls outside every wave switch. Its own comment says why: its callers span
#   the DLL-present path as well as cutover, and the two registrations it
#   replaced were not gated either.
#
# So this row IS a port and is now counted as one. That moves `internal` and
# TOTAL by exactly one row; it does NOT touch System, which is Wave 22's
# subject. Keep this list minimal: every entry is a claim that a specific
# registration in an opt-in file runs anyway, and it must be re-checked when
# that file's Register plumbing changes. Keys are the exported NAME or the
# lowercase `0x...` ADDRESS, exactly as the row is keyed.
GATED_OFF_EXCEPTIONS = {
    "runtime/src/system/allocator.cpp": frozenset({"0x180009c30"}),
}

# The same overstatement one level finer: a registration inside an `if (<flag>)`
# block that is OFF by default. GATED_OFF_FILES cannot see these because the
# file around them registers plenty of live ports too.
#
# Found 2026-09-07 while preparing wave 9, and found by the PROCESS, not by
# reading: the BANISHED_RT_STRICT residue census recorded four names arriving
# at Runtime-x64.dll that this script was calling ported --
# `??0Internal@Object@@QEAA@XZ`, `??1Internal@Object@@UEAA@XZ`,
# `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` and
# `?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z`. All four are
# in WAVE3.md's NO-GO handle-table tier, which is exactly why they are off.
#
# `table` LEFT THIS LIST 2026-09-09 (Wave 11 follow-up lane ARM). That gate is
# now DEFAULT-ON with `BANISHED_RT_INTERNAL_TABLE=0` as its kill switch, so its
# three registrations (`??0Internal@Object@@QEAA@XZ`,
# `??1Internal@Object@@UEAA@XZ`, `?ResetPersistState@InternalFactory@Object@@`)
# are live on every ordinary run and are ordinary ports. Leaving it here would
# have inverted this list's own purpose: instead of refusing to count an
# unarmed row it would have refused to count an armed one, and the census would
# have read exactly the same before and after the arming — which is what it did
# on this lane's first measurement, and is why this entry is mentioned in
# WAVE11F_LANE_ARM.md rather than quietly edited.
#
# `shadow` and `checkPtr` stay: shadow is an oracle tier that is opt-in by
# design, and checkPtr registers a body under BANISHED_RT_INTERNAL_CHECKPTR.
#
# Keyed by file -> the gate variables whose `if (<var>) { ... }` body is opt-in.
GATED_OFF_BLOCKS = {
    "runtime/src/object/internal.cpp": ("shadow", "checkPtr"),
}

# A FOURTH GATING SHAPE, and the first three cannot describe it either: a
# registration inside a function whose FIRST STATEMENT is a guard clause on a
# global that only the arming path sets.
#
#     extern "C" void RtAllocSwapPostRegister() {
#         if (!g_swapTaken) return;                 <-- the gate
#         RegisterRuntimeExport("?Allocate@MemoryAllocator@System@@...", ...);
#
# GATED_OFF_FILES cannot see it (the file registers plenty of live ports too),
# GATED_OFF_BLOCKS cannot see it (there is no `if (<flag>) { ... }` around the
# call), and it is not the mint. So `?Allocate@` and `?Free@` have been counted
# PORTED while their ONLY registrations -- and their only two `[port]` detours,
# in RtAllocSwapInstall, which the arming path is likewise the sole caller of --
# run only under BANISHED_RT_ALLOC.
#
# Found 2026-09-11 by Wave 22 lane ALLOC, as the MIRROR of the understatement
# that GATED_OFF_EXCEPTIONS fixes: having found the census wrong by one row in
# the understating direction, the lane asked the same question in the other
# direction on the same subject and found it wrong by two.
#
# THIS LIST DELIBERATELY MOVES NO COUNT. It drives a REPORT ONLY, for the
# reason this script states about its own unclassified-door screen: these two
# rows are in System, System is Wave 22's subject, and silently growing its gap
# from 25 to 27 mid-wave would move a number five lanes are quoting. The screen
# names them; an integrator decides. If that decision is taken, the honest
# implementation is span-aware gating in scan(), not an entry here.
#
# file -> {function name: what actually gates it}
GATED_OFF_FUNCS = {
    "standalone/rt_alloc_swap.cpp": {
        "RtAllocSwapPostRegister": "BANISHED_RT_ALLOC (guard: if (!g_swapTaken) return;)",
        "RtAllocSwapInstall":      "BANISHED_RT_ALLOC (only the arming path calls it)",
    },
}


def gated_func_keys(repo):
    """Keys registered or detoured inside a GATED_OFF_FUNCS body.

    Brace-matched from the function's opening `{`, on the comment-STRIPPED text,
    so a `}` inside a comment or a string cannot end the body early -- the
    defect class that made a previous checker pair across function boundaries.
    Returns {key: (file, function, gate)}; an unreadable file yields nothing and
    says so, rather than a silent zero.
    """
    out, unread = {}, []
    for rel, funcs in GATED_OFF_FUNCS.items():
        path = os.path.join(repo, rel)
        try:
            text = strip_comments(open(path, encoding="utf-8", errors="replace").read())
        except OSError:
            unread.append(rel)
            continue
        for fn, gate in funcs.items():
            m = re.search(r'\b%s\s*\(' % re.escape(fn), text)
            if not m:
                unread.append("%s::%s (signature not found)" % (rel, fn))
                continue
            i = text.find("{", m.end())
            if i < 0:
                unread.append("%s::%s (no body)" % (rel, fn))
                continue
            depth, j = 0, i
            while j < len(text):
                if text[j] == "{":
                    depth += 1
                elif text[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            body = text[i:j]
            for r in RE_REG_EXPORT.finditer(body):
                nm = concat_literal(body, r.end())
                if nm:
                    out[nm] = (rel, fn, gate)
            for r in RE_REG_ADDR.finditer(body):
                out["0x%x" % int(r.group(1), 16)] = (rel, fn, gate)
            for r in RE_DETOUR_ADDR.finditer(body):
                out.setdefault("0x%x" % int(r.group(2), 16), (rel, fn, gate))
    return out, unread


# The MINT is a third gating shape and the first two could not describe it.
#
# standalone/rt_data_mint.cpp registers the exported DATA symbols -- ??_7
# vtables, ?_typeInfo@ BaseTypeInfo, and the .data statics -- by LOOPING over
# the generated tables in rt_data_mint_tables.inc, so the names reach
# RegisterRuntimeExport as `v.symbol` / `t.symbol` / `s.symbol`.
#
# BEFORE THIS, THE CENSUS REPORTED THEM UNDER THE NON-LITERAL WARNING, whose
# text is "These ports are LIVE in the process but INVISIBLE to this census, so
# every number above UNDERSTATES what is actually registered." **Both halves of
# that were wrong here.** They are not live -- RegisterMintedData() runs only
# under BANISHED_RT_MINT=1, and this script's own rule is that "a gated
# registration is not a port". And they are not an understated PORT count at
# all: a vtable and a BaseTypeInfo have no body to reimplement, which is the
# whole premise of rt_data_mint.h. The warning was pointing a reader at a
# shortfall that does not exist while saying nothing about the 197 real data
# symbols that are genuinely not live yet.
#
# So the mint gets its own line, counted from the generated table where every
# symbol IS a literal, and it is kept OUT of the non-literal warning.
MINT_FILE   = "standalone/rt_data_mint.cpp"
MINT_TABLES = "standalone/rt_data_mint_tables.inc"

# Wave 10 lane MINTSEL made registration PER-ROW, and this file is the list of
# rows registered unconditionally on every run. Those rows are ordinary ports
# and are already counted as such by the main scan above: the file spells each
# symbol as a literal at its RegisterRuntimeExport call site precisely so that
# no tool change is needed to see them. It is read here only to keep the mint's
# own summary line honest about how many rows are still unregistered.
MINT_PROMOTED_FILE = "standalone/reg_mint_promoted.cpp"


def mint_promoted_symbols(repo):
    """Minted rows promoted to unconditional registration, as a SET of symbols.

    Returns None -- not an empty set -- when the file cannot be read, so a
    caller can distinguish "nothing is promoted" from "I could not look". The
    vtable fold-in below depends on that distinction: an unreadable file would
    otherwise silently demote every promoted row and read as a clean zero.
    """
    path = os.path.join(repo, MINT_PROMOTED_FILE.replace("/", os.sep))
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None
    # WAVE 25 INTEGRATION -- the FIFTH recorded instance of this project's most
    # repeated blind spot: an instrument that greps for a LITERAL is blind to
    # the same fact spelled through a VARIABLE, and never announces it.
    #
    # Lane SWEEP routed all twelve promoted rows through ONE
    # `PromoteVtableGuarded(symbol, va, &n)` helper so a row REFUSES itself
    # rather than stamping abort traps into a live vptr. That is strictly
    # better engineering. But the helper calls RtMintPromotionStorage with a
    # VARIABLE, so this scan stopped seeing eleven of the twelve promotions.
    # Measured at integration:
    #
    #     12 PROMOTED -> 1, the vtable fold-in 151 rows -> 9, and the BOARD
    #     read 1333/1185 88.9% -> 1222/1074 87.9%, the gap GROWING 137 -> 148.
    #
    # Nothing was unported. A refactor that touched no port erased 111 rows
    # from the denominator -- and it was caught ONLY because it pointed the
    # unflattering way. A version of this that raised the percentage would
    # have been believed and quoted.
    #
    # Both doors are matched. The literal still lives at each
    # PromoteVtableGuarded call site, so this stays a source scan;
    # --selftest-promoted-doors fails if either door stops yielding.
    src = strip_comments(text)
    return (set(re.findall(r'RtMintPromotionStorage\(\s*"([^"]+)"', src)) |
            set(re.findall(r'PromoteVtableGuarded\(\s*"([^"]+)"', src)))


def mint_promoted_count(repo):
    """Minted rows promoted to unconditional registration."""
    syms = mint_promoted_symbols(repo)
    return 0 if syms is None else len(syms)


# The three tables whose rows ARE the minted symbols. Everything else in the
# .inc that looks like a mangled name is a vtable SLOT TARGET -- the function
# each slot points at -- and those are ordinary members, not minted data. A
# first cut at this counted every mangled string in the file and reported 637
# where the truth is 197; the generated header says "55 vtables, 104
# BaseTypeInfo, 38 storage symbols" and disagreeing with it by 3x is exactly the
# kind of number that gets quoted once and then believed.
MINT_TABLE_ARRAYS = ("kMintVtables", "kMintTypeInfos", "kMintStorage")


def mint_symbol_count(repo):
    """Minted exported-data symbols, counted from the generated tables.

    Counted from the .inc rather than the .cpp because that is where the names
    are; the .cpp only walks them. Only rows INSIDE the three symbol tables are
    counted -- see MINT_TABLE_ARRAYS."""
    path = os.path.join(repo, MINT_TABLES.replace("/", os.sep))
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        # (0, 0), not 0: every caller unpacks two values, and returning a bare
        # int made the census CRASH instead of reporting an unreadable table.
        # Found by exercising this instrument's own "I MEASURED NOTHING" path,
        # which is why that path is worth having.
        return 0, 0

    total, named = 0, 0
    for arr in MINT_TABLE_ARRAYS:
        m = re.search(r'\b' + arr + r'\s*\[\s*\]\s*=\s*\{', text)
        if not m:
            continue
        # Brace-match the initialiser so a row count cannot run off the end of
        # the table into the next one.
        i, depth = m.end() - 1, 0
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[m.end():i]
        # Rows are `{ ... }` at the table's own nesting level. A row whose first
        # field is a STRING carries an exported name; a row whose first field is
        # `nullptr` is an UNEXPORTED slot the mint knows only by address.
        #
        # Both are real and they are not interchangeable, so both are returned.
        # Counting only the named ones gave 193 against the generated header's
        # 197 -- right direction, wrong amount, which is the shape that reads as
        # success and hides four rows. The four are the nullptr-symbol slots.
        total += len(re.findall(r'\{\s*(?:"|nullptr)', body))
        named += len(re.findall(r'\{\s*"', body))
    return total, named


# ===========================================================================
# THE MINTED-VTABLE SLOT DOOR -- blind spot EIGHT (Wave 20 Phase 0a)
# ===========================================================================
# THE BUG. This script builds its row set by ITERATING `called_exports` and
# `called_addrs`. `ported` is therefore exactly |called INTERSECT registered|,
# and a REGISTRATION WITH NO CALL DOOR NEVER BECOMES A ROW AT ALL. A body whose
# only route into execution is a MINTED VTABLE SLOT has no ResolveExport, no
# RT_EXPORT_ONCE, no RT_DATA_SLOT and no ResolveAddr anywhere in the tree --
# the mint reaches it by asking the PORT REGISTRY for it by name, inside a loop
# over the generated tables (rt_data_mint.cpp OurPort()/MintVtables()). So the
# work that makes a vtable row true was invisible to the number this project
# treats as its authority. MEASURED: lane CSTREAM (Wave 19) registered twelve
# exports and `ported` moved +3; the other nine were vtable slot bodies.
#
# THE PRECEDENT, and it is twelve lines from where this is used. Wave 10 hit
# the identical root cause with UI::TabElement::UpdateContainers and folded
# DETOUR TARGETS into the row set as called+ported+detoured. Its justification,
# quoted verbatim from main() below:
#
#     "`called` counts functions our tree resolves through a door
#      (ResolveExport / ResolveAddr / an ordinal). A detour creates callers a
#      different way -- it patches the DLL, so RETAIL'S OWN internal callers
#      arrive at our body -- and those callers are the entire point of
#      installing one. [...] So a detour target is folded in here as called +
#      ported + detoured. This is not crediting unrun code: the detour is what
#      makes it run."
#
# A MINTED VTABLE CREATES CALLERS THE SAME THIRD WAY. It publishes OUR table
# under the DLL's exported `??_7` symbol, so every object constructed against
# that symbol dispatches its virtual calls into whatever the mint put in the
# slots -- our port if RtPortLookupExport found one, a per-slot trap stub if it
# did not. Same argument, same place in the file.
#
# ---------------------------------------------------------------------------
# THE RULING: ONLY A **PROMOTED** ROW'S SLOTS ARE FOLDED IN. Both numbers are
# printed, because both are wanted and they mean different things.
# ---------------------------------------------------------------------------
# WHY. `MintVtables()` fills the static kVTableN arrays on every run, but
# FILLING A TABLE IS NOT PUBLISHING IT. The `RegisterRuntimeExport("??_7X@",
# table)` that makes `RT_DATA_SLOT("??_7X@")` hand our table to a constructor
# happens in exactly two places:
#
#   * `RegisterMintedData()` -- gated behind BANISHED_RT_MINT=1, which by this
#     script's own oldest rule (GATED_OFF_FILES: "a gated registration is not
#     a port") is NOT live. It is also not merely off-by-default: with the DLL
#     present it publishes our zeroed MemoryAllocator singleton and dies.
#   * `RtMintRegisterPromotedRows()` (standalone/reg_mint_promoted.cpp) --
#     unconditional, with BANISHED_RT_MINT_PROMOTED=0 as a kill switch. That is
#     the ordinary default-on shape this project already counts as a port.
#
# WHICH PROMOTION COUNT THIS IS -- there are TWO SUBJECTS and they are not the
# same question (integrator correction C2, Wave 20):
#
#   STATIC   what reg_mint_promoted.cpp REGISTERS IN SOURCE. Derived here on
#            every run by mint_promoted_symbols(), never cited from a document.
#            This is the subject the census measures, because the census is a
#            source scan and cannot observe a process.
#   RUNTIME  what a PARTICULAR RUN registered. Differs whenever
#            BANISHED_RT_MINT_PROMOTED=0 is set (reg_mint_promoted.cpp:173
#            returns 0 before registering anything) -- in which case the STATIC
#            count is an OVERSTATEMENT of that run and every folded row is
#            dispatching to the DLL instead. The census cannot see an env var
#            and does not pretend to; the header says which subject it means.
#
# DO NOT HARD-CODE OR CITE A PROMOTED COUNT ANYWHERE. The figure carried by
# WAVE19_CLOSED.md §7.5a and WAVE20_GUIDE.md §2 ("5 of 62") was itself already
# stale when written: lane IOBASE promoted five IO vtables in 9f2edbc on the
# last day of Wave 19, making it 10. A count that corrected another count and
# then went stale in the same wave is the strongest possible argument for
# deriving this one at run time, which is what happens here.
#
# For an UNPROMOTED row, `RT_DATA_SLOT` falls through to the DLL, our objects
# get RETAIL'S table, and RETAIL'S BODIES RUN. Wave 19 measured this directly:
# lane FILE's ten bodies read TRAP=0 with ATTENDANCE ZERO on every one. Folding
# those slots in as `ported` would encode WAVE20_GUIDE.md §5 item 2 -- "reading
# TRAP=0 as progress-in-play" -- into the tool, and the identity would then
# credit a row for being READY rather than for being a route. READY is not a
# route. So an unpromoted row's slot bodies are reported on their own line, as
# the PROMOTION BACKLOG, and move no column.
#
# WHAT THE FOLD-IN DOES AND DOES NOT CLAIM. It claims a DISPATCH ROUTE EXISTS
# on every ordinary run: our table is published under the symbol the DLL
# exports, and the slot holds our body rather than a trap. It does NOT claim
# ATTENDANCE -- whether any constructor in a given run actually asks for that
# symbol is a separate question and nothing here observes a call. That is
# exactly the tier the detour fold-in has carried since Wave 10, and it is
# written down here rather than left to be inferred.
#
# CONSEQUENCE THE COMMISSIONING NOTE DID NOT ANTICIPATE, stated so nobody has
# to rediscover it. Lane CSTREAM's nine invisible bodies are slots of
# ??_7CompressedStream@ / ??_7CompressedFileStream@ / ??_7CompressedMemoryStream@,
# and all three are UNPROMOTED. Under this ruling they STILL do not move
# `ported` -- but they are no longer invisible: they are counted, by name, on
# the PROMOTION BACKLOG line. That is the honest answer rather than the
# flattering one. The lesson of Wave 19 §7.5 is that those bodies do not run,
# and a census that said they did would be repeating the error, not fixing it.
# The backlog line is what a DATA lane should now be scored on, and promoting
# the row is what converts it into `ported`.
#
# UNPORTED SLOTS OF A PROMOTED ROW ARE FOLDED IN AS `called` AND NOT `ported`,
# which is the honest reading and not a rounding: the published table holds a
# TRAP in that slot, so a dispatch through it aborts. That is precisely the
# state `called but not ported` exists to name.
#
# PURECALL slots are skipped: there is no body to port.

RE_MINT_VSLOT_ARRAY = re.compile(
    r'static\s+const\s+RtMintVSlot\s+(kVSlots\d+)\s*\[\s*\]\s*=\s*\{')
RE_MINT_VSLOT_ROW = re.compile(
    r'\{\s*(?:"([^"]*)"|nullptr)\s*,\s*(0x[0-9a-fA-F]+)ull\s*,\s*(RT_MINT_\w+)\s*\}')
# A kMintVtables row: { symbol|nullptr, dllVa, slotCount, kVSlotsN, ... }
RE_MINT_VTABLE_ROW = re.compile(
    r'\{\s*(?:"([^"]*)"|nullptr)\s*,\s*(0x[0-9a-fA-F]+)ull\s*,\s*(\d+)\s*,\s*'
    r'(kVSlots\d+)\s*,')


def load_mint_vtables(repo):
    """[(vtableSymbol|None, dllVa, [(slotName|None, slotVa, kind), ...]), ...].

    Parsed from the GENERATED table, which is where the names are, exactly as
    mint_symbol_count() does. Returns None -- not [] -- when the file cannot be
    read, so "I could not look" stays distinguishable from "there is nothing
    there"; the caller prints the difference.
    """
    path = os.path.join(repo, MINT_TABLES.replace("/", os.sep))
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None

    # slot arrays first: name -> rows, brace-matched so one array cannot bleed
    # into the next.
    slots_by_array = {}
    for m in RE_MINT_VSLOT_ARRAY.finditer(text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[m.end():i]
        slots_by_array[m.group(1)] = [
            (r.group(1) or None, int(r.group(2), 16), r.group(3))
            for r in RE_MINT_VSLOT_ROW.finditer(body)]

    m = re.search(r'\bkMintVtables\s*\[\s*\]\s*=\s*\{', text)
    if not m:
        return []
    i, depth = m.end() - 1, 0
    while i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                break
        i += 1
    tbody = text[m.end():i]

    out = []
    for r in RE_MINT_VTABLE_ROW.finditer(tbody):
        sym, va, count, arr = r.group(1) or None, int(r.group(2), 16), \
            int(r.group(3)), r.group(4)
        # slotCount is the generator's own proven extent; the array may be
        # longer only through a generator bug, so truncate to the count rather
        # than trusting whichever is bigger.
        out.append((sym, va, slots_by_array.get(arr, [])[:count]))
    return out


def _add_detour(ports, instruments, label, key, rel):
    """File a detour under `ports` or `instruments` according to its label.

    A label is trusted only when it carries the `[port]` marker the tree uses
    for a real takeover. Anything else -- including an UNLABELLED one -- is an
    instrument, because the safe default here is to under-credit: a probe
    counted as a port silently inflates the headline number, while a port
    counted as a probe shows up as a missing row somebody chases.
    """
    bucket = ports if label.endswith("[port]") else instruments
    bucket.setdefault(key, set()).add(rel)


def gated_block_spans(text, gate_vars):
    """Byte spans of every `if (<gate>) { ... }` body, brace-matched.

    Brace matching rather than a regex because the bodies contain both braces
    and strings; strip_comments() has already run, so only string literals can
    hide a brace, and those are skipped here."""
    spans = []
    for var in gate_vars:
        for m in re.finditer(r'\bif\s*\(\s*%s\s*\)\s*\{' % re.escape(var), text):
            i = m.end() - 1
            depth = 0
            while i < len(text):
                ch = text[i]
                if ch == '"':
                    lit = STRING_LITERAL.match(text, i)
                    i = lit.end() if lit else i + 1
                    continue
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                    if depth == 0:
                        spans.append((m.start(), i))
                        break
                i += 1
    return spans


def gated_func_spans(text, funcs):
    """Byte spans of the bodies of the GATED_OFF_FUNCS functions in this file.

    Same brace-matching discipline as gated_block_spans, and for the same
    reason: strip_comments() has already run, so only a string literal can hide
    a brace, and those are skipped. `funcs` is {name: gate}; only the names are
    read here.

    A function whose signature is not found yields NO span, which is the safe
    direction (nothing is gated) but is SILENT -- gated_func_keys() is the
    screen that says so out loud, and it is printed for exactly that reason.
    """
    spans = []
    for fn in funcs:
        for m in re.finditer(r'\b%s\s*\(' % re.escape(fn), text):
            i = text.find("{", m.end())
            if i < 0:
                continue
            # A forward DECLARATION has a ';' before any '{'; skip it, or the
            # span would run from the declaration to the end of some unrelated
            # later function.
            semi = text.find(";", m.end())
            if 0 <= semi < i:
                continue
            depth = 0
            while i < len(text):
                ch = text[i]
                if ch == '"':
                    lit = STRING_LITERAL.match(text, i)
                    i = lit.end() if lit else i + 1
                    continue
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                    if depth == 0:
                        spans.append((m.start(), i))
                        break
                i += 1
    return spans


def strip_comments(text):
    """Blank out C/C++ comments, preserving length so byte offsets still line up.

    Added 2026-09-06 (Wave 5). The census reported a port row literally named
    `<literal>` -- a function that does not exist -- because four source files
    document this tool's own match pattern inside a comment. The regex matched
    the comment text. One fake row in the tool this project treats as
    authoritative over any agent's "all N ported" claim is exactly the kind of
    small wrongness that makes the true rows less believable, so it is fixed
    here rather than by rewording the four comments: a reworded comment fixes
    the four that exist, this fixes the fifth nobody has written yet.

    Comment bodies are replaced with SPACES rather than deleted so that every
    match offset stays valid for concat_literal(), which reads forward from
    m.end() to join adjacent string literals.

    String and character literals are tracked, so a "//" or "/*" inside one (a
    URL, a Windows path) is not mistaken for the start of a comment.
    """
    BACKSLASH = chr(92)
    out = list(text)
    i, n = 0, len(text)
    state = None          # None | '"' | "'" | '//' | '/*'
    while i < n:
        c = text[i]
        if state is None:
            if c == '"' or c == "'":
                state = c
            elif c == '/' and i + 1 < n and text[i + 1] == '/':
                state = '//'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
            elif c == '/' and i + 1 < n and text[i + 1] == '*':
                state = '/*'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
        elif state == '"' or state == "'":
            if c == BACKSLASH:
                i += 2
                continue
            if c == state:
                state = None
        elif state == '//':
            if c == '\n':
                state = None
            else:
                out[i] = ' '
        elif state == '/*':
            if c == '*' and i + 1 < n and text[i + 1] == '/':
                out[i] = out[i + 1] = ' '
                state = None
                i += 2
                continue
            if c != '\n':
                out[i] = ' '
        i += 1
    return ''.join(out)


def scan():
    called_exports, called_addrs, called_ordinals = {}, {}, {}
    ported_exports, ported_addrs, ported_ordinals = {}, {}, {}
    gated_exports, gated_addrs = {}, {}
    detoured = {}
    nonliteral_regs = {}   # file -> [identifier, ...] the census cannot resolve
    nonliteral_calls = {}  # same, for ResolveExport CALL sites
    nonliteral_detours = {}  # same, for InstallRuntimeDetour* targets
    instrument_detours = {}  # probe:/selftest:/[allocshadow] -- NOT ports
    nfiles = 0               # the SUBJECT's size: what this scan actually read

    for path in iter_sources():
        nfiles += 1
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue

        # Match against CODE only. A comment that quotes this tool's own match
        # pattern used to produce a phantom port row -- see strip_comments().
        text = strip_comments(text)

        # Same-file `const char* const kX = "?Mangled@..."` table. Built HERE,
        # before the call scans, because BOTH the call side and the
        # registration side need it -- see RE_EXPORT_NONLITERAL.
        name_consts = {}
        for m in RE_NAME_CONST.finditer(text):
            lit = concat_literal(text, m.end())
            if lit:
                name_consts[m.group(1)] = lit

        for m in RE_EXPORT.finditer(text):
            name = concat_literal(text, m.end())
            if name:
                called_exports.setdefault(name, set()).add(rel)
        for m in RE_EXPORT_NONLITERAL.finditer(text):
            ident = m.group(1)
            if ident in name_consts:
                called_exports.setdefault(name_consts[ident], set()).add(rel)
            elif ident not in ("mangled", "name", "sym", "symbol", "mangledName"):
                # Those are PARAMETER names in the declarations and definitions
                # of ResolveExport and of InstallRuntimeDetourExport, forwarding
                # a caller's string -- not call sites naming a symbol.
                nonliteral_calls.setdefault(rel, []).append(ident)
        # The macro doors (RT_EXPORT_ONCE / RT_DATA_SLOT / RtResolveCached).
        # These are live cutover dependencies exactly as ResolveExport is; see
        # RE_MACRO_DOOR above for what their invisibility cost.
        for m in RE_MACRO_DOOR.finditer(text):
            lit = first_literal_after(text, m.end())
            if lit is None:
                continue
            name = concat_literal(text, lit)
            if name:
                called_exports.setdefault(name, set()).add(rel)
        for m in RE_RESOLVE_CACHED.finditer(text):
            name = concat_literal(text, m.end())
            if name:
                called_exports.setdefault(name, set()).add(rel)
        if rel.startswith("dx11/"):
            for m in RE_DX11_MANGLED.finditer(text):
                called_exports.setdefault(m.group(1), set()).add(rel)
        for m in RE_ORDINAL.finditer(text):
            called_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_ADDR.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)
        for m in RE_ADDR_RAW.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)

        # Registrations inside an opt-in `if (<flag>)` block are gated the same
        # way an opt-in FILE is -- see GATED_OFF_BLOCKS.
        block_spans = gated_block_spans(text, GATED_OFF_BLOCKS.get(rel, ()))
        # ...and so is one inside a FUNCTION whose first statement is a guard
        # clause on a global only the arming path sets -- the fourth gating
        # shape, see GATED_OFF_FUNCS. Same predicate, same effect on the count.
        block_spans += gated_func_spans(text, GATED_OFF_FUNCS.get(rel, {}))
        def in_gated_block(pos):
            return any(a <= pos <= b for a, b in block_spans)
        # A registration inside an OPT-IN file is NOT a live port: its
        # Register function is only called when an env var arms it, and it
        # returns 0 otherwise. Counting it would repeat the exact overstatement
        # this census exists to prevent -- see GATED_OFF_FILES.
        gated = rel in GATED_OFF_FILES
        # ...EXCEPT for the keys GATED_OFF_EXCEPTIONS names. The file-level rule
        # is COARSE and was wrong by one row; see the table's own banner.
        exempt = GATED_OFF_EXCEPTIONS.get(rel, frozenset())

        for m in RE_REG_EXPORT.finditer(text):
            name = concat_literal(text, m.end())
            if name:
                g = (gated and name not in exempt) or in_gated_block(m.start())
                (gated_exports if g else ported_exports).setdefault(name, set()).add(rel)
        for m in RE_REG_EXPORT_NONLITERAL.finditer(text):
            # `const` is the parameter type in the DECLARATIONS of
            # RegisterRuntimeExport itself (rt_port.h, resolve.h,
            # resolve_standalone.cpp), not a call passing a constant.
            if m.group(1) == "const":
                continue
            # The MINT is gated DATA, not an unseen port -- see MINT_FILE. It
            # gets its own reported line; leaving it here made the warning claim
            # the port numbers were understated, which was false in both
            # directions (not live, and not functions).
            if rel == MINT_FILE:
                continue
            # rt_selftest.cpp deliberately registers a FAKE name and one real
            # one to prove the seam works end to end. It is a self-test, not a
            # port, and counting it would be wrong -- so it is excluded rather
            # than left to emit a permanent warning that trains readers to
            # ignore this block.
            if rel == "standalone/rt_selftest.cpp":
                continue
            resolved = name_consts.get(m.group(1))
            if resolved:
                g = gated or in_gated_block(m.start())
                (gated_exports if g else ported_exports).setdefault(
                    resolved, set()).add(rel)
                continue
            nonliteral_regs.setdefault(rel, []).append(m.group(1))
        for m in RE_REG_ORDINAL.finditer(text):
            ported_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_REG_ADDR.finditer(text):
            a = int(m.group(1), 16)
            g = (gated and ("0x%x" % a) not in exempt) or in_gated_block(m.start())
            (gated_addrs if g else ported_addrs).setdefault(a, set()).add(rel)

        # NOT EVERY DETOUR IS A PORT DETOUR, and counting them together
        # credited two instruments as ports.
        #
        # The tree already distinguishes them by label, and consistently:
        #   "...[port]"          a reimplementation taking over retail's callers
        #   "probe:..."          a recorder installed to observe a call
        #   "selftest:..."       a deliberate fake proving the seam works
        #   "...[allocshadow]"   an allocator-shadow instrument
        #
        # Only the first is what the `detour` column means ("of those PORTED,
        # ALSO patched so the DLL's own callers come to us"). Before this,
        # `probe:Audio::Device::Stop` -- a detour to a RECORDER, which is a test
        # fake by construction -- counted as a detour, and once detour targets
        # started conferring `ported` it would have counted as a port too. An
        # instrument is not a port, and a census that says otherwise is the
        # thing this file exists to prevent.
        #
        # A DETOUR INSIDE A GATED FUNCTION IS NOT INSTALLED EITHER, and a
        # detour target confers `ported`. Skipping it here is what stops the
        # count from being restored through the back door after the
        # registration side has been correctly excluded: `?Allocate@` and
        # `?Free@` are registered ONLY in RtAllocSwapPostRegister and detoured
        # ONLY in RtAllocSwapInstall, so gating one without the other would
        # have moved nothing and looked like it had. (Wave 22 lane ALLOC.)
        for m in RE_DETOUR_ADDR.finditer(text):
            if in_gated_block(m.start()):
                continue
            _add_detour(detoured, instrument_detours, m.group(1),
                        int(m.group(2), 16), rel)
        for m in RE_DETOUR_EXPORT.finditer(text):
            if in_gated_block(m.start()):
                continue
            _add_detour(detoured, instrument_detours, m.group(1),
                        m.group(2), rel)

        # Detour targets spelled as a same-file constant. Resolved rather than
        # punished, exactly as RE_NAME_CONST does for mangled names -- naming
        # the address once and using it at both the install site and the byte
        # guard is good practice, and a census that could not follow it printed
        # `detour 0` for a detour that was demonstrably installed.
        addr_consts = {mm.group(1): int(mm.group(2), 16)
                       for mm in RE_ADDR_CONST.finditer(text)}
        for m in RE_DETOUR_NONLITERAL.finditer(text):
            ident = m.group(2)
            if ident in addr_consts:
                _add_detour(detoured, instrument_detours, m.group(1),
                            addr_consts[ident], rel)
            else:
                nonliteral_detours.setdefault(rel, []).append(ident)

        # The byte-copy installer (blind spot seven). Address FIRST, label LAST,
        # so the label is the last string literal in the argument list.
        for target, label in bytecopy_detour_calls(text):
            if target.startswith("0x"):
                _add_detour(detoured, instrument_detours, label,
                            int(target, 16), rel)
            elif target in addr_consts:
                _add_detour(detoured, instrument_detours, label,
                            addr_consts[target], rel)
            else:
                nonliteral_detours.setdefault(rel, []).append(target)

    return dict(
        nfiles=nfiles,
        nonliteral_regs=nonliteral_regs,
        nonliteral_calls=nonliteral_calls,
        nonliteral_detours=nonliteral_detours,
        instrument_detours=instrument_detours,
        gated_exports=gated_exports, gated_addrs=gated_addrs,
        called_exports=called_exports, called_addrs=called_addrs,
        called_ordinals=called_ordinals, ported_exports=ported_exports,
        ported_addrs=ported_addrs, ported_ordinals=ported_ordinals,
        detoured=detoured,
    )



# ---------------------------------------------------------------------------
# UNPROMOTABLE rows (Wave 10E lane FINAL's proposal, accepted by the integrator)
#
# A `??_7` / `?_typeInfo@` row can be PORTED IN FULL and still never promotable,
# because nothing could ever dispatch through it. Such a row is not unfinished
# work, but it sits in "Called but not ported" looking identical to a function
# nobody has written -- and two waves of route-hunting is what that cost.
#
# THIS FUNCTION CHANGES NO COUNT. `total`, `called`, `ported`, `detour` and
# `called covered` keep exactly their previous arithmetic, so every cross-wave
# comparison stays valid. It only adds a per-row TAG and a derived `ceiling`.
#
# The source of truth is standalone/reg_mint_promoted.cpp, NOT a heuristic here:
# a row is unpromotable because a lane PROVED it and wrote the proof down.
# ---------------------------------------------------------------------------
def load_unpromotable(root):
    """Parse RT_UNPROMOTABLE: lines out of standalone/reg_mint_promoted.cpp."""
    path = os.path.join(root, "standalone", "reg_mint_promoted.cpp")
    out = {}
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                # The 5th field (REASON CLASS) is optional and defaults to
                # NO-OP, so every pre-existing 4-field line keeps its exact
                # meaning. It exists because Wave 12 hit a row that is
                # unpromotable for the OPPOSITE reason and the two must not
                # share one word:
                #
                #   NO-OP    promotion could not change behaviour on any route
                #            that could ever exist (the Wave 10E vtable rows:
                #            the value is stamped and overwritten unread).
                #   HARMFUL  promotion WOULD change behaviour, for the worse.
                #            Application::_singleton is read by 26 in-DLL
                #            RIP-relative sites that are not doors, so minting
                #            our own storage splits the brain.
                #
                # Collapsing these into one tag would have made "unpromotable"
                # mean both "harmless to promote" and "dangerous to promote",
                # which is the sort of conflation this project keeps paying for.
                # WAVE 25 lane MANIFEST -- the REASON CLASS field was `(\S+)`,
                # i.e. ONE whitespace-free token. Five rows spell their class
                # as "HARMFUL (DLL-present) / REQUIRED (cutover)", and for
                # those the optional group could not match, which made the
                # WHOLE expression fail and dropped the tag entirely. The five
                # rows then printed as ORDINARY unported work.
                #
                # The rows it hid were the worst possible five: a class that
                # names both routes is precisely a lane declaring that the
                # verdict INVERTS at cutover, so the instrument was blind to
                # exactly the tags that said the most. Measured: 41 matches
                # before, 46 after, with a positive control on both a
                # single-token and a multi-token class.
                # WAVE 25 lane INTERNAL42 -- the TWIN of the defect above, one
                # field to the LEFT and in the same two lines. The SYMBOL key
                # was `(\?\S+)`, anchored on a literal `?`. A space cannot
                # truncate it (every mangled name starts with `?`), but an
                # ADDRESS-KEYED row cannot match it AT ALL: write
                # `RT_UNPROMOTABLE: 0x18002ac40 | ...` and the whole expression
                # fails and the tag is dropped -- the identical symptom, from a
                # different cause.
                #
                # LATENT, NOT LIVE, and that was measured rather than assumed:
                # every RT_UNPROMOTABLE: line in reg_mint_promoted.cpp today
                # carries a `?`-prefixed symbol, so 0 rows were affected. But
                # the population that hits it is the whole `internal` bucket --
                # 190 rows, every one keyed by an address -- so the FIRST
                # address-keyed row anyone tags would have disappeared silently.
                #
                # It is TWO changes, not one: the capture must admit `0x...`
                # AND the key must then be spelled the way the row set spells
                # it ("0x%x" % addr, lowercase and unpadded, as at line 1783),
                # or the tag parses and then matches nothing -- which would
                # look exactly like the bug it replaced.
                m = re.search(r"RT_UNPROMOTABLE:\s*((?:\?\S+)|(?:0[xX][0-9a-fA-F]+))\s*\|"
                              r"\s*([^|]+?)\s*\|"
                              r"\s*([^|]+?)\s*\|\s*([^|]+?)\s*(?:\|\s*(\S.*?)\s*)?$", line)
                if m:
                    key = m.group(1)
                    if key[:2].lower() == "0x":
                        key = "0x%x" % int(key, 16)   # normalise to the row-set spelling
                    out[key] = (m.group(2), m.group(3), m.group(4),
                                (m.group(5) or "NO-OP"))
    except OSError:
        pass
    return out


# ===========================================================================
# THE DIFF-HARNESS SITE CENSUS (Wave 25 lane SWEEP)
# ===========================================================================
# Until now the only way to count the harness was a grep, and every wave that
# tried got a different answer:
#
#   Wave 24 kickoff quoted 354   -- an UNSCOPED, comment-counting grep
#   Wave 24 lane DIFF measured 305 RT_DIFF_SHADOW occurrences IN CODE
#                        and    630 sites in the armed RUN
#
# 354 = 305 code sites + 49 COMMENT LINES. The inflation is not random: the
# comments most likely to name a macro are the ones explaining that a site is a
# self-check, so the sites least able to catch anything were counted twice.
#
# SUBJECT: every *.cpp/*.h under SCAN_DIRS, comment- and string-stripped with
# this file's own strip_comments(), which is the same stripper the port scan
# uses. This counts SOURCE SITES. It is NOT the run-site count (630): a site in
# a header expands once per includer, macros build names at runtime, and
# hand-rolled `static RtDiffSite s = {...}` sites never mention a macro. The
# two numbers are different questions and are printed as such.
SITE_MACROS = ["RT_DIFF_BEGIN", "RT_DIFF_SHADOW", "RT_DIFF_SCALAR",
               "RT_DIFF_FLOAT", "RT_DIFF_BYTES", "RT_DIFF_WORK",
               "RT_DIFF_REPLAY_PRE", "RT_DIFF_REPLAY_POST"]


def blank_string_contents(text):
    """Blank the INSIDE of string/char literals, preserving length.

    strip_comments() tracks literals so a "//" inside one is not a comment, but
    it leaves their CONTENTS intact -- correct for its own job (it must not
    destroy the mangled names the port scan reads out of literals) and wrong for
    counting sites: a macro name quoted inside a string would be counted as
    code. Measured on the fixture below: without this pass the control scores
    2/4 and the failure looks like a miscount rather than a missing pass.
    """
    BS = chr(92)
    out = list(text)
    i, n = 0, len(text)
    st = 0            # 0 code, 1 in "", 2 in ''
    while i < n:
        c = text[i]
        if st == 0:
            if c == '"':
                st = 1
            elif c == "'":
                st = 2
        else:
            q = '"' if st == 1 else "'"
            if c == BS:
                out[i] = ' '
                if i + 1 < n:
                    out[i + 1] = ' '
                i += 2
                continue
            if c == q:
                st = 0
            else:
                out[i] = ' '
        i += 1
    return ''.join(out)


def diff_site_census():
    """Returns {macro: (code_occurrences, comment_or_string_occurrences)}."""
    counts = {m: [0, 0] for m in SITE_MACROS}
    counts["RtDiffSite{handrolled}"] = [0, 0]
    files = 0
    for path in iter_sources():
        raw = open(path, encoding="utf-8", errors="replace").read()
        code = blank_string_contents(strip_comments(raw))
        files += 1
        for m in SITE_MACROS:
            c = code.count(m)
            counts[m][0] += c
            counts[m][1] += raw.count(m) - c
        # The hand-rolled sites: `static RtDiffSite s = { "name", ... }`. A
        # macro-only count reports ~101 live sites as absent.
        c = len(re.findall(r'RtDiffSite\s+\w+\s*=\s*\{', code))
        counts["RtDiffSite{handrolled}"][0] += c
        counts["RtDiffSite{handrolled}"][1] += (
            len(re.findall(r'RtDiffSite\s+\w+\s*=\s*\{', raw)) - c)
    return counts, files


def selftest_site_census():
    """Control for diff_site_census, BOTH directions, on a handed shape.

    An absence-shaped count ("how many are NOT in comments") has no natural
    positive case: 0 comment hits is indistinguishable from a stripper that
    blanks the whole file. So the stripper is handed a buffer that contains
    BOTH forms of every macro and must split them exactly.
    """
    BS = chr(92)
    fixture = (
        "// RT_DIFF_BEGIN in a line comment\n"
        "/* RT_DIFF_BEGIN in a block comment */\n"
        'const char* s = "RT_DIFF_BEGIN in a string";' + "\n"
        "void f() { RT_DIFF_BEGIN(\"real\"); RT_DIFF_SHADOW(a, b, c); }\n"
        "static RtDiffSite g_s = { \"handrolled\", 0, 0, false, false };\n"
    )
    code = blank_string_contents(strip_comments(fixture))
    checks = [
        ("RT_DIFF_BEGIN code", code.count("RT_DIFF_BEGIN"), 1),
        ("RT_DIFF_BEGIN prose", fixture.count("RT_DIFF_BEGIN") - code.count("RT_DIFF_BEGIN"), 3),
        ("RT_DIFF_SHADOW code", code.count("RT_DIFF_SHADOW"), 1),
        ("handrolled code", len(re.findall(r'RtDiffSite' + BS + r's+' + BS + r'w+' + BS + r's*=' + BS + r's*' + BS + r'{', code)), 1),
    ]
    ok = True
    for (what, got, want) in checks:
        good = (got == want)
        ok = ok and good
        print("   %-24s got %d want %d  %s" % (what, got, want, "ok" if good else "*** WRONG ***"))
    print("   fixture: %d bytes, %d of them blanked by the stripper (effectiveness)"
          % (len(fixture), sum(1 for i in range(len(fixture)) if fixture[i] != code[i])))
    print("site-census selftest:", "ALIVE" if ok else "*** BROKEN ***")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", help="write per-function rows to this CSV")
    ap.add_argument("--unported", action="store_true",
                    help="list every called-but-not-ported function")
    ap.add_argument("--namespace", help="restrict the listings to one namespace")
    ap.add_argument("--allow-degraded", action="store_true",
                    help="proceed without pefile/the DLL. The address-keyed "
                         "rows become invisible and BOTH numbers are "
                         "understated -- the census says so on stderr. Do not "
                         "publish a number produced this way.")
    ap.add_argument("--selftest-unpromotable-key", action="store_true",
                    help="regression guard for load_unpromotable()'s symbol/class regex")
    ap.add_argument("--selftest-promoted-doors", action="store_true",
                    help="control: BOTH promotion doors must be visible to the scan")
    ap.add_argument("--selftest-classifier", action="store_true",
                    help="run the symbol-kind classifier against its fixture "
                         "table and exit. Includes a DELIBERATELY MALFORMED "
                         "name, because the UNKNOWN bucket is absence-shaped: "
                         "'0 unparseable names' in a real run is otherwise "
                         "indistinguishable from a dead parser.")
    ap.add_argument("--selftest-ordinal-fold", action="store_true",
                    help="run the ORDINAL FOLD-IN controls and exit. Includes "
                         "a SYNTHETIC unnameable ordinal, because the fold's "
                         "'unresolved' list is absence-shaped: in a real run "
                         "it is empty, and an empty list is indistinguishable "
                         "from a dead export-directory lookup.")
    ap.add_argument("--diff-sites", action="store_true",
                    help="count the diff-harness sites in SOURCE, with comments "
                         "and string literals stripped, and exit. The number "
                         "waves have been quoting came from an unscoped grep "
                         "that counted prose as code (354 vs 305).")
    ap.add_argument("--selftest-diff-sites", action="store_true",
                    help="control for --diff-sites: hand the stripper a buffer "
                         "holding both forms of every macro and require the "
                         "split to be exact.")
    args = ap.parse_args()

    if args.selftest_unpromotable_key:
        return selftest_unpromotable_key()
    if args.selftest_promoted_doors:
        return selftest_promoted_doors()
    if args.selftest_classifier:
        return selftest_classifier()
    if args.selftest_ordinal_fold:
        return selftest_ordinal_fold()

    if args.selftest_diff_sites:
        return selftest_site_census()

    if args.diff_sites:
        counts, files = diff_site_census()
        print("DIFF-HARNESS SITES IN SOURCE")
        print("subject: %d *.cpp/*.h files under %s, comment- and string-stripped"
              % (files, "/".join(SCAN_DIRS)))
        print()
        print("  %-26s %8s %8s" % ("macro", "code", "prose"))
        code_total = prose_total = 0
        for m, (c, p_) in counts.items():
            print("  %-26s %8d %8d" % (m, c, p_))
            code_total += c
            prose_total += p_
        print("  %-26s %8d %8d" % ("TOTAL", code_total, prose_total))
        print()
        print("`prose` is occurrences inside comments or string literals. They are")
        print("NOT sites, and a grep that counts them inflates the harness: Wave 24's")
        print("kickoff quoted 354 RT_DIFF_SHADOW where 305 are code and 49 are prose.")
        print()
        print("THIS IS NOT THE RUN-SITE COUNT. An armed BANISHED_RT_DIFF=1 run")
        print("registered 630 sites; that number includes hand-rolled RtDiffSite")
        print("statics and header sites expanded per includer, and excludes every")
        print("source site no execution reached. Join the two with")
        print("tools/rt_diff_run_join.py; never substitute one for the other.")
        return 0

    global ALLOW_DEGRADED
    ALLOW_DEGRADED = args.allow_degraded

    d = scan()

    # An address that names an EXPORT is the same function as that export.
    # Resolving it lets the row land in its real namespace, and lets a function
    # our tree reaches BOTH ways be counted once instead of twice. See
    # load_export_addrs() for the two failure modes this fixes.
    addr_to_name = load_export_addrs()

    merged = {}   # canonical key -> [kind, key, ns, ported, detoured, set(files)]

    def add_entity(kind, key, ns, ported, detoured, files):
        e = merged.get(key)
        if e is None:
            merged[key] = [kind, key, ns, ported, detoured, set(files)]
        else:
            e[3] = e[3] or ported
            e[4] = e[4] or detoured
            e[5].update(files)

    for name, files in d["called_exports"].items():
        add_entity("export", name, namespace_of(name),
                   name in d["ported_exports"], name in d["detoured"], files)
    for addr, files in d["called_addrs"].items():
        name = addr_to_name.get(addr)
        ported = addr in d["ported_addrs"]
        detoured = addr in d["detoured"]
        if name:
            # Named export reached by address. Its port may be registered under
            # either door, so consult both.
            add_entity("export", name, namespace_of(name),
                       ported or (name in d["ported_exports"]),
                       detoured or (name in d["detoured"]), files)
        else:
            add_entity("addr", "0x%x" % addr, "internal", ported, detoured, files)

    # ---- THE ORDINAL FOLD-IN (blind spot NINE, Wave 25 lane INTERNAL42) -----
    # Modelled exactly on the address fold-in directly above, for the same
    # reason: an ordinal and a mangled name can denote ONE function, and a row
    # per door counts it twice. See load_export_ordinals() for the four rows
    # this moved and why three of them were phantoms.
    #
    # `ported` consults ALL THREE doors, because ResolveOrdinal does: it checks
    # RtOrdinalReg(), then falls through to the NAME registry, and a by-address
    # registration of the same function is the third way the body is ours.
    ordinal_to_export = load_export_ordinals()
    ord_fold = {"resolved": {}, "unresolved": [],
                "readable": bool(ordinal_to_export)}
    for ordn, files in d["called_ordinals"].items():
        ent = ordinal_to_export.get(ordn)
        if ent is None:
            # No export-directory row (a NONAME export, or a degraded run).
            # Such an ordinal keeps its own row -- there is no name to fold to.
            ord_fold["unresolved"].append(ordn)
            continue
        name, addr = ent
        ported = ((ordn in d["ported_ordinals"])
                  or (name in d["ported_exports"])
                  or (addr in d["ported_addrs"]))
        add_entity("export", name, namespace_of(name), ported,
                   name in d["detoured"], files)
        ord_fold["resolved"][ordn] = name

    # A DETOURED PORT IS NEVER CALLER-LESS, and before this it was invisible
    # twice over: absent from the namespace table and listed as a suspected
    # vestigial resolve.
    #
    # `called` counts functions our tree resolves through a door
    # (ResolveExport / ResolveAddr / an ordinal). A detour creates callers a
    # different way -- it patches the DLL, so RETAIL'S OWN internal callers
    # arrive at our body -- and those callers are the entire point of
    # installing one. Wave 10's UI::TabElement::UpdateContainers is the case
    # that exposed it: ported, registered under both doors, detoured, verified
    # against the DLL, exercised in a live run... and printed as `detour 0`
    # with the function itself sitting in the "ports with no caller" list,
    # because our tree calls it as a direct C++ call rather than through a
    # resolve door.
    #
    # So a detour target is folded in here as called + ported + detoured. This
    # is not crediting unrun code: the detour is what makes it run.
    for dkey, dfiles in d["detoured"].items():
        if isinstance(dkey, int):
            name = addr_to_name.get(dkey)
            if name:
                add_entity("export", name, namespace_of(name), True, True, dfiles)
            else:
                add_entity("addr", "0x%x" % dkey, "internal", True, True, dfiles)
        else:
            add_entity("export", dkey, namespace_of(dkey), True, True, dfiles)

    # ---- THE MINTED-VTABLE SLOT FOLD-IN (blind spot EIGHT, Wave 20 Phase 0a)
    # Modelled on the detour fold-in directly above, and ruled on at length at
    # RE_MINT_VSLOT_ARRAY. Summary of the ruling, restated here so a reader of
    # main() does not have to go looking: a PROMOTED vtable row publishes our
    # table unconditionally, so its slots are a live dispatch route and are
    # folded in as called (+ ported iff the body is registered); an UNPROMOTED
    # row is registered only under BANISHED_RT_MINT=1, which this script has
    # always refused to count, so its slots move nothing and are reported as the
    # PROMOTION BACKLOG instead.
    #
    # The snapshot is taken BEFORE the fold so the fold's effect can be printed
    # as a SET-DIFF -- which rows became visible, and which flipped to ported --
    # rather than as a percentage delta that says nothing about what moved.
    prefold_keys = set(merged)
    prefold_ported = {k for k, v in merged.items() if v[3]}

    mint_vtables = load_mint_vtables(REPO)
    promoted_syms = mint_promoted_symbols(REPO)
    vt_fold = {
        "readable": mint_vtables is not None and promoted_syms is not None,
        "vtables": 0, "promoted_vtables": 0,
        "applicable": set(),      # registered slot bodies of PROMOTED rows
        "folded_called": set(),   # every slot key folded into the row set
        "folded_names": set(),    # ... as export names (for the orphan screen)
        "folded_addrs": set(),    # ... as raw VAs   (for the orphan screen)
        "backlog": {},            # symbol -> set of registered slot bodies
        "backlog_rows": 0,
        "backlog_slots": {},      # symbol -> non-purecall slot count
        "backlog_complete": [],   # unpromoted vtables with EVERY slot ours
    }
    if vt_fold["readable"]:
        promoted_vt = {s for s in promoted_syms if s.startswith("??_7")}
        vt_fold["vtables"] = len(mint_vtables)
        for sym, _vva, slots in mint_vtables:
            # An address-keyed (unexported) vtable row cannot be promoted:
            # every RtMintPromotionStorage call names a symbol.
            is_promoted = bool(sym) and sym in promoted_vt
            if is_promoted:
                vt_fold["promoted_vtables"] += 1
            src = "mint-vtable:%s" % (sym or "unexported")
            row_key = sym or ("0x%x" % _vva)
            n_slots = n_ours = 0
            for sname, sva, kind in slots:
                if kind == "RT_MINT_PURECALL":
                    continue        # no body to port
                # The mint's OWN lookup order: name first, then address.
                registered = (sname in d["ported_exports"]) if sname else False
                registered = registered or (sva in d["ported_addrs"])
                n_slots += 1
                if registered:
                    n_ours += 1
                if not is_promoted:
                    if registered:
                        vt_fold["backlog"].setdefault(row_key, set()).add(
                            sname or ("0x%x" % sva))
                    continue
                name = sname or addr_to_name.get(sva)
                if name:
                    add_entity("export", name, namespace_of(name),
                               registered or (name in d["ported_exports"]),
                               name in d["detoured"], [src])
                    key = name
                    vt_fold["folded_names"].add(name)
                    vt_fold["folded_addrs"].add(sva)
                else:
                    key = "0x%x" % sva
                    add_entity("addr", key, "internal", registered,
                               sva in d["detoured"], [src])
                    vt_fold["folded_addrs"].add(sva)
                vt_fold["folded_called"].add(key)
                if registered:
                    vt_fold["applicable"].add(key)
            if not is_promoted and n_slots:
                vt_fold["backlog_slots"][row_key] = n_slots
                # COMPLETE = every non-purecall slot has a registered body, so
                # promoting the row would put NO trap into a live table. This is
                # the static half of the readiness question and nothing more: it
                # does NOT decide promotability, which turns on whether we own
                # the function that STAMPS the table (WAVE20_GUIDE.md §2), and
                # that is not a question a source scan can answer.
                if n_ours == n_slots:
                    vt_fold["backlog_complete"].append(row_key)
        vt_fold["backlog_rows"] = sum(len(v) for v in vt_fold["backlog"].values())

    # The fold's EFFECT, computed here so the header above the table can quote
    # it. (Code order is the fix, not the wording: a headline cannot carry a
    # number the code has not computed yet.)
    vt_fold["newly_visible"] = sorted(set(merged) - prefold_keys)
    vt_fold["newly_ported"] = sorted({k for k, v in merged.items() if v[3]}
                                     - prefold_ported
                                     - set(vt_fold["newly_visible"]))
    vt_fold["control"] = vt_fold["applicable"] & (set(vt_fold["newly_visible"])
                                                  | set(vt_fold["newly_ported"]))

    rows = [(k, key, ns, p, det, sorted(f)) for k, key, ns, p, det, f in merged.values()]
    for ordn in ord_fold["unresolved"]:
        # An ordinal the export directory cannot name carries no name, so no
        # namespace can be inferred from it — its own bucket, not lumped in
        # with names that failed to parse. Every ordinal this tree uses IS
        # nameable today, so this list is normally empty; see the ORDINAL
        # FOLD-IN block in main() and its --selftest-ordinal-fold control,
        # which hands this path a synthetic unnameable ordinal precisely
        # because "0 unresolved" is otherwise indistinguishable from a dead
        # lookup.
        rows.append(("ordinal", str(ordn), "ordinal",
                     ordn in d["ported_ordinals"], False,
                     sorted(d["called_ordinals"][ordn])))

    # Ports with no caller in our tree are worth surfacing: for the exe port a
    # registration nobody asks for was usually a vestigial resolve, and the same
    # trap exists here.
    # A port is an orphan only if NEITHER door has a caller. Registering both
    # the name and the address for one function is deliberate insurance in this
    # project, and before this was fixed it made every such function report as
    # caller-less forever -- which trained the reader to ignore the list that
    # exists to catch reference_uncalled_port_is_not_verified.md.
    name_of_called_addr = {addr_to_name[a] for a in d["called_addrs"] if a in addr_to_name}
    called_export_names = set(d["called_exports"]) | name_of_called_addr
    # Detoured names/addresses have retail's callers, so they are not orphans.
    detoured_names = {k for k in d["detoured"] if not isinstance(k, int)}
    detoured_names |= {addr_to_name[k] for k in d["detoured"]
                       if isinstance(k, int) and k in addr_to_name}
    detoured_addrs = {k for k in d["detoured"] if isinstance(k, int)}
    detoured_addrs |= {a for a, nm in addr_to_name.items() if nm in detoured_names}
    called_export_names |= detoured_names
    # A body in a PROMOTED minted vtable slot has callers for the same reason a
    # detoured one does -- something other than a resolve door dispatches it --
    # so it must leave this list too. Omitting this was an inconsistency in the
    # first cut of the fold-in: the row entered the table as `called` and stayed
    # in "ports with no caller", which is the two halves of one tool disagreeing.
    called_export_names |= vt_fold["folded_names"]
    orphan_exports = sorted(set(d["ported_exports"]) - called_export_names)

    addr_of_called_name = {a for a, nm in addr_to_name.items() if nm in d["called_exports"]}
    called_addr_set = set(d["called_addrs"]) | addr_of_called_name
    called_addr_set |= detoured_addrs
    called_addr_set |= vt_fold["folded_addrs"]
    called_addr_set |= {a for a, nm in addr_to_name.items()
                        if nm in vt_fold["folded_names"]}
    orphan_addrs = sorted(set(d["ported_addrs"]) - called_addr_set)

    per_ns = collections.defaultdict(lambda: {"called": 0, "ported": 0, "detoured": 0})
    for kind, key, ns, ported, detoured, files in rows:
        b = per_ns[ns]
        b["called"] += 1
        b["ported"] += 1 if ported else 0
        b["detoured"] += 1 if detoured else 0

    # ---- SUBJECT AND DENOMINATOR, BEFORE ANY FINDING ----------------------
    # WAVE19_CLOSED.md §7.1 / WAVE20_GUIDE.md §4.1: an instrument that names its
    # subject only AFTER its findings has not named it, and a green without its
    # denominator is not a measurement. Six instruments in one wave printed a
    # verdict without stating what they had examined.
    #
    # Everything printed here is COMPUTED ABOVE this line. That ordering is the
    # actual fix, not the wording: a headline cannot carry a number the code has
    # not computed yet, and two Wave 19 tools were repaired exactly this way.
    n_prefold = len(prefold_keys)
    print("Runtime-x64.dll port census")
    print("=" * 78)
    print("SUBJECT")
    print("  tree root      %s" % REPO)
    print("                 (derived from THIS script's path -- run the script")
    print("                  inside the tree you mean to measure)")
    print("  scanned        %d source files under %s"
          % (d["nfiles"], "/, ".join(SCAN_DIRS) + "/"))
    print("  DLL read       %s%s"
          % (RUNTIME_DLL,
             "" if addr_to_name else "   [NOT READ -- degraded, see stderr]"))
    print("  export map     %d named exports (address -> name)" % len(addr_to_name))
    print("PARAMETERS  (a check with a free parameter can be aimed; these are it)")
    print("  --namespace    %s" % (args.namespace or "(none: every namespace)"))
    print("  --unported     %s" % ("on" if args.unported else "off"))
    print("  --csv          %s" % (args.csv or "(none)"))
    print("  --allow-degraded %s" % ("ON -- numbers UNDERSTATE" if ALLOW_DEGRADED
                                     else "off"))
    print("POPULATION  (the denominator every percentage below is taken against)")
    print("  %d rows from resolve doors + detour targets" % n_prefold)
    if not vt_fold["readable"]:
        print("  ** MINTED-VTABLE SLOTS: MEASURED NOTHING. %s could not be read,"
              % MINT_TABLES)
        print("  ** so no slot-body row is folded in and `ported` UNDERSTATES by an")
        print("  ** unknown amount. This is not a zero; it is a refusal to answer.")
    else:
        print("  %d minted-vtable slot rows folded in, from the %d PROMOTED"
              % (len(vt_fold["folded_called"]), vt_fold["promoted_vtables"]))
        print("    vtable(s) of %d minted; %d of those %d were NOT previously"
              % (vt_fold["vtables"], len(vt_fold["newly_visible"]),
                 len(vt_fold["folded_called"])))
        print("    visible, the rest already had a resolve door. The other %d"
              % (vt_fold["vtables"] - vt_fold["promoted_vtables"]))
        print("    vtables are registered only under BANISHED_RT_MINT=1 and move")
        print("    NO column -- see PROMOTION BACKLOG below.")
    # THE ORDINAL FOLD-IN, reported as a SET-DIFF and not a delta, so a reader
    # can see WHICH rows moved rather than that a number changed. Before Wave
    # 25 every one of these was a row of its own in a pseudo-namespace.
    print("  %d ordinal call site(s), of which %d folded onto the export row"
          % (len(d["called_ordinals"]), len(ord_fold["resolved"])))
    if ord_fold["resolved"]:
        for ordn in sorted(ord_fold["resolved"]):
            print("      ord %-5d -> %s" % (ordn, ord_fold["resolved"][ordn]))
    if ord_fold["unresolved"]:
        print("      UNNAMEABLE (kept as its own row): %s"
              % ", ".join(str(o) for o in sorted(ord_fold["unresolved"])))
    elif not ord_fold["readable"]:
        print("      (export ordinals NOT READ -- degraded; every ordinal")
        print("       keeps its own row, which is the pre-Wave-25 behaviour)")
    print("  = %d rows total, of which %d are ordinal rows"
          % (len(rows), len(ord_fold["unresolved"])))
    print("=" * 78)
    unprom = load_unpromotable(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    unprom_per_ns = {}
    for _k, _key, _ns, _p, _d, _f in rows:
        if _key in unprom and not _p:
            unprom_per_ns[_ns] = unprom_per_ns.get(_ns, 0) + 1

    print("%-14s %8s %8s %8s %8s  %s  %s" %
          ("namespace", "total", "called", "ported", "detour", "called covered",
           "ceiling"))
    print("-" * 78)

    order = sorted(per_ns, key=lambda n: (-per_ns[n]["called"], n))
    tot_called = tot_ported = tot_detour = 0
    for ns in order:
        b = per_ns[ns]
        total = TOTAL_FUNCTIONS.get(ns, "")
        pct = (100.0 * b["ported"] / b["called"]) if b["called"] else 0.0
        nu = unprom_per_ns.get(ns, 0)
        if nu:
            ceil_n = b["called"] - nu
            cpct = (100.0 * ceil_n / b["called"]) if b["called"] else 0.0
            ceil = "  %5.1f%%%s" % (cpct, "  AT CEILING" if b["ported"] >= ceil_n else "")
        else:
            ceil = ""
        print("%-14s %8s %8d %8d %8d  %5.1f%%%s" %
              (ns, total, b["called"], b["ported"], b["detoured"], pct, ceil))
        tot_called += b["called"]
        tot_ported += b["ported"]
        tot_detour += b["detoured"]

    print("-" * 78)
    pct = (100.0 * tot_ported / tot_called) if tot_called else 0.0
    print("%-14s %8d %8d %8d %8d  %5.1f%%" %
          ("TOTAL", sum(TOTAL_FUNCTIONS.values()), tot_called, tot_ported, tot_detour, pct))
    print()
    print("total  = functions in the DLL (PLAN.md §3.1 Ghidra census; blank for "
          "non-namespaced internals)")
    print("called = distinct functions our tree resolves today, OR that retail's")
    print("         own callers reach through a detour or a PROMOTED minted vtable")

    # ---- WRAPPER-DOOR SCREEN (Wave 21; defect found by lane GFXVT) --------
    # See RE_MANGLED_CALL. Changes no count; names what the door scan cannot
    # classify, instead of adding a seventh hardcoded pattern.
    wrap = collections.Counter()
    wrap_where = {}
    for path in iter_sources():
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        # Comment-stripped, unlike the RT_UNPROMOTABLE checker: THIS subject is
        # code. A checker with no comment stripper reports prose as code, and
        # the prose most likely to name a door is a comment saying that door was
        # DELETED (reference_uncommented_checker_manufactures_work).
        stripped = strip_comments(text)
        for m in RE_MANGLED_CALL.finditer(stripped):
            ident = m.group(1)
            wrap[ident] += 1
            wrap_where.setdefault(ident, path)
    n_all = sum(wrap.values())
    known = {i: c for i, c in wrap.items() if i in WRAPPER_KNOWN_DOOR}
    notdoor = {i: c for i, c in wrap.items() if i in WRAPPER_KNOWN_NOTDOOR}
    unclass = {i: c for i, c in wrap.items()
               if i not in WRAPPER_KNOWN_DOOR and i not in WRAPPER_KNOWN_NOTDOOR}
    print()
    print("WRAPPER-DOOR SCREEN  (Wave 21; the SIXTH instance of one blind-spot")
    print("family, found by lane GFXVT -- and a SCREEN, not a seventh pattern)")
    print("  subject     every identifier in the scanned tree called with a")
    print("              mangled-name string literal: %d call site(s), %d distinct"
          % (n_all, len(wrap)))
    print("  known DOOR        %5d site(s) across %d identifier(s)"
          % (sum(known.values()), len(known)))
    print("  known NOT a door  %5d site(s) across %d identifier(s)"
          % (sum(notdoor.values()), len(notdoor)))
    print("  UNCLASSIFIED      %5d site(s) across %d identifier(s) -- an"
          % (sum(unclass.values()), len(unclass)))
    print("                    identifier this script cannot resolve is UNKNOWN,")
    print("                    NOT absent. Each is named below with a file.")
    for ident, c in sorted(unclass.items(), key=lambda kv: (-kv[1], kv[0])):
        print("      %-34s %4d   first seen in %s" % (ident, c, wrap_where[ident]))
    if not unclass:
        print("      (none -- every identifier is classified)")
    print("  CAVEAT      this screen CHANGES NO COUNT above. Whether an")
    print("              unclassified identifier is really a door is a judgement")
    print("              about that wrapper's semantics, and making it here would")
    print("              move numbers other lanes quote mid-wave. Known live")
    print("              example: RtVtable (runtime/src/graphics/mesh.cpp) IS a")
    print("              door -- three Graphics vtable rows reach the DLL only")
    print("              through it and are absent from `called` entirely.")

    # ---- GAP DECOMPOSITION BY SYMBOL KIND (Wave 21 lane CLASS) ------------
    # The doctrine this implements: a namespace gap figure is uninterpretable
    # until decomposed by symbol kind. See classify_symbol() for what it reads
    # and, more importantly, what it cannot.
    secmap = load_section_map()
    gap_kinds = collections.defaultdict(collections.Counter)
    unknown_why = collections.Counter()
    unknown_sect = collections.Counter()
    unknown_named = []
    for kind, key, ns, ported, detoured, files in rows:
        if ported:
            continue
        bucket, why = classify_symbol(key, kind)
        gap_kinds[ns][bucket] += 1
        if bucket == "UNKNOWN":
            unknown_why[why] += 1
            if kind == "addr":
                unknown_sect[section_of(secmap, int(key, 16)) or "(not probed)"] += 1
            if why.startswith("mangled name") or why.startswith("type code"):
                unknown_named.append(key)

    print()
    print("GAP DECOMPOSITION BY SYMBOL KIND  (Wave 21 lane CLASS)")
    print("  subject     the %d rows that are called-but-NOT-ported, i.e. exactly"
          % (tot_called - tot_ported))
    print("              the per-namespace gap in the table above. Ported rows are")
    print("              NOT classified and are not in any figure here.")
    print("  evidence    the MSVC MANGLED NAME and nothing else. Not the DLL, not")
    print("              the mint tables, not our source. So this says what KIND of")
    print("              thing a row is; it says NOTHING about size, reachability,")
    print("              or whether a DATA row is already minted.")
    print("  why         a gap read as \"N functions to port\" is scoped against a")
    print("              population that does not exist. Wave 10 paid to learn this")
    print("              for UI and Wave 21's kickoff paid again for Graphics.")
    print()
    print("  %-13s %5s | %s" % ("namespace", "gap",
                                " ".join("%8s" % k for k in KIND_ORDER)))
    print("  " + "-" * 76)
    gap_tot = collections.Counter()
    for ns in sorted(gap_kinds, key=lambda n: (-sum(gap_kinds[n].values()), n)):
        c = gap_kinds[ns]
        gap_tot.update(c)
        print("  %-13s %5d | %s" % (ns, sum(c.values()),
                                    " ".join("%8d" % c[k] for k in KIND_ORDER)))
    print("  " + "-" * 76)
    print("  %-13s %5d | %s" % ("TOTAL", sum(gap_tot.values()),
                                " ".join("%8d" % gap_tot[k] for k in KIND_ORDER)))
    code_kinds = ("BODY", "CTOR", "DTOR", "OPNEWDEL", "OPERATOR")
    n_code = sum(gap_tot[k] for k in code_kinds)
    n_gap = sum(gap_tot.values())
    print()
    print("  BODY/CTOR/DTOR/OPNEWDEL/OPERATOR = a function to reimplement.")
    print("  VTABLE = promotion work (the body rows are its slots, counted")
    print("           separately; see PROMOTION BACKLOG below).")
    print("  DATA   = mint/publish work; there is no body to write.")
    print("  UNKNOWN = this classifier REFUSES to guess. Not zero, not BODY.")
    print("  => %d of %d gap rows (%.0f%%) are a function to reimplement."
          % (n_code, n_gap, 100.0 * n_code / n_gap if n_gap else 0.0))
    print()
    print("  UNKNOWN, itemised -- %d rows this classifier cannot read:"
          % gap_tot["UNKNOWN"])
    for why, n in sorted(unknown_why.items(), key=lambda kv: -kv[1]):
        print("    %4d  %s" % (n, why))
    if unknown_named:
        print("    the unparseable NAMES, so they can be fixed rather than absorbed:")
        for k in sorted(unknown_named):
            print("      %s" % k)
    print("    SECONDARY HINT for the address-keyed rows -- which DLL SECTION each")
    print("    lands in. Different evidence from the name grammar, so it is a hint")
    print("    beside the bucket and never folded into one:")
    if secmap is None:
        print("      (not probed: pefile or the DLL was unavailable -- this is a")
        print("       REFUSAL, not a zero)")
    elif not unknown_sect:
        print("      (no address-keyed rows in the gap)")
    else:
        for s, n in sorted(unknown_sect.items(), key=lambda kv: -kv[1]):
            print("      %-28s %4d" % (s, n))
        print("      .text = code; .rdata/.data = data. So even the address-keyed")
        print("      half of the gap is NOT all port work either.")

    # ---- GATED-BUT-WRITTEN, PER NAMESPACE  (Wave 22 lane ALLOC) -----------
    #
    # WHY THIS EXISTS. This script has printed, for many waves, both halves of
    # a fact and never the join between them: a per-namespace GAP, and further
    # down a flat count of "N registration(s) are present in the source but NOT
    # counted above". Nothing connected the second to the first, so a reader
    # could not tell which of a namespace's gap rows are UNWRITTEN and which
    # are WRITTEN-AND-UNARMED. Those two are not the same work and are not even
    # the same KIND of work: one is a port, the other is a risk decision.
    #
    # It cost a wave. WAVE22_PLAN.md's headline finding -- that System's
    # "eleven bodies" are six, because five of them are MemoryAllocator bodies
    # that have existed and been registered since Wave 2 behind
    # BANISHED_RT_ALLOC -- was produced by a person grepping the file THIS
    # SCRIPT ALREADY NAMES IN ITS OWN WARNING TEXT. The Wave 21 gap-kind
    # decomposition could not have caught it: all five classify correctly as
    # BODY. They are not unwritten, they are unarmed, and no instrument said so.
    #
    # WHAT IT IS. A join, nothing cleverer: for every called-but-not-ported row
    # above, is that same key also registered somewhere this script has ruled
    # GATED (an opt-in FILE in GATED_OFF_FILES, or an opt-in `if (<flag>)`
    # BLOCK in GATED_OFF_BLOCKS)? If yes, the row is written; arming its gate
    # would move it.
    #
    # WHAT IT IS NOT, and this is the load-bearing half:
    #
    #   * It does NOT move any count. A gated registration is not a port --
    #     that rule is older than this section and this section does not
    #     weaken it. Every figure above is unchanged.
    #   * "Arming would move it" is a claim about the CENSUS, not about the
    #     GAME. Whether the row is SAFE to arm is a separate question with a
    #     separate answer, and for the allocator the standing verdict is
    #     route-scoped (see WAVE22_LANE_ALLOC.md).
    #   * Its denominator is the gap rows THIS SCRIPT CAN SEE. A function
    #     reached only by a plain C++ call has no row here at all, so it can be
    #     neither in the gap nor in this join. Corroborated twice in Wave 22:
    #     `?PlatformStartup@Timer@System@@CA_NXZ` is registered, runs every
    #     boot, writes three statics and appears in no row of this census.
    #   * It reads OUR SOURCE only. It cannot tell a gate that is off by
    #     accident from one that is off because arming it corrupts the heap.
    gated_by_file = {}
    for f, gv in GATED_OFF_FILES.items():
        gated_by_file[f] = gv
    for f, gates in GATED_OFF_BLOCKS.items():
        gated_by_file[f] = "if(%s)" % (") / if(".join(gates))

    def gate_for(files):
        names = sorted({gated_by_file[f] for f in files if f in gated_by_file})
        return ", ".join(names) if names else "(gated, source unnamed)"

    gated_gap = collections.defaultdict(list)   # ns -> [(kind, key, gate)]
    for kind, key, ns, ported, detoured, files in rows:
        if ported:
            continue
        if kind == "export":
            where = d["gated_exports"].get(key)
        elif kind == "addr":
            try:
                where = d["gated_addrs"].get(int(key, 16))
            except ValueError:
                where = None
        else:
            where = None
        if where:
            gated_gap[ns].append((classify_symbol(key, kind)[0], key, gate_for(where)))

    n_gated_gap = sum(len(v) for v in gated_gap.values())
    print()
    print("GATED-BUT-WRITTEN GAP ROWS, PER NAMESPACE  (Wave 22 lane ALLOC)")
    print("  subject     the same %d called-but-NOT-ported rows decomposed above."
          % (tot_called - tot_ported))
    print("  question    which of them are not UNWRITTEN but WRITTEN-AND-UNARMED?")
    print("  evidence    our source only: the row's key also appears at a")
    print("              RegisterRuntime* call site inside a file or an if-block")
    print("              this script has ruled opt-in. Not the DLL, not a run.")
    print("  changes     NOTHING above. A gated registration is still not a port.")
    print("  blind spot  a registration reached by a plain C++ call has no row in")
    print("              this census at all, so it cannot appear here either.")
    print()
    if not n_gated_gap:
        print("  (no gap row is gated-but-written)")
        if gated_by_file:
            print("  *** POSITIVE CONTROL FAILED. %d file(s)/block(s) are on the"
                  % len(gated_by_file))
            print("  *** opt-in lists and the flat count below is %d, yet this join"
                  % (len(d["gated_exports"]) + len(d["gated_addrs"])))
            print("  *** matched zero gap rows. Either every gated registration is")
            print("  *** for a symbol our tree never CALLS -- possible, and then it")
            print("  *** is not in the gap by construction -- or this screen is dead.")
            print("  *** Do not read the zero as `nothing is gated` without checking")
            print("  *** which (a screen with no natural positive case cannot tell")
            print("  *** a true absence from a broken parser).")
    else:
        print("  %-13s %5s %7s | %s" % ("namespace", "gap", "gated",
                                        " ".join("%8s" % k for k in KIND_ORDER)))
        print("  " + "-" * 84)
        gg_tot = collections.Counter()
        for ns in sorted(gated_gap, key=lambda n: (-len(gated_gap[n]), n)):
            c = collections.Counter(b for b, _, _ in gated_gap[ns])
            gg_tot.update(c)
            print("  %-13s %5d %7d | %s"
                  % (ns, sum(gap_kinds[ns].values()), len(gated_gap[ns]),
                     " ".join("%8d" % c[k] for k in KIND_ORDER)))
        print("  " + "-" * 84)
        print("  %-13s %5d %7d | %s"
              % ("TOTAL", tot_called - tot_ported, n_gated_gap,
                 " ".join("%8d" % gg_tot[k] for k in KIND_ORDER)))
        print()
        print("  ITEMISED -- the rows, and the gate that holds each one back.")
        print("  Scoping a porting lane on these would scope work that is already")
        print("  written. Read the gate name as the question to ask, not as a")
        print("  cheap win: arming is a measurement and a risk decision.")
        for ns in sorted(gated_gap):
            print("    %s" % ns)
            for bucket, key, gate in sorted(gated_gap[ns], key=lambda t: (t[0], t[1])):
                print("      %-7s %-58s %s" % (bucket, key, gate))
        print()
        print("  => %d of %d gap rows (%.0f%%) are WRITTEN and held back by a gate,"
              % (n_gated_gap, tot_called - tot_ported,
                 100.0 * n_gated_gap / (tot_called - tot_ported)
                 if (tot_called - tot_ported) else 0.0))
        print("     not by an absent body. Arm it, measure it, then move it.")

    # ---- THE FOURTH GATING SHAPE, AND WHETHER IT IS STILL ALIVE -----------
    #
    # Every screen above asks "is an UNPORTED row secretly written?". This one
    # started life asking the opposite -- because a lane that finds an
    # instrument wrong in one direction has not thereby checked the other, and
    # on the allocator the answer was yes BOTH times -- and it found
    # ?Allocate@/?Free@ counted PORTED while their only registrations sat
    # inside a function opening `if (!g_swapTaken) return;`.
    #
    # GATED_OFF_FUNCS now feeds scan(), so those rows are EXCLUDED from
    # `ported` like any other gated registration and this screen no longer has
    # a suspect to report. Which is exactly why it must not print a bare
    # "(none)": a screen whose success state and whose DEAD state print the
    # same thing cannot be trusted in either. So it reports LIVENESS instead --
    # what the declared spans actually matched, and where those keys ended up.
    # A declaration that matches nothing is a dead declaration and says so.
    gf_keys, gf_unread = gated_func_keys(REPO)
    print()
    print("GATED-FUNCTION EXCLUSIONS -- the FOURTH gating shape (Wave 22 lane ALLOC)")
    print("  shape       a registration inside a function whose first statement is a")
    print("              guard clause on a global only the arming path sets:")
    print("                  void RtAllocSwapPostRegister() {")
    print("                      if (!g_swapTaken) return;          <-- the gate")
    print("                      RegisterRuntimeExport(\"?Allocate@...\", ...);")
    print("              GATED_OFF_FILES cannot see it (the file holds live ports too),")
    print("              GATED_OFF_BLOCKS cannot see it (no `if (<flag>) { ... }`), and")
    print("              it is not the mint. Declared in GATED_OFF_FUNCS; bodies are")
    print("              brace-matched on the comment-stripped text.")
    print("  effect      these keys ARE excluded from `ported` above, registration")
    print("              side AND detour side. Gating one without the other would")
    print("              have moved nothing and looked like it had.")
    print("  LIVENESS    this is a positive-case report, NOT an all-clear. A bare")
    print("              zero here would be indistinguishable from a dead parser.")
    # TWO IMPLEMENTATIONS, DELIBERATELY, AND THEY CHECK EACH OTHER.
    # gated_func_spans() runs inside scan() on scan's own text and is what
    # actually MOVES the count; gated_func_keys() re-reads the file here and is
    # what PRINTS. They share the declaration and nothing else. So the
    # "excluded / LEAK" column below is not decoration: it is one
    # implementation confirming the other's effect on the numbers. A bug in
    # either shows up as a LEAK.
    #
    # Plus a synthetic pair, because agreement between two implementations that
    # are BOTH dead is still agreement. The positive proves the span walk finds
    # a body; the negative proves it does not swallow a forward declaration and
    # run on to the end of the file, which is the failure that would silently
    # gate everything after it.
    _SYN = ('void SynGateFn();\n'                    # a declaration, not a body
            'void Unrelated() { RegisterRuntimeExport("?NotGated@Syn@@YAXXZ", 0); }\n'
            'void SynGateFn() {\n'
            '    if (!g_synTaken) return;\n'
            '    RegisterRuntimeExport("?Gated@Syn@@YAXXZ", 0);\n'
            '}\n')
    _spans = gated_func_spans(_SYN, {"SynGateFn": "synthetic"})
    def _inside(needle):
        pos = _SYN.index(needle)
        return any(a <= pos <= b for a, b in _spans)
    _pos_ok = len(_spans) == 1 and _inside("?Gated@Syn@@YAXXZ")
    _neg_ok = not _inside("?NotGated@Syn@@YAXXZ")
    print("  CONTROL     span walk, synthetic: positive (gated body found) -> %s;"
          % ("GREEN" if _pos_ok else "*** RED ***"))
    print("              negative (forward decl does NOT open a span, and an")
    print("              unrelated registration before it stays ungated) -> %s"
          % ("GREEN" if _neg_ok else "*** RED ***"))
    if not (_pos_ok and _neg_ok):
        print("  *** THE SPAN WALK IS BROKEN. Every `gated` verdict from")
        print("  *** GATED_OFF_FUNCS is untrustworthy and the `ported` count with it.")
    if gf_unread:
        print("  *** COULD NOT READ: %s" % ", ".join(gf_unread))
        print("  *** A REFUSAL, not a zero: the list below is INCOMPLETE by an")
        print("  *** unknown amount, and some keys may be counted PORTED that")
        print("  *** should not be.")
    if not GATED_OFF_FUNCS:
        print("    (nothing declared)")
    elif not gf_keys:
        print("    *** %d function(s) DECLARED and ZERO keys matched. The"
              % sum(len(v) for v in GATED_OFF_FUNCS.values()))
        print("    *** declaration is DEAD -- a renamed function, a moved")
        print("    *** registration, or a broken span walk. It is excluding")
        print("    *** NOTHING, silently, and every figure above is as if this")
        print("    *** list were empty.")
    else:
        ported_names = set(d["ported_exports"])
        for key, (rel, fn, gate) in sorted(gf_keys.items()):
            state = "counted PORTED  <-- LEAK" if key in ported_names else "excluded"
            print("    %-56s %s" % (key, state))
            print("        in %s::%s -- %s" % (rel, fn, gate))
        leaks = sum(1 for k in gf_keys if k in ported_names)
        print("    => %d key(s) matched; %d excluded, %d still counted PORTED."
              % (len(gf_keys), len(gf_keys) - leaks, leaks))
        if leaks:
            print("    A LEAK means a key this list names is reaching `ported` by some")
            print("    OTHER registration -- find it before trusting the figure.")

    # ---- THE VTABLE FOLD-IN, REPORTED AS A SET-DIFF -----------------------
    # Never as a percentage delta: a percentage cannot say WHICH rows moved,
    # and this census has `called` in both halves of its own ceiling identity,
    # so a delta is uninterpretable by construction.
    print()
    print("MINTED-VTABLE SLOT FOLD-IN  (Wave 20 Phase 0a; precedent: the detour")
    print("fold-in, same file, same argument -- see RE_MINT_VSLOT_ARRAY)")
    if not vt_fold["readable"]:
        print("  *** MEASURED NOTHING. The generated table and/or %s could not be"
              % MINT_PROMOTED_FILE)
        print("  *** read. Treat every `ported` figure above as an UNDERSTATEMENT of")
        print("  *** unknown size, and do not scope a lane on it.")
    else:
        newly_visible = vt_fold["newly_visible"]
        newly_ported = vt_fold["newly_ported"]
        applicable = vt_fold["applicable"]
        control = vt_fold["control"]
        print("  subject     %d minted vtables (denominator from %s;"
              % (vt_fold["vtables"], MINT_TABLES))
        print("              re-derive it with rt_mint_gen.py --check), of which")
        print("              %d are PROMOTED. Both figures are DERIVED on this run"
              % vt_fold["promoted_vtables"])
        print("              from the files named -- neither is cited from a doc.")
        print("  which       promotion count is STATIC: what %s"
              % MINT_PROMOTED_FILE)
        print("              registers IN SOURCE. A run with")
        print("              BANISHED_RT_MINT_PROMOTED=0 registers NONE of them, and")
        print("              for that run this whole fold-in is an OVERSTATEMENT.")
        print("              A source scan cannot see an env var; it does not try.")
        print("  ruling      an UNPROMOTED row's slots move NO column: its table is")
        print("              published only under BANISHED_RT_MINT=1, RT_DATA_SLOT")
        print("              falls through to the DLL, and RETAIL'S bodies run.")
        print("              READY is not a route (WAVE20_GUIDE.md §5 item 2).")
        print("  tier        a folded row has a DISPATCH ROUTE, not ATTENDANCE.")
        print("              Nothing here observes a call. Same tier the detour")
        print("              fold-in has carried since Wave 10.")
        print()
        print("  POSITIVE CONTROL -- this MUST be non-zero or the fold-in did nothing:")
        print("    %d of %d applicable rows moved."
              % (len(control), len(applicable)))
        print("    applicable = a slot body of a PROMOTED vtable that IS registered")
        print("    (%d such bodies). A slot of an unpromoted table, a PURECALL slot"
              % len(applicable))
        print("    and an unregistered slot are NOT applicable and are not scored.")
        if not applicable:
            print("    *** ZERO APPLICABLE CASES: this control cannot pass or fail.")
        elif not control:
            print("    *** CONTROL READ ZERO. Every applicable body was already")
            print("    *** visible through another door, or the fold is broken.")
        print()
        print("  SET-DIFF vs the row set before the fold (%d rows):" % n_prefold)
        print("    +%d rows NEWLY VISIBLE (a registration with no call door --"
              % len(newly_visible))
        print("       exactly the rows the old census could not build):")
        for k in newly_visible:
            mark = "  PORTED" if merged[k][3] else "  unported (slot holds a TRAP)"
            print("       %-62s %s" % (k, mark))
        print("    +%d rows already visible that FLIPPED to ported:" % len(newly_ported))
        for k in newly_ported:
            print("       %s" % k)
        if not newly_visible and not newly_ported:
            print("       (none -- every promoted slot body already had a door)")
        print()
        print("  PROMOTION BACKLOG -- registered bodies whose ONLY dispatch route is")
        print("  a vtable that is NOT promoted. These are NOT counted above, and")
        print("  they are the measure of Wave 20 Candidate A:")
        print("    %d bodies across %d unpromoted vtable(s), of which %d are"
              % (vt_fold["backlog_rows"], len(vt_fold["backlog"]),
                 len(vt_fold["backlog_complete"])))
        print("    COMPLETE (every non-purecall slot registered). COMPLETE is the")
        print("    STATIC half of readiness ONLY: it says promoting would put no")
        print("    trap in a live table. It does NOT say the row is promotable --")
        print("    that turns on owning the function that STAMPS the table, which")
        print("    no source scan can answer (WAVE20_GUIDE.md §2).")
        print("      %-52s %5s %5s  %s" % ("vtable", "ours", "slots", "complete"))
        for sym in sorted(set(vt_fold["backlog"]) | set(vt_fold["backlog_slots"])):
            print("      %-52s %5d %5d  %s"
                  % (sym, len(vt_fold["backlog"].get(sym, ())),
                     vt_fold["backlog_slots"].get(sym, 0),
                     "COMPLETE" if sym in vt_fold["backlog_complete"] else ""))
        if not vt_fold["backlog_slots"]:
            print("      (none)")

    ng = len(d["gated_exports"]) + len(d["gated_addrs"])
    if ng:
        print()
        print("%d registration(s) are present in the source but NOT counted above:" % ng)
        for f in sorted(GATED_OFF_FILES):
            n = sum(1 for v in list(d["gated_exports"].values()) + list(d["gated_addrs"].values())
                    if f in v)
            if n:
                print("  %-40s %d  (opt-in; its Register function returns 0 unless armed)" % (f, n))
        for f, gates in sorted(GATED_OFF_BLOCKS.items()):
            n = sum(1 for v in list(d["gated_exports"].values()) + list(d["gated_addrs"].values())
                    if f in v)
            if n:
                print("  %-40s %d  (opt-in; inside if(%s))"
                      % (f, n, ") / if(".join(gates)))
        print("  A gated registration is not a port. Arm it, measure it, then move it.")

    # The mint is a THIRD gating shape and it used to be reported as a port
    # shortfall, which was wrong twice over -- see MINT_FILE.
    nmint, nmint_named = mint_symbol_count(REPO)
    npromoted = mint_promoted_count(REPO)
    if nmint:
        print()
        print("%d DATA slot(s) are MINTED (%d carry an exported name; the other %d "
              "are UNEXPORTED and known only by address):"
              % (nmint, nmint_named, nmint - nmint_named))
        print("  %-40s (vtables + BaseTypeInfo + .data statics, from %s)"
              % (MINT_FILE, MINT_TABLES))
        print("  %d PROMOTED, i.e. registered on every run with no env var (%s)."
              % (npromoted, MINT_PROMOTED_FILE))
        print("  %d still registered only under BANISHED_RT_MINT=1, so by this "
              "script's own" % (nmint - npromoted))
        print("  rule those are NOT ports.")
        print()
        print("  ACCOUNTING, corrected 2026-09-09 (Wave 10 lane MINTSEL). This note")
        print("  used to end 'they therefore cannot move the columns above at all,")
        print("  in either direction'. That was wrong in both directions:")
        print("    * A minted row ALWAYS moved the *called* side. MINT_FILE is")
        print("      excluded from the ported scan but not the called scan, so an")
        print("      unregistered minted row that our tree resolves has always")
        print("      appeared under --unported. That is the correct accounting --")
        print("      the call really is unsatisfied by our code -- but the note")
        print("      denied it, and lane UIDATA spent a lane discovering that the")
        print("      13 UI rows it was sent after were of exactly that shape.")
        print("    * A PROMOTED row now moves the *ported* side too, and is counted")
        print("      by the ordinary scan above rather than here: reg_mint_promoted.cpp")
        print("      spells each symbol as a literal at its RegisterRuntimeExport")
        print("      call site so that no special case in this script is needed.")
        print("  What remains true is that a minted row has no BODY to reimplement,")
        print("  so it is not a shortfall in the function port.")
    nl = d.get("nonliteral_regs") or {}
    if nl:
        total = sum(len(v) for v in nl.values())
        print()
        print("*** WARNING: %d RegisterRuntimeExport call(s) pass a NON-LITERAL name."
              % total)
        print("*** These ports are LIVE in the process but INVISIBLE to this census,")
        print("*** so every number above UNDERSTATES what is actually registered.")
        for f in sorted(nl):
            print("  %-44s %s" % (f, ", ".join(sorted(set(nl[f])))))
        print("  A same-file `const char* const kX = \"?Mangled...\";` IS resolved now;")
        print("  these are something else -- a name built at runtime, or from a header.")
        print("  Fix by spelling the mangled name as a literal at the call site.")
        print("  (Wave 4 lane D hit this: 6 ports registered through kName* constants")
        print("   reported as IO 30/41 when 36 were registered. Nothing warned.)")
        print()
    nlc = d.get("nonliteral_calls") or {}
    if nlc:
        total = sum(len(v) for v in nlc.values())
        print()
        print("*** WARNING: %d ResolveExport call(s) pass an identifier this script"
              % total)
        print("*** cannot follow, so `called` -- and therefore `ported` -- UNDERSTATE.")
        for f in sorted(nlc):
            print("  %-44s %s" % (f, ", ".join(sorted(set(nlc[f])))))

    inst = d.get("instrument_detours") or {}
    if inst:
        print()
        print("%d detour(s) are INSTRUMENTS, not ports, and are excluded above:" % len(inst))
        for k in sorted(inst, key=lambda x: (isinstance(x, str), str(x))):
            shown = ("0x%x" % k) if isinstance(k, int) else k
            print("  %-44s %s" % (shown, ", ".join(sorted(inst[k]))))
        print("  A `probe:` recorder, a `selftest:` fake and an `[allocshadow]` shadow")
        print("  all patch the DLL, and none of them is a reimplementation. Only a")
        print("  label ending in `[port]` counts toward the detour column.")

    nld = d.get("nonliteral_detours") or {}
    if nld:
        total = sum(len(v) for v in nld.values())
        print()
        print("*** WARNING: %d detour target(s) are neither a literal address nor a"
              % total)
        print("*** same-file `constexpr uintptr_t kX = 0x...;` this script can follow.")
        print("*** The detour column UNDERSTATES by at least that much.")
        for f in sorted(nld):
            print("  %-44s %s" % (f, ", ".join(sorted(set(nld[f])))))

    print("ported = of those, taken over by a RegisterRuntime* call")
    print("detour = of those, ALSO patched in the DLL so its own internal callers "
          "come to us")

    if orphan_exports or orphan_addrs:
        print()
        print("Ports with no caller in our tree (%d exports, %d addresses) -- each row"
              "\ncarries a verdict below, or prints as UNTRIAGED:"
              % (len(orphan_exports), len(orphan_addrs)))

        # Bucket every orphan by its verdict. A row absent from
        # ORPHAN_VERDICTS is UNTRIAGED -- there is deliberately no state in
        # which a missing verdict reads as coverage.
        buckets = collections.defaultdict(list)
        for n in orphan_exports:
            st, wave, note = ORPHAN_VERDICTS.get(n, ("UNTRIAGED", "", ""))
            buckets[st].append(("export", n, wave, note))
        for a in orphan_addrs:
            st, wave, note = ORPHAN_VERDICTS.get(a, ("UNTRIAGED", "", ""))
            buckets[st].append(("addr", "0x%x" % a, wave, note))

        total = sum(len(v) for v in buckets.values())
        tally = "  ".join("%s=%d" % (s, len(buckets[s]))
                          for s in ORPHAN_STATUS_ORDER if buckets[s])
        print("  %d rows: %s" % (total, tally))

        for st in ORPHAN_STATUS_ORDER:
            rows_ = buckets.get(st)
            if not rows_:
                continue
            print()
            if st == "UNTRIAGED":
                # The actionable bucket. Named in full, because the whole point
                # of this section is that an untriaged caller-less port is
                # indistinguishable from a verified one until somebody looks.
                print("  ---- UNTRIAGED (%d) -- no verdict recorded. A caller-less port with no"
                      % len(rows_))
                print("       verdict is NOT evidence of anything; it is work nobody has done.")
                for kind, key, _w, _n in rows_:
                    print("    %-7s %s" % (kind, key))
                continue
            print("  ---- %s (%d)" % (st, len(rows_)))
            for kind, key, wave, note in rows_:
                print("    %-7s %s   [wave %s]" % (kind, key, wave))
                if note:
                    for line in textwrap.wrap(note, 84):
                        print("            %s" % line)

        # SELF-CHECK the old prose could not have had: a verdict whose key is
        # no longer caller-less. That happens when the port gains a caller --
        # good news -- but the verdict left behind still claims to describe a
        # caller-less port, so it is now wrong and must be retired.
        live = set(orphan_exports) | set(orphan_addrs)
        stale = [k for k in ORPHAN_VERDICTS if k not in live]
        if stale:
            print()
            print("  *** %d STALE VERDICT(S): the key is no longer caller-less, so the verdict"
                  % len(stale))
            print("  *** describes a port that now HAS a caller. Retire or rewrite each:")
            for k in sorted(stale, key=str):
                print("        %s" % (("0x%x" % k) if isinstance(k, int) else k))

    if args.unported:
        print()
        sel = [r for r in rows if not r[3]]
        if args.namespace:
            sel = [r for r in sel if r[2] == args.namespace]
        print("Called but not ported (%d):" % len(sel))
        unprom = load_unpromotable(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        n_unprom = 0
        for kind, key, ns, _ported, _detoured, files in sorted(sel, key=lambda r: (r[2], r[1])):
            tag = ""
            if key in unprom:
                slots, doc, expiry, cls = unprom[key]
                # "PORTED" is only true of the NO-OP class. A HARMFUL row is
                # deliberately NOT ported -- printing PORTED beside it would
                # assert the opposite of the verdict it carries.
                # startswith, not ==: WAVE 25 lane MANIFEST widened the class
                # field to admit "HARMFUL (DLL-present) / REQUIRED (cutover)".
                # An == test would have sent those five rows down the PORTED
                # branch, printing the exact assertion the comment above
                # forbids.
                if cls.upper().startswith("HARMFUL"):
                    tag = "   [ON THE DLL BY DESIGN · UNPROMOTABLE(HARMFUL) · %s]" % doc
                else:
                    tag = "   [PORTED %s · UNPROMOTABLE(%s) · %s]" % (slots, cls, doc)
                n_unprom += 1
            print("  %-8s %-12s %s%s" % (kind, ns, key, tag))
            print("           %s" % ", ".join(files[:4]) + (" …" if len(files) > 4 else ""))
            if key in unprom:
                print("           expires: %s" % unprom[key][2])
        if n_unprom:
            print()
            print("  %d of the %d above cannot be promoted, and the REASON CLASS matters:"
                  % (n_unprom, len(sel)))
            print("    NO-OP   -- ported in full; a lane proved no route could EVER dispatch")
            print("               through them, so promoting would change nothing.")
            print("    HARMFUL -- promoting would change behaviour FOR THE WORSE. These are")
            print("               not \"done\"; they are deliberately left on the DLL, and a")
            print("               future lane that promotes one would introduce a defect.")
            print("  Neither moves any count above. An UNPROMOTABLE tag is a")
            print("  WRITTEN VERDICT from the named lane doc (source of truth:")
            print("  standalone/reg_mint_promoted.cpp), never a heuristic, and a row loses it")
            print("  the moment its stated expiry condition is met.")

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            # symbol_kind is APPENDED, not inserted: every existing column keeps
            # its index, so an ad-hoc positional reader of the old five + callers
            # is unaffected. It is emitted for every row, ported or not, so a
            # consumer can re-derive the gap decomposition instead of trusting
            # the printed table.
            w.writerow(["kind", "key", "namespace", "ported", "detoured",
                        "callers", "symbol_kind"])
            for kind, key, ns, ported, detoured, files in sorted(rows, key=lambda r: (r[0], r[2], r[1])):
                w.writerow([kind, key, ns, int(ported), int(detoured),
                            ";".join(files), classify_symbol(key, kind)[0]])
        print()
        print("wrote %s (%d rows)" % (args.csv, len(rows)))

    return 0


if __name__ == "__main__":
    sys.exit(main())
