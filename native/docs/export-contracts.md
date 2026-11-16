# Native editor architecture and export contracts

The editor has separate layers and one sequential export plan. A valid allocation
and a successful round trip do not establish that every game consumer received
the right data; those still require tests against the original game.

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

## Reusing compilation work

`CompilationCache` stores successful C compilation artifacts after preprocessing.
Each export still authenticates the bundled toolchain and original C inputs.
The key includes the resolved source, actual compiler/assembler/linker/extraction
arguments, link script (addresses and symbol bindings), qualified layout and
toolchain manifest. Preprocessing runs every time, so included files and dynamic
macros cannot silently become stale. Inline assembly with file dependencies
compiles afresh. Assembly-only owners and resource compression remain uncached.
Changes to artifact transformations not represented by that recipe must bump
the cache format/version before old artifacts can be reused.

Hits restore the assembly listing, object, linked ELF and extracted bytes into
the current temporary build directory. Original-owner comparisons, module and
relocation inspection, data comparisons, allocation/composition checks, checksum
and final ROM reopening still run. No patched ROM image or project is cached.

Storage is under the platform's generic cache directory in
`LordlyCaliber/rom-compiler-v1`. Complete entries use atomic replacement and an
integrity checksum; absent, partial or corrupt entries are misses. Unavailable
storage falls back to normal compilation. Oldest entries are pruned to a 256 MiB
budget at initialization and after each 16 MiB of writes; individual entries are
limited to 64 MiB. Deleting the directory only causes recompilation.
For diagnostics/tests, `LORDLY_COMPILER_CACHE_DIR` selects an isolated directory;
`LORDLY_DISABLE_COMPILER_CACHE=1` forces fresh compilation. GUI and CLI share the
same default cache and report reused/compiled C unit counts.

## Two different collision risks

**Storage or code ownership:** allocations can overlap, or one whole-function
replacement can bypass another feature's inner hook. `tools/runtime-ownership.cjs`
checks catalogued spans and fixed storage. `tools/runtime-sharing.json` records
the specific overlapping ranges whose behaviors are deliberately composed.
Movement and cursor instruction tests also compare actual exported patch records
against those claims, so missing catalog coverage fails.

`rebuild/export_plan.*` coordinates the domain builders used by both GUI and CLI.
Named steps declare prerequisites; missing, reversed or cyclic dependencies fail
before builders run. Execution retains the established order, including lazy
project loads and the resident-builder outputs consumed by later steps.

`engine/rom_writer.*` owns the committed image and exposes only read access.
Named writes or privately prepared replacements are checked before commit.
Each plan step receives a private candidate. The writer compares its actual changed,
appended and removed spans with the current owners before accepting the result.
An explicit bounded write claims its entire requested span, even when bytes agree.
Complete-function claims from the existing catalogs and native declarations
protect untouched inner hooks when another part of their function changes. Unchanged steps do not
take ownership. A conflict names both steps and the z64 range; any failure restores
the plan's input image. Rebuild must also pass its existing readback checks before
`exportRom` can publish a file.

Intentional composition is directional: name the earlier step and exact permitted
ranges, and declare the dependency. The later step takes ownership only of the
spans it actually writes or replaces. `export_contracts.cpp` records the existing
supported handoffs, reusing native catalogs and qualified relocation maps rather
than another allocator. The symmetric static sharing inventory is not blanket
permission. The final CIC checksum is a derived eight-byte header field; other
metadata and resource checksums require their explicit writer contracts.

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

## Restoration and composite builders

Project loading uses the same writer to normalize cursor, movement and capacity
installations. Capacity may restore the exact patch spans recorded beneath
movement; each remover still authenticates its input. Export separately names
weapon-type, item-tree, attack-reference and custom-encounter removal. Cleanup
writers are checked against one another, then release their ownership before
new installation. A failure cannot publish an intermediate image.

Class linking separately records storage, table references, names, save hooks,
description routing, flags, affinity, item trees, attack metadata and promotion
menus. The shared relocation map still rejects two different values for the same
word. Body height, custom encounters, equipment markers, animation resources,
body selection, pose execution and visual policy are separate export steps.

Complete function claims also cover body/pose replacements, personal names,
expanded promotion menus, consumable execution, persuasion, weapon classification,
weapon status and the replaced cutscene dispatcher. Animation directory updates
inside equipment construction have a four-byte handoff; the later visual-policy
C preserves the marker-aware implementation. Custom encounter functions retain
relocated class references, and later capacity/attack patches have their own
bounded composition contracts.

## Current limits

Every registered export step and the class-linker subwriters use the controlled
writer. Legacy domain helpers still prepare byte arrays internally; their final
changes and declared full owners are checked at the boundary. This does not
observe every intermediate assignment inside every helper. New compound
features must use separately named writes or steps, rather than hiding multiple
features behind one candidate. Dynamic allocations and overlay lifetimes retain
the domain allocators and validators. Consumer semantics still need game tests;
write ownership cannot prove that two valid IDs mean the same thing.

For a new writer: add a named step and real prerequisites, declare any complete
function it replaces, then add only the bounded handoffs its composition needs.
Test a competing write and a supported combination. Keep class, dialogue, item
and other consumer ID meanings separate in domain models; an address check cannot
prove those meanings. Original-game consumer and game-path tests remain necessary.

## Shared scenario setup

Movement initialization now replaces only US Rev 0 z64 `102DB0..102E34` inside
scenario setup `00101DDC`. The rest of that owner still executes its configured
encounter rewards, resource loads and mission/capacity references. The bounded
helper copies all movement rows and applies the original scenario-13/flag-14 and
scenario-17/flag-15 storm restrictions. It uses the native source-first copy
service. All skipped instructions are checked before installation; the branch
back to native setup has a safe delay slot.

`movement-setup-mips.cjs` compares this block with the original instructions.
`movementPreservesSharedSetup` combines movement, weighted rewards and consumable
artwork, checks preservation of the rest of the owner, and runs the native reward
selector. The old whole-function movement replacement fails the entry check.
