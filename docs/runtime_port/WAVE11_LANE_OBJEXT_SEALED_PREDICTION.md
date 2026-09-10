Wave 11 lane OBJEXT: SEALED correction to the ctl=5 prediction, before the run

Committed BEFORE reading any result from the queued run. The prediction in
8920ee5 is already in the binary under test (d8cd53ba) and is NOT being
changed -- changing a prior after the data is the move that makes a prior
worthless, and re-queueing a third time would cost the wave a lock cycle for a
prediction I can simply correct on the record instead.

WHAT I NOW EXPECT THE INSTRUMENT TO FLAG, AND WHY

Applying the rule the team lead set one message ago -- grep for OUR port of a
callee before deriving its semantics -- to the OTHER callees this lane uses
turned up a second caveat, in the same class as the GetPathFilename one and
missed the same way.

runtime/src/system/filename.cpp:366-370, on GetPathIdentifer:

    Returns everything after the last ':' -- i.e. the path with its drive or
    package identifier removed. If there is no ':' at all the whole path comes
    back UNLESS a '.', '/' or '\' was seen on the way, in which case the result
    is empty.

I had assumed "no ':' means empty identifier". That is true for every input in
the set EXCEPT index 10, "Solo", which has no ':', no '.', and no '/' -- so its
identifier is the WHOLE STRING "Solo", not empty.

ctl=5 substitutes the extension for the identifier and is therefore dead only
where the two are EQUAL. For "Solo": ext="" but ident="Solo", so they differ
and the control SHOULD bite.

  committed prediction  ctl 5  "XXXX.XXXXX..XX"   (4, 10, 11 insensitive)
  corrected  prediction ctl 5  "XXXX.XXXXXX.XX"   (4, 11 insensitive)

SO THE SEALED CLAIM IS: the run will print exactly one disagreement, at ctl=5
index 10, and none anywhere else. If it prints that, the instrument caught a
model error I had already found by another route, and both are working. If it
prints disagreements elsewhere, my model is wrong in a way I have not found and
the lane doc says so.

Index 10 is also the reason the earlier 5->10 enlargement would have proved
nothing: it added extension-BEARING paths, and ctl=4 can only bite on
extension-less ones. "Solo" is in the set because the CORRECTED model of
GetPathFilename put it there, and it is now also the input that tests
GetPathIdentifer. Inputs chosen from a model find things; inputs chosen to pad
a count do not.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
