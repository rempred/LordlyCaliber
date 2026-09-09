# LordlyCaliber

LordlyCaliber is a browser-based editor for *Ogre Battle 64: Person of
Lordly Caliber*. It edits ROM data, mission scenarios, neutral encounters,
art, combat animations, and supported save files.

The editor runs locally in a web browser. It does not upload your ROM, and it
never overwrites the source file.

> You must supply your own legally obtained US retail ROM. No ROM or complete
> game archive is included in this repository.

## Start here

1. Download the latest package from [GitHub Releases](https://github.com/rempred/LordlyCaliber/releases), or clone this repository.
2. Open `index.html` in a modern browser.
3. If direct file access blocks a browser feature, serve the folder locally with `npx serve .`.
4. Select **Load ROM** and choose a supported US ROM.
5. Make your edits, then select **Export ROM**.
6. Use **Save Project** to keep a portable JSON copy of your edits.

The editor accepts `.v64`, `.z64`, and `.n64` ROM byte orders. Exports retain
the byte order of the loaded ROM.

## What you can edit

| Area | Main capabilities |
|---|---|
| Shops | Equipment and consumable inventories for all 35 shops |
| Consumables | Supported effect values, ranges, and descriptions |
| Classes | Stats, growth, resistance, promotion, attacks, equipment defaults, and descriptions |
| Items | Stats, prices, resistances, level-up bonuses, and descriptions |
| Encounters | Retail creature pools, global and per-scenario rates, custom neutral squads, persuasion, retreat behavior, rewards, equipment, and encounter-card text |
| Scenario | Enemy squads, formations, levels, equipment, placements, routes, triggers, treasure, towns, and added squads |
| Art and Animation | Class-card avatars, item icons, Army sprites, combat sprites, frame layers, and separated combat sequences |
| Sprite Editor | Reusable sprites, frames, sequences, layers, pixel tools, native art imports, PNG, JPEG, asset files, and transparent exports |
| Cutscene Studio | Native scene inspection and evidence-backed previews |
| Tools | Experience Size Weight Scale, Chaos Frame, Character Card Luck, Squad Menu Alignment, and High Attack Streamsplit patches |
| Save Game Editor | Characters, classes, stats, equipment, inventory, Goth, and Chaos Frame |
| Changelog | A readable report generated from the current Project changes |

The editor validates each changed area before export. A feature becomes
read-only when the loaded ROM does not match that feature's known structure.

## Animation sequence labels

The Body Sprite Sequence menu labels each available sequence by its source.
Mapped sequence labels include the source action and source lane.
A native body program has one lane-neutral label.
The selected target shows `Normal Attack` or `Attack Blocked` separately.
The pose-offset suffix reports changes to local pose-source fields.
It does not claim that the visible art moves.

`Assigned` identifies the sequence used by the selected target.
Selecting another row changes the preview label.
The assignment changes only when the user selects `Assign`.
`Copy From and Separate` includes class idle, advance, return, and Get Hit sequences.
`Preview` identifies a sequence shown without an assignment change.
`Linked` means that two labels share one mutable body program.
Independent private sequences are not linked, even when their program bytes match.

Copy Art retains the destination frame duration. Copy Art and Duration also copies
timing. Duplicate Frame creates independent art with the same duration.
Stable Origin keeps the preview coordinate view fixed; Automatic Fit follows
edited bounds. Position fields use local sprite coordinates.

Whole-sequence imports open preparation before changing the target. Choose
Original Size, Fit, Fill/Crop, or Stretch and inspect each converted frame.
Preparation shows source and output dimensions, scale, and alignment anchors.
Import applies every frame and duration together. A blocked conversion or
replacement keeps the previous sequence and selection. Cancel makes no target changes.

## ROM compatibility

The strongest supported baseline is the North American header revision 0 ROM:

- Size: `41,943,040` bytes
- Game ID: `NOBE`
- SHA-256: `6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12`

North American header revision 1 is also recognized:

- SHA-256: `3BFBAF0AF968795102F6D136713665E347C22723B4CA75BD5494FDC97DF5919E`

| Capability | Header revision 0 | Header revision 1 |
|---|---:|---:|
| General data editing and export | Yes | Yes |
| Chaos Frame Counter | Yes | Yes |
| Squad runtime overrides | Yes | Yes |
| Per-scenario neutral rates and weighted creature drops | Yes | Yes |
| Experience Size Weight Scale | Yes | No |
| Custom neutral squads | Yes | No |
| Character Card Luck and Squad Menu Alignment | Yes | No |
| High Attack Streamsplit | Yes | No |
| Native Art and Animation editing | Yes | No |

Revision 1 runtime patches pass current static checks, but their complete
cold-boot regression is still pending. Unknown regions, game IDs, or header
revisions load in diagnostic mode only.

Modified ROMs can load when every required structure remains recognizable.
The compatibility report explains which areas are readable, restricted, or
blocked. The editor refuses to overwrite unknown bytes in a guarded patch
region.

## Emulator requirements

### Expansion Pak

Use 8 MiB RDRAM / Expansion Pak when an export includes an upper-RAM runtime
override. This includes custom squads, shop overrides, custom neutral
encounters, per-scenario neutral rates, weighted drops, Chaos Frame Counter,
Character Card Luck, Squad Menu Alignment, or High Attack Streamsplit.

Experience Size Weight Scale is a code-only patch. It does not require the
Expansion Pak.

A strict 4 MiB configuration can hang or fault when one of these modules loads.

### Project64 CPU core

High Attack Streamsplit currently requires Project64's **Interpreter** CPU
core. The exported implementation is still tail format v21. The normal
Project64 recompiler is unsupported for this one tool.

Other editor runtime patches do not currently require Interpreter for normal
gameplay.

### Cold-boot testing

Cold-boot a newly exported ROM before judging a runtime patch. An old savestate
can restore old RAM and hide the code that the ROM would load naturally.

Project64 creates a separate battery-save directory for each ROM hash. After
exporting a changed ROM, copy the desired `.sra` file into the new
`OgreBattle64-<hash>` directory. The optional
`supplemental-tools/project64-battery-save-manager/` utility can do this safely.

For RetroArch, use Mupen64Plus-Next with extra memory enabled. Keep the same
core and core version when reusing savestates.

## Projects and exports

A LordlyCaliber Project is a JSON description of edits. It does not contain the
source ROM. Load the same supported ROM before applying a Project.

The Sprite Library is part of the Project. A Sprite Library asset does not
change the ROM. Import a compatible asset into Art and Animation before ROM export.

New blank assets use known target dimensions. Combat frame targets use the
canvas dimensions from the loaded ROM.

Sprite Editor layer imports provide Original Size, Fit, Fill/Crop, and Stretch.
Original Size preserves source RGBA pixels and dimensions. Layer movement keeps
off-canvas pixels until Crop Layer to Canvas is selected.

Scale Artwork resamples pixels. Canvas Size changes working bounds and offers
top-left, center, or bottom-right placement. Canvas Size preserves stored artwork
unless Crop is selected. Both operations preview affected frames and support Undo.

Sprite Library schema 2 and asset-file version 2 preserve layer dimensions,
positions, and alignment anchors. Existing version 1 files load with validated
defaults. Native imports retain alignment anchors where the target supports
local layer positioning. Native palette conversion still applies, and fixed
native canvases can crop larger Original Size sources.

Art and Animation can save supported content to the Sprite Library or export an
asset file. Transfer controls identify the preserved pixels, layer positions,
durations, and alignment anchor. Both editors share the Keep Eyedropper preference.

Create Library Asset adds content to the current Project library. Export Asset File
prepares a separate file download. Use in Art and Animation opens native-target
selection for combat animation, avatars, item icons, or Army sprites. Preparation
shows the replacement scope before application. Return navigation restores the
originating library asset, frame, and layer. Transfer does not save or export a Project or ROM.

Used colors appear in a bounded swatch grid with selected RGB and opacity details.
Enter the grid once with Tab, then use arrow keys to navigate. Layer, frame, and
sequence scope and paging remain available for large color sets.

Sprite Editor provides inline frame-sequence playback. Both animation workflows
provide play/pause, frame stepping, speed control, scrubbing, and current-frame
feedback. Combat comparisons share one timeline. Playback uses approximate editor
timing at 30 ticks per second.

On a focused editable pixel canvas, arrow keys move the cursor. Space or Enter
paints, Delete or Backspace erases, and I samples. Shift plus arrows extends a
rectangular selection. Cancelling a selection drag restores its previous selection.
Dialogs contain keyboard focus and return to an available launching control.

Undo Delete restores one deleted library asset and its editing history. The current
session retains the last 20 deletions. Recovery preserves later edits to other assets.
Applying a Project clears deletion recovery.

Batch alignment shows selected sequences, exact frame numbers, counts, and the
movement vector before application. Review that summary before moving the listed frames.

A combat frame can add one Sprite Library layer without replacing existing layers.
The preparation dialog keeps its library dimensions and colors by default.
Users can change its output size, zoom, crop position, resize method, and dithering.

Class avatar templates allow at most 80 opaque colors. Item icon templates allow
at most 255 opaque colors.

The Art and Animation import maps item colors to the selected icon pack's shared palette.

The Army Sprites tab shows every class route and all six player and enemy formation atlases.
The class view can select a 16×24 small plane or a 32×28 large plane.
This selection also changes the class footprint.
Each Army sprite must use its selected atlas dimensions.
Existing planes keep both fixed palettes.
Existing plane edits rebuild inside the verified compressed envelope.
Missing player planes can use a blank sprite, an imported image, or a converted enemy sprite.
These custom planes expand and relocate the player atlas.
Export verifies resource owners and compressed data.
Users must verify an expanded atlas in the game.

Exports use an isolated copy of the loaded ROM. Before download, the editor:

- validates guarded source bytes and feature ownership;
- rejects conflicting ROM or RAM patch regions;
- rebuilds changed compressed resources;
- reads back supported semantic changes; and
- recalculates the N64 CIC-6102 checksum when required.

A no-edit export is byte-identical to the loaded input.

Save Game Editor changes are separate from ROM Projects. Use **Export Save** in
that tab for RetroArch `.state`, BizHawk `.SaveRAM`, Project64 `.sra`, or raw
8 MiB RDRAM files.

## Important limitations

- High Attack Streamsplit remains an experimental revision 0 feature and requires Interpreter in Project64.
- Custom neutral squads and native Art and Animation editing remain revision 0 only.
- Some oversized mission archives cannot relocate because they use multiple fetch windows.
- A scenario squad leader must use a class with a valid map-unit sprite.
- Shop and text edits must fit their validated capacity or relocation budget.
- Cutscene and combat-animation previews are inspection tools, not cycle-exact Nintendo 64 rendering.
- A structurally valid custom animation can still look wrong in-game; test the exported ROM.

The editor reports the applicable limit at the affected control or during
export. It does not silently truncate unsupported data.

## Developer notes

LordlyCaliber uses vanilla JavaScript and has no application build step.
`index.html` loads the browser application directly.

Key source areas:

- `app.js` and `style.css` — application shell and interface
- `dataset-loader.js` — deferred animation and cutscene datasets during editable revision 0 ROM initialization
- `parsers.js` and `repack.js` — ROM/save decoding and serialization
- `rom-export-candidate.js` — detached export candidate creation and validated state adoption
- `patch.js` and `changelog.js` — Project persistence and change reports
- `scenario.js` and `scenario-eset-codec.js` — mission editing
- `runtimeblob.js`, `squadblob.js`, and `tools.js` — runtime patches
- `art.js`, `art-ui.js`, `army-sprites.js`, `army-sprite-ui.js`, `animation-art.js`, and `animation-ui.js` — art and animation
- `sprite-library.js` and `sprite-editor-ui.js` — reusable sprite assets and pixel editing
- `cutscene-codec.js`, `cutscene-runtime.js`, and `cutscene-renderer.js` — Cutscene Studio
- `tests/` — focused static and serialization regressions

Files named `*-data.js` are generally generated artifacts. Follow the source
generator named in the file header instead of editing generated data by hand.

UI changes must preserve scroll position, focus, caret selection, open dialogs,
active tabs, and active editor selections across rerenders. Add the smallest
deterministic regression test for every reproduced UI bug.

## Credits

LordlyCaliber was built through an AI-assisted reverse-engineering effort
coordinated by **rempred**. See [CREDITS.md](CREDITS.md) for community sources
and individual acknowledgements.

## License

LordlyCaliber is released under the [MIT License](LICENSE).

## Disclaimer

LordlyCaliber is an unofficial fan tool. *Ogre Battle 64: Person of Lordly
Caliber* is © 1999 Quest Corporation and its respective publishers. This
project is not affiliated with or endorsed by any rights holder.
