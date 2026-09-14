import re, os, collections

ADDRS = """0x140003110 0x1400032e0 0x1400021d0 0x14000a590 0x140001bb0 0x14001c890
0x14000f090 0x14000d030 0x14000da70 0x1400335b0 0x14001ee40 0x14001fa80
0x14001f8d0 0x14001f250 0x14001e210 0x14001d830 0x14001d940 0x140023a00
0x140037ad0 0x14000e8d0 0x14000e760 0x14000cf40 0x14000ca10 0x140009f80
0x140002540 0x140001770 0x140044810 0x14000f9e0 0x14001bf10 0x14001c7c0""".split()
A = set(a.lower() for a in ADDRS)

mydirs = ['code/Game/Component', 'code/Game/Action', 'code/Game/Work', 'code/Game/Entity']
pat = re.compile(r'ResolveAddr\(Module::Exe,\s*(0x14[0-9a-fA-F]+)\)')
res = collections.defaultdict(list)
for d in mydirs:
    for root, _, fs in os.walk(d):
        for f in sorted(fs):
            if not (f.endswith('.cpp') or f.endswith('.h')):
                continue
            p = (root + '/' + f).replace(os.sep, '/')
            t = open(p, encoding='utf-8', errors='replace').read()
            hits = sorted(set(a.lower() for a in pat.findall(t)) & A)
            if hits:
                res[d].append((p, hits))
for d in mydirs:
    print('=====', d, len(res[d]), 'files')
    for p, h in res[d]:
        print(p, ' '.join(h))
