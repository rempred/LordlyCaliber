# Draft Release Notes

## v0.2.0 Candidate

This is a large editor release candidate after `v0.1.2`. It expands the editor
from table editing into runtime-backed squad editing and experimental ROM tool
features, so `v0.2.0` is the recommended version number rather than `v0.1.3`.

### Highlights

- Added the public Squads tab for runtime-key enemy squad editing.
- Added scenario/runtime key context, wiki identity labels, branch labels, and
  loaded EDAT rows so scenario variants can be edited separately.
- Added squad formation portraits and class-card portraits from the bundled
  offline portrait set.
- Corrected class-header framing and same-class unit size parsing.
- Added warning-gated raw class record viewing/editing for sentinel, story, and
  NPC class records.
- Corrected squad C-slot capacity handling and added experimental raw-capacity
  mode for all encoded squad anchors.
- Expanded the Tools tab beyond Chaos Frame with experimental High Attack
  Streamsplit support.
- Added ROM/RAM patch-region metadata and collision checks so enabled patches
  cannot silently share the same tail, bootstrap, or runtime region.
- Fixed JSON patch save/load for Squads so saved patches carry the same
  runtime-key 35-byte replacement records that Export ROM writes.
- Updated release-facing docs to describe default versus experimental squad
  capacity, Tools safety checks, and the current high-attack caveats.

### Tools

- `cf-army-counter` remains the stable Tools feature. It shows Chaos Frame on
  the Army Management screen and restores cleanly.
- `high-attack-streamsplit` is exposed as experimental. It installs the v13
  high-attack battle-stream fix on a separate non-conflicting lane:
  tail `0x027A0000`, module RAM `0x80440000`, owner/context
  `0x8044A820/0x80450000`.
- High Attack does not edit class attack-count bytes. Use the Classes tab for
  those values.
- Unit Info display wraps count `9` to `x1`; combat behavior remains the
  authority, and testing has validated counts above 8.
- High Attack still needs fresh Project64 cold-boot/high-count regression on
  the moved editor lane before it should be called release-ready.

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
- Raw squad capacity mode can encode formations beyond vanilla organization
  assumptions; this is intentional modder-facing behavior, not a guarantee that
  every game menu supports over-cap organization.
- More than two follower class groups is still not exported; supporting that
  requires a larger runtime record/resolver design.
- The parent research workspace docs and generated research artifacts are not
  part of the nested LordlyCaliber product Git repo.
