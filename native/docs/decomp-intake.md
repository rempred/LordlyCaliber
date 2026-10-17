# Using newly accepted Decomp C

## October 17 library intake

The full seven-wave library batch from Decomp `17e07534` is imported: 212 new
functions, with no previously imported function lost. The current manifest has
1,102 compiler-recorded functions from 1,057 PURE_C targets in 926 complete
producer files, plus 24 included source/header dependencies. Its acceptance
fingerprint is `171A02BF`. Six retired single-function files were replaced by
their complete upstream producers. Existing adaptation baselines are unchanged.

Graphics/math, audio, Controller Pak and system-service sources are available
for future consumers. This intake does not switch the renderer, add audio output,
or alter gameplay/export behavior. The native selection audit authenticates
1,057 PURE_C and 45 HYBRID_C targets / 995 source-and-header files and reports
zero catalogued whole-owner assembly gaps and zero unclassified assembly files.

The importer, audit and interest lookup share `tools/decomp-evidence.cjs`.
It supports report versions 5 and 6. Version 6 also authenticates each explicitly
named source-object proof, joins its source/class/dependencies to the fresh and
linked reports, verifies representation identities and exact physical owners,
and retains complete-producer membership and compiler provenance. It does not
run upstream tools or reproduce upstream matching acceptance.

`node --test native/tools/tests/decomp-evidence.test.cjs` exercises intake and
failure paths using synthetic evidence, including multiple functions/physical
owners, complete producers, changed reports/sources/headers/proofs, and safe
retirement of prior imports. It is also registered as CTest `decomp_intake`.
Real intake separately checks all imported file bytes against accepted hashes.
All 24 fixture checks, the registered CTest, the 19 new interest entries and the
real source-selection audit passed for this intake. Native application behavior
was not changed, so no application rebuild or emulator test was required.

Joe subsequently authorized preview adoption from this batch. Sine/cosine,
look-at, perspective and RPY rotation now have active portable consumers;
`game/accepted/graphics-imports.json` records their baselines independently of
the library. See [the graphics math architecture note](architecture.md) and the
per-function interest statuses; packed wrappers remain available references.

## Source selection and lookup

The [native source-selection audit](source-selection-audit.md) records the actual
export paths that still use assembly despite accepted C or matching hybrid C.
Run `node native/tools/audit-source-selection.cjs --decomp <checkout>` after an
intake; `--require-clean` fails when catalogued whole-owner gaps remain. This
checks executing source selection separately from bundled source availability.

The maintained [function list](../game/decomp-interest.json) maps upstream symbols
to concrete editor consumers, benefits, priorities and extensions to preserve.
It includes reconstructed behavior to replace, partial adaptations to finish,
already used source to keep current, and a smaller set of optional future uses.
It is a targeted starting list, not a requirement to port every game subsystem.
The complete accepted pure-C library is under `game/accepted/library/`, with
source/owner/dependency provenance in `game/accepted/library-imports.json`.
`--all` includes that library in the report; `--match` also searches functions
outside the targeted interest list. Unclassified consumers are explicitly named
as such. Library refreshes retain the older animation/gameplay adaptation baselines.
Complete accepted batches may also be retained as `imported` for future use.
That status means the original source and dependencies are bundled, with no
current executing consumer. It remains visible in `--ready` for later adaptation.

## Look up a new function or source file

From the editor repository root, use the read-only report command:

```powershell
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --match combat_pose_metadata.c
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --match func_00201798
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --ready
node native/tools/decomp-interest.cjs --all --match func_00040f88
```

Omit `--match` for the full list. `--json` gives structured output; without
`--decomp`, the list is still available with matching status explicitly unchecked.
`--ready` lists matching pure-C functions whose current source and recorded headers
are available and whose listed consumer is reconstructed, partial or a candidate.
It means ready to assess for integration, not safe to copy directly into PC code.
`adapted` and `original` consumers remain visible in the full list. For sources
recorded in `game/accepted/animation-imports.json`, `gameplay-imports.json` or
`graphics-imports.json`, a newer verified source identity
is marked **upstream changed** and returns to `--ready`. `companion` marks an empty
helper retained with its coupled source, rather than a missing editor feature.
Imports with recorded dependencies also detect changes to included C and headers,
even when the producer's own file is unchanged. Functions inside a shared owner
are resolved through its verified compiler-function list. The JSON report names
the accepted owner and the bundled source file explicitly.

Matching status is read from Decomp's existing `build/current/state.json`,
`verification.json` and `fresh-compilation.json`. The active target registry
provides source names for work not yet present in the verified report. A target
is not classified as matching just because it appears in that registry. An exact
`HYBRID_C` result is displayed separately from matching pure C. Changed source or
headers are flagged; the last report's accepted version remains useful upstream.
Missing/changing reports produce an explicit error, not an empty ready list.

The command reads existing reports and, for version 6, only the hash-bound proof
files named by those reports. It does not run Decomp tools, import sources, rebuild anything, watch
for changes or maintain a second acceptance database. It does not compare older
portable adaptations semantically. Older adaptations without an import identity
still require comparing their documented revision when updating them.

## Integration order

1. **P1: animation, actors and resources.** Start with combat pose metadata and
   action selection. Then use newly matching cast, motion, layering, projection
   and image readers where they improve the corresponding editor behavior.
2. **P2: existing features.** Keep the already adapted Director/dialogue functions
   current; replace reconstructed promotion, Save and background behavior as the
   corresponding C becomes available.
3. **P3: useful expansion.** Assess Squad condition/lifecycle and Shop helpers for
   better behavior previews and source-built exports. These do not make full-game
   simulation or recreation of the game's entire menu renderer prerequisites.

## When incorporating a function

- Select the version identified by the existing matching record. Preserve its
  source and required headers/companion functions. A shared `.c` file can own
  several linked functions or tables; import the complete source context.
- Keep unchanged recovered source identifiable under `game/accepted/`, or beside
  an established adapter where that is already the convention. Record upstream
  source path, revision and verified source identity at the integration point.
- Compile portable adaptations into the engine for previews; compile ROM-side
  sources with the bundled N64 toolchain during user export. Make the intended
  consumer actually call the new implementation. Users never need Decomp.
- Preserve expanded tables, private animations, project compatibility and host
  bounds. Original matching source reproduces retail restrictions too. Document
  intentional differences rather than overwriting editor extensions.
- Reuse the affected workflow checks. For changed exports, check relevant
  allocation/references/checksum and reopening. For PC adaptations, compare the
  relevant behavior at the portability boundary. Explain any new test's concrete
  uncovered failure before adding it; do not repeat Decomp's matching process.
- Update `editorUse`, per-function `symbolUses` and preservation notes when
  integration lands. Use a per-function override for partially adopted groups. Matching
  status stays upstream, and successful integration is separate from matching.

The function list's evidence comes from the named editor source comments and
consumers. Squad W3 and Shop entries additionally use their accepted wave records
in Decomp (`5490bb78` and `46525af8`). Candidate benefit is a proposed editor use,
not proof that an entire game function is already implemented or needed.

## Refresh the complete library

`node native/tools/import-accepted-c.cjs --decomp <checkout>` validates the
passing snapshot and all source dependencies without writing. Add `--write` to
refresh the bundled library and its manifest. It rejects unsupported evidence
versions and changed source identities; it never runs a Decomp build. Existing
adaptation baselines and third-party notices remain intact.
Refreshes remove a retired file only when the previous library manifest names it
and its bytes still match that manifest; local edits cause refusal before writing.
Unlisted historical/support files are preserved.

Use the passing `build/current/state.json`, `verification.json` and
`fresh-compilation.json` together. Select every exact `PURE_C` owner, join its
compiler-recorded function names, and retain each complete producer source plus
all recorded included C/header dependencies. Verify current file identities
against that report before copying; an unfinished newer source is not a replacement
for the accepted version. Preserve relative paths, original bytes, and required
third-party source/notices. Record the source revision and acceptance identity in
`library-imports.json`; inspect and commit the resulting source changes. Do not
replace active adaptation baselines merely because a library file is newer.
