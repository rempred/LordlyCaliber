# Scenario resource rebuild

## Independent squad members

`SquadMembersEditor` supplies the common member table and formation controls for
Scenarios and Encounters. The Scenario model preserves each original 35-byte
template exactly when controls are unchanged, including inactive cohorts and
unknown padding. Compatible edits retain its original framing. When a squad needs
more follower specifications, its main template uses `LS` at bytes25–26 and four
big-endian relative record offsets at27–34. Each active follower's offset equals
its one-based member index; inactive offsets are zero. The complete main record
and its immediately appended 35-byte follower records form one project value.
The main record retains the leader and original position bytes (B:13–15, C:22).
Each appended record begins with its own class, raw level offset and two equipment
choices. Original resource links, one-based ESET IDs and 35-byte library stride
remain; allocation checks include the whole member block. No stock record changes.

`game/scenarios/squad-member.c` adapts accepted `func_0019554C.c` into the original
644-byte owner at z64 `0x19554C` / scenario RAM `0x802150BC`. The original squad
constructor still allocates members and stores formation positions. Before each
follower call it has published earlier member IDs in the source record, letting
the helper select that member's appended specification. Pool selection, class
copy, level initialization, equipment precedence, leader flags, special naming,
record finalization and persistent-player tail behavior remain. Soldier groups
use a different original constructor branch; they retain stock cohort encoding
and cannot be mixed into an extended individual specification.

The generated `squad-member.json` authenticates the complete adapted owner and
its class references. The normal class relocation pass recognizes and relocates
it, including later class growth. Generate with
`python native/tools/generate-squad-member-runtime.py` using the pinned toolchain.
Export compiles the maintained C within the original allocation; reopening rejects
extended member data without that recognized constructor. No separate hook or
resident-memory allocation is added.

`engine_tests scenarioIndependentMembers` covers all original record round trips,
five distinct member specifications, independent edits, project persistence,
export/reopen, repeated edits and subsequent class growth. The original-MIPS
check `tests/scenario-members-mips.cjs <fixture.json>` compares 144 stock helper
cases, then runs 24 original-constructor cases with two through five distinct
members across both record pools and allied flags. Initialization/equipment/finalizer services are
controlled doubles; these checks do not establish a natural game load.

## Original resource layout

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

### Why key 54 is excluded

The 2026-10-08 parent investigation identified an explicit game-code bypass,
not a different ESET format or unknown resource. Keys 52/53/54 all request
collection index 56 (`eset4_14.bin`). On fresh setup, z64 `0x10358C` marks key54
initialized before `0x103670` checks that marker, bypassing the normal constructor
call at `0x103704` (RAM `0x80215C18`, z64 `0x1960A8`). Depending on a map-object
flag, it instead copies code `0x1A2C20..0x1A4C10` to RAM `0x80214F80` and sets
mode46. Its observed base-one writer is the ordinary reset at z64 `0x102090`.
Parent `tools/inspect_key54_scenario_path.cjs` executes these original branches
with controlled inputs for keys52/53/54; callees and DMA are trapped.
See parent `docs/enemy-deployment-system.md` for evidence and limits. Standard
base-level and squad editing remain unsupported because their normal consumer
is skipped. Enabling normal squads requires an explicit behavior change in game
code. Special scripted creation and natural story reachability remain unverified.

### Rebuild layout

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
