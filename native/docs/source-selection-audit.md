# Native export source audit

## Current audit — 2026-11-01

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
