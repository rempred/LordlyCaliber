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
- `patch.js` imports/exports portable JSON patches for supported edits.
- `style.css` contains the full parchment-themed interface.

Research-only scripts and emulator probes are kept outside this repository.
Only concrete, tested findings are ported into LordlyCaliber.

A browser-based mod editor for *Ogre Battle 64: Person of Lordly Caliber*
(Quest, N64, 1999). Edit shops, classes, items, neutral encounters, and save
files — then export a patched ROM.

No installation, no build step. Open `index.html` in any modern browser, drop
in your own copy of the US retail `.v64`, and start modding.

> **ROM compatibility:** the editor is built and tested against the North
> American (USA) retail dump:
> `Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64` (41,943,040 bytes,
> .v64 byte-swapped, GoodN64-verified, Game ID `NOBE`).
> Japanese, European, debug, prototype, or otherwise modified ROMs are not
> supported and will likely produce wrong offsets, garbled data, or crash on
> export. Verify your file matches the name and size above before reporting bugs.

## Features

- **Shops** — modify the inventory of all 35 in-game shops, within the empirical
  324-item / 24-per-shop budget.
  Shop cards are ordered by playthrough scene, show budget warnings, and use
  searchable item pickers.
- **Classes** — edit base stats, growth curves, resistances, combat multipliers,
  promotion gates, and row-attack counts for all 164 classes (0x01–0xA4) using
  the authoritative GameShark mapping.
  Class cards expose equipment defaults, promotion requirements, unit type,
  movement type, and combat behavior fields.
- **Items** — change weapon/armor/spellbook stats, prices, and resistances for
  all 277 equipment entries.
  Item names and IDs use the game's 1-based item numbering.
- **Encounters** — adjust the neutral-encounter creature pool across all 40
  scenario slices, tune per-terrain encounter thresholds, and set the global
  encounter-roll pass rate with a vanilla-relative multiplier slider (`x1`
  vanilla, `x3` normal cap, optional `x100` test cap).
  Creature drop entries are editable from the same tab.
- **Save Game Editor** — load RetroArch `.state` saves (RZIP-compressed or raw),
  BizHawk in-game `.SaveRAM` battery saves, or 8 MB RDRAM `.bin` dumps. Edit
  character names, classes, levels, stats, equipment overrides, alignment,
  element, experience, and army inventory (equipment + consumables + treasures).
  BizHawk files expose all valid native in-game slots through a slot selector.
  Goth editing is available for live-state/RDRAM formats; native `.SaveRAM`
  funds are hidden until that packed field is mapped.
- **Patches** — save supported edits (shops, item prices, and the global
  encounter-roll multiplier) to a portable JSON patch file for sharing or
  reapplying to a fresh ROM.
- **Export** — writes a clean `.v64` with the N64 CIC-6102 CRC re-calculated.

## Current Limitations

- Only the North American retail `.v64` listed above is supported. Other regions,
  prototypes, `.z64` files, or already-modified ROMs may parse incorrectly.
- The editor creates new ROM/save files in your browser downloads. It does not
  overwrite your original files or patch a running emulator directly.
- Shop exports must fit the original compressed archive slot. The UI warns about
  known budgets, but very large inventory changes can still fail export.
- BizHawk `.SaveRAM` supports roster and inventory editing across valid native
  slots. Native `.SaveRAM` Goth/funds and some game-state fields are still
  hidden until their packed-save locations are mapped.
- Adding entirely new reserve characters is not enabled yet. The game has an
  additional active/reserve validation structure that is still being decoded.
- Per-mission deployment editing, stronghold editing, map editing, audio editing,
  and combat-buffer expansion are research targets, not shipped features.

## Usage

1. Serve the folder locally — any static server works:
   ```bash
   npx serve .
   ```
   Or open `index.html` directly in a browser (most features work, but file
   downloads need a real `http://` origin in some browsers).
2. Click **Load ROM** and select your legally-obtained US retail `.v64`.
3. Use the tabs to make edits — pending changes show in the status bar.
4. **Export ROM** writes a fresh `.v64` to your downloads.
5. (Optional) **Save Patch** writes your edits as JSON. **Load Patch** re-applies
   them to a clean ROM.

For save editing: switch to the **Save Game Editor** tab and **Load Save**.
RetroArch `.state` files (Mupen64Plus-Next core) work out of the box.

> **You must supply your own ROM.** No ROM or game code is bundled. Small
> extracted UI/item icons are included only as identification references for the
> editor.

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

Built with vanilla JavaScript — no framework, no build step. Single bundled
dependency: [fflate](https://github.com/101arrowz/fflate) for RetroArch RZIP
save-state decompression.

## Planned features

- **Promotion-tree editing** — write back to the stat gate table (LZSS region,
  parser already in place).
- **Stronghold editor** — modify the 316 stronghold records (location, owner,
  shop assignment).
- **Per-mission enemy deployment** — once the deployment opcode stream
  (currently runtime-instantiated) is fully decoded.
- **Map tab** — visualize the 38-node world map with edit controls (currently
  hidden behind a feature flag).
- **Class promotion-tree visualizer** — interactive graph of all promotion
  paths derived from class def `reqClass` (B55).
- **Combat attack buffer expansion** — lift the hard-coded 28-attack-per-battle
  cap so high-attack-count classes (e.g. patched Fighters at 10/round) can be
  combined without crashing battle setup. Requires relocating the 560-byte
  attack-event log at RAM `0x80220DBC` (adjacent memory is live) and patching
  the 17 `slti 28` bound-check sites in the combat overlay.
- **Bulk patches** — apply common community patches (XP rate, encounter rate,
  rare-item drops) as one-click toggles.

## Credits

See [CREDITS.md](CREDITS.md). LordlyCaliber was built with help from AI
assistants, including **Claude** and **OpenAI Codex**, and coordinated by
**rempred**. The editor would not exist without the community wikis
(OgreBattle64.net, ogrebattle64archive.com), Cralex's GameShark guide, and the
reverse-engineering work of everyone who came before.

## License

MIT — see [LICENSE](LICENSE). Bundled fflate library is also MIT — see
[vendor/LICENSE-fflate](vendor/LICENSE-fflate).

## Disclaimer

LordlyCaliber is an unofficial fan tool. *Ogre Battle 64: Person of Lordly
Caliber* is © 1999 Quest Corporation, published in North America by Atlus and
on N64 by Nintendo. Item icons in `resources/` are extracted from the original
game for identification purposes only. This project is not affiliated with or
endorsed by any rights holder.
