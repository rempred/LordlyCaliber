# Dialogue runtime

The Dialogue page now includes standalone native playback: step, play/pause,
restart, confirm/cancel, choice navigation, history and script resume. The user
selects resource passes per second and supplies explicit preview substitutions.
Scene actor placement and sound output remain separate. DialogueSession composes
opening/closing, resource dispatch and source resolution transactionally, then
draws windows in reverse priority order. A retail entry reaches its script wait,
resumes, closes, restarts and obeys timer pause; source/cutscene navigation also
pass (1,844 ms initial combined UI run). Visual inspection caught the standalone
portrait override: -1 means mirrored portrait one. Zero correctly preserves any
source portrait and otherwise leaves it absent. The corrected view was inspected;
latest focused playback check passes (315 ms). Expected offscreen font/platform
warnings remain. Editing, persistence and ROM export are next.

The artwork backend now consumes the compiled C drawing plan directly, including
style-one/two layout, all prompt positions, selection/history markers and opacity.
All 192 original-trace states compose and paint; the 93 exact rectangle comparisons
still pass (476 ms combined). Painting coverage checks safe asset/branch integration,
not original framebuffer pixels. Alpha blending uses the PC preview's nearest
sampler. Text reads its actual bounded terminator as the native draw helper does.
The broader check found an uncompressed method-80 history-arrow member and an
incorrect font interpretation: ROM EAF18 contains `{C0}{V3}`, so the parameter
selects vertical spacing, not another font. Both are corrected. Non-ASCII native
encodings and additional general text tags remain explicit unsupported boundaries.

The complete drawing control path now compiles a portable adaptation of accepted
Decomp 8012b84a func_000E6D90.c. It decodes explicit big-endian fields and emits
rendering services through a sink instead of using global native pointers. All
192 original-instruction traces match service order, arguments, text-format
selection and inline display words across styles, prompt/choice/history paths,
mirroring and alpha values. Unsafe history rows reject before emitting services.
Rendering helpers are controlled; this checks the orchestration, not their pixels.
The ordinary 93-rectangle regression also passes (224 ms combined). Connecting
the complete plan to the host artwork/raster backend remains the next step.

The ordinary opaque compositor now emits typed texture rectangles using current-ROM
frame, portrait and 2-bit glyph resources, and paints a bounded RGBA target without
Widgets. All 93 rectangles from six existing native display-command fixtures agree
in bounds, UVs, steps, scissor, texels and consumed palette colors (111 ms). The
rendered sample was inspected. Opening states may contain collapsed zero-pixel
rectangles; their native extent-minus-one step calculation is retained. Initial
rejection of those rectangles was corrected. The scope remains ordinary opaque
presentation; transparency, other styles/prompts/encodings and final N64 filtering
are not established by this check. Maintained Decomp src/lib/func_000E6D90.c is now
available for the remaining drawing paths; read it before broader reconstruction.

SceneDialogueRuntime connects initialization, heap-style payload restoration and
saving, full window updates, shared controls, child registration, sound requests
and release status to the generic dispatcher. Host payload handles are identities;
Qt owns source/frame storage. A two-window composition test passes (1 ms): both
reveal text, only the priority head accepts input, script resume finishes/ closes
the first window, input moves to the next, and slot reuse starts with fresh text.
This includes pauses before every service and one-stage dispatcher budgets. The
initial test string omitted its NUL; the corrected fixture passes without changing
engine validation. This is component composition, not a full retail-scene render.

Both original dialogue archive roots now load from the current ROM. The alternate
root 021B8BA4 contains one archive; together all 349 populated archives agree with
the independent browser LH5 decoder. Window source resolution uses record +96's
selector and +88's table bits / 14-bit entry, appending its required terminator.
Groups 40/C0 remain explicitly unresolved. Archive and composition checks pass
(298 ms). The Dialogue page exposes the alternate archive and navigates encoded
entries correctly; source-selection and cutscene-navigation UI checks pass
offscreen (1,468 ms). Expected offscreen font/platform warnings remain.

The resumable resource dispatcher reconstructs ROM 69D8..6EE8 and the flagged
cleanup traversal. It preserves circular initialization, ascending updates,
priority-based input selection, scratch-record publication, immediate release,
queue-count narrowing and the original RNG advance after each update. All 192
sequences / 576 passes match original records, service traces, queues, input
selection and RNG seeds, both uninterrupted and with a pause before every service
plus one-stage budgets (191 ms). Deliberate failed-service mutations remain private.
Callbacks, payload/heap services and copy/clear are controlled; original priority,
input selection, recursive release, cleanup and RNG execute. Render lists are
explicitly empty in this check. Payload owners and final drawing remain separate.

Roster context now publishes stable source identities and complete current backing
records to text execution, including both retained 64-byte name workspaces. All
480 original shared-name sequences / 960 snapshots pass through these bindings
(1,818 ms). The comparison checks original returned pointers, complete workspaces
and preservation of unrelated sources; string helpers and registration remain
controlled. This connects name storage, not the resource dispatcher or drawing.

Insertion sources now separate binding identity from mutable backing bytes.
An active insertion retains its selected source when a global name is rebound,
while later writes to that same storage are visible on the next reveal call.
Literal sources retain their selected value. Six original-code invocations
distinguish rebinding from mutation and pass; missing retained storage rejects
without partial writes. Reveal, full window updates and complete initialization
regressions also pass (3,508 ms combined). This corrects the earlier key-only
continuation model before connection to changing roster/name context.

Recursive resource release reconstructs ROM 8388..84D4 at RAM 80077F88. All 384
complete pool/callback/handle-order comparisons pass (119 ms), including callback
changes that affect later child discovery. The cascade bit is record byte3 bit8;
children compare signed record+A2 with the releasing slot. The supplied callback
runs before ten optional handles and the unconditional payload-handle request,
then the full record clears. Callback mutations, allocator release and zero-fill
are controlled in the reference. Cycles and absent callbacks preserve the host
pool. Callers must stage any callback-owned state beyond that pool themselves.

Closing steps reconstruct ROM 7FF8..8380 at RAM 80077BF8. All 224 sequences /
2,688 complete pool snapshots and ordered release requests pass (184 ms), with
release controlled to record clearing. Signed margins, fractional shrinking,
wrapping bounds and release on the following pass are retained. An already
closed zero-speed record requests release without division; other zero-speed
failures preserve its record. Recursive cleanup is the next separate owner.

The opening queue compiles ROM 79EC..7FF8 at RAM 800775EC. All 512 sequences /
6,144 complete pool snapshots pass (300 ms), with no controlled helpers. This
preserves direction flags, signed extents, clipping, stored fractional byte lanes
and skipped-axis accumulators carried across records within a pass. Invalid queue
entries and late zero-speed failures roll back the whole host pool.

Shared-resource priority rebuilding now compiles ROM 859C..86EC at RAM 8007819C.
It stably sorts active slots by unsigned descending priority, then renumbers
nonzero priorities while retaining zero values and unused queue cells. All 384
complete queue/pool comparisons pass without controlled helpers; registration
and exhaustion regressions also pass (249 ms combined). Opening/closing and the
generic callback dispatcher remain to connect to the dialogue update owner.

The dialogue update callback now composes scrolling, input/state transitions,
reveal, shake, motion, pointer placement and bounds in their native order.
It reconstructs ROM E659C..E6D90, EA628..EA684 and EACC0..EACFC with explicit
pressed/repeated/history input and current-ROM shake bytes. All 408 sequences /
4,896 full snapshots pass (967 ms), including opening, scrolling, choices,
confirm/cancel, waits, portrait fades and closing. Text reveal still passes.
Sound, child allocation, zero-fill and formatted append are controlled; sound
and child operations return requests for the scene owner to commit. Zero-speed
fast-forward and invalid shake indices reject atomically. The initial constructed
choice fixture omitted the rows its menu referenced; adding those rows fixed
the invalid fixture without weakening the host's line-table bounds.

Timed text reveal reconstructs ROM E87F8..E93A4 at RAM 8019BA78 with its line
helper at E93A4. All 5,037 sequences / 10,592 complete snapshots pass (1,222 ms):
all retail entries plus explicit controls, six speed boundaries and repeated
invocations. Output bytes, full records, typed insertion cursors, eight shared
controls, sound requests and child-window requests agree. Original segment and
choice helpers run; sound, child allocation, zero-fill and formatted append are
controlled. The accepted eacfc helper is a U16 identity. The centering format is
`{X%03d}`. Malformed text, missing names and exhausted budgets roll back all output
and requests. The 768-byte output and 96-entry line table are bounded on PC.
This is one reveal invocation; surrounding input/scroll/window callbacks remain
to connect. Sound requests are not yet audible output.

Choice scanning reconstructs ROM EA684..EA8EC at RAM 8019D904/8019DA58.
All 4,616 original choice counts and 55,392 marker seeks pass (11,719 ms), with
no controlled helpers. Retail sources and constructed operand/two-byte cases
exercise the distinct skip grammar. Counts stop at `@#`; seeks narrow the marker
to a byte and publish the cursor after the matched control and its operands.
Malformed controls and budget exhaustion preserve the caller's cursor.

Window motion, bounds placement and pointer tracking compile maintained C from
ROM EA184..EA628 at RAM 8019D404. All 270 sequences / 2,160 complete window and
payload snapshots pass (186 ms), with no controlled helpers. Checks include
overshoot, signed-halfword wrap, retained completion, zero/large steps, signed
margins and portrait/style pointer limits. Invalid record sizes preserve inputs.
These operations are ready for the dialogue update owner; its scheduling and
text reveal state machine are still being connected.

The initializer now composes prefix parsing, both measurements and accepted-source
geometry. All 4,644 complete 168-byte window/1,144-byte payload comparisons pass
(1,700 ms), including every retail source. Only archive resolution, zero-fill and
diagnostic output are controlled; the original measurement/prefix/string helpers
run. Synthetic metric/name context is explicit. Fixed dimensions skip unavailable
measurements. Automatic line counts use computed columns before the native clamp.
The first reference setup had the wrong initializer payload location and omitted
the line routine; correcting both resolved those fixture failures. Scheduling,
text execution and drawing still require integration.

Dialogue pixel measurement reconstructs ROM E98BC..E9EB0 at RAM 8019CB3C.
All 6,169 comparisons pass, including every retail entry, minimum/column/fixed-font
limits, page breaks and every style/side/control/portrait branch for `@i`/`@p`.
Those spacing branches all reduce to 15 pixels. The `@z` width path consumes one
additional byte after its substitution operand; line/segment grammars do not.
Original general/segment/glyph/string helpers execute, with explicit synthetic
names and metric tables. Width plus general-text regression passed in 2,611 ms.
A constructed `@!` substitution truncates a two-byte glyph and makes native code
read uninitialized scratch beyond its terminator; it is excluded from matching
claims and explicitly rejected by the host. Missing context and bounds reject.

The general text width helper reconstructs ROM 4F3C8..4F86C at RAM 801794C8,
including its numeric reader at ROM 51400. All 1,851 original comparisons pass
(189 ms), covering byte classes, two-byte glyphs, spacing, tabs, font changes,
paired sections and 16-bit width wrap, with both metric tables supplied explicitly.
No game helpers are replaced. Unlike the leaf glyph query, this routine ignores
the fixed-index override. The host measurement bounds every source/table read
and keeps the original helper's temporary cursor globals local. Malformed braced
input, invalid indices, missing metrics and exhausted budgets reject.

Line measurement reconstructs ROM E9528..E98BC at RAM 8019C7A8. All 32,305
original-instruction comparisons pass for retail entries and boundary sources
across seven column limits. The original recursive scanner and classifier run;
names and the long-name suffix are explicit test inputs. Insertions count raw
glyph units, including the raw-byte limit for `@&`, while page breaks retain the
maximum line count. The host query keeps helper cursor publications local.
Missing/truncated input and budget exhaustion reject. The line/scanner checks
passed together in 5,752 ms. Pixel measurement and rendering remain separate.

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
