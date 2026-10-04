# Dialogue runtime

The bounded segment scanner reconstructs ROM E9EB0..EA184 at RAM 8019D130.
All 23,070 original-instruction comparisons pass (938 ms): 4,569 retail sources
and 45 boundary sources at five column limits, comparing count, cursor, stopping
byte and publication. Substitution pointers are explicit synthetic context;
the original classifier and recursive scanner execute without controlled helpers.
Missing substitutions, cycles, malformed controls and exhausted budgets reject.
A known null string preserves the native publication distinction. This grammar
counts segments; line wrapping and pixel width remain separate operations.

Glyph-width and lead-byte classification compile maintained C from resident ROM
4F2E4..4F3C8 and 51450..5148C (RAM 801793E4 and 8017B550). All 4,608 original
comparisons pass with no helper substitutions (121 ms), covering both tables,
fixed-index overrides and byte narrowing. Invalid metric ranges fail explicitly.
Metric tables are current context inputs; their presence alone does not prove
that a scene selected that font. These leaf helpers do not implement the larger
width/line/body grammars.

Leading-control parsing now compiles maintained C reconstructed from ROM
EA930..EACC0, loaded at RAM 8019DBB0. All 4,569 retail entries plus 56 boundary
cases match complete window/payload records and cursor offsets (419 ms), with
no controlled game helpers. Unknown/body controls stop before consumption; byte
and halfword numeric narrowing is retained. Truncated input fails atomically.

Geometry compiles a portable adaptation of accepted `func_000E5968.c` at Decomp
8012b84a. Fixed-width host fields replace target globals; statement expressions
become ordinary C and diagnostic prints become warning bits. Window headers and
1,144-byte payload initialization preserve the native fields, callbacks as identity
values, clipping/pointer arithmetic, dimensions and opening distance. Text pointers
are source-relative offsets on PC.

All 1,152 complete initialized window/payload comparisons match original ROM
E5968..E659C at RAM 80198BE8. The original prefix parser executes; archive-pointer
resolution, zero-fill, width/line measurements and diagnostics are controlled.
The combined prefix/geometry checks passed in 601 ms, then the geometry check
passed again in 159 ms after making the intended narrowing casts explicit.
The clean build has no new compiler warnings. Width/line/body grammars, scheduled
initialization and final text rendering remain to connect; text length is not used
as a substitute for native measurements.

`SceneDialogue` owns the 28 six-byte Actor/window tracking records and eight
control bytes. `SceneResources` owns the shared six 168-byte UI records. Creation
and placement stage copies so unavailable projection, portrait context or storage
does not partially alter the scene. Native resource addresses are identities,
never callable PC pointers.

ScenePlayback accepts the current shared pool/window state, handles resume/close
and window/control queries, and follows Actor anchors in the post-Director stage.
Missing projection suspends that stage without replaying Director commands when
it resumes. The composition check passes (96 ms), comparing normal execution with
one-command/one-stage budgets; existing scene composition still passes (93 ms).
BF creation now prepares shared name context when current roster/group records
are supplied. Missing context or projection retains the command and all prior
window/context bytes; the recovery check passes (93 ms). Ordinary automatic
portrait lookup retains the unresolved boundary described below; explicit
portraits, known absent actors and linked army row portraits are supported.
The first composition fixture incorrectly ended with a padding word; replacing it
with the native terminal-hold word fixed the fixture.

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

`SceneDialogueContext` implements shared speaker references, 64-byte owner/group
name workspaces and the shared text parameter. Guest records precede army records
for fallback speaker lookup. Owner names append every matching override; group
names append only the first. Non-mode-two creation preserves the previous owner
reference, and absent names do not clear retained workspaces. Native pointer
identities are represented by typed host references.

All 480 original shared-name sequences / 960 reference/workspace snapshots pass,
including duplicate overrides, full-word owner comparisons, missing actors,
mode changes and absent group names (1,590 ms including the composition check).
String copy/append, allocation, registration and projection are controlled. Bounds
and overflow failures preserve the previous context. The initial reference load
omitted the text-parameter reset's delay-slot store; its corrected full range passes.

Source mapping: descriptor 14 maps ROM 283808/283B6C/283C8C to RAM
80227838/80227B9C/80227CBC. The initial reset adapts accepted `func_0004e434.c`
at Decomp 50aed758; the qualified cold-boot slab maps ROM 4E434..4E448 to
RAM 80178534. These mappings differ from the Actor slab.

The desktop Cutscenes workspace includes a read-only Dialogue source page with
archive/entry selection and search across exact control-bearing source. Double-click
a BF source command to open its narrowed archive/entry. Missing targets clear stale
text. Both focused UI checks pass offscreen (1,456 ms), including the existing image
catalog path, and the rendered source view was inspected. This page is an inspector;
dialogue editing/export and native text layout are still unfinished.

Portrait follow-up: ROM 29FF74 calls the raw loader at RAM 8009DBB8 for key
01A8E7AE. That resource's 332 bytes begin with 00000152 and appear compressed;
the raw halfword lookup exceeds its allocation for many ordinary banks. The raw
loader was inspected at ROM 2DFB8..2E138 and does not decompress. Preserve this
unresolved distinction; do not silently replace it with LZSS. Linked army actors
instead read their current row's word +48, return -1 if zero, otherwise narrow to
signed halfword. These lookup paths have not yet been connected.
