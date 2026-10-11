# Game source owners

For current upstream adoption priorities and a symbol/source-file lookup, see
[Using newly accepted Decomp C](../docs/decomp-intake.md) and the maintained
[function interest list](decomp-interest.json). Matching status comes from
Decomp's existing reports; the list tracks editor uses and intentional extensions.

## Complete accepted library

`accepted/library/` contains the full accepted pure-C set, with its companion
sources and headers. `accepted/library-imports.json` identifies the version,
original owners and function bodies. It is separate from the smaller historical
imports used by active adaptations: updating available source does not silently
change those implementations. The interest report supports `--all` and searches
the full library by symbol or source name. See the library README for scope.

## Accepted save and pursuit batch — 2026-10-09

`accepted/2026-10-09/` retains the complete accepted Squad W4 and save-record
checksum chain: 29 original owners containing 33 functions, including earlier
accepted companions. The original relative source/include paths are preserved.
`accepted/gameplay-imports.json` records the source revisions, the October 9
verification time, and identities of every source and included dependency.
These are source files, with no ROM data or precompiled N64 code.

The active save implementation shares `classes/save_checksums.h` between PC save
editing and the C compiled during user export. Its two kernels adapt accepted
`00005E84`/`00005EC4` (byte sum) and `00005F00`/`00005F60` (bit count), including
halfword narrowing and seeded/zero-seed variants. `save-format.c` supplies the
signature/validation policy from `00005D9C`, `0000581C`, `00005978`, and `00005A88`.
The existing `save-runtime.c` read/copy/check services implement the chain around
those shared functions. Explicit big-endian storage, bounded buffers, expanded
record sizes and signatures, and Controller Pak resealing remain intentional
adaptations. The empty suspend header still skips checksums only after its
signature is recognized. Save caching, SRAM I/O, allocation and slot repair are
supplied by the existing host/game services.

All 21 Squad W4 functions are **imported**, available for future use. They are not
compiled into the PC engine or executed by Routes yet. This includes the complete
pursuit producer, `00127D30` target selection, `001284C4` movement-cell evaluation,
and their target/reset/eligibility companions. Current Routes edits and explains
authored orders; it does not simulate live flags, dynamic candidate lists,
terrain costs or N64 object lifetime. Keep the accepted source intact when
introducing portable consumers with those inputs. Importing source and adapting
an executing consumer are separate statuses in `decomp-interest.json`.

Existing save checks compare source-built results with original ROM instructions,
including signatures, corruption rejection, cleared suspend records, expanded
SRAM/Pak conversion, and runtime dispatch. Save editing/Undo and ROM export/reopen
checks cover the consumers; Decomp's matching verification is reused upstream.

## Accepted stock and character initialization

`accepted/gameplay-imports.json` identifies the unchanged matching originals for
this integration. `shops/stock.c` adapts `func_0019BE40` into a complete ROM-side
function compiled during export. Allocation, scenario/site-to-stronghold lookup,
and archive cleanup follow the recovered C. Authored halfword stock replaces the
retail archive range and fixed consumables, retaining expanded items and shops.
The function and the small assembly selection guard share the original 432-byte
owner (z64 `19BE40`, Shop RAM `802195E0`). The guard retains its existing `s0`
category convention. Version-4 descriptors cover the complete function and its
inventory relocation; versions 1–3 remain readable and upgrade during export.
`shops/producer.s` is the earlier producer retained for compatibility research,
not a source used by new exports.

`army/character.c` adapts `func_0004501C`, `func_00044aa4` and their seven item
growth getters into the PC engine. Save recruitment now offers levels 1–99 for
both reserve characters and new squad leaders. It uses the project's class and
item tables, growth-class transitions, randomized growth, default equipment,
999 stat caps and the initializer's final HP/luck reset. The game uses the same
item getter for HP and STR growth; that behavior is retained. Explicit endian
reads and caller-owned random state replace N64 addresses and global RNG. The
save allocation, packed format, names and atomic Undo remain host responsibilities.

Existing execution checks compare the adaptation with original ROM instructions;
shop checks execute exported code with allocation/resource/DMA services trapped.
These establish relevant records, references and service calls, not full-game
rendering or a natural recruitment/shop session.

## Accepted animation and sprite readers

`accepted/animation-imports.json` records the exact upstream source versions and
verified source identities for this integration. The complete original C files
are retained unchanged beside it; portable differences live in the adapters.

| Accepted function | Executed editor implementation |
|---|---|
| `00201798` | `art/animation_readers.c`: base action/mode selector; the C++ caller applies existing OBSO overrides afterwards. |
| `00204F34`, `002050AC` | `art/animation_readers.c`: counted pose decoding into the playback cache. `art/pose_operands.h` also executes in the ROM class reader compiled during export. |
| `00201E38` | The unchanged accepted C is compiled into the PC engine and used for sprite row strides. |
| `00203414` | `images/sprite_readers.c`: bounded, big-endian child-layout traversal. |
| `00203664` | `images/sprite_readers.c`: nibble/byte/halfword subtraction and RGBA32 XOR kernels. Recursion, cycle checks and allocation remain with `SpriteObject`. |

The PC adaptations take explicit buffers instead of N64 pointers. Pose decoding
walks each program once; cached playback remains constant-time. The shared ROM
operand helper retains nullable/aliased writes and the existing compact layout,
leaving room for private registration. Wide frames/states, additional classes and
normal/blocked override semantics remain supported. No original instruction blob
is shipped, and end users need no Decomp checkout.

The accepted sprite-layout reader specifies zero secondary-plane bytes for RGBA32
even with child flag2 set. The adapter follows that rule and preserves the flag
when editing. Complete accepted files are also retained for functions only partly
adapted; source presence alone does not mean the full function runs on the PC.

`accepted/func_000bbd50.c` is copied unchanged from OB64 Decomp's accepted
`src/lib/func_000bbd50.c` at commit `96b75a67b71bf6f5e938dbef34e355c48f039865`.
It is a pure-C class-family lookup for the existing retail promotion roster.
It is compiled locally during applicable exports, not built into an N64 byte
bundle before shipping. The export compares its complete 48-byte result with
the corresponding source ROM owner before writing it.

Placement: z64 ROM `0x000BBD50`, Army overlay RAM `0x801CF2F0`.
The placement comes from the accepted Army overlay mapping documented in the
parent `docs/promotion-system.md`, not the early-boot RAM mapping.

This owner still has the retail 81-entry family bound. It does not establish
support for expanded class IDs. Modified owners must have explicit allocation,
dependency, and behavior coverage before replacing it.

`promotion/family-menu.c` is readable, modified-game C compiled during export.
It replaces the complete menu builder within its original 528-byte extent.
Its configuration currently retains the 81-entry manual roster and menu format;
changing the count alone does not complete expanded-class support. The prior
`family-menu.s` remains a behavior reference used by the developer MIPS test.
The accepted Decomp `func_000bf248.c` is hybrid C/assembly, not pure C, and was
used as a semantic reference rather than copied as this implementation.

`promotion/class-selection.c` replaces the complete 388-byte class selection
input routine with maintained C (384 compiled bytes including alignment). It
preserves four-row navigation, button precedence and refresh calls, and guards
an empty list against an out-of-bounds read. Its named game services and globals
are linked through the explicit Army addresses in `owners.json`. The original
accepted `func_000c91a0.c` is hybrid C/assembly and is the semantic reference.

`promotion/family-lookup.s` retains the special two-register return convention
used by existing callers. It is assembled and linked during user export.
Seven companion owners preserve authenticated input-ROM
words and replace their documented family comparison or resource-key statements
in generated assembly. No original game instruction bundle is embedded here.

`promotion/class-discovery.c` rebuilds the roster discovery routine at Army RAM
`0x801D734C` (z64 `0xC3DAC`) in 308 bytes within its original 420-byte owner.
It preserves both roster walk limits, the complete eligibility call sequence,
and the retail terminal-row probe. The count and bitmap location are explicit
configuration for later expansion. A primary ID beyond the configured record
count uses its secondary-class fallback instead of reading beyond the alias
table. This is modified-game C reconstructed from the original routine; it is
not an accepted matching-C source copied from Decomp.

`promotion/owners.json` is generated by
`native/tools/generate-promotion-owner-catalog.cjs` from the validated browser
Rebuild baseline. It contains extents and expected hashes only. Every assembled
function must match its recognized source implementation exactly. Prior family
owners have coverage of all 6,561 source/target compatibility pairs and 330
retail menu cases. `native/tests/class_menu_mips.cjs` executes 1,089 cases comparing
the compiled C with the previous menu assembly, plus explicit 256-ID list cases.
Those expanded cases use a synthetic larger owner buffer and relocated discovery
storage; they do not validate the unchanged game's consumers or save format.
With `LORDLY_TEST_ROM` supplied, the same test compares selection against the
user's original routine in 2,420 cases. Seven explicit expanded/empty-list cases
verify full-byte IDs and navigation bounds. These checks execute integer MIPS
with service traps; they are not rendered game playback.
Discovery adds 144 original/C comparisons of flags and callback order, plus two
256-ID cases with guard bytes around the relocated bitmap and an unchanged army
roster. The comparisons initially caught an incorrect early loop exit; the C
preserves the original behavior of checking the remaining actors after a match.
Native checks also cover compilation, allocation, persistence and export/reopen.
The implementation still covers the 81-entry manual-promotion roster; expanded
class IDs require broader data, menu and discovery changes.

`promotion/class-render-rows.c` replaces the two class-row construction loops
inside the Army renderer at z64 `0xA4D5C..0xA4ED8` / RAM `0x801B82FC..0x801B8478`.
The surrounding renderer remains authenticated source-ROM code. The 16-byte
`class-render-entry.s` bridge and compiled C fit within the existing 380 bytes:
324 bytes for the retail configuration, 340 for the expanded configuration.
The C retains the two menu modes, zero terminator, row type, grid coordinates,
and matrix calls. It refuses to exceed the 167 rows before the count field.
In expanded mode both class-change and equipment screens use the full list,
submitting at most 81 rows around the selected column, preserving
their original coordinates for scrolling. This bounds row/matrix submissions;
it does not yet prove live-game visual behavior or display-list peak capacity.

`class-render-style.s` rebuilds the 76-byte class-specific style/sprite drawing
block at z64 `0xA5668`. It retains the surrounding function's register contract.
Its retail configuration reproduces the original bytes; the expanded version
reads a halfword, separates bit 15 from the full-byte class ID, and retains the
four selected/disabled styles. Other renderer row types keep their own formats.
The exporter currently uses the retail configuration. Expanded configuration
is covered by execution tests and awaits the other new-class consumers.

The MIPS test compares 144 row-building cases with the original user-ROM code,
checks 1,821 expanded-list/buffer-boundary cases, and checks 1,536 class/style/sprite
combinations. Matrix/coordinate/sprite services are traps: this proves arguments,
row data, flags and write bounds, not rasterized output. Public export/reopen
checks cover this renderer alongside family and relocated-name edits. Assembly
call targets are supplied as linker bindings; absolute local assignments in
this historical assembler produced zero jump targets during integration and
were rejected by the whole-owner hash check.

`promotion/compatibility-mask.c` adapts the accepted pure-C
`src/lib/func_000bbd80.c` at Decomp commit `96b75a67b71bf6f5e938dbef34e355c48f039865`.
The retail configuration reproduces its entire 44-byte Army owner at
RAM `0x801CF320` / z64 `0xBBD80`. Family exports and standalone mask edits compile
this reader along with edited data. The expanded configuration reads one mask
per class from the loaded promotion resource, preserving low-byte input wrapping
and the configured count bound. Its 256-ID form fits the same function extent.
The developer MIPS suite checks 2,048 retail/expanded lookup cases and unchanged
RAM; private-ROM comparison proves the retail byte match. Expanded runtime
integration still requires the dynamic promotion resource and other consumers.

Compiled function fitting uses ELF symbol extents, including local functions.
Only zero linker padding beyond every function may be removed. The compiler
test checks a `jr ra; nop` leaf: fitting its eight bytes succeeds, cutting the
zero delay instruction fails, and nonzero trailing bytes fail. This prevents
alignment padding from being confused with executable zero instructions.

`promotion/equipment-class-menu.c` rebuilds the standard-equipment purchase list
at Army RAM `0x801D2B7C` / z64 `0xBF5DC..0xBF728`. Its 308 bytes fit the original
332-byte owner. It retains the summary call, discovered/listed-class filter,
cost-before-Goth-read ordering, short-circuit capacity check, disabled marking,
count and terminator. Its expanded configuration reads the retained class flags
from the promotion resource and uses the larger list/disabled format. Tests
compare 81 scenarios with original instructions, then check high IDs, an empty
list and 255 nonempty class entries with guarded owner writes.

This default equipment-list source needs no family resource. Its owner manifest
records that distinction, allowing standalone equipment-list flag edits to
compile the list reader without introducing an LCFM allocation. Family exports
also compile it. Both paths check the original complete owner and expected C
hash, and public export/reopen tests cover the standalone path.

`classes/class-runtime.c` supplies the compiled one-time class-data load, main
heap reset and secondary-pool reset. `rebuild/class_runtime.*` combines it with
the dynamic class-data builder, checks the cartridge/RAM bounds and returns the
actual linked entries. Name edits and exports from relocated sources install it
through the public ROM exporter.
The original class/appearance/name storage becomes additional allocator pools;
the directory must be relocated and expanded to ten entries before using this
module. Boot instruction-cache preparation must cover the new code. The parent
`check_class_boot.cjs` executes this maintained source with the original allocator
in 4 MB, including one DMA, repeated resets and descriptor combinations. This
does not establish full-game peak memory or a completed expanded-class export.

The internal linker now installs those directory/call/cache changes and records
`LC05CLAS` version 1 at z64 `0x5DC80` in the retired table region. The 128-byte
descriptor stores the layout, code entries and checksums; the compiled code
occupies z64 `0x5DB14` up to the descriptor. Class payloads use an explicit ROM
allocation at or after the original cartridge end, bounded to 64 MiB. RAM remains
within 4 MiB. Reopening validates the descriptor, code, payload and name pointers.
This path is covered by engine tests and an actual linked-candidate integer MIPS
check. Project/UI exports now use it for class-name edits, including subsequent
numeric and family edits, and validate reopening. Additional class IDs still
require the remaining menu and save consumers before becoming a complete feature.
