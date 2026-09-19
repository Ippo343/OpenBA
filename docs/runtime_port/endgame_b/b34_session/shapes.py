"""Classify the guard SHAPES the guard-bearing globals appear in."""
import os, re, collections
ns = {}
exec(open('sweep.py').read().split('def main()')[0], ns)
strip = ns['strip']; files = ns['files']; ROOT = ns['ROOT']
G = ["g_fn_Serializer_Ctor","g_fn_Serializer_Dtor","g_fn_Serialize_Int","g_fn_Serialize_Bool",
     "g_fn_Serializer_GetVersion","fn_Serializer_Serialize_Float","fn_Serializer_Serialize_Uint",
     "fn_Serializer_GetEncoding","fn_Serializer_SerializeBulk","fn_InternalBasePtr_Serialize",
     "fn_Ser_Slot_SerByte_410","fn_Ser_Slot_SerSizedRaw_3F8","fn_Ser_Slot_SerSizedRaw_400",
     "fn_Persistence_GetSavePath","fn_Persistence_WriteSettings","fn_Persistence_ReadSettings",
     "fn_FileSystem_GetDataDirectory","fn_Modification_GetCount","fn_Modification_GetMod"]
PAT = re.compile(r"\b(" + "|".join(G) + r")\b")
cls = collections.Counter(); ex = collections.defaultdict(list)
for path in files():
    try: src = open(path, encoding="utf-8", errors="replace").read()
    except OSError: continue
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    if rel.startswith("engine/funcs/"): continue
    code = strip(src); raw = src.split('\n')
    for i, ln in enumerate(code.split('\n')):
        if not PAT.search(ln): continue
        t = ln.strip()
        if re.match(r"^if\s*\(\s*!", t) and 'return' in t:            k = "early-return  if (!p) return;"
        elif re.match(r"^if\s*\(\s*!", t):                            k = "negated chain  if (!p || !q ...)"
        elif re.search(r"\?", t) and ':' in t:                        k = "ternary with FALLBACK VALUE"
        elif re.match(r"^\}?\s*else\s+if\s*\(", t):                   k = "else-if guard"
        elif re.match(r"^(bool|static bool|int)?\s*\w*\s*(ok|s_ok|resolved)\w*\s*=", t): k = "readiness chain  ok = a && b && ..."
        elif re.match(r"^if\s*\(.*&&", t):                            k = "if (p && other)"
        elif re.match(r"^if\s*\(", t):                                k = "plain  if (p) ..."
        elif '&&' in t:                                               k = "conjunct in a multi-line chain"
        else:                                                         k = "OTHER - read by hand"
        cls[k] += 1
        if len(ex[k]) < 6: ex[k].append(f"{rel}:{i+1}  {raw[i].strip()[:120]}")
for k, c in cls.most_common():
    print(f"{c:5d}  {k}")
    for e in ex[k]: print(f"          {e}")
