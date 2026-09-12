"""Refine the 141: 'in source but not in the census' is NOT 'missing'.

If an address is an EXPORT, the census legitimately carries it under its NAME
and needs no addr row.  Split the 141 on that, using the EXPORT screen that was
proved live (1125/1125, 0/2000 false positives) in control_handed.py.
"""
import csv
import os
import re
import sys
sys.path.insert(0, 'tools')
from internal_row_classify import Image, RETAIL, classify

DOOR = re.compile(r'(?:ResolveAddr\s*\(\s*Module::Runtime\s*,|ResolveRuntimeRaw\s*\(|'
                  r'RT_ADDR\s*\(|RtResolveAddr\s*\(\s*Module::Runtime\s*,)\s*(0x1[0-9a-fA-F]{8})')
found = {}
for root in ('runtime', 'code', 'engine', 'standalone', 'dx11'):
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith(('.cpp', '.h', '.c')):
                continue
            p = os.path.join(dp, fn)
            txt = open(p, encoding='utf-8', errors='replace').read()
            txt = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
            txt = re.sub(r'//[^\n]*', '', txt)
            for m in DOOR.finditer(txt):
                found.setdefault(int(m.group(1), 16), set()).add(p)

csv_addrs, csv_names = set(), set()
for row in csv.DictReader(open(os.environ.get('W23_CSV','w23.csv'))):
    if row['kind'] == 'addr':
        csv_addrs.add(int(row['key'], 16))
    elif row['kind'] == 'export':
        csv_names.add(row['key'])

img = Image(RETAIL, 'retail')
missing = sorted(set(found) - csv_addrs)
exported, exported_with_row, truly_absent = [], [], []
for a in missing:
    c = classify(img, a)
    if c['export_exact']:
        exported.append(a)
        if any(n in csv_names for n in c['export_exact'].split(';')):
            exported_with_row.append(a)
    else:
        truly_absent.append(a)

print('literal Runtime address doors in source: %d' % len(found))
print('  of which the census has an addr row for: %d' % len(set(found) & csv_addrs))
print('  of which NO addr row:                    %d' % len(missing))
print('      ... but the address IS an export:            %d' % len(exported))
print('          and the census carries that export row:  %d  (correctly covered)'
      % len(exported_with_row))
print('          export with NO census row at all:        %d  (INVISIBLE)'
      % (len(exported) - len(exported_with_row)))
print('      ... unexported AND no row anywhere:          %d  (INVISIBLE)'
      % len(truly_absent))
print()
print('INVISIBLE unexported doors (would be "internal" rows if the census saw them):')
for a in truly_absent[:40]:
    c = classify(img, a)
    print('   0x%x  %-22s %s' % (a, c['verdict'], ';'.join(sorted(found[a]))[:78]))
print('   ... %d total' % len(truly_absent))
