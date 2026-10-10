# Phase D.3 step 4: standalone -> src/app (the launcher) + src/diag (diagnostics, harnesses)
import subprocess
SRC_ROOTS = ["src", "standalone", "tests"]
OLD_INCS = [".", "src/game", "src/engine", "src/render/dx11", "standalone", "tests/unit"]
NEW_INCS = ["src/game", "src/engine", "src/render/dx11", "src/app", "src/diag", "tests/unit"]
APP = {
    "main.cpp", "options.cpp", "options.h", "runtime_init.cpp", "runtime_init.h",
    "video_interface.cpp", "video_interface.h", "boot_window.cpp", "boot_window.h",
    "frame_limiter.cpp", "frame_limiter.h", "engine_loader_gate.cpp", "engine_loader_gate.h",
    "res_load_lock.cpp", "res_load_lock.h", "icon.rc", "log_strip.h",
    "perf_experimental.cpp", "perf_experimental.h",
}
RENAMES = {}
for f in subprocess.check_output(["git", "ls-files", "standalone"]).decode().split():
    base = f[len("standalone/"):]
    RENAMES[f] = ("src/app/" if base in APP else "src/diag/") + base
MOVES = []
