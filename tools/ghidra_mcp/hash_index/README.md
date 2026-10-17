# Ghidra function-hash indexes (gitignored, regenerate locally)

`*.json` here and `docs/history/runtime_port/runtime_hash_index.json` are bulk maps of every
function in the retail binaries (normalized-opcode sha256 -> address, name, completeness score),
built with Ghidra. They are derived from the retail binaries, so they are not tracked: metadata,
not assets, but kept out of the public tree. Nothing in the build, the tests or the live tools
reads them. They fed the profile-to-retail function mapping (`ToolKit/PROFILE_ANALYSIS.md`,
`docs/history/runtime_port/hashmap_report.md`).

Format (all five): `{"version","hash_algorithm":"normalized_opcodes_sha256","functions":{hash:
{"canonical": instance|null, "instances":[{program,address,name,completeness_score,indexed_at}]}}}`.

| File | Programs (Banished_RE.gpr) | Filter (inferred) | Dated |
|---|---|---|---|
| `retail_hash_index.json` | Application-x64.exe | all | 2026-03-13 |
| `profile_hash_index.json` | Application-x64-profile.exe | documented (no FUN_ names in it) | 2026-03-13 |
| `cross_hash_index.json` | both of the above | all | 2026-03-13 |
| `function_hash_index.json` | both of the above (the bridge default file name) | all | 2026-03-14 |
| `docs/history/runtime_port/runtime_hash_index.json` | Runtime-x64-profile.dll + Runtime-x64.dll | all (stated in hashmap_report.md) | 2026-09-06 |

Producer: the bridge tool `build_function_hash_index(programs, filter, index_file, merge)` in
`tools/ghidra_mcp/bridge_mcp_ghidra.py` (itself untracked). The exe-index parameters were not
recorded; the filters above are inferred from the file contents, and the originals were built with
`merge=True` over several runs, so a regenerated file will not be byte-identical (timestamps and
counts may differ).

## Regenerate

Start Ghidra with `Banished_RE.gpr` (skill `ghidra`), then:

    python tools/ghidra_mcp/regen_hash_index.py all        # or retail|profile|cross|function|runtime

The script calls the same GhidraMCP HTTP endpoints as the bridge and exits with a clear message
when the server (`GHIDRA_SERVER_URL`, default http://127.0.0.1:8089/) is down. Program paths
default to `/<program name>`; override with `--retail-path` etc. The script has not been run
against a live server by its author.
