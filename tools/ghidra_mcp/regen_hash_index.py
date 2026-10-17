#!/usr/bin/env python3
"""Regenerate the Ghidra function-hash indexes (gitignored, see hash_index/README.md).

Talks to the running GhidraMCP HTTP server (GHIDRA_SERVER_URL, default
http://127.0.0.1:8089/) with the same endpoint calls and the same JSON format as
`build_function_hash_index` in bridge_mcp_ghidra.py (open_program, then paged
get_bulk_function_hashes). Needs Ghidra running with Banished_RE.gpr open.

    python tools/ghidra_mcp/regen_hash_index.py retail|profile|cross|function|runtime|all

The recipes are INFERRED from the existing files (see README.md); the original
call parameters were not recorded. Program paths default to "/<name>" in the project
root; override with --retail-path etc. if `list_open_programs` shows otherwise.
Standard library only.
"""
import argparse
import json
import os
import sys
import urllib.parse
import urllib.request
from datetime import datetime

SERVER = os.environ.get("GHIDRA_SERVER_URL", "http://127.0.0.1:8089/").rstrip("/")
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "hash_index")
RUNTIME_OUT = os.path.join(HERE, "..", "..", "docs", "history", "runtime_port",
                           "runtime_hash_index.json")

PROGRAMS = {
    "retail": "/Application-x64.exe",
    "profile": "/Application-x64-profile.exe",
    "rt_retail": "/Runtime-x64.dll",
    "rt_profile": "/Runtime-x64-profile.dll",
}
# index -> (output path, [program keys in scan order], filter)
RECIPES = {
    "retail": (os.path.join(OUT, "retail_hash_index.json"), ["retail"], "all"),
    "profile": (os.path.join(OUT, "profile_hash_index.json"), ["profile"], "documented"),
    "cross": (os.path.join(OUT, "cross_hash_index.json"), ["profile", "retail"], "all"),
    "function": (os.path.join(OUT, "function_hash_index.json"), ["profile", "retail"], "all"),
    "runtime": (RUNTIME_OUT, ["rt_profile", "rt_retail"], "all"),
}


def get(endpoint, **params):
    url = "%s/%s?%s" % (SERVER, endpoint, urllib.parse.urlencode(params))
    try:
        with urllib.request.urlopen(url, timeout=120) as r:
            body = r.read().decode("utf-8", "replace")
    except OSError as e:
        sys.exit("error: cannot reach the GhidraMCP server at %s (%s).\n"
                 "Start Ghidra with Banished_RE.gpr open (skill `ghidra`, "
                 "docs/reference/HOW_TO_START_GHIDRA.md) and retry." % (SERVER, e))
    try:
        return json.loads(body)
    except ValueError:
        sys.exit("error: non-JSON reply from %s: %s" % (endpoint, body[:200]))


def scan(index, path, flt):
    res = get("open_program", path=path)
    if "error" in res:
        sys.exit("error: open_program %s failed: %s" % (path, res["error"]))
    name = res.get("name", path)
    offset, batch, n = 0, 500, 0
    while True:
        res = get("get_bulk_function_hashes", offset=offset, limit=batch, filter=flt)
        if "error" in res:
            sys.exit("error: get_bulk_function_hashes on %s: %s" % (name, res["error"]))
        funcs = res.get("functions", [])
        for f in funcs:
            custom = f.get("has_custom_name", False)
            score = 0
            if custom:
                score = get("analyze_function_completeness",
                            address=f["address"]).get("completeness_score", 0)
            inst = {"program": name, "address": f["address"], "name": f["name"],
                    "completeness_score": score,
                    "indexed_at": datetime.now().isoformat()}
            e = index["functions"].setdefault(f["hash"], {"canonical": None, "instances": []})
            e["instances"].append(inst)
            if custom and (e["canonical"] is None
                           or score > e["canonical"].get("completeness_score", 0)):
                e["canonical"] = inst
            n += 1
        offset += batch
        if len(funcs) < batch:
            break
    print("  %s: %d functions" % (name, n))


def build(key, paths):
    out, progs, flt = RECIPES[key]
    out = os.path.normpath(out)
    print("%s -> %s (filter=%s)" % (key, out, flt))
    index = {"version": "1.0", "hash_algorithm": "normalized_opcodes_sha256", "functions": {}}
    for p in progs:
        scan(index, paths[p], flt)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as fh:
        json.dump(index, fh, indent=2)
    print("  wrote %d unique hashes" % len(index["functions"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("which", choices=list(RECIPES) + ["all"])
    for k, v in PROGRAMS.items():
        ap.add_argument("--%s-path" % k.replace("_", "-"), default=v)
    a = ap.parse_args()
    paths = {k: getattr(a, k + "_path") for k in PROGRAMS}
    for k in (RECIPES if a.which == "all" else [a.which]):
        build(k, paths)


if __name__ == "__main__":
    main()
