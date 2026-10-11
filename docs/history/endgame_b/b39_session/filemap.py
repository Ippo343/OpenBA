"""B.3 step 9 -- the file map generator (B39_FILEMAP.md section 1).

Run from the repo root:  python docs/runtime_port/endgame_b/b39_session/filemap.py [--check]
It lists the git-tracked source files in scope (every *.cpp *.h *.hpp *.inc *.c under
runtime/ standalone/ engine/ src/game/ src/render/dx11/ tests/unit/, plus the tool / build files named in
COORD_FILES), assigns each to exactly ONE owner by the rules below (first match wins) and prints
the per-owner lists as markdown. --check only verifies: 0 unassigned, 0 assigned twice.
The rules ARE the file map; B39_FILEMAP.md section 1 is this script's output pasted verbatim.

HISTORICAL since the B.3.9 close: several COORD_FILES below (the cutover manifest, the tag file,
the deleted mint paths) no longer exist; the script still runs, it just names them.
"""
import subprocess, sys, re, collections, os

EXTS = (".cpp", ".h", ".hpp", ".inc", ".c")
SCOPE = ("runtime/", "standalone/", "engine/", "src/game/", "src/render/dx11/", "tests/unit/")

# ---- coordinator-owned (B39_FILEMAP section 2 says what a block may hand in for each) ----
COORD_FILES = {
    "Makefile", "tests/unit/sources.mk", "tests/unit/main.cpp", "tests/unit/unit.h",
    "tests/unit/link_stubs.cpp", "tests/unit/rt_port_stub.cpp", "tests/unit/collection_link_stubs.cpp",
    "tools/rt_mint_gen.py", "tools/cutover_manifest.py", "tools/dispatch_slot_map.py",
    "src/engine/rt_port.h", "src/engine/rt_resolve_cache.cpp", "src/engine/rt_sink.h",
    "src/engine/runtime_funcs.cpp", "src/engine/runtime_funcs.h", "src/engine/common.h",
    "standalone/rt_data_mint.cpp", "standalone/rt_data_mint_tables.inc", "standalone/rt_export_addrs.inc",
    "standalone/cutover_manifest.cpp", "standalone/cutover_manifest.inc", "standalone/reg_mint_promoted.cpp",
    "standalone/resolve_standalone.cpp", "src/app/options.h", "src/app/options.cpp",
    "engine/funcs/engine_funcs.h", "engine/funcs/engine_funcs.cpp",
}
# ---- the reflection fold (D6) ----
RF_FILES = {
    "src/app/main.cpp", "standalone/factory_enum_stub.cpp", "standalone/dlgctrl_route.cpp",
    "src/game/Goal/statgoal.cpp", "src/game/Game/game_main.cpp",
}
RF_DIRS = ("src/game/Object/",)
# ---- the closing owner's registrars (they empty out as the caller batches convert) ----
CLOSE_RE = re.compile(r"^standalone/reg_[a-z0-9_]*typeids?\.cpp$|^standalone/reg_unreg_lane4\.cpp$")
# ---- the exe-side construction block (D8): builders, copy sites, exe callers of Runtime doors ----
EXE_FILES = {
    "src/game/Action/Action.h", "src/game/Action/action_base_complete.cpp", "src/game/Action/showtool.cpp",
    "src/game/Component/CitizenComponent.h", "src/game/Component/clearareacomponent_states.cpp",
    "src/game/Component/component.cpp", "src/game/Component/Component.h",
    "src/game/Component/modelcomponent.cpp", "src/game/Component/ModelComponent.h",
    "src/game/Component/storagedisplaycomponent.cpp", "src/game/Component/citizenai_actionctors.cpp",
    "src/game/Component/citizenai_handlers.cpp", "src/game/Component/controller_state_table.h",
    "src/game/Dialog/edittool_select.cpp", "src/game/Dialog/mainmenudialog.cpp", "src/game/Dialog/mapui.cpp",
    "src/game/Dialog/moddialog.cpp", "src/game/Dialog/newgamedialog.cpp", "src/game/Dialog/noiseui.cpp",
    "src/game/Dialog/saveloaddialog.cpp", "src/game/Dialog/thumb_vtables.cpp", "src/game/Dialog/toolbardialog.cpp",
    "src/game/Game/game_leaf_helpers.cpp", "src/game/Game/gameupdateloop.cpp", "src/game/Game/keystone_a_gamedtor.cpp",
    "src/game/Game/menudialogloopt.cpp", "src/game/Game/minimap_paint.cpp", "src/game/Game/minimap_upload.cpp",
    "src/game/Game/pathfinder_seed.cpp", "src/game/Game/serialize_image_upload.cpp",
    "src/game/Game/w2_takescreenshot.cpp", "src/game/Game/gameupdateloop_serialize.cpp",
    "src/game/Game/GameUpdateLoop.h", "src/game/Game/MenuDialogLoopT.h", "src/game/Game/GameLoadDialogLoopT.h",
    "src/game/Game/GameLoaderLoop.h", "src/game/Game/InnerLoaderLoop.h",
    "src/game/Player/fencedtool.cpp", "src/game/Player/placementtool.cpp", "src/game/Player/tool_base_render.cpp",
    "src/game/Tutorial/tutorial_next_scenario.cpp", "src/game/Tutorial/tutorialui.cpp",
    "src/game/Work/consumeproduce.cpp", "src/game/Work/work_act_clear.cpp", "src/game/Work/work_act_hunt.cpp",
    "src/game/Work/work_act_path.cpp", "src/game/Work/work_act_perform.cpp",
    "src/game/Work/work_base_actionctor.cpp", "src/game/Work/work_base_reimpl.cpp",
}

# ---- Runtime namespace blocks ----
NS = {
    "object": "OBJ",
    "system": "SYS", "collection": "SYS", "math": "SYS", "volume": "SYS", "utility": "SYS", "crt": "SYS",
    "io": "IO",
    "video": "VID", "graphics": "GFX",
    "engine": "ENG", "save": "ENG", "achievement": "ENG", "audio": "ENG", "input": "ENG",
    "pathfinding": "ENG", "picking": "ENG", "application": "ENG", "collision2d": "ENG",
}
# UI: the widget pairs split by the existing leaf tests (families stay whole, B39_RULES section 4)
UI_A = ["numberedit", "animatedimage", "border", "button", "check", "clipregion", "combo", "group",
        "image", "page", "radio", "edit", "label", "number", "text"]
UI_B = ["documenttext", "container", "layout", "list", "scroll", "table", "tab", "graph", "progress",
        "ribbon", "slider", "spinner"]
UI_NOT_WIDGET = ("combodialog", "element.cpp", "element_", "elementdescription", "description_common",
                 "description_dtors", "descriptionsbatch1")

def ui_owner(path):
    stem = os.path.basename(path).lower()
    if stem.startswith(UI_NOT_WIDGET) or stem in ("element.h", "elementdescription.h"):
        return "UI-D"
    for fam, blk in sorted([(f, "UI-A") for f in UI_A] + [(f, "UI-B") for f in UI_B],
                           key=lambda t: -len(t[0])):
        if stem.startswith(fam) and (("description" in stem) or ("element" in stem) or stem.startswith("documenttext")):
            return blk
    return "UI-D"

TEST_RULES = [  # (prefix, owner)
    ("ui_leaf_c1", "UI-A"), ("ui_leaf_c2", "UI-A"), ("ui_leaf_c3", "UI-A"),
    ("ui_leaf_c4", "UI-B"), ("ui_leaf_c5", "UI-B"),
    ("ui_", "UI-D"), ("rb_ui", "UI-D"),
    ("object_", "OBJ"),
    ("system_", "SYS"), ("string_", "SYS"), ("filename_", "SYS"), ("alloc_door", "SYS"), ("array_", "SYS"),
    ("list_", "SYS"), ("set_", "SYS"), ("math_", "SYS"),
    ("io_", "IO"),
    ("video_", "VID"), ("rb_video", "VID"),
    ("graphics_", "GFX"), ("rb_graphics", "GFX"),
    ("achievement_", "ENG"), ("application_", "ENG"), ("audio_", "ENG"), ("engine_loop", "ENG"),
    ("input_", "ENG"), ("pathfinding_", "ENG"), ("save_", "ENG"), ("rb_engine", "ENG"),
    ("rb_achievement", "ENG"),
]

def batch_owner(path):
    parts = path.split("/")
    d, name = parts[2], parts[-1].lower()
    if d == "Component":
        return "C1" if name[0] < "i" else "C2"
    if d == "Work":
        return "C3"
    if d in ("Action", "Goal", "Entity"):
        return "C4"
    return "C5"   # Dialog, Player, Tutorial, Game

def owner(path):
    if path in COORD_FILES:
        return "COORD"
    if path in RF_FILES or path.startswith(RF_DIRS):
        return "RF"
    if CLOSE_RE.match(path):
        return "CLOSE"
    if path in EXE_FILES:
        return "EXE"
    if path.startswith("src/engine/") or path.startswith("src/engine/"):
        ns = path.split("/")[2]
        if ns == "ui":
            return ui_owner(path)
        if ns in NS:
            return NS[ns]
        return "COORD"
    if path.startswith("src/engine/"):
        return "SYS"
    if path.startswith("src/render/dx11/"):
        return "VID"
    if path.startswith("engine/types/"):
        return "EXE"
    if path.startswith("standalone/"):
        return "EXE"
    if path.startswith("src/game/"):
        return batch_owner(path)
    if path.startswith("tests/unit/"):
        name = path.split("/")[-1]
        for pre, own in TEST_RULES:
            if name.startswith(pre):
                return own
        return "EXE"
    return None

def main():
    files = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True).stdout.split()
    inscope = [f for f in files if (f.startswith(SCOPE) and f.endswith(EXTS)) or f in COORD_FILES]
    seen, by = collections.Counter(inscope), collections.defaultdict(list)
    for f in inscope:
        o = owner(f)
        by[o].append(f)
    dup = [f for f, n in seen.items() if n > 1]
    una = by.pop(None, [])
    total = sum(len(v) for v in by.values())
    print(f"<!-- filemap.py: {len(inscope)} files in scope, {total} assigned, {len(una)} unassigned, {len(dup)} twice -->")
    if una or dup:
        print("UNASSIGNED:", una, "DUP:", dup)
        sys.exit(1)
    if "--check" in sys.argv:
        return
    order = ["COORD", "OBJ", "SYS", "IO", "VID", "GFX", "UI-A", "UI-B", "UI-D", "ENG", "EXE", "RF",
             "C1", "C2", "C3", "C4", "C5", "CLOSE"]
    print("\n| owner | files |\n|---|---:|")
    for o in order:
        print(f"| {o} | {len(by.get(o, []))} |")
    for o in order:
        fl = sorted(by.get(o, []))
        print(f"\n### {o} ({len(fl)} files)\n")
        groups = collections.OrderedDict()
        for f in fl:
            d, _, n = f.rpartition("/")
            groups.setdefault(d, []).append(n)
        for d, ns in groups.items():
            print(f"- `{d}/`: " + ", ".join(ns))

if __name__ == "__main__":
    main()
