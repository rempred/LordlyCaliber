# Native C export migration — 2026-10-16

This completes the eleven complete-function migrations identified by the
[October 15 audit](source-selection-audit.md). These functions compile during
the affected export, using the bundled authenticated toolchain and source.
No user installation of Decomp is required.

| Export feature | Original function | Source selected |
|---|---|---|
| Sleep and Paralysis recovery | 0022EDD4, 0022EC08 | Accepted pure C |
| Wake on physical hit | 0022A964 | Accepted pure C, or the recognized weapon-status C adaptation |
| Class eligibility | 000C37EC | Accepted hybrid C |
| Encounter selection and rates | 0013C060 | Accepted pure C |
| Encounter rewards | 00101DDC | Accepted pure C |
| Custom encounter card text | 000E5968 | Accepted pure C |
| Action budget and menu iteration | 00215CF0, 002159D0 | Accepted pure C |
| Site/card message and battle round | 0010DDB4, 0021EBBC | Accepted pure C and qualified switch-table placement |

## Compilation and compatibility

`game/accepted/owners.json` records the source hashes, headers, original ROM and
RAM placement, linker bindings, original text hashes and switch-table hashes.
The parent generator `tools/qualify_native_c_owners.py` freshly compiles each
source and qualifies text and tables against the read-only master ROM. Its
inputs are the authenticated accepted reports, bundled source and pinned tools.
Hybrid C is kept separately from the pure-C library, retaining its classification.

`Compiler::compileAcceptedOwner` authenticates that source closure, compiles C,
links at the original address and checks the original text and tables. Word
alignment is set on the object section so the linker cannot insert entry NOPs.
Only compiler alignment padding after the function may be discarded.
No original function instructions are stored in this manifest.

Existing mod behavior remains in bounded, separately checked hooks: recovery
uses three threshold/comparison instructions; class eligibility keeps its single
family-return comparison; custom callers retain their two-instruction call sites.
Dispatcher and reward hooks occupy 120 bytes each within the compiled owners.
The seven full-function assembly sources have been removed from the product.
This migration does not claim that the mod hooks themselves are pure C.

Existing exported ROMs retain their previous instruction layout and readback
rules. The accepted card-message body ends four bytes before its historical
physical owner; its separately authenticated suffix is preserved. The large
handlers' switch tables retain their original addresses. Character/class and
artwork reference relocation still runs after compiling and applying hooks.

Wake-on-hit editing compiles the exact recognized version: original retail,
the earlier Poison-only adaptation, or the current six-status adaptation.
The earlier source and manifest are retained as `weapon-hit-v1.*`, from editor
commit `b0479ce`. Changing recovery therefore does not upgrade another behavior.
Unknown loaded code still fails authentication and remains preserved.

## Checks and remaining scope

The static source-selection audit now reports zero catalogued complete-owner
assembly gaps and zero unclassified assembly files. It inventories 66 assembly
files, including the new bounded hooks. Run from the editor root:

```powershell
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --require-clean
```

`acceptedSourceOwners` compares all eleven compiled owners with the private ROM;
the compiler also verifies their switch tables. Recovery tests exercise edited
chances, export/reopen, stable re-export and retail restoration. The compatibility
test checks editing and restoration of both prior and current weapon-status C.
Encounter and promotion tests exercise the actual migrated export paths.

Validation passed: Release build; all eleven original C owners/tables; status
rules and both weapon-version recovery round trips; global/scenario rates;
rewards; custom caller install/remove; expanded pools; promotion-family and
166/256-class menu export; public added classes; 30-enemy full-army save/load
and composition with added classes, custom encounters, training and recovery.
The save tests performed 1,024 writes within 32 KiB simulated SRAM. MIPS probes
also passed for global gates, 13,750 scenario gate cases, candidate draws/removal,
all 256 reward classes, custom adapters and installed encounter flows. Source
ROM preservation, export checksums and stable reopened exports are covered.

The expanded-menu test's old-version fixture now restores the suspend-menu
owner as well as Pak/reset owners before marking itself version 1. Previously
it mixed a current version-4 hook into that older fixture and correctly failed
authentication. Save-runtime product code was not changed.

The separate assembly cases from the earlier audit remain: the custom two-register
family lookup, bounded patches in other owners, authored extension modules and
two unselected historical sources. Untouched game code is preserved from the
source ROM. This is not a conversion of the entire game or a full-game emulator
test; it changes the source selected for the eleven previously identified paths.
