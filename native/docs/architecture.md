# Native Lordly Caliber

The native engine compiles a bounded adaptation of recovered `func_002A0B14.c`
from Decomp commit `9955a5cdcf73a35e4ccf57ab9bb3b6d85dab40b3`.
Motion is owned by a scene slot, independently of its occupant. Teleport and
both-axis tolerance return paths preserve an existing motion. Speed-based moves
use float distance, double scaling, truncation and a signed-halfword denominator;
fixed duration divides by the full word but stores a halfword countdown. The
current ROM supplies both tolerances and the scale. Errors reject atomically.
The planar updater preserves paused-zero cleanup and active-zero wrapping; terrain,
proximity and scheduler eligibility remain explicit responsibilities of the caller.
Seventy-eight original-MIPS creator comparisons pass, with exact float bits and
retained fields. Allocation and terrain services are controlled doubles. Separate
lifetime/failure checks pass. Test COP1 models finite nearest-rounding arithmetic,
not FCSR exceptions or denormal handling. No full-scene timing claim follows.


Actor animation keys now resolve through compiled `game/director/actor_state.c`,
a bounded adaptation of recovered `func_002A13EC.c` from accepted Decomp commit
`3de8033d0e794bcf759ccfe4c7a67d82bd9ca656`. `ActorArt` reads the current ROM's
compressed vocabulary resource at key 0x0109EEB0. An offline original initializer
run identified that resource as the source of RAM pointer 0x8022A97C; the table is
allocated per scene but its bank vocabulary comes from this shared resource.
Lookup uses the bank's halfword offset, compares animation-key times ten plus
facing, and returns the matching list position as the physical pose state.
The PC arithmetic preserves target 32-bit wrap and reports out-of-bounds walks
separately from an ordinary no-match. All 4,620 checked cases agree with the
original resolver at RAM 0x80231BBC / ROM 0x002A13EC: 2,275 known selectors, their
option-rejected counterparts, missing keys and overflow controls. No resolver
service was mocked. Frame/pose corpus checks still pass after this connection.

`ActorArt` follows the current ROM's one-based cutscene-bank directory at
resource key 0x0109F95E, then reads the descriptor's metadata, pose, configuration,
palette and art members. It preserves empty pose ranges and every counted bytecode
record. Frame assembly preserves signed layer origins, scale fields, flips and
variant fallback; metadata crops reject if they would remove visible pixels.
The reader follows an edited directory entry rather than substituting catalogued
keys. All 68 banks, 2,275 pose programs, 5,696 referenced frames and 16,768 layers
match the independent parser/renderer's records, dimensions and pixel hashes.
A native six-frame Bank-30/state-12 walk contact sheet has been inspected. Pose
selection and execution, actor movement and camera projection remain separate
unfinished connections; these are frame/resource checks, not scene playback.

`SpriteObject` decodes native 0x5554 sprite containers, preserving original
headers and padded color/alpha/palette lanes. Variant deltas support nibble,
byte and halfword subtraction plus RGBA32 XOR; discriminator cycles and missing
base lanes reject. Pixel conversion supports embedded/external palettes and
separate I4/I8 alpha. An out-of-range requested child selects child zero and the
result records that fallback explicitly. Dimensions, source extents and decoded
memory are bounded. Manual color/alpha/delta/fallback/malformed cases pass, and
7,366 actor resource/palette combinations with 22,867 child variants match the
independent prior decoder's lanes and pixels. This covers the 68 catalogued
actor-art banks; it does not yet connect actor state, pose scheduling or scene
projection, and does not claim original RSP blend equivalence.

`DirectorSession` now owns the single saved parent stream used by the recovered
loader. Nested continuations replace the active child; return restores the first
parent's saved cursor. Replacement scenes clear the saved slot and resume at zero.
The engine reads all five continuation resources through the current ROM directory
at key 0x0189099E; their decoded hashes and lossless encoding match the independent
catalog. Copied loader source `func_00283FA8.c` is from accepted Decomp commit
`7f47ce6556babae08277f3997c9f647fa3f96149`. Its translation loop is adapted into
compiled `stream_words.c`: every tagged word is replaced by a supplied 16-bit launch
value. Missing inputs remain explicit, and the host rejects them before changing
any stream words or replacing the current stream. A supplied zero is valid.
Synthetic resource-directory checks cover nested continuation ownership, return,
replacement, missing context and suspension at one-instruction budgets. These
session checks follow the recovered loader contract; they are not yet a full
original-loader instruction comparison or scene playback check. Domain services
and update-tick ordering still belong to the next execution layer.

The PC engine also compiles `game/director/dispatch_core.c`, a bounded control
adaptation of the recovered dispatcher. It preserves barrier cursor commits,
local handoff state, signed query scans, labels and registered-counter arithmetic.
A query disabled by the current mode leaves its operand words executable, as the
original does; it is not skipped using the structural grammar width. Host budget
or missing-service suspension retains local state without pretending to finish a
native tick. Domain operations use explicit callbacks; stream transfers
return a distinct boundary handled by DirectorSession.

Thirty-two control cases agree with the original MIPS dispatcher and helpers in
4 MiB test memory: return result, committed cursor, counter, tracked-object state
and query-producer calls. Code placement is read from overlay descriptor 14
(ROM 0x281860..0x286BD0); the dispatcher executes at RAM 0x802282B8. The external
query producer is a controlled double. FPR save/restore is modeled as opaque pairs;
these cases do not test floating-point actor/camera services. Separate checks cover
missing services, truncated input, execution-budget suspension and transfer
boundaries. This is control-flow verification, not full scene playback.

The Cutscenes workspace now searches 1,498 Director resources and 366 image
assets, displays current-ROM command words and background requests, and previews
signed background layers or individual raw frames/palette banks. Image decoding
runs outside the UI thread with one active decoder and a replaceable pending
selection; stale completion cannot overwrite a newer selection. Zoom/pan,
layer visibility and frame selection change only the preview. The scene bounds
exclude Qt selection outlines. Source commands are currently read-only.

`SceneImages` reads environment, foreground and scene-group selectors from the
current ROM. All 191 rows match the independent catalog, and a redirected table
entry selects the changed image. Group 10's format-5 wrapper selects an embedded
archive after its B5 header/record; it is not an ordinary archive key. Its six
members are assembled using coordinates read from the current ROM records.
Raw CI4/CI8/I4/I8/RGBA5551 layouts preserve frames and palette banks. All 366
individual images plus the compound assembly agree with the earlier decoder:
323 non-HUFF exact hashes and 43 HUFF previews with no channel differences in
this corpus. Preview equivalence is not original RSP/rendering equivalence.
Scene-mode references are used only while the current stream hash matches;
otherwise the preview requires an explicit mode, except class-owned group
preload. Foreground selection remains an independent launch input. This is an
image/command workspace, not yet actor playback or a cutscene exporter.

The background decoder also reads K palettes, signed B5 layer records and HUFF
entropy streams in native C++. HUFF uses the native coefficient layout and scale
with a floating-point IDCT; original RSP fixed-point rounding remains approximate.
B5 preserves layer origins and exposes separate records, while its composite uses
record order and interprets the separate I8 plane as preview alpha. Those choices
are reported as preview limitations. Bounds and memory budgets reject malformed
images. Hand-specified alpha, palette and constant-HUFF fixtures pass; 246 retail
non-HUFF images match the independent browser decoder exactly, and all 43 HUFF
previews also match exactly in the checked corpus. This comparison establishes
compatibility with the prior preview, not pixel equivalence to the original RSP.
The decoded Graduation hall has been rendered and visually inspected. External
B5 members and raw-image layouts are now integrated through SceneImages.

The native Director reader follows the selector directory at z64 0x1F3CA84,
deduplicates resource keys, decodes the current ROM's custom-LZ envelopes and
preserves every command and operand word. All 1,498 retail streams behind 1,693
selector slots agree with the earlier independent decoded hashes; 193,771
commands round-trip without changes. The 154-command grammar is regenerated
from the maintained recovered dispatcher C, checked against semantic operand
descriptions from the browser feature reference. No user Decomp checkout or
JavaScript runtime is required. `LordlyCaliber-cli --scene 01FA8BF4` inspects the
282 commands at that z64 payload address (selector 1066, resource class 1).

Recovered sources `game/director/func_00284288.c` and `func_002861C8.c` come from
Decomp commit `8012b84aec7c04dd61e383cbf40d5b2ae5dcd2a8`. Their original 32-bit
pointer casts are retained for source provenance. `control_flow.c` adapts the
condition scanner to bounded arrays and is compiled into the PC engine. Raw
word scanning, signed comparisons, nesting and native result -1 are preserved;
bounded host exhaustion is a separate error and leaves caller state intact.
Hand-specified comparison, nested-bridge, operand-marker and malformed-input
cases pass. This is not a whole-dispatcher execution or full scene playback
claim. The original dispatcher is currently the structural grammar source;
its service calls, render paths and timing still require PC adaptation.

The engine decodes native `64` image containers into RGBA bytes without Widgets
or browser dependencies. RGBA5551, RGBA32, CI4, CI8, I4 and I8 preserve the game's
padded storage rows while producing only the visible pixels. Dimensions, payload
and palette bounds are checked. A manually specified CI4 fixture checks padding
and colors; an independent browser decoder agrees on dimensions and pixel hashes
for all 104 supported image members in the source ROM. A decoded `s_map04.n64`
map image has been rendered and inspected. The scenario map reads compact native
resource collections for world bounds and one-based placement points, and follows
the game's image selector to a named map archive. All 64 post-load bounds agree
exactly with existing independent captures. The desktop workspace draws initial
squads, waypoints, sites and treasure on the coordinate grid, shows route links,
supports zoom/pan and overlapping-object selection, and opens the corresponding
field editor. Squad/waypoint/treasure movement uses the existing model and undo
commands, including coordinate-mode conversion and its nonzero-Z sentinel.
Persistence, compiled export, checksum and reopening are checked from UI edits.
The ROM terrain image remains a separate reference until its projection onto the
grid is verified. Special placement differences and dynamic group behavior are
explicit limits; this is an initial-placement editor, not live mission execution.

The native archive writer produces LH5 streams with an 8 KiB match window and
bounded block sizes. Rebuilding a member preserves its original header metadata
and updates lengths and CRCs. Native decoding and the independent browser decoder
agree on repeated data, window boundaries, multiple blocks, and rebuilt squad
and scenario archives. The ESET codec preserves unnamed fields and trailing data
while regenerating section offsets and the final deployment's shared node framing.
All 63 retail ESET files round-trip byte-for-byte; edited framing also passes the
browser codec. `ScenarioResources` now rebuilds complete ESET and enemy-library
resource envelopes. The original scenario table at z64 0x654A0 uses nine-byte
rows; byte eight selects an ESET member. Its 65 entries match the existing
runtime catalog (including key zero). Edited scenarios get separate appended
members; subsequent rebuilds compact them while preserving the original 63.
The original resource links select relocated collections, without a code hook
or fixed DMA windows. LC05SCEN at z64 0x27C4400 authenticates the owned allocation,
selectors and baseline directory. Growth preserves later allocations. The engine
checks original readers and rejects unrecognized archive redirects. Original
MIPS dispatcher, framing and LH5 code load all 65 selected resources plus the
grown enemy library in guarded 4 MiB test memory. Allocation and lower transfer
services are simulated; full-game loading and peak heap use remain unverified.
The scenario model stores a local 35-byte squad record for each deployment. Its
project representation omits allocated EDAT IDs; export deduplicates or appends
records while retaining the complete loaded enemy library. Separate ESET members
allow independent edits to scenario keys that originally shared a resource.
The original automatic-alliance cases become explicit deployment flags when
records move. The desktop page edits cohorts, formation, equipment, base levels,
placement, initial orders, supplies and drops, and supports adding/removing authored
squads with undo. Formations and carrying capacity are checked when their inputs
change, including class-size and capacity edits in the Classes page. Unrelated
original values remain intact. Folder/archive persistence and compiled export
reopening cover added classes and relocated templates. Applying untouched UI
controls to every loaded deployment preserves project data. The route page also
edits direct orders, waypoints, dormant gates, condition predicates and squad
groups. Sequential node/condition IDs and known incoming references are checked;
renumbering rejects ambiguous overlaps and extended forms. New rows cannot
silently resolve an existing dangling reference. Original condition IDs can be
consumed by mission scripts, so their predicates can change but only added
conditions can be deleted. Unknown payload bytes survive untouched controls;
unclassified extended gate bytes remain visible and preserved. Route tests cover
reference rejection/remaps, limits, persistence and resource rebuild/reopening.
Browser scenario projects (versions 1–4) and scenario sections in the main project
now import explicit base-level intent, added squads, copied level records and
ordinary squad overrides. Source hashes, ownership claims and aliases must agree;
older resource paths can migrate under matching hashes. Native restore operations
remain authoritative over retained browser state on reopening. Unknown project
sections remain pending and block export. An independent fixture
uses the browser's authoring and project writer, then checks native persistence,
compiled export and reopening. Alternate-overlay key 54 remains unsupported.
Site and treasure editing now uses a separate world model. Global site metadata,
scenario-specific allegiance, treasure item/coordinate changes and added treasures
persist, undo and compile during ordinary export. World resource collections grow
through their original directory links; original archive ordinals and shopcsv are
preserved. The Shops reader follows that directory. Added treasures receive unused
collection flags and respect the original 16-entry scene arrays. Native and original
MIPS checks cover relocation, cache/free/reload, flag IDs through 255 and combined
shop/squad/world exports. The desktop controls preserve untouched original data.
See [world-resources.md](world-resources.md) for evidence and limitations. Browser
site/treasure imports now retain their original shared-resource behavior, validate
resource identities and preserve explicit intent. Independent browser-exported
payloads match native import, persistence and compiled export. The coordinate map
workspace is implemented; terrain registration, dynamic special-scenario placement
and full-game validation remain unfinished.

Carrying capacity is class body byte B59, at offset 71 in the full 72-byte native
record. The initial implementation in f49d509 mistakenly read full-record offset
59 (rear action). The corrected reader and export change detection use the named
ClassData offset. Independent browser decoding now agrees for all 165 class
contributions and 360 distinct selected squad records, including authored data.

The class linker can reserve a maintained resident extension after expanded
save code, or at the beginning of reclaimed class storage when saves are not
expanded. Descriptor version 4 records its address, size and CRC. The allocator
returns only the remaining suffix to the heap; an exact-capacity allocation
omits that pool. The original permanent DMA loads the extension and existing
instruction-cache preparation covers it. Rebuilds must supply the extension
again or explicitly remove it. Allocation, relocation when adding saves, bounds,
reopening, and boot/reset instruction checks are covered in guarded 4 MB memory.
The custom encounter C core and its register adapters now use this allocation
in installed ROM probes. All twelve caller owners compile from maintained
assembly, including the shared dispatcher and reward owner. The global/scenario
rate and class-reward readers authenticate their code after reversing the
declared custom call sites. LC05NEUT records the ROM profile index, packed
profiles, resident allocation and linked entries. Install/reopen/removal,
installed call-site execution and profile-to-reward instruction flows are
checked in 4 MB memory. Original actor services and PI hardware are simulated;
full-game behavior remains unverified. The native project now stores independent
members, formation positions, per-member equipment, card text, persuasion,
retreat and weighted rewards. Browser cohort imports expand into these members.
The ordinary exporter reserves the resident module, compiles the ROM library,
and installs its callers with the class/save, reward and rate owners. Re-export
reuses owned space when it fits; growth preserves later allocations. Removing
the last squad restores the callers and releases the module. Public export tests
cover 65 profiles, a newly added class, moving behind expanded saves, unchanged
re-export and removal. The desktop page provides searchable squads and classes,
per-member settings, a clickable formation grid with large-member clearance,
persuasion/retreat/reward controls, and undoable create/copy/restore/remove.
Qt checks exercise those controls and atomic rejection of invalid edits. Rendered
pages have been inspected. Selection now declines an authored squad before
staging its encounter when there are too few free transient characters or no
free source record in slots 30..49. This avoids the original allocator's unchecked
fallback to player source 29. Character allocation uses slots 1..98, matching
the transient reset at z64 0x45F30; slot 99 is outside that reset and IDs 100+
have a different reference meaning. The original save/load routines cover 100
52-byte scratch records, not 120. Earlier instruction fixtures incorrectly
allowed records 118/119; corrected fixtures guard that boundary. Installed
dispatcher tests verify no spawn or record writes on capacity rejection.
The materializer also preflights every member before construction. The interval
between selection and materialization assumes the original modal transition
does not allocate other scratch characters; injected changes during that
interval and full battles are not yet proven.

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
edits to a retail source still compile into its existing table. New class creation now composes the expanded menus, save conversion and
description routing in the normal export path.

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
class-ID consumers. Expanded menus and save support now use separate authenticated owners.
Peak full-game memory use remains unmeasured.

Class names use raw source bytes in project state and undo commands. Display
decoding never rewrites an untouched name. Edited names support printable game
characters and the two-line framing shared by all seven two-line retail names.
Name edits persist in `classNames` inside the project manifest. The table's name
editor uses Ctrl+Enter to commit, and dependent class selectors update with undo
and redo. Growing an owned ROM allocation moves it when unrelated bytes follow;
shrinking an allocation at the end removes only its owned trailing bytes.

The promotion model also supports 165ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ256 class IDs. Growing from the original
81 manual entries preserves existing values and gives the remaining retail IDs
no manual family by default. Appending a new ID copies a chosen template's
requirements, family and compatibility mask. Schema 2 catalogs persist the
larger assignment list. Its expanded payload contains eight requirement bytes,
four packed family bytes, one mask byte and one class-line flag byte per class, followed by aligned family
metadata. Readback validates framing and agreement between metadata and runtime
family words. The public exporter compiles this model with the matching class count,
source-built menu owners, save module and description routing. Count mismatches
fail before writing; expanded data cannot fall through to a retail-size write.

The class-line flag byte preserves the original high nibble separately from
family membership. Its known bit controls inclusion in the standard-equipment
purchase list; the Classes panel exposes that setting and preserves the three
unknown upper bits. Flags persist in new project manifests; older manifests
without them inherit the source ROM's flags. New class templates copy the flags.

Category lists and roster counts now compile from maintained C alongside the
family, discovery and equipment lists. Their source configuration supports
larger arrays, and expanded public exports use the configured screen layout. The relocation linker authenticates the complete
normalized code before choosing a source-built owner's reference locations;
recompiled instructions need not retain the original load/store positions.

`rebuild/class_menu_layout.*` derives the expanded Army allocation and both C
and assembly configuration from the class/promotion count. Four terminated
halfword arrays follow the original owner: the family/equipment list, category
class IDs, current counts and eligible counts. Their locations are shared by
generated readers and future installation metadata; unrelated screen storage
keeps its original positions. Layout validation requires matching table counts,
aligned nonoverlapping arrays, and resident data inside the 4 MB address range.
The normal exporter now installs this layout. It does not prove peak
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
game tables. Reopening requires the complete declared owner inventory. A partial inventory
is rejected; it cannot substitute for an installed expanded runtime.

`rebuild/promotion_runtime.*` now assembles the expanded menu installation as
an internal build component. It compiles configured C, preserves authenticated
instruction ranges outside those functions, updates the category selection and
drawing operands, and grows both Army allocation and clearing to the computed
size. Actual linked data references form its inventory. Candidate checks execute
selection, name, count and sprite-call paths at high IDs with old arrays poisoned.
Version 2 reopening verifies the complete inventory and restores promotion
edits. Class and menu builds can be linked together so count changes remain
consistent. Later name-only growth relocates the saved references and is checked
against fresh compilation. The public exporter now composes these owners with class tables, save support
and description routing. Isolated instruction checks are not a full-game boot
or playthrough proof.

The expanded inventory also owns the original alias readers used by class
eligibility, equipment summaries and confirmation paths. Those indexed reads
move to the expanded alias array while C readers use linked symbol references.
Discovery's required-equipment loop uses the configured class count, preserving
its four slot checks for newly added IDs.
Its special secondary-class rule remains limited to original IDs 42ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ164;
new IDs use their authored compatibility and requirement tables.
The Army action checks likewise apply their special-class restrictions only to
original IDs 81ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ164. Both alias routes retain their original default-class
selection; appended IDs take the ordinary-class path. The unrelated 128-entry
squad-index bound remains unchanged.

`game/classes/save-format.c` is shared source for desktop and game record
conversion. It validates a legacy record, inserts 32 discovery bytes before
the suspend-only payload, and writes a distinct signature with the original
checksum algorithm. The eleven legacy discovery bytes come from their actual
unaligned position in the packed payload; remaining bits start clear. Options
and a cleared suspend record retain their original meaning. Invalid records and
short buffers fail before any write. This record-level component is tested
against the original codec and checksum instructions. SRAM and Controller Pak
dispatch and runtime callbacks are installed by the coordinated class export. Whole SRAM-image conversion now moves records
in reverse order to avoid overwriting unread bytes, retains invalid records as
invalid, and preserves options and unallocated tail bytes. A checksummed
`LC05SRA1` marker occupies 16 bytes after the expanded suspend record, allowing
repeat conversion to leave the image unchanged and damaged markers to be
rejected before writes. Conversion alone performs no cartridge write.

Expanded class builds now compile and install the complete SRAM dispatch from
`save-runtime.c`. Normal, suspend and options records share the format definitions;
the existing game callbacks and device flush remain the services below them.
Reads normalize only the cache, explicit writes mark it dirty, and a damaged
layout marker blocks mutation. Individual bad records can be initialized without
erasing valid neighbors. Save counters come from the prior valid record, removing
the original dependency on recycled allocation contents.

The module occupies a measured prefix of the former class table at resident RAM
`0x80187F00` (z64 `0x5DE00`). Every heap reset reclaims only the remainder. Cache
preparation covers the module, including signed low-half address carry. Version 2
of the class descriptor records its size and integrity checks for code, entry
dispatch and the callback directory; version 1 remains readable. Rebuilds compile
the save module again when class names move the discovery allocation. Isolated
instruction checks execute the exported boot/reset and save dispatch with guarded
allocations and synthetic SRAM. Public new-class exports now use this module. Full-game behavior remains
unverified by these isolated checks.

Controller Pak import/export retains the existing device workflow and 0x1900-byte
file allocation. Two assembled adapters call maintained C to validate either
legacy or expanded records, convert discovery data, and seed cartridge checksums
for the chosen destination slot. Pak checksums retain the original zero seed.
Import leaves its input file intact; corrupt and truncated records cannot write
SRAM. Export clears unused file bytes. Save installation version 2 adds both
whole Pak owners to its integrity checks; earlier SRAM-only metadata upgrades
after authenticating the original Pak owners. The adapter placement is supported
by full-function direct signatures in the parent runtime atlas, inputs 0499 and
0503, with delta 0x80129520. Candidate instruction tests check helper arguments
and both continuation branches, separately from the synthetic device data tests.

Save installation version 3 also extends the existing new-game state-clear call
to clear the relocated discovery bitmap. The original 0x94-byte state clear and
neighboring state remain intact. It owns only that call inside the authenticated
resident initializer, and checks execute the exported call with all discovery
bits initially set and both allocations guarded.

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

Class description resources can now grow beyond the original compressed slot.
The exporter compiles authored entries, allocates a bounded cartridge resource,
and updates both resident readers: the bank-key table and shared-help expansion
code. A checksummed LC05TEXT descriptor occupies the beginning of the former
resource; the rest of that slot remains untouched. Reopening validates the entire
resource and both full owners, accounting for independent class-data relocation.
Later growth moves around unrelated allocations. Shared help entries retain their
indices and raw bytes. New-class description index routing is installed with the class runtime.

The Project model now appends classes atomically with their cloned appearance,
raw record, name, description and promotion state. New named-character aliases
start at zero. New description entries start at index 225, after all 225 retained
retail entries; class ID165 maps to225. Project manifests persist the baseline
assets and subsequent edits separately. Added records have no original ROM
offset. Folder/archive reload and whole-operation undo cover all 91 available
new IDs. The Classes workspace offers creation from the selected template; the
normal exporter now builds its runtime dependencies together.

Class descriptor version 3 adds the description adapter entry at byte 100. The
maintained 64-byte assembly adapter fits after the three C loader functions
inside their existing resident allocation. It maps class IDs 165..255 to text
entries 225..315 at the ordinary bank5 reader. Shared HELP references call the
lower reader directly and keep entries 200..222. Magnus's Prince exception uses
request index 256 and maps back to original entry 224. Both affected function
owners are authenticated in full; descriptor/code checks cover the adapter.
No additional resident allocation or Expansion Pak requirement is introduced.

Normal export now coordinates class data, configured promotion/menu code,
description resources/routing and save conversion for 165..256 entries. It
compiles maintained sources during export, validates all owned allocations,
updates CIC-6102 and reopens every edited domain. The 256-class public workflow
passes creation, folder/archive persistence, undo, export/reopen, growth of a
previously exported ROM, and subsequent promotion/description edits. Instruction
checks execute the original description selector for all 256 IDs plus the special
Prince description. These checks use 4 MiB modeled RAM and synthetic SRAM/Pak;
full-game cold boot, peak live memory and hardware-device behavior remain unproven.

Description edits and exports enforce the original 0x500-byte scratch buffers,
including the final CRLF and retained HELP expansion. Help cycles and missing
targets fail before mutation; failed edits retain the previous bytes.

The native archive reader supports bounded LH5 decoding with per-instance
Huffman trees and an 8 KiB dictionary. LHA member lookup uses filenames and
validates decoded size and the stored CRC16, avoiding dependence on a shifted
archive index. Shop and town archives pass their independent stored checksums;
malformed trees, short input and altered member data fail.

Shops now has a native membership model:36 slots with immutable empty slot0,
35 real shops, ordered equipment IDs1..277 and consumable IDs1..44. It retains
original stock separately and validates the existing50/30 rebuild capacities,
uniqueness and record ordering before mutation. Native project folders and
archives persist edits. Town labels use actual names, excluding objective
records; archive group numbers are not presented as mission numbers. The Shops
workspace searches towns/items and supports add/remove/reorder, copying stock,
restoration and undo. Browser projects migrate partial shop edits while retaining
their original import data. Explicit native state prevents restored stock from
being overwritten by an archived import when a project reopens.

Shop export compiles generated stock data and maintained MIPS sources during
export. Thirty consumable halfwords fit the original60-byte array, while the50
equipment words remain unchanged. All15 width/stride/read changes are covered by
full-owner hashes, normalizing the overlapping class references independently.
The producer DMAs one176-byte row into its existing512-byte temporary allocation;
no persistent data table or code module needs additional game RAM. The original
cleanup remains responsible for releasing resources. A64-byte LC05SHOP descriptor
at z64 27C4000 records the appended6336-byte data allocation and checksums.
Unknown existing shop patches are rejected before exposing retail stock.

Descriptor version2 retains the final consumable category even when empty. Its
88-byte selection guard fits the producer's otherwise-unused space and prevents
confirmation for empty or out-of-range selections. Version1 builds reopen and
upgrade on export. Tests execute exported producer/splitter/reader/category and
confirmation paths with guarded4 MiB RAM, including empty shops and maximum
stock; the normal export also composes with added classes and reopens every row.
Full-game rendering, navigation and peak memory remain outside those proofs.

Consumable metadata uses the actual 45 records of 12 bytes at z64 0x645CC
(US Rev1 adds 0x20), preserving each name pointer. The effect model separately
owns ten magnitude/range settings in the US Rev0 resident routine at ROM
0x41098, RAM 0x8016B198. Its 345 maintained assembly instructions compile during
export within the original 1,380-byte extent. No added RAM or tail-code hook is
needed. The complete routine is authenticated after normalizing its 16 parameters;
the original 19-entry dispatch table is independently authenticated. Heal Pack's
two magnitude paths must agree. Shared stat boosters use one range.

The accepted routine currently has ASM, not accepted C. Its migration generator
checks every instruction against the private reference ROM, converts branch targets
to labels using actual instruction operands, and gives external calls explicit
linker symbols. GNU 2.6's automatic VR4300 multiply workaround is disabled only
for this instruction-position-preserving build; the original spacing is retained.
The compiled retail routine must match all original bytes. Edited builds must
differ only in the modeled parameters. Other builds retain their assembler flags.

Effect changes and optional healing-description number updates are one undoable
transaction. Only a single recognizable HP amount is replaced; other encoded text
bytes remain intact. Ambiguous custom descriptions reject automatic replacement
and can instead be edited directly. Browser magnitude/range imports migrate to
canonical native state; retained import data cannot revive a restored effect.
Normal export composes effects with class relocation, Shops, records and text,
then reopens and checks every loaded domain. Instruction tests execute authored
query/apply effects, limits and special-target pools in guarded 4 MiB RAM with
a controlled random service. They do not establish full-game menu or RNG behavior.

The encounter pool owner is 900 bytes at z64 0x141ED4: 45 pools of ten byte pairs.
This corrects the browser model's 40-pool bound. The leading four bytes and adjacent
creature reward table at 0x142258 are retained. Scenario metadata keys59,63,64,65,66
reference pools41..45. The workspace shows all pools and their actual metadata
references; high-confidence labels are generated separately as navigation aids.
Each pair validates against the project's current class count, including added IDs.

The two32-byte terrain tables at z64 0x141E80/0x141EA0 retain their original allocation.
Disabling a slot sets its chance to zero. Inspection of the original selector
shows no zero-slot guard after the terrain roll passes: allowing a positive rate
with a zero slot would index the preceding pool's last pair. Such imports are
rejected. This refines the earlier general description that slot zero alone disables
encounters in the parent neutral reference. Complete dispatcher authentication
normalizes only the known global-rate operands and recognized gate branch forms;
foreign runtime hooks are not silently presented as retail behavior.

Pool and terrain exports compile generated C arrays and compare/reopen the result.
Tests compare the entire image with independently constructed table edits, including
pool45 and both neighboring allocations. Executed original instructions cover pool
selection, unsigned class IDs and spawn markers, plus terrain thresholds and the
existing item-based rate doubling. Full encounter materialization and gameplay are
outside that check. Remaining imported encounter features stay retained and block
export until their native implementation is available.

Global rates are now a separate model within the encounter workspace. Its divisor
and two pass counts are unsigned32-bit values, with a nonzero divisor and each
count bounded by that divisor. Normal and alternate rolls can independently be
disabled or always pass. The existing state-flag17 selection and RNG calls remain.
The maintained dispatcher assembly has an exact retail mode and a native mode:
native mode replaces14 instructions at z64 0x13C1F8..0x13C230 with full-width count
selection and an unsigned remainder comparison. It occupies the same1,084-byte
function allocation and retains all other instruction positions. No tail hook,
additional resident code allocation or external source checkout is required.

Source generation authenticates every instruction against accepted
func_0013C060.s. The native source catalog records full normalized owner hashes,
six parameter halves and explicit external service symbols. Loading accepts the
authenticated retail forms and this native form; edited export compiles source
and reopens its parameters. Earlier projects without the nested globalRate field
retain the loaded ROM's rate or migrate retained browser settings once.

The original RNG at RAM80092A60 / z640x22E60 updates a30-bit state:
next=(4*state*state+5*state+1) mod2^30. Its complete44-byte function is authenticated
before interpreting rate percentages. The display counts the share of values in
that range whose remainder passes, including modulo imbalance and divisors larger
than the RNG range. It does not claim an observed per-frame encounter frequency
or prove the RNG's full-period distribution. Tests execute576 original RNG steps,
and108 state-flag/RNG boundary cases across the original and five native builds.
Whole-image export checks retain all bytes outside the dispatcher, and project
and UI tests cover legacy imports, restoration, linking, invalid values and undo.

Creature rewards retain the original296-byte table at z640x142258 and derive
effective class outcomes using the game's first matching halfword key. A zero
key terminates the search; unlisted classes select that row's three outcomes,
even if those outcomes are nonzero. The pad byte is the key's high byte. This
preserves behavior that an all-zero-row sentinel test would miss. Partial browser
imports update only affected effective classes, preserving unrelated native
rewards. Canonical native state prevents retained imports from undoing restoration.

The rebuilt table covers all256 byte-sized IDs in4,096 ROM bytes. Each16-byte row
contains three big-endian reward halfwords, three unsigned weight halfwords and
four reserved zero bytes. Weight totals must be positive; zero-weight outcomes
are unreachable. Reward zero with positive weight is a no-reward outcome.
Equipment uses bit15 with IDs1..277; consumables use IDs1..44. The original
inventory's40 consumable entries are capacity slots, not a maximum item ID.

The maintained mission-mode owner is accepted ASM func_00101DDC.s; accepted C
is not available for it. The generation check compiles all1,097 instructions to
the exact4,388 original bytes. Overlay descriptor7 proves z64101DDC maps to
RAM801AD69C; nominal ASM comments are not used for placement. Native mode
replaces only the120-byte class-reward selector at z6410245C..1024D4. It calls
the existing synchronous DMA wrapper for one row, calls the existing RNG once,
and selects a reward by cumulative weights. All other instruction positions stay
unchanged, including the normal mission-drop path and later reward eligibility.

The row occupies frame offsets0x20..0x2F inside the original0x50-byte stack
frame. This region is otherwise unused, remains at least8-byte aligned, and
does not share cache lines with saved registers starting at0x38 for either o32
stack alignment. The existing DMA wrapper uses its own temporary0x40-byte frame.
No added persistent RAM, module loader or tail-code hook is needed. PI transfer,
cache hardware and full-battle behavior still require game-level validation.

A64-byte LC05REWD descriptor at z6427C4100 authenticates the appended16-aligned
data allocation, full rebuilt owner and header with CRC32. The data range is
bounded by0x2800000..0x4000000 and reused on subsequent exports. Two address
halves are normalized for the maintained owner hash; the overlay descriptor and
DMA wrapper are also authenticated. Unrecognized existing selection code fails
before exposing misleading retail settings. Data and code compile at user export.

Validation covers no-op byte identity, independent retail lookup expectations,
folder/archive round trips, canonical restoration, partial browser imports into
an already rebuilt ROM, full-image conservation, checksums, allocation reuse,
tampering, collisions and composition with added classes. Desktop checks exercise
the controls and undo. Instruction checks execute2,560 retail and2,978 native
cases across all256 IDs, zero/max weights and both stack alignments, including
the original DMA wrapper and guarded caller state in4 MiB modeled RAM. The lower
PI transfer and RNG call are controlled services in the reward selection check.

Scenario rates use two independent optional overrides for each byte-sized runtime
key. A branch either inherits the global divisor/count or stores its own unsigned
32-bit divisor/count, including zero and full pass counts. Area Investigation is
the state-flag17-set branch; the mission branch is flag clear. The workspace keeps
the two branches separate, with undo, restoration and explicit inheritance.
Older browser inherit/override/disabled records import by direct scenario key;
the former shared-slice field does not redirect an override to another key.

Export materializes256 sixteen-byte effective rows in ROM, followed by256 flag
bytes recording which branches are explicit. Inherited values must match the
global settings stored in the64-byte LC05RATE descriptor at z6427C4200. The
appended4,352-byte data allocation is16-aligned, bounded to the cartridge range,
checksummed and reused. Global edits rebuild inherited rows while preserving
explicit overrides. Removing the final override rebuilds the global-only source,
clears the authenticated descriptor and frees its data: truncate only when that
owned range is at the end, otherwise fill just the owned range with0xFF.

The dispatcher has a third maintained source mode, still1,084 bytes at its
original placement. At z6413C1E8..13C230 it loads the staged key, DMAs one row into
frame offsets0x20..0x2F, queries state flag17, and loads the selected divisor/count.
These temporary bytes are overwritten by the original permutation initialization.
The remaining roll instructions occupy z6413C270..13C290, formerly unreachable
division checks. The candidate count starts30, decrements once per rejected
candidate and loops only while nonzero; its divisor is therefore1..30. The RNG
returns a nonnegative30-bit value, so zero and signed-overflow exceptions cannot
occur. The candidate draw now jumps over the helper with an unsigned division in
its delay slot, retaining the original remainder instruction at13C290. The global
helper's final jump shares that harmless remainder delay instruction; the next
candidate draw overwrites s4 before use. No other instruction positions move.

Source generation still byte-matches retail and retains the prior global-only
source hash. Scenario loading authenticates its full normalized owner, descriptor7,
DMA wrapper, effective rows, flags and global values. Checks cover project and
browser round trips, restoration, independent inheritance, global re-export,
removal with later allocations intact, corruption/collisions and composition with
added classes and weighted rewards. Executed instructions pass13,750 gate cases,
300 original/rebuilt candidate draws and930 removal cases for counts1..30 in
guarded4 MiB RAM. This does not establish PI/cache hardware or full-game timing.
