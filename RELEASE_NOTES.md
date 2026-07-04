# Draft Release Notes

## v0.3.0 Candidate

### Highlights

- Added the **Scenario tab**: a map-first per-mission editor for all 64 runtime
  scenario keys. Missions render as maps with draggable enemy squad markers
  (leader portraits + allegiance rings); clicking a squad opens the full
  composition editor in the sidebar. The standalone Squads tab is retired —
  its editor now lives inside Scenario.
- **Add Squad**: place entirely new enemy squads on a mission. New squads
  splice a real placement row into the mission's ESET archive and carry their
  composition through the scenario-gated runtime override lane; the full path
  is cold-boot proven in Project64.
- **Routes and behaviors**: drag-draw movement routes with editable waypoints;
  a live behavior builder edits triggers and gates (player-at-site, site
  flags, squads-remaining thresholds, AND/OR compound gates) and applies
  changes as you make them, re-using its own Section 2/3 slots so exploring
  templates never erodes the mission's 16-node/16-extra caps.
- **Mission archive grow/relocate lane**: when an edited mission no longer
  fits its original compressed slot, export automatically copies it to free
  ROM-tail space behind a small DMA redirect installed in the game's resource
  loader. Currently supported for single-fetch-window missions (~32 of 63);
  the UI reports exact fit/relocation status per mission. Relocation exports
  recalculate the CRC, and the UI explains the resulting new Project64 save
  folder with a recovery recipe.
- **Patch v7**: JSON patches now embed the full Scenario project payload
  (modified mission ESETs, added squads, site intents) alongside all previous
  lanes, so one patch file reproduces a complete scenario mod on a clean ROM.
  v6 and earlier patches still load.
- **Crash guard — squad leaders need map sprites**: 85 of 165 classes
  (monsters, undead, Ninja, most special/story classes) have no map-unit
  sprite, and the game hangs during mission LOADING in a runaway DMA if one
  leads a deployed squad (the sprite lookup's "none" sentinel is consumed
  unchecked as a resource index — a vanilla engine defect). Export now blocks
  added squads and leader-changing overrides whose leader class has no sprite,
  naming the squad and the class. Squad members are unrestricted.
- **Cache-coherency hardening** for the squad-override runtime module: the
  bootstrap now invalidates the CPU instruction/data caches over the blob
  region before use, calling the game's own resident cache helpers — the same
  pattern the game's loader uses for its own DMAs. Verified end-to-end by
  cold-boot regression.
- Scenario work saves standalone as a JSON project file, independent of
  patches.

### Scenario Editing Details

- Per-mission maps use calibrated per-key registrations with a schematic
  fallback (bounds, sites, markers); full-art backgrounds are a local
  calibration workflow and are not bundled.
- Site rings and trigger targets resolve to real town names from the static
  scene tables; kind-12 site triggers edit through a site dropdown.
- Town-allegiance intents (neutral/allied) persist in projects and patches but
  do not export to ROM yet; enemy-held-via-garrison placement exports fully.
- Added-squad donor records come from a verified census of enemy-data records
  referenced by no mission (used read-only as templates; the override lane
  never overwrites them in ROM). Export validates donor content collisions.
- Per-mission add-squad budget follows the game's 50 deploy slots and the
  archive fit / relocation status; the UI meters both.

### Safety And Validation

- A no-edit export remains SHA-256 byte-identical to the loaded ROM (verified
  after every lane added in this release).
- The relocation lane declares its ROM regions (hook, cave, redirect table,
  tail windows) through the same patch-region ownership checks the Tools tab
  uses, so overlapping feature combinations are rejected on export.
- The mission fetch-window model was corrected from a fixed offset to the
  live-traced 0x200-byte grid; relocation is guarded to missions whose
  original fetch is a single window and whose rebuilt archive still fits it,
  with precise per-mission error messages otherwise.
- The spriteless-leader gate runs before any squad-override bytes are staged,
  in both the Squads and Scenario export paths.
- New headless checks:
  - `node tools/test_editor_leader_guard.js` (parent workspace)
  - `node tools/test_editor_noop_export.js` (parent workspace)

### Known Caveats

- Squad leaders must use classes with map-unit sprites (export enforces this);
  giving monsters and special classes map sprites is a research target.
- Multi-fetch-window missions still enforce their original archive slot size.
- Newly placed squads inherit the dormancy semantics of their placement row;
  dormant/ambush authoring works but reads as advanced usage — wake timing is
  driven by the game's trigger evaluation, not by the editor.
- The Squads/High Attack/Chaos Frame cache-invalidate hardening shares a
  partitioned code cave; the Squads slot is cold-boot regressed, while the
  High Attack and Chaos Frame slots remain static-build-only pending their own
  regression runs.

## v0.2.0 Candidate

### Highlights

- Added the public Squads tab for runtime-key enemy squad editing.
- Added scenario/runtime key context, wiki identity labels, branch labels, and
  loaded EDAT rows so scenario variants can be edited separately.
- Added squad formation portraits and class-card portraits from the bundled
  offline portrait set.
- Corrected class-header framing and same-class unit size parsing.
- Thanks to GrantWChapman for the unit/class byte correction that identified
  the proper same-class unit-size byte and companion-slot capacity behavior.
- Added warning-gated raw class record viewing/editing for sentinel, story, and
  NPC class records.
- Corrected squad C-slot capacity handling and added experimental raw-capacity
  mode for all encoded squad anchors.
- Expanded the Tools tab beyond Chaos Frame with experimental High Attack
  Streamsplit support.
- Refactored the Chaos Frame counter into its own standalone mod-region module
  instead of sharing fragile tail/free-RAM space with other patches.
- Added ROM/RAM patch-region metadata and collision checks so enabled patches
  cannot silently share the same tail, bootstrap, or runtime region.
- Fixed JSON patch save/load for Squads so saved patches carry the same
  runtime-key 35-byte replacement records that Export ROM writes.
- Updated release-facing docs to describe default versus experimental squad
  capacity, Tools safety checks, the Chaos Frame refactor, and the current
  high-attack caveats.

### High Attack Streamsplit

- `high-attack-streamsplit` is exposed as experimental. It installs the v13
  high-attack battle-stream fix on a separate non-conflicting lane:
  tail `0x027A0000`, module RAM `0x80440000`, owner/context
  `0x8044A820/0x80450000`.
- High Attack Streamsplit is a runtime battle-engine patch for classes with
  attack counts above the vanilla-safe range. The original failure was not the
  small result log; high-count battles overran the action-stream/context layout
  used by battle setup and playback.
- The streamsplit patch keeps the normal battle behavior path but relocates the
  battle context/action-stream work area into free RAM and splits the control
  trailer away from the growing stream. It also installs the current guards for
  known null actor/class stream cases and final stream handoff behavior.
- High Attack does not edit class attack-count bytes. Use the Classes tab for
  those values.
- Unit Info display wraps count `9` to `x1`; combat behavior remains the
  authority, and testing has validated counts above 8.
- High Attack remains experimental and should be treated as a research feature,
  not a safe default patch.

### Squads And EDAT Overrides

- The Squads tab edits enemy squad templates by runtime scenario key, not by
  wiki mission number alone. This matters because several runtime keys are
  branch variants, aliases, internal cases, or loaded-only scenarios.
- Each editable row is an EDAT template used by the selected runtime key. The
  editor shows the matched wiki mission/squad label when the current research
  atlas can identify it, plus loaded EDAT rows that were present in ESET even
  when the older builder trace did not observe them.
- Export ROM does not rewrite global `enemydat.bin`. Instead, it writes a
  runtime override module:
  - hook: record-builder trampoline at ROM `0x00195584`
  - bootstrap: z64 `0x0283C4` / RAM `0x80097FC4`
  - override blob: z64 tail `0x02780000` -> RAM `0x80400000`
- At runtime, the module checks the live scenario key at `0x801936A7`, matches
  the original 35-byte EDAT record, and copies the replacement 35-byte record
  over the live template before the game builds the deployed 52-byte unit
  records. This makes reused EDATs safely editable per scenario/key.
- Save Patch / Load Patch now round-trips these squad overrides as
  runtime-key/EDAT 35-byte replacement records, so a JSON patch can reproduce
  the same Squads output as Export ROM.
- The safe default editor path stays inside the vanilla-style limit of five
  formation slots and two follower class groups. Experimental raw-capacity mode
  can encode all seven EDAT anchors (`Leader + Bx3 + Cx3`), but this is a
  research option, not a promise that map inspection, battle placement, or
  targeting will support it.

### Tools

- `cf-army-counter` remains the stable Tools feature. It shows Chaos Frame on
  the Army Management screen and restores cleanly in the validated mid-game
  Army Management flows.
- The Chaos Frame counter was refactored into a standalone module lane:
  - hook: z64 `0x023F7C`
  - bootstrap: z64 `0x034E78` / RAM `0x800A4A78`
  - tail blob: z64 `0x02790000`
  - module RAM: `0x80420000`
- The bootstrap checks an `OBCF` sentinel, PI-DMAs the module from ROM tail into
  free RAM, then jumps into the counter module. The module inserts the display
  task into the Army Management UI path and uses screen/header fingerprints
  rather than a volatile single state pointer. This keeps the CF counter away
  from the Squads and High Attack patch regions.

### Safety And Validation

- Tools writes now reject RAM-looking values used as ROM offsets.
- Tools and dynamic squad overrides declare occupied ROM/RAM regions.
- Export blocks selected feature combinations that would overlap.
- JSON patch round-trip now covers squad overrides as well as Tools toggles.
- Current headless checks:
  - `node scripts/ob64_editor_tools_test.js`
  - `node tools/verify_squadblob.js`
  - `node --check editor/tools.js`
  - `node --check editor/app.js`
  - `node --check editor/tools-data.js`
  - `node --check scripts/ob64_editor_tools_test.js`
  - `python -m py_compile tools/export_editor_cf_tool.py tools/build_high_attack_stream_shift_rom.py tools/project64/ob64_high_attack_adapter.py tools/project64/pj64_repro_high_attack_manual_za.py`

### Known Caveats

- High Attack Streamsplit is experimental until the moved lane gets fresh
  emulator regression coverage.
- High Attack known-bugs warning: pressing `A` at battle start / `FIGHT IT OUT`
  can take an unstable path and may crash.
- High Attack known-bugs warning: opening or using the battle menu during a
  high-count battle may end the battle early, softlock, or crash.
- High Attack may still expose scheduler/stream edge cases in unusual battle
  flows, high unit counts, animation timing, or scenario-specific cut-in paths.
- High Attack display caveat: Unit Info can misrepresent very high attack
  counts, including wrapping count `9` to `x1`; combat behavior is the
  authority.
- Chaos Frame display caveat: one very-early-game Army Management screen showed
  no counter even though the ROM patch bytes were present; this is suspected to
  be a runtime visibility/state gate and needs a targeted follow-up.
- Raw squad capacity mode can encode formations beyond vanilla organization
  assumptions; this is intentional modder-facing behavior, not a guarantee that
  every game menu or battle-placement path supports over-cap organization.
- Raw squad capacity known-bugs warning: a key 2 / EDAT 13 seven-unit test
  applied the replacement record correctly, but map inspection hid units and
  battle placed units off-grid; the misplaced units also could not be attacked.
  Treat 6-7 unit squads as research-only until that exact scenario is manually
  validated.
- More than two follower class groups is still not exported; supporting that
  requires a larger runtime record/resolver design.
- The parent research workspace docs and generated research artifacts are not
  part of the nested LordlyCaliber product Git repo.
