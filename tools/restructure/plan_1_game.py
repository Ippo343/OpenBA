# Phase D.3 step 1: code/Game -> src/game
SRC_ROOTS = ["code", "runtime", "standalone", "dx11", "tests"]
OLD_INCS = [".", "code/Game", "runtime/types", "runtime/funcs", "dx11/types", "standalone", "tests/unit"]
NEW_INCS = [".", "src/game", "runtime/types", "runtime/funcs", "dx11/types", "standalone", "tests/unit"]
RENAMES = {}
MOVES = [("code/Game/", "src/game/")]
