import re, os, collections

reg = {}
for l in open('scratchpad/coll-component/reg.txt'):
    p = l.split()
    if len(p) >= 2:
        reg[p[0]] = p[1]

mydirs = ['code/Game/Component', 'code/Game/Action', 'code/Game/Work', 'code/Game/Entity']
files = []
for d in mydirs:
    for root, _, fs in os.walk(d):
        for f in fs:
            if f.endswith('.cpp') or f.endswith('.h'):
                files.append((root + '/' + f).replace(os.sep, '/'))

pat = re.compile(r'ResolveAddr\(Module::Exe,\s*(0x14[0-9a-fA-F]+)\)')
addr2files = collections.defaultdict(set)
for f in files:
    t = open(f, encoding='utf-8', errors='replace').read()
    for a in pat.findall(t):
        addr2files[a.lower()].add(f)

arrpat = re.compile(r'array|resize|grow|push|erase|dtor|clear|arr|free', re.I)
rows = []
for a, fs in addr2files.items():
    n = reg.get(a, '?')
    if arrpat.search(n):
        rows.append((len(fs), a, n, sorted(fs)))
rows.sort(reverse=True)
tot = 0
for c, a, n, fs in rows:
    tot += c
    print('%s %s  files=%d' % (a, n, c))
    for f in fs:
        print('      ', f)
print('total (addr,file) pairs:', tot, 'distinct addrs:', len(rows))
