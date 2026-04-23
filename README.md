# LordlyCaliber

A browser-based mod editor for *Ogre Battle 64: Person of Lordly Caliber*
(Quest, N64, 1999). Edit shops, classes, items, neutral encounters, and save
files — then export a patched ROM.

No installation, no build step. Open `index.html` in any modern browser, drop
in your own copy of the US retail `.v64`, and start modding.

## Features

- **Shops** — modify the inventory of all 35 in-game shops, within the empirical
  324-item / 24-per-shop budget.
- **Classes** — edit base stats, growth curves, resistances, combat multipliers,
  promotion gates, and row-attack counts for all 164 classes (0x01–0xA4) using
  the authoritative GameShark mapping.
- **Items** — change weapon/armor/spellbook stats, prices, and resistances for
  all 277 equipment entries.
- **Encounters** — adjust the neutral-encounter creature pool across all 40
  scenario slices (10 globally-consistent terrain slots each).
- **Save Game Editor** — load RetroArch `.state` saves (RZIP-compressed or raw)
  or 8 MB RDRAM `.bin` dumps. Edit character names, classes, levels, stats,
  equipment overrides, alignment, element, experience, army inventory
  (equipment + consumables + treasures), and Goth.
- **Patches** — save your edits to a portable JSON patch file for sharing or
  reapplying to a fresh ROM.
- **Export** — writes a clean `.v64` with the N64 CIC-6102 CRC re-calculated.

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

> **You must supply your own ROM.** No copyrighted code or assets are bundled.

## How it was built

The editor is the working surface of an extended reverse-engineering effort
on the US retail ROM:

- All 825 LHA archives in the data section catalogued and round-trip-decoded.
- 56-byte character struct, 72-byte class definition table (166 records),
  32-byte item stat table (295 records), 12-byte consumable master table
  (45 records), 20-byte neutral-encounter scenario slice, and the 28-byte
  stronghold record decoded against in-game testing and emulator memory diffs.
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
- **Bulk patches** — apply common community patches (XP rate, encounter rate,
  rare-item drops) as one-click toggles.
- **Audio replacement** — pending decode of the 20 MB custom audio format.

## Credits

See [CREDITS.md](CREDITS.md). The editor would not exist without the
community wikis (OgreBattle64.net, ogrebattle64archive.com), Cralex's
GameShark guide, and the reverse-engineering work of everyone who came before.

## License

MIT — see [LICENSE](LICENSE). Bundled fflate library is also MIT — see
[vendor/LICENSE-fflate](vendor/LICENSE-fflate).

## Disclaimer

LordlyCaliber is an unofficial fan tool. *Ogre Battle 64: Person of Lordly
Caliber* is © 1999 Quest Corporation, published in North America by Atlus and
on N64 by Nintendo. Item icons in `resources/` are extracted from the original
game for identification purposes only. This project is not affiliated with or
endorsed by any rights holder.
