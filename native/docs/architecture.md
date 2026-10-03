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
that every game consumer already supports that many IDs. Project/UI integration,
runtime reference updates, save handling and export placement remain separate
unfinished work; the current exporter still uses the existing retail table.

The internal class linker now installs the compiled data/boot module, updates
the cataloged address pairs and heap-reset calls, and extends instruction-cache
preparation before the resident DMA. A versioned, checksummed descriptor records
the ROM/RAM layout and linked entries. It can reopen, relink and grow its own
allocation, refusing overlap with unrelated data. Function hashes are checked
after reversing known prior address fixups, including compatible promotion
owners. The public Project/UI export path has not enabled this linker yet.

`native/tools/generate-class-relocations.py` generates addresses and owner hashes,
not instruction bytes. Its 382 direct address pairs cover class data, appearance
data and the allocator directory; ten calls cover initial load and later resets.
The generator rejects mixed address uses, long pairs and relative branches into
the middle of a pair. This does not prove an exhaustive census of all additional
class-ID consumers. Full-game memory, remaining menus and save support are still
required before enabling new classes for users.

Compiled C modules expose their actual global function addresses through the
linked ELF32/MIPS symbol table. Callers must use these entries, not assume source
order or an entry at the allocation start. Module extraction rejects nonempty
runtime data/BSS sections until they have their own allocation. Supplied headers
are isolated from stale build-directory files and host include paths.

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
