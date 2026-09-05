#!/usr/bin/env python3
"""Report every exe PE-import slot our port reads, with the symbol the exe's own
import table puts there, and which files read it.

WHY THIS IS A REPORT AND NOT A CHECKER
--------------------------------------
Some sites in the port read an import slot under the WRONG symbol name. Known
examples found by hand in the 2026-09-04 wave:

  engine_funcs.cpp  0x1401527f8 -> read as DialogInstance::GetRoot
                                   PE says ParticleInstance::SetParticle
                                   (real GetRoot is 0x1401517a8)
  engine_funcs.cpp  0x140152830 -> read as Audio::Scene::PlayEffect
                                   PE says Achievement::Goal::Goal()
  engine_funcs.cpp  0x140151a60 -> AddController fallback
                                   PE says Element::AddChild
  address_registry  0x140152400 <-> 0x140152618 registered swapped
  address_registry  0x1401518f0 -> wrong GetAlignedPosition overload

Such a site works ONLY while the slot stays unregistered, because it falls
through to a ResolveExport-by-name fallback. Register the slot and the consumer
is silently rebound to a different function.

Detecting these automatically was ATTEMPTED AND ABANDONED: our target names are
hand-abbreviated (`iatIBPSer` for InternalBasePtr::Serialize, `s_fn_Vec3Ctor`
for ??0Vector3) and the intent often sits in a comment several lines from the
ResolveAddr call. Every matching heuristic tried (context words, assignment
target, prefix matching, ctor/dtor decoding) produced a flood of false positives
AND still missed all five known cases. Do not trust a scoring heuristic here.

What IS exact, and is checked by `dispatch_slot_map.py --check`, is the
`RegisterDispatchSlot(addr, ResolveExport("name"))` form, where the name is
literally in the source next to the address. That check found the swap above.

This script just lays the ground truth next to the call sites so a human or an
agent can review a bounded list.
"""
import re, sys, glob, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dispatch_slot_map import slotmap

SITE = re.compile(r'ResolveAddr\(\s*Module::Exe\s*,\s*(0x1401[0-9a-fA-F]{5})\s*\)')

def collect(paths):
    readers = collections.defaultdict(set)
    for path in paths:
        try:
            text = open(path, encoding='utf8', errors='replace').read()
        except OSError:
            continue
        for m in SITE.finditer(text):
            readers[int(m.group(1), 16)].add(path.replace(os.sep, '/'))
    return readers

if __name__ == '__main__':
    paths = (glob.glob('code/Game/*/*.cpp') + glob.glob('code/Game/*/*.h') +
             glob.glob('engine/funcs/*.cpp') + glob.glob('engine/funcs/*.h') +
             glob.glob('standalone/*.cpp') +
             glob.glob('runtime/**/*.cpp', recursive=True))
    readers = collect(paths)
    imports = {a: r for a, r in readers.items() if a in slotmap}
    print(f"{len(readers)} distinct exe addresses read via ResolveAddr(Module::Exe);"
          f" {len(imports)} of them are PE import slots\n")
    for a in sorted(imports):
        dll, nm = slotmap[a]
        print(f"0x{a:09x}  {nm}")
        for f in sorted(imports[a]):
            print(f"              {f}")
        print()
