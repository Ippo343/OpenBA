// FIXTURE -- rawdoor_audit.py --self-test POSITIVE CONTROL.  Not compiled, not
// in ROOTS, never scanned by an ordinary audit run.
//
// Every function below MUST be reported.  A self-test that only asserts the
// tree is clean is a tautology: it passes when the matcher is broken, which is
// how a checker becomes decoration.  This file is the effectiveness control --
// if the matcher stops working, THESE rows disappear and the self-test fails.
//
// Expected findings (file:line is asserted by run_self_test):
//   UNGUARDED-CALL  fixture_immediate_call
//   DEFERRED-CALL   fixture_accessor_shape   (the shape the old classifier missed)
//   DEFERRED-CALL   fixture_static_accessor  (escape-return: the widgets_input shape)

using ReleaseFn = void (*)(void*);

void fixture_immediate_call(void* p) {
    // The one spelling the pre-hardening checker could see.
    RT_ORIGINAL_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ")(p);
}

void fixture_accessor_shape(void* a, void* b) {
    // The shape that classified OK-RESOLVE-ONLY and was wrong: the resolve and
    // the call are separate statements.  value_types.cpp held two of these.
    ReleaseFn release = RtOriginal<ReleaseFn>("?Release@String@System@@AEAAXXZ");
    // A long comment between the resolve and the call, deliberately, because a
    // line-window pairing would lose the call to it -- that is the 166 -> 162
    // incident, reproduced here so it cannot come back silently.
    //
    // ...
    // ...
    // ...
    // ...
    // ...
    release(a);
    release(b);
}

ReleaseFn fixture_static_accessor() {
    // widgets_input.cpp's StringToInteger() / RealNumberEditElementSetValue():
    // the pointer escapes by RETURN and is called at sites this scan cannot
    // see.  Reported as escape-return, not silently passed.
    static ReleaseFn fn = reinterpret_cast<ReleaseFn>(
        ResolveExportRaw("?ToInteger@String@System@@SAHPEBD@Z"));
    return fn;
}
