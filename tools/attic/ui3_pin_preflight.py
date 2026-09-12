#!/usr/bin/env python3
"""Wave 10 lane UI3 -- PRE-FLIGHT for the shipped-byte displacement pins.

`PinDisp` in runtime/src/ui/dialog_create.cpp finds a byte pattern in a shipped
Runtime body, requires it to occur EXACTLY ONCE, and compares the field that
follows against the constant the port compiles in.  Every one of those patterns
was hand-encoded from a Ghidra listing, and a hand-encoded modrm byte is exactly
the kind of input that produces a confident WRONG finding about a body that is
completely correct (`reference_recheck_must_match_its_own_proof.md`).

So the patterns are checked HERE, against the shipped DLL read straight off
disk, before any run -- because a NOT-UNIQUE or NOT-FOUND pin reports as a
FAILURE of the port, and a lane that ships one of those spends its evening
debugging its own instrument.

This duplicates PinDisp's search deliberately: it is a pre-flight on the INPUTS
(the patterns and the expected values), not a second implementation of the
oracle.  The oracle still runs in-process against the loaded DLL.

Usage:  python tools/ui3_pin_preflight.py [path-to-Runtime-x64.dll]
Exit 0 iff every pin is unique, found, and carries the expected value.
"""
import struct
import sys

DLL = sys.argv[1] if len(sys.argv) > 1 else r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000


def load(path):
    data = open(path, "rb").read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    coff = e_lfanew + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    secs = []
    so = coff + 20 + opt_size
    for i in range(nsec):
        s = so + i * 40
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, s + 8)
        secs.append((vaddr, max(vsize, rsize), raddr))
    return data, secs


def read(data, secs, va, n):
    rva = va - BASE
    for vaddr, vsize, raddr in secs:
        if vaddr <= rva < vaddr + vsize:
            off = raddr + (rva - vaddr)
            return data[off:off + n]
    raise KeyError("va 0x%x not mapped" % va)


H = bytes.fromhex

# (body va, body length, [ (pattern, width, expected, what) ]).
# width 4/1 = read that many bytes after the pattern as a little-endian signed
# field.  width 0 = PRESENCE only (the value lives inside the pattern).
# ("call", target) = the byte after the pattern must be 0xE8 and its rel32 must
# resolve to `target`.
BODIES = [
    (0x1801137C0, 0x77, "?StartTimer@", [
        (H("443951"), 1, 0x70, "DialogInstance::_timers._size"),
        (H("4d8b4b"), 1, 0x68, "DialogInstance::_timers._data"),
        (H("41395c01"), 1, 0x10, "DialogTimer::_id"),
        (H("48894c02"), 1, 0x08, "DialogTimer::_period"),
        (H("4883c0"), 1, 0x18, "DialogTimer stride"),
        (H("895c2430"), "call", 0x180076250, "Array<0x18>::SetSize"),
    ]),
    (0x1801133B0, 0xB0, "?AddController@(tab)", [
        (H("7312488b40"), 1, 0x40, "TypeNode::right"),
        (H("eb04488b40"), 1, 0x38, "TypeNode::left"),
        (H("488b48"), 1, 0x10, "TypeNode::value.info"),
        (H("ff51"), 1, 0x20, "BaseTypeInfo::factoryFn"),
        (H("488957"), 1, 0x10, "DialogController::_config"),
        (H("48896f"), 1, 0x28, "DialogController::_dataSource"),
        (H("488977"), 1, 0x18, "DialogController::_dialog"),
        (H("488947"), 1, 0x20, "DialogController::_container"),
        (H("488d4e"), 1, 0x78, "DialogInstance::_controllers"),
        (H("4c8d442440ffc2"), "call", 0x180076370, "Array<ptr>::SetSize"),
    ]),
    (0x180113470, 0x2B2, "?AddController@(6)", [
        (H("488b79"), 1, 0x50, "DialogInstance::_root"),
        (H("488b4f"), 1, 0x10, "Element::_name"),
        (H("448bb7"), 4, 0xA8, "Element::_children._size"),
        (H("488b9f"), 4, 0xA0, "Element::_children._data"),
        (H("48899f"), 4, 0x98, "Element::_parent"),
        (H("814b"), 1, 0x34, "Element::_flags"),
        (H("814b34"), 4, 0x08000000, "kElementNeedsSizeCalc"),
        (H("ff50"), 1, 0x40, "ElementDescription vtable slot 8"),
        (H("4d8bc7488d542430"), "call", 0x180115650, "Element::FindElement"),
        (H("4c8d442430"), "call", 0x180076900, "Array<ptr>::SetSize"),
    ]),
    (0x180114870, 0x1E3, "??0DialogInstance", [
        (H("48890648895e"), 1, 0x10, "DialogInstance::_ui"),
        (H("8b45"), 1, 0x40, "Dialog::_defaultFlags"),
        (H("895e48488b4d"), 1, 0x38, "Dialog::_rootDescription"),
        (H("488d56"), 1, 0x50, "DialogInstance::_root"),
        (H("48894e"), 1, 0x58, "DialogInstance::_spriteSheet"),
        (H("48c786"), 4, 0x90, "DialogInstance::_timerHandle"),
        (H("4889b0"), 4, 0x90, "Element::_dialogParent"),
        (H("ba18000000448d42f0"), 0, 0, "LinkedList node MemAlloc(0x18, align 8)"),
        (H("837d"), 1, 0x50, "Dialog::_controllerCount"),
        (H("4883c7"), 1, 0x20, "DialogControllerConfigEntry stride"),
        (H("498d41"), 1, 0x18, "config entry +0x18 -> arg6 (String)"),
        (H("498b41"), 1, 0x08, "config entry -> arg5 (ElementDescription)"),
        (H("4d8b49"), 1, 0x10, "config entry -> arg4 (DialogControllerConfig)"),
        (H("488bce"), "call", 0x180113470, "AddController(6)"),
    ]),
]


def find_unique(body, pat):
    hits = [i for i in range(len(body) - len(pat) + 1) if body[i:i + len(pat)] == pat]
    return hits


# THE NEGATIVE CONTROL FOR THIS SCRIPT.  A checker that prints "ok" for
# everything is not a checker, and the question to ask of any instrument is
# *could this field express the answer if the answer were different?*
# (`reference_diagnostic_field_cannot_express_the_answer.md`).  Three
# deliberately broken pins against the SAME shipped bytes; all three must FAIL.
CONTROLS = [
    (0x1801137C0, 0x77, "control: wrong value (off by one)", [
        (H("443951"), 1, 0x71, "_timers._size, deliberately wrong"),
    ]),
    (0x1801137C0, 0x77, "control: absent pattern", [
        (H("deadbeef"), 1, 0, "a pattern that is not in the body"),
    ]),
    (0x1801137C0, 0x77, "control: ambiguous pattern", [
        (H("48"), 1, 0, "a bare REX.W prefix, which occurs many times"),
    ]),
]


def check(data, secs, bodies, quiet=False):
    def _p(*a):
        if not quiet:
            print(*a)
    bad = 0
    total = 0
    for va, length, name, pins in bodies:
        body = read(data, secs, va, length)
        if not quiet:
            print("== %s  0x%x  (%d bytes)" % (name, va, length))
        for pat, width, expected, what in pins:
            total += 1
            hits = find_unique(body, pat)
            if len(hits) != 1:
                bad += 1
                _p("   FAIL  %-52s pattern %s occurs %d times (need exactly 1)"
                      % (what, pat.hex(), len(hits)))
                continue
            at = hits[0] + len(pat)
            if width == "call":
                if body[at] != 0xE8:
                    bad += 1
                    _p("   FAIL  %-52s byte after pattern is 0x%02x, not E8"
                          % (what, body[at]))
                    continue
                rel = struct.unpack_from("<i", body, at + 1)[0]
                tgt = va + at + 5 + rel
                ok = tgt == expected
                bad += 0 if ok else 1
                _p("   %s  %-52s -> 0x%x (expected 0x%x)"
                      % ("ok  " if ok else "FAIL", what, tgt, expected))
                continue
            if width == 0:
                _p("   ok    %-52s present exactly once at body+0x%x" % (what, hits[0]))
                continue
            raw = body[at:at + width]
            got = int.from_bytes(raw, "little", signed=True)
            ok = got == expected
            bad += 0 if ok else 1
            _p("   %s  %-52s = 0x%x (expected 0x%x)"
                  % ("ok  " if ok else "FAIL", what, got, expected))
    if not quiet:
        print("\n%d pins, %d FAILED" % (total, bad))
    return total, bad


def main():
    data, secs = load(DLL)
    total, bad = check(data, secs, BODIES)

    # The control runs LAST but must be read FIRST: if the checker cannot reject
    # a pin it should reject, the block above proves nothing.
    print("\n-- NEGATIVE CONTROL (read this before the %d lines above) --" % total)
    caught = 0
    for entry in CONTROLS:
        _t, b = check(data, secs, [entry], quiet=True)
        ok = b == 1
        caught += 1 if ok else 0
        print("   %s  %s" % ("REJECTED " if ok else "*** ACCEPTED ***", entry[2]))
    if caught == len(CONTROLS):
        print("   the checker DISCRIMINATES (%d/%d rejected) -- the pins above mean "
              "something" % (caught, len(CONTROLS)))
    else:
        print("   *** THE CHECKER FAILED TO REJECT %d/%d -- IGNORE THE PINS ABOVE ***"
              % (len(CONTROLS) - caught, len(CONTROLS)))
        return 2
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
