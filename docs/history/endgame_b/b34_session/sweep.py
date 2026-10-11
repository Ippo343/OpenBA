#!/usr/bin/env python3
"""B.3.4a closing residual sweep.

Comment/string-aware. Two passes:
  A) global door names -> CALL sites tree-wide
  B) per-file locals bound POSITIONALLY to a Serializer dispatch slot (or a mangled
     Serializer export, or an alias of a global door) -> CALL sites in that file.

Usage: sweep.py [--strings-intact]
"""
import os, re, sys, json

ROOT = r"C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-ac6ce140a639573ac"
DIRS = ["code", "engine", "standalone", "runtime", "dx11"]
SKIP_PARTS = (os.sep + "build" + os.sep, os.sep + "build-", "standalone" + os.sep + "build")

SLOTS = ["140152ad8","140152ad0","140152a48","140152ac8","140152a58","140152a60",
         "140152a50","140152468","140152418","140152410","140152488","1401523f8",
         "140152490","140152618","140152478","140152400","140151278","140151458",
         "140151ea8","140152aa0"]
SLOT_RE = re.compile(r"0x(" + "|".join(SLOTS) + r")\b", re.I)

GLOBAL_DOORS = [
    "g_fn_Serializer_Ctor","g_fn_Serializer_Dtor","g_fn_Serialize_Int","g_fn_Serialize_Bool",
    "g_fn_Serializer_GetVersion","fn_Serializer_Ctor","fn_Serializer_Dtor","fn_Serialize_Int",
    "fn_Serialize_Bool","fn_Serializer_GetVersion","fn_Serializer_Serialize_Float",
    "fn_Serializer_Serialize_Uint","fn_Serializer_GetEncoding","fn_Serializer_SerializeBulk",
    "fn_Ser_Slot_SerInt_AC8","fn_Ser_Slot_SerFloat_A60","fn_Ser_Slot_SerByte_410",
    "fn_Ser_Slot_SerUShortArr_488","fn_Ser_Slot_SerSizedRaw_3F8","fn_Ser_Slot_SerSizedRaw_400",
    "fn_Ser_Slot_SerByteArr_618","SerializerEncodingVCall",
    "fn_InternalBasePtr_Serialize",
    "fn_Modification_Find","fn_Modification_GetCount","fn_Modification_GetMod",
    "fn_Modification_Swap","fn_Modification_Enable","fn_Modification_RefreshList",
    "fn_Modification_SaveList","fn_ModEntry_CopyCtor","fn_ModEntry_Assign","fn_ModEntry_Dtor",
    "fn_ModArrayCopy",
    "fn_Persistence_SetBool","fn_Persistence_SetInt","fn_Persistence_GetSavePath",
    "fn_Persistence_WriteSettings","fn_Persistence_ReadSettings",
    "fn_FileSystem_GetDataDirectory","fn_FileSystem_GetSourceDirectory",
    "fn_Struct60B_Serialize","fn_TradeWork_impl_04d0","fn_TradeWork_impl_f1a0","fn_MemZeroFill",
]

def strip(src):
    """Remove comments and string/char literal CONTENT, keeping line count."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i+1] == '/':
            j = src.find('\n', i)
            if j < 0: j = n
            out.append(' ' * (j - i)); i = j
        elif c == '/' and i + 1 < n and src[i+1] == '*':
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in src[i:j])); i = j
        elif c == '"' or c == "'":
            q = c; j = i + 1
            while j < n and src[j] != q:
                if src[j] == '\\': j += 2
                elif src[j] == '\n': break
                else: j += 1
            j = min(j + 1, n)
            out.append(''.join(ch if ch == '\n' else ' ' for ch in src[i:j])); i = j
        else:
            out.append(c); i += 1
    return ''.join(out)

def files():
    for d in DIRS:
        for dp, dn, fn in os.walk(os.path.join(ROOT, d)):
            dn[:] = [x for x in dn if x != "build" and not x.startswith("build-")]
            for f in fn:
                if f.endswith((".cpp", ".h", ".hpp", ".cc")):
                    yield os.path.join(dp, f)

CALL_CTX = re.compile(r"(?:\w|\)|\]|>)\s*$")

def call_sites(code, name):
    """Occurrences of `name` followed by '(' that are CALLS, not decls/typedefs/&name."""
    hits = []
    for m in re.finditer(r"\b" + re.escape(name) + r"\s*\(", code):
        s = m.start()
        # preceding non-space char
        k = s - 1
        while k >= 0 and code[k] in " \t": k -= 1
        prev = code[k] if k >= 0 else ''
        if prev in ('&', '*'):   # &name(...) not a thing; *name( could be a cast-call
            pass
        # a declaration: "type name(" -- preceded by an identifier on same line
        line_start = code.rfind('\n', 0, s) + 1
        before = code[line_start:s]
        if re.search(r"\b(typedef|using)\b", before): continue
        if re.search(r"[A-Za-z_]\w*\s*\*?\s*$", before) and not re.search(r"(return|if|while|for|else|,|\(|=|&&|\|\||!|;|:)\s*$", before.strip() + ' '):
            # could be a declaration like `fn_X_t fn_X(` -- skip only if it looks like a decl
            tok = re.findall(r"[A-Za-z_]\w*", before)
            if tok and (tok[-1].endswith("_t") or tok[-1] in ("extern","static","void","int","bool","float")):
                continue
        hits.append(code[:s].count('\n') + 1)
    return hits

def main():
    gcounts = {n: [] for n in GLOBAL_DOORS}
    local_hits = []
    for path in files():
        try:
            src = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        code = strip(src)
        for n in GLOBAL_DOORS:
            if n in code:
                for ln in call_sites(code, n):
                    gcounts[n].append(f"{rel}:{ln}")
        # --- pass B: locals ---
        if not (SLOT_RE.search(code) or re.search(r"@(Serializer|FileSystem|Modification|Persistence|PackageFile|Stream|ReaderWriter|Encoding)@IO@@", src)):
            continue
        # --- statement-based binding pass ---
        # rebuild statements with their start line numbers
        stmts = []
        cur, start = [], 1
        line = 1
        for ch in code:
            if ch == '\n':
                line += 1
            if ch in ';{}':
                s = ''.join(cur).strip()
                if s: stmts.append((start, s))
                cur, start = [], line
            else:
                cur.append(ch)
        srcstmts_raw = src  # for mangled-name lookup we use the raw text per statement span
        bound = {}
        # mangled Serializer/PackageFile/etc export lines, strings intact
        mangled_lines = set()
        for i, rl in enumerate(src.split('\n'), 1):
            if re.search(r"(ResolveExport|ResolveFn|ResolveExref)", rl) and re.search(r"[?@][A-Za-z_0-9]+@(Serializer|FileSystem|Modification|Persistence|PackageFile|ReaderWriter|Encoding)@IO@@", rl):
                mangled_lines.add(i)
        # Ordered list of (line, name, kind, detail) assignment events.
        # kind: 'ser' (a serializer door), 'other' (anything else -- KILLS a binding),
        #       'via:<name>' (from another local/global)
        ANY_ADDR = re.compile(r"0x1[0-9a-f]{8,}", re.I)
        events = []
        for (ln0, st) in stmts:
            nlines = st.count('\n')
            lhs_list = re.findall(r"\b([A-Za-z_]\w*)\s*=(?![=])", st)
            if not lhs_list: continue
            hit_slot = SLOT_RE.search(st)
            hit_mangled = any(l in mangled_lines for l in range(ln0, ln0 + nlines + 2))
            other_addr = ANY_ADDR.search(st) and not hit_slot
            toks = [t for t in dict.fromkeys(re.findall(r"[A-Za-z_]\w*", st)) if t not in lhs_list]
            for lhs in lhs_list:
                if hit_slot:            k, d = 'ser', hit_slot.group(0)
                elif hit_mangled:       k, d = 'ser', 'mangled Serializer export'
                elif other_addr:        k, d = 'other', ANY_ADDR.search(st).group(0)
                elif toks:              k, d = 'via', toks
                else:                   k, d = 'other', 'unrecognised'
                events.append((ln0, lhs, k, d))

        by_name = {}
        for e in events: by_name.setdefault(e[1], []).append(e)

        def resolve(name, before_line, depth=0, seen=None):
            """kind of `name` as of before_line: nearest PRECEDING assignment wins."""
            if depth > 5: return None
            seen = seen or set()
            if name in seen: return None
            seen = seen | {name}
            evs = by_name.get(name)
            if not evs: return None
            prior = [e for e in evs if e[0] <= before_line]
            ev = prior[-1] if prior else evs[0]   # file-static assigned in a later Ensure()
            if ev[2] == 'ser': return ev
            if ev[2] == 'other': return None
            for t in ev[3]:
                if t in GLOBAL_DOORS: return ev
                if resolve(t, ev[0], depth + 1, seen): return ev
            return None

        for (ln0, nm, k, d) in events:
            pass
        names = {e[1] for e in events}
        for nm in names:
            if nm in GLOBAL_DOORS: continue
            for cl in call_sites(code, nm):
                ev = resolve(nm, cl)
                if ev:
                    bound[nm] = f"{rel}:{ev[0]} {ev[2]} {ev[3]}"
                    local_hits.append((rel, cl, nm, bound[nm]))
        bound = {}
        for name, why in bound.items():
            if name in GLOBAL_DOORS: continue
            for l in call_sites(code, name):
                local_hits.append((rel, l, name, why))

    print("=== A. GLOBAL DOOR CALLS ===")
    for n in GLOBAL_DOORS:
        if gcounts[n]:
            print(f"{len(gcounts[n]):4d}  {n}")
            for s in gcounts[n]:
                print(f"        {s}")
    print("=== B. LOCAL/BOUND CALLS ===")
    for rel, l, name, why in sorted(local_hits):
        print(f"  {rel}:{l}  {name}   [bound {why}]")
    print(f"total local call sites: {len(local_hits)}")

main()
