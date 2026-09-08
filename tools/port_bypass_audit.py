#!/usr/bin/env python3
"""Find PORTS THAT ARE BYPASSED: a call site that asks `ResolveAddr(Module::Runtime,
0x1800xxxxx)` for a function we have ALREADY ported and registered BY NAME.

WHY THIS EXISTS

`ResolveAddr(Module::Runtime, addr)` consults the ADDRESS registry only
(`RtAddrReg()`, standalone/resolve_standalone.cpp). A port registered with
`RegisterRuntimeExport("?Mangled@...", &Ours)` lives in a different map, so a
caller that asks by ADDRESS walks straight past it into the shipped DLL.

Nothing complains. The port is live, registered, and unused:

  * `BANISHED_RT_DIFF` never sees those calls -- it wraps our registered body,
    which is not the one running -- so the port reports `calls=0` or reports a
    call count that omits this site entirely.
  * `tools/runtime_census.py` folds address -> exported name through the DLL's
    export directory, so it sees the name, sees the registration, and counts the
    function PORTED.
  * At cutover the DLL is gone and the address resolves to nothing, so a
    function every progress number calls done becomes a missing symbol.

Wave 9 lane 0 found three of these by running the process under
`BANISHED_RT_STRICT=1` and diffing the residue against the census. This script
answers the follow-up question -- *are there more?* -- from source, so it can gate
in CI rather than needing a nine-cell harness matrix.

It is the third hat of `reference_uncalled_port_is_not_verified.md`: not a port
with no callers, but a port whose callers go around it.

THE FIX AT A FLAGGED SITE

Change the CALL SITE to ask by NAME:

    fn = (T)ResolveExport("?Mangled@Name@@...");   // hits the port registry

NOT "make ResolveAddr fall back to the export map". That papers over the site,
still needs the shipped DLL's export directory to map address -> name, and cannot
work at all once the DLL is deleted. Asking by name is the only form that
survives cutover.

WHAT IS DELIBERATELY NOT FLAGGED

A by-address call to something the DLL does NOT export is correct and often the
only option -- `Collection::Array<T>` and `Collection::Set<T>` are header-only
templates with zero exports in either build, so `runtime/src/ui/element.cpp:226`
calls one by address on purpose, with eighteen lines of comment saying so. Those
sites have no name to ask for; they are counted and listed under --all, never
flagged.

Likewise a site whose address IS registered with `RegisterRuntimeAddr` (the
address door is served) or carries an installed detour (the shipped bytes jump to
us) reaches the port and is not a bypass.

AND -- the correction that took this number from 35 to 5 -- a GUARDED FALLBACK is
not a bypass either:

    fn = (T)ResolveExport("?Mangled@...");     // <- this hits the port
    if (!fn) fn = (T)ResolveAddr(Module::Runtime, 0x1800xxxxx);   // dead

The primary asks by name, so the port IS reached; the address branch never runs.
A first version of this script flagged all 30 of those and the count read as an
order-of-magnitude finding. They are `tools/resolve_fallback_audit.py`'s
territory (166 sites, all of which lane F must DELETE before cutover), and this
script reuses that script's pairing logic to recognise them rather than
re-deriving it. A bypass is an UNGUARDED by-address call.

THE SECOND DOOR: ResolveExportRaw
=================================

A port can be bypassed through the NAME door too, and the address sweep above
cannot see it by construction. `ResolveExportRaw(name)` takes the mangled name
and **deliberately skips the port registry**, so that `RT_ORIGINAL_ONCE`,
`RtOriginal` and body-inspection checks can reach the SHIPPED implementation.
That deliberate skip is the whole point of the door — and it is a silent bypass
anywhere else.

Wave 9 lane P hit exactly that porting `System::Thread::ThreadFunc`: leaving the
`Thread` ctor on `ResolveExportRaw` would mean "the port registers, the census
counts it, and every thread still runs the DLL's body".

So the second pass sweeps every direct `ResolveExportRaw` / `ResolveOrdinalRaw`
call site whose name we have ported, and classifies it:

  OK-DATA       the name is exported DATA (`??_7…`, `?_singleton@…`,
                `?_typeInfo@…`, `?_first…`, `?_seconds…`). There is no body to
                port, so raw is the only thing raw could mean. Lane C mints these.
  OK-UNPORTED   we have not ported that name, so raw and non-raw would reach the
                same DLL function. (It still returns null at cutover — that is
                the raw-door verdict list, a separate item, not a bypass.)
  OK-ORACLE     the name IS ported, but the site is oracle machinery: inside a
                diff/save-trace guard, a probe, a self-test, a detour installer,
                or a body-byte check. Legitimate — this is what the door is for.
  BYPASS        the name is ported and the site is a live call path. Fix it by
                switching to `ResolveExport`.

`RT_ORIGINAL_ONCE` and `RtOriginal<>` ARE swept, since 2026-09-07 -- and the
first version of this script was wrong to skip them. It reasoned that they are
oracle machinery "by definition", which is true of the macro and false of nine
call sites: `??DQuaternion`, `??0Geometry`, `??1Geometry`, `??1External`,
`??4String`, `??HSystem`, `??YString`, `?Set@String` and `?AdjustDelimiters` were
all reached through `RT_ORIGINAL_ONCE` on ordinary live paths, every one of them
ported. Wave 1 proved `??DQuaternion` bit-exact and `MeshInstance::UpdateHierarchy`
had been running the DLL's copy ever since. **A door's documented purpose is not
evidence about what goes through it**, and "by definition" was a reason not to
look.

What makes the sweep tractable is that the guard analysis comes from
`tools/rawdoor_audit.py` (Wave 9 lane V) rather than from a fourth copy here: it
already knows, per site, whether the raw door sits inside a diff guard, and it
carries a 13-entry allowlist of indirect guards WITH the evidence for each. This
script asks its own question -- "is the name one we have ported?" -- and combines
the two. The fix at a flagged site is `RT_EXPORT_ONCE`, the port-registry twin of
`RT_ORIGINAL_ONCE` added to `runtime/funcs/rt_port.h` for exactly this.

THE BLIND SPOT THAT WAS MEASURED, AND CLOSED (Wave 9 lane H)
============================================================

Until 2026-09-07 "is the resolved pointer CALLED?" meant "is the resolve
immediately applied to an argument list on this same line?". That is one
spelling of a call and it is not the common one. The common one is the
accessor:

    ReleaseFn release = RT_EXPORT_ONCE(ReleaseFn, "?Release@String@…");
    release(&tmp);                     // <- the call, a statement later

`runtime/src/system/value_types.cpp` held THREE live-path `String::Release`
bypasses. **This script reported ONE** -- the immediate-call spelling. The other
two classified `OK-RESOLVE-ONLY`, a verdict that is correct for a pointer that
is only stored and wrong the moment it is called. Two more, `widgets_input.cpp`'s
`StringToInteger()` / `RealNumberEditElementSetValue()` accessors, were the same
shape and were reported by no checker at all. All five were found by hand-grepping
a mangled name.

`rawdoor_audit.py` now follows the assignment target through the brace-matched
enclosing function, so `called` means "reached on a live path" by either
spelling, and `OK-RESOLVE-ONLY` now carries the evidence for its verdict (which
local, in which function, with no unguarded use). `--self-test` proves it against
the known answer: `tools/fixtures/bypass_value_types_before.cpp` is that file as
it stood, and the hardened classifier reports 3/3 where the old one reported 1.

A SECOND hole fell out of the cross-check below: the macro sweep read mangled
names from the resolve's own LINE, so a site whose name sits on a continuation
line -- the normal formatting here --

    RtOriginal<Box*(*)(const Box*, Box*, const Vector3*)>(
        "?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z")(self, &theirs, p);

had no name to match and was `continue`d past without any verdict at all. That
was **76 sites absent from a checker printing a clean pass** (361 -> 437). Names
are statement-scoped now.

`--by-name`: THE CROSS-CHECK THAT CANNOT BE DEFEATED BY PARSING
==============================================================

For every mangled name we have REGISTERED, grep the whole tree for that literal
and report every raw-door spelling of it. No verdicts -- a grep cannot tell an
oracle site from a bypass -- but it is a strict superset of the parser's
population, and comparing the two answers the question the parser cannot answer
about itself: *is the population the whole population?* It is what found the
`widgets_input.cpp` pair by hand and what found the 76 above. It exits non-zero
when it sees a raw-door statement the parser did not, and the count is printed
whether or not it is zero. **Run it after every port that LANDS**, not after
every edit: a site is correct when written and becomes a bypass when a different
lane ports its name, with nothing in that file changing.

**How the classification is paired, and its one honest limit.** The gate is looked
for in the ENCLOSING FUNCTION's body, brace-matched — not in a fixed line window.
A window is what let a twelve-line comment silently delete a site from
`resolve_fallback_audit.py` (166 -> 162, and the vanished row was the one just
fixed). The limit of function-granularity: a function that does oracle work AND
has a live raw call on another path classifies as OK-ORACLE. The verdict therefore
prints the gate that decided it, so the claim is checkable; with the ported set
this small (9 sites at the time of writing) every one is meant to be read.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/port_bypass_audit.py
    ... --all        list every site and its verdict, not just bypasses
    ... --by-name    the grep cross-check (see above); use this at the gate
    ... --self-test  prove the classifier against the known-answer fixture
    ... --csv PATH   write every site to CSV
    ... --quiet      exit code only

EXIT CODES
    0  no ported function is reached by address
    1  at least one bypass, or --by-name saw a raw-door statement the parser
       did not
    2  the shipped DLL is not there to map addresses onto names (the same
       refusal-to-answer as runtime_census.py: with no export directory this
       script cannot tell a bypass from a header-only template call, and a
       checker that returns 0 when it cannot check is how this project got here)
"""
import argparse, csv, importlib.util, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

ADDR_SITE = re.compile(
    r"Resolve(?:Addr\(\s*Module::Runtime\s*,|RuntimeRaw\()\s*(0x[0-9a-fA-F]+)\s*\)")


def load_census():
    """Import runtime_census.py as a module and reuse its scanner.

    Sharing the scanner rather than re-implementing it is the point: the census
    has been wrong about what counts as a live registration five separate times
    (gated files, gated `if` blocks, non-literal names, dx11's own resolver,
    address-only bucketing). A second copy of that logic would drift, and the
    two tools would disagree about which ports exist.
    """
    spec = importlib.util.spec_from_file_location(
        "runtime_census", os.path.join(HERE, "runtime_census.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_fallback_audit():
    spec = importlib.util.spec_from_file_location(
        "resolve_fallback_audit", os.path.join(HERE, "resolve_fallback_audit.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def guarded_fallback_sites():
    """{(relpath, line)} of every guarded ResolveExport->ResolveAddr fallback.

    Straight out of resolve_fallback_audit.find_sites(), because the two scripts
    must agree about what a fallback is. If that pairing logic changes, this
    exclusion changes with it.
    """
    fb = load_fallback_audit()
    out = set()
    for site in fb.find_sites(REPO):
        rel = os.path.relpath(site["file"], REPO).replace(os.sep, "/")
        out.add((rel, site["line"]))
    return out


# --- the name door -----------------------------------------------------------

RAW_SITE = re.compile(r"Resolve(?:ExportRaw|OrdinalRaw)\s*\(\s*")
# The macro/template spellings of the same door. Swept via rawdoor_audit's
# per-site guard analysis; see the module docstring for why skipping them was a
# defect rather than a simplification.
RAW_MACRO = re.compile(r"RT_ORIGINAL_ONCE\s*\(|RtOriginal\s*<")
# `RegisterRuntimeExport("<mangled>", (void*)&Target)` -> Target, so a port that
# calls the very name it is registered under can be told apart from one that
# calls a sibling. UI::GraphElement::RemoveGraphs is the former: its whole body
# is a call to the DLL's RemoveGraphs, so switching it to ResolveExport would
# recurse into itself. That is not a bypass to fix here -- it is a port that
# never really replaced anything, and a cutover row of its own.
REG_TARGET = re.compile(
    r'RegisterRuntimeExport\s*\(\s*"((?:[^"\\]|\\.)+)"\s*,\s*'
    r'\(\s*void\s*\*\s*\)\s*&\s*([A-Za-z_][\w:]*)', re.S)


def registered_targets(census):
    out = {}
    for path in census.iter_sources():
        try:
            text = census.strip_comments(
                open(path, encoding="utf-8", errors="replace").read())
        except OSError:
            continue
        for m in REG_TARGET.finditer(text):
            out[m.group(1)] = m.group(2).split("::")[-1]
    return out
# The resolver itself and the two headers that DECLARE these entry points.
RESOLVER_FILES = {"runtime/funcs/rt_port.h", "standalone/resolve.h",
                  "standalone/resolve_standalone.cpp"}
# Exported DATA prefixes: a vtable, a singleton slot, a BaseTypeInfo, a constant.
DATA_PREFIXES = ("??_7", "?_singleton@", "?_typeInfo@", "?_first", "?_seconds")
# Markers that make an enclosing function oracle machinery rather than a live path.
ORACLE_MARKERS = ("rt_diff_on__", "RtSaveTrace::", "RtDiffEnabled", "RT_DIFF",
                  "RtDiffReport", "Verify", "memcmp", "RtOriginal",
                  "InstallRuntimeDetour")
ORACLE_FN_WORDS = ("verify", "probe", "selftest", "prologue", "trace", "detour",
                   "original", "shadow")
# A thin accessor whose whole job is to hand back exported DATA -- `RtVtable`,
# `RtDataExport<T>`, `…SingletonSlot`. Its argument is the caller's literal, so
# the name is not visible here; the function's own name is what says "data".
DATA_FN_WORDS = ("vtable", "singleton", "typeinfo", "dataexport", "slot")
NAME_CONST = re.compile(
    r'\b(?:static\s+)?(?:const|constexpr)\s+char\s*\*\s*(?:const\s+)?'
    r'([A-Za-z_]\w*)\s*(?:\[\])?\s*=\s*"((?:[^"\\]|\\.)*)"')
def enclosing_function(text, pos):
    """(name, body) of the brace-matched function definition containing `pos`.

    Brace matching rather than a line window, for the reason in the module
    docstring: a window's answer changes when someone adds a comment. The
    INNERMOST containing definition wins, so a helper defined inside a namespace
    block is attributed to the helper.

    The implementation lives in `rawdoor_audit` and is imported rather than
    copied: that module's deferred-call pass has to agree with this one about
    where a function begins, and two copies of a brace matcher is how this
    project gets a checker that disagrees with its own gate.
    """
    name, body, _start = load_rawdoor().enclosing_function(text, pos)
    return name, body


def sweep_name_door(census, ported_names):
    rows = []
    for path in census.iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        if rel in RESOLVER_FILES:
            continue
        try:
            raw_text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        text = census.strip_comments(raw_text)
        consts = {m.group(1): m.group(2) for m in NAME_CONST.finditer(text)}
        for m in RAW_SITE.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            name = census.concat_literal(text, m.end())
            if not name:
                ident = re.match(r"([A-Za-z_]\w*)\s*\)", text[m.end():])
                name = consts.get(ident.group(1), "") if ident else ""
            fn, body = enclosing_function(text, m.start())
            gate = next((g for g in ORACLE_MARKERS if g in body), "")
            if not gate:
                w = next((w for w in ORACLE_FN_WORDS if w in fn.lower()), "")
                gate = ("fn name: " + w) if w else ""
            if not name:
                # A name this script cannot follow (a parameter, a table walked
                # in a loop). Inside oracle machinery that is expected and needs
                # no verdict; anywhere else it is a row a HUMAN must read, which
                # is why it is reported rather than assumed benign.
                dw = next((w for w in DATA_FN_WORDS if w in fn.lower()), "")
                verdict, why = (("OK-ORACLE", gate) if gate
                                else ("OK-DATA", "fn name: " + dw) if dw
                                else ("UNRESOLVED-NAME", "name not a literal"))
            elif name.startswith(DATA_PREFIXES):
                verdict, why = "OK-DATA", "exported data, no body to port"
            elif name not in ported_names:
                verdict, why = "OK-UNPORTED", "not ported; raw == non-raw today"
            else:
                verdict = "OK-ORACLE" if gate else "BYPASS"
                why = gate or "live path"
            rows.append((verdict, rel, line, fn, name, why))
    return rows


_RAWDOOR = None


def load_rawdoor():
    global _RAWDOOR
    if _RAWDOOR is None:
        spec = importlib.util.spec_from_file_location(
            "rawdoor_audit", os.path.join(HERE, "rawdoor_audit.py"))
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        _RAWDOOR = mod
    return _RAWDOOR


def sweep_macro_door(census, ported_names, paths=None):
    """RT_ORIGINAL_ONCE / RtOriginal<> sites naming a function we have ported.

    The guard bit and the allowlist come from tools/rawdoor_audit.py, which owns
    that analysis (brace-tracked diff guards, RT_DIFF_* argument lists spanning
    lines, and 13 hand-verified indirect guards each carrying its evidence).
    Duplicating it here would give the project two checkers that disagree.
    """
    rd = load_rawdoor()
    targets = registered_targets(census)
    rows = []
    for path in (rd.walk(REPO) if paths is None else paths):
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        fs = rd.scan(path)
        if fs.error:
            rows.append(("UNSCANNABLE", rel, 0, "", "", fs.error))
            continue
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        consts = {m.group(1): m.group(2) for m in NAME_CONST.finditer(text)}
        for site in fs.sites:
            line, guarded, names, code = (site["line"], site["guarded"],
                                          site["names"], site["text"])
            # THE HOLE THIS CLOSES.  `called` used to mean "the resolve is
            # IMMEDIATELY applied to an argument list on this line" -- one
            # spelling, and not the common one.  A resolve stored in a local and
            # called a statement later classified OK-RESOLVE-ONLY, a verdict
            # that is right for a pointer that is only stored and wrong the
            # moment it is called.  rawdoor_audit now follows the assignment
            # target through the brace-matched enclosing function, so `called`
            # here means "reached on a live path", by either spelling.
            live = site["deferred_live"]
            called = site["called"] or bool(live)
            if not RAW_MACRO.search(code):
                continue          # the direct door, already swept above
            cand = [n for n in names if n.startswith("?")]
            if not cand:
                for ident in re.findall(r"\b(k[A-Z]\w*)\b", code):
                    if ident in consts:
                        cand.append(consts[ident])
            hit = [n for n in cand if n in ported_names]
            if not hit:
                continue
            key = "%s:%d" % (rel, line)
            fn, _body = enclosing_function(census.strip_comments(text),
                                           sum(len(l) + 1 for l in
                                               text.split("\n")[:line - 1]))
            body = _body
            if key in rd.ALLOWLIST:
                v, why = "OK-ALLOWLIST", rd.ALLOWLIST[key]
            elif guarded:
                v, why = "OK-ORACLE", "inside a diff guard"
            elif not called:
                # LANE V'S DISTINCTION, and it is the whole rule here: storing
                # the pointer outside a guard is harmless, CALLING outside one is
                # not. `?IsEffectPlaying` looks identical to the two real defects
                # at a glance -- the difference is that every call through its
                # local is inside a guard, which is now CHECKED (rawdoor_audit's
                # deferred pass), not assumed from the absence of an argument
                # list on the resolve's own line.
                v, why = ("OK-RESOLVE-ONLY",
                          "pointer stored; no unguarded use of `%s` in %s"
                          % (site.get("target") or "it", fn or "this function"))
            elif targets.get(hit[0]) == fn:
                # The port IS this function, so ResolveExport would call itself.
                v, why = "OK-DELEGATING", ("this port's body IS a call to the DLL "
                                           "under its own name -- a cutover row, "
                                           "not a bypass")
            elif "g_portOn" in body:
                # The `if (!g_portOn)` arm: the port declined to register, so
                # reaching the shipped body is the POINT. Null-checked since
                # Wave 9 lane R -- see runtime/src/io/serializer.cpp.
                v, why = "OK-PORT-OFF", "the !g_portOn fallback arm"
            elif any(w in fn.lower() for w in ORACLE_FN_WORDS):
                v, why = "OK-ORACLE", "oracle fn name"
            else:
                if live:
                    u = live[0]
                    why = ("live path -- `%s` %s at %s:%d, outside every guard"
                           % (site.get("target"), u["kind"], rel, u["line"]))
                else:
                    why = "live path"
                v = "BYPASS"
            rows.append((v, rel, line, fn, hit[0], why))
    return rows


# --- the belt-and-braces cross-check ----------------------------------------
#
# Everything above parses. This does not, and that is the point: it takes the
# set of mangled names we have REGISTERED and greps the tree for each literal,
# reporting every raw-door spelling of it. It is a strict superset of the
# parser's population and cannot be defeated by a shape the parser has not been
# taught -- which is exactly how `widgets_input.cpp`'s two accessor bypasses
# were found, by hand, after the parser reported the file clean.
#
# It is not a replacement for the parser (it has no verdicts -- an oracle site
# and a bypass look identical to a grep). It is the control that says whether
# the parser's population is the whole population. Read it whenever a port
# lands.
RAW_TOKEN = re.compile(r"ResolveExportRaw|ResolveRuntimeRaw|ResolveOrdinalRaw|"
                       r"RT_ORIGINAL_ONCE|RtOriginal\s*<")


def sweep_by_name(census, ported_names, covered):
    """Every raw-door STATEMENT mentioning a name we have ported.

    Pairing is by STATEMENT (back to the previous `;`/`{`/`}`, forward to the
    next `;`), never by a line window: an `RT_ORIGINAL_ONCE` whose mangled name
    sits on a continuation line is the normal formatting in this tree, and a
    window is what a comment moves.

    Comments are stripped first. A policy comment containing a worked example of
    a mangled symbol made `export_name_audit.py` report a fabricated 663rd
    phantom; this tool's own docstring quotes several mangled names and would do
    the same to itself.
    """
    rd = load_rawdoor()
    rows = []
    for path in rd.walk(REPO):
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        if rel in RESOLVER_FILES:
            continue
        try:
            text = census.strip_comments(
                open(path, encoding="utf-8", errors="replace").read())
        except OSError:
            continue
        # `k…` constants holding a mangled name count as a spelling of it.
        aliases = {}
        for m in NAME_CONST.finditer(text):
            if m.group(2) in ported_names:
                aliases[m.group(1)] = m.group(2)
        needles = [(n, n) for n in ported_names if n in text]
        needles += [(a, n) for a, n in aliases.items()]
        for needle, name in needles:
            for m in re.finditer(re.escape(needle), text):
                lo = max(text.rfind(c, 0, m.start()) for c in ";{}") + 1
                hi = text.find(";", m.end())
                hi = len(text) if hi < 0 else hi
                door = RAW_TOKEN.search(text[lo:hi])
                if not door:
                    continue
                # Report the line of the RAW DOOR, not of the name literal. The
                # normal formatting in this tree puts the mangled name on a
                # continuation line, so reporting the literal's line made 79
                # sites the parser HAD seen read as "PARSER MISSED" -- the first
                # number being too big, again, and this time a fabricated
                # disagreement between two checkers rather than a real one.
                line = text.count("\n", 0, lo + door.start()) + 1
                rows.append((rel, line, name,
                             "seen by the parser" if (rel, line) in covered
                             else "PARSER MISSED THIS SITE"))
    # One row per (file, line, name): a statement spanning several lines is one
    # site, and a constant plus its literal must not double-count.
    return sorted(set(rows))


def run_self_test(census, ported_names):
    """The fixture with the KNOWN ANSWER.

    `tools/fixtures/bypass_value_types_before.cpp` is `value_types.cpp` as it
    stood at 955f78a~1, with three live-path `String::Release` bypasses. The
    pre-hardening classifier found one. This asserts the hardened one finds all
    three -- and, on the tree, that all three are gone.
    """
    fx = os.path.join(HERE, "fixtures", "bypass_value_types_before.cpp")
    if not os.path.isfile(fx):
        print("SELF-TEST CANNOT RUN -- missing %s" % fx, file=sys.stderr)
        return 2
    rows = sweep_macro_door(census, ported_names, paths=[fx])
    rel = os.path.relpath(fx, REPO).replace(os.sep, "/")
    bad = sorted(r[2] for r in rows
                 if r[0] == "BYPASS" and "Release@String" in r[4])
    print("fixture %s: %d raw-door sites naming a ported function, "
          "%d BYPASS on String::Release at lines %s"
          % (rel, len(rows), len(bad), bad))
    for r in rows:
        if r[0] == "BYPASS":
            print("  %-8s :%d  (in %s)  %s\n      [%s]"
                  % (r[0], r[2], r[3], r[4], r[5]))
    ok = True
    if bad != [330, 349, 403]:
        print("SELF-TEST FAILED -- expected BYPASS at fixture lines "
              "[330, 349, 403] (the accessor shape at :330/:349 is what the old "
              "classifier missed); got %s. The matcher is broken, and a clean "
              "tree proves nothing until this passes." % bad, file=sys.stderr)
        ok = False

    live = [r for r in sweep_macro_door(census, ported_names)
            if r[0] == "BYPASS"
            and r[1] == "runtime/src/system/value_types.cpp"]
    if live:
        print("SELF-TEST FAILED -- the repaired value_types.cpp sites are "
              "bypasses again: %s" % live, file=sys.stderr)
        ok = False
    if not ok:
        return 2
    print("SELF-TEST OK -- 3/3 found on the known-answer fixture "
          "(the old classifier found 1), 0 in the repaired tree.")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--csv")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--by-name", action="store_true",
                    help="the grep cross-check: every raw-door spelling of every "
                         "REGISTERED mangled name, parsed or not")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the classifier on the known-answer fixture")
    args = ap.parse_args()

    census = load_census()
    exports = census.load_export_addrs()   # exits 2 if it cannot answer
    scan = census.scan()
    ported_names = set(scan["ported_exports"])
    if args.self_test:
        return run_self_test(census, ported_names)
    ported_addrs = set(scan["ported_addrs"])
    detoured = {k for k in scan["detoured"] if isinstance(k, int)}
    detoured_names = {k for k in scan["detoured"] if not isinstance(k, int)}
    fallbacks = guarded_fallback_sites()

    rows = []
    for path in census.iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        text = census.strip_comments(text)
        for m in ADDR_SITE.finditer(text):
            addr = int(m.group(1), 16)
            line = text.count("\n", 0, m.start()) + 1
            name = exports.get(addr, "")
            if (rel, line) in fallbacks:
                verdict = "GUARDED-FALLBACK"   # dead; resolve_fallback_audit owns it
            elif not name:
                verdict = "NO-EXPORT"      # header-only template &c -- correct
            elif addr in ported_addrs:
                verdict = "ADDR-REGISTERED"
            elif addr in detoured or name in detoured_names:
                verdict = "DETOURED"
            elif name in ported_names:
                verdict = "BYPASS"
            else:
                verdict = "UNPORTED"       # ordinary DLL call, lane B/C/D work
            rows.append((verdict, rel, line, "0x%x" % addr, name))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))
    name_rows = sweep_name_door(census, ported_names)
    name_rows += sweep_macro_door(census, ported_names)
    name_rows.sort(key=lambda r: (r[0], r[1], r[2]))
    bad = [r for r in rows if r[0] == "BYPASS"]
    name_bad = [r for r in name_rows
                if r[0] in ("BYPASS", "UNRESOLVED-NAME", "UNSCANNABLE")]

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["door", "verdict", "file", "line", "address_or_fn",
                        "export_name", "note"])
            for verdict, rel, line, addr, name in rows:
                w.writerow(["address", verdict, rel, line, addr, name, ""])
            for verdict, rel, line, fn, name, why in name_rows:
                w.writerow(["name", verdict, rel, line, fn, name, why])

    if not args.quiet:
        def tally(rs):
            c = {}
            for r in rs:
                c[r[0]] = c.get(r[0], 0) + 1
            return "  ".join("%s=%d" % kv for kv in sorted(c.items()))

        print("ADDRESS door: %d ResolveAddr(Module::Runtime, 0x…) sites; "
              "%d reach a function we have ALREADY ported by name"
              % (len(rows), len(bad)))
        print("  " + tally(rows))
        for verdict, rel, line, addr, name in rows:
            if verdict != "BYPASS" and not args.all:
                continue
            print("  %-16s %s:%d" % (verdict, rel, line))
            print("      %s   %s" % (addr, name or "(not exported)"))

        print()
        print("NAME door: %d raw-door sites (direct ResolveExportRaw plus the "
              "RT_ORIGINAL_ONCE / RtOriginal<> spellings); %d are a ported function "
              "on a live path"
              % (len(name_rows), len([r for r in name_rows if r[0] == "BYPASS"])))
        print("  " + tally(name_rows))
        for verdict, rel, line, fn, name, why in name_rows:
            if (verdict not in ("BYPASS", "UNRESOLVED-NAME", "UNSCANNABLE")
                    and not args.all):
                continue
            print("  %-16s %s:%d  (in %s)" % (verdict, rel, line, fn or "?"))
            print("      %s   [%s]" % (name or "(name not a literal)", why))

    missed = []
    if args.by_name:
        covered = {(r[1], r[2]) for r in name_rows}
        byname = sweep_by_name(census, ported_names, covered)
        missed = [r for r in byname if r[3] != "seen by the parser"]
        print()
        print("BY-NAME cross-check: %d ported names; %d raw-door statements "
              "mention one; %d of those the parser did NOT see"
              % (len(ported_names), len(byname), len(missed)))
        for rel, line, name, note in byname:
            if note == "seen by the parser" and not args.all:
                continue
            print("  %-22s %s:%d" % (note, rel, line))
            print("      %s" % name)
    return 1 if (bad or name_bad or missed) else 0


if __name__ == "__main__":
    sys.exit(main())
