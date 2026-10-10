# Phase D.3 step 2: runtime -> src/engine (types + src folded per namespace, funcs at the root),
# the 35 *_win.cpp seams -> src/platform/win
import subprocess
SRC_ROOTS = ["src", "runtime", "standalone", "dx11", "tests"]
OLD_INCS = [".", "src/game", "runtime/types", "runtime/funcs", "dx11/types", "standalone", "tests/unit"]
NEW_INCS = [".", "src/game", "src/engine", "dx11/types", "standalone", "tests/unit"]
RENAMES = {
    "runtime/src/io/serializer.h": "src/engine/io/serializer_internal.h",
    "runtime/src/system/string.h": "src/engine/system/string_internal.h",
    "runtime/src/system/timer.h": "src/engine/system/timer_internal.h",
    "runtime/src/system/widestring.h": "src/engine/system/widestring_internal.h",
}
for f in subprocess.check_output(["git", "ls-files", "runtime/src"]).decode().split():
    if f.endswith("_win.cpp"):
        RENAMES[f] = "src/platform/win/" + f.rsplit("/", 1)[1]
MOVES = [("runtime/types/", "src/engine/"), ("runtime/src/", "src/engine/"), ("runtime/funcs/", "src/engine/"),
         ("runtime/", "src/engine/")]
