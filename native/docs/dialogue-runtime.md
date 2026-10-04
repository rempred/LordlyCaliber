# Dialogue runtime

`SceneDialogue` owns the 28 six-byte Actor/window tracking records and eight
control bytes. `SceneResources` owns the shared six 168-byte UI records. Creation
and placement stage copies so unavailable projection, portrait context or storage
does not partially alter the scene. Native resource addresses are identities,
never callable PC pointers.

The implementation reconstructs original US Rev 0 ROM 2A0088..2A05DC (loaded at
RAM 80230858) and F9FF8..FA56C (loaded at RAM 8019EE58), including the dispatcher's
portrait/mirror/pointer rules. The coordinate projector remains an explicit input.
Null projection means a known empty Actor; an absent projector is unavailable
context. Fixed placement needs no projection. Text interpretation, shared name
context, opening/closing queues and final rendering are separate and unfinished.

Window IDs have distinct widths: lookup/resume/close use a byte, tracking stores a
signed halfword, and tracking removal compares that halfword with the full command
word. Duplicate IDs retain first-resource matching. Creating another window for
the same Actor replaces its tracking while preserving the earlier live resource.
Close marks the resource for its queue; it does not immediately free it.

`DialogueArchive` resolves current-ROM resource directory key 01A3B7B2, including
empty selectors. It validates each LHA member inside its resource envelope and
checks decoded length/CRC. Byte-offset entries retain exact controls, aliases and
unreferenced source bytes. No text-length heuristic determines native pauses.

Verification: `dialogue_runtime_tests.exe` with `LORDLY_TEST_ROM` passes four
substantive checks (408 ms). Original MIPS compares 64 sequences / 560 complete
pool/tracking/control snapshots. Projection, allocation and shared name-context
helpers are controlled. An independent browser LH5 decoder agrees on all 348
populated archives / 4,569 byte-exact entries from 503 selectors. Malformed
directory/envelope/checksum and missing-context/exhaustion rollback checks pass.
No emulator, text layout or full-scene rendering claim follows from these checks.

Portrait follow-up: ROM 29FF74 calls the raw loader at RAM 8009DBB8 for key
01A8E7AE. That resource's 332 bytes begin with 00000152 and appear compressed;
the raw halfword lookup exceeds its allocation for many ordinary banks. The raw
loader was inspected at ROM 2DFB8..2E138 and does not decompress. Preserve this
unresolved distinction; do not silently replace it with LZSS. Linked army actors
instead read their current row's word +48, return -1 if zero, otherwise narrow to
signed halfword. These lookup paths have not yet been connected.
