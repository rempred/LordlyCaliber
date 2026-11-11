# Native export source audit

## Body/equipment C intake and migration — 2026-11-09

Accepted snapshot `5894EA0E` adds pure-C `002AB810` and `002AAC60`. Both now
participate in the compiled ROM baseline: 1482 producers, 1659 functions and
1698 exact code/table blocks. No older imported source or header changed.

Complete accepted-derived C replaces the body/equipment fragments in `00205C88`,
`00206340`, `00207658`, `002AAC60`, `002AB810` and `002ABB3C`. Private artwork,
equipment suppression, body construction, linked/special bodies, fixed poses and
wide frame markers retain their existing behavior. Code, helper and parameters
fit the original allocations; no new runtime allocation is required.

The equipment allocator also consumes the relocatable descriptor directory.
Its C reads a data word at z64 ROM `0x2060E8` (runtime `0x801C2C58`); directory
relocation updates this data instead of the former two instruction operands.
The C policy query occupies z64 `0x207A30` (runtime `0x801C45A0`), with the
private-mapping back-offset at z64 `0x207A6C`. `LC05VPOL` version 3 authenticates
complete functions, tables and parameters. `LC05POSE` version 3 uses the new C
query ABI. Versions 1–2 remain readable and upgrade during export, including CLI
exports that have not opened the artwork editor. Legacy ownership checks also
verify code outside the old fragments and account for relocated directories.

Checks execute full exported functions against original ROM instructions;
cover all 256 equipment class IDs, 137 real pose banks, short/wide marker banks,
private/linked poses, body-policy inheritance, project persistence, allocation
bounds, corruption rejection, and exact second exports. Three genuine previous
exports also upgrade and re-export identically. An unedited ROM remains byte
identical. The public CTest suite has no failures (four private-ROM targets skip
without a ROM); focused private-ROM tests run separately. Resource allocation,
transfer, terrain and audio services are controlled in instruction tests; this
is not a new full-emulator playthrough.

The source-selection census now reports zero larger C-covered instruction
adapters and zero whole-owner gaps. The 33 extension hooks and mixed groups
below remain deferred. The old body/equipment assembly is retained only for
legacy recognition and regression fixtures, not selected by the native writer.

## Consumable C intake and migration — 2026-11-04

Decomp snapshot `51174C7C` adds seven accepted pure-C producers: `00040FF4`,
`00041098`, `000C4650`, `000C9850`, `001A9290`, `001AD0E4`, and `001AD8CC`.
No previously carried source or header changed. The active ROM baseline now
compiles 1480 producers covering 1657 functions from 1611 accepted owners,
with 1696 exact code/table blocks. All seven additions are included in that
baseline, rather than only bundled as reference files.

The newly recovered consumable application function `00041098` also replaces
the editor's whole-function assembly implementation. Its maintained C adaptation
preserves the query/apply phases and special Soldier-pool healing limits. The
ten authored models use fifteen parameter halfwords in a checked footer, within
the original 1380-byte allocation. User exports compile and authenticate the C;
reopening authenticates its code and parameters. Older parameter-edited ROMs
upgrade automatically without losing their values. Unchanged retail exports
still use the exact accepted implementation.

`consumables/effects.s` remains a compatibility fixture, not an export source.
The source-selection gate again has zero whole-owner assembly gaps and zero
unclassified assembly files. The seven larger adapters and 33 extension hooks
described below remain deferred mixed groups; this is not a C-only exporter.

## Adapter migration — 2026-11-01

The follow-up migration replaces six previously reported C-covered sites with
complete maintained C adaptations:

- All four shop consumers, their switch table and the empty-selection guard.
  Halfword consumable IDs, equipment categories and Cancel behavior remain.
  Version 5 exports recognize older versions 1–4 and regenerate class/item
  references from compiler relocations.
- The entire settlement function `001390F0`, including configurable Goth and
  the reserve-XP loader. Original wallet limits, mission exclusions and item/
  Soldier awards remain. The loader uses the game's synchronous DMA service;
  the separate editor-authored reserve-XP module remains unchanged.
- The full task-submission C producer containing the Chaos-counter call.
  Its ordinary C call replaces the prior jump adapter, while preserving the
  counter's separate loader, module and return contract.

The audit now reports **7 larger C-covered instruction adapters and 33 extension
hooks**, zero whole-owner gaps and zero unclassified assembly files. This is
intentionally not an entirely C-only exporter. Joe requested that mixed groups
wait until complete C is available. Body/equipment visuals include three
unrecovered fragments; wide markers share that constructor. Expanded capacity
has 17 C-covered sites among 102 hooks. High Attack has 546 additional runtime
reference patches, including unrecovered functions. Animation overrides share
its `0021CBC4` constructor and fixed register/continuation contract, so they stay
with that group. Custom encounters likewise retain mixed recovered/unrecovered
consumers. The audit lists these deferrals rather than hiding their assembly.

Qualified adaptations authenticate source/header identities, compiled code and
switch tables. No compiled instruction rewriting implements the migrated shop
or settlement behavior. Relocating declared data pointers remains necessary for
expanded classes, items and armies. Original `.s` fragments remain historical
compatibility fixtures, excluded from the active native resource selection.

Validation includes complete exported shop, settlement and task-submission
functions in the instruction interpreter; older-export upgrades; custom weapon
categories; expanded armies; export/reopen; unchanged second exports; and exact
untouched-ROM identity. Cache, DMA, drawing and OS services are trapped in these
tests; this does not claim a new full-emulator playthrough.

## Pre-migration audit — 2026-11-01

This updates the historical October 15 record below and the
[October 16 migration](source-selection-migration.md). It covers the native
editor, not Editor-Old. Decomp CURRENT `04E67D51`, commit `8038c349`, is unchanged
from the preceding intake: 1569 PURE_C and 35 HYBRID_C accepted targets, producing
1650 functions. Every accepted producer already participates in the exact ROM
baseline build. That proves retail source coverage, not that all custom patches
are C-only.

The expanded selection audit checks 206 ranges and inventories 66 assembly
files. There are no catalogued whole-function assembly replacements with an
available accepted C owner, and no unclassified assembly files. It separately
reports **12 larger instruction adapters and 34 extension hook sites inside
accepted C**. These remain assembly; the audit is not fully clean.

### Feature review

| Area | Active implementation and finding |
|---|---|
| Classes, families, promotion, equipment categories | Records and relocated tables are data. Selection/menu/runtime extensions already compile C. The extended family lookup now compiles an accepted-derived C adaptation, replacing its assembly selector. An empty register constraint retains the two-register ABI; this is not newly accepted upstream pure C. |
| Items, weapon types, names, descriptions | Record/text edits compile data. Category dispatch and expanded item/class consumers use maintained C plus relocation and entry adapters. No new meaning was assigned to unknown item fields. |
| Consumables | Effects remain a maintained assembly owner; its original routine lacks accepted C. Accepted inventory addition confirms the existing quantity cap of 99 and available-versus-total count rules. |
| Shops | Stock construction uses adapted accepted C. Four consumers still use assembly width/guard patches. Compiling wider C types changes instruction layout/register allocation; migration must also update class-data relocation, authentication and reopened-ROM recognition. The unchanged retail C alone cannot replace those extensions. |
| Attacks and targeting | Authored runtime behavior is C; the retail calculation/effect owners use the accepted baseline. Bounded hooks connect the extensions. Existing damage rules retain defense, resistance, rounding and HP-based special handling. |
| Attack effects | All 133 retail effect programs are covered by the existing compiled preview pack. Artwork/composition edits use maintained runtime C and resource edits. This audit found no additional missing accepted program to import. |
| Encounters and custom squads | Selection, rewards and the migrated custom callers compile accepted/adapted C with bounded hooks. Persuasion and combat leader-affinity table lookups are now explicitly audited: their original enclosing functions still have no accepted C. |
| Scenarios, squads, orders and events | Record, route, flag and map edits compile data. Expanded-army owners compile available accepted C; register-sensitive predicate/ownership hooks remain. Recovered source does not establish every unknown event flag's meaning. |
| Towns and treasures | Accepted `func_0012FBB8` exposed an actual editing defect: morale's high bit is a 40-point takeover penalty, not part of its numeric morale. The UI/model now handle this separately and preserve the exact 255 objective marker. |
| Tools: rates, recovery, combat and economy | Tactics/elements/weapon/soldier-population settings are data. Sleep, Paralysis and wake recovery use C with bounded chance hooks. Clock, fatigue, XP scaling and remaining attribute-rule routines do not have accepted replacement C. Goth settlement still uses a larger assembly fragment inside accepted `001390F0`. |
| Tools: displays, High Attack and reserve XP | Added displays/bootstrap and multi-action behavior are editor extensions. Their small ABI hooks remain assembly, including 9 High Attack/Chaos-counter sites inside accepted functions. Goth's spare fragment also houses the reserve-XP bootstrap, so an unqualified whole-function replacement would remove another feature. |
| Combat sprites, animation and graphics | Host decoding/selection uses existing portable C adaptations; source-built retail functions supply the ROM baseline. Seven larger body/equipment fragments and 8 animation entry/return/carrier sites still alter accepted owners. Their wide-frame, equipment and private-artwork behavior is absent from retail C. |
| Save Editor, expanded saves and Controller Pak | Host editing uses record codecs and accepted-derived initialization/checksum behavior. Expanded save/compression/game services compile maintained C. Pak ABI adapters remain assembly; those enclosing Pak functions lack accepted C. |
| Testing, Art Library, imports/exports, recovery and table utilities | These are host workflows; serializing a project, assigning a battery save or exporting an image has no game assembly owner to replace. They use the same export/runtime consumers described above. |
| Cutscene/scene previews | Existing bounded host adaptations retain explicit resource, matrix and draw services. Raw console C is not a drop-in desktop renderer; accepted source availability does not imply exact console pixels or complete game simulation. |

### Remaining larger adapters

All offsets below are US Rev0 **z64 ROM offsets**. The enclosing accepted C is
compiled into the baseline; these feature-specific fragments are then applied.

| Adapter | Accepted enclosing function(s) | Required migration work |
|---|---|---|
| Goth reward arithmetic | `001390F0` | Preserve continuation/caller frame, wallet cap, scenario exclusion, divisor readback and shared reserve-XP bootstrap allocation. |
| Four shop readers | `001977E0`, `001989C4`, `001994B4`, `0019A294` | Port wider stock types and guards together with relocation/old-export recognition. A compile experiment confirms this is not a byte-stable substitution. |
| Equipment registration/allocation/images | `00207658`, `00205C88`, `00206340` | Preserve equipment suppression, private-resource ownership, custom layout and existing allocation contracts. Five fragments. |
| Sticky roster body | `002ABB3C` | Preserve the identity/current-class visual policy. |
| Wide-frame equipment markers | `00205C88` | Preserve expanded frame opcodes and equipment marker behavior. |

The additional 34 C-covered extension sites comprise 17 expanded-capacity
predicate/ownership hooks, 8 animation hooks/carriers, 8 High Attack hooks and
1 Chaos-counter hook. Existing compiled-C owner catalogs also identify their
bounded encounter/promotion/recovery hooks. Counts describe named catalogued
sites, not a proof that every individual generated instruction has been migrated.

### Verification and reproduction

`class_menu_mips.cjs` executes the newly compiled lookup and the legacy reference
for all byte IDs, high input bits, and retail/expanded roster bounds: 1536 cases,
both return registers, no RAM writes. Private promotion/added-class exports,
class-menu layout and the unchanged-ROM baseline check pass. The latter verifies
all compiled regions and an entirely byte-identical untouched v64 export.

The world UI tests browse every retail site without modifying it, check morale
and penalty decoding, edit the penalty, exercise undo/redo and objective toggles.
`worldAuthoringExport` checks project-folder/archive persistence, ROM export and
reopening of a changed packed morale byte. Its old treasure rejection fixture
was corrected to ID511: expanded equipment now supports IDs through510.

These are automated engine, UI, compiled-code and instruction-level checks,
not a new emulator playthrough of every feature. No additional gameplay defect
was established by the shop compile experiment or by remaining assembly use.

```powershell
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --json
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --require-clean
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --require-whole-owners
```

The full `--require-clean` gate now exits2 for the remaining C-covered patches
and hooks. `--require-whole-owners` names the narrower historical check and
currently passes. Neither mode is a gameplay test. Original imported sources,
unknown ROM data and unrelated edits remain intact.

## Historical audit — 2026-10-15

The October 15 intake adds 18 accepted functions in 16 producer files. The
bundled pure-C library contains 890 functions from 847 accepted owners, with
836 source/header files. Existing imported source identities did not change.
Upstream acceptance is CURRENT `DA39A6C8`, Decomp commit `5c6ab254`.

**The source-preference check has not passed.** Bundling accepted source is
separate from selecting it during ROM export. Eleven complete-owner export paths
still assemble instructions where accepted C or matching hybrid C exists:

| Native feature/path | Original function | Accepted alternative | Current export |
|---|---|---|---|
| Class eligibility comparison | `000C37EC` | HYBRID_C | Generated instruction source from promotion owner/patch catalog |
| Custom encounter card text | `000E5968` | PURE_C | `encounters/custom-owners/card_text.s` |
| Custom encounter site/card message | `0010DDB4` | PURE_C | `encounters/custom-owners/card_message.s` |
| Custom encounter action budget | `00215CF0` | PURE_C | `encounters/custom-owners/action_budget.s` |
| Custom encounter menu iteration | `002159D0` | PURE_C | `encounters/custom-owners/menu_iterator.s` |
| Custom encounter battle round | `0021EBBC` | PURE_C | `encounters/custom-owners/round.s` |
| Encounter selection/rates | `0013C060` | PURE_C | `encounters/dispatcher.s` |
| Class and squad encounter rewards | `00101DDC` | PURE_C | `encounters/rewards.s` |
| Sleep recovery | `0022EDD4` | PURE_C | Status-rule modified owner emitted as `.word` instructions |
| Paralysis recovery | `0022EC08` | PURE_C | Status-rule modified owner emitted as `.word` instructions |
| Wake-on-hit/recovery setting | `0022A964` | PURE_C | Status-rule modified owner emitted as `.word` instructions |

The last row has an existing C weapon-status implementation too. Weapon-status
export compiles that adaptation; the independent recovery-setting path still
rewrites the authenticated loaded owner through assembly. The check therefore
must inspect export paths rather than just searching for a `.c` file.

These are migration gaps, not evidence of incorrect gameplay. A replacement
must retain authored encounter rates, custom squads, class relocation, status
settings and reopened-project compatibility. In particular DDB4 has a separate
52-entry switch table and four-byte original ASM suffix; the battle-round
producer has coupled switch-table placement. Copying the retail C alone would
not preserve the editor's changes. No executing owner was changed in this intake.

## Separate assembly cases

- `promotion/family-lookup.s` replaces the complete `000BBD50` function, for which
  pure C exists. Its editor-specific contract returns the extended family and
  behavior through two registers. This is explicitly reported as a custom ABI,
  not counted as a drop-in retail-C replacement or hidden as already converted.
- Shop reader instructions, Goth settlement arithmetic, body/equipment readers,
  Controller Pak adapters, XP scaling and capacity thunks patch parts of larger
  functions. Some enclosing functions have accepted C. Their assembly remains
  an adoption opportunity, but a bounded patch is not an alternative complete
  implementation of its enclosing function.
- Animation override, display, bootstrap and reserve-XP modules implement editor
  additions. Original matching C does not supply those added behaviors.
- `promotion/family-menu.s` and `shops/producer.s` are retained historical source
  files, not selected by the native exporter. Their active replacements use C.
- Unchanged game code is preserved from the user's ROM. The editor rebuilds
  affected regions; it does not currently compile the entire accepted library
  into every export. Retained ROM bytes are not an assembly-source selection.

## Reproduce the check

From the editor root:

```powershell
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp'
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --json
node native/tools/audit-source-selection.cjs --decomp '../OB64 Decomp' --require-clean
```

The last command intentionally returns exit 2 while gaps or unclassified assembly
files remain. It authenticates accepted PURE_C and HYBRID_C source dependencies,
joins native owner ranges to exact linked evidence, checks generated game/status
instruction paths, inventories all 69 bundled assembly files and lists native
assembly call sites. New assembly files are reported as unclassified. Missing,
changed or unsupported upstream acceptance returns an error, never a clean result.

This is a static audit of the named export catalogs and source paths, not an
emulator test or an automatic proof that a proposed C adaptation preserves game
semantics. Re-run after changing these selectors, their catalogs or Decomp intake.
