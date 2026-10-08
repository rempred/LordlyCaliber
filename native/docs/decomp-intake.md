# Using newly accepted Decomp C

The maintained [function list](../game/decomp-interest.json) maps upstream symbols
to concrete editor consumers, benefits, priorities and extensions to preserve.
It includes reconstructed behavior to replace, partial adaptations to finish,
already used source to keep current, and a smaller set of optional future uses.
It is a targeted starting list, not a requirement to port every game subsystem.

## Look up a new function or source file

From the editor repository root, use the read-only report command:

```powershell
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --match combat_pose_metadata.c
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --match func_00201798
node native/tools/decomp-interest.cjs --decomp 'C:/Users/Joe/Projects/OgreBattlel64/OB64 Decomp' --ready
```

Omit `--match` for the full list. `--json` gives structured output; without
`--decomp`, the list is still available with matching status explicitly unchecked.
`--ready` lists matching pure-C functions whose current source and recorded headers
are available and whose listed consumer is reconstructed, partial or a candidate.
It means ready to assess for integration, not safe to copy directly into PC code.
`adapted` and `original` consumers remain visible in the full list for updates.

Matching status is read from Decomp's existing `build/current/state.json`,
`verification.json` and `fresh-compilation.json`. The active target registry
provides source names for work not yet present in the verified report. A target
is not classified as matching just because it appears in that registry. An exact
`HYBRID_C` result is displayed separately from matching pure C. Changed source or
headers are flagged; the last report's accepted version remains useful upstream.
Missing/changing reports produce an explicit error, not an empty ready list.

The command reads existing evidence without running Decomp tools or entering its
production build directory. It does not import sources, rebuild anything, watch
for changes or maintain a second acceptance database. It does not compare older
portable adaptations semantically or claim their copied source is up to date.

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
