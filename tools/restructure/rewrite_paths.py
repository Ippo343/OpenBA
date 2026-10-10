"""Rewrite tree paths in text (scripts, Makefiles, docs, source comments) after a restructure
step.

    python tools/restructure/rewrite_paths.py <move_map.txt> <rules.txt> <path-or-dir>...

move_map.txt : "old new" per line (exact file paths; written by restructure.py)
rules.txt    : "old_prefix new_prefix" per line, applied in order after the exact map
Only occurrences preceded by a non-path character are rewritten (so `standalone/x` in
`foo/standalone/x` is left alone). Line endings are preserved byte for byte.
"""
import os
import re
import sys

SKIP_DIRS = {".git", "build", "build-perf", "build-og", "worktrees", "ToolKit", "Banished_RE.rep",
             "__pycache__", ".venv", "renderdoc_mcp", "renderdoc_build", "obj", "history", "runs"}
TEXT_EXT = {".md", ".sh", ".py", ".txt", ".csv", ".h", ".hpp", ".cpp", ".c", ".rc", ".mk", ".json",
            ".yml", ".yaml", ".cmake", ".ps1", ".bat", ".cfg", ".ini", ".inc"}
NAMED = {"Makefile", "CMakeLists.txt", ".gitignore"}
BOUNDARY = r"(?<![A-Za-z0-9_/.\\-])"


def load_pairs(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            a, b = line.split()
            out.append((a, b))
    return out


def main():
    move_map = load_pairs(sys.argv[1])
    rules = load_pairs(sys.argv[2])
    targets = sys.argv[3:]
    # exact files first (longest first so a/b/c.h wins over a/b), then prefix rules in order
    move_map.sort(key=lambda p: len(p[0]), reverse=True)
    compiled = [(re.compile(BOUNDARY + re.escape(a) + r"(?![A-Za-z0-9_])"), b) for a, b in move_map]
    compiled += [(re.compile(BOUNDARY + re.escape(a)), b) for a, b in rules]

    files = changed = 0

    def process(path):
        nonlocal files, changed
        base = os.path.basename(path)
        ext = os.path.splitext(path)[1].lower()
        if ext not in TEXT_EXT and base not in NAMED:
            return
        with open(path, "rb") as f:
            raw = f.read()
        if b"\x00" in raw[:4096]:
            return
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            return
        new = text
        for rx, rep in compiled:
            new = rx.sub(rep, new)
        files += 1
        if new != text:
            with open(path, "wb") as f:
                f.write(new.encode("utf-8"))
            changed += 1

    for t in targets:
        if os.path.isfile(t):
            process(t)
            continue
        for dirpath, dirnames, filenames in os.walk(t):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for fn in filenames:
                process(os.path.join(dirpath, fn))
    print(f"scanned {files} files, rewrote {changed}")


if __name__ == "__main__":
    main()
