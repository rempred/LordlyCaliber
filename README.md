# LordlyCaliber

## Project Overview

LordlyCaliber is a reverse-engineering and modding tool for *Ogre Battle 64:
Person of Lordly Caliber*. The editor code, ROM parsers, repackers, save-file
codecs, and supporting research scripts were entirely written by AI assistants,
coordinated by rempred. AI assistants also helped with reverse-engineering work:
inspecting ROM data, interpreting runtime memory, building probes, and turning
verified findings into editor features.

rempred keeps the project grounded by choosing research targets, supplying test
cases, checking behavior in emulators, and deciding which findings are ready to
ship.

The public editor application is intentionally clean and browser-only.
Optional desktop utilities are kept separate from its runtime:

- `index.html` loads the app shell and vendored dependencies.
- `app.js` owns the UI, tabs, editing flows, and export actions.
- `data.js` stores decoded constants, lookup tables, and save/ROM layout data.
- `parsers.js` reads the ROM, save states, BizHawk `.SaveRAM`, and decoded data
  structures into editor-friendly objects.
- `repack.js` serializes edits back into ROM/save formats, including LHA/LZSS
  repacking and N64 CRC repair.
- `patch.js` imports/exports portable Project JSON files for supported edits
  and still accepts older patch/Scenario-project JSON.
- `changelog.js` turns that same canonical Project diff into a plain-English
  release summary and downloadable `.txt` changelog.
- `consumable-effects.js` owns the evidence-backed ten-model consumable-effect
  catalog, explicit Rev 0/Rev 1 feature manifests, deterministic healing-text
  codec, guard/transaction ledger, Project v13 payload, combined-diff
  provenance, and Consumables-tab renderer.
- `damage-calculator.js` provides the read-only Physical damage model and one
  evidence-bounded Magic product slice for native template 45 or 51 resolving
  to action 55, plus expected class-growth projection, native-action filtering,
  derived-variable overrides, and formula constants used by the Damage
  Calculator tab.
- `tools.js` detects, applies, and removes Tools-tab ROM features.
- `tools-data.js` is generated from the research workspace's Tools feature
  builds and holds the Tools-tab feature byte definitions. Do not hand-edit.
- `squadblob.js` builds the runtime squad-override hook/blob on export, with
  cache-invalidate hardening mirroring the game's own resource loader.
- `squads-data.js` is generated from the research workspace's runtime scenario
  atlas and holds runtime-key-to-edat rows. Do not hand-edit.
- `squads.js` renders the squad composition editor (embedded in the Scenario
  tab sidebar; the standalone Squads tab is retired).
- `scenario.js` renders the map-first Scenario tab: resource-scoped enemy base
  levels, scenario-local squad level copies, placement, routes, triggers,
  buried treasure, added squads, and the ESET export/relocation lane.
- `scenario-eset-codec.js` parses and rebuilds the per-mission ESET archives
  (validated round-trip against all 64 selected runtime-key payloads).
- `scenario-eset-data.js` and `scenario-map-calibration.js` are generated from
  the research workspace (mission data, donor census, per-key map
  registrations). Do not hand-edit.
- `resources/maps/vgmaps/` bundles the full-art scenario map PNGs used by the
  Scenario tab's calibrated map view.
- `style.css` contains the full parchment-themed interface.
- `supplemental-tools/` contains optional standalone release utilities. These
  tools are not loaded by `index.html` and do not add editor tabs.

Research-only scripts and emulator probes are kept outside this repository.
Only concrete, tested findings are ported into LordlyCaliber.

A browser-based mod editor for *Ogre Battle 64: Person of Lordly Caliber*
(Quest, N64, 1999). Edit shops, classes, items, neutral encounters, entire
mission scenarios — enemy squads, placements, routes, triggers — and save
files, then export a patched ROM.

No installation, no build step. Open `index.html` in any modern browser, drop
in your own copy of the US retail ROM, and start modding.

> **ROM compatibility:** the editor is built and tested against the North
> American (USA) retail header revision 0 dump:
> `Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64` (41,943,040 bytes,
> .v64 byte-swapped, GoodN64-verified, Game ID `NOBE`).
> It also supports the common USA header revision 1 dump in `.z64`, `.v64`, or
> `.n64` byte order for data editing/export. Header revision 1 supports the Chaos
> Frame Counter and Squads runtime override export; High Attack Streamsplit
> remains header revision 0-only until its changed header revision 1 code path is
> rebuilt.
>
> Verified reference images:
> - Header rev 0: `Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64`
>   SHA-256: `6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12`
> - Header rev 1: `Ogre Battle 64 - Person of Lordly Caliber (USA) (Rev 1).z64`
>   SHA-256: `3BFBAF0AF968795102F6D136713665E347C22723B4CA75BD5494FDC97DF5919E`
>
> Filenames are not compatibility checks. The loader recognizes `.z64`, `.v64`,
> and `.n64` from their contents, normalizes them internally, and preserves the
> input byte order on export. Japanese, European, debug, and prototype layouts
> remain unsupported. Modified US ROMs may load when their header and archives
> remain parseable; feature-specific checks disable only operations whose
> required code/data no longer matches and report the first relevant mismatch.

## Releases and Downloads

Packaged builds are published on GitHub Releases:

- Release index: [LordlyCaliber releases](https://github.com/rempred/LordlyCaliber/releases)
- First packaged download asset: [LordlyCaliber-v0.1.0.zip](https://github.com/rempred/LordlyCaliber/releases/download/v0.1.0/LordlyCaliber-v0.1.0.zip)

GitHub tracks download counts for uploaded release assets. Repository clones and
GitHub's automatically generated source-code archives are separate from the
project download asset.

## Supplemental Desktop Tools

`supplemental-tools/project64-battery-save-manager/` contains the standalone
Project64 Battery Save Manager. It scans arbitrary directory trees for Ogre
Battle 64 `.sra` battery saves and copies a selected save into the directory
expected for a chosen patched ROM. It does not handle savestates and is not
part of the browser editor. See the tool's README for launch requirements and
safety behavior.

## Features

- **Shops** — modify the equipment and consumable inventory of all 35 in-game
  shops through the shared Expansion-Pak runtime override. The editor enforces
  the statically safe 50-equipment / 15-consumable per-shop ceilings; 24 was
  only the largest retail equipment list, not a code limit. The corrected
  40+10 path is state-replay and cold-export/New Game/shop-load verified;
  exact-max and purchase/use tests remain.
  Shop cards are ordered by playthrough scene, show capacity warnings, and use
  searchable item pickers.
- **Consumables** — view consumable IDs 1–31 with the same parsed names and
  item icons used by Shops; quest/story IDs 32–44 are intentionally omitted.
  Fifteen rows are editable when their local normalized structures match:
  Heal Leaf, Heal Seed, Heal Pack, Power Fruit, Angel Fruit, Cup of Life, six
  linked stat boosters, Scroll of Discipline, Urn of Chaos, and Goblet of
  Destiny. Healing values support `0..999`; `1..99` are warned and displayed as
  `001..099`, while `0` is a warned consuming no-op. Their three numeric
  in-game descriptions are synchronized through a deterministic complete-slot
  rewrite. Heal Pack exposes one value backed by two required equal, atomic
  code words. Fruit values support `0..65535`; `0` is a warned consuming no-op
  and `256..65535` are warned as redundant with the ordinary u8 result at 255.
  IDs 11–16 remain six synchronized views of one shared range and one encoded
  word pair. The other 16 visible rows remain present with native-disabled
  controls and evidence-accurate reasons.
  Equivalent filenames and `.z64`/`.v64`/`.n64` byte orders are accepted.
  Rev 0 and Rev 1 use explicit feature manifests: a local healing-description,
  fruit-word, or range-profile mismatch locks only its dependent rows and
  shows the exact diagnostic; Project data cannot bypass those checks. See the
  [consumable-effects verification guide](scratch/CONSUMABLE_EFFECTS_TESTING.md)
  for the prepared Joe runtime matrix. Runtime acceptance of IDs 1–5 remains
  pending that execution.
- **Classes** — edit base stats, per-level base gains, resistances, class combat coefficients,
  promotion gates, and row-attack counts for all 164 classes (0x01–0xA4) using
  the authoritative GameShark mapping.
  Class cards expose equipment defaults, the B53-B57 level-progression chain,
  promotion stat gates, unit type,
  movement type, corrected same-class unit size, base HP, HP growth fields, and
  bundled class portraits. Card View has a warning-gated raw-record mode for
  inspecting and editing terminator or sentinel story/NPC class slots. Card
  View and the Raw Bytes table subview expose every logical record byte;
  uncertain and runtime-pointer fields are shaded with warnings.
  Story duplicate classes are labeled Special/Boss unless behavior is proven
  actually buggy. Row-action pickers retain the raw action byte while replacing
  the nine ambiguous `[Elemental Magic]` placeholders with verified Tier 1,
  Tier 2, Tier 3, summon/high-level, fixed-spell, and elemental Blast template
  roles.
- **Damage Calculator** — compare a native action against a selected defender.
  Physical remains the initial/fallback mode with its existing controls,
  normal/critical results and hit comparison. Both modes now show explicit
  total-adjustment equations, current component arithmetic, and lookup/rule
  derivations instead of presenting the adjustments as an unexplained list.
  The enabled **Magic** mode is deliberately narrower: it keeps the class-native
  template ID separate from the resolved spell ID and displays an amount only
  for native T1 template 45 resolving to Wind/Lightning 55 or native fixed
  Lightning template 51 resolving to 55. Effective action behavior must retain
  D0/action family `3` and D1/Wind `1`; differing overrides fail closed, while
  D2-D7 remain fixed by the exact accepted action-55 record rather than exposed
  as calculator overrides. Flame, Earth, Water, Virtue, Bane, Drakonite,
  Random/None, non-native, and every non-55 resolution are unavailable. Its
  evidence panel labels the exact
  action-55 ordinary single-caster selector-0/pattern-1 full-power primary-target
  runtime anchor separately from supported-static inputs and resolution. Magic
  exposes a selected random adjustment (default 0; the accepted fixture uses
  +1), an explicitly supported-static endpoint range, and one nonlethal
  Hit-Points result. It does not display Magic hit/success, critical/doubling,
  satellite, combined-caster, healing/status/special, or lethal-branch numbers.
  Every attacker/defender Growth Gear and Current Gear input must be an own
  combatant property containing an exact four-slot array whose indices `0..3`
  are own properties. Each positive ID must be a finite integer inside the
  parsed table and resolve through an own, non-null item record; explicit own
  `[0, 0, 0, 0]` remains valid. Missing, inherited, sparse, malformed, negative,
  fractional, nonnumeric, nonfinite, or unknown-item data renders no supported
  Magic amount, range, score, or Hit-Points result and cannot be materialized or
  cleaned into eligibility. Product derivation and gear-card presentation now
  keep the permanently tested `Symbol` ID and throwing-conversion object inert:
  their raw slot remains invalid, the card uses the fixed `(invalid item ID)`
  label without invoking conversion hooks, and render reaches the unavailable
  result while clearing stale amount/range/Hit-Points cards. Valid finite-integer
  item labels and ordinary numeric unknown/out-of-bound labels retain their
  existing presentation.
  The retail default Physical-to-Magic transition now evaluates each candidate
  against the complete normalized attacker/defender state, installs the first
  eligible route, and rechecks the exact installed state through the same
  policy. Retail deterministically selects class 15, native fixed template 51,
  rear row, resolved action 55, and default gear `[74, 167, 135, 235]`; it does
  not invent a Generic Spellbook selector.
  The browser global deliberately exposes only `render`; unrestricted formula
  helpers remain available solely through CommonJS for Node tests.
  An open-by-default, mode-aware variable guide explains the source, formula,
  and effect of the calculator inputs, including independent terrain/movement
  lookups and exact anti-dragon requirements. Every applicable selector, input,
  derived variable, constant, and result also has a keyboard-accessible styled
  `?` popup with explicit What, Source, Use, and Rule sections. Calculator
  changes never enter the Project diff, Changelog, or ROM export. The own-data
  re-review returned `Revision required` because rejected hostile IDs still
  crashed gear-label rendering; permanent direct metadata, real CommonJS
  render, and browser-like Node `vm` render regressions now cover both hostile
  fixtures through the standard-library fake DOM, including stale-output
  clearing and zero conversion hooks.
  A later fresh critical complete-delta review preserved that behavior,
  calculator arithmetic, and every accepted boundary, while returning
  `Revision required` for three guidance defects. The bounded wording
  correction states that Alignment comes directly from each resolved
  selected class rather than level/Growth-Gear projection, keeps
  attacker-Alignment out of accuracy, gives Physical and Magic separate
  Luck/Character-stats/current-HP help, and describes STR as unused by the
  enabled bounded action-55 Magic calculation without calling Magic dormant.
  The reviewer's corrected 32-case fake-DOM/event/formula-edge harness remains
  intact and a 33rd rendered semantic case covers CommonJS and browser-like
  Node `vm` mode switches. The fresh re-review passed all 13 assigned guidance
  instances but found one separate Physical Attack Score popup using the false
  Dexterity factor `(+50)/50`. At Joe's explicit direction, the Director
  corrected that one rule to the already-canonical `(+100)/100` value and
  added a 34th rendered CommonJS/browser-like Node `vm` regression, including
  registered return to Physical and the direct CommonJS API. All local gates
  pass. See the
  [review AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-action55-own-data-transition-closure-correction-independent-review.md)
  [hostile-ID correction AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-action55-hostile-id-render-closure-correction-aar.md),
  [variable-guide/tooltip/formula-reference AAR](../wiki/after-action-reports/20260723-damage-calculator-physical-adjustment-formula-reference-aar.md),
  [complete-delta review AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-hostile-id-ui-reference-complete-delta-independent-review.md),
  [mode-accurate guidance correction AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-guidance-mode-accuracy-correction-aar.md),
  [guidance re-review AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-guidance-mode-accuracy-correction-independent-review.md),
  and [popup correction AAR](../wiki/after-action-reports/20260723-b52-magic-calculator-physical-attack-dex-popup-correction-aar.md).
  Status: **accepted for the tested non-browser product scope by Joe's explicit
  Director-correction disposition; Joe's final browser smoke remains pending**.
- **Items** — change weapon/armor/spellbook stats, prices, resistances, and the
  packed B20-B21 permanent level-up additions for all 277 equipment entries.
  Every logical byte in the 32-byte item record is editable; unknown/tail bytes
  and the runtime name pointer are shaded and carry caution tooltips. The table
  covers all 278 logical records (ID 0 sentinel plus IDs 0x01-0x115), and every
  byte-backed descriptive choice retains its raw hex value beside the label.
  Item names and IDs use the game's 1-based item numbering.
- **Scenario** — a map-first per-mission editor covering all 64 runtime
  scenario keys. The 62 renderable mission keys use site-fitted full-art map
  registrations, while internal/no-image keys keep the schematic fallback.
  Enemy squads appear as draggable portrait markers; click a squad to edit its
  composition and formation in the sidebar (the former Squads tab, embedded).
  The first class, member, or formation change automatically creates a
  scenario-local copy for that deployment; there is no separate override
  checkbox. The shared stock EDAT remains unchanged. You can also drag-draw
  movement routes with editable waypoints. The squad detail links to
  its shared route nodes, and clicking a linked node opens the node editor for
  exact movement, gate, and waypoint-target edits. Squad-level Behavior
  templates are limited to row-local guard/sally presets so shared node edits do
  not look like one-EDAT-only changes.
  Buried treasure can be added, removed, or moved on the map using the Shops
  item icons. **Add Squad** places entirely new enemy squads on a mission using
  verified donor records plus the scenario-gated runtime override lane, and
  exports end-to-end (cold-boot proven in Project64). Squad comps export as
  runtime overrides without changing global `enemydat.bin`; the default UI
  enforces vanilla-style formation limits, with the experimental raw-capacity
  mode still available for mod testing.
  **Enemy base level** edits decompressed ESET byte `0x08` for the selected
  physical resource, shows the resource path, alias keys, original value, and
  the safe range that avoids byte wrap and the game's `1..99` clamp. The leader
  has its own adjustment; every Group B unit shares one setting, and every
  Group C unit shares one setting. Empty group controls remain visible and
  enable as soon as that group gains a member. The first EDAT-record edit to a
  stock deployment—composition, formation, group level, or starting
  equipment—copies
  its complete 35-byte record into the verified custom-squad allocator and
  repoints only that row. The shared stock EDAT and other Scenarios remain
  unchanged. Added squads own
  their records and begin with all three offsets at zero. Revert restores a
  stock row's original reference and removes its level-copy allocation. Key 25
  blocks only its accepted ambiguous B/C selectors, and internal key 54 remains
  fail-closed.
  The Scenario squad sidebar exposes **Starting equipment changes** for the
  leader and follower groups B/C. Each of the two item-choice buttons opens the
  editor's searchable shop-style modal with item icons. It offers **No change**
  or a known equipment ID through `0x0115`, displayed with name, category/type,
  and full four-digit ID. The UI says which normal class slot
  each choice replaces, explains when a choice cannot be used, and warns that
  Choice 2 wins when both choices replace the same slot. Every member of the
  group receives the change; these are not individual inventory slots. Stock
  edits reuse the same complete Scenario-local custom record as
  level/class/formation edits;
  added squads edit their owned record. Project v4 and the existing export lane
  preserve the full u16 values. Existing source IDs beyond the proven range are
  shown and preserved read-only. This feature proves initialization only; it
  makes no claim about later AI choice, use, consumption, effects, or drops.
  The same sidebar also has **Starting consumable supplies**. Choose a healing,
  fruit, or revival supply preset from the searchable parchment modal; every
  choice shows its item icons, names, and duplicate counts. These three bytes
  are category presets expanded by the scenario initializer, not three direct
  carried-item slots. **Clear supplies** writes the canonical `00 00 00`, while
  **Revert supplies** restores the row's three original bytes without undoing
  composition, levels, starting equipment, normal drops, placement, or other
  Scenario work. Existing unknown bytes (including fruit preset `04`, the
  alternate empty encoding) are shown and preserved until a known preset is
  chosen; preserve-only values are never offered as new choices.
  Supply presets belong to the physical ESET deployment row. Runtime keys that
  select that same physical ESET update together, while an equal EDAT reference
  in another ESET remains isolated; no global or custom `enemydat` record is
  allocated merely for supplies. The Initialization preview uses the current
  five-member composition and the exact class-B59 sum. The engine does not
  clamp that sum before writing its ten-byte destination, so preset mutation
  and export fail closed unless capacity is `1..10`; compositions with more
  than five occupied anchors are also blocked instead of receiving an invented
  preview. Complete modified ESET `rawHex` persists through canonical Scenario
  Project v4, so direct and Project-reloaded exports use the same bytes.
  Fixed-slot ESET exports keep each level-2 member's declared length at the
  original next-member boundary. This preserves the game's sequential archive
  walk. The preview describes only the sorted scenario-start mix after capacity
  is applied; it does not claim which item the AI later chooses, uses, or consumes.
  Clicking a town exposes its scenario-specific starting Allegiance plus its
  global Population and Morale. Population/Morale rebuild the shared 316-record
  `ktenmain` table; Morale preserves B24 bit 7 and is locked on exact `0xFF`
  objective records. The detail panel warns when another runtime key references
  the same global record.
  Oversized mission edits take an automatic **grow/relocate lane**: when a
  rebuilt mission archive no longer fits its original slot, export copies it
  to free ROM-tail space behind a small DMA redirect (currently supported for
  single-fetch-window missions, about half of them; the UI reports precise
  fit/relocation status per mission). A safety gate blocks exports that would
  hang the game: squad leaders must use a class with a map-unit sprite (85 of
  165 classes — monsters, undead, and most special classes — have none, and
  the game crashes during mission LOADING if one leads a deployed squad).
  Scenario work is saved through the top-level Project JSON flow, so one file
  can carry Scenario edits together with shop, class, item, encounter, and
  Tools changes.
- **Encounters** — adjust the neutral-encounter creature pool across all 40
  scenario slices, tune per-terrain encounter thresholds, and set the global
  encounter-roll pass rate with a vanilla-relative multiplier slider (`x1`
  vanilla, `x3` normal cap, optional `x100` test cap).
  Creature drop entries are editable from the same tab.
- **Tools** — toggleable ROM fixes and quality-of-life features applied on
  export and removable again (the original bytes are restored). Features
  already present in a loaded ROM are detected; unrecognized bytes at a
  feature's addresses disable its toggle instead of overwriting another mod,
  and declared ROM/RAM patch-region overlaps are rejected on export.
  **Chaos Frame Counter** shows the hidden Chaos Frame stat
  on the Army Management screen as a native parchment plate titled CHAOS
  FRAME, in line with the SOLDIER/CHARACTER/UNIT labels. The current payload
  uses a standalone ROM-tail/free-RAM module and gates on the Army graphics
  task buffers plus War Funds/header fingerprints, so it survives returning
  from Class Change without relying on volatile menu-state bytes. Experimental
  **High Attack Streamsplit** installs the high-attack battle-stream fix on a
  separate ROM/RAM lane, including interrupt-safe end-of-stream handling,
  first-menu separator handling, and relocated battle-menu state stores;
  attack-count bytes are still edited from the Classes tab, and fresh
  cold-boot regression is required before treating it as release-ready. High
  Attack Streamsplit is currently enabled only on header revision 0.
- **Save Game Editor** — load RetroArch `.state` saves (RZIP-compressed or raw),
  BizHawk in-game `.SaveRAM` battery saves, Project64 `.sra` cartridge saves, or
  8 MB RDRAM `.bin` dumps. Edit character names, classes, levels, HP, stats,
  one-byte equipment overrides, alignment, luck, element, experience, and army
  inventory (equipment + consumables + treasures).
  BizHawk/Project64 files expose all populated native in-game slots through a
  slot selector (Project64 `.sra` is the same SRAM word-swapped; exports
  round-trip byte-exactly back to `.sra`).
  Goth (war funds) and Chaos Frame are editable in every format, including
  battery saves.
- **Projects** — save supported edits (shops, item prices, item stats, class
  definitions, encounter pools/rates, creature drops, consumables, stat gates,
  the global encounter-roll multiplier, squad overrides, Scenario-tab edits,
  consumable-effect magnitudes/ranges, and Tools-tab feature toggles) to a
  portable JSON project file for sharing or reapplying to a ROM. Project format
  v16 adds explicit Scenario enemy-base intents and scenario-local squad-copy
  provenance while retaining older formats. v15 stores independent
  Normal/Blocked combat-animation selectors; v14 projects migrate their single
  selector into both lanes. v13 adds absolute `magnitude`
  entries for IDs 1–5 and preserves the v12 range
  entries, including one `11-16` key for all six linked stat items. v12 accepts
  only the older range keys; v11 and older retain their prior behavior.
  Effect collection compares against the compatible values imported from the
  loaded ROM, so reopening an already-edited ROM is clean and resetting a
  custom import to retail remains an explicit Project request.
  Squad project data stores per-runtime-key 35-byte replacement records so a
  saved project can reproduce the exported squad override blob.
  The Project JSON container embeds the full Scenario Project v4 payload
  (explicit `levelBaseEdits`, validated scenario-local `squadLevelCopies`,
  modified mission ESETs, buried treasures, added squads, squad comp records,
  site allegiance intents, and global stronghold Population/Morale intents),
  so one file reproduces a complete scenario mod; older patch files and legacy
  Scenario-only project files still load.
  Save Game Editor changes are separate save-file edits; use that tab's Export
  Save control for them.
- **Changelog** — previews every currently recorded ROM-project change in
  readable categories with before/after values where the baseline is known.
  The tab downloads the same report as a plain-text file suitable for release
  notes. It is derived from the Project JSON diff, so there is no second list
  for users to maintain manually. Save-game edits remain separate.
- **Export** — writes a clean ROM in the same byte order that was loaded, with
  the N64 CIC-6102 CRC re-calculated when needed. A no-edit export is
  byte-identical to the input. When an export changes the CRC (scenario
  relocation, squad overrides, some Tools features), Project64 keys a NEW save
  folder for the ROM — the UI surfaces the recovery recipe so existing saves
  don't silently "disappear".
  Exports containing consumable-effect edits preflight every requested local
  facet and exact preimage before writing an isolated candidate. Sixteen
  four-byte code regions have one exact owner; a pending healing change adds a
  distinct exact owner for the complete selected-revision description slot.
  Broad code/text profiles remain guards and collision surfaces, never delta
  owners. The combined transaction then recalculates CIC-6102 once after all
  writes, independently verifies it, restores the loaded byte order, computes
  the candidate hash and complete concrete-owner ledger in memory, and
  downloads exactly one ROM.
  Every finished candidate now passes an automatic export validation gate
  before the normal ROM download starts. A themed progress dialog shows the
  active build or validation stage and its completion percentage. The gate
  checks the ROM format, revision,
  byte order, CIC-6102 checksum, deterministic serialization, archive catalog,
  authorized changed-byte ownership, and patch integrity. Edited values are
  parsed from the finished ROM and compared with the requested model.
  Rebuilt scenario archives also verify their level-2 header checksum, data
  checksum, assigned boundary, and exact extracted payload. ESET payloads pass
  the scenario structure validator again after extraction.
  A failure opens the themed error dialog with plain-language problems and
  stops the automatic ROM download. **Download Error Report** saves the full
  machine-readable JSON report with stable error codes, hashes, changed ranges,
  and technical details. **Download Anyway** saves the failed candidate but
  does not adopt it or clear the current editor changes.
  Successful ROM exports do not create a JSON sidecar.
  **Save Project** is the sole user-facing JSON export and records every
  supported current ROM-edit family, including consumable effects.

## Current Limitations

- Only North American retail header revision 0/1 layouts are supported.
  Other regions and prototypes are rejected. Modified US images are
  experimenter inputs: compatible features remain available, while operations
  with a known structural mismatch are disabled or rejected with a diagnostic.
- Healing/Fatigue consumable controls have accepted static ownership, codec,
  and synthetic product verification, but their prepared cold-boot and
  gameplay matrix has not yet been executed by Joe. Do not treat this as
  runtime acceptance.
- Consumable-effect editing is narrower than general editor compatibility: it
  requires the 41,943,040-byte US layout plus matching normalized effect-code,
  dispatch, target-metadata, and current-word guards. Filename, raw SHA-256,
  header CRC, and `.z64`/`.v64`/`.n64` packaging do not determine eligibility.
- The editor creates new ROM/save files in your browser downloads. It does not
  overwrite your original files or patch a running emulator directly.
- Shop exports must fit the original compressed archive slot. The UI warns about
  known budgets, but very large inventory changes can still fail export.
- Squad comps use conservative vanilla validation by default: up to 5
  formation slots, where regular units cost 1 slot and large units cost 2
  slots. The experimental raw-capacity mode can encode all seven vanilla
  template anchors (`Leader + Bx3 + Cx3`) and ignores large-unit spacing for
  mod testing, but over-cap squads may not be supported by the game's
  organization, map inspection, or battle-placement paths. A key 2 / EDAT 13
  seven-unit test applied correctly but hid units in map inspection and placed
  units off-grid in battle; the misplaced units also could not be attacked.
  More than 2 follower class groups is not exported yet; supporting it requires
  a larger runtime record/resolver design.
- Scenario squad leaders must use a class with a map-unit sprite; export blocks
  the rest (a game engine limit, not an editor choice — a spriteless leader
  hangs mission LOADING in a runaway DMA). Members are unrestricted.
- Mission archive relocation currently supports single-fetch-window missions
  (~32 of 63); multi-window missions still enforce the original slot-size cap.
  Per-mission add-squad budget is also capped by the game's 50 deploy slots.
- Neutral/allied town-allegiance edits export for towns with existing scincsv
  descriptors. Towns with no descriptor row still cannot be authored until the
  editor can add new descriptor rows safely.
- Population/Morale edits must keep global `ktenmain` archive #691 within its
  original ROM slot. The dual greedy/lazy LH5 encoder provides normal edit
  headroom, but sufficiently numerous high-entropy changes can still block
  export with an exact overfill count.
- Full-art mission map backgrounds are bundled and site-fitted for the 62
  renderable runtime keys. Two internal/no-image keys still render through the
  schematic fallback.
- Class sex/voice/body and leadership bytes are exposed from the corrected
  name-framed header, but their exact runtime consumers are not fully traced.
- Raw story/NPC class records can be viewed and edited in Classes Card View, but
  their sentinel values are not proven combat-safe.
- BizHawk `.SaveRAM` and Project64 `.sra` support roster, inventory, Goth, and
  Chaos Frame editing across valid native slots. Calendar/scenario fields are
  hidden for battery saves (only partially persisted in the packed format).
- Adding entirely new reserve characters is not enabled yet. The game has an
  additional active/reserve validation structure that is still being decoded.
- Remaining stronghold fields, world-map editing, audio editing, and
  combat-buffer expansion are research targets, not shipped features.

## Usage

1. Serve the folder locally — any static server works:
   ```bash
   npx serve .
   ```
   Or open `index.html` directly in a browser (most features work, but file
   downloads need a real `http://` origin in some browsers).
2. Click **Load ROM** and select your legally-obtained supported US retail ROM.
3. Use the tabs to make edits — pending changes show in the status bar.
4. **Export ROM** writes a fresh ROM in the loaded byte order to your downloads.
5. (Optional) **Save Project** writes your edits as JSON. **Load Project** re-applies
   them to a clean ROM.

For save editing: switch to the **Save Game Editor** tab and **Load Save**.
RetroArch `.state` files (Mupen64Plus-Next core) work out of the box. Project64
cartridge saves live at `Project64/Save/OgreBattle64-<hash>/OgreBattle64.sra`
(each ROM build gets its own hash folder); edited exports drop back in as the
same file name.

> **You must supply your own ROM.** No ROM or game code is bundled. Small
> extracted UI/item icons are included only as identification references for the
> editor.

## Emulator Settings For Override Patches

Runtime override patches need the N64 Expansion Pak / 8 MB RDRAM. This applies
to exported ROMs that include Squads runtime overrides, Scenario **Add Squad**
composition overrides, Chaos Frame Counter, or High Attack Streamsplit. These
features install code/data in the free upper-RDRAM lanes at `0x80400000+`; a
strict 4 MB setup can hang, black-screen, or fault when the patched ROM tries to
load the module.

Most override-patched ROMs do **not** require interpreter core just for
gameplay. Current High Attack Streamsplit is the exception: v21 passed its
first-menu cold boot under Project64 Interpreter, while the default recompiler
hard-locked at battle load when the live combat-overlay rewrite installed.
The attempted v22 cache-maintenance workaround is rejected: it reached the
menu under the recompiler but then generated an endless stream and overwrote
RDRAM. The editor therefore exports v21 and requires Interpreter for High
Attack Streamsplit. Interpreter is also required for debugger/watchpoint
tracing.

### Project64 / PJ64

Project64 is the recommended emulator for testing exported ROMs and for using
the editor's Project64 `.sra` save support.

- Use Project64 4.x or a recent Project64 development build with the GLideN64
  video plugin.
- Set the per-game profile to **8 MB RDRAM / Expansion Pak**. Re-check this
  after exporting a ROM with a changed CRC, because Project64 may create a new
  per-ROM profile entry.
- If editing Project64 config files manually, the exact key varies by build;
  the required result is 8 MB RDRAM. Common forms include `RDRAM Size=8`,
  `RDRamSize=8388608`, or `Game_RDRamSize=0x800000`.
- For **High Attack Streamsplit**, select **Interpreter** under the per-game CPU
  core settings. Project64's default recompiler is unsupported for this tool.
  Other override features can use the normal/default recompiler.
  Debugger/watchpoint work also requires Interpreter.
- If Project64 runs OB64 at about 15 fps, disable **Sync using Audio** in
  Project64's settings. OB64 should run at about 30 fps in-game.
- Cold-boot the exported ROM before judging runtime patches. Loading an old
  savestate can restore old RAM and hide or overwrite the module that the new
  ROM would normally load.
- Project64 creates a separate save folder for every different ROM hash. After
  exporting a patched ROM, expect a new `OgreBattle64-<hash>` save folder and
  move or re-export the `.sra` save you want to use into that folder.

### RetroArch

[RetroArch](https://www.retroarch.com/) is supported mainly through the
Mupen64Plus-Next core's `.state` files in the Save Game Editor.

- Use the **Mupen64Plus-Next** N64 core for save-state files you plan to load in
  LordlyCaliber.
- Make sure Expansion Pak / extra memory is enabled for the core. If the core
  exposes an RDRAM-size option, set it to **8 MB**. In Mupen64Plus-Next `.opt`
  files, the important value is `mupen64plus-ForceDisableExtraMem = "False"`.
- No RetroArch-core failure equivalent to Project64's High Attack recompiler
  hard lock has been established. Treat High Attack dynamic-recompiler support
  as unverified and cold-boot test it before relying on that configuration;
  the other override patches have no current interpreter requirement.
- Keep the same core and core version for a save-state workflow. RetroArch
  states are not a portable save format across unrelated cores or emulator
  versions.
- Prefer in-game saves when validating a newly exported ROM. Like Project64,
  RetroArch savestates can carry old RAM forward and mask whether the patched
  ROM cold-boots correctly.
- Legacy research scripts that talk to RetroArch expect **Network Commands**
  enabled on UDP port `55355`. Normal editor use does not require this.

## How it was built

The editor is the working surface of an extended reverse-engineering effort
on the US retail ROM:

- All 825 LHA archives in the data section catalogued and round-trip-decoded.
- 56-byte character struct, 72-byte class definition table (166 records),
  32-byte item stat table (278 logical records), 12-byte consumable master table
  (45 records), 20-byte neutral-encounter scenario slice, adjacent
  terrain-rate tables, and the 28-byte stronghold record decoded against
  in-game testing and emulator memory diffs.
- Custom LZSS compressor / decompressor for editing dialogue scripts and the
  stat-gate region, plus a pinned deterministic full-slot plan for the three
  synchronized healing-description numbers.
- N64 CIC-6102 CRC re-calculation to keep patched ROMs bootable.
- Per-class data cross-validated against the GameShark Class Hacking Guide
  and community wiki tables.
- The Scenario tab rests on live Project64 tracing of the game's mission
  loader: the per-mission ESET format (placement, routes, compound trigger
  gates), the runtime squad-builder hook, the archive fetch/DMA-window model
  behind the relocation lane, and the map projection used to register mission
  maps. The Population/Morale lane is byte-exact through serialize, LH5
  recompress, in-slot splice, re-extract, and parse; a cold-boot gameplay check
  of newly edited values remains pending.

Built with vanilla JavaScript — no framework, no build step. Single bundled
dependency: [fflate](https://github.com/101arrowz/fflate) for RetroArch RZIP
save-state decompression.

### Combat Attack Animation selector overrides

Classes > Combat now exposes one shared **Attack Animations** editor in both
table and card views. It maps an ordinary class and accepted action ID to two
independent class-compatible body selectors: **Normal (modes 0/1)** and
**Blocked (mode 2)**. The game continues to choose the raw mode; the editor
never chooses or stores one. Multiple actions may share either or both
selectors, no-longer-assigned action mappings remain available, and each
class/action row consumes one record in the exact global `N / 128` OBSO v2
table capacity.

Changing a live Front, Middle, or Rear attack now uses one shared operation in
both Classes presentations. It captures the pre-change rear action, preserves
any explicit mapping for the newly selected action, and otherwise inserts the
rear action's resolved vanilla Normal/Blocked pair. Rear changes use the
previous rear action; `None` creates no row; a full table rejects the class-field
and mapping change together. The modal derives `currently assigned` rank names
from live B43/B45/B47 fields and labels generated action/mode data as vanilla
reference context. Its mapping table also shows each untouched live vanilla action
with the generated Normal/Blocked pair and a `vanilla (no override record)` label;
these display-only rows do not consume global capacity until their selector is
changed. **Attack Animations** uses the parchment/gold beveled button
treatment shared visually with Scenario controls.

This feature is **US Rev 0 only** and requires **8 MiB RDRAM / Expansion Pak**.
Structural selector availability is not visual compatibility. The seeded
Normal/Blocked runtime logic is accepted, but Joe must cold-boot test every
exported ROM and judge its visible animation compatibility. Outcome/reaction
and hit/spell effects remain separate. Exact ordinary OBSO v1 installs migrate
without becoming dirty and upgrade to v2 on the first actual edit/export;
advanced or foreign/partial selector lanes remain preserved read-only. Rev 1
leaves normal editor features available while this control is disabled.

## Planned features

- **Promotion graph polish** — expose the already-editable stat gates and ROM
  promotion links as a fuller visual workflow.
- **Stronghold editor expansion** — expose the remaining global record fields
  beyond the shipped Population/Morale controls, including names, capabilities,
  types, and shop assignments.
- **Town descriptor authoring** — support towns that do not already have a
  scincsv descriptor row.
- **Multi-window mission relocation** — extend the grow/relocate lane to the
  missions whose archives span multiple fetch windows, removing the remaining
  per-mission size caps.
- **Map-unit sprites for more leader classes** — investigate the game's
  special-leader sprite table so monster-led squads can appear on the world
  map instead of being blocked at export.
- **Class promotion-tree visualizer** — interactive graph of all promotion
  paths derived from the B53-B56 base/intermediate/final progression chain.
- **High-attack combat stability** — continue ROM-side regression for extreme
  attack-count mods. The old 28-entry/result-log theory is retracted; current
  research points at combat action-stream/context relocation and scheduler
  cleanup guards. The experimental Streamsplit toggle exists in Tools, but it
  remains a research/testing feature until fresh emulator regression is done.
- **Bulk patches** — apply common community patches (XP rate, encounter rate,
  rare-item drops) as one-click toggles.

## Credits

See [CREDITS.md](CREDITS.md). LordlyCaliber was built with help from AI
assistants and coordinated by **rempred**. The editor would not exist without
the community wikis (OgreBattle64.net, ogrebattle64archive.com), Cralex's
GameShark guide, and the reverse-engineering work of everyone who came before.

## License

MIT — see [LICENSE](LICENSE). Bundled fflate library is also MIT — see
[vendor/LICENSE-fflate](vendor/LICENSE-fflate).

## Disclaimer

LordlyCaliber is an unofficial fan tool. *Ogre Battle 64: Person of Lordly
Caliber* is © 1999 Quest Corporation, published in North America by Atlus and
on N64 by Nintendo. Item icons in `resources/` are extracted from the original
game for identification purposes only. Scenario map art in
`resources/maps/vgmaps/` is from VGMaps.com and used with permission; see
[CREDITS.md](CREDITS.md). This project is not affiliated with or endorsed by
any rights holder.
