# Attack playback regression harness

`AttackDataTests::playbackPipeline` exports a fresh US Rev 0 ROM with two new
attack IDs and High Attack enabled, verifies reopening, then runs
`attack-playback-mips.cjs`. The ROM and compiler are private local inputs;
neither belongs in Git.

Run the Qt test with `LORDLY_TEST_ROM` pointing at the read-only master and
`LORDLY_N64_TOOLCHAIN` pointing at the authenticated compiler directory:

```powershell
./native/build/Release/attack_data_tests.exe playbackPipeline
```

The test prints the candidate path under the parent `Test ROMs` directory.
For a longer deterministic run against that candidate (keep its ELF files):

```powershell
$env:LORDLY_PLAYBACK_CASES = '2048'
$env:LORDLY_PLAYBACK_SEED = '7319'
node native/tests/attack-playback-mips.cjs '<candidate ROM path>'
```

Each run writes `attack-playback-report.json` beside the candidate. Save a
failure object, its `minimized` configuration, or any configuration as JSON
and pass that file as the third argument to replay it. The first failure of
each error category is minimized by removing options while retaining that
failure. This is a useful smaller reproduction, not a proof of global minimality.
Each call and the playback loop have instruction/tick limits. Seeds and counts
are explicit; generated tests are reproducible rather than nondeterministic CI.

## What executes

- Newly exported generic, melee, ranged, alternate, combined and Pedra producers.
- Native command insertion and variable-length packet ordering.
- Native impact consumption and its real damage/status presentation writers.
- Exported playback, absolute-time seeking and completion barriers.
- Normal and real High Attack relocated buffer layouts.
- A following healing action in the same stream, so late insertion cannot
  silently discard the next action.

The harness varies targeting, side, strike count (through 18), hit/miss,
immunity, HP boundaries, incoming power, added status/chance/condition,
drain/recoil/basis/percentage, and early/late contact times. It checks final HP
and ailment flags against calculation, packet boundaries, native result queues,
finite completion and stack balance. Existing attack tests separately check
target eligibility, formulas, names, class assignment, project persistence,
export/reopen, checksum, expanded armies and rendered repeated effects.

Configurations alter the exported attack/settings tables in **test RAM**;
they do not edit the source ROM. The Qt fixture independently verifies the
exported table's persistence and newly allocated IDs.

## Controlled services and limits

Actors use the native record layout, but roster discovery and input scores,
hit chances and immunity are fixtures. Artwork metadata and pose completion
are finite controlled services; effect dispatch schedules explicit contacts
and visual completion. Graphics, audio, battle AI, caches and the full console
are not emulated. Native HP/status writers, impact callbacks and clock services
are deliberately not mocked. Floating-point support is the existing finite
COP1 test interpreter, not full FCSR/exception emulation.

This catches calculation/playback disagreement and reproducible queue/clock
stalls. Passing does not prove every retail effect emits the right contacts,
or that a class uses the desired sprite pose in Project64. Those require the
separate effect tests and focused gameplay checks. In particular, the reported
Vampire Take a Peek/Abyss combination needs the exact edited configuration for
a visual reproduction; effect artwork and class body movement are independent.
