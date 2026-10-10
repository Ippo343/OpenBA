"""B11 over a whole tree after a move: copy the base objects under their NEW flattened names,
then run b11cmp.py over every object.

    python tools/restructure/b11_mapped.py <base_obj_dir> <tip_obj_dir> <rules.txt> [--detail]

rules.txt: "old_prefix new_prefix" per line (directory prefixes, as rewrite_paths.py takes);
object names flatten "/" to "__", so the same rules are applied to the names with "/" -> "__".
"""
import os
import shutil
import subprocess
import sys
import tempfile

base, tip, rules_path = sys.argv[1:4]
extra = sys.argv[4:]
rules = []
with open(rules_path, encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if line and not line.startswith("#"):
            a, b = line.split()
            rules.append((a.replace("/", "__"), b.replace("/", "__")))
rules.sort(key=lambda r: len(r[0]), reverse=True)

tmp = tempfile.mkdtemp(prefix="b11base_mapped_")
n = 0
for name in os.listdir(base):
    if not name.endswith(".o"):
        continue
    new = name
    for a, b in rules:
        if new.startswith(a):
            new = b + new[len(a):]
            break
    shutil.copyfile(os.path.join(base, name), os.path.join(tmp, new))
    n += 1
tip_names = {x for x in os.listdir(tip) if x.endswith(".o")}
mapped = {x for x in os.listdir(tmp)}
print(f"base objects {n}; tip objects {len(tip_names)}; only-base {sorted(mapped - tip_names)[:10]}; "
      f"only-tip {sorted(tip_names - mapped)[:10]}")
cmp_py = os.path.join("docs", "runtime_port", "endgame_b", "b4_session", "b11cmp.py")
rc = subprocess.call([sys.executable, cmp_py, tmp, tip, *extra])
shutil.rmtree(tmp, ignore_errors=True)
sys.exit(rc)
