# Native Lordly Caliber

The product targets Windows and integrated graphics. C++ owns the engine and
Qt Widgets owns the desktop interface. The engine has no dependency on Widgets,
browser APIs, JavaScript, or an external Decomp checkout. The browser Editor is
a feature and format reference, not the implementation architecture.

Projects are folders containing a manifest, authored data, and assets. Sharing
archives omit the user's ROM and temporary build output. Original bytes and
unrecognized imported project fields remain available until an edit replaces
them. Builds occur on a copy and never overwrite the source ROM.

The PC application and the N64 export compiler are distinct toolchains. User
export compiles maintained game sources and generated edited data with the
authenticated KMC/GNU toolchain. Game executable bytes are not prebuilt into
the shipped application. The native runtime adapts appropriate recovered C to
explicit memory, arithmetic, scheduling, and resource services. Full-console
emulation and full-game playback are outside this increment.

The engine exposes editable domain data and deterministic execution state.
The application observes that data through Qt models and undoable commands.
Long-running loading, compilation, decoding, and export run outside the UI
thread. Rendering consumes snapshots; widget lifetime does not own game state.

`engine/class_data.*` owns the relocatable class-data input and layout builder.
It keeps raw 72-byte records, six-byte appearance entries, encoded names and
named-character aliases. Appending clones a chosen record/appearance without
inheriting a unique character alias. The builder deduplicates names, regenerates
their pointers, preserves the preceding 72 bytes used by legacy index bases,
and calculates aligned record/appearance/alias/name/discovery allocations within
an explicit 4 MB RAM interval. Its 256-entry representation limit is not a claim
that every game consumer already supports that many IDs. Project/UI name edits
and already-relocated source ROMs use this builder during export. Numeric-only
edits to a retail source still compile into its existing table. New class
creation needs the remaining consumer/menu/save work.

The internal class linker now installs the compiled data/boot module, updates
the cataloged address pairs and heap-reset calls, and extends instruction-cache
preparation before the resident DMA. A versioned, checksummed descriptor records
the ROM/RAM layout and linked entries. It can reopen, relink and grow its own
allocation, refusing overlap with unrelated data. Function hashes are checked
after reversing known prior address fixups, including compatible promotion
owners. Project/UI exports use this linker for class-name edits and for later
edits to those relocated ROMs. Managed name pointers are read-only fields.

`native/tools/generate-class-relocations.py` generates addresses and owner hashes,
not instruction bytes. Its 382 direct address pairs cover class data, appearance
data and the allocator directory; ten calls cover initial load and later resets.
The generator rejects mixed address uses, long pairs and relative branches into
the middle of a pair. This does not prove an exhaustive census of all additional
class-ID consumers. Full-game memory, remaining menus and save support are still
required before enabling new classes for users.

Class names use raw source bytes in project state and undo commands. Display
decoding never rewrites an untouched name. Edited names support printable game
characters and the two-line framing shared by all seven two-line retail names.
Name edits persist in `classNames` inside the project manifest. The table's name
editor uses Ctrl+Enter to commit, and dependent class selectors update with undo
and redo. Growing an owned ROM allocation moves it when unrelated bytes follow;
shrinking an allocation at the end removes only its owned trailing bytes.

The promotion model also supports 165–256 class IDs. Growing from the original
81 manual entries preserves existing values and gives the remaining retail IDs
no manual family by default. Appending a new ID copies a chosen template's
requirements, family and compatibility mask. Schema 2 catalogs persist the
larger assignment list. Its expanded payload contains eight requirement bytes,
four packed family bytes, one mask byte and one class-line flag byte per class, followed by aligned family
metadata. Readback validates framing and agreement between metadata and runtime
family words. The public exporter still refuses this expanded model until the
remaining game consumers and versioned ROM descriptor are integrated; it cannot
fall through to a retail-size write.

The class-line flag byte preserves the original high nibble separately from
family membership. Its known bit controls inclusion in the standard-equipment
purchase list; the Classes panel exposes that setting and preserves the three
unknown upper bits. Flags persist in new project manifests; older manifests
without them inherit the source ROM's flags. New class templates copy the flags.

Category lists and roster counts now compile from maintained C alongside the
family, discovery and equipment lists. Their source configuration supports
larger arrays, but public exports retain the retail layout until all screen and
save consumers are integrated. The relocation linker authenticates the complete
normalized code before choosing a source-built owner's reference locations;
recompiled instructions need not retain the original load/store positions.

`rebuild/class_menu_layout.*` derives the expanded Army allocation and both C
and assembly configuration from the class/promotion count. Four terminated
halfword arrays follow the original owner: the family/equipment list, category
class IDs, current counts and eligible counts. Their locations are shared by
generated readers and future installation metadata; unrelated screen storage
keeps its original positions. Layout validation requires matching table counts,
aligned nonoverlapping arrays, and resident data inside the 4 MB address range.
This allocation model does not yet install the expanded runtime or prove peak
game memory use.

Compiled C modules expose their actual global function addresses through the
linked ELF32/MIPS symbol table. Callers must use these entries, not assume source
order or an entry at the allocation start. Module extraction rejects nonempty
runtime data/BSS sections until they have their own allocation. Supplied headers
are isolated from stale build-directory files and host include paths.

Compiled modules also expose bound data-address references derived from ELF
HI16/LO16 relocation records. Function symbols establish the displacement between
object and linked text, including any linker alignment. Each reference records
its symbol, signed addend and linked instruction positions, and is checked
against the actual linked operands. This supplies reference locations for
configured runtime builds without assuming that every class count emits the
same instruction layout.

`engine/class_runtime_inventory.*` serializes configured owner hashes and data
references in a bounded, checksummed metadata frame. It accepts only declared
runtime owner extents, validates reference positions, and verifies complete code
after normalizing its declared data addresses. Reapplying the references at new
addresses is checked against independent compiler links. This metadata belongs
in ROM outside the decompressed promotion resource. The version 2 promotion
container now stores and validates that inventory after its compressed stream,
with separate bounds and checksums. The ordinary resource decoder sees only
game tables. Complete expanded installation is still pending. A partial inventory alone
does not prove that every required game consumer has been installed.

`rebuild/promotion_runtime.*` now assembles the expanded menu installation as
an internal build component. It compiles configured C, preserves authenticated
instruction ranges outside those functions, updates the category selection and
drawing operands, and grows both Army allocation and clearing to the computed
size. Actual linked data references form its inventory. Candidate checks execute
selection, name, count and sprite-call paths at high IDs with old arrays poisoned.
Version 2 reopening verifies the complete inventory and restores promotion
edits. Class and menu builds can be linked together so count changes remain
consistent. Later name-only growth relocates the saved references and is checked
against fresh compilation. Public export remains guarded until remaining
consumers and saves are integrated; these candidates are not a claim of a
playable expanded ROM.

The expanded inventory also owns the original alias readers used by class
eligibility, equipment summaries and confirmation paths. Those indexed reads
move to the expanded alias array while C readers use linked symbol references.
Discovery's required-equipment loop uses the configured class count, preserving
its four slot checks for newly added IDs.

## Migration baseline

Branch: `codex/lordly-0.5-native`.

`559b231` preserves 161 changed/new files from the Rebuild Editor worktree.
`ec90801` merges current `origin/main` Companion changes. Both source checkouts
remain untouched. Existing generated-file CRLF/whitespace is retained in the
baseline; new code uses consistent line endings.

## Dependency policy

Verify current official releases and supported Windows combinations before
pinning. Record the version, source, verification date, and any older-version
reason. Use Qt shared libraries so application and library remain separable.
Ship required notices and dependency sources/instructions with the distribution.

Every newly justified test is explained to Joe before implementation: behavior,
failure risk, inputs, execution, and independent expected result. Report the
actual invocation, outcome, and limitations after it runs.
