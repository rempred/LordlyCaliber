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
