# Scenario sites and treasure resources

The US Rev 0 directory at z64 `0x2743C78` has three keys. Key values are envelope
ROM offsets minus `0x594280`. Slot 0 contains 40 maizo treasure members, slot 1
contains three spmap members, and slot 2 contains 61 members: ktenmain at index 0,
59 SCINCSV files at indices 1..59, and shopcsv at index 60. The middle collection
is preserved; its meaning is not inferred here.

The table at z64 `0x13C550` has 12-byte rows. Byte 5 is an unsigned site selector
(zero means no SCINCSV); byte 6 is the unsigned treasure selector. The site caller
at z64 `0x1278D4..0x127934` and treasure caller at `0x196138..0x196174` use resource
key `0x021AF9F8`, directory slots 2 and 0, and command -3. Their release command
is -17. These differ from ESET's -4 path. Descriptor 7 places the table and site
caller at RAM `0x801E7E10` and `0x801D3194`. The treasure caller's overlay placement
has not been resolved in this work; its straight-line argument setup is tested
at an explicitly arbitrary scratch address.

`WorldResources` keeps the original member order and compressed bytes. Modified
scenario resources are appended, with independent selectors for originally shared
keys. Rebuilding compacts these appended variants. Site-table edits rebuild index
0. Shopcsv remains at index 60 with unchanged bytes. The Shops model follows the
resource link, so relocated copies do not create ambiguous filename searches.

LC05WRLD at z64 `0x27C4500` records a checksummed allocation and baseline selectors.
Payload storage grows from 40 MiB up to the existing 64 MiB cartridge limit. Growth
behind unrelated data relocates the allocation; only owned bytes are cleared.
The middle directory key, source ROM, and other resources remain intact.
Reader hashes come from `native/tools/generate-world-resource-owners.cjs`, which
authenticates the user's canonical ROM and emits metadata only.

`engine_tests worldResources` checks independent aliases, archive preservation,
repeated edits/restoration, corruption, source mismatch, foreign allocations and
Shops readback. `world-resources-mips.cjs` runs original dispatcher/cache/framing/LH5
instructions against the candidate: 309 loads include cache hits, free and reload
for every member of both rebuilt collections. All 65 selector setups are checked.
RAM is bounded to 4 MiB with allocation guards. OS, allocation and PI services are
simulated. These checks do not establish full-game loading or peak memory use.

`ScenarioWorld` keeps complete 28-byte global site records and per-scenario
treasure/SCINCSV data. Site membership cannot change independently of mission
geometry, and inactive descriptor tails are retained. Editing a site changes its
global name, population, morale/objective, facilities, type or shop assignment;
allegiance edits affect only the chosen scenario. Untouched controls preserve raw
names, unclassified values and descriptor addends. Explicit allegiance changes
use neutral `0x0000`, enemy `0x0004`, or allied `0x2012`.

The original site initializer at z64 `0x1279A8..0x1279C8` **adds** the descriptor
to `(capabilities << 8) | type`. Clearing only `0x2000` from allied `0x2002` leaves
the allied low bit set. This explains why the older simple bit-clearing rule is
unsuitable for editing. Retail also uses allied `0x2012`. Existing descriptors
remain unchanged unless the user chooses a new allegiance.

Treasure records contain a collection ID, X, Z, item table and big-endian item ID.
The loader at z64 `0x196174..0x196228` writes the count at RAM `0x801F0FE2`, then
16 halfword items at `0x801F0FE4`, 16 X bytes at `0x801F1004`, 16 Z bytes at
`0x801F1014`, and 16 IDs at `0x801F1024`. The adjacent arrays establish the
16-entry scene limit. New entries receive an unused nonzero ID across all loaded
resources and authored scenarios. Original IDs remain reserved after removal.
The collection bitmap is 32 bytes at RAM `0x80196A38`; the reset at z64 `0x127A64`
clears 32 bytes. Original read/write instruction checks cover IDs through 255
with guards around that bitmap. These do not establish complete save/playthrough
behavior.

Native folder/archive persistence and ordinary compiled export compose sites and
treasure with rebuilt shops and scenario squads. Reopening checks the full logical
model; unchanged re-export is identical. Qt checks apply untouched controls to
every loaded site/treasure, then exercise editing, addition/removal and undo. The
rendered pages are inspected.

Browser scenario projects now import treasure payloads, allegiance intents and
population/morale edits. The compatibility catalog checks 40 treasure archives,
59 site streams and 470 site-selector mappings against the source ROM; 52 absent
descriptors remain explicit rejections. The generator emits identities and hashes,
not decoded game content. Imports retain the browser's shared-resource behavior,
including ignoring allegiance intents equal to their original value and rejecting
conflicting changes. Native edits can then diverge for individual scenarios while
retaining shared collection flags. Clearing imported edits persists without reviving
the retained browser project. The independent browser fixture authors and exports
three payloads for native comparison, then native compile/export/reopen checks the
same data. Map presentation remains unfinished.
