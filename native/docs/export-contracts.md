# Native editor architecture and export contracts

The editor has separate layers, but ROM feature composition still has manual
boundaries. A valid allocation and a successful round trip do not establish that
every game consumer received the right data.

## Layers

| Layer | Responsibility | Main entry points |
|---|---|---|
| Desktop UI | User actions, selection, previews, undo | `app/`, linked against `lordly_ui` |
| Domain and project | ROM parsing, typed edits, project persistence, resource identity | `engine/`, `Project` |
| ROM rebuild | Compile game C, allocate/link resources, compose patches, validate output | `rebuild/compiler.cpp`, domain rebuilders |
| Game implementation | Authenticated matching C and explicit authored extensions | `game/accepted/`, feature directories |
| Verification | UI tests, persistence, exported instruction comparisons, external game harness | `tests/`, `tools/`, parent `tools/testing/` |

The engine is independent of Qt Widgets. GUI and CLI use the same project and
rebuild path. Feature allocations have descriptors, sizes and checksums; many
native owners also authenticate their original instructions before replacement.

## Two different collision risks

**Storage or code ownership:** allocations can overlap, or one whole-function
replacement can bypass another feature's inner hook. `tools/runtime-ownership.cjs`
checks catalogued spans and fixed storage. `tools/runtime-sharing.json` records
the specific overlapping ranges whose behaviors are deliberately composed.
Movement and cursor instruction tests also compare actual exported patch records
against those claims, so missing catalog coverage fails.

**Shared meaning:** two consumers can interpret the same numeric ID differently
while all addresses remain valid. Dialogue appearance IDs 165–185 are NPCs;
custom class IDs can use those numbers in a different context. Dialogue and class
card lookups therefore require separate routing. An allocator cannot detect this
semantic error. Tests must identify and exercise both native consumers.

## Required evidence for a shared or expanded resource

- Identify native readers, writers and loaders, their ID ranges and lifetime.
  Do not infer that a table belongs only to the editor tab which displays it.
- Separate unchanged native behavior from authored behavior. Compare unchanged
  consumers with original-ROM instructions/data; use explicit user-facing rules
  for new behavior.
- Test the first and last added IDs through each relevant consumer. Include an
  interaction that activates another owner of the same code or data.
- Treat export/reopen equality as persistence evidence. Reader and writer may
  share the same mistaken interpretation.
- State trapped or substituted services. Real selector execution proves a
  selector contract; substituted animation durations cannot prove synchronization.
- For a regression, retain a failing prior implementation or equivalent negative
  control when practical. Confirm the check distinguishes the defect.

## Current limits

`rebuild()` remains an ordered pipeline, with some dependencies expressed by call
order and feature-specific symbol mapping. The ownership scan covers its listed
families; it is not a complete inventory of every export write, dynamic allocation
or overlay lifetime. Actual-patch coverage is currently enforced for movement and
cursor modules, not every exporter. Full game-path observations remain necessary.

A future centralized patch plan should declare writes, complete replaced owners,
consumer namespaces and composition dependencies before mutating the image.
Until then, expanding the existing checks is useful only when their stated scope
and independent game evidence remain explicit.
