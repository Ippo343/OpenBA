"""Generate tests/unit/sources.cmake from tests/unit/sources.mk (the Makefile stays the gate's
build until the kit scripts are ported; the two lists must say the same thing).

    python tools/restructure/sources_mk_to_cmake.py
"""
import re

MK = "tests/unit/sources.mk"
CM = "tests/unit/sources.cmake"

srcs = []
with open(MK, encoding="utf-8") as f:
    text = f.read().replace("\\\n", " ")
for line in text.split("\n"):
    line = line.split("#", 1)[0].strip()
    m = re.match(r"UNIT_TREE_SRCS\s*\+=\s*(.*)$", line)
    if m:
        srcs += m.group(1).split()

seen = set()
ordered = [s for s in srcs if not (s in seen or seen.add(s))]
with open(CM, "w", encoding="utf-8", newline="\n") as f:
    f.write("# GENERATED from tests/unit/sources.mk by tools/restructure/sources_mk_to_cmake.py -- edit the .mk\n")
    f.write("# and regenerate. The tree sources the unit tests link against, one family per block there.\n")
    f.write("set(UNIT_TREE_SRCS\n")
    for s in ordered:
        f.write(f"  ${{CMAKE_SOURCE_DIR}}/{s}\n")
    f.write(")\n")
print(f"{len(ordered)} sources -> {CM}")
