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

The public editor is intentionally clean and browser-only:

- `index.html` loads the app shell and vendored dependencies.
- `app.js` owns the UI, tabs, editing flows, and export actions.
- `data.js` stores decoded constants, lookup tables, and save/ROM layout data.
- `parsers.js` reads the ROM, save states, BizHawk `.SaveRAM`, and decoded data
  structures into editor-friendly objects.
- `repack.js` serializes edits back into ROM/save formats, including LHA/LZSS
  repacking and N64 CRC repair.
- `patch.js` imports/exports portable Project JSON files for supported edits
  and still accepts older patch/Scenario-project JSON.
- `tools.js` detects, applies, and removes Tools-tab ROM features.
- `tools-data.js` is generated from the research workspace's Tools feature
  builds and holds the Tools-tab feature byte definitions. Do not hand-edit.
- `squadblob.js` builds the runtime squad-override hook/blob on export, with
  cache-invalidate hardening mirroring the game's own resource loader.
- `squads-data.js` is generated from the research workspace's runtime scenario
  atlas and holds runtime-key-to-edat rows. Do not hand-edit.
- `squads.js` renders the squad composition editor (embedded in the Scenario
  tab sidebar; the standalone Squads tab is retired).
- `scenario.js` renders the map-first Scenario tab: placement, routes,
  triggers, buried treasure, added squads, and the ESET export/relocation lane.
- `scenario-eset-codec.js` parses and rebuilds the per-mission ESET archives
  (validated round-trip against all 64 selected runtime-key payloads).
- `scenario-eset-data.js` and `scenario-map-calibration.js` are generated from
  the research workspace (mission data, donor census, per-key map
  registrations). Do not hand-edit.
- `resources/maps/vgmaps/` bundles the full-art scenario map PNGs used by the
  Scenario tab's calibrated map view.
- `style.css` contains the full parchment-themed interface.

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
> Supported exact source images:
> - Header rev 0: `Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64`
>   SHA-256: `6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12`
> - Header rev 1: `Ogre Battle 64 - Person of Lordly Caliber (USA) (Rev 1).z64`
>   SHA-256: `3BFBAF0AF968795102F6D136713665E347C22723B4CA75BD5494FDC97DF5919E`
>
> Japanese, European, debug, prototype, or otherwise modified ROMs are not
> supported and will likely produce wrong offsets, garbled data, or crash on
> export. Verify your file is a supported USA header revision 0 or 1 image before
> reporting bugs.

## Releases and Downloads

Packaged builds are published on GitHub Releases:

- Release index: [LordlyCaliber releases](https://github.com/rempred/LordlyCaliber/releases)
- First packaged download asset: [LordlyCaliber-v0.1.0.zip](https://github.com/rempred/LordlyCaliber/releases/download/v0.1.0/LordlyCaliber-v0.1.0.zip)

GitHub tracks download counts for uploaded release assets. Repository clones and
GitHub's automatically generated source-code archives are separate from the
project download asset.

## Features

- **Shops** — modify the inventory of all 35 in-game shops, within the empirical
  324-item / 24-per-shop budget.
  Shop cards are ordered by playthrough scene, show budget warnings, and use
  searchable item pickers.
- **Classes** — edit base stats, growth means, resistances, combat multipliers,
  promotion gates, and row-attack counts for all 164 classes (0x01–0xA4) using
  the authoritative GameShark mapping.
  Class cards expose equipment defaults, promotion requirements, unit type,
  movement type, corrected same-class unit size, base HP, HP growth fields, and
  bundled class portraits. Card View has a warning-gated raw-record mode for
  inspecting and editing terminator or sentinel story/NPC class slots.
  Story duplicate classes are labeled Special/Boss unless behavior is proven
  actually buggy.
- **Items** — change weapon/armor/spellbook stats, prices, and resistances for
  all 277 equipment entries.
  Item names and IDs use the game's 1-based item numbering.
- **Scenario** — a map-first per-mission editor covering all 64 runtime
  scenario keys. The 62 renderable mission keys use site-fitted full-art map
  registrations, while internal/no-image keys keep the schematic fallback.
  Enemy squads appear as draggable portrait markers; click a squad to edit its
  composition and formation in the sidebar (the former Squads tab, embedded);
  drag-draw movement routes with editable waypoints. The squad detail links to
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
  one-byte equipment overrides, alignment, element, experience, and army
  inventory (equipment + consumables + treasures).
  BizHawk/Project64 files expose all populated native in-game slots through a
  slot selector (Project64 `.sra` is the same SRAM word-swapped; exports
  round-trip byte-exactly back to `.sra`).
  Goth (war funds) and Chaos Frame are editable in every format, including
  battery saves.
- **Projects** — save supported edits (shops, item prices, item stats, class
  definitions, encounter pools/rates, creature drops, consumables, stat gates,
  the global encounter-roll multiplier, squad overrides, Scenario-tab edits,
  and Tools-tab feature toggles) to a portable JSON project file for sharing or
  reapplying to a fresh ROM.
  Squad project data stores per-runtime-key 35-byte replacement records so a
  saved project can reproduce the exported squad override blob.
  The Project JSON container embeds the full Scenario payload (modified mission
  ESETs, buried treasures, added squads, squad comp records, and site intents),
  so one file reproduces a complete scenario mod; older patch files and legacy
  Scenario-only project files still load.
  Save Game Editor changes are separate save-file edits; use that tab's Export
  Save control for them.
- **Export** — writes a clean ROM in the same byte order that was loaded, with
  the N64 CIC-6102 CRC re-calculated when needed. A no-edit export is
  byte-identical to the input. When an export changes the CRC (scenario
  relocation, squad overrides, some Tools features), Project64 keys a NEW save
  folder for the ROM — the UI surfaces the recovery recipe so existing saves
  don't silently "disappear".

## Current Limitations

- Only the North American retail header revision 0/1 ROMs listed above are supported.
  Other regions, prototypes, or unknown modified ROMs are rejected or unsupported.
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
- Stronghold editing, world-map editing, audio editing, and combat-buffer
  expansion are research targets, not shipped features.

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
  32-byte item stat table (295 records), 12-byte consumable master table
  (45 records), 20-byte neutral-encounter scenario slice, adjacent
  terrain-rate tables, and the 28-byte stronghold record decoded against
  in-game testing and emulator memory diffs.
- Custom LZSS compressor / decompressor for editing the dialogue scripts and
  the stat-gate region.
- N64 CIC-6102 CRC re-calculation to keep patched ROMs bootable.
- Per-class data cross-validated against the GameShark Class Hacking Guide
  and community wiki tables.
- The Scenario tab rests on live Project64 tracing of the game's mission
  loader: the per-mission ESET format (placement, routes, compound trigger
  gates), the runtime squad-builder hook, the archive fetch/DMA-window model
  behind the relocation lane, and the map projection used to register mission
  maps — every export lane was proven by cold-booting patched ROMs, not just
  by static byte checks.

Built with vanilla JavaScript — no framework, no build step. Single bundled
dependency: [fflate](https://github.com/101arrowz/fflate) for RetroArch RZIP
save-state decompression.

## Planned features

- **Promotion graph polish** — expose the already-editable stat gates and ROM
  promotion links as a fuller visual workflow.
- **Stronghold editor** — modify the 316 stronghold records (location, owner,
  shop assignment).
- **Town descriptor authoring** — support towns that do not already have a
  scincsv descriptor row.
- **Multi-window mission relocation** — extend the grow/relocate lane to the
  missions whose archives span multiple fetch windows, removing the remaining
  per-mission size caps.
- **Map-unit sprites for more leader classes** — investigate the game's
  special-leader sprite table so monster-led squads can appear on the world
  map instead of being blocked at export.
- **Class promotion-tree visualizer** — interactive graph of all promotion
  paths derived from class def `reqClass` (B55).
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
