"""HANDED-IN positive control for the EXPORT screen, in-population.

control_ported.py failed to produce a positive case: 0 of 141 PORTED
address-keyed rows have an export name either.  That is consistent (the address
door is used BECAUSE there is no export) but it leaves the "0 of 42" answer
resting only on a generic control.  So hand the screen an address that is a
census row AND known to be exported: take rows the census keys by NAME, look
each name's address up in the export table, and push that address through the
SAME classify() path an internal row takes.  If the screen is live, every one
must come back with its own name.
"""
import os
import csv
import sys
sys.path.insert(0, 'tools')
from internal_row_classify import Image, RETAIL, classify

img = Image(RETAIL, 'retail')
name2rva = {}
for rva, names in img.exports.items():
    for n in names:
        if n:
            name2rva[n] = rva

handed, hit, miss = 0, 0, []
for row in csv.DictReader(open(os.environ.get('W23_CSV','w23.csv'))):
    if row['kind'] != 'export':
        continue
    rva = name2rva.get(row['key'])
    if rva is None:
        continue
    handed += 1
    c = classify(img, rva + img.base)
    if row['key'] in c['export_exact'].split(';'):
        hit += 1
    else:
        miss.append((row['key'], c['export_exact']))

print('HANDED-IN control: %d census EXPORT rows pushed through the ADDRESS path' % handed)
print('  screen recovered the correct name: %d  (%.1f%%)' % (hit, 100.0 * hit / max(1, handed)))
print('  misses: %d' % len(miss))
for m in miss[:5]:
    print('    ', m)
print()
print('NEGATIVE control (must NOT fire): the same addresses shifted by +3 bytes')
false_hits = 0
n = 0
for name, rva in list(name2rva.items())[:2000]:
    n += 1
    if classify(img, rva + img.base + 3)['export_exact']:
        false_hits += 1
print('  %d of %d shifted addresses produced an export name: %d expected 0' %
      (false_hits, n, false_hits))
