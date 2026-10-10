# Phase D.3 step 3: dx11 -> src/render/dx11
SRC_ROOTS = ["src", "standalone", "dx11", "tests"]
OLD_INCS = [".", "src/game", "src/engine", "dx11/types", "standalone", "tests/unit"]
NEW_INCS = [".", "src/game", "src/engine", "src/render/dx11", "standalone", "tests/unit"]
RENAMES = {"dx11/src/README.md": "src/render/dx11/SOURCES.md"}
MOVES = [("dx11/src/", "src/render/dx11/"), ("dx11/types/", "src/render/dx11/"), ("dx11/", "src/render/dx11/")]
