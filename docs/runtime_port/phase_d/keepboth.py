#!/usr/bin/env python3
"""keepboth.py FILE — resolve an APPEND-ONLY merge conflict by keeping both sides.

Removes the three conflict-marker lines (matched as WHOLE lines: a `=======` inside a table
or a comment banner is not a marker) and keeps every other line, ours first. For files where
two blocks each appended rows (docs/known_port_issues.md). Prints the number of marker lines
removed; exits 1 when the file has no marker (nothing to do)."""
import re
import sys

p = sys.argv[1]
b = open(p, 'rb').read()
lines = b.split(b'\n')
out = [ln for ln in lines if not re.match(rb'^(<<<<<<< .*|=======\r?|>>>>>>> .*)$', ln)]
n = len(lines) - len(out)
if n == 0:
    sys.exit(1)
open(p, 'wb').write(b'\n'.join(out))
print(f'{p}: {n} marker line(s) removed, both sides kept')
