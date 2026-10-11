# Scenario Seeder inputs

The two user-provided battery saves and mission ordering are copied from the
parent repository's `tools/scenario_seeder` standalone tool (2026-10-10).
`story-start.sra` supplies the initial story state; `default-army.sra` supplies
the selected 26-character army before the existing tester boosts and inventory
stocking. No ROM data or machine-specific settings are included.

The native `ScenarioSeeder` embeds these inputs. Story routes, world roads,
class equipment and save-codec fields are read from the user's selected ROM.
The default army remains editable through Testing → Edit Default Army → Save Editor.
An edited default is stored in per-user app data and does not replace these
bundled source inputs. Imported armies keep their own statistics.

Developer comparison: run `native/tools/seeder-reference.py` with `--standalone`,
`--rom` and a fresh `--output` directory. Set `LORDLY_TEST_ROM` to that ROM and
`LORDLY_SEED_REFERENCE` to the output directory, then run `save_document_tests`
cases `scenarioSeederParity` and `scenarioSeederInstallation`. The comparison
normalizes inventory row ordering only; quantities, full decoded saved state and
load-menu summaries must agree. Installation checks use an isolated temporary
folder and leave any running Project64 session alone.
