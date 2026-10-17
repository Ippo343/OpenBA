"""The Banished install directory for the python tools (the one game-directory variable).

    game_dir()      env BANISHED_DIR, default C:/GOG Games/Banished (the same name and default as
                    tools/build.sh and every shell script under tools/)
    retail(name)    that directory joined with a retail file name, e.g. retail("Runtime-x64.dll")

Tools under tools/ import it with their own directory on sys.path:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from banished_dir import retail
"""
import os

DEFAULT_GAME_DIR = "C:/GOG Games/Banished"


def game_dir():
    return os.environ.get("BANISHED_DIR") or DEFAULT_GAME_DIR


def retail(name):
    return os.path.normpath(os.path.join(game_dir(), name))
