# Finding and changing game code in the portable editor

Start with your US Rev 0 ROM and the release folder. All commands below use the portable executable. Arguments to `--commands` are JSON arrays of operations. `--guide` and `--capabilities` work before selecting a ROM.

## Find the routine behind an edit

1. Search `lookup` for a gameplay term such as `actor identity`, `movement types`, `day and night` or `consumable`. Hits point into the shipped editor references. The per-function intake table documents one group of 926 newly accepted owners. `owners` covers every accepted translation unit in this release.
2. Use `symbol` with the exact function or binding name. Its `owner` lists the translation unit, targets, headers, digest and placed parts. `referencedBy` lists accepted source units with a linker binding of that name. Inspect each unit with `read` to determine whether the reference is a function call or data access. Indirect calls and assembly-only callers can require tracing the instructions. An empty owner means the symbol is known only as a dependency.
3. Read the function and headers. Use `regions` with one of the part's ROM offsets, then `region` with the physical region name for the original words and annotations. `assembly` takes a normalized ROM offset, RAM address and byte length. Use the owner's or region's placement for this overlay. A RAM address can denote different loaded overlays.
4. To locate a fact in other shipped game files, `files` accepts a filename `query` and literal `content` substring. `read` returns the text or lossless binary. Use `page` on list operations to bound the response. Unidentified assets remain readable by physical range. The release contains metadata and matching sources. Original game bytes come from the supplied ROM.

The same installation contains the editor's C++ feature consumers under `reference/editor/app`, game format readers under `reference/editor/engine`, and ROM builders under `reference/editor/rebuild`. For example, search `files` with `query:"reference/editor/app/"` and `content:"attackPower"` to find the desktop control. Use `find` on the returned file with `query:"powerPercent"`, then `read` with the returned one-based `line` and a `count` for surrounding code. Search the engine and rebuild folders for the same member. These files describe the shipped editor's behavior. Confirm original game behavior against the matched game source or the installed instructions.

## Choose a replacement

For a single-function owner, `patch.put` with `owner` fills the expected digest, RAM address, size, bindings and matching headers. The C entry name must match the function; the linker checks its address. In assembly, a source body can use the bound symbols directly, for example `jal function_name`. Supply the branch delay instruction explicitly. The assembler uses big-endian MIPS III, 32-bit ABI and no automatic reordering. Respect the original prototype, saved registers, stack alignment and return convention at every call site. Place branch labels on their own lines or before instructions. An assembler error includes the compiler tool's diagnostic.

For existing shared owners with several functions or separate tables, verify the exact allocation before using an explicit range. Do not treat a run of zeroes as an executable allocation. For a project using an editor-managed relocated module, use the build's original source digest and preserve the public entries. The feature builder places the replacement and updates its callers. Mutable global storage needs an existing verified game allocation. For in-place C using constant data, `inlineConstants:true` requests that read-only constants be included within the replacement's declared space. Validate their extent and the exported instructions.

Run `--build-plan --build-dir <folder> --report plan.json` against the project. The plan lists active modules and the ordered export steps with prerequisites, declared ROM ranges and recorded writes. The retained build folder contains the actual source files, included headers, assembly listings, link scripts, objects and ELF files. The ROM checksum is finalized after those steps. To compose with an earlier change, obtain the earlier step name from the plan, export that version and read the digest of the exact range with `bytes`. Use that name in `after` and that digest in `expectedSha256` on an explicit range patch. Check whether the earlier code is rebuilt or relocated when other project settings change.

## Build a two-function example

The release includes `reference/agent-patch-example.py`. It uses Python's standard library to send JSON to the CLI. It accepts three paths and creates a new destination folder.

```powershell
python .\reference\agent-patch-example.py --cli .\LordlyCaliber-cli.exe --rom $rom --destination .\actor-example
```

The example finds the matched actor-identity predicate from a gameplay reference and inspects the sources that call it. It writes assembly that merges the ID groups 0x54–0x55 and 0x56–0x57, preserving the other comparisons. It edits a C caller so that one roster operation scans its first 50 entries. The predicate change applies to its other callers too. The example reads back the new predicate, the caller's loop bound and the installed jump target from the exported ROM. This is a demonstration mod. Its output includes a sharing project, the game ROM and retained build artifacts.

For your own mod, export once with the patches disabled for comparison, then enable the patches and export to another file. Inspect installed instructions against the expected behavior and reopen the sharing project to confirm the source and bindings. Exercise the relevant route in the game for gameplay verification. Descriptions of unknown functions, live overlay state and custom instruction behavior can require further investigation.
