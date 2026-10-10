# Wave 9 — lane ENC-2: the `SaveSettings` framing family

**Branch** `wave9/lane-enc2` · **base** `be75284` · build clean under `-Wall -Wextra`.

Closes the `?OnClosed@DialogInstance@UI@@` row that lanes Z, UIC and UID each declined.

## 1. What landed — six bodies

| # | Body | Addr | Doors |
|---|---|---|---|
| 1 | `?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z` (slot `+0x98`) | `0x1800a8fb0` | name + addr |
| 2 | `?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z` (slot `+0xa0`) | `0x1800a8fd0` | name + addr |
| 3 | `??1BinaryWriter@IO@@UEAA@XZ` | `0x1800a8980` | name + addr |
| 4 | BinaryWriter scalar deleting dtor (slot `0`, **unexported**) | `0x180023b50` | addr only |
| 5 | `?SaveSettings@DialogInstance@UI@@QEAAXXZ` | `0x180114240` | name + addr |
| 6 | `?OnClosed@DialogInstance@UI@@UEAAXXZ` | `0x180115330` | name + addr |

Files: `runtime/src/io/binary_writer.cpp` (+ probe), `runtime/src/ui/dialog_settings.cpp`
(+ probe). Kill switches `BANISHED_RT_BINWRITER=0`, `BANISHED_RT_DLGSETTINGS=0`.

## 2. The framing scheme — and why the ctor could not be checked alone

`??1BinaryWriter` is the mirror of `??0BinaryWriter` (lane ENC). Together:

```
offset:   0             4                  8           12
        [ 0xAAAAAAAA ][ -1 placeholder ][ version ][ ... fields ... ][ 0xBBBBBBBB ]
                        ^ _word20 = 4      ^ _word24 = 8
```

The destructor computes `len = pos − _word24`, writes the trailer, seeks back to
`_word20`, back-patches `len`, and seeks forward again.

**`_word24` is captured BEFORE the version is written**, so the back-patched
length is `4 + 4*fields`, **not** `4*fields`. Calling `_word24` "payload start"
is off by exactly one dword. The probe asserts the arithmetic that is actually
there.

This is the strongest available check on the constructor merged in lane ENC: if
the two halves disagreed about which field is which, the round-trip fails. It
does not — `framing=4` cases, all holding.

## 3. Three corrections to the record

1. **`dialog.cpp`'s DECLINED note said this "becomes a one-function job the
   moment `?SaveSettings@` lands". It does not** — `SaveSettings` builds a
   BinaryWriter and dispatches into it, needing four more bodies that were
   themselves minted traps. Six functions, not one. **Corrected at source**, not
   just here.
2. **`DialogInstance +0xa8/+0xb0` is a settings BUFFER and its BYTE LENGTH**, not
   opaque user data. `SaveSettings` sets `ms.data = _userData`,
   `ms.size = _userFlags`; `?LoadSettings@` (`0x180114450`) is the exact mirror
   with `direction = 0` and reads the `0xDEADBEEF` head magic back. Header names
   left alone (renaming would ripple); the role is now documented in both places.
   Treating `_userFlags` as a bitfield would corrupt the buffer. This also
   re-confirms lane ENC's direction mapping from a second, unrelated call site.
3. **Two DialogController vtable slots are now named by the EXPORT TABLE**, not by
   a caller — `dialog.cpp` had flagged `+0x30` as "pinned by ONE unported caller,
   not yet evidence of a name". `??_7DialogController@UI@@6B@` (`0x1801565f0`) read
   byte-wise: `+0x30` = `?OnClosed@DialogController@` (`0x1800cd290`), `+0x50` =
   `?OnSerializeSettings@DialogController@` (`0x1800cd250`). (`+0x10` is `?Type@`.)

## 4. Two loops over one container that disagree

`SaveSettings` **snapshots** `_controllers._size` into a stack slot and compares
against the copy every iteration, but **re-reads** `_controllers._data` from the
object twice per iteration. `OnClosed`, three instructions later in the binary,
**re-reads the count** every iteration. Different decisions about the same
container, and precisely the kind of thing a "cleaned up" port unifies silently.
Both transcribed as-is — and see the limit in §7.

## 5. Not new cutover surface — the arithmetic that made this six bodies

`SaveSettings` dispatches `self->vtable[+0x48]` and the controllers' `+0x10`/`+0x50`;
`OnClosed` dispatches `+0x30`. **None is surface this port adds** — those vtables
are minted regardless of who calls them. What porting `SaveSettings` *would* have
added, and what bodies 1–4 removed first, is the BinaryWriter tail, reached only
because `SaveSettings` builds a writer. Distinguishing the two is what kept this
row from being either a two-body underestimate or an open-ended refusal.

## 6. Measured — all three probes, from that run's own log

```
[Encoding]    cases=8 readers=4 writers=4 emitted=48 distinctR=3 distinctW=3 neg=FIRED
              -- identical object bytes, stream cursor and emitted bytes
[BinWriter]   cases=4 records=4 fields=14 distinct=3 framing=4 neg=FIRED
              -- identical record bytes and cursor; framing arithmetic holds
[DlgSettings] cases=4 runs=7 nonzeroBytes=106 stubCalls=118 distinct=2 zeroCtrl=1
              onClosed=4 neg=FIRED -- identical record bytes and identical call traces
[RtDiff] IO::BinaryWriter record [probe]                   calls=1 worked=1 divergences=0
[RtDiff] UI::DialogInstance::SaveSettings/OnClosed [probe] calls=1 worked=1 divergences=0
```

Process exited cleanly (`standalone_crash.log` is 0 bytes; the final debug-log
line is the static-destructor attendance line).

**The `DlgSettings` probe compares the call TRACE as well as the bytes** — a port
that emitted identical bytes while visiting controllers in the wrong order, or
visiting one twice, would otherwise pass, because the per-controller records are
framed identically.

**Deliberate isolation:** registering a function does not rewrite the pointer in
the shipped DLL's own `.rdata` vtable, so *both* sides of the `DlgSettings` probe
drive the **DLL's** codec bodies. Any difference there is `SaveSettings`' own
logic; the codec is covered separately by `BinWriter`. The `[Encoding]` attendance
line staying at `CreateWriter=8` (4 + 4 from the two IO probes, none from
`DlgSettings`) is the live confirmation that this isolation holds.

### The first full run crashed, and it was the harness

ACCESS_VIOLATION reading `0x48`, symbolized to `VSlot` ← `SaveSettings+0xbe` ←
the probe. Cause: `memcpy(obj, &s->dlgVtbl, 8)` copies `dlgVtbl[0]` — a null slot
— **not the array's address**, so the port read vtable slot `+0x48` off a null
vptr. The neighbouring `dataPtr`/`bufPtr` lines had used the correct decay
pattern; only the vtable line took the address of the array. Fixed in the
harness, which is where the null came from — not with a guard in `SaveSettings`.

## 7. HONEST LIMITS

1. **The two loop-bound decisions in §4 are NOT distinguished by any oracle.**
   Telling them apart needs a controller that mutates the array from inside its
   own callback. The probe does not do that: it would be testing a scenario no
   caller in the game produces, and guessing what retail does when the array
   moves under it would manufacture a green. **Both behaviours are transcription
   only. This is the largest gap in the lane.**
2. **One declared divergence from the shipped bytes.** The deleting destructor's
   `flags & 2` (`delete[]`) arm is a **named abort** rather than a reproduction of
   the CRT vector-destructor path. It is unreachable by construction — the only
   producer of a BinaryWriter is `CreateWriter`'s bare `MemAlloc(0x30, 4)`, which
   writes no array cookie — and reproducing it would have opened an address door
   onto an unported CRT helper (`0x18007ec98`) for a path no caller can take.
   `flags = 0` is likewise unexercised.
3. **The free is not compared.** `deletingDtor(1)` releases through the
   allocator; the probe compares the bytes left behind and the returned pointer,
   not heap state.
4. **Stubs stand in for the dialog's `+0x48` and the controllers' virtuals.** The
   probe says `SaveSettings` *calls them correctly*, not that the real ones work.
5. **`size < 4`** (buffer too small for the head magic) is unexercised.
6. **Reachability is NOT measured for bodies 1–6, and is reasoned to be zero
   pre-cutover.** `OnClosed` is a virtual on the DLL's `DialogInstance` vtable and
   `SaveSettings` is called from inside the DLL, so with the DLL present the DLL's
   own bodies run. I did **not** add attendance counters to these six and did not
   measure it separately — the `[Encoding]` line above is adjacent evidence for the
   same mechanism, not a direct measurement of these. Stated as reasoning, not
   as a result.

## 8. Verified independently along the way

`0x180006a10`, the unexported MemFree the shipped deleting dtor calls, was
compared instruction-for-instruction against exported
`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (`0x1800c7ad0`): **byte-identical,
including the leading null check.** `compressedstream.cpp` asserted this; this
lane re-derived it rather than inheriting it.
