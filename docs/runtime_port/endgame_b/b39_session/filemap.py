"""B.3 step 9 -- the file map generator (B39_FILEMAP.md section 1).

Run from the repo root:  python docs/runtime_port/endgame_b/b39_session/filemap.py [--check]
It lists the git-tracked source files in scope (every *.cpp *.h *.hpp *.inc *.c under
runtime/ standalone/ engine/ code/Game/ dx11/ tests/unit/, plus the tool / build files named in
COORD_FILES), assigns each to exactly ONE owner by the rules below (first match wins) and prints
the per-owner lists as markdown. --check only verifies: 0 unassigned, 0 assigned twice.
The rules ARE the file map; B39_FILEMAP.md section 1 is this script's output pasted verbatim.

HISTORICAL since the B.3.9 close: several COORD_FILES below (the cutover manifest, the tag file,
the deleted mint paths) no longer exist; the script still runs, it just names them.
"""
import subprocess, sys, re, collections, os

EXTS = (".cpp", ".h", ".hpp", ".inc", ".c")
SCOPE = ("runtime/", "standalone/", "engine/", "code/Game/", "dx11/", "tests/unit/")

# ---- coordinator-owned (B39_FILEMAP section 2 says what a block may hand in for each) ----
COORD_FILES = {
    "Makefile", "tests/unit/sources.mk", "tests/unit/main.cpp", "tests/unit/unit.h",
    "tests/unit/link_stubs.cpp", "tests/unit/rt_port_stub.cpp", "tests/unit/collection_link_stubs.cpp",
    "tools/rt_mint_gen.py", "tools/cutover_manifest.py", "tools/dispatch_slot_map.py",
    "runtime/funcs/rt_port.h", "runtime/funcs/rt_resolve_cache.cpp", "runtime/funcs/rt_sink.h",
    "runtime/funcs/runtime_funcs.cpp", "runtime/funcs/runtime_funcs.h", "runtime/types/common.h",
    "standalone/rt_data_mint.cpp", "standalone/rt_data_mint_tables.inc", "standalone/rt_export_addrs.inc",
    "standalone/cutover_manifest.cpp", "standalone/cutover_manifest.inc", "standalone/reg_mint_promoted.cpp",
    "standalone/resolve_standalone.cpp", "standalone/options.h", "standalone/options.cpp",
    "engine/funcs/engine_funcs.h", "engine/funcs/engine_funcs.cpp",
}
# ---- the reflection fold (D6) ----
RF_FILES = {
    "standalone/main.cpp", "standalone/factory_enum_stub.cpp", "standalone/dlgctrl_route.cpp",
    "code/Game/Goal/statgoal.cpp", "code/Game/Game/game_main.cpp",
}
RF_DIRS = ("code/Game/Object/",)
# ---- the closing owner's registrars (they empty out as the caller batches convert) ----
CLOSE_RE = re.compile(r"^standalone/reg_[a-z0-9_]*typeids?\.cpp$|^standalone/reg_unreg_lane4\.cpp$")
# ---- the exe-side construction block (D8): builders, copy sites, exe callers of Runtime doors ----
EXE_FILES = {
    "code/Game/Action/Action.h", "code/Game/Action/action_base_complete.cpp", "code/Game/Action/showtool.cpp",
    "code/Game/Component/CitizenComponent.h", "code/Game/Component/clearareacomponent_states.cpp",
    "code/Game/Component/component.cpp", "code/Game/Component/Component.h",
    "code/Game/Component/modelcomponent.cpp", "code/Game/Component/ModelComponent.h",
    "code/Game/Component/storagedisplaycomponent.cpp", "code/Game/Component/citizenai_actionctors.cpp",
    "code/Game/Component/citizenai_handlers.cpp", "code/Game/Component/controller_state_table.h",
    "code/Game/Dialog/edittool_select.cpp", "code/Game/Dialog/mainmenudialog.cpp", "code/Game/Dialog/mapui.cpp",
    "code/Game/Dialog/moddialog.cpp", "code/Game/Dialog/newgamedialog.cpp", "code/Game/Dialog/noiseui.cpp",
    "code/Game/Dialog/saveloaddialog.cpp", "code/Game/Dialog/thumb_vtables.cpp", "code/Game/Dialog/toolbardialog.cpp",
    "code/Game/Game/game_leaf_helpers.cpp", "code/Game/Game/gameupdateloop.cpp", "code/Game/Game/keystone_a_gamedtor.cpp",
    "code/Game/Game/menudialogloopt.cpp", "code/Game/Game/minimap_paint.cpp", "code/Game/Game/minimap_upload.cpp",
    "code/Game/Game/pathfinder_seed.cpp", "code/Game/Game/serialize_image_upload.cpp",
    "code/Game/Game/w2_takescreenshot.cpp", "code/Game/Game/gameupdateloop_serialize.cpp",
    "code/Game/Game/GameUpdateLoop.h", "code/Game/Game/MenuDialogLoopT.h", "code/Game/Game/GameLoadDialogLoopT.h",
    "code/Game/Game/GameLoaderLoop.h", "code/Game/Game/InnerLoaderLoop.h",
    "code/Game/Player/fencedtool.cpp", "code/Game/Player/placementtool.cpp", "code/Game/Player/tool_base_render.cpp",
    "code/Game/Tutorial/tutorial_next_scenario.cpp", "code/Game/Tutorial/tutorialui.cpp",
    "code/Game/Work/consumeproduce.cpp", "code/Game/Work/work_act_clear.cpp", "code/Game/Work/work_act_hunt.cpp",
    "code/Game/Work/work_act_path.cpp", "code/Game/Work/work_act_perform.cpp",
    "code/Game/Work/work_base_actionctor.cpp", "code/Game/Work/work_base_reimpl.cpp",
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
    if path.startswith("runtime/src/") or path.startswith("runtime/types/"):
        ns = path.split("/")[2]
        if ns == "ui":
            return ui_owner(path)
        if ns in NS:
            return NS[ns]
        return "COORD"
    if path.startswith("runtime/funcs/"):
        return "SYS"
    if path.startswith("dx11/"):
        return "VID"
    if path.startswith("engine/types/"):
        return "EXE"
    if path.startswith("standalone/"):
        return "EXE"
    if path.startswith("code/Game/"):
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
