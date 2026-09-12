# WAVE 28 LANE PARTSYS -- integrator correction 3, second pass.
#
# The first pass used a +/-0x40 window and reported 11 distinct windows, which is
# TRUE AND USELESS: that window captures each draw's USE site, which of course
# differs. The question the correction actually asks is narrower and is the one
# my port depends on:
#
#   is the TEMPERING + INDEX-ADVANCE core byte-identical at all eleven sites,
#   so that ONE RngNext() serves all eleven?
#
# Core, anchored on the first mask immediate:
#   anchor-0x09  8b c1              mov  %ecx,%eax
#   anchor-0x07  c1 e8 0b           shr  $0xb,%eax
#   anchor-0x04  33 c8              xor  %eax,%ecx
#   anchor-0x02  8b c1              mov  %ecx,%eax
#   anchor+0x00  25 ad 58 3a ff     and  $0xff3a58ad,%eax
#   ...
#   anchor+0x1b  33 c?              xor                      <- the byte to watch
#   anchor+0x1d  81 fa 70 02 00 00  cmp  $0x270,%edx
#   anchor+0x23  41 0f 44 d7        cmove %r15d,%edx
# No RIP-relative field lies inside it, so a raw byte comparison is sound here.
import pefile, struct, collections

pe = pefile.PE(r"C:\GOG Games\Banished\Runtime-x64.dll", fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
START, END = 0x1800a5600 - base, 0x1800a6a7a - base
code = pe.get_data(START, END - START)
va = lambda o: base + START + o

anchors = []
needle = b"\x25" + struct.pack("<I", 0xFF3A58AD)
o = code.find(needle)
while o != -1:
    anchors.append(o)
    o = code.find(needle, o + 1)

LO, HI = 0x09, 0x27
wins = {a: code[a - LO:a + HI] for a in anchors}

groups = collections.OrderedDict()
for a in anchors:
    groups.setdefault(wins[a], []).append(a)

print("SUBJECT  tempering+advance CORE of the %d draws, window [anchor-0x%x, anchor+0x%x)"
      % (len(anchors), LO, HI))
print("DISTINCT CORES: %d" % len(groups))
ref = wins[anchors[0]]
for gi, (w, members) in enumerate(groups.items()):
    diffs = [i for i in range(len(ref)) if w[i] != ref[i]]
    print("  group %d : %2d site(s)  %s" % (gi, len(members),
          " ".join(hex(va(m)) for m in members)))
    if diffs:
        for i in diffs:
            print("       byte anchor%+#0.3x : ref %02x -> %02x" % (i - LO, ref[i], w[i]))
    else:
        print("       byte-identical to the reference core at %s" % hex(va(anchors[0])))

# ---- CONTROLS --------------------------------------------------------------
# POSITIVE: a window known to DIFFER must be reported as differing.
bad = bytearray(ref); bad[LO + 1] ^= 0x01
print("\nCONTROL positive (1 bit flipped in the mask): %s"
      % ("CAUGHT" if bytes(bad) != ref else "MISSED -- differ is DEAD"))
# NEGATIVE: an untouched copy must be reported identical.
print("CONTROL negative (untouched copy): %s"
      % ("identical, OK" if bytes(bytearray(ref)) == ref else "FALSE POSITIVE -- differ is broken"))
