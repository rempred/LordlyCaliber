# Native export source audit — 2026-10-15

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
