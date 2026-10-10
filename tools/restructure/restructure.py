"""Phase D restructure tool: move whole directories and rewrite #include lines so every
include still resolves to the same file under the new layout.

    python tools/restructure/restructure.py <plan.py> [--apply]

The plan file is a Python module defining:
    MOVES     : list of (old_prefix, new_prefix) applied to tracked paths, first match wins;
                a prefix ending in "/" moves a directory, otherwise an exact file
    RENAMES   : dict old_path -> new_path (exact, applied before MOVES)
    OLD_INCS  : the -I directories before the move, in Makefile order
    NEW_INCS  : the -I directories after the move, in Makefile order
    SRC_ROOTS : the directories whose source files are scanned for #include lines

Method: for every `#include "x"` in every source file, resolve x under the OLD layout
(includer's directory first, then OLD_INCS in order), map the resolved file through the
move, then check whether the ORIGINAL spelling still resolves to that same file under the
NEW layout (new includer directory, NEW_INCS). If it does, the line is untouched. If not,
the spelling is rewritten to the first NEW_INCS-relative form that resolves, else to a
path relative to the includer. Nothing semantic changes: a header that resolved to file F
before resolves to F (moved) after. Without --apply the tool only reports.
"""
import importlib.util
import os
import re
import subprocess
import sys

INCLUDE_RE = re.compile(rb'^(\s*#\s*include\s*")([^"]+)(".*)$')
SRC_EXT = {".cpp", ".h", ".hpp", ".inc", ".c", ".rc"}


def load_plan(path):
    spec = importlib.util.spec_from_file_location("plan", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def tracked(root, dirs):
    out = subprocess.check_output(["git", "ls-files", "--", *dirs], cwd=root)
    return [l for l in out.decode("utf-8").replace("\r", "").split("\n") if l]


def map_path(p, plan):
    if p in plan.RENAMES:
        return plan.RENAMES[p]
    for old, new in plan.MOVES:
        if old.endswith("/"):
            if p.startswith(old):
                return new + p[len(old):]
        elif p == old:
            return new
    return p


def norm(p):
    return os.path.normpath(p).replace("\\", "/")


def resolve(spelling, includer_dir, incs, exists):
    """Mimic GCC's quoted-include search: includer's dir, then -I dirs in order."""
    cand = norm(os.path.join(includer_dir, spelling)) if includer_dir else norm(spelling)
    if cand in exists:
        return cand
    for inc in incs:
        cand = norm(os.path.join(inc, spelling)) if inc != "." else norm(spelling)
        if cand in exists:
            return cand
    return None


def canonical(target_new, includer_new_dir, new_incs, new_exists):
    """First NEW_INCS-relative spelling that resolves to target_new, else a relative path."""
    for inc in new_incs:
        base = "" if inc == "." else inc.rstrip("/") + "/"
        if target_new.startswith(base):
            sp = target_new[len(base):]
            if resolve(sp, includer_new_dir, new_incs, new_exists) == target_new:
                return sp
    rel = norm(os.path.relpath(target_new, includer_new_dir or "."))
    if resolve(rel, includer_new_dir, new_incs, new_exists) == target_new:
        return rel
    return None


def main():
    plan_path = sys.argv[1]
    apply = "--apply" in sys.argv
    root = os.getcwd()
    plan = load_plan(plan_path)

    all_files = tracked(root, plan.SRC_ROOTS)
    old_exists = set(all_files)
    mapping = {p: map_path(p, plan) for p in all_files}
    new_exists = set(mapping.values())
    if len(new_exists) != len(old_exists):
        seen = {}
        for o, n in mapping.items():
            if n in seen:
                print(f"COLLISION: {o} and {seen[n]} both map to {n}")
            seen[n] = o
        sys.exit(2)
    # Windows: case-insensitive collisions in one directory
    lower = {}
    for n in new_exists:
        k = n.lower()
        if k in lower and lower[k] != n:
            print(f"CASE COLLISION: {n} vs {lower[k]}")
            sys.exit(2)
        lower[k] = n

    moved = {o: n for o, n in mapping.items() if o != n}
    print(f"files tracked: {len(all_files)}; moving: {len(moved)}")

    rewrites = {}  # old path -> new bytes
    stats = {"lines": 0, "kept": 0, "rewritten": 0, "unresolved": 0, "external": 0}
    unresolved = []
    for old in all_files:
        if os.path.splitext(old)[1].lower() not in SRC_EXT:
            continue
        with open(os.path.join(root, old), "rb") as f:
            data = f.read()
        new = mapping[old]
        old_dir = os.path.dirname(old)
        new_dir = os.path.dirname(new)
        out_lines = []
        changed = False
        for line in data.split(b"\n"):
            m = INCLUDE_RE.match(line)
            if not m:
                out_lines.append(line)
                continue
            stats["lines"] += 1
            spelling = m.group(2).decode("utf-8")
            target_old = resolve(spelling, old_dir, plan.OLD_INCS, old_exists)
            if target_old is None:
                stats["external"] += 1  # system header or a non-tracked file
                if "--verbose" in sys.argv:
                    print("  external", old, spelling)
                out_lines.append(line)
                continue
            target_new = mapping[target_old]
            if resolve(spelling, new_dir, plan.NEW_INCS, new_exists) == target_new:
                stats["kept"] += 1
                out_lines.append(line)
                continue
            sp = canonical(target_new, new_dir, plan.NEW_INCS, new_exists)
            if sp is None:
                stats["unresolved"] += 1
                unresolved.append((old, spelling, target_new))
                out_lines.append(line)
                continue
            stats["rewritten"] += 1
            out_lines.append(m.group(1) + sp.encode("utf-8") + m.group(3))
            changed = True
        if changed:
            rewrites[old] = b"\n".join(out_lines)

    print(f"include lines: {stats['lines']}; kept {stats['kept']}; rewritten {stats['rewritten']}; "
          f"external {stats['external']}; UNRESOLVED {stats['unresolved']}; files rewritten {len(rewrites)}")
    for u in unresolved[:50]:
        print("  UNRESOLVED", u)
    if unresolved:
        sys.exit(3)
    if not apply:
        for o, n in list(moved.items())[:10]:
            print("  mv", o, "->", n)
        return

    # 1. git mv every moved file (dirs created as needed)
    for o, n in moved.items():
        os.makedirs(os.path.join(root, os.path.dirname(n)), exist_ok=True)
        subprocess.check_call(["git", "mv", "-k", o, n], cwd=root)
    # 2. write the rewritten contents at the new paths
    for o, data in rewrites.items():
        with open(os.path.join(root, mapping[o]), "wb") as f:
            f.write(data)
    # 3. drop now-empty directories
    for o in moved:
        d = os.path.join(root, os.path.dirname(o))
        while d and os.path.isdir(d) and not os.listdir(d) and os.path.abspath(d) != os.path.abspath(root):
            os.rmdir(d)
            d = os.path.dirname(d)
    # 4. write the path map for the script / makefile rewriters
    with open(os.path.join(root, "tools", "restructure", "last_move_map.txt"), "w") as f:
        for o, n in sorted(moved.items()):
            f.write(f"{o} {n}\n")
    print("applied")


if __name__ == "__main__":
    main()
