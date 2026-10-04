# Scenario resource rebuild

Observed from US Rev 0 and checked with the original instructions:

- The table at z64 `0x654A0` / resident RAM `0x8018F5A0` has nine-byte rows.
  Byte eight is a signed ESET archive index, consumed at z64 `0xEC1A0`.
  Keys 0..64 resolve within the original collection. Keys 10/22/35/62 share
  index zero; 52/53/54 share index 56. Key 54 remains excluded from authoring.
- The caller belongs to overlay descriptor 3: z64 `0xEBBB0` loads at RAM
  `0x8019EE50`. It passes `(0x021B35B2, 0, -4, index)` to the original resource
  dispatcher at RAM `0x8007938C`.
- ESET outer envelope: z64 `0x2747832`, length four, one resource key at
  `0x2747836`. Its original target is `0x274783A`, containing length `0x55E5`,
  all 63 contiguous LHA members and one zero terminator. Original members start
  at `0x274783E`. A resource key is its envelope address minus `0x594280`.
- Enemy library outer envelope: `0x27429CC`, key at `0x27429D0`, original inner
  envelope `0x27429D4`. The collection contains one `enemydat.bin` member and
  its terminator. Records are 35 bytes; ESET IDs are one-based.

Rebuild leaves original members and ordinals available, and appends changed
versions keyed by their original member plus desired contents. It regenerates
the collection, length words, link pointers and selector bytes together. Each
scenario can change independently even when the original resource was shared.
The original decoder allocates from the stored and decoded lengths. The editor
does not install executable code for this resource operation.

`LC05SCEN` version 1 is a 64-byte descriptor at z64 `0x27C4400`. Fields after
the eight-byte magic: version, ROM offset, aligned allocation size, enemy relative
offset/size, ESET relative offset/size, allocation CRC32, selector CRC32, original
member count, original selector directory relative offset, eight reserved zero
bytes, header CRC32. Owned data starts at/after `0x2800000`, stays below 64 MiB,
and preserves later allocations when moved. The retained selector directory
allows repeated edits to compact their appended indices and restore aliases.

Checks: `engine_tests scenarioResources` exercises real-ROM round trips,
independent aliases, library growth, large ESET data, repeated edits, restoration,
foreign allocations and corrupt metadata. `scenario-resources-mips.cjs` executes
the original caller, dispatcher, archive parser and LH5 decoder on that candidate.
Its heap and lower transfer services are controlled doubles with allocation
guards in 4 MiB memory. This establishes the bounded resource path, not natural
game transitions, game-state correctness or peak full-game memory availability.
