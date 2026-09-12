"""Partition the REMAINING cutover surface into modules, for wave planning.

Two independent inputs, deliberately kept separate rather than unioned into one
number -- a union hides which screen saw what
(reference_union_control_hides_a_blind_screen):

  A. PREDICTED vtable walls  -- tools/mint_wall_predict.py's third section:
     RT_MINT_NAMED rows with neither name nor address registered. Static.
  B. OBSERVED name residue   -- the [RtUnresolved] list from a DLL-absent boot.
     Dynamic, boot-path only, and therefore a LOWER bound on what is reachable.

WHAT THIS CANNOT SEE, stated up front because every instrument here has had an
unannounced blind spot:
  * Registrations spelled through a VARIABLE or named constant are invisible to
    the literal scan, so (A) is an UPPER bound on the walls, never a lower one.
  * (B) only names what THIS boot asked for. Everything past the first fault is
    unobserved -- the residue cannot shrink-to-zero its way to a scope.
  * Neither screen sees non-virtual exported functions that no minted slot and
    no executed path reached. Those are real work and are NOT counted here.

So: (A) over-scopes, (B) under-scopes. Planning off either alone is wrong; the
overlap is what to do FIRST, and (A)-minus-(B) is the backlog.
"""
import re
import sys
from collections import defaultdict

SIGNATURE = re.compile(r"^[A-Z]{4}")


def owner(sym):
    """Return the fully-qualified owner ('Video::Device::Adapter') of a mangled name.

    MSVC writes qualifiers INNERMOST-FIRST after the method name and terminates
    the chain with '@@', so the owner is that chain reversed. Taking only the
    first two tokens splits a nested class across two buckets -- it filed
    ??0Adapter@Device@Video@@ under 'Video::Device' and
    ?FindOrAddMode@Adapter@Device@Video@@ under 'Device::Adapter', i.e. the same
    real class in two rows, which is exactly the kind of split that makes a
    partition under-count a lane.

    A bare digit is a back-reference to an earlier name in the same symbol
    (index 0 = the method name, then each qualifier in order).
    """
    if not sym.startswith("?"):
        return "other"
    # '??0'/'??1' are ctor/dtor opcodes: three chars, not two.
    if sym[:3] in ("??0", "??1"):
        body = sym[3:]
    elif sym.startswith("??"):
        body = sym[2:]
    else:
        body = sym[1:]
    parts = body.split("@")
    if len(parts) < 2:
        return "other"
    # For a ctor/dtor ('??0Adapter@Device@Video@@') the FIRST token is the class
    # itself, not a method name, so it belongs in the qualifier chain. Without
    # this the ctors file under the PARENT (Video::Device) while the class's own
    # methods file under Video::Device::Adapter -- one class, two lanes.
    ctor_dtor = sym[:3] in ("??0", "??1")
    seen, quals = [parts[0]], ([parts[0]] if ctor_dtor else [])
    for tok in parts[1:]:
        if tok == "":
            break                     # '@@' -- end of the qualifier chain
        if tok.isdigit():
            idx = int(tok)
            tok = seen[idx] if idx < len(seen) else tok
        elif SIGNATURE.match(tok):
            # A '?X@Class@1@UEAAPEAVExternal@Object@@XZ' form has NO '@@' closing
            # its qualifier chain -- the first '@@' belongs to the RETURN TYPE.
            # Splitting on that terminator yields 'Object::UEAAPEAVExternal::...'.
            # Access/calling-convention codes open with 4+ capitals (UEAA, QEBA,
            # AEAA); the class names in this corpus are CamelCase and do not.
            break
        quals.append(tok)
        seen.append(tok)
    if not quals:
        return "other"
    return "::".join(reversed(quals))


def load(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            s = line.strip()
            if s.startswith("?"):
                out.append(s)
    return out


def main():
    walls = load(sys.argv[1])
    residue = set(load(sys.argv[2])) if len(sys.argv) > 2 else set()

    groups = defaultdict(list)
    for w in walls:
        groups[owner(w)].append(w)

    res_groups = defaultdict(list)
    for r in residue:
        res_groups[owner(r)].append(r)

    print("PREDICTED vtable walls: %d in %d modules" % (len(walls), len(groups)))
    print("OBSERVED residue      : %d in %d modules" % (len(residue), len(res_groups)))
    print()

    hot = set(res_groups) & set(groups)
    print("=== MODULES ===  (* = also in the observed residue: reached on the boot path)")
    print("%-46s %6s %8s" % ("module", "walls", "observed"))
    for mod in sorted(groups, key=lambda m: (-len(groups[m]), m)):
        mark = "*" if mod in hot else " "
        print("%s%-45s %6d %8d" % (mark, mod, len(groups[mod]), len(res_groups.get(mod, []))))

    only_res = sorted(set(res_groups) - set(groups))
    if only_res:
        print()
        print("=== OBSERVED but NOT predicted as vtable walls (%d modules) ===" % len(only_res))
        print("These are name doors, not minted slots -- (A) structurally cannot see them.")
        for mod in only_res:
            print("  %-44s %d" % (mod, len(res_groups[mod])))
            for s in sorted(res_groups[mod]):
                print("      %s" % s)


if __name__ == "__main__":
    main()
