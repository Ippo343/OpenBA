"""STATIC re-derivation of the cutover board's open set.

SUBJECT: the 29 rows of kDoors in runtime/src/io/file_stream_cutover_probe.cpp.
CHANNEL: a literal RegisterRuntimeExport/RegisterRuntimeAddr scan of the
         compiled source dirs, comments stripped -- the SAME scan
         mint_stamper_census.py uses.
WHAT IT IS NOT: the live board.  A registration this scan sees may sit behind a
         runtime condition (the allocator's DLL-absent arm is the known case),
         so this is an UPPER bound on shut rows -- the optimistic direction.
"""
import os, re, sys
sys.path.insert(0, os.path.join(os.getcwd(), "tools"))
import mint_stamper_census as C
import screenshot_door_walk as W

root = os.getcwd()
probe = os.path.join(root, W.PROBE)
src = W.strip_comments(open(probe, encoding="utf-8", errors="replace").read())
s = src.find("const Door kDoors[]")
e = src.find("};", s)
body = src[s:e]
rows = re.findall(r'\{\s*(?:"([^"]+)"|nullptr)\s*,\s*(0x[0-9a-fA-F]+)?\s*,\s*"([^"]*)"'
                  r'\s*,\s*(true|false)', body, re.S)
names, addrs, nf, nb = C.load_sources()
print("SUBJECT   %s, %d rows parsed" % (W.PROBE, len(rows)))
print("CHANNEL   literal registration scan, %d files / %d bytes, %d names / %d addrs"
      % (nf, nb, len(names), len(addrs)))
print("POS CTL   ??0BinaryFileStream@IO@@QEAA@XZ registered: %s"
      % ("YES" if "??0BinaryFileStream@IO@@QEAA@XZ" in names else "NO -- DEAD"))
print("NEG CTL   synthetic registered: %s"
      % ("no (ok)" if C.NEG_CONTROL not in names else "YES -- DEAD"))
openrows = 0
for nm, ad, note, conly in rows:
    a = int(ad, 16) if ad else 0
    byname = (nm in names) if nm else None
    byaddr = (a in addrs) if a else None
    ro = (nm and not byname) or (a and not byaddr)
    if ro:
        openrows += 1
    print("  %-4s name=%-4s addr=%-4s %-11s %-26s %s"
          % ("OPEN" if ro else "shut",
             "-" if not nm else ("yes" if byname else "NO"),
             "-" if not a else ("yes" if byaddr else "NO"),
             ad or "", note, nm or "(unexported)"))
print("==== STATIC: %d of %d rows shut, %d OPEN ====" % (len(rows)-openrows, len(rows), openrows))
