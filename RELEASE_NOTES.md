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
- Thanks to GrantWChapman for the unit/class byte correction that identified
  the proper same-class unit-size byte and companion-slot capacity behavior.
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
  the Army Management screen and restores cleanly in the validated mid-game
  Army Management flows.
- `high-attack-streamsplit` is exposed as experimental. It installs the v13
  high-attack battle-stream fix on a separate non-conflicting lane:
  tail `0x027A0000`, module RAM `0x80440000`, owner/context
  `0x8044A820/0x80450000`.
- High Attack does not edit class attack-count bytes. Use the Classes tab for
  those values.
- Unit Info display wraps count `9` to `x1`; combat behavior remains the
  authority, and testing has validated counts above 8.
- High Attack remains experimental and should be treated as a research feature,
  not a safe default patch.

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
