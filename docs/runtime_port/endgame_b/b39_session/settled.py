"""B.3 step 9 -- the SETTLED list generator (B39_RULES.md section 1).

Run from the repo root:  python docs/runtime_port/endgame_b/b39_session/settled.py
Reads the block-0 inventories (INVENTORY_A table A, INVENTORY_B table T4, INVENTORY_D table A.1), the
generated mint (`?_typeInfo@` rows) and tools/rt_mint_retired/*.txt, and prints one row per minted
vtable / RT_DELDTOR row / BaseTypeInfo record / storage cell with its OWNER block and WAVE by the rules
of B39_RULES section 1.1 (encoded below; the rules are the authority, this is their mechanical form).
"""
import os, re, sys, glob, collections

# HISTORICAL since the B.3.9 close: the RT_DELDTOR table (runtime/src/object/deleting_dtor.cpp) is
# deleted and the mint holds no vtable / BaseTypeInfo row, so there is nothing left to settle. The
# numbers of the finished step are in CLOSE_NUMBERS.md; `git show 851adb15:<this file>` ran last.
if not os.path.exists("runtime/src/object/deleting_dtor.cpp"):
    print("settled.py: HISTORICAL -- step 9 is closed (deleting_dtor.cpp and every minted vtable / "
          "BaseTypeInfo row are gone); see b39_session/CLOSE_NUMBERS.md")
    sys.exit(0)

S = "docs/runtime_port/endgame_b/b39_session/"

def table(path, header_start):
    lines = open(path, encoding="utf-8").read().splitlines()
    i = next(k for k, l in enumerate(lines) if l.startswith(header_start))
    hdr = [c.strip() for c in lines[i].strip().strip("|").split("|")]
    out, j = [], i + 2
    while j < len(lines) and lines[j].startswith("|"):
        cells = [c.strip() for c in lines[j].strip().strip("|").split(" | ")]
        out.append(dict(zip(hdr, cells)))
        j += 1
    return out

def retired():
    keys = {}
    for f in sorted(glob.glob("tools/rt_mint_retired/*.txt")):
        for l in open(f, encoding="utf-8"):
            l = l.split("#", 1)[0].strip()
            if l:
                p = l.split()
                keys[p[0]] = os.path.basename(f)
    return keys

UI_A = {"AnimatedImage", "Border", "Button", "Check", "ClipRegion", "Combo", "Edit", "Group", "Image",
        "Label", "Number", "NumberEdit", "Page", "Radio", "Text"}
UI_B = {"Container", "DocumentText", "Graph", "Layout", "List", "Progress", "Ribbon", "Scroll", "Slider",
        "Spinner", "Tab", "Table"}

def block_of(qual):
    """qual = 'NS::Class'."""
    ns, _, cls = qual.partition("::")
    if ns == "Object": return "OBJ"
    if ns == "System": return "SYS"
    if ns == "IO": return "IO"
    if ns == "Video": return "VID"
    if ns == "Graphics": return "GFX"
    if ns == "UI":
        stem = re.sub(r"(Description|Element)$", "", cls)
        if cls != stem and stem in UI_A: return "UI-A"
        if cls != stem and stem in UI_B: return "UI-B"
        return "UI-D"
    return "ENG"

# ---- section 1.1 wave rules for the vtable rows (and so their RT_DELDTOR rows) ----
# W2 = waits for another block (named); W3 = a base whose subtree crosses blocks; else W1.
VT_WAVE = {
    # IO: the one exe creator (gameupdateloop_serialize.cpp:160) waits for EXE-2; its bases follow it
    "IO::CompressedMemoryStream": ("W2", "after EXE-2 (exe builder gameupdateloop_serialize.cpp:160)"),
    "IO::CompressedStream": ("W2", "after 32"),
    "IO::Stream": ("W2", "after every IO leaf (M3)"),
    # VID
    "Video::ImageBuffer": ("W2", "after GFX 61 and EXE-1 (the 0x14015c8c0 copy site, 5 builders)"),
    # GFX
    "Graphics::MaterialInstance": ("W2", "after EXE-1 (thumb_vtables.cpp copy, 4 builders)"),
    "Graphics::MeshInstance": ("W2", "after EXE-1 (7 exe slot callers + menudialogloopt.cpp)"),
    "Graphics::FrameCopyImageBuffer": ("W2", "after EXE-1 (w2_takescreenshot.cpp:151)"),
    "Graphics::EdgeMesh": ("W2", "after EXE-2 (tool_base_render.cpp:431)"),
    "Graphics::EdgeMeshInstance": ("W2", "after EXE-2 (tool_base_render.cpp:512); port the NULL ~Class first (INVENTORY_B finding 2)"),
    "Graphics::ParticleSystemInstance": ("W2", "after EXE-2 (menudialogloopt.cpp)"),
    "Graphics::ParticleInstance": ("W2", "after 100"),
    "Graphics::GeneratedImageBuffer": ("W2", "after 60, 110"),
    "Graphics::Geometry": ("W2", "after 49, 89, 81 (M3)"),
    # UI-D
    "UI::SpriteSheet": ("W2", "after EXE-1 (thumb_vtables.cpp copy, 4 builders)"),
    "UI::DialogInstance": ("W2", "after EXE-2 (6-arg door, 4 exe sites) and 27, 141"),
    "UI::Element": ("W2", "after UI-A, UI-B and every UI-D element (M3)"),
    "UI::ElementDescription": ("W2", "after UI-A, UI-B and every UI-D description (M3)"),
    # ENG
    "Engine::DialogLoaderLoopT": ("W2", "after EXE-1 (loop builders L2/L6/L9/L12/L15)"),
    "Engine::DialogLoopT": ("W2", "after EXE-1 (L1/L11/L17)"),
    "Engine::LoaderLoop": ("W2", "after EXE-1 (L3/L7/L10/L13/L16) and 84"),
    "Engine::UpdateLoop": ("W2", "after EXE-1 (L5/L8/L14) and 44, 45, 85 (M3)"),
    "Engine::Loader": ("W2", "after SYS-1 (real System::Thread(unsigned)); with storage A45 `_loader`, one commit, COORD hunk ConstructLoader"),
    "Audio::Stream": ("W2", "after SYS-1 (real System::Thread ctor)"),
    # cross-block bases
    "System::Thread": ("W3", "after ENG 84, 128 (its subclasses)"),
    "Object::CompiledExternal": ("W3", "after every CompiledExternal subclass in every block (M4)"),
    "Object::Internal": ("W3", "after every Internal subclass in every block (M4)"),
    "Object::External": ("W3", "after every External subclass in every block (M5)"),
    "Object::Base": ("W3", "LAST row of the step (M-last)"),
}
UNEXPORTED = {  # row -> (owner, class to define, note)
    "0": ("IO", "the IO::Encoding table stamped by ADDRESS (compressedstream.cpp:725, file_stream.cpp:588, "
                "userinterface.cpp:1490 = foreign site)"),
    "1": ("IO", "the PersistenceWorker thread table (persistence_worker_win.cpp:557; singleton_ctors.cpp:444 = foreign site)"),
    "2": ("IO", "the TextFileStream encoding table (file_stream.cpp:634)"),
    "3": ("IO", "the VisitorStream encoding table (iobase_vtables.cpp:527)"),
    "4": ("ENG", "the Audio device voice-callback table (singleton_ctors.cpp:756)"),
}

def main():
    ret = retired()
    A = table(S + "INVENTORY_A_MINT_VTABLES.md", "| # | mangled")
    T4 = table(S + "INVENTORY_B_DOORS_DELDTOR.md", "| # | class | deleting dtor")
    D = table(S + "INVENTORY_D_STORAGE_RESIDUE.md", "| # | row (mangled / address)")

    print("#### 1.2 The minted vtables (INVENTORY_A table A; `#` = its row id)\n")
    print("| # | class | D10 | owner | wave | waits for / note | status |")
    print("|---|---|---|---|---|---|---|")
    vt_owner = {}
    counts = collections.Counter()
    for a in A:
        idx, cls, mang = a["#"], a["class"], a["mangled"].strip("`")
        if cls.startswith("(unexported"):
            own, note = UNEXPORTED[idx]
            wave, note = "W1", "define the class: " + note
        else:
            own = block_of(cls)
            wave, note = VT_WAVE.get(cls, ("W1", ""))
        key = mang if mang.startswith("??_7") else a["DLL VA"]
        st = "RETIRED (" + ret[key] + ")" if key in ret else ""
        if "retire now" in a["verdict"]: note = (note + "; " if note else "") + "retire now (no stamper, no reader)"
        if "retire after door" in a["verdict"]: note = (note + "; " if note else "") + "class concrete already; doors -> real ctor"
        if not st: counts[(own, wave)] += 1
        vt_owner[cls] = (own, wave, idx)
        print(f"| {idx} | {cls} | {a['D10']} | {own} | {wave} | {note} | {st} |")
    print()
    print("Open vtable rows per owner and wave: " + ", ".join(
        f"{o} {w} {n}" for (o, w), n in sorted(counts.items())))

    print("\n#### 1.3 The `RT_DELDTOR` rows (INVENTORY_B table T4; `#` = its row id)\n")
    print("Every row retires IN THE SAME COMMIT as the vtable row whose slot 0 it fills (D4; the generator "
          "refuses the vtable row otherwise), so its owner and wave are that row's.\n")
    print("| # | class | deleting dtor | vt row | owner | wave | status |")
    print("|---|---|---|---|---|---|---|")
    done = 0
    src = open("runtime/src/object/deleting_dtor.cpp", encoding="utf-8").read()
    for t in T4:
        cls = t["class"]
        own, wave, vidx = vt_owner.get(cls, ("?", "?", "?"))
        gone = not re.search(r"RT_DELDTOR\(" + re.escape(t["deleting dtor"]) + r"\b", src)
        st = "RETIRED (pilot)" if gone else ""
        done += gone
        print(f"| {t['#']} | {cls} | {t['deleting dtor']} | {vidx} | {own} | {wave} | {st} |")
    print(f"\n{len(T4)} rows; {done} retired by the pilot; {len(T4) - done} open.")

    print("\n#### 1.4 The minted `BaseTypeInfo` records (generated mint, `kMintTypeInfos`)\n")
    print("All follow D6's three steps: W1 the owner ports `static TypeId()` / `GetTypeInfo()` over the minted "
          "record; W2 the foreign readers (engine_funcs, standalone, code/Game) switch to them; W3 the owner "
          "`DEFINE_TYPE`s the class and retires the row. `Internal` goes with the duplicate (D6).\n")
    inc = open("standalone/rt_data_mint_tables.inc", encoding="utf-8").read()
    tis = re.findall(r'\{ "(\?_typeInfo@[^"]+)"', inc)
    rows = collections.defaultdict(list)
    for s in tis:
        p = s.split("@")
        cls, ns = p[1], p[2]
        if ns == "1" or ns == "": ns = cls
        rows[block_of(ns + "::" + cls)].append(ns + "::" + cls)
    print("| owner | n | classes |")
    print("|---|---:|---|")
    for o in sorted(rows):
        print(f"| {o} | {len(rows[o])} | " + ", ".join(sorted(rows[o])) + " |")
    print(f"\n{len(tis)} records open (the pilot retired none: PickingMesh's record stays until W3).")

    print("\n#### 1.5 The storage cells (INVENTORY_D table A.1; `#` = its row id)\n")
    print("| # | row | class | owner | wave | rule |")
    print("|---|---|---|---|---|---|")
    SPECIAL = {
        "A1": ("COORD", "W0", "no reader: retire"), "A2": ("COORD", "W0", "no reader: retire"),
        "A3": ("COORD", "W0", "no reader: retire"), "A18": ("COORD", "W0", "no reader: retire"),
        "A23": ("COORD", "W0", "no reader: retire"),
        "A19": ("SYS", "STAYS", "System::Debug unported (B34B 4.1); 1 reader"),
        "A24": ("SYS", "STAYS", "MemoryAllocator is carry-over 1 (D10 OUT)"),
        "A14": ("OBJ", "W3", "with the fold (D6); 3 by-address readers are RF's"),
        "A45": ("ENG", "W2", "no redirect: one commit with vt row 84 (real Loader/Thread ctors)"),
        "A46": ("GFX", "W1", "no redirect: one commit, the ONLY seeder (double-seed rule, D7)"),
        "A6": ("GFX", "W1", "1 site: file static + retire in one commit"),
        "A7": ("GFX", "W1", "1 site: file static + retire in one commit"),
        "A47": ("GFX", "W1", "1 site, with A48: file static + retire in one commit"),
        "A48": ("GFX", "W1", "1 site, with A47"),
    }
    for d in D:
        rid = d["#"]
        cls = d["class"].split(" ")[0]
        if rid in SPECIAL:
            own, wave, rule = SPECIAL[rid]
        else:
            own = block_of(cls)
            wave, rule = "W1 / CLOSE", "W1: class static + redirect + own readers; the row retires when its last reader goes (W3 owner, else CLOSE)"
            if cls.startswith("System::Timer"):
                rule = "the FIVE together (one commit), EXTRA_INIT goes (COORD hunk); " + rule
            if cls.startswith("System::Error"):
                rule = "the THREE together; " + rule
        print(f"| {rid} | `{d['row (mangled / address)'].strip('`')}` | {d['class']} | {own} | {wave} | {rule} |")

if __name__ == "__main__":
    main()
