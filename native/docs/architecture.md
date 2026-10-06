# Native Lordly Caliber

## Saved reserve membership

Regular battery slots now move ordinary nonleaders to reserve and add ordinary
reserve characters to empty cells in existing squads. Reserve means an existing
character absent from the Army-selected saved squad rows. The original loader's
initialization and membership conversion establish that distinction; no extra
reserve flag is written to the character. Stale leader flags and conflicting or
missing character references refuse. Runtime/suspend copies have no inferred
reserve list, since their active squads may own the assignments.

Both operations use the same atomic membership path as transfers: validate size
and formation, retain carried items only when capacity permits, clear affected
Unity and recalculate movement from current Classes. Roster bytes, leaders,
inventory, order arrays and Soldier data remain intact. Save undo owns one change
per transfer. Pending formation edits block transfers. Squad details scroll at
smaller window sizes. Soldier transfers, leader changes and create/delete remain
separate work.

`save_document_tests reserveMembership squadTransfers` passed in 1919ms; strengthened
reserve guards passed in 735ms. Two edited snapshots pass the original Army
initialization/membership code after original save decoding. Three battery layouts,
both regular slots, whole decoded-memory preservation, exact undo/redo, export and
reopening pass. Capacity, full squads, missing characters, invalid sources/targets
and runtime/suspend refusal pass. `ui_tests saveSquadEditing saveWorkspaceEditing`
passed in 2750ms; the reserve controls capture was inspected. These are instruction
and save-file checks, not a natural Army-menu session.

## Selecting visible Actors in the scene

Clicking an Actor selects its live slot and pauses playback; double-clicking opens
its action when editing is enabled. Dragging retains the existing pan behavior.
A cosmetic outline shows the selected Actor's projected extent, independently of
the rendered/exported frame. Dropdown selection and absent-step retention remain.

Each completed render snapshot retains ordered Actor layer geometry and cover
alpha. Picking rasterizes only the clicked pixel with the same Qt mapping and
clipping as scene composition. Transparent holes pass through to lower Actors;
opaque foregrounds, scene objects, masks, dialogue and screen covers block them.
Shadows are not selection targets. Partially transparent covers retain a target
while its contribution remains at least one alpha byte. Screen cover images store
alpha only, and ordinary playback does not rasterize an extra selection frame.
Captured imagery has no invented live Actor identity. This follows the current
desktop rendering approximation, not an additional N64 rasterizer claim.

`scene_runtime_tests visibleActorSelection sceneActorCompositionAndRollback`
passed in 112ms. Synthetic overlapping layers, transparent holes, shadows,
projected/scissored pixels, opaque and translucent covers and crop masks pass;
the pixel picker agrees with independent full-frame Qt composition. Existing
Actor tint/order/atomic rollback checks pass unchanged.
`ui_tests classActorCreationControls sceneRoomComposition sceneSpriteComposition
savedStoryCastPreview` passed in 4536ms. Actual click, drag and double-click input,
backward seeking, project/frame preservation, real room foreground occlusion,
sprite composition and saved-cast dropdown behavior pass. The final outline
capture was inspected. No ROM output changes are introduced by selection.

## Creating class Actors

The Actions inspector now offers Create class Actor for fresh environment resource
classes 1 and 8. It emits native Actor allocation (0x14) immediately followed by
body selection (0x2A), recognizes that same layout in existing streams, and treats
the pair as one action for editing, insertion boundaries, ordering and removal.
The slot field updates both commands. Primary/secondary class, pose, XYZ, facing
and depth are editable; ordinary initializer words and independent body flags
remain intact. Symbolic and noncanonical fields remain read-only. New class edits
must refer to the current project; unrelated edits preserve existing opaque values.

Class fields also offer named choices from the current project beside the numeric
ID. Additions, renames and undo refresh the choices without replacing incomplete
field input. Existing body selections include the native retain-current option;
symbolic launch fields keep their read-only labels. Hexadecimal numeric entry
and named selection share the same encoded field and undo path.

Scene playback declares class-default equipment for these explicit fresh bodies
and supplies its preview appearance rows at restart. This reuses the existing
native class appearance and body initializer. It supplies no army roster, member
binding or event translation. Those services still require their explicit inputs.
The existing story-cast launch path remains unchanged.

`scene_action_tests classActorCreation authoringBoundaries bodyActionFields exactFields`
passed in 8144ms. The strengthened `classActorCreation` passed in 6746ms, covering
paired slots, indivisible commands, reordered/removed pairs, preserved opaque words,
native class pair/XYZ readback, missing-context atomicity and a newly added class.
The project saves, compiles game source, exports and reopens with that new class
still executable through the native body path; allocation bounds, references, CRC
and master preservation pass. Candidate: parent `Test ROMs/native-class-actor-cVGAwm`.
UI creation, existing body fields, saved cast and action insertion passed in 6272ms;
the final creation/sentinel regression passed in 1165ms. It covers shared undo,
invalid-class input, exact backward seek, visible rendering and action navigation.
The rendered capture was inspected. Existing body-initializer instruction evidence
is reused because its maintained C/runtime implementation is unchanged. Natural
game event launch and arbitrary caller army context are not claimed.

## Blank environment scenes

New scene now offers qualified environment templates as well as the existing room
templates. Resource classes 1 and 8 must have one explicit initial environment
request, no additional presentation requests, a matching native launch scan and
a supported release exit. The existing wipe implementation retains that setup
and exit. Caller-selected environments, presentation handoffs and unresolved
parent-event relaunches remain unavailable as blank templates. Room creation is
unchanged. The dialog labels the two kinds and prefers the selected scene.

`director_tests blankEnvironmentScenes` passed in 1611ms: both classes preserve
independently expected setup/exit bytes, dispatch setup and release, reject the
unsupported contexts without mutation, save projects and source-build/export/reopen
with distinct UUIDs/selectors, valid allocation bounds and CRC. The master ROM stays
unchanged. Candidate: parent `Test ROMs/native-blank-environment-VdnJSs`.
The existing room identity/export regression passed; the first new test had an
incorrect expected service list, corrected to include background setup.
`ui_tests blankEnvironmentCreationControls authoredSceneCreationControls` passed
in 1581ms, covering environment selection, creation, undo/redo and persistence,
plus the existing room editing/duplication workflow. Natural event launch is not
claimed by these desktop and export checks.

## Existing class-body action fields

The Actions inspector recognizes native0x2A as Set Actor class and pose. It exposes
the slot, primary/secondary class and pose ID with the exact -1 retain/skip
sentinels. Symbolic launch operands remain read-only and display Launch valueN.
The two independent body-flag operands remain intact; they are not ordinary facing
fields. Noncanonical encodings remain intact in Commands. Later action templates
now recognize a class-body command's Actor slot when choosing their default.

This edits existing commands. It does not add a generic body-creation template or
invent a caller army/appearance context. The existing maintained C selection and
native preview service remain unchanged, including the mode-two restriction.

`scene_action_tests bodyActionFields` passed in1910ms. Pair54/55 and pose0 reached
the native Actor record with preservedXYZ; retain/skip and mode-zero no-op cases
passed. Opaque flags/symbolic source and unrelated words stayed exact. A one-word
pose edit in01FA8BF4 saved, source-built and reopened via stable Director selectors,
with allocation bounds, CRC and master preservation checked (parent Test ROMs/
native-body-action-dKJoba). The first reopen test incorrectly used the old physical
resource key after relocation; the selector-based check corrects that test.
`exactFields` and `authoringBoundaries` also passed. UI bodyActionEditing plus the
live-cast/structured-actions/insertion regressions passed5830ms; the final readable
symbolic-label check passed698ms. The final post-layout visibility/value check
passed707ms and its capture was inspected. The full native build passed without
compiler warnings. No natural event launch or new generic roster context is claimed.

## Live Actor inspection and action navigation

Follow-up: the live Actor dropdown now retains its entries while membership stays
the same. Previously each rendered frame rebuilt the list and reset an open
popup's highlighted choice. A desktop interaction check reproduced Actor2's
highlight reverting to Actor1 on step3->4. With the fix, the popup stays open and
keeps Actor2 highlighted without committing a selection; seek and existing
Actor/cast behaviors remain intact. `ui_tests savedStoryCastPreview storyCastPreview`
passed2344ms, and the full native build passed without compiler warnings. This
removes unnecessary per-frame list reconstruction as well.

Scene playback lists its currently instantiated Actor slots and displays the chosen
Actor's actual class pair/art bank and XYZ record values. Selection survives seeking
through a step where that slot is absent; absent Actors have no active navigation.
This reads the current playback snapshot and does not assign new game coordinates.

Find Actor action resolves the active script selector to its project scene, then
selects that slot's Create Actor action or first supported direct reference. It
opens the shared Actions inspector without changing the program. Conditional scripts
can contain several actions for a slot; this is navigation by slot, not a claim to
identify the exact executed branch. A launch-created Actor without a direct editable
reference reports that limit. Read-only previews hide the navigation button, and
incomplete source/action inputs block navigation without discarding text.

`ui_tests savedStoryCastPreview structuredSceneActions sceneActionInsertion` passed
in5467ms. The real01FA8BF4 preview exposed Actor1's independent pair86/87 and its
native stream XYZ, kept selection and exact readback through step3->1->3 and0->3,
opened Actor1's placement fields, and retained incomplete input when navigation
was attempted. Save/project bytes stayed unchanged. Existing action editing,
insertion, timing and shared-undo UI regressions passed. The existing engine
`scene_action_tests exactFields` passed in5ms. The integrated capture was inspected,
and the full native build passed without compiler warnings.
No rendered-pixel hit test or additional game-runtime qualification is claimed.

## Portable check without interactive command-line dialogs

The isolated package check sets Qt's documented
`QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES` option, captures the exact GUI version
response and bounds that startup wait to30seconds. It restores the caller's
previous environment afterward. This changes verification only; the application
retains its normal command-line behavior. Qt documents the Windows dialog fallback
at https://doc.qt.io/qt-6.12/qcommandlineparser.html.

Portable ZIP20261006-004218 contains native commita9a935c. Its isolated browser
import check passed2648 exact scene bytes (parent Test ROMs/
portable-scene-import-92e574a6843247fa95edd380e2ed7677). The corrected general check
passed startup, bundled user-time compilation, independent Knight STR300 readback,
export/reopen and master preservation (native-portable-1843cac354654d45b549137ec3524e9d).
The first startup invocation remained in Qt's version dialog without a console;
automatic approval review rejected process termination, so it was left intact.
These are isolated-environment checks on this machine, not a clean-machine claim.

## Saved story-cast preview inputs

The five-person story preview now has independent primary/secondary selectors and
Use open save / Use sample cast controls. The open Save workspace's selected slot
supplies its ten saved launch-bank bytes. The preview reports the source slot,
validates all pairs before applying them, and preserves the current cast if the
save is absent or incompatible. It never edits the save, scene or project.

Primary changes choose their matching secondary as an editable default; manual
secondary changes preserve the primary. Project class names and additions reach
the selectors. Undoing a class addition clears an invalid secondary reference,
returns it to the current primary and explains the change. Seeking restarts with
the chosen pairs and retains the existing preview input-history rules.

These are saved launch inputs, not a fresh execution of event67's army selection.
The original source-bank getters are resident ROM46314/46324, RAM80170414/80170424,
reading8019367C..80 and80193681..85. The current-ROM save descriptor preserves all
ten bytes at eight bits each. The existing row66 classifier/translation contract
and literal scene coordinates remain unchanged; full roster launch state is separate.

`scene_runtime_tests savedStoryCast storyCastLaunch` passed in515ms: original
getter checks for20 source bytes, original-packer fixtures, both slots in three
battery layouts plus raw RAM, distinct secondary pairs, refusal cases, exact
save/master preservation and Actor class/coordinate readback. `ui_tests
savedStoryCastPreview storyCastPreview stagedAnimationPreviewAndUndo` passed
in3250ms through the MainWindow/Save/Cutscenes connection, including deterministic
seek, missing/invalid input, slot changes, added class165 and its removal by undo.
The saved-cast capture was inspected. The full native build passed without
compiler warnings. Initial fixture/async-catalog setup errors
were corrected; they did not indicate a native runtime failure. These checks do
not establish a natural story-event launch or infer the current army's new cast.

## Saved nonleader transfers

The Save Squads page now moves ordinary nonleaders to empty cells in another
existing regular battery-save squad, alongside occupied-member swaps. Both
resulting formations and carrying capacities validate before one atomic undo
change. Pending formation/Unity/tactic inputs disable membership operations.
Leaders, roster/status data, items and order/activation arrays stay intact.

Both operations now recalculate saved movement byte24. The current-ROM priority
bytes select among five class-derived types. Soldiers use saved character record0
for movement, but pair1/1 for capacity and formation size; these distinct original
rules are retained. A move that would require returning carried items is refused.
Reserve/Soldier transfers, leader changes, creation/removal and runtime copies
remain separate unfinished operations.

`save_document_tests squadFormation squadTransfers editsPreserveNativeSaves` passed
in5493ms:27225 original class pairs,32768 movement tuples,12 removal/24 placement
Unity cases,217 formation conversions, original save decode, all three battery
layouts, exact whole-RAM preservation, undo/reopen and refusal cases. A stronger
Soldier record0 pair44/44 case passed `squadTransfers` in813ms: movement7 and
pair1/1 capacity3 agree with the original finalizers. Desktop `ui_tests
saveSquadEditing saveWorkspaceEditing` passed in2556ms, including move/undo/redo,
pending-input protection and export/reopen. The transfer capture was inspected, and the full native build passed without
compiler warnings. No natural Army-menu execution or active-state coherence claim is made.

## Actor action controls

The Actions inspector supports Turn Actor, Room Actor Channel and Release Actor.
Turn fields expose cardinal start/target, native phase direction and update budget;
only -1/+1 direction values are accepted. Room channels retain their mode-zero
transform/draw meaning and warn that an automatic divider can reassign them. Actor
selectors support the exact -1/all sentinel where the underlying command does.
Noncanonical words stay intact and read-only in typed fields.

Follow-up templates use the nearest preceding Actor reference, with the last known
cardinal facing for a turn. This is an editable script default, not a claim about
dynamic launch occupants. New pose actions preserve existing art, animation,
facing, variant and position. The coordinate tooltip explains the original X/Y
sentinel test: both -1 keep the entire XYZ tuple; otherwise all three are replaced.

`scene_action_tests actorActionServices authoringBoundaries actionOrdering
exactFields` passed in 4843 ms. A current-ROM Actor received an exact channel-byte
change; turn0->1/direction-1/budget30 completed after25 updates at facing1; all-slot
assignment, mode-two channel preservation, individual/all release and pose defaults
passed. Noncanonical channel words remain intact. The authored scene saved,
compiled, exported and reopened with bounds/CRC/master checks. Candidate: parent
Test ROMs/native-actor-actions-HmmXCC. The existing `scene_runtime_tests
turnsAgainstMips` passed its266 original-instruction cases in1014 ms.

`ui_tests sceneActionInsertion structuredSceneActions` passed in4451 ms: direction0
refusal, valid correction, shared invalid-input guards and Actor slot continuity
from creation to turn/channel, plus prior editing/order/dialogue checks. The capture
was inspected. The full native build passed without compiler warnings. Rendering and natural
game launch are not newly qualified by these adapter tests.

## Action ordering

Move action relocates a selected complete action before a named destination,
retaining selection and one shared Undo operation. Ordinary operands and connected
dialogue IDs stay intact. Waits are regenerated for the destination's counter span
with their relative duration and original comparison rule; later deadlines shift
consistently. Movement across separate native control sections is refused. Initial
setup remains in place, and dialogue cannot move into an active counter span.

`scene_action_tests actionOrdering authoringBoundaries insertedWaitTimingAndExport
dialogueActions` passed in 8068 ms. A movement moved from update 0 to 3 while the
exit stayed at 7; moving the first wait past a scale action changed that action
from update 3 to 0 without changing exit time. A shared wait moved between gates,
retaining scale/exit at20 while movement changed from10 to0. Exact ordinary-word
preservation, reverse moves, setup/control refusals and comparison retention pass.
The reordered scene persisted and rebuilt/reopened with allocation bounds, checksum
and master preservation checks. Candidate: parent Test ROMs/native-action-order-bWjIGw.
Existing wait and dialogue authoring checks also pass; this is dispatcher ordering,
not a new claim about rendered movement or natural game launch.

`ui_tests sceneActionInsertion structuredSceneActions` passed in 4375 ms, including
moving connected dialogue before a wait, selected-action synchronization, undo/redo
and reopening. The capture was inspected. The complete native build passed without compiler warnings.

## Connected dialogue actions

The Actions inspector now creates a complete dialogue window/completion/resume/close
sequence and exposes its text reference, owner, placement and portrait settings.
Window ID changes update all four linked operands. The unused BF operand remains
untouched. Existing windows with other control layouts expose text/presentation
fields while retaining their separate IDs and control commands. Edit dialogue text
opens the existing text workspace on the referenced primary or alternate entry.

New dialogue uses fixed placement with a transparent portrait and an unused
script window ID. Insertion requires a valid condition barrier outside active
counter spans: player input cannot be represented as a fixed counter delay.
Recognized complete dialogue outside a span allows subsequent waits. Nested
counter queries without intervening yields are now excluded from structured timing
because their failure scanner requires additional barriers. The maintained C
runtime and original window-service qualification remain unchanged.

`scene_action_tests` passed in 8256 ms: linked ID edits, reserved-word preservation,
separate-layout refusal, nested-counter guards, one window creation during repeated
pending queries, controlled ready state, one resume/close and a following three-update
wait. An appended dialogue entry and authored scene saved, compiled, exported and
reopened with preserved master, valid resource bounds and checksum. Candidate:
parent Test ROMs/native-dialogue-action-knkXas. This uses a controlled readiness
flag; it does not establish real input or full text-render timing.

`ui_tests sceneActionInsertion structuredSceneActions` passed in 3127 ms, including
window identity undo/redo, navigation to the correct text, editing and project
reopening. The return-to-Actions visibility/value check passed in 1973 ms. Its first
capture ran before Qt's pending layout; the completed capture was inspected and all
fields fit. The complete native build passed without compiler warnings.

## Native action insertion and removal

The Actions inspector adds placement, movement, camera, fade, scale, opacity and
wait actions at named script positions, and removes complete actions with shared
project undo. Initial background/camera setup remains ahead of new content. Actor
creation defaults to a slot not referenced by recognized script operands; dynamic
launch occupants still require the author to choose the intended slot.

Wait insertion checks the native raw-word scan, including operand words, for a
real barrier/yield boundary. Outside a counter span it emits an arm/wait/reset
compound. Inside a known monotonic span it adds a gate and shifts later targets
through the reset. Shared wait fields show relative duration; editing or removing
one shifts those targets consistently. Branch-dependent counters, extra yields,
unknown resets, nonmonotonic targets and overflow refuse structured wait insertion.
The original commands remain available. This is script authoring, not a claim that
all branches have one absolute timeline. Native dispatcher C is unchanged.

`scene_action_tests` passed in 4442 ms, including sequential 3+4 waits; a controlled
10/20 movement/exit span becoming 40/50 with a 30-update insertion, 17/27 after
editing to 7, and exact original bytes after removal; setup/boundary guards;
project persistence and two source-built exports/reopens. The authored export is
in parent Test ROMs/native-action-insert-vcseOx. References, bounds, CRC and master
preservation pass. `ui_tests sceneActionInsertion structuredSceneActions
cutsceneBackgroundSelection` passed in 3619 ms: Add/Remove, undo/redo, invalid-input
guards, project reopen and previous inspector/background behavior. The capture was
inspected. The additional branch/extra-yield guards passed with the timing/export cases in
2379 ms (candidate native-action-insert-LVMbTq); the two affected UI cases passed
again in 2117 ms. The complete native build passed without compiler warnings. These tests do not establish natural game launch or pixel accuracy.

## Native action fields

The Cutscenes Actions inspector derives structured fields directly from the native
scene program. It covers Actor creation/state/movement, both cameras, screen fades,
scale/opacity and exact registered-counter wait compounds, including A-skippable
waits. Positions and camera angles use exact three-decimal conversion from the
stored signed words. Durations remain game update counts, not guessed wall-clock
timestamps. Rows retain script order; native conditions still control execution.
Noncanonical encodings remain intact and read-only in the typed fields, with the
Commands inspector available. No second action document or browser compiler is
introduced into native editing.

A recognized wait retains its arm, handoff, barrier, yield, comparison and reset;
only its target changes. Unknown control arrangements are not silently flattened.
Structured timeline scheduling remains separate unfinished work.
The two inspectors share scene undo and selection. Incomplete input in either
blocks the other, scene selection and background editing, and reaches the existing
save/export guard. Movement tooltips explain the current-position sentinel and
speed/duration precedence from the maintained motion source.

`scene_action_tests` passed in2824ms: exact negative/32-bit coordinate conversion,
precision and overflow refusals, untouched-word and noncanonical-byte preservation,
ordinary/skippable waits with equality/greater-or-equal gates at1/2/30/120 updates,
A-skip and counter reset through the maintained native dispatcher. Its existing
original-MIPS qualification is reused; this is not a new full-game timing claim.
A real movement edit in resource01A14970 also persisted, compiled, exported and
reopened with valid references, allocation bounds, CRC and preserved master.
Candidate: parent Test ROMs/native-scene-action-MthcdN. `ui_tests
structuredSceneActions cutsceneBackgroundSelection` passed in2333ms, covering shared
selection/undo, incomplete-input guards, background regression and project reopen.
The Actions capture was inspected. The complete native build passed without compiler warnings.

## Cutscene background authoring

Background preview now offers current-ROM scene groups or environments, preview
before Apply, Restore original and shared project undo. The model retains the
scene's command framing and changes only its selector word. Actor positions,
cameras and other commands stay as authored. The image list includes foreground
pieces for fresh environment launches. Unapplied command input and the standalone
image catalog disable these editing controls.

`BackgroundEdit` permits one initial positive background request for supported
class4/5 room setups and class1/8 fresh environment setups. Missing, derived,
inherited, repeated or noninitial requests retain their source and explain why
selection is unavailable. Launch code and route-table identities come from the
qualified browser source reference; the generator records hashes, not original
code payloads. Options use current-ROM selector extents and known image layouts.
An edited stream must still produce the chosen selector in the raw-word C launch
scan. Qualified fresh world scenes retain their mode after stream edits or ROM
relocation instead of depending on a now-stale source hash.

`background_edit_tests choicesAndExport` passed in6960ms:172 offered selections
across actual class1/4/5/8 streams, selector-only byte changes, unchanged Actor and
camera bytes, foreground identities, malformed/inherited/code-owner refusal,
original MIPS pre-scan, project reopen and four rebuilt ROM streams. Resource
references, allocation bounds, checksum and master preservation pass. Candidate:
parent Test ROMs/native-background-choice-jZAxuA. The MIPS comparison controls
allocation/resource delivery; it does not claim natural game launch or final
rendering accuracy. `ui_tests cutsceneBackgroundSelection` passed in1087ms,
including pending preview, Apply, restore, undo/redo, project reopen and guards for
incomplete command input and image-catalog browsing. The capture was inspected.
The existing `image_tests imageLaunchAgainstMips` regression passed in1322ms, and
the complete native build passed without compiler warnings.

## Project change review

`Project::changeReport` compares the current project with its loaded source ROM,
not the last project save. Record fields retain their signed values, packed growth
lanes and catalog labels. Class additions, names/descriptions, promotion families,
shops, consumable effects and optional tools show readable settings. Encounters,
scenarios, scenes, dialogue and artwork identify their changed entries; binary
scene/artwork bodies are summarized rather than dumped. Private animation removal
is compared with source-owned groups. Sprite-library assets are listed separately
from their game assignments. Pending imported sections are explicitly unverified.

The File menu and toolbar open an asynchronous snapshot with find, copy and UTF-8
text export. Reviewing never changes the live project. The report states its
source-ROM baseline and excludes unapplied form inputs and the separate Save
workspace. Export rejects the source ROM, including equivalent canonical paths.

`change_report_tests recordsAndPersistence` passed: signed and packed fields,
restore-to-empty, class/family additions, consumable shop/icon identities, no binary
payload dump, no project mutation and identical folder/archive reopening reports.
`otherEditingAreas` passed in1518ms after correcting a one-based encounter-pool
iteration; the last pool is covered. It checks untouched loaded models, tools,
terrain/pools, scenarios/sites, authored scenes, dialogue, routes and persistence.
`ui_tests projectChangeReview` passed in1940ms: asynchronous menu entry, find/copy,
exact text export, unchanged project and refusal to overwrite a temporary source
ROM. The existing `loadDesktopWorkspace` check passed too. The capture was inspected and the complete native build passed without compiler
warnings. These are change-report checks, not additional game execution or export validation.

## Atomic saved-member swaps

The Squads page can swap ordinary nonleader characters between two existing
regular battery-save squads. Both retain their leaders, cells, flags, carried
items, roster records and order/activation lists. Both Unity values reset to zero,
matching removal of an ordinary nonleader. Formation and carrying capacity are
validated for each resulting squad before any edit reaches the document. A
character must have exactly one active squad reference and must not carry the
leader status bit. Empty/reserve moves, Soldier transfers, leader changes and
unit creation/removal remain separate operations.

`ClassData::carryingCapacity` follows resident ROM43100..43154, including the
primary/secondary selection. Existing carried entries must fit the new capacity;
the editor refuses swaps that would require returning items to inventory.
`SaveDocument::difference` produces one final set of byte spans after both packed
records and the checksum are composed. This avoids overlapping intermediate
checksum edits in undo. Formation/Unity/tactic edits waiting for Apply disable the
swap controls so staging cannot be silently discarded.

`save_document_tests squadFormation editsPreserveNativeSaves` passed in5784ms:
all27,225 original size/capacity pairs,12 controlled ordinary-removal Unity stores,
217 original formation conversions, edited packed data decoded by original code,
all three SRAM variants, both changed squads, full-memory preservation and exact
undo. Nonzero Soldier, order and activation data remain intact; leader,
empty/Soldier selection, insufficient capacity and runtime-format refusals pass. The existing scalar-save/calendar/RZIP checks also pass.
`ui_tests saveSquadEditing saveWorkspaceEditing` passed in2745ms, including pending
edit protection, atomic swap, undo/redo, export/reopen and the existing workspace
regressions. The complete native build passed and the member-swap capture was
inspected. This does not claim full Army
menu execution or natural game loading.


## Saved squad formations

The Save workspace now has a Squads page for the30 persistent player-unit rows.
It uses the original Army loader's flag selection, paired five-member/cell arrays,
zero empty members and zero-based grid positions. The first saved member is the
leader. Soldier group IDs100–219 remain visible and movable; they are not
mistaken for empty slots. Higher unknown IDs are preserved and their formation
changes are rejected. Formation moves can swap occupied cells before one
atomic Apply/undo operation. Unity and the four game tactics also edit there.

Regular battery slots0/1 support these edits. Suspend saves and runtime states
retain read-only squad views until their active copies can be coordinated.
Membership IDs, leader, flags, carried items and unknown bytes remain intact.
Soldier movement retains its indexed HP and companion data. Formation
validation checks distinct cells, five-place capacity and large-member spacing.
Size uses the original primary/secondary-class choice through `ClassData::unitSize`,
so independent secondary classes and edited class records affect validation.

`SaveDocument::planBytes` shares the existing packed-codec writing/checksum path
between scalar edits and atomic squad edits. It emits only changed file spans.
`save_document_tests squadFormation editsPreserveNativeSaves` passed in6256ms:
native/word-swapped/expanded SRAM, unchanged other slots, exact undo, invalid
formation/value rejection, original decoder, calendar and compressed-state
regressions. The follow-up `squadFormation` passed in1297ms with27,225 original
class-size pairs and145 original Army materialize/store comparisons, including
special IDs, leader pairing, large-member boundaries and edited packed data.
`ui_tests saveSquadEditing saveWorkspaceEditing` passed in2654ms; the final
`saveSquadEditing` check passed in478ms after the size correction:
staged occupied-cell swap, Apply/undo/redo, export/reopen, unknown-member refusal
and runtime read-only controls. The full build passed with no compiler warnings.
The updated capture was inspected. These checks
do not establish a natural Army menu launch or active-state cache coherence.


Soldier follow-up: original Army placement owner ROMC2F70..C37E8 was qualified
against the complete captured bytes, and its C2FEC..C3044 block selects class pair1/1
for Soldier members before coercing zero size to one. The saved Soldier arrays
contain120 entries for IDs100–219. `save_document_tests squadFormation` passed
in2065ms with range-end movement, full-memory preservation, original decoder,
217 formation conversions and the original Soldier size block. `ui_tests
saveSquadEditing` passed in909ms with Soldier movement/undo and unknown-ID rejection.
The full application build also passed.

## Initial scenario site counts

Site lists now use the original initializer's placement count and its six scenario
exceptions. The maintained C adapter is `game/scenarios/site-count.c`; the engine
checks the current ROM count block before applying it and reads the compact point
collection once, without decoding map imagery. Empty site selectors remain empty.

Scenario64 is the only nonempty retail resource whose header differs: it declares
one descriptor but the initializer consumes six. All six now appear in Sites and
Map, and allegiance edits retain the header, resource size and membership IDs.
Bytes beyond the actual active count remain preserved. Browser imports retain
their historical declared-header row mapping.

`map_tests initialSiteCounts` passed in800ms: all256 keys with six representative
u32 point counts, plus64 current-ROM cases, matched the original MIPS initializer
prefix (1,600 comparisons). Unknown count code, invalid placement selectors and
short SCINCSV resources were rejected. `ui_tests scenarioSiteCountEditing
scenarioWorldEditing scenarioMapEditing` passed in19,987ms: five newly exposed
rows, undo/redo, all six map-to-site selections, project save, compiled export,
allocation bounds, checksum and reopening; other scenarios and source ROM remained
unchanged. `engine_tests browserWorldImport worldAuthoringExport` also passed
in19,799ms, including shared-resource browser compatibility and compiled save/reopen.
The full build passed with no compiler warnings. The scenario64 capture was inspected. This establishes initial count
and editing behavior, not natural launch of that internal scenario or later
runtime-dependent site placement.


## Calibrated Scenario terrain references

The map canvas now draws49 current-ROM terrain images behind the existing world
coordinate grid, markers and route links. Terrain can be hidden independently of
Grid/Labels. The view labels this as approximate calibration. Placement bytes,
selection, pan/zoom, drag conversion, undo and export keep their existing paths.
The image is clipped to the playable coordinate grid; parts of that grid outside
the available illustration remain untextured.

`game/scenarios/map-images.json` contains numeric image/world rectangles and decoded
image identities. `native/tools/generate_map_image_alignment.py` generates it from
the parent `tools/native_map_image_probe.cjs` and `tools/native_map_alignment_probe.py`
report. The research inputs are the user's ROM and the existing manually adjusted
`wiki/scenario-map-calibration.json` reference registrations. The parent probe fits
24 image scales and all translations using masked normalized RGB correlation,
then refines independent X/Y scale and translation. Each image quadrant is also
excluded from both global search and refinement, then checked against that fit.
The complete57-image analysis took294.8seconds locally and reuses identical image
pairs. The product ships no reference artwork, NumPy or research-image dependency.

Included fits have overall correlation >=0.75, every excluded quadrant >=0.60,
maximum normalized rectangle-parameter drift <=0.015 and an existing site-fitted
manual reference. The49 selections pass these checks. Runtime source-image hashes
and dimensions prevent reuse of a calibration after the artwork changes. Unknown
or changed images continue to load as separate references. Keys39,47,48,51,52,53,
60 and61 retain that fallback: their reference images differ in terrain/building
detail or fail regional stability. Seven scenarios have no selected ROM image.
This is registration to an existing manual reference, not a recovered game image
projection or full terrain simulation. Original placement differences remain open.

`map_tests coordinateEncoding calibratedTerrain` passed in350ms: exact grid
conversion, all64 scenarios/57 image selections/49 qualified alignments, absent
images and a well-formed replacement image that must retain coordinates while
losing its old calibration. The replacement fixture remains in memory.
`ui_tests scenarioMapEditing` passed in1865ms: terrain visibility without project
or undo changes, zoom/pan/drag, all three movable object types, field navigation,
undo, project save, compiled ROM export, checksum and reopening. The map capture
was inspected. Its candidate is under parent `Test ROMs/native-map-edit-EtCNXW/`.
An old mangled separator in that test's treasure lookup was corrected during this
check. The original source ROM and stored scenario coordinate formats are unchanged.


## Ordered color dithering during artwork import

The shared image-preparation dialog offers an unchecked Ordered dithering option
for portraits, item icons, Army sprites, combat artwork with reduced color
precision, and imported private animation frames. Full-RGBA Sprite studio imports
retain their exact colors and have no dithering control. The preview shows the
actual pixels that assignment stores; cancellation leaves the project unchanged.
Stored pixel formats and the source-built export paths are unchanged.

The existing browser importer supplies the 4-by-4 ordered pattern. Native palette
assignment uses its offset in five-bit color steps. Portrait palette construction
is unchanged: dithering affects the final nearest-color lookup only when more
than80 native colors require reduction. Fixed palettes remain fixed. Direct RGB555
sprites adjust color quantization while keeping all256 separate opacity levels.
Direct channel endpoints remain0/255; alpha never receives the ordered offset.
Army/combat assignment still retains an existing exact pixel's duplicate index.
This adds the browser capability without adopting its entire quantizer or claiming
identical reduced palettes. No new dependency is introduced.

`artwork_dither_tests` passed in185ms: independently tabulated pattern outputs,
repeating coordinates, unchanged alpha/input, endpoint retention, sprite encode /
decode, portrait budget and stable palette, both icon packs, all six Army atlases
with both palettes, and indexed/RGB16 combat assignment. The browser decoder
independently qualified retail samples003bac1c:0 and006ca532:56. Retail descriptors
also contain non-sprite members, and that scan found noRGBA32 combat sample.
`ui_tests portraitDitherAssignment spriteLayerImagePreparation iconArtworkEditing
portraitArtworkEditing armyArtworkEditing combatArtworkEditing` passed in5498ms:
prepared portrait preview versus stored pixels, cancellation, undo, project reopen,
and existing default-off import workflows. The preparation capture was inspected.
`sequence_editor_tests importedFrameAndUndo` passed in932ms, including dithered
private frame pixels, undo and project persistence. No ROM export was repeated:
this changes image preparation, not stored resource formats or rebuild allocation.


## Stored sprite-color picker

Sprite studio can browse colors stored in the selected layer, current frame or
entire sequence and reuse an exact RGBA value for painting. Hidden layers and RGB
under zero alpha are included. The picker pauses playback, presents128 swatches
per page with keyboard navigation and RGBA labels, and does not edit project data
or undo history when choosing/cancelling a color.

The engine collects four-byte color words within the existing64-million-pixel
asset budget, sorts by alpha then RGB, removes duplicates and releases spare
storage. It avoids an unbounded widget population and the larger per-entry cost of
a hash set. Collection and sorting have cancellation checks. The dialog serializes
background jobs, cancels superseded scopes and accepts only the latest result.
Read-only access to the shared result avoids a large copy on the UI thread.
Workers capture asset/token values and retain no UI references after cancellation.

`sprite_library_tests usedColors` passed in7ms: hand-specified RGBA scopes, duplicate
removal, invisible layers, distinct hidden RGB, invalid selections, unchanged input
and cancellation during collection/sorting. `ui_tests spriteUsedColorPicker
spriteWorkspaceEditing` passed in501ms: rapid scope changes,128-color paging,
keyboard selection, exact alpha-zero painting, cancellation, unchanged project and
undo until painting, and the existing sprite editing regression. The picker capture
was inspected. Collection speed on maximum-budget projects has not been benchmarked;
these checks establish behavior and cancellation, not a worst-case timing guarantee.


## Repeat-briefing confirmation and shared preset queries

Main panel preset2 now opens the current-ROM repeat-briefing confirmation through
the same native text/confirmation controller as preset1. Its selector is0x10102;
Yes/No/Cancel and child lifetime remain native. Preset0 remains unsupported because
its zero section selector has no bounded match in the retail text scanner.

Original query ROM283700..283718 matches an owner's preset identity against any
attached entity's nested selector. The native query now does the same: another
attached copy keeps a same-preset owner live after its retained entity detaches.
The original updater controls only the foremost panel, then finds its owner by
selector. That behavior is intentional; controllers were not changed to run all
at once. Independent shared entity animation retains its existing ordering.

The native child renderer also had an incorrect ROM address for Yes/No, causing
garbage labels. Renderer ROM54AE4/54B38 reads live8018FE10/8018FE14. The qualified
resident load40E80->8016AF80 maps these strings to ROM65D10/65D14. The desktop now
reads those bounded four-byte current-ROM strings and rejects missing terminators.

`scene_runtime_tests confirmationPanelsAgainstMips panelsNativeReference` passed
in627ms:2478 new original-instruction entity/payload/query/audio snapshots over30
cases, including early input, directions, Yes/No/Cancel, overlapping owners,
duplicate presets and ordered release;788 earlier menu snapshots still agree.
The heap, text resource decoder and audio service are controlled; original
constructor, text builder, controller, query and entity updater execute. Retail,
altered and malformed confirmation strings are checked separately.
`ui_tests panelPreviewSeek` passed in9387ms across all five supported presets,
including repeat-confirmation input, cancellation, seeking, restart and unchanged
source scene data. The corrected Yes/No capture was inspected. Frame/font rendering
remains approximate; other unimplemented presets still report their missing owner.


## Display blackout and deferred reveal

Director commands84/85 now use a shared display-state owner. Blackout waits for
submitted graphics work to finish before clearing a pending reveal and selecting
black output. Reveal sets a request that only a type-four graphics completion
consumes; type-eight/sixteen completions leave it pending. The preview submits and
completes one task per committed render. It hides only the presented image, leaving
the framebuffer available for rendering and capture. State copies preserve pending
requests and task counts through checkpoints. The status line identifies blackout.

Maintained C follows the resident drain19E10, blackout19FC0, reveal1A050 and completion
19974..19A50 paths. NTSC display state is modeled; actual GPU/VI queue latency and
PAL scaling are outside the desktop renderer. `scene_runtime_tests
displayControlAgainstMips displayControlPlayback` passed in248ms:516 original
request/count/VI-bit snapshots, suspended dispatch without duplicated counter
updates, explicit completion, independent copies and unchanged input image pixels.
The instruction fixture controls queue receipt, callbacks and interrupt masks;
original drain/request/completion routing and the VI black-bit setter execute.
`ui_tests displayControlPreviewSeek titleRevealPreviewSeek` passed in1505ms,
covering blackout, reveal, status, repaint, restart and checkpoint restoration,
plus the title-reveal regression. The blackout capture was inspected.


## Title-scene edge reveal

Selector zero now uses the early title wrapper's counter/Director/reveal update
order instead of the Actor scheduler. Its two-edge record advances only during
update, supports replacement and final-state retention, and survives desktop
checkpoints. B7/B8 select the reveal's standard/high-resolution geometry. The
preview draws this overlay separately from the main cutscene scissor.

Maintained portable C reconstructs ROM290C50..291130. DMA setup at z64
2829BC..2829D8 loads ROM286BD0..29A4C0 into live8022AC90; this qualifies both
code and display-list data. The prefix at ROM299EF8/live8023DFB8 selects opaque
black primitive color and a primitive-only combiner. High resolution loads a CI4
board tile, but its texel cannot affect that combiner. Desktop rectangle coverage
and high-resolution downsampling remain approximate. Zero-duration retained draws
report the native interpolation division fault instead of inventing an instant
transition. No ROM/export data or new dependency is introduced.

`scene_runtime_tests titleRevealAgainstMips titleRevealPlayback screenCropAgainstMips`
passed in 509 ms: 2944 original title record/query/rectangle snapshots over 256
sequences, phased scheduler budgets, state copying and both resolution switches,
plus 1472 existing main-crop comparisons. Heap allocation, resolution lookup and
the independent board texture loader are controlled in the instruction fixture;
the constructor, update, query, interpolation and rectangle emission execute.
`ui_tests titleRevealPreviewSeek screenCropPreviewSeek` passed in 1632 ms:
edge positions, release, resolution, redraw stability, restart and checkpoint
seeking. The preview capture was inspected. Title menus and the People board still
require their own owners; their launch commands remain explicitly unavailable.
This establishes the reveal, not complete retail title-menu playback.


## Transparent animation export

Sprite studio also exports animated PNG with full RGBA pixels, authored durations,
integer scaling and finite play counts or continuous looping. The PNG writer uses
Qt's existing encoder and the animation chunks in the published
[W3C PNG Third Edition](https://www.w3.org/TR/png-3/#animation-information).
Each complete frame replaces the canvas, preserving transparent regions without
trails. Animated PNG keeps exact scaled dimensions, including tiny/odd images;
MP4 retains its separate opaque background and compatibility padding. No additional
codec or runtime dependency is installed. Cancellation preserves an existing file.

`sprite_video_tests` passed in 1165 ms, including the existing MP4 checks. The new
independent FFmpeg decode compares every RGBA byte in three frames with partial
alpha and a mostly transparent replacement frame, verifies all frame durations,
checks continuous/finite loop metadata, tiny scaled output and cancellation.
`ui_tests spriteVideoExport` passed in 271 ms for both formats, including static
PNG decoding of the first APNG frame and unchanged project/undo history. The static
comparison checks dimensions and RGBA bytes independently of color-space metadata;
the APNG explicitly declares sRGB. Transparent animation export uses APNG; the
native app does not currently emit WebM.

## Sprite video export

Sprite studio exports MP4/H.264 through Windows Media Foundation. Video entry
points load from the Windows system directory only during export, so unavailable
media components do not prevent the editor from starting. No codec executable or
Qt Multimedia package is shipped. The Windows SDK GUID library and COM are linked.
Implementation references: [Microsoft sink writer](https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--using-the-sink-writer-to-encode-video)
and [H.264 encoder](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-encoder).

The export worker snapshots the asset, composites visible layers over a chosen
opaque background, scales with nearest sampling and encodes 30 samples per second.
Each frame lasts max(1, ticks), matching Sprite studio playback. Integer scale and
repeat count are configurable; output is bounded to 4096 pixels per side and ten
minutes. Each side is padded to at least 64 pixels and even dimensions, preserving
the selected artwork scale. The Windows pipeline rejected tiny test frames; this
compatibility padding is displayed before export. RGB32 positive stride passed
independent orientation checks. H.264 is lossy; native assets/PNG retain exact art.

A cancellable background job writes into an owned temporary directory, finalizes
and closes the encoder, then copies into QSaveFile for atomic replacement. Failure
or cancellation preserves the previous destination, including during final copy.
`sprite_video_tests` passed in 2545 ms. An independent
installed FFmpeg decoder verified 12 frames, 0.4-second duration, zero-tick handling,
repetition, 36 color/orientation samples and padded odd-size output. Separate checks
verified cancellation, temporary cleanup and invalid-option destination preservation.
FFmpeg is only a development verifier and is not an application dependency.
`ui_tests spriteVideoExport` passed in 252 ms: export menu/options, background job,
finished dialog, output and unchanged project/undo history. The options screenshot
was inspected. Other Windows installations and missing-media-component systems have
not yet received clean-machine validation.

## Prepared sprite layer images

Sprite studio can import PNG/JPEG into the selected layer through the shared
fit/crop/stretch/pixel-size preparation dialog, with sampling, zoom and offsets.
The result retains the layer identity, position and dimensions, all other frames
and layers, and partial transparency. Cancel does not edit; accepting is one undo
command. A changed asset invalidates a stale open preparation dialog.

Nearest placement now shares an explicit pixel-center sampler with game-pose
library transfer. It preserves hidden RGB under zero alpha; Qt composition had
removed those editable values. Smooth sampling remains an explicit filtered option.
`ui_tests spriteLayerImagePreparation` passed in 657 ms: known RGBA placement,
cancel/accept, undo/redo, unchanged neighbors, folder reopening and stale-dialog
rejection. Existing spriteWorkspaceEditing, iconArtworkEditing,
portraitArtworkEditing, armyArtworkEditing and combatArtworkEditing passed after
the shared change. Sprite source placement/ROM comparisons passed in 134 ms.
The preparation screenshot was inspected. No ROM data changes until an asset is
explicitly assigned through an artwork editor.

## Game pose artwork in the sprite library

Class sequence editing and the cutscene pose inspector can copy authored frame
lists into independent Sprite studio assets. The shared engine conversion retains
frame timing, layer order and RGBA values, the selected body/equipment or actor
variant, staged private artwork, and a common canvas anchor across frames.
Flips arrive from the existing decoder; scale and fractional placement use explicit
nearest pixel-center sampling. Hidden RGB and partial alpha survive copying.
Empty/invisible frames retain transparent layers; library size/pixel limits apply.
The inspector uses its selected equipment/palette; the class shortcut uses defaults.

This transfers artwork instructions once in authored order. It does not bake runtime
loops, movement, audio, lighting, sepia or material programs. The UI states that
limit. Copying is one independent library undo command; it does not change the
ROM resource or assign a new game animation. Existing assignment controls remain
available for explicit transfers back into the game.

`sprite_library_tests gameFramePlacement gamePoseFrames` passed in 144 ms: hand-specified scale/placement/alpha pixels, fractional
sampling, empty/invisible frames, limits, exact decoded stock layer pixels, body
variants, cutscene art and asset-file roundtrips. `sequence_editor_tests
copyFramesToLibrary` passed in 870 ms after integration: retail and edited private
frames, unchanged game data, undo/redo, distinct identities and folder reopening.
`ui_tests actorPoseLibraryTransfer equipmentBodyPreview` passed in 1247 ms: selected
actor variant and equipment appearance against engine output, undo, persistence
and existing equipment preview regression. Both transfer screenshots were inspected.
These checks establish editable artwork transfer, not game execution equivalence.

## Browser private-sequence conversion

Browser `art.separations` documents from schemas one through four convert into native private
descriptors and attack assignments. Conversion validates current-ROM target/donor
references, complete sprite children and palettes, frame identities, dimensions,
timing, preserved control instructions and prior assignment baselines. It remaps
frame tokens and attack selectors, carries idle loops into their recovery consumers,
and retains Normal/Blocked restoration. New frame tokens can use the native wide
instruction. Linked owners retain secondary programs through the maintained pairing
model. Browser source files and compilation methods are not runtime dependencies.

Converted projects save an explicit native snapshot, including an empty snapshot
after removal. Older native projects that retained this browser section convert
on load; subsequent native edits and deletions do not trigger another import.
Malformed conversion leaves the project unchanged. Schema one reconstructs frame
structure and normalized body children from the current ROM; its embedded generated
binding vocabulary contains identities and roles, not game artwork. Schemas two
and three reconstruct unchanged control instructions and validate the original
frame identities/timing, including ordered frame deletion in schema three.

`animation_sequences_tests browserSequenceImport` generates its input using the
browser's authoring APIs and the supplied private ROM. It changes idle timing,
position and a child pixel, duplicates a frame, and imports Normal/Blocked programs
from another class. Native programs, every layer field, image policy, palette and
all child pixels/rendering match the browser's decoded export plan. Compression is
stubbed only in the reference planner; the native ROM export uses the real encoder
and compiler. Folder save/load, retained-project migration, malformed import
atomicity, export/reopening, unchanged export and persistent removal pass. The
fixture now covers four copied sequences in two groups, including a linked attack
and companion. Five historical fixtures cover schemas one/two/three, deleted frames
and an alternate body child; their native output equals the corresponding modern
conversion. Damaged historical dimensions/timing reject without changing the project.
The expanded check passed in 30854 ms. Candidate: parent
Test ROMs/native-browser-sequences-wVSzNy. This establishes asset/program conversion
and rebuild behavior, not full-game rendering or audio fidelity.
The full application build passed. Existing private-attack persistence/export passed
in 25132 ms. The general browser-project import check exposed an existing empty
portrait-section load against its synthetic ROM; an unloaded empty section now
remains a no-op. That regression passed in 782 ms after the correction.

## Compression search performance

The native LZSS encoder skips candidates that cannot improve an available distance
class and tests the first byte needed for an improvement before rescanning a long
repeated prefix. It preserves the exhaustive match result, nearest-distance tie
selection and existing optimal-cost parse. No search-depth cap or compression
quality tradeoff was introduced.

Profiling copied equipment sprites exposed the issue: 25984 and 27888 decoded bytes
took 13.783 and 31.643 seconds with the earlier encoder. The new encoder took 809
and 1221 ms with byte-identical compressed output. `engine_tests compressionCommands
compressionMatchBoundaries compressionSpriteCompatibility` passed in 2201 ms.
Checks cover original command fixtures, deterministic roundtrips immediately around
the 2048/16384/65536 distance limits and exact recorded output hashes for the two
private-ROM resources. The full local browser-sequence import/export/reopen workload
fell from 272948 to 39904 ms. Timings are observations on this PC, not release
performance guarantees. A per-plan decoded-byte cache also avoids repeatedly
compressing identical private sprite copies. The same three-sequence export fell
from 39904 to 26008 ms with a byte-identical ROM; this includes additional legacy
format checks.

## Inheritable body construction and fixed poses

Animation sets now carry independent body-construction and fixed-pose policies.
Stock class 135 uses Danika-body construction; 136 and 161 use tendril construction.
All three retain the fixed 0/50 base poses, except explicitly assigned private
primaries and companions. Creating a class or copying its animation set carries
these policies. The native controls support editing, undo and project persistence.
Ordinary private assignments now remember their primary identity even while the
fixed-pose policy is disabled, so enabling it later preserves those assignments.

LC05ARTE version 4 extends the existing 512-byte policy table with bit 10 for fixed
poses and bits 11..12 for construction (0 ordinary, 1 body, 2 tendril). Version 3
remains byte-compatible and derives stock construction from the class ID. Older
saved project rows likewise retain stock defaults. The shared query is at live
801C425C, within the existing equipment-registration allocation; it preserves
inputs and temporaries except v0/v1. The maintained C pose interpreter calls this
query while retaining the original signed-halfword source domain and -1 sentinel.
Invalid sources cannot wrap onto a valid policy. LC05POSE version 2 records this
dependency at header +24. No new tail-code allocation or high-RAM requirement is
introduced by these policies.

Four maintained symbolic assembly fragments replace original class-ID predicates
inside their existing second-Director allocations: z64 [2AB884,2AB8BC) construction,
[2ABD68,2ABD80) roster sticky state, [2AADC8,2AADF0) entrance skip, and
[2AAFA4,2AAFBC) entrance sticky state. The earlier owner-one/linked-owner priorities,
repeated-body behavior and unavailable-member handling stay intact. LC05VPOL
version 2 authenticates these four blocks alongside the five equipment blocks.
Version-one exports upgrade the shared query before any new caller uses it.
Native roster/entrance preview reads the same staged policies.

`body_policy_tests` passed in 88104 ms: three stock templates cloned into new IDs,
folder persistence, source compilation, export/reopen/no-op, neighboring bytes,
altered-code rejection, and upgrade from an equipment-only export with the old
zero-padded helper area. Original, rebuilt stock and cloned-ID executions agree
across 50 complete roster constructions, 3520 return-presentation snapshots, 168
boundary sequences, 964 speed samples and 2000 entrance snapshots per image.
Only the class identity fields are normalized for cloned-ID comparison. Native
preview replays all cloned rosters and entrances. Private-pose checks include
toggle-after-assignment, explicit existing poses, persistence/export, 90 synthetic
interpreter cases, 16 new-ID/domain cases and 432 original reader comparisons.
Candidates are in parent Test ROMs/native-body-policies-hTeAFC,
native-private-body-policy-VJyoBx and native-body-policy-upgrade-BfIWpt.

`ui_tests bodyConstructionPolicies equipmentArtworkPolicies` passed in 1445 ms;
the 1250x850 control capture was inspected. `presentation_runtime_tests` passed
all four behavior cases in 2772 ms. `animation_sequences_tests
fixedPosesAndWideTransfers privateAttackAssignments` passed in 56564 ms, retaining
195664 original pose-invocation comparisons. Resource allocation/terrain/audio
services are controlled in instruction fixtures; this is not full-game validation.
The final whole-tree build also passed. `animation_routes_tests
equipmentPolicyInheritance` passed in 50870 ms, including the moved private-map
table and all 400 image/100 full registration cases per image. `sequence_editor_tests
fixedOwnerPreview attackAssignmentsAndUndo linkedCompanionEditing` passed in 816 ms.

## Per-class equipment artwork policy

Animation sets now carry independent equipment artwork selection: use the equipped
item, use a fixed item-art lookup, and optionally treat item-colored layers as
equipment-child variants. Stock policies reproduce the existing class57..70 switches
and the kind-two exceptions for57/63. Copying a set or creating a class copies these
policies. The native controls edit and undo them; preview uses them for image layers
and body registrations. Older project rows without these fields keep their original
class-ID defaults.

LC05ARTEversion3 adds a512-byte policy table immediately after its preserved original
prefix and before private pose mappings. The mappings still directly precede class
handles. Each big-endian halfword stores the item-art index in bits0..8 (511 means
equipped) and equipment-child mode in bit9. Reserved bits and missing-row values are
validated. Existing loaders copy this bounded table with the complete resource.
Version1/2 data remain supported. Once loaded, a version3 table remains owned even
after individual policies are restored.

Three existing selection switches and two small image-context/predicate blocks are
assembled from maintained symbolic sources at user export, within their original
allocations. The blocks are z64[2076A8,20772C), [205CBC,205D38), [206370,206400),
[206470,206484), and[2064EC,206510); their source names begin `game/art/equipment-`.
They preserve the interleaved callee-save stores and the image routine's live cache
return paths. Reads use live class-handle pointer801D0690 minus mapping bytes and512,
with a source-domain guard. LC05VPOL at z6427C5100/96 authenticates map count, policy
hash and aggregate code hash. No tail-code hook or new high-RAM requirement is added.

`animation_routes_tests equipmentPolicyInheritance` passed53386ms: five templates
and added classes, all four body flags, five items, normal/sepia selection, folder
persistence, export/reopen/no-op, adjacent code and altered-owner checks. Original,
rebuilt and mapping-coexistent images each execute400 image selections and100 full
parameter registrations; outputs agree after normalizing only the source class ID.
Heap/resource transfer and final pixel expansion are controlled. Native frame pixels
also match templates. Candidate: parent `Test ROMs/native-visual-policies-kqCvPg/`.
`ui_tests equipmentArtworkPolicies` passed719ms, including undo, cloning and persistence;
its1250x800 capture was inspected. Body image/initialization regressions passed16192ms
(62,316 original image layers and all65,536 warm-color inputs); older route/import/export
checks passed30183ms, including4,800 selectors and complete loader copies. Assignment
and staged-preview UI regressions passed15427ms. Full-game/hardware validation remains
separate. Fixed-pose constructor inheritance and browser sequence import remain open.

## Linked class inheritance

Creating a class from linked templates 10/25/123 now preserves the template owner
in full-record byte 69 when the template stored zero. Zero would otherwise make
the new character's secondary class use its new ID, losing linked-body behavior.
The existing class-copy match then selects the new class's independent data and
animation routes while retaining the linked owner. Explicit nonzero edits remain
unchanged; further clones inherit the established value. This uses existing class
data and adds no new game code. The persistent-to-battle importer at z64 20D870
copies character primary/secondary class bytes to appearance row +48/+4C.

`animation_routes_tests linkedClassInheritance` passed in 12501 ms for all three
families, a second-generation clone, an explicit changed owner, all four body
routes, primary/secondary setup at poses 40/90, folder save/load, export/reopen
and unchanged export. Candidate: parent `Test ROMs/native-linked-class-clones-oRWQIM/`.
The existing `inheritanceAndLegacyProjects` case also passed. The first new test
fixture incorrectly marked class Actors as ordinary art; setting their real body
decoder mode corrected the fixture. Full and targeted builds pass. These checks
do not complete the separate fixed-pose constructor policy audit; equipment/image
inheritance is completed above.

## Private attack leader slots

The complete leader normalizer now rebuilds from `game/art/leader-slots.c` in
z64 [2AB574,2AB6BC), live 8023BE14. Its extended function and leading alignment
use 324 of 328 bytes. It calls the primary-only lookup in the body initializer,
which shares the same resident Director slab. Registered linked primaries can
move to persistent/deployed leader slots 0/1; companions and unregistered high
poses remain excluded. Each iteration reads the current slot after previous
swaps, and both moved Actors receive corrected slot fields. Native preview uses
the same eligibility and mutable scan. The pose interpreter's broader
primary-or-companion registration query is deliberately not used here.

LC05LEAD at z64 27C50A0 (96 bytes) stores version/count, mapping and code hashes,
and the compiled primary-query address. Export installs or restores body and
leader code together, guards ownership and preserves their neighbors. Earlier
LC05BSEL exports load and acquire the missing normalizer during export. The new
routine adds no RAM allocation or hook.

`animation_sequences_tests linkedPrivateAttacks` passed in 16107 ms: each of the
extended and restored normalizers matches 48 complete original slot scans and
16 explicit primary/companion cases; native preview matches all 128 results,
including whole Actor records. The existing 675 body-initialization comparisons,
export/reopen/no-op, restoration and corrupt-code checks also pass. Candidate:
parent `Test ROMs/native-linked-attacks-dNAyre/`. `presentation_runtime_tests
rosterMaterializationAgainstMips rosterMutableLeaderScan` passed in 401 ms (50
original materializations and three mutable-scan cases). The combined fixed-pose
export regression passed again in 23486 ms, candidate `native-private-pose-runtime-EyuYaw`.
No full-game or hardware claim follows from these controlled instruction checks.

Correction to the parent command reference's older wording: the original
normalizer skips linked-owner poses **at or above 50**, not below 50. Original
instructions at live 8023BE80..8023BE8C and the comparisons above establish this.

## Fixed private poses and wide transfers

Fixed sources 135/136/161 now accept registered private attack poses while retaining
the original 0/50 clamp for all unregistered states. Registration covers both the
primary and its linked companion, including a fixed source edited to use a linked
owner. A pair value of -1 registers a primary without a companion. Preview reads
the actual class body-owner field; on the canonical ROM, class 160 uses owner 19,
while 135/136/161 use their own IDs when that field is zero.

Companion self-transfers now use opcode 22 with a little-endian 16-bit target.
Validation rejects missing targets, states outside 0..32767 and delay-free cycles.
Copies preserve unrelated transfers. Wide execution remains installed while any
private program needs it, even after its attack assignment has been restored.

Maintained `game/art/pose-advance.c` and `body-record.c` compile during user export
into the original z64 allocations [29E218,29E70C) and [204F34,2050AC), respectively.
Their public live entries remain 8022E9E8 and 801C1AA4. The common reader also holds
the registration query, so playback does not depend on the second Director slab.
The extended reader uses 372 of 376 bytes including leading alignment; only zero
linker padding outside the allocation is discarded. Opcode 22 gets width 3 in the
existing 48-byte width area at z64 212370; its other unused bytes are preserved.
No additional high-RAM code allocation is introduced. Existing OBSO assignments
retain their Expansion Pak requirement.

LC05POSE at z64 27C5000 (160 bytes) authenticates map count/hash, wide flag, query
address, both compiled owners and width data. Source validation distinguishes
loaded ROM programs from staged edits. Removal rebuilds baseline C semantics;
this is not byte-identical retail code restoration. Unknown owners reject safely.

`animation_sequences_tests fixedPosesAndWideTransfers` passed in 21492 ms: fixed
sources, linked self-transfer, combined fixed/linked owner without opcode 22,
folder import, export/reopen/no-op, removal, altered-code rejection and adjacent
allocation conservation. Compiled execution matches 195,664 original invocations,
with controlled resource and sound services. Three export variants pass 99/81/90
synthetic policy cases and 432 reader comparisons each, including null/aliased
operand pointers. Candidate: parent `Test ROMs/native-private-pose-runtime-FQoDki/`.
`pose_bank_tests` passed in 109 ms; five existing sequence regressions in 61374 ms;
body/Actor regressions in 8422/2571 ms. Existing UI cases passed, and the corrected
`sequence_editor_tests fixedOwnerPreview` passed in 271 ms. Its 1400x1000 capture
was inspected. Full builds succeeded without new warnings. These checks do not
establish full-game heap or hardware behavior. Leader normalization is completed
above; fixed-ID template inheritance and browser sequence import remain active work.

## Private linked-body attacks

Private attack copies for linked body owners10/25/123 now retain a primary/companion
pair. Primary selectors remain100..255; companions append at256 or above. The first
paired copy reserves the unused primary interval before padding the bank to256.
Subsequent copies use unoccupied reserved states, so edited padding is never overwritten.
Each group saves `pairedPoses` and `reservedAttackStart`, clones them with the class,
and validates both programs. Companion self-transfers use the wide transfer described above.

LC05ASEQversion3 stores group metadata at record+12: two big-endian halfwords give
baseline-row count and reserved-primary start, followed by the existing8-byte baseline
records and sorted4-byte primary/secondary pairs. Earlier version1/2 resources remain
accepted. Active private assignments generate sorted8-byte runtime rows: source, owner,
body flags, primary selector, signed-halfword companion, two zero bytes. LC05ARTEversion2
places those rows immediately before the relocated class-handle table. Its original
prefix size, full resource hash and repacking validation establish that extent; existing
full-resource loaders copy the map along with the tables. No additional high-RAM hook.

`game/art/body-selection.c` is a reconstruction of the complete Director body initializer,
compiled during export into its original z64[2A9364,2A985C) allocation, live80239C04.
The linked build occupies1260 of1272bytes, including the historical linker's leading
NOP alignment. It preserves appearance allocation/getters, registration, immediate pose,
and recursive unresolved arguments. Registered extended primaries select their mapped
companions; unregistered states retain the original secondary skip. SceneActors applies
the same mapping before its retained C record boundary. LC05BSEL at z6427C4F00 authenticates
map count, map hash and compiled code hash. Unknown code/header owners are rejected.
Removing all active mappings compiles the baseline C behavior in that same allocation;
it does not claim byte-identical restoration of the retail function. No compiled game
payload is shipped. OBSO attack overrides retain their existing Expansion Pak requirement.

`animation_sequences_tests linkedPrivateAttacks` passed11212ms. It checks project folders,
paired preview, export/reopen, unchanged export, altered-code rejection, neighboring code
conservation, assignment restoration and further copies after reopening. Its instruction
fixture compares both mapped and baseline C against675 original body-selection cases,
then compares100/256 and101/257 against original40/90 including before/after Actor bytes,
appearance rows and registration calls. Unregistered102 leaves its secondary unchanged.
Candidate: parent Test ROMs/native-linked-attacks-GFliUt/masterfilename.
The four existing sequence regressions passed42935ms; original body initialization and
command atomicity passed912ms. `sequence_editor_tests` passed2493ms, including independent
companion timing, selection, undo and removal. A final229ms companion-only check and inspected
1400x1000 image verify the shortened assignment summary and actual companion frame preview.
These are instruction-model/desktop checks, not full-game heap or hardware validation.
The later fixed-pose work above extends this implementation. New-class visual template
policy and browser sequence import remain in progress.

## Private attack assignments

Private sequences can duplicate the selected pose into an independent Normal or Blocked
attack selector. The other lane retains its effective assignment. PoseBank.appendProgram
preserves prior programs and aliases, validates progress, and rejects trailing end-of-resource
pointers whose meaning would change. New ordinary attack copies begin at100, retaining the
original primary/secondary ranges. The existing override patch still has one-byte selectors
and128 rows and requires an Expansion Pak; no new execution hook is added by this change.

Project publishes sequence/override changes atomically. Each private class/body route retains
its prior exact-body override (or absence), once per action. Restore assignment and Remove
private copy restore that state; bypassing restoration or deleting a managed override rejects.
Undo restores both models and the selected appended pose. The native controls show current
Normal/Blocked selectors. Saved groups carry optional attackBaselines; runtime LC05ASEQversion2
uses record+12 for a raw length-prefixed baseline resource. Its sorted8-byte records contain
action0, exact1, priorNormal2, priorBlocked3, zero4..7. Standard allocation/reference/hash checks
include this resource. Version1 remains used when no baselines exist, preserving earlier ROMs.

`pose_bank_tests` passed111ms with appended wide-token playback and all234existing descriptor
roundtrips. Full `animation_sequences_tests` passed31029ms, including version2 export/reopen,
no-op, restoration, prior/default assignments, marker cases and imported-sprite conversion.
A subsequent privateAttackAssignments run passed10677ms with class165 export/reopen and
baseline inheritance added. Candidate: parent Test ROMs/native-private-attacks-JMnTrT/masterfilename.
`sequence_editor_tests` passed2172ms, including assignment controls, atomic undo, pose selection
restoration, private-copy removal and all frame-import/layer regressions. The1400x1000 capture
was inspected. Build37833 passed without compiler warnings after renaming a shadowed local.

This initial ordinary-body implementation is extended by the linked and fixed-pose
work documented above. No full-game heap/hardware validation is claimed.

## Private frame image imports

Private sequences import PNG/JPEG or a sprite-library frame through image preparation,
then append a new sprite and single-layer metadata frame before the selected instruction.
Existing artwork and metadata remain intact. The inserted frame inherits the selected layer's
position, has independent timing/layer controls, and uses shared undo. A5554 creation path
encodes RGBA5551 color with a separateI8 opacity lane, padded by native row strides. No palette
quantization limit is introduced for these direct-color imports.

AnimationDescriptor.appendSprite supports body/equipment/palette-selection policy and a
complete512-byte palette bank. Banks deduplicate or append through256; art ordinals can grow
through65535. Configuration growth inserts the two byte-array entries, updates count/map
offset, and preserves all other bytes. Header4/8/12 remains unchanged: the exercised original
constructor/converter sizes its caches from count and allocates image data dynamically.
Saved optional extraSprites carry decoded containers/policy/palette before frame/pose import;
older snapshots remain accepted. Reopened exported ROMs treat their appended resources as
loaded originals. Existing source-built allocation ownership handles the larger descriptor.

`animation_descriptor_tests` passed296ms: known six-pixel color/opacity results and padding,
child fallback, invalid input, new bank/reference, original-config conservation, native frame
preview, snapshot roundtrip, palette reuse and edits to added sprites.
`animation_sequences_tests projectExportAndOwnership` passed12499ms: appended art107/frame33,
source export/reopen, project/archive, independent class165 inheritance, no-op, relocation,
removal and ownership checks. Candidate: parent Test ROMs/native-private-animations-wpYeYG/
masterfilename. `private-sprite-mips.cjs` additionally executed the original constructor,
layer/image selection and pixel conversion in4MiB modeled RAM:108art entries, correct child/
palette/newresource, exact48-byte converted image and intact allocation/request guards.
`sequence_editor_tests` passed1839ms: import/cancel, sprite-library transfer, preview stepping
to added art107/108, persistence and undo, plus layer/marker controls. The1300x880 capture was
inspected with Windows fonts. Build45041 passed without new warnings. These checks are not
full-game heap/hardware validation. Private attack assignment and browser sequence import remain.

## Expanded equipment-marker frames

Private marker poses38/88 now accept opcode21 frame tokens. Export compiles maintained
assembly into z64 [00205FD8,002060B4), live [801C2B48,801C2C24), the original220-byte
scan allocation. No tail hook or additional RAM is needed. The scan retains the last two
frame tokens and native instruction widths, and checks pose-table bounds for counts below39
and exactly88. Export authenticates code ownership; removing the need restores the original
loops from source. This supersedes the temporary rejection described in the earlier entry.

Staged and reopened previews use the same substitution behavior. The layer dialog applies
its proposed marker change before rendering, and identifies substitution poses in the UI.
`animation_sequences_tests` passed29855ms:137 original descriptors,390 instruction fixtures
in4MiB modeled RAM, frame256/88-entry source export, pixels after reopen, no-op export,
class165 cloning, code restoration, ownership/corruption guards, and existing private-resource
checks. Candidate: parent Test ROMs/native-wide-markers-oSWByq/masterfilename.
`sequence_editor_tests` passed1117ms: marker draft pixels/positions and undo, plus normal
layer/timing controls. The first new UI check needed the same Qt premultiplication conversion
on both images; the missing test include was also corrected. Final build26229 passed.
These are file, instruction and desktop checks, not full-game heap/hardware validation.

## Private frame layer authoring

Private sequences now open a frame-layer dialog with live native body-image composition,
an origin marker, sprite ordinal, signed position, dimensions, flips and fixed-point scale.
Layers can be added, duplicated, removed and reordered. Other flag bits survive edits.
The dialog validates its draft, cancellation changes nothing, and acceptance appends a frame
and redirects only the selected pose instruction in one shared undo command. Unknown original
metadata and other poses using the source frame remain intact.

The native equipment-marker consumer scans only opcode1 in poses38/88. New edits introducing
opcode21 there currently reject before publication/export, even when its token fits a byte.
Unchanged absent marker programs remain allowed. This guard still needs the consumer update
for full wide-marker authoring; it is not a completion claim for that runtime path.

`sequence_editor_tests` passed967ms: modal controls, live pixel redraw, positions/flips/scale,
layer insertion/removal/order, original-frame isolation, appended pose reference, oversized
layer rejection, cancellation and undo/redo. The1040x760 dialog screenshot was inspected.
`animation_sequences_tests` passed14247ms, including all656 unchanged class/body copies,
wide-marker rejection and the complete source-export/reopen/ownership regression. Candidate:
parent Test ROMs/native-private-animations-NRyJAM/masterfilename. Desktop load passed937ms.
Build86866 passed without compiler warnings. Sprite creation/import, private attack-selector
assignment, browser sequence import and the fixed-class visual-policy audit remain.

## Appended frame metadata

AnimationDescriptor can append validated layer records and expand the relative-offset frame
directory. Every original pointer moves by the directory growth; the complete original body,
including unknown bytes, is retained. Editing one pose instruction can therefore select a
new frame without changing other instructions sharing the old token. Saved descriptors carry
metadata before pose validation. Import accepts older three-field snapshots; new metadata
must retain original pointers/body and claim all added rows contiguously. Invalid art indexes,
dimensions, overlaps, original-byte changes and excess data reject atomically.

`animation_descriptor_tests` passed208ms: copied Fighter frame17 with first-layerX+11, all
original frames unchanged, native body layer coordinates, JSON/legacy roundtrip, invalid edits,
and frame256 through the actual opcode21 pose interpreter. `animation_sequences_tests` passed
16088ms with private frame32/X+5 through source export, reopening, clone/growth/removal and
ownership checks; candidate native-private-animations-vyOQRf. Sequence UI regression passed774ms.
Build50512 passed. These checks cover ordinary pose40; the equipment-marker consumer scanning
poses38/88 still recognizes only opcode1 and needs a compatibility fix before wide marker
frames can work. Layer authoring controls and additional sprite bindings remain in progress.

## Private sequence authoring controls

The Art workspace includes Private sequences: searchable class/body-route/pose selection,
make/remove copy, frame token and duration, insert/duplicate/remove, reorder and shared undo.
Control records remain visible and retained. Commands validate a candidate before publishing
it. The embedded ActorPoseEditor follows the selected route/pose and keeps its current step
across edits and undo. It retains equipment, palette, sepia, playback and projection controls;
redundant source selectors are hidden in this context. The standalone inspector keeps them.

`sequence_editor_tests editPreviewAndUndo` passed752ms: actual controls alter current pose
bytes, duplicate/reorder/remove frames, preserve preview step2 through edits/undo/redo, reject
control-record removal and unmatched class search without changing undo/model, remove/restore
private copies, switch body routes, persist and inherit into class165. Its1300x880 screenshot
was inspected. `ui_tests loadDesktopWorkspace stagedAnimationPreviewAndUndo
 equipmentBodyPreview` passed1898ms after the layout changes. Build47940 has no compiler warnings.
Structural layer/frame authoring, private attack selectors and browser sequence import remain.

## Private class sequence storage and export

AnimationSequences owns one private descriptor per class/body route. Project snapshots,
folders/archives, class-template copying and BodyAnimationData preserve private poses and
pixels independently. The editor retains baseline route assignments; exported ROMs point
to expanded direct handles. Recognized private descriptors are excluded from CombatArtwork
so two allocation owners cannot modify the same members. Preview undo observes the sequence
snapshot. Changing the underlying route requires removing its private copy first.

LC05ASEQ v1 at z64 027C4E00 is96bytes: allocation offset12,size16,count20,reserved24..63,
SHA-25664..95. Sorted16-byte records hold class/body flags, baseline/private handles and a
runtime descriptor key. Each descriptor owns all controls and sprites. Identical resource
containers share allocation storage; descriptor identities remain separate. Loading checks
bounds, references, hashes, full coverage and FF padding. Export preserves unchanged compressed
containers, recompresses changed resources, compiles the complete generated C payload and
directory, and then compiles the effective class routes. No new execution hook is introduced.
Cleared expansion handles are reused; the directory grows when needed. Removal clears only
owned data and restores baseline routes. Readback compares every control resource and sprite.

`animation_sequences_tests` passed15322ms: private-route timing versus the actual unchanged
alternate route, native folder/archive restoration, rejected route conflicts, source-built
export/reopen/no-op, newclass165 inheritance with an independent handle, all private sprites,
original-byte conservation/CIC-6102, relocation around a later marker, removal, handle reuse,
corruption/occupied-region/destination guards and duplicate-record atomicity. Candidate:
parent Test ROMs/native-private-animations-gxhrDa/masterfilename. An initial fixture assumed
the alternate Fighter route used the same frames; it now compares its actual original pose.
An earlier run was stopped because unchanged sprites were needlessly recompressed; the
container-preservation change eliminated that cost. Existing route tests passed18954ms,
combat artwork13695ms, descriptor preview107ms, and staged-animation/class-assignment/override/
new-class UI regressions15787ms. No full-game heap or hardware execution claim is made.
Sequence authoring UI, structural layer/frame edits, private attack selectors and browser
sequence-project import still need integration.

## Private descriptor snapshots

AnimationDescriptor captures descriptor controls and staged sprite edits without changing
shared resources. ActorArt binds the snapshot while retaining body source/owner/variant
and equipment selection. Private previews ignore subsequent global artwork changes;
content identities keep renderer caches separate across private edits. Changed pose entries
validate progress and metadata tokens before publication. Unknown original records stay
intact. This is the preview foundation; project authoring and export integration remain.

`animation_descriptor_tests` passed115ms against Fighter resources: independent pose40
first-frame timing, captured pixels, later shared edit isolation, rejected missing frames
and malformed sprites, and renderer cache redraw. Body program/frame and equipment checks
passed11437ms; staged-animation/equipment UI checks passed1157ms. The first render test
used the wrong write width for the32-bit frame field; correcting that fixture made the
check pass. These checks do not establish authored-sequence ROM export or game execution.

## Editable counted pose programs

PoseBank now owns the counted-program codec used by ActorArt. It preserves physical
empty slices (forward aliases), count-zero programs, every instruction byte and existing
wide-frame encodings. Editing a shared program materializes the last preceding alias,
so the other states retain their previous behavior. Frame token/timing edits, insertion,
removal and reordering preserve non-frame records and remap record-index jumps. Removal
retargets a deleted frame to its next instruction or the implicit end. Opcode21 stores
the low token byte first, unlike ordinary ROM halfwords; smaller replacements keep an
existing wide opcode. Programs remain bounded to255 instructions.

Candidate edits follow deterministic control flow from the edited entry, including
state changes and continuations after delays. A reachable cycle with no delay, or a
transition to an absent state, fails atomically. Unreachable original instructions are
retained. Zero-duration frames remain allowed when execution reaches a delay/end.
Caller layers still need to validate metadata/art references and class-specific pose
policy. This codec is the sequence editing foundation, not the finished private-sequence
project/export/UI workflow.

`pose_bank_tests` passed107ms. Explicit programs executed by the native pose interpreter
establish expected frame order/timing, wide token300, alias isolation, loop remapping,
invalid edit atomicity, cross-state cycles and the255-instruction boundary. Across234
current-ROM descriptors,8790 pose-table entries and42274 instructions re-encode exactly.
Existing `body_art_tests bodyProgramsAndFrames` passed10511ms; desktop staged-animation
and equipment previews passed1096ms after replacing the original parser. No ROM export
or full-game behavior for authored private sequences is claimed by these codec checks.

## Expandable direct descriptor directory

DirectDescriptors owns the twelve-bit direct-handle directory used by class bodies,
combat artwork and Stage art. The retail root003B6CD0 has208 entries. A complete
LUI/ORI resource-key scan found12 operand pairs in6 loaders and no aligned literal
keys. All6 request the directory's resource size before allocating/reading it; no
fixed208-entry allocation or new loader hook is required. Native ActorArt, AnimationRoutes
and CombatArtwork now discover/authenticate the current directory through this owner.

LC05DRTE v1 at z64027C4D00 is96bytes: allocation offset/size at12/16, entry count20,
reserved zeros24..63, SHA-25664..95. A length-prefixed raw directory is aligned to16
with FF padding. Original data stays intact. Export requires compiled generated C
data, updates all12 existing key operands without changing opcodes/registers, guards
original directory/preimages, and authenticates readback. Tail growth/reuse and movement
around another owner's allocation are supported through4095 entries. Class-owner
canonicalization authenticates these sites independently of ClassData. This is the
allocation foundation for private sequence descriptors; sequence authoring is separate.

`direct_descriptors_tests` passed12764ms: compiled257-entry directory, original-byte
conservation/checksum, all6 original loader slices (18 first/fixed/last-handle cases),
new class165 using handle257 through public source export/reopen/no-op export, compiled
4095-entry growth, later-marker preservation, bounds/corruption/occupied/preimage guards.
The one fixed-handle loader retains handle37. Loader service memory is modeled within
4MiB; full-game heap/hardware behavior is not established. Candidate:
parent Test ROMs/native-descriptor-directory-lhhnqO/masterfilename.
CombatArtwork full regression passed11505ms, including38 browser materialized/delta
comparisons and source export/reopen. The staged animation, equipment and story-cast
UI regressions passed1321ms. No new automatic Expansion Pak requirement is introduced.

## Staged class animation previews

BodyAnimationData is an immutable snapshot shared by Actors and playback checkpoints.
It holds current class records, compound animation assignments and authenticated
Normal/Blocked override rows. ActorArt and ActorInstance bind the snapshot, SceneActors
uses its equipment defaults and movement speeds, and drawing uses its class-size
fields. New class IDs therefore preview before export. Unknown override ownership
prevents action resolution; explicit pose inspection remains available. Pending body
assignments fail visibly rather than displaying an unrelated template.

The pose inspector can resolve an action for the selected source, owner, body flags
and raw mode0/1/2. Its default selector matches original ROM201798, followed by the
existing OBSO first-match rules, including exact original selectors and body flags.
Direct cutscene pose requests retain their explicit states. Class/animation undo reloads
the inspector at its current step and replays scene snapshots to their current target
with recorded input. Pure artwork changes do not rebuild animation snapshots.
SceneCommandEditor now emits a program-change signal on undo only if scene data changed;
unrelated edits had previously reset scene selection/playback through catalog refresh.

`animation_override_tests stagedBodyAnimations` passed3561ms:4824 current-project
selector cases match original instructions followed by the freshly assembled patch,
including new IDs, owner fallback, all raw modes and exact body routes. Snapshot lifetime,
pending assignments, body creation and speeds also pass. The9216 patch lookup checks
remain successful. Body equipment/atomicity/all programs and frames passed11215ms;
presentation lifecycle/missing-context checks passed2339ms. `ui_tests
stagedAnimationPreviewAndUndo sceneCommandEditingAndPreview sceneDraftSurvivesEventUndo
authoredSceneCreationControls` passed2140ms: panel edits, Normal/Blocked resolution,
new-class projected rendering, undo and identical scene replay at the same tick.
Earlier artwork/equipment/story-cast regressions passed1397ms. The1280x850 screenshot
was inspected. These are instruction/resource and offscreen Qt checks, not full-game
or hardware validation. Sequence authoring and fixed-ID visual-template behavior still
need their own implementation/audit.

## Existing combat override compatibility

AnimationOverrideImage authenticates retail and OBSO-v1/v2/v3 independently of
ClassData. It checks bootstrap, hook, return and six carrier sites, immutable module
identity, table hash, flags, order, duplicate keys and unused records. Unknown or
partial owners remain identified as unavailable; bytes are preserved. V1 rows expose
their original single selector as both Normal and Blocked. This binary inspector
does not make advanced exact-original records editable.

Class-owner fingerprints now canonicalize authenticated override instruction sites.
Validation stays independent of ClassData to avoid recursively loading a class
owner through its selector-return hook. Existing owned override bytes can coexist
with a new-class source rebuild and reopening.

`native/tools/generate-animation-overrides.py` translates the parent maintained
OBSO producer to seven symbolic assembly sources. Generated metadata holds compiler
bindings and compatibility hashes, not compiled module bytes. Named linker symbols
are required for historical GNU J/JAL assembly. This developer generator is separate
from the user's export path; users will not need a parent or Decomp checkout.
`node native/tests/animation-override-source.cjs` passed: all seven fragments match
the accepted patch,9,216 lookup cases,16 mode-return cases, null actors and four
bootstrap paths including controlled PI DMA and resident reuse. The complete128-row
table and a match in its final slot are exercised. The existing patch uses8MiB RAM;
these checks do not establish full-game heap room or hardware timing.

`animation_override_tests` passed5,763ms: retail/all historical versions, corruption,
mixed instructions, duplicate/undeclared/malformed records, unchanged region guards,
class readers and an added-class public export with an installed v3 patch.
AnimationOverrides now holds logical class/action/Normal/Blocked/body-route entries.
Project folders/archives and browser version14 single-selector or version15+ paired
entries import into this model. Explicit native empty snapshots suppress older
imported rows. Available pose choices come from current resource programs across
all four routes; count-zero programs are empty. The old catalog only enumerated six
candidate IDs. Fourteen retail classes additionally have a nonempty0x2D program;
independent browser parsing confirms the expanded current-resource choices.

The ordinary owner is full class byte69 when nonzero, otherwise the class ID. Most
ordinary classes (including Fighter) store zero, so copying Fighter to165 gives
owner165 by default. The unrelated named-alias byte is not this mapping. Changes
to that class byte rebind logical overrides during export. New-class template
copying inherits editable overrides atomically; an advanced raw override affecting
the template prevents an incomplete copy, while unrelated templates remain usable.
Advanced/foreign data stay preserved and read-only.

The exporter compiles the module header/table from generated C and every instruction
from the maintained assembly, authenticates the source regions, and reads back the
result. Installation/update uses OBSO-v3; removal compiles original site instructions
and clears only the authenticated bootstrap/module. No precompiled N64 payload is
bundled. The existing8MiB requirement is explicit in the export report.

`animation_override_tests logicalEntriesAndCurrentSelectors` passed951ms: all164
classes against current browser resource parsing, retention of old choices, changed
body assignments, new IDs, zero/nonzero owner contexts, native/legacy serialization,
capacity/duplicate/compatibility failures and historical logical decoding.
`sourceExportImportAndReset` passed13,347ms: native folder/archive and browser import,
reset preservation, source-built installation, exact byte conservation/CRC, browser
readback, reopen/no-op export, template inheritance and removal preserving later
allocations. Updated historical/advanced guard checks passed13,070ms; the complete
animation_routes_tests regression passed26,738ms. The native Attack overrides panel now supports class/action/body-route selection,
Normal/Blocked choices, add/update/remove/remove all, shared undo and advanced-row
read-only display. Selection changes cannot reselect the previously current table row;
unmatched search input and duplicate keys leave the model/undo stack unchanged.
`ui_tests attackAnimationOverrides classAnimationAssignments addClassUndoRedo
loadDesktopWorkspace` passed18,788ms: actual controls, class-template inheritance,
shared undo between both animation panels, project persistence and the existing class
animation source-export regression. The1100x820 screenshot was inspected. Staged
animation previews are integrated as described above; sequence authoring remains outstanding.

## Expandable class animation data

AnimationRoutes owns the five class-indexed sections of raw resource00315736:
four body handles, two movement-speed bytes, one retained opaque byte, two
retained opaque bytes, and default action-selector groups. Retail has172 rows,
although ClassData has165. New classes clone the full current template Entry;
expansion reaches the one-byte class domain without overwriting unrelated sections.
Saved projects carry explicit entries. Older added-class documents can recover an
exact protected72-byte template; ambiguous or unknown templates remain pending and
export refuses them until the user assigns a source.

The full original11008-byte compound resource stays as a prefix. Appended tables
change only header indices0,1,3,7,8 and15. Header15 is the complete resident copy
size; headers are independent offsets, not adjacent begin/end pairs. In particular,
header11 still locates another live section and cannot be repurposed as a larger
copy size. The two short loaders at z646E810 and1B902C originally used header11 for
both allocation and copying. Maintained game/art/animation-copy.s compiles two LHU
fragments during export to use header15 instead. They replace the four existing
instructions in place. The combat loader201BAC already reads header15, and the
Stage loader1EA0B8 retains the entire resource. All eight LUI/ORI key pairs move,
including resource-read keys whose ORI executes in a JAL delay slot. No new loader
hook or fixed high-RAM allocation is added.

LC05ARTE v1 metadata at z64027C4C00 is96 bytes: allocation offset/size at12/16,
original prefix size20, row count24, original36-byte header28..63, allocation hash
64..95. The allocation is a size-prefixed raw container aligned to16 with FF fill.
Readback authenticates links, copy operands, hash, padding and regenerated layout.
Growing at the ROM tail reuses the allocation; growth around another owner moves
it and clears only authenticated old storage. Class resources and the copy-load
fragments compile during user export. Native body speed, equipment variants and
Stage placement readers resolve the relocated resource; other sections remain intact.

`animation_routes_tests` passed17,201ms: inheritance, legacy ambiguity, folders and
archives, source export, all four handles, speeds, unchanged Stage data, class-data
coexistence, checksum, reopen/no-op export, further class creation, row255, ownership
corruption and tail/non-tail growth. Its `animation-routes-mips.cjs` executes all four
original resource loaders with controlled heap/resource services. The new14,772-byte
allocation, copy guards and all published pointers agree, and4,800 original selector
calls match expected results including new classes and source/owner fallback. The
model uses4MiB RAM; this establishes the consumer contract, not full-game heap room.
Class-specific equipment and secondary-body branches still need a template audit.
The Art workspace now exposes Class animations: searchable source/class selection,
complete template copies, four body routes, movement speeds, default action-selector
add/update/remove, and shared undo. Pending older projects can assign their source
here. Unknown companion bytes stay with their set; they are not given guessed labels.
Typed search text must resolve to the chosen item before a command can change it.
`ui_tests classAnimationAssignments addClassUndoRedo loadDesktopWorkspace` passed
14,840ms, including control edits, pending-source undo/redo, invalid handles and typed
searches, duplicate actions, project folders, source export and ROM reopening. The
1100x820 panel screenshot was inspected. The existing Normal/Blocked override and
sequence patch workflows, and staged animation previews, remain separate work.

## Combat artwork resource editing

CombatArtwork reads the current-ROM direct descriptor directory: retail has 208
handles, 166 unique descriptors and 9,965 artwork bindings. Pixels decode on demand.
A binding owns its edits independently of other descriptors sharing that source.
CI8/I4, CI8/I8 and direct RGBA5551 lanes preserve exact indices, separate alpha,
embedded palettes, child headers and row padding. Empty children and the separate
3634 image format remain intact. Palette-less direct-color descriptors are valid.

SpriteObject rebuilds materialized children against their edited bases, so changing
one delta base does not accidentally recolor its descendants. Combat edits persist
in native project folders/archives and import browser art.animations records,
including original single-child documents. Native reset snapshots prevent legacy
edits from returning. Import vocabulary is generated by
native/tools/generate-combat-import.cjs from browser sequence metadata; it contains
identities, not ROM assets. Modified resources compile from generated C during user
export, then compress and relocate through individual existing descriptor members.
The animation runtime and selector patch semantics are unchanged.

LC05CMBA v1 at z64 027C4B00 owns a dynamic allocation. Its 96-byte descriptor
contains offset/size/count at +12/+16/+20 and raw SHA-256 at +64. The allocation starts
with count 16-byte rows (descriptor key, art ordinal, relative container offset,
container length), followed by aligned compressed containers. Loading checks bounds,
references, hash, padding and exact consumption. Re-export retains all previously
owned artwork, reuses space or grows at the tail, and moves around later owners.
Only authenticated prior storage is cleared. The ROM limit remains 64 MiB. There
is no automatic Expansion Pak requirement; full-game allocation behavior is unproved.

combat_artwork_tests passed against the private Rev0 ROM (12,695 ms): 38 browser
encoding/materialization comparisons, unchanged children/palettes/padding, exact
image remapping, malformed pixels, native/browser persistence, source compilation,
shared-source isolation, byte conservation, CRC, browser compressed readback,
reopening, repeat no-op export and owned allocation growth/relocation. The Art workspace now has a searchable combat descriptor/member browser,
child selection, exact color-index and opacity-plane painting, combined image
painting, preview, restore/undo, PNG and sprite-library transfers, and image
preparation with continuous alpha quantization. Raw channel erasure writes index
zero even when its display palette is opaque. Empty encoded deltas remain editable
when the materialized child has valid planes. The separate 3634 intensity image
remains read-only. Reference class names aid discovery. Staged pixel resources now feed ordinary/body
frame composition, the flat/projected pose inspector, scene Actors, 2D sprites and
flat/perspective props. Renderers clear their frame caches on artwork changes;
CutsceneEditor observes the shared project undo stack. Scene preview invalidates
captured/checkpoint frames and replays to the current tick with recorded input.
Pose inspection redraws without advancing its cursor. Runtime pose and metadata
selection stay intact. combat_artwork_tests composedPreviewPixels passed (107 ms);
ui_tests editedCombatPreviewAndUndo, equipmentBodyPreview and storyCastPreview
passed (968 ms), including immediate flat/projected changes, undo/redo and exact
backward/forward replay of dynamic scene 01FA8BF4. Animation assignment and sequence
authoring remain in progress. ui_tests combatArtworkEditing passed
(662 ms); indexedPixelTools, Army/icon/portrait workflows and loadDesktopWorkspace
also passed after the shared canvas/image-preparation changes. The desktop screenshot
was inspected. These are offscreen Qt checks, not emulator operation. Full-game visual
correctness is not claimed by these file-level checks.

## Dynamic Army models and class assignments

All six atlas lanes now expose model IDs 0–63, the native six-bit selector domain.
Each pack grows only through its highest authored new plane. Gaps become transparent;
unselected existing planes and both fixed palettes remain intact. Paired allocation
still retains the shared upper operand and refuses a pair that cannot fit that
constraint. New model data increases decoded atlas storage; no Expansion Pak
requirement is imposed automatically, and full-game heap behavior remains unverified.

The Army page assigns the selected model to a current class, shows that class's
assignment, and restores it with undo. Large models require unit size 2; ordinary
and special groups use other sizes. Assignments apply to both facings. The model
retains unknown appearance bit 6, byte 5 and both portrait words. New assignments
require both facing planes before export. Existing retail missing back planes are
preserved unless an edit introduces a new assignment or switches the class's size
into a missing reference. The UI can retain a pending assignment while its images
are being authored; export reports the missing side.

Native Army JSON version 1 adds an optional routes map with class ID, lane and
model. Project.classData applies these assignments. They force the maintained class
rebuild and enter its generated appearance array; an assignment alone leaves the
six atlas resources untouched. Class cloning now carries an authored Army assignment
and current per-class portrait edits, rather than silently reverting to ROM artwork.
All changes remain project-owned and undoable. A later unit-size change that makes
an assignment incompatible is rejected at export while the project stays editable.

Build 31252 and the complete army_artwork_tests passed 86,523 ms. New checks cover
route-only source rebuild, no unrelated atlas allocation, added classes 165–167,
models 56/63, dynamic 57/64-plane packs, preserved unknown appearance fields,
portrait/route inheritance, project round trips, export/CRC/reopen and original
MIPS/browser readback. Maximum-size high-entropy large packs fit the shared-upper
operand constraint in the checked samples; allocation bounds also guard all exports.
Candidates are parent Test ROMs/native-army-routes-yrdbBf and native-army-MprRtN.
The earlier missing-plane case now yields 454 images instead of 480 because growth
stops at the requested plane; its 450 original images remain unchanged.

UI Army assignment/restore/undo/compatibility, portrait and desktop checks passed
2342 ms; the new controls screenshot was inspected. After build 32325, existing
engine_tests addedClassProjects addedClassPublicExport passed 24,175 ms. The
unit-size reference test initially encountered the existing formation-capacity
validator; it was isolated with an unused added class. After build 35643,
army_artwork_tests changedClassSizeNeedsValidModels passed 6295 ms: the actual
missing-reference error is required, then a compatible Large assignment exports
and reopens while retaining portraits and unknown appearance data. No full-game
or hardware validation is claimed.

## Army workspace and indexed pixel tools

Art now includes an Army sprites page with all six front/back atlases, shared-class
consumer lists (including newly added classes), model search, both palette views,
explicit missing-plane creation, restoration and project undo. The canvas paints
palette indices directly, including erasing, fill, picking, selection and moves;
brush, zoom and grid controls remain available. A second preview displays the other
palette. Sprite-library transfers and PNG import/export use the selected palette;
original palette words remain fixed. Source-ROM overwrites are refused.

SpriteCanvas has an optional indexed mode. It retains indices as opaque channel
values during existing pixel operations and resolves the palette only when drawing.
Thus equal visible colors can carry different indices, and moving one preserves
its other-palette identity. Cleared pixels use the selected atlas's transparent
index. Typed index commits support one undo command per stroke. Ordinary RGBA
editing retains its earlier behavior.

The shared art_image_assignment dialog now serves icons, portraits and Army images.
It handles placement/cropping, zoom, offsets, sampling and transparency/background;
a domain callback supplies exact bytes and the final game-color preview. Output
previews use an integer scale. Callers retain selection/change guards and their
original project ownership. Duplicated dialog code was removed from the first two
pages; their existing assignment workflows remain checked.

Build 78267 and ui_tests indexedPixelTools spriteWorkspaceEditing
iconArtworkEditing portraitArtworkEditing passed 1691 ms. The indexed case verifies
changing duplicate-color index 1 to 2 without a visible color change, exposing that
change through the other palette, and exact move/erase/pick behavior. Build 86702
and ui_tests armyArtworkEditing indexedPixelTools iconArtworkEditing
portraitArtworkEditing loadDesktopWorkspace passed 2844 ms. The Army case uses real
missing-plane creation, painting, undo, palette switching, library assignment,
PNG transfers, restore, project reopening, source guard and new-class consumers.
After preview/label polish, build 70420 passed without warnings and the three art
UI cases passed 2171 ms. Both authored and populated Army workspace screenshots
were inspected. Backend resource/export checks below remain applicable.

## Army and battle-formation artwork rebuild

ArmyArtwork reads all six native CI8 atlases, their two fixed RGBA5551 palettes,
225 existing planes, and the 512-byte unknown player/ordinary tail. Missing player
special models 24–37 and player large model 25 remain explicitly absent until
assigned. Creating a missing plane expands its atlas and initializes other new
slots to transparent indices. Existing palettes, tail bytes and unedited planes
remain intact. Native project folders/archives and browser art schemas 4–5 import
these edits, including browser hexadecimal model keys and exact palette identity.
Explicit empty native state prevents restored legacy edits from returning.

Class routing uses ClassData's full-record unitSizeOffset (4), not the old browser
record-view offset 64: both refer to retail z64 0x5DB18 for class zero. Appearance
byte 4 supplies the lower six model bits and special-lane bit 7; unit size 2 selects
large. Bit 6 and appearance byte 5 remain uninterpreted and unchanged. Consumers
are computed from current ClassData, including added classes.

A whole-ROM LUI/ORI/ADDIU and literal-key scan identified 26 actual resource
operand pairs; there were no further literal keys. The battle model selector
shares one LUI between front/back ORIs for each lane (z64 0x2093DC, 0x2093F4 and
0x20942C). Each exported pair therefore starts within one 64 KiB resource-key
bank, retaining the original instructions and cache buffers. Bank alignment is
resourceBase 0x594280 plus N times 65536, not a 64 KiB ROM boundary. All six decoded
atlases compile from generated C at user export before compression/placement.
The existing class menu's Army operands participate in normalized owner hashes;
actual links and original a0 operand registers are checked separately.

LC05ARMY version 1 at z64 0x27C4A00 owns 160 bytes of metadata: payload offset/size
at 12/16, six relative offsets at 32, six container sizes at 56, raw SHA256 at 128,
and zero reserved spans. Its payload contains three paired banks with FF padding.
Reads validate bounds, pairing, exact padding and ownership. Later exports reuse,
grow the tail, or relocate only the authenticated allocation; cartridge bounds
remain 64 MiB. No Expansion Pak requirement or new runtime hook is introduced.

After build 49300, army_artwork_tests passed 50,376 ms. An earlier comparison
caught the class-record-view offset mismatch above; it was corrected before any
export passed. Build 11562 adds forced allocation growth: the full case passed
54,817 ms, including reuse at the ROM tail and relocation around later class data.
It compares all 450 original palette images and raw indices, 165 class routes,
480 expanded images through the independent browser decoder, and all six original
MIPS side-selection branches up to their resource call. It also checks unchanged
ROM bytes, palettes/tail, source compilation, CRC, repeated/reopened exports,
both class/art editing orders, class growth, damaged owners, and project/native/
browser persistence/reset. Latest candidate: parent Test ROMs/native-army-wxGZcc.
engine_tests classMenuSourceLayout promotionFamiliesExport passed 21,798 ms;
icon_artwork_tests browserImportAndReset passed 580 ms. These are file/resource/
instruction checks, not an emulator cold boot. The authoring UI is described above.

## Portrait workspace and edited-art previews

Art now contains Item icons and Portraits pages. Portraits lists every current
class, with independent A/B selection, optional assignment to both, thumbnails,
search, an 80-color swatch grid, custom opaque colors, brush/fill/pick/selection/
move, zoom/grid, restoration and project undo. PNG and sprite-library transfers
preserve provenance. Image preparation offers fit/crop/stretch/original-size,
zoom, output offsets, nearest/smooth sampling and an opaque background, with the
actual final game colors visible before assignment. Source-ROM overwrites reject.

Existing images within 80 RGB555 colors are exact; larger palettes use deterministic
weighted median cuts and nearest-color assignment. Transparency composites onto
an explicit opaque background. Direct painting rejects excess colors rather than
quietly changing other pixels. The export still uses the source-built path below.

DialogueDrawing accepts a pixel provider independent of Widgets. Dialogue and
scene previews supply the current project's class/portrait model, including added
classes, without compiling a preview ROM. Scene portrait changes invalidate earlier
render snapshots and replay to the current tick with recorded inputs retained.

Build 97644 succeeded after fixing a missing QUndoStack include. The new
`avatar_artwork_tests imageMappingAndLivePreview` passed 96 ms: exact original
colors, deterministic gradient reduction, opacity/background arithmetic and live
edit/restore/new-class dialogue textures. `ui_tests portraitArtworkEditing
iconArtworkEditing loadDesktopWorkspace` passed 2199 ms: real palette/pixel input,
undo, independent/both selectors, library assignment dialog, PNG transfers,
restoration, project reopening and source-file guard. The portrait workspace
screenshot was inspected. Existing `dialogue_runtime_tests ordinaryDrawingCommands`
passed 109 ms. Export checks below remain applicable; no emulated-game claim.

## Portrait authoring and source-built export

`AvatarArtwork` stores independent class/selector pixel edits as 40 by 48 opaque
RGBA5551 words, with an 80-color limit per image. Both selectors are supported,
including classes added after the artwork model was opened. Native project folders
and archives persist the edits; browser art versions 1–5 convert grouped selectors.
An explicit empty native section prevents restored browser edits from reappearing.

At user export, each edited image compiles from generated C, compresses into a
native CI8 bundle, and receives a new directory member. Unselected member indices
and original resources remain intact. The current class data compiles with its
new selectors; the final two Army appearance bytes remain unchanged. The existing
portrait loaders and fixed decoded image buffers are retained; no Expansion Pak
requirement is added.

The `LC05AVTR` version-1 descriptor occupies z64 ROM 0x27C4900 (96 bytes): payload
offset/size at 12/16, directory count at 20, zero reserved bytes 24–63, and raw
SHA256 at 64. The 16-byte-aligned payload begins with the raw directory and contains
its owned compressed bundles. Loading validates the hash, bounds, links, padding,
unique resource spans and absence of unclaimed data. A subsequent export copies
every still-referenced owned bundle before clearing its earlier allocation.
Unknown descriptor/code/payload owners are rejected. Cartridge growth is bounded
to 64 MiB. Class owner hashes normalize only validated icon/portrait key operands;
the emitted code retains actual resource keys.

`avatar_artwork_tests` passed after build 19513 (54,359 ms): all untouched selectors
and Army bytes, independently decoded 210 palette variants after two edits,
source-built export/reopen/checksum, deterministic repeated export, both class/art
editing orders, later class and portrait growth, ownership refusal, palette limits,
and folder/archive/browser persistence. The candidate is under parent
`Test ROMs/native-portraits-fnjoxs`. Original-library/dialogue checks also pass.
Build 73366 includes recognition of a stale native descriptor even when resource
operands have been reset to retail. No emulator or hardware validation is claimed.
Painting/image preparation and project-aware playback are now integrated above.

## Current-ROM portrait resources

`AvatarLibrary` follows all three guarded directory-reference pairs in the
current ROM. It retains the 13-bit member/3-bit palette token and reads raw
resource directories plus boot-LZ 40×48 CI8 bundles with up to eight 80-color
RGBA5551 palettes. Geometry, indices, opaque palette words, reference agreement
and ROM extents are bounded. Bundles are cached by resource key, retaining
intentional duplicate directory entries. This reader is independent of class
placement, so resource validation does not recurse into class-runtime metadata.

DialogueDrawing now reads appearance words from `ClassData::load` and follows
the current avatar directory, replacing its hardcoded retail route table and
resource key. It caches both readers per supplied ROM and rejects an identity
outside that ROM's class count. Null routes retain blank portraits. This fixes
portrait previews for added classes and already-relocated portrait directories;
project-authored portrait export and desktop authoring are described above.

Build74285 succeeded. `avatar_artwork_tests` passed in 7710 ms: 140 distinct
bundles/208 palette variants compared byte-for-byte with the browser resource
decoder, all 330 class/variant dialogue selections, a relocated directory with
a replaced member, malformed references/extents, and the source-built class
165 portrait with out-of-range rejection. `dialogue_runtime_tests
ordinaryDrawingCommands` passed in 113 ms against its existing 93 drawing
rectangles and palette/index expectations. ROM changes in this reader test are
in-memory only; it is not a game-load test. The parent narrow reference is
`docs/class-card-avatar-rom-research-20260813.md`.

## Item-icon rebuild engine

`IconArtwork` loads all 277 equipment and 44 consumable CI8 icons through the
current ROM's 17 LUI/ORI resource-reference pairs. It validates instruction
forms, agreement, compressed/decoded sizes and each pack's transparent index.
The authoring model retains exact RGBA5551 words; native display uses the same
rounded channel expansion as the existing image decoder. Mapping a native
image back to its words is lossless. Image assignment can map to the current
shared palette; complete-pack validation rejects more than 255 opaque colors.
Atomic multi-icon imports can replace shared colors together. Unedited icon
pixels and unused palette words remain intact.

Native project `iconArtwork` version 1 stores changed word arrays. Browser art
schemas 1–5 import `art.icons`; unknown art sections remain retained and block
export. Canonical empty edits override prior browser imports. During export,
both decoded packs compile from generated C, compress and relocate together
through the existing game loaders. Decoded sizes remain unchanged; this path
adds no RAM or Expansion Pak requirement. Original retail/browser pack storage
is retained. A verified native allocation can be reused, grown at the ROM tail
or replaced when another allocation follows it.

The 96-byte `LC05ICON` descriptor is at z64 ROM 0x27C4800, outside the other
native resource descriptors. Version 1 stores payload offset/allocation, two
relative resource offsets/lengths, reserved zero bytes and the SHA-256 of its
complete payload. Containers and padding are aligned to 16 bytes and bounded
within the 40–64 MiB cartridge range. Readback checks links, padding, extents,
resource bytes and all 321 pixel-word images. Export refuses changed descriptor,
payload or instruction preimages and updates CIC-6102 CRC through the shared
export path.

The class menu at z64 0xAEB8C also owns two icon-address pairs. Its class-runtime
fingerprint now normalizes only these recognized key immediates for comparison;
actual source/rebuilt instructions retain current icon keys. Class/promotion
verification separately checks all current icon links/resources when relocated,
and native descriptor integrity when present. Opcode/register changes and
unrelated menu edits still fail. Partial class inventories without icon sites
do not gain a dependency on unrelated icon resources. Old raw inventory hashes
remain accepted for backward compatibility.

Build94337 `icon_artwork_tests` passed in 63057 ms: all 321 original displays and
inverse RGBA mappings; both edited packs compiled/exported/reopened; independent
browser decoder checks of all 321 images; unchanged data, checksums, repeated
exports, owner corruption, shared-palette overflow, project folders/archives,
browser import/reset and unknown-art retention. New-class integration passes
in both editing orders, repeat combined exports are byte-identical, and later
growth to 167 classes retains new icon edits. Candidates are in parent
`Test ROMs/native-icons-zEwzVr/` under the master ROM filename. Build22155 then
scoped dependency checks to inventory owners containing icon sites;
`engine_tests classMenuSourceLayout promotionFamiliesExport` passed in 19222 ms.
The full-inventory icon checks retain the same path. These are offline rebuild
and decoder checks, not emulator loads. Desktop icon controls are described below.

## Art workspace: item icons

The native Art workspace exposes both icon packs with names, image thumbnails,
search, the complete shared palette and a transparency swatch. It reuses the
native pixel canvas for pencil/erase/fill/pick/selection/movement, brush size,
zoom and grid. Painting, restoring the loaded icon and assignments use project
Undo/Redo. Artwork can be exported as PNG, copied into the sprite library with
source provenance, or assigned from a selected library frame.

Image assignment previews source and final game colors before accepting it.
Placement supports fit, cover/crop, stretch and original pixel size, nearest or
smooth sampling, zoom, output-pixel offsets and an explicit one-bit alpha
threshold. It maps to the selected pack's current palette and keeps other icons
intact. Source images are limited to 4096 pixels per dimension. PNG export
writes atomically and refuses to overwrite the source ROM. Unrecognized icon
sources display a local error while the other workspaces remain usable.

Build47999 succeeded. `ui_tests iconArtworkEditing spriteWorkspaceEditing
loadDesktopWorkspace` passed in 1562 ms: real mouse erasure/undo, library transfer,
accepting the preparation dialog, PNG import/export across packs, restore,
project save/reopen, source protection and existing workspace regressions.
Screenshot inspection found the initial palette cell-size order was clamping
columns to the style minimum and hiding swatches behind scrollbars. Build14274
corrected this; `ui_tests iconArtworkEditing` passed in 778 ms and the full
16×16 palette screenshot was inspected. Engine/export evidence above remains
applicable. Portraits, Army sprites, combat artwork and video are still separate
unfinished work; this does not mark the whole Art/Animation domain complete.

## Sprite studio and project artwork

The native Sprites workspace owns editable RGBA assets with independent frame
layers, signed offsets, visibility, canvas anchors and 30 Hz frame durations.
Pencil/eraser/fill, eyedropper, rectangular selection, layer/selection movement,
clipboard transfer, canvas resize/crop, nearest scaling, rotation, flips and
transparent-edge trimming use project Undo/Redo. Frame/layer creation, copying,
ordering, removal, names and timing persist. Mouse strokes commit once; Escape
cancels. Copy-on-write images retain unedited layers and avoid a full image copy
on every pencil segment. Selection movement copies raw RGBA, retaining hidden
RGB and translucent pixels. Preview timing uses elapsed time, avoiding drift
from rounding each display timer to 33 ms.

Native sprite asset/library schema 3 embeds lossless PNG layers; imports accept
browser versions 1/2 and native version 3. Browser-compatible version 2 export
retains raw RGBA, layer geometry, identifiers, names, anchors and provenance.
PNG/JPEG image import/export work independently; JPEG flattens onto white.
Imported files create fresh asset identities. Project folders and sharing
archives store the complete library. A canonical empty library overrides the
retained browser import so deleted artwork never reappears when reopening.
Unassigned library artwork does not change ROM bytes or block ROM export.
Game-resource assignment/export, video export, source-art import and the
remaining palette/image-preparation conveniences are still being implemented.

Limits are 4096 pixels per dimension, 512 assets, 1024 frames per asset, 64
layers per frame and 64 Mi pixels across the library. Base64, dimensions, PNG
headers, IDs, frame kinds and budgets are validated before accepting edits.
Project reading now allows bounded 512 MiB documents. Archive inflation uses
Qt's existing zlib `uncompress2`, requiring exactly the declared output size
and complete input consumption; it no longer trusts the size header passed to
unbounded `qUncompress`. This adds no dependency.

Build37387 succeeds. `sprite_library_tests` passed in 3317 ms: 24 synthetic
layered assets/72 composites, rotation and nearest resize compared with the
preserved browser implementation; v1/v2/v3 round-trips; transparent RGBA,
clipping, brush/fill/move/selection semantics; project folders/archives/import,
deleted-asset persistence, a valid 65 MiB document and malformed archive
rejection. `ui_tests spriteWorkspaceEditing` passed in 156 ms: real mouse
strokes, correct reversed layer selection, selection erasure, Escape, undo,
frames/timing/order/visibility, PNG/JPEG/asset export and import, source-ROM
protection and deletion recovery. The earlier workspace-load regression also
passed (build2044, combined desktop tests 868 ms). Screenshot inspected. These
are offline asset/editor checks, not ROM art assignment or game-load checks.
Build44104 additionally removes the converted browser pixel copy from saved
legacy metadata and bounds writes to the reader limit. The targeted
`projectPersistenceAndArchiveBounds` check passed in 3047 ms; older native
projects retaining that copy still honor canonical deletion intent.

## Save workspace

The native Save workspace owns a separate document and undo stack. It opens
native/word-swapped 32/64 KiB SRAM, 4/8 MiB native RDRAM, raw libretro states and
RetroArch RZIP states. It preserves input byte order/container and returns the
exact original file for no-op/fully undone edits. Export requires a different
path, validates reopening and writes atomically. Save contents are independent
of ROM project files. MainWindow routes Undo/Redo to the active workspace and
includes unexported save edits in its discard check.

The current ROM supplies the 21-group, 49433-bit packing descriptor at z64
0x5D344/live RAM 0x80187444. Its placement is proven by the resident boot DMA
0x40E80 -> 0x8016AF80, not the early-boot address mapping. Parsing bounds group
counts, fields, widths, pointers and total payload size. Every edit changes only
the affected descriptor bits and record checksum. Unmapped bytes, unused final
bits, unknown name tokens and other records remain intact. The maintained
`game/classes/save-format.c` supplies checksums and legacy/expanded image rules
to the desktop as well as the exported game runtime. Two normal records and the
larger suspend record have distinct sizes, offsets and checksum seeds.

The verified character model supersedes the browser's old field assumptions:
all 99 character IDs are visible; classes at +0x11/+0x12 stay independent;
HP/attributes and equipment IDs are full halfwords; Element is +0x1A. HP/attributes
use the supported 0..999 bounds, with maximum HP positive and current HP no
higher than maximum; zero current HP remains zero. Equipment extends through
ID 277, class IDs through the loaded project catalog. Body code, fatigue, status,
table index and unknown bytes are preserved. Names have 16 visible ASCII bytes
in runtime RAM and 12 plus terminator in the 13 descriptor-backed battery bytes.
Unchanged token names retain their original bytes. New roster entries/removal
remain unavailable because unit membership is not yet fully mapped.

Inventory grids edit quantities and permit adding a nonduplicate ID in the next
empty record. Existing identities remain fixed. Native equipment uses the
descriptor-backed 0x196B00 list; raw runtime input retains the browser's 0x196CCC
list contract. Consumables use 0x193AC0. The tracked parent golden SRAM confirms
21 equipment entries and four consumable entries; it does not independently
validate the raw runtime list address. Unknown values remain readable and are
not normalized merely by opening a cell. Goth and Chaos Frame edit independently.

Calendar correction: accepted `func_0019BD14.c` and original `func_000424bc.s`
identify year at RAM 0x80196A30 (halfword), month at +2 and day at +3. The older
parent `docs/game-state.md` and browser day/month labels on the two year bytes
are incorrect. The increment routine consumes 15 month lengths from live
0x80187000 (z64 0x5CF00); month 3 gains a day when `(year+1748)&65535` satisfies
the Gregorian leap rule. Editors offer years 0..998, months 1..15 and only days
valid in that month/year. Existing terminal year 999 remains untouched. The old
12-month name list is not reused. Mission/progression, time and location controls
still need their own qualified semantics; they are not mislabeled as calendar
fields or current scenario. Parent domain files have other uncommitted work,
so this correction is recorded here without absorbing those edits.

RZIP decompression uses bounded `uncompress2` from Qt's bundled zlib 1.3.2,
verified against the official release on 2026-10-05. Each chunk must consume
exactly its declared compressed bytes and produce exactly its bounded output.
The pinned Qt6::ZlibPrivate header interface and Qt Core prefixed symbols are
used; no separate dependency is downloaded. Existing Qt notices include zlib.

Build93945 succeeded. `save_document_tests` passed in 3588 ms: fixtures packed by
the original MIPS encoder; 10 edited values decoded by its original decoder;
240 calendar increment boundaries; 99 records, multiple slots/layouts/orders,
16-bit HP/items, zero HP, name limits, unknown bytes/unused bits, inventory edits,
exact undo, import/reopen and malformed checksum/RZIP rejection. Node zlib
independently reads exported compressed states. The parent golden `.sra` is read
only and its edited copy reopens. `ui_tests saveWorkspaceEditing
loadDesktopWorkspace dialogueInvalidInputBlocksSave` passed in 2683 ms, including
actual cell commit, cross-slot undo, file export, source-overwrite rejection,
active Undo routing and destruction. The Save screenshot was inspected.
These are offline engine/UI checks, not emulator loads or a complete Save
feature audit. No source save or master ROM was modified.

Build61894 adds current-project class limits to an already-open save and rejects
export when a newly changed class reference no longer exists after a project
undo. Unchanged unknown references remain preserved. The final
`save_document_tests` passed in 4331 ms and `ui_tests saveWorkspaceEditing`
passed in 1744 ms, including late class creation, missing-class rejection,
restoration and opening/confirming an unknown long name without truncation.
The earlier desktop/dialogue regression results remain applicable.

## Tools: High Attack stream split

The experimental High Attack setting now has independent undo/restore, native
project folders/archives, browser patch and rebuild-owner imports, source-built
export, v21 migration and exact retail restoration. Tools schema 4 reads schemas
1–3 without resetting newer settings. All eleven owned regions must match a
complete recognized installation. Foreign code/allocation bytes are preserved,
and overlapping rebuild owners fail before publication. Rev0 does not claim the
Rev1 continuation at z64 0x306CC. The UI and export report retain the existing
Expansion Pak and Project64 Interpreter requirements; full-battle validation is
still pending. This does not change the 4 MiB Classes allocation policy.

Maintained ASM is adapted from parent `tools/build_high_attack_stream_shift_rom.py`
v21 and its qualified 546-entry context-reference inventory. The inventory's
words are comparison data, never executed as instructions. No compiled game
payload ships. Version 23 corrects the loader's uncached marker address from
0xA0400000 to 0xA0440000, checks both marker and version, and preserves a0/a1/ra
across the resident cache helpers. Valid warm entry retains install/high-water
metadata; stale modules reload. The module remains at RAM 0x80440000, context at
0x80450000 and trailer at 0x80449810; ROM placement remains z64 0x27A0000.

At each active insert, exact original-word guards recheck the known references
and advance-helper entry. An already relocated context is not copied again.
A fresh context relocates if the retained flag, any known shifted reference,
or the shifted helper indicates that consumers already use the split layout.
Otherwise the original proposed-length threshold (>0x800) remains in force.
This handles modeled complete/partial overlay reloads without assuming a stable
overlay generation. Foreign words remain untouched. Recovery helpers at module
offset 0x400 onward are unchanged from v21. The rejected v22 per-site cache
experiment is not used; this change does not establish recompiler support or
prove the cause of the historical v22 endless-growth failure.

Build42919 succeeded. `tool_settings_tests highAttackProjectsAndExports
projectPersistenceAndImport chaosProjectPersistenceAndImport` passed in 34894 ms:
folder/archive/import/reset/rollback, old schemas, 11 independent foreign-owner
cases, complete-image conservation/checksum, physical exports/reopens, stable
repeat export, v21 migration and removal, unowned continuation preservation,
and all five Tools with a new class. Candidates: parent
`Test ROMs/native-high-attack-FBdtKT`. The master ROM remained unchanged.
The export-compiled source passes 10 loader and 80 layout instruction cases,
including threshold boundaries, partial reloads, helper-only reloads, foreign
words, argument preservation, owner/head/trailer copying and high-water lifetime.
The longest modeled insert executes 70743 instructions; this is not a hardware
performance measurement. DMA/cache services are modeled, not game execution.
`ui_tests highAttackToolsEditing displayToolsEditing chaosToolsEditing
experienceToolsEditing` passed in 444 ms; the screenshot was inspected.

## Tools: Luck and squad Alignment

Both displays now have independent native controls, loaded-setting restoration,
undo/redo, project folder/archive persistence, and browser patch/rebuild-owner
imports. Tools snapshots use version 3 and accept versions 1 and 2. The workspace
scrolls on smaller windows. Each feature retains its existing Expansion Pak
requirement; foreign code disables only the affected setting and remains intact.

Export assembles the maintained sources adapted from parent
`tools/luck_alignment_display_patch.py`: Luck version 6 and Alignment version 10.
The current Alignment correction is independently identified by committed Decomp
`1641ccb6:tools/build_squad_alignment_correction.py`; the older Phase 3 generator
is not used for its behavior. Alignment replays native drawing, waits for the
final ordinary visible squad row (including the six-row cap), and then appends
the averages as a batch. Its occupied-member average, empty fallback 50, displayed
value clamp and original call arguments are retained. Luck reads the selected
character and replays the displaced card helper with its original arguments.

The loaders verify module sentinel and version before cache invalidation and PI
DMA. Source module reservations are ROM `27C2000..27C3000` / RAM
`80480000..80481000` for Luck and ROM `27C3000..27C4000` / RAM
`804A0000..804A1000` for Alignment. Recognized current legacy modules migrate
from ROM `27F0000` and `27F1000`; older/foreign versions remain unavailable.
Installed bounded textures are preserved during source rebuilding. New installs
derive labels from the current Army UI pack and digits from the CRC-checked named
`suujifont2.k64` archive, including relocated archives. LCK uses native L/C and
the original tool's authored K recipe; ALI uses native A/L/I. No captured texture
or precompiled game module is shipped.

`tool_settings_tests displayArtworkFromCurrentRom chaosArtworkFromCurrentRom`
passed in 1,416 ms: both complete 512-byte sheets match the current browser
modules; relocated and edited digits work; malformed dimensions, missing glyphs,
duplicate archives and bad CRCs are rejected. The shared Chaos title still matches.
`displayProjectsAndExports chaosProjectPersistenceAndImport projectPersistenceAndImport`
passed in 37,945 ms. It checks the three enabled combinations against full browser
ROM bytes, project round trips, physical export/reopen, repeat builds, removal,
current legacy migration, custom textures, occupied allocations, import conflicts,
and combined Luck/Alignment/Chaos/experience/new-class export. Candidates:
parent `Test ROMs/native-displays-hrHulg`. The master ROM is unchanged.
`ui_tests displayToolsEditing chaosToolsEditing experienceToolsEditing` passed
in 322 ms; the complete workspace screenshot was inspected. No in-game cold-boot
validation was performed, and that release check remains pending.

## Tools: Chaos Frame counter

The Army Management counter now has a native toggle, loaded-value restoration,
undo/redo, folder/archive persistence, and imports for both browser
`patches.tools.cf-army-counter` and `rebuild_owners.chaos_frame_counter`.
Tools snapshots use version 2; version 1 experience-only snapshots remain readable.
Unknown counter instructions or occupied allocations disable this setting without
disabling experience editing or changing the unknown bytes.

Export assembles maintained hook, bootstrap, cache/PI continuation, insertion
routine and display-list sources. The routine is transcribed from committed
Decomp `e50fe500` `tools/lordly_phase3_native_payload.py`, retaining its Army owner,
relocatable graphics-buffer, trailer and roster-marker guards. No external Decomp
checkout or compiled game module is required by users. The historical assembler
needs named linker symbols for absolute jumps; numeric operands produced zero
jump targets and were caught by the compiled-owner comparison.

Qualified placements are ROM `23F7C` / RAM `80093B7C` (hook), ROM `3078C` /
RAM `800A038C` (continuation), ROM `34E78` / RAM `800A4A78` (bootstrap), and
ROM `27C1000..27C2000` / RAM `80420000..80421000` (module reservation).
This optional feature retains the existing Expansion Pak requirement, shown next
to its checkbox and in the export report. Class expansion still fits within 4 MiB.
The current browser source module and packed legacy version 3 are recognized;
earlier or foreign variants remain preserved and unavailable. Version 3 migrates
from its authenticated ROM `2790000` module when enabled, or can be removed.

New installation derives the 48x16 CI4 title from the current ROM's shared Army
texture resource. The loader's LUI/ORI operands at ROM `AED4C` / `AED54` select
the resource; the retail key `01DC873E` resolves to ROM `235C9BE`. Decoded offsets
`2828` and `40F8` hold the War Funds and header lettering. The recovered composition
recipe selects letter bodies, creates M by flipping W, and regenerates shadows.
It does not ship captured game artwork. A recognized installed counter's bounded
title texture is preserved when rebuilding its code.

`tool_settings_tests chaosArtworkFromCurrentRom` passed in 377 ms: exact existing
title bytes, relocated resource, edited glyph, malformed size/instructions and
missing glyphs. Its initial synthetic relocation omitted four-byte ROM alignment;
the fixture was corrected. `chaosProjectPersistenceAndImport` and the existing
experience persistence/export cases pass, including conflicting imports and
atomic validation. `ui_tests chaosToolsEditing experienceToolsEditing` passed in
222 ms, covering independent controls, memory-requirement wording, undo/redo and
restore; the screenshot was inspected. The export test checks compiled bytes,
checksums, physical exports and reopening, removal, legacy migration, foreign
allocation preservation, and combined class/experience/counter rebuilds.
The final `chaosSourceExportAndReopen` run passed in 17,465 ms, including custom
label preservation and removing a legacy counter while leaving an occupied new
module slot intact. Candidates are in parent `Test ROMs/native-chaos-ePlxTs`.
Full-game cold-boot validation remains pending.

## Tools: experience size-weight scale

The Tools workspace now edits the shared0..300% multiplier with undo/redo,
loaded-value restoration and a retail100% reset. Project folders/archives and
both browser patch and rebuild-owner imports retain the value. Native snapshots
override earlier imported values; unsupported tool edits remain retained and
continue to block export. Unrecognized experience code disables this setting
without changing its bytes.

User export assembles maintained game/tools/experience.s for three bounded
regions of the existing combat calculator: ROM21928C/2192BC/2192F8, qualified
live801D5FBC/801D5FEC/801D6028. The build authenticates source regions, checks
compiled instruction hashes and parameters, and rejects overlapping owners.
100% assembles the original instructions. No allocation growth or extra RAM is
needed. The generated catalog contains recognition hashes, not game payloads.
The historical assembler requires nested .if blocks, not ==/.elseif syntax.

Build39271 tool_settings_tests sourceExportAndReopen passed5689ms:0/75/137/300%
match browser export bytes, save under the master filename, reopen with their
selected values, and restore100% byte-for-byte. Foreign code remains unchanged.
Candidates: parent Test ROMs/native-tools-wBqQoW. Build49980
projectPersistenceAndImport passed3361ms: folder/archive round trips, both import
forms, native reset precedence, invalid values, unknown native fields, retained
unsupported browser tools, conflict rollback and unavailable foreign code.
Build59587 ui_tests experienceToolsEditing passed112ms: effective weights,
undo/redo, restore/reset and foreign-code disabling; screenshot inspected.
Existing engine_tests projectPersistence browserProjectImport shopLegacyImport
passed3772ms. These checks establish export compatibility and editor behavior;
full-game natural-load validation remains pending.

## Reward artwork

ItemIcons decodes shared CI8/RGBA5551 equipment and consumable packs, following
the reward constructor's current LUI/ORI resource keys. Relocated packs remain
readable. The reward panel uses the native16x16 icon at left+4/top+4 and the name
at left+26; frame and text rasterization remain approximate. The content offsets
and index formula come from ROM2A54A8..2A5CA0, live80235C78..80236470.

Build18170 image_tests itemIconsFromCurrentRom passed542ms: all321 current-ROM
icons match the existing browser decoder, including transparency. Both packs
also pass relocated-pointer checks; invalid IDs, sizes and instruction layouts
are rejected. ui_tests panelPreviewSeek storyCompleteReplay passed14579ms and
the updated reward screenshot was inspected. The full story still releases at
step1349 with deterministic replay. No ROM was written.

## Automatic Actor portraits

SceneActors now supplies the default BF portrait callback from current Actor
identity. Missing Actors return -1; body rows use the full-word zero test and
signed low halfword at row48. Ordinary Actors execute the bounded bank/variant
route from original ROM29FF74..2A0088, live80230744..80230858.

The original raw loader ROM2DFB8..2E128 (live8009DBB8) was included in the reference
execution. Its DMA reads332 stored bytes from key1A8E7AE; it does not decompress
those bytes. Many ordinary bank/variant lookups exceed this allocation. The host
rejects those reads and requests an explicit dialogue portrait. It does not
substitute the plausible decompressed338-byte table or read unrelated heap data.

Build51691 actor_runtime_tests automaticPortraitAgainstMips passed232ms:288 cases
with valid Actor art banks, including current-ROM unsafe reads, signed body IDs,
empty slots and unchanged Actor records. The generator executes296 cases (also
including absent art banks0/69);197 attempted out-of-allocation reads are rejected.
Only DMA/heap services are controlled. Existing dialogue_runtime_tests
creationResumesWithCurrentContext passed87ms. This establishes lookup behavior,
not a valid portrait assignment for every ordinary Actor.

## Reward panels and complete dynamic story playback

Preset34 now takes an explicit parent-event reward item, preserves the native
56-byte owner and604-byte payload, and executes opening, A/B dismissal, query
and audio-request behavior. The preview selects equipment or consumables from
current-ROM names without changing the scene. Frame/glyph drawing is approximate;
the reward icon is not yet drawn. Item-name pointers use the qualified resident
pool mapping, not the early-boot address delta.

scene_runtime_tests rewardPanelAgainstMips passed255ms:32 original constructor/
controller cases and1408 full entity/payload/status/audio snapshots. Resource
artwork decoding is an opaque handle in this check. Build37737 ui_tests
panelPreviewSeek storyOpeningDialogue passed7606ms, including reward changes,
input-history reset, dismissal, checkpoint replay and unchanged project data.
The reward screenshot was inspected.

The full unchanged01FA8BF4 integration initially stopped at word580 because Actor
release13 lacked dispatch. It now calls the existing record-only vacancy path,
with exact full-word -1 for all28 slots and bounded individual selectors. Parallel
jobs retain their own lifetimes, matching original ROM2A05EC..2A0680 (qualified
live placement80230DBC..80230E50). Build15309 actor_runtime_tests scriptActorRelease
passed94ms: occupied/empty/all slots, three modes, invalid-selector preservation,
and retained motion jobs. ui_tests storyCompleteReplay passed10145ms: release at
step1349, three audio requests, two intermediate checkpoint replay samples and
identical final frame/request count. This is deterministic desktop integration,
not a claim of full-game timing, audio synthesis or pixel-identical rendering.

## Scene audio requests

SceneAudio adapts ROM28308C..28364C into portable C with a typed downstream sink.
It handles6E/6F/70/71/B4, deferred replacement bytes, signed gain envelopes and
ordered controller writes. Voice counts, scene route, suppression flag and initial
gain buses are explicit inputs. Services retain full words until the native route
narrows them; envelopes retain signed halfwords and clamp only during updates.
Pending-controller overwrite and voice synthesis belong to the future resident
consumer, not this request layer. This is reconstructed C, not accepted matching C.

The desktop explicitly starts silent, with no voices, route zero, clear suppression
and unity gain buses. It records service requests and runs the separate audio
update once before each scene step. It does not manufacture voice lifetimes or
claim original global scheduler cadence. Audible playback remains unfinished.

Build78913 scene_runtime_tests audioRequestsAgainstMips passed121ms:331 original
sequences/951 snapshots match ordered requests, deferred bytes, gain state and full
eight-byte envelopes. Resident services, allocation, voice counts and flag reads
are controlled. The original constructor requests four allocation bytes but writes
eight; the host owns the complete eight-byte state rather than copying that size.
Build26343 ui_tests storyOpeningDialogue sceneDialogueReplay imageSkipInputReplay
passed5284ms. The unchanged01FA8BF4 passes both audio requests, reveals its room,
and seeks backward/forward with identical frames and request counts. The screenshot
scene-story-revealed.png was inspected. Later panel preset34 still needs its owner.

Correction to the older parent cutscene-director-command-reference.md audio-program
paragraph: action5 is not a release alias. Original283190 compares context s0 with5,
after action1's branch; context5 uses direct start and other contexts use pending10.
The original-instruction cases include action5 for every context and both routes.

## Dialogue in scene playback

DialoguePlayback now owns the shared resource pass used by both standalone
DialogueSession and ScenePreview. The Director and resource pass share the same
six-window pool, controls and context. Scene snapshots retain payloads, queues and
edited archive sources; confirm/cancel and choice input reuse the recorded input
timeline. Returning from dialogue editing refreshes playback only when the model
changed. Four explicitly named preview substitutions replace unavailable army
context without constructing fictitious roster records.

The desktop runs one resource pass after each completed scene step and composes
dialogue after the scene fade. These are preview cadence/composition assumptions.
Anchors project current packed Actor matrices on a copy, preserving drawing state.
This does not claim exact native projector rounding, global scheduler timing or
N64 framebuffer coverage. Automatic ordinary-Actor portraits and audio remain open.

Build73370 ui_tests sceneDialogueReplay dialoguePlaybackControls panelPreviewSeek
storyCastPreview passed5770ms. The new check covers edited source, substitutions,
confirm, script resume/close, checkpoints, rewind/restart, changing text and names,
and unchanged project data. It found and fixed a teardown crash: QLineEdit focus
loss can emit editingFinished after derived members are destroyed. Both preview
owners now disconnect their text fields before QWidget teardown.
Build63268 storyOpeningDialogue sceneDialogueReplay dialoguePlaybackControls passed
2893ms; the unchanged01FA8BF4 reaches its native-script wait with "Happy Birthday!"
over the authored black fade and replays exactly. Screenshots were inspected.
The canvas now refits when appearing controls resize the viewport. Existing
sharedNamesAgainstMips, creationResumesWithCurrentContext, composedResourceLifecycle
and playbackTracksWindowsAcrossSuspension passed1705ms. No ROM writes or emulator
input occurred. Later commands in01FA8BF4 still require the audio domain.

## Dynamic story preview cast

SceneStoryCast recognizes the five paired class replacements and seventeen launch
translations used by event66/67. Eighteen retail streams, including01FA8BF4, use
this contract. Five preview selectors supply primary/secondary class pairs and
the recovered dialogue classifier without rewriting script operands. The UI uses
matching primary/secondary classes; the engine retains independent pairs. These
are declared sample inputs, not inferred historical army membership.

Build44366 scene_runtime_tests storyCastLaunch passed138ms: two casts execute the
actual01FA8BF4 script, retain every literal Actor coordinate and resolve the chosen
body identities. It also checks recognition, source preservation and invalid input.
Build42720 ui_tests storyCastPreview panelPreviewSeek launchImageSelection passed
3623ms. The cast check changes a body, rewinds/replays exactly, preserves project
JSON and hides the controls on unrelated scenes. Its visual fixture makes only
the initial authored fade transparent so bodies can be compared. Production
playback retains that black fade. scene-story-cast.png was inspected. Full dialogue
integration is still needed to advance this scene through its opening sequence.

## Actor roster materialization

45/AB now materialize supplied twenty-row army contexts through SceneActors.
The selector and pose policy are adapted from accepted PURE_C func_002ABB3C at
Decomp8012b84aec7c04dd61e383cbf40d5b2ae5dcd2a8; its unchanged source is retained
under game/director. Host code owns the target pointer graph, capacity checks,
resource binding, construction, pose execution and mutable leader-slot scan.
The existing constructor now accepts its native direction argument independently
of roster-source flags. Command failure leaves the scene unchanged.

Build99357 presentation_runtime_tests rosterMaterializationAgainstMips passed261ms:
50 cases execute original selection, construction, pose and leader normalization.
The test controls resource allocations, terrain123.75, availability, and draw/audio
sinks. Every Actor record and immediate draw request matches; cases cover the two
command variants, both sources, consumable/family selectors, paired and special
bodies, context filters, and six scenario depth inversions. It does not establish
Stage construction, terrain correctness, final audio or immediate-draw pixels.
An invalid synthetic second87 selected a46-pose resource then requested pose50;
the port correctly rejected it. The fixture now uses the valid special-body depth
route. Existing entrance/return and missing-context tests passed within the2222ms
Build5534 suite. Build43897 rosterMutableLeaderScan and missingContextIsAtomic
passed195ms: three retained original mutable-scan cases and capacity rollback.

The original normalizer skips paired-owner Actors when pose>=50. An older parent
command-reference paragraph says below50; the original2AB574 branch and corrected
retained finalizer fixtures establish the opposite. This note corrects that
interpretation without rewriting another owner's domain document.

UI party selection and native upstream row/linked-object construction remain
unfinished. The engine requires explicit launch context/scenario and roster state
where the game reads it. No guessed actor records or historical party claims are
introduced. Immediate draw requests are retained as pre-normalization slot events.

## Map help and briefing panels

ScenePanels owns the fourteen script handles and the separate animated entity
list. Presets33(help),3(briefing choices),1(confirmation with a Yes/No child) now
execute through62/6A/6B. Reconstructed portable C implements the shared native
opening/closing geometry, float increments, alpha delays and detach timing.
This is a source reconstruction, not an accepted matching-C claim.

Text is decoded from the current-ROM18855FE directory's first member, with
native section delimiters, line offsets, control bytes and image-size selection.
Host identities replace pointers; snapshots retain copy-on-write records. Script
handle release does not remove an attached shared entity. Preset3 replacement
explicitly unlinks its previous entity, following the constructor. The wrapper
at ROM281A8C invokes menu updates after the complete scene scheduler, including
the alternate path; this is separate from the dialogue-placement service.

Reference functions: ROM51EB0/51F74/52284/5A0F4(entity animation),2A6150(creation),
2A47C0(text),2A6E50(update),2A6BE4(options),55014(child),283748(handle release).
Qualified overlay placements and original traces are retained in browser
cutscene-map-menu-data.js and tests/fixtures/cutscene-map-{menu,option-menu}.json.

Build80132: scene_runtime_tests panelsNativeReference passed112ms, comparing788
original state snapshots, child records, text payloads and audio requests.
The first comparison caught premature entity removal on handle release; the
port now preserves native independent lifetimes. Pointer fields are normalized.
Build77110: ui_tests panelPreviewSeek manualImageInputReplay imageSkipInputReplay
screenCropPreviewSeek irisPreviewSeek passed7284ms. The new UI check exercises
all three presets through scripts (including absent-owner query), visible
opening/selection/dismissal, recorded input, backward seek, checkpoints,restart
and branching after rewind. scene-briefing-panel.png was inspected.

Desktop frame/font rendering and composition are approximate and labeled. The
native glyphs, frame texture, exact hardware clipping and final audio playback
are not established here. Other menu presets remain explicit unsupported services.
No ROM writes or live emulator operations were performed.

## Draw-driven screen crop

SceneCrop handles3B/3C for the non-title launch variant using reconstructed C.
The constructor preserves full-word sentinel comparison before signed-halfword
storage. Progress advances before interpolation during a completed draw; query,
zero duration, persistence, replacement and release follow the original record.
UI repaint and snapshot presentation do not advance it. The preview now includes
the common renderer's default scissor (top24, bottom215, right319) and applies
authored crop edges before its independent full-screen fade. Desktop limits are
interpreted as exclusive; exact hardware coverage is not claimed. Selector0's
separate textured title reveal remains explicitly unsupported.

Complete original functions uniquely match retained opening-scene RAM:
ROM2AD6D0..2AD7B0 at8023DF70,2AD7B0..2ADA20 at8023E050,
2ADA20..2ADA48 at8023E2C0. The54-byte shared interpolation instruction span at
RAM80227DC4 uniquely matchesROM283D94. Wrapper2821DC calls the crop draw and
emits default scissor wordsED000060/004FC35C when it returns zero.

Build3511 scene_runtime_tests screenCropAgainstMips passed145ms:1472 complete
record/query/scissor snapshots across128 original sequences. Allocation/free are
controlled; native interpolation and display-list emission execute. Inputs cover
sentinels, halfword truncation, signed edges,zero/negative duration, persistence
and replacement. Negative durations are sampled before completion. The first
fixture used800F9BA0 rather than the signed-address result800E9BA0 for the display
cursor; fixing that input resolved the mismatch without changing product code.

Build3511 ui_tests screenCropPreviewSeek passed within the5590ms scene suite;
iris,sprite,color,capture and manual-input regressions also passed. The empty-room
fixture now expects black outside the scissor, verified in Build25222 alongside
scenePlaybackPreview,imagePresentationPlayback and pathRibbonPreviewSeek (2905ms
suite). World-pan centroid measurement now excludes the black border rather than
counting it as sprite opacity; Build86651 worldScenePanAndSeek passed252ms.
New checks cover visible boundaries, repaint stability,persistence,release and
checkpoint/restart restoration. No ROMs were written.

## Iris playback and composition

ScenePlayback now advances the iris in MODE_ZERO5 and exposes the4E query.
Starting4D moves actual Actor/sprite channels without replacing sprite identities;
close completion releases the live pools. Image assignments and all twenty
transform records follow the saved/reordered stack. A finished transition no
longer constrains the channel count of a later background group.

ScenePreview retains background resources referenced by current or saved iris
layers, and prunes older unreferenced groups. Frozen frames use the channel's
background projection. ROM2B04FC restores the enclosing channel matrix and calls
2B616C after its Actor pass; the desktop mask follows that position. Ring and
quad triangle indices follow2B5F8C..5FA8 and2B6394..63D0. Coverage and alpha
interpolation are desktop approximations. Capture timing is explicitly the last
completed desktop frame supplied to4D, not a claim about N64 framebuffer timing.
Manual indicators actually released from the live pool cannot be resurrected by
the image owner; attempting to reuse a freed indicator pauses with an error.

Build94006 ui_tests irisPreviewSeek sceneRoomComposition sceneSpriteComposition
sceneColorPlayback capturedScenePlayback manualImageInputReplay passed12 checks
in5024ms. Closing/opening, saved checkpoint124/back11/forward151, unchanged closed
draw and exact desktop restoration/restart pass; scene-iris.png was inspected.
Build24913 image_tests irisMaskCoverage passed104ms: transparent center,
interpolated edge, opaque outer region, full-quad corners and opening symmetry.
Build17604 scene_runtime_tests irisPlaybackLifecycle passed149ms; Build7752's
added finished-iris/smaller-group boundary passed202ms. Engine checks cover layer
movement, stable sprite identity, invalid-start rollback, release timing,
quantized capture, restoration and independent checkpoints.

The initial new mask fixture placed z=0 outside its orthographic near plane;
the initial Actor fixture selected an absent bank. Correcting those fixture
inputs required no product change. These checks do not establish pixel-equivalent
N64 blending, live event inputs or full-game execution.

## Iris transition state and geometry

SceneIris owns the24-byte transition, twenty opaque resource/channel records,
saved layer stack and captured image. Reconstructed C handles field widths,
progress/query timing, signed interpolation and93-vertex mesh generation.
Closing moves the selected image to the last active layer; Actor and sprite
layer comparisons differ at equality. Completion saves current layers, installs
the frozen frame, requests Actor/sprite release and retains the closed state.
Opening restores that saved stack; final cleanup reverses the selected-layer
move. Invalid phase/lifecycle/geometry changes leave the previous state intact.

Build19550: image_tests irisNativeReference passed111ms against the28 original
snapshots and three complete packed meshes retained in the browser fixture.
Its generator is parent docs/reviews/cutscene-framebuffer-effects-20260916/native-iris.py;
it executes the original functions with uniquely matched retained-RAM placement
and verifies the two frozen-frame copies. The new check additionally preserves
current channel edits at close completion and rejects invalid transitions.
Native desktop composition is the next integration step, not established by this
state/geometry check. Existing framebuffer quantization is reused.

The32-byte RAM80243D10 constant block uniquely matches ROM2B8B10. Mesh geometry
has three rings: base radius with zero alpha,1.3 times radius with mask alpha,
then fixed640/480 outer radii with mask alpha. This corrects the two-edge prose
summary in the parent command reference; the retained original mesh agrees.

## Recorded Start-skip input

ScenePlayback now connectsB2/B3, image-owner840 and the existing Director barrier
redirect. The normal image update latches Start before Director execution; an
executed handoff consumes state2 and selects label3039. The next dispatch runs
the destination. B3 cancels a pending request. Absent image owners safely ignore
arming, and alternate updates do not run the input latch. Pressed-button input
is separate from held pan/zoom controls and also supplies the Director's B5 input.

The preview exposes Skip scene and Enter while armed. One-shot press history is
saved alongside held-input history for seeking and restart. New input after a
rewind invalidates both future input maps and later checkpoints.

Build80637: ui_tests imageSkipInputReplay manualImageInputReplay passed2942ms.
Keyboard and button skipping,124-step/backward replay, restart and manual controls
agree. Build97262: scene_runtime_tests imageSkipHandoff passed164ms, checking
arming order, null owner, alternate path, cancellation and bridge/destination
timing. Its first fixture incorrectly expected an unarmed counter to increment;
the counter is now explicitly initialized to distinguish destination execution.
Original input-latch and Director-control instruction checks are reused unchanged.

## Map-path ribbons

Correction to the earlier image sections: the twenty owners at scene-root19F4
updated by ROM2B529C and drawn by2B555C are animated path ribbons, not captions.
The previously recorded empty-pool echo results are unchanged. The49..4C parser
handlers, nested path directory1A8D7A6 and geometry builder establish this identity.

SceneRibbons decodes current-ROM paths and resolves dynamic points from supplied
map-marker positions. Reconstructed C preserves the native distance-parameterized
cubic curve, mixed single/double arithmetic, excluded final sample, endpoint
precedence, forward-only reveal, activity flag and thirty-count removal. The
normal mode-zero scheduler updates ribbons before the Director; the alternate
path skips that update. Failed path/style/endpoint changes leave existing owners
intact. Qt paints the sampled strip with an explicit approximation label; native
vertex texturing, draw-driven alpha and framebuffer equivalence remain unqualified.

Build35406: image_tests pathRibbonsAgainstMips passed1182ms. All121 retail paths
match400 original samples per axis, generated count/length, milestone lookup and
45 retarget/reveal/fade snapshots each. Original constructor and curve/reveal code
execute with browser-decoded resources, explicit synthetic marker positions,
controlled allocation/libm and vertex packing. Geometry buffers, endpoint-angle
fields and draw pixels are outside these comparisons. The two existing scheduler
composition/image-motion checks passed281ms. Build48657 retained that comparison
with an additional invalid-palette rejection, passing974ms.

Build48657: ui_tests pathRibbonPreviewSeek passed1367ms: visible path over the
current image, reveal, checkpoint124/back35 replay, fade removal restoring the
underlying image, and restart. The resulting frame and approximation label were
inspected. These checks do not establish live-event marker reconstruction.

Retained opening RAM qualifies628 bytes80232030 at ROM2A1860 and796 bytes80232694
at2A1EC4. A40-byte data window80239340 uniquely matches2A8B70 and includes the
sampler time constant at2A8B90. The shorter sampler prefix has another ROM match;
it alone cannot establish placement. The width margins at2B8B00 are7/9/11/13/15.

## Preview launch-image selection

Class-four scenes expose a preview-only launch-image choice resolved through the
current ROM's evolution and image-root tables. The scene's own selection remains
the default. Streams requiring an external event image wait for an explicit
choice. Changing it restarts playback and clears input history; selecting another
scene resets the choice. Neither action edits Director commands or project data.

Build43830: ui_tests launchImageSelection manualImageInputReplay passed2085ms.
The new check covers missing event input, two distinct rendered sources, reset
to an explicit scene selection, unsupported-row omission and unchanged scene
JSON. The existing manual-input/seek check still passes. This supplies preview
context; automatic event-state resolution remains unfinished.

## Manual scene-image controls and reproducible input

SceneImageMotion::manual reconstructs opcode55's input and zoom behavior. It
retains the signed image dimensions, strict pre-movement bounds, analog deadband
and truncation, combined digital/analog movement and native thirty-update zoom.
The host receives the two input bytes and raw digital mask explicitly.

SceneImageControls implements6C/6D and the post-channel-draw indicator manager.
Bank56 supplies the four retail animation700 poses. Hidden arrows retain their
records and only attached sprites advance. Reattachment copies the last live
arrow's cursor/delay, retaining its own frame. Available-direction bits attach
arrows when set. Snapshot-owned sprite identities distinguish retained arrows
from new sprites assigned to the same slot. Commands can replace those slots
without silently replacing the image owner's retained record.

The preview offers arrow keys and pan/zoom buttons while6C is enabled. A press
while paused advances one step; held input continues during playback. Per-step
input history makes backward seeking and restart reproducible. Changing input
after seeking invalidates later input and frame checkpoints. New scene selection
clears the preview input history; this history is not project/ROM content.

Build83511: image_tests imageManualAgainstMips passed324ms, comparing1214 original
sequences/4014 updates with no controlled movement or zoom helper. They cover
threshold dimensions, exact edges, combined inputs, disabled controls and full
zoom cycles. Full1304-byte manual code and324-byte indicator code uniquely match
retained opening RAM. A114-byte data window uniquely qualifies the four initial
indicator flags; the sixteen-byte all-ones sequence alone was not unique.

Build24377: sprite_runtime_tests imageControlsAgainstMips passed266ms across14
complete original enable/visibility/disable sequences. Sprite creation/release
use supplied real records; original ownership/visibility/counter logic executes.
Checks include occupied slots, idempotence, replacement records, pose-counter
synchronization and invalid bank rollback. Build63449 retained scheduler
composition/suspension and scripted motion checks, passing277ms.

Build30795: ui_tests manualImageInputReplay passed1948ms: keyboard movement,
124-step checkpoint replay, input changes after seeking, button-controlled zoom,
restart and scene-controlled disabling. The final image was inspected. Its first
run correctly found no movement with a224x160 source; the test now uses retail
row47's288x276 image, which exceeds both native movement thresholds. Existing
pan/echo UI checks passed after the input integration. This establishes manual
image preview interaction, not a complete game controller or image-caption UI.

## Scene image presentation and trails

Class-four previews resolve the image from the edited launch stream and current
ROM evolution/root tables. The raw-word pre-scan matches117 retail streams and
ten boundary cases; all69 table rows and edited selectors/pointers are checked.
Missing event-row fallbacks remain explicit. SceneImagePresentation retains the
original and half-resolution image; ScenePlayback owns their shared0x848-byte
context with the existing pan/zoom service.

Opcode3A reconstructs the camera-angle, packed-matrix and floating-endpoint
sequence, five-bit RGB shifts, ellipse and edge feather. The latter scans
sixteen distances but only writes below eight, correcting the older prose
summary that implied a full sixteen-pixel applied feather. Repeated construction
rejects safely rather than recreating the target's lost source and format error.
Slot zero has a separate drawing owner and currently pauses explicitly.

Opcode50 retains three reusable trail matrices, five-update sampling, the extra
counter increment while draining, low-byte operands and retargeting. Query54
returns1 during interpolation, signed-1 during the tail, then0. The separate
2B529C scheduler service decreases eligible trail alpha by20, including a
repeated cyclic slot in reverse mode. Its caption pool remains separately owned;
ScenePlayback requires the supplemental service to acknowledge that domain.

The drawing plan follows complete2B2868 instruction traces. Forward mode draws
up to three trails before the current image; reverse mode can visit four trails
after it, repeating a slot. Trails use primitive alpha directly; current-image
alpha adds one to texture alpha. Only the current processed image doubles its
geometry. Special sprite markers800/801 draw afterward with pass1 and the
enclosing channel matrix, after the image/trail matrices have been popped.
Their retail pose vocabulary has no authored800/801 entries; supplying real
sprite records verifies drawing, not their still-unimplemented launch creation.

Build17973: image_tests imagePresentationAgainstMips passed3032ms, comparing24
complete original pixel constructions and96 matrix constructors, source retention
and failed-input rollback. Allocation and host libm are controlled. Desktop
imagePresentationPlayback/sceneRoomComposition/worldScenePanAndSeek passed1273ms.
The launch check passed1075ms and groupCoverageAndFixedOrigin passed1384ms.

Build15662: image_tests imageDrawingAgainstMips passed218ms across112 complete
original draw invocations, matrix stacks, alpha and combiner commands. Its
special-sprite stack observation is independent of the desktop renderer.
The attempted authored800/801 UI cases failed on absent pose vocabulary; those
invalid fixture assumptions were replaced with explicit native records.

Build11127: image_tests imageEchoAgainstMips passed894ms:96 complete original
sequences/4299 snapshots plus1008 three-step fade/input sequences. Native
allocation/free/copy and libm are controlled; the caption pool is empty. The
fixture adds bounded aligned-word-pair support already used by other original
instruction checks. sprite_runtime_tests specialImageComposition passed166ms
with real decoded art, visible pixels and independent20/10-pixel movement.
ui_tests imagePresentationPlayback passed1956ms for pan, forward and reverse
transitions,124 updates, backward checkpoints, repaint and restart.

These checks establish owner state, source pixels and desktop composition.
Qt filtering, coverage, blending and combined projection remain approximations.
Manual image controls, caption owners and dynamic launch creation remain open.

## Equipment-dependent body layers

Body drawing now adapts accepted205C88/205484/206340 selection from Decomp
main62cc7ef4. Configuration byte kinds choose body child, equipment child or
item palette; sources57/63 reroute kind2 to equipment children. Special-class
item overrides reuse the maintained registration resolver. Item palette16
requires an explicit launch replacement. Scene draws read the actor's actual
248-byte appearance row; standalone projected draws use class defaults.

Pose38/88 supply the native last-two-frame substitutions. Their replacement
metadata retains flags40/80 and applies the recovered warm-tone conversion after
optional sepia, preserving alpha. The image cache distinguishes source class,
equipment and dynamic palette input. Ordinary/direct raw asset inspection stays
available separately. Body cache aging remains approximate; target allocation
waits and exact RDP rasterization are not emulated.

`body_art_tests bodyImagesAgainstMips equipmentRenderingAndRollback` passed
10959ms after build63774:23094 original metadata/loader cases,62316 selected
layers, all65536 RGBA16 warm inputs, visible equipment changes, repeated rendering
and rollback. Original205C88 registration/205484 metadata/206340 loader execute;
allocation/resource transfer/final pixel expansion are controlled. Every route
and278 items across seven representative classes are covered. The first test
run exposed an incorrect reference entry for202258; correcting the16-byte fixture
offset resolved it without changing the conversion implementation.

The existing raw body corpus passed after build94605. Scene special/material/
projection regressions passed165ms; desktop focus/capture/black/world-seek checks
passed3138ms. The final source-class cache key change retained the focused body
checks. These establish selected resources and desktop behavior, not original
framebuffer equivalence or complete dynamic army launch context.


## Linked secondary placement before depth sorting

World matrix preparation now follows ROM2AA3B0: a linked secondary body searches
slot order for the first primary sharing its body-row identity, copies its height,
and offsets X/Z by opposite eight-unit amounts. Signed facing1 chooses +X/-Z;
other values choose -X/+Z. Missing primaries retain the existing coordinates.
The original body-row predicate controls classification. Missing required rows
reject atomically, retaining the previous Actor pool. Drawing builds its depth
queue after this preparation, as the original world driver does.

`scene_runtime_tests linkedPlacementAgainstMips actorWorldDrawingIsAtomic
specialActorRendering` passed779ms after build99789. The new fixture executes
240 full original prepasses and their classification predicate, with camera and
final matrix callbacks controlled; it compares the positions at each callback.
Existing full matrix comparisons remain applicable. The desktop world/room/
capture regressions passed1774ms after build40005, before a test-only sentinel
correction. Linked equipment artwork and full army launch context remain separate
unfinished work; this establishes placement, not complete dynamic playback.

## Scene color commands reach the preview

The existing SceneEffects opcode33 execution now feeds rendering. Mode-zero
background channels take their record's current RGB directly; sprites consume
the matching pass record, including the native callback's use of its red byte
for all three channels. Actor materials remain independently owned. World
shared masks select background image1, background props2, pre-foreground props4,
foreground image8, post-foreground props10 and direct Stage objects40 (hex).
Background multiplication detaches the image before changing RGBA, retaining
immutable decoded/snapshot images. Existing color timing and snapshots own the
transition; repainting does not advance it.

After build98573, `stage_props_tests passSelectionAndWrapping` passed874ms:
nine masks executed through both original image owners agree on18 background
primitive words and prop-transition routing. `background_tests
backgroundColorPixels` passed128ms, checking RGBA arithmetic and source retention.
`ui_tests sceneColorPlayback worldScenePanAndSeek sceneRoomComposition
sceneSpriteComposition` passed1730ms. Four new rows cover a four-update world
transition, layered backgrounds, sprite channel color, Actor material independence
and exact backward seeking. The green hall result was inspected. Subsequent
changes only rename local coordinates to clear shadowing warnings and format
engine files; these do not invalidate the behavior checks.

References: mode-zero image owner2B04FC, sprite driver29A930/callback29F5E8,
world image owners1FEA1C/1FED0C and object callback1F3C00. This connects authored
scene colors, not the separate gameplay/global lighting interpolation or complete
launch context. Qt filtering/coverage/blending qualifications still apply.

## Stage objects sharing Actor depth order

StagePerspective retains the248-byte owner and192-byte drawable as separate
records. Queue coordinates remain signed32 world coordinates times30; type3
objects use independent signed16 screen positions for drawing. Their80-byte
direct pose retains its linked object identity, call stacks and offsets. Host
slot identities replace pointers explicitly. Randomized placements preserve the
original PRNG call order and below-ground pose-switch/reset behavior. The update
uses the accepted1FFE80 C as its reference; constructors follow original1FB32C
and20F1FC instructions. These host adaptations are not accepted matching C.

StageObjectRenderer shares the world camera basis and layer raster with Actors,
but retains the Stage callback's geometry, size lookup, independent layer alpha
and material presets. The1F3C00 reference is HYBRID_C; its compiler constraints
are not copied into the host. Stage objects bind mask then color; flat props
bind color then mask. Three environment selections use compressed intensity
resource3224BA, RGBA32 expansion with5-bit RGB left shifts, and wrapping texture
coordinates. The outer world drawing owner advances its phase once after each
completed draw. Qt uses point sampling for that texture's quarter-texel phase;
filtering, coverage, blending and RSP intermediate precision remain approximate.

ScenePreview now feeds these objects into the same depth queue as Actors, and
snapshots their poses and texture phase. Snow, scrolling light and environment34
(the background used by scene01FA8BF4) restore identical frames after seeking
past the120-step checkpoint and back, repainting and restarting. Army bindings,
terrain, equipment, scene-color integration, special sprites and final audio
remain incomplete; this does not establish complete dynamic-scene playback.

After build43888, `stage_props_tests -o stage-props-report.txt,txt` passed2617ms:
all102 perspective instances/80 selectors,6126 original update comparisons,
40 original callback matrices, all116 flat props and prior placement/pose/pass
checks. `ui_tests scatteredPropsSeek worldScenePanAndSeek sceneRoomComposition
sceneSpriteComposition` passed3384ms, including three data rows for the first
check. Snow/light/hall images were inspected. `scene_runtime_tests
worldDrawingAgainstMips actorLayerProjectionAndComposition actorMaterialPixelsAndState`
passed164ms. After build90134 the added per-pixel optional-texture RGB/alpha
assertions passed1846ms (`stage_props_tests perspectivePixels`). Texture changes
and a128-quarter-texel wrap match; no original framebuffer comparison is claimed.
The shared-basis extraction separately retained all2196 matrices in732 original
Actor cases, passing with its raster regression in1700ms after build80905.

Construction tests execute the original placement section with controlled
allocation, registration, cache warming, colors and PRNG; updates execute the
original pose decoder/sequencer with controlled resource registry/audio sinks.
The render-matrix reference executes the full callback with empty art and host
libm. Complete bytes uniquely match retained opening RAM:
ROM1FBF00/1212 ->801B8A70;20F1FC/172 ->801CBD6C;
1F3C00/6740 ->801B0770;213270/64 ->801CFDE0;
211EC8/112 ->801CEA38. No emulator input or source-ROM writes occurred.

## Orthographic Stage prop composition

StageOrthographic expands normal and randomized foreground-owned placements into
their native100-byte records. The world drawing prepass advances direct poses
and performs the original signed16 boundary correction. ScenePreview commits
this once per drawing snapshot, respecting alternate mode; repaint and backward
seek retain the completed state. Scattered placements use an explicit fixed
desktop seed, not the current game's random state. Perspective Stage objects
remain a separate owner and are still disclosed as incomplete.

The original drawing order has three prop phases: after the background image,
before the foreground image, and after the foreground image. Base-distance ties
belong to the background. Among foreground-nearest props, headerZ greater than
the foreground anchor draws before its image; the remainder draws after it.
Pose Z offsets affect projection but not this pass selection. This corrects the
earlier task-note interpretation of the latter comparison as a visibility cutoff;
reading only the final sweep omitted the foreground owner's first sweep.

StageFlatRenderer caches decoded frames, retains packed translation and layer
scales, and uses the Stage material presets. Their color-before-mask texture
binding differs from Actors' mask-before-color binding. Layer opacity accumulates
across a prop's layers. Native I4/I8 descriptor images now supply intensity in all
four texture lanes; background-image decoding keeps its separate opacity rules.
Colors/transitions/tint have explicit render inputs; the current preview still
uses neutral scene colors. Qt sampling, coverage, blending and combined matrix
precision remain approximations, not a complete RDP implementation.

After build73533, `stage_props_tests -o stage-props-report.txt,txt` passed1190ms.
It loads/advances/renders all116 orthographic instances across80 selectors,
checks texture alpha numerically, compares75 depth classifications with both
complete original image owners, and compares48 wrapping updates with the original
Stage updater. Existing334 placement,32 initialization and10,880 direct pose
comparisons also pass. Image payloads/final callbacks are controlled in the pass
reference; these checks do not establish original framebuffer equivalence.

`ui_tests scatteredPropsSeek worldScenePanAndSeek sceneRoomComposition
sceneSpriteComposition` passed1288ms: visible animation, seeking beyond a120-step
checkpoint and back, exact restart/repaint, pan and prior layering. Hall candles
and snowy-scene images were inspected. `scene_runtime_tests
actorLayerProjectionAndComposition` passed3ms for the shared raster regression.

## Direct Stage placement and pose records

StagePlacements reads the foreground selector's two lists from current-ROM resource
315736, preserving each complete source row. Its sparse resolver retains untouched
output fields, the -1 terminator, -2 randomized rows and the perspective 8000
height conversion. These are distinct from cutscene Actor and Sprite records.

DirectPosePlayer uses direct descriptor handles through ActorArt::loadDirect.
The portable C sequencer adapts accepted func_001F0E64 and func_001F1218 from
Decomp main62cc7ef4, with explicit big-endian80-byte storage, bounded native
four-entry stacks and resumable instruction budgets. Randomness is supplied by
the caller; sound requests retain their opcode, key and linked-object identity.
This is a host adaptation, not newly accepted matching C or completed audio.

After build58798, `stage_props_tests -o stage-props-report.txt,txt` passed547ms:
334 original resolver comparisons (all80 selectors,168 populated source rows,
terminators and synthetic boundaries),32 initializer comparisons and10,880 pose
updates. The latter use all placement-selected programs plus calls/returns,
counted loops, signed16 wrapping, saturating colors and one-command suspension.
Original resource lookup and audio sinks are controlled; original counted decoding
and record writes execute. Invalid host stacks and missing RNG reject safely.
The shared-loader regression `body_art_tests bodyProgramsAndFrames` passed14170ms.
No rendered geometry or complete Stage playback is established by these checks.

The complete original ranges were uniquely located in the retained opening RAM:
ROM201908/236bytes -> RAM801BE478;2019F4/292 ->801BE564;
1F0E64/248 ->801AD9D4;1F1218/1892 ->801ADD88;2050AC/372 ->801C1C1C.
Further geometry/update references qualified likewise:1FFE80/796 ->801BC9F0,
1FDEC8/2900 ->801BAA38 (including the16-byte preamble).

## World drawing passes

World previews now use SceneCameras::freshModeTwo's current-ROM common-launch
camera, independent from the orthographic background/sprite camera. The latter
consumes the game's packed clip matrix, including its16.16 quantization. Plane
clipping retains depth separately from homogeneous division, so perspective and
orthographic planes share bounded clipping without changing their geometry.
SceneWorldDrawing constructs the packed scale-times-translation matrix consumed
by the world driver (Z=-100); command2C's current projection values feed it.

The observed order is background, sprite pass0, Actor/object pass, foreground,
sprite pass1. SceneBackgroundRenderer accepts independent environment and
foreground selectors. The preview currently uses the common fresh-launch rule
that initializes both selectors from the explicit environment request. Other
launch paths need their own context. Stage props, special sprites, equipment,
terrain, dynamic army bindings and audio remain unfinished and disclosed.
Missing command owners pause. Neutral scene colors and reference lights remain
explicit preview inputs. No exact RDP pixels or full-scene equivalence is claimed.

World non-B5 images use the centered native vertex builder ROM1F91E8, unlike
layered images' fixed origin. Odd widths retain its truncated half-width mirrored
on both sides. B5 records retain their signed origins. Converted HUFF backgrounds
retain the existing decoder's rounding approximation. No fitted-image path remains
in ScenePreview. Images are cached and snapshots retain them through Qt sharing.

`worldDrawingAgainstMips` runs15 complete original drawing-driver invocations
with scene-presence/draw callbacks, memory copying, trig and final services
controlled. Original orthographic setup and packed matrix constructors execute;
all recorded clip/model matrices and the pass order agree. Complete ROM2AC14C's
1460 bytes match uniquely at saved RAM8023C9EC; ROM1FC3BC's380 bytes match
RAM801B8F2C; ROM225D0's336 bytes match RAM800921D0. Revisions/overlays are not
inferred from the generic early-boot mapping. Perspective/Actor projection
regressions passed222ms after build44925; final world/camera checks passed259ms
after build21154, including orthographic clipping and invalid inputs.

After build21154, `background_tests groupCoverageAndFixedOrigin
worldBackgroundSelectors layeredRoomGeometry` passed1987ms. This expands original
vertex comparisons to eight builds across both image owners, loads all80 paired
world selections, and checks differing environment/foreground selectors. The
four UI checks `worldScenePanAndSeek sceneRoomComposition sceneSpriteComposition
scenePlaybackPreview` passed1113ms: visible sprite movement by the pan operands,
background pan, exact backward restoration, and prior layered composition.
The world scene image was inspected. Full game and complete dynamic-scene
playback validation remain outstanding.

## Layered background placement

SceneImages::groupMembers retains every original table ordinal, including zero
entries. SceneBackgroundRenderer owns up to20 immutable channels and64MiB of
decoded images. B5 records retain signed origins and separate record images;
ordinary64 images use the dispatcher's fixed(-160,-120) origin regardless of
dimensions. Background quads span full width/height, unlike Actor vertices.
Packed channel transforms apply before camera projection. Each channel draws
its background, then scene sprites, then its Actor passes. Empty groups clear
old images; unsupported non-image owners reject explicitly. Cached QImages and
Qt shared snapshots avoid re-decoding or copying pixels on every update.

`layeredRoomGeometry` reuses qualified retained original-instruction observations
from the parent room study: six populated channels plus a changed transform.
Every decoded record corner occurs in the native vertices, packed parent
matrices agree, and projected corners agree within.001 pixels. Together with
current-ROM selector checks it passed258ms after build2714. The room overlap/
channel-change, sprite overlap and playback/seek UI checks passed643ms. The
rendered room was inspected, including the Actor behind its table.

`groupCoverageAndFixedOrigin` passed727ms after build27657: all31 groups load
with original ordinals (23 populated,8 empty); five ordinary64 members retain
the fixed origin. Four original image-dispatch/vertex-builder runs cover images
64x48,320x240,415x303 and128x1; only bzero is host-supplied. Dispatch/vertex code
is ROM1FAC5C/1FA714, loaded at RAM801B77CC/801B7284. The dispatcher's first64
bytes also match uniquely in the retained opening RAM image. These checks do
not establish native RDP blending, runtime framebuffer/effect ownership, world
background placement, launch initialization or full-scene equivalence. B5 mask
planes retain the decoder's alpha approximation; current preview colors are
neutral until their scene color-transition owner is connected.

## Layered scene sprites

SpriteRenderer retains native pool selection, packed translation, Z rotation and
uniform XYZ scale from ROM29A930, and the callback geometry/material rules from
ROM29F5E8. It shares the qualified layer raster path, preserving separate texture
alpha lanes and per-layer opacity. Each eligible sprite resets the native main
preset and environment to black/alpha1. The caller retains that environment for
following Actors even when the sprite clips outside the viewport.

Mode-zero playback draws each channel's sprites before its Actor passes, using
the channel's packed parent transform. Special sprite effect projection remains
unfinished; ordinary world passes are now composed separately. Host concatenation and Qt rasterization
are approximations to RSP intermediate precision and RDP coverage/blending.

`packedMatricesAgainstMips` captures packed matrices from192 complete original
sprite-driver calls with original matrix constructors; only sin/cos and the final
raster callback are controlled. `spriteProjectionAndPlacement` uses current-ROM
art to verify visible pixels, pass filtering, a20-pixel translation and2x scaling
without scaling translation. Together with `drawPlanAgainstMips`, they passed
470ms after build2066. The composition UI check passed180ms after build71898:
same-channel overlap, later-channel ordering, native environment inheritance and
exact backward seeking. The Actor playback regression passed344ms in the prior
run (the initial composition expectation omitted environment inheritance).
These establish desktop composition and matrix bytes, not full scene launch or
game framebuffer equivalence. The rendered overlapping sprites were inspected.

## Scene playback panel

The Cutscenes workspace now includes step/play/restart/backward-seek playback
using the project's current execution snapshot. Completed update/draw state and
its image are stored together, so repainting cannot advance draw-driven tint.
Checkpoints every120 updates bound repeat work; seeking yields to the Qt event
loop after at most four updates/eight milliseconds and can be cancelled. The
preview has a10000-update bound and stops when an owner is unavailable.

The current stage explicitly supplies reference cameras, flat terrain, two lights,
neutral world environment, empty external jobs and no army roster. Script camera,
Actor and material changes execute normally. Mode-zero groups retain native
ordinals and image geometry. World environments and ordinary sprites use their
orthographic owner, with Actors on the native perspective camera. Equipment,
Stage props, special sprite effects and audio remain separate. Dynamic selectors/roster requirements
and unknown commands report their missing context. This is not complete native
launch reconstruction or full game playback.

ScenePlayback accepts initial lighting context and handles retained dispatcher
case47. Signed operands convert directly to floats (no thousandths); mode2 also
writes planar0 in X/Z/height order, retaining planar1. Lighting command checks in
both modes and existing playback/transform timing passed392ms after build37441.
The scenePlaybackPreview/cutsceneCatalogSelection UI checks passed1656ms: visible
actors, changed material frames, exact backseek/repaint, missing-context pause,
seek cancellation and reloaded edits. Final focused UI check passed219ms after
build23165 and the scene-sprite guard. The rendered panel was inspected. No game
frame or complete launch-context equivalence is claimed.

## Edited playback resources

ScenePrograms::runtimeDirectors builds an immutable execution snapshot containing
current primary and continuation edits and authored scenes. Existing aliases keep
their shared physical resource identity. New scenes use explicit virtual resource
keys above F0000000; these are not ROM offsets. Deleted imported authored selectors
are removed from new snapshots while already-running snapshots remain usable.
No ROM or project model mutation is needed before previewing an edit.

The editedRuntimeSnapshot check dispatches known marker commands from edited
primary scripts, new scenes and edited continuations. It checks deletion,
original-data preservation and prior snapshot lifetime. It passed223ms after
build6168; existing continuation ownership also passed in the preceding301ms run.
Export allocation remains owned by the separate source-built export path.

## Composed Actor passes

SceneActorRenderer now joins prepared matrices, native queue selection, tint/
material updates and projected layers. Shadows do not advance tint; eligible main
passes advance it once. Each main Actor resets the native preset, retaining the
previous environment. World draws then apply ROM2ADF9C's position-dependent
six-byte gradient, overriding material environment in the original order. Object
geometry requires an explicit render callback. All Actor edits and the private
frame commit together; a late missing object owner leaves the pool unchanged.
Matrix/terrain preparation, equipment placement and full scene context remain
separate. Rendering mutates tint progress and is not a side-effect-free redraw;
UI seeking must retain update/draw snapshots.

The complete520-byte world environment function matches saved opening-state
RAM8023E83C and ROM2ADF9C. actorEnvironmentAgainstMips compares198 full original
calls without controlled helpers, spanning six gradients and clamped positions.
sceneActorCompositionAndRollback checks real bank30 art, hidden-main/visible-shadow
selection, draw-driven tint progress, ordered object rendering, rollback, world
environment override and two shadows. Both tests plus material/primitive regressions
passed296ms after build15998. These establish the composed Actor pass, not scene
background/equipment geometry, complete playback or RDP pixel equivalence.

## Actor material pixels

The projected renderer now consumes the native tint/material command stream.
ColorCombiner decodes RGB and alpha muxes for one/two cycles, including the
second-cycle texture-name swap. ActorRenderState retains environment color and
command order, validates unsupported inputs atomically, and preserves the
single-texture callback's combiner override across following layers. Callers
provide inherited environment state; convenience inspector calls use explicit
transparent black. Native driver resets and world environment gradients are handled by the
scene composition owner described above.

SpriteFrame retains color alpha separately from the transparency mask. Both
lanes crop/flip together and count toward the bounded frame cache. The original
RGBA16-plus-intensity expansion (ROM204A70) and callback (29EE18..29F4C0) bind
mask to tile0 and color to tile1. The halfword2 in the expanded header describes
RGBA16 format, not a texture count. RGBA32 Actor expansion is rejected until its
native copying/layout behavior is implemented; ordinary sprite inspection still
retains its existing RGBA32 decode.

The pose inspector exposes color multiply, tint blend and both material routes,
with Actor/environment RGB controls. A material may make an Actor almost
transparent when it selects zero color-alpha despite a visible separate mask;
the inputs are deliberately kept distinct. Qt coverage/blending/sampling and
floating combiner intermediates remain approximations of RDP output.

Checks after build88427: scene_runtime_tests colorCombinerPrograms
actorMaterialPixelsAndState actorLayerProjectionAndComposition
actorRendererUsesBodyVariant passed104ms; sprite_tests
separateTextureAlphaAndOrientation deltaVariantsAndFallback
actorBanksPosesAndFrames passed3276ms (5696 frames/16768 layers unchanged);
ui_tests projectedActorPreview cutsceneCatalogSelection passed1785ms.
Literal RGBA expectations distinguish the textures and native presets; malformed
command rollback and inherited state are checked. Cropping/flipping uses distinct
alpha bytes; UI changes and backward seeks restore identical output. Rendered
controls were inspected. These are desktop composition checks, not game-frame
or RDP hardware comparisons.


## World Actor matrices

ActorWorldMatrices reconstructs ROM2A9AD0 and its camera-basis owner2AA1EC,
with point-light helpers29D158/29D1B8. Main and two shadow matrices retain
single-precision multiplication order before final packing. Unlike mode zero,
world preparation leaves the stored screen anchor untouched. Depth attachments
use the Actor's own slot identity; terrain queries narrow X/Z to signed16.
SceneActors commits the entire prepared pool atomically and requires explicit
lighting and any needed terrain service. The caller's separate equipment-relative
placement prepass remains outside this matrix owner.

Shadow height follows terrain, absolute-height, additive-height and ordinary
height flags. Default angles/scales with a zero-height point light select two
planar lights; other inputs select explicit projection or the point-light path.
The latter preserves the game's asymmetric height rounding, equal-height branch
and scale clamp. Renderer passes now select either shadow matrix and supply its
independent opacity. The inspector offers world placement using two labeled
reference lights and flat terrain, with separate camera settings for each path.
The native camera-basis convention can visibly tilt sprites at arbitrary angles;
the world reference starts at65 degrees instead of a straight-on view.

The retained opening-ceremony state independently matches the complete matrix
and camera functions at RAM8023A370/8023AA8C, the64-byte constants at8023EC10,
point-light constants at80239220 and rounding threshold at800AE400. These use
their separate qualified ROM ranges; the mode-zero saved image is not a valid
reference for the mode-two slab.

actorWorldMatricesAgainstMips compares732 cases/2196 complete packed matrices
exactly against original instruction execution, controlling only trigonometry
and terrain. Atomic preparation, native coordinate narrowing, depth identity,
unrelated-field preservation, missing terrain and invalid mode checks pass.
The first atomic-check fixture mistakenly supplied eight bytes for a four-byte
depth attachment; correcting the fixture produced a passing check. Projection
checks independently cover the second shadow's displacement and opacity.
These establish the matrix owner and reference preview, not full scene playback,
target-libm/RSP/RDP equivalence or game-level export validation.

## Actor layer rendering and placement inspection

ActorRenderer projects decoded ordinary and class-body layers using the packed
main/shadow matrices, inclusive signed16 vertices and packed per-layer scale.
Primitive color follows ROM29EAFC..29EE18, including the red-only global-color
sentinel and the distinct main/shadow opacity rules. The class-size choice adapts
accepted func_00043d1c.c (43d43ddd); size2 uses the current-ROM 1.1 multiplier.
Body images use the resolved body-handle variant, separately from body flags.
Decoded frames share a bounded64 MiB cache. Qt performs clipping, sampling and
blending; linked equipment remains separate. Material color programs are now modeled as described above.

The Pose inspector now offers reference-camera placement, scale, facing and
shadow controls, with layer selection/visibility and repeatable backward seeks.
Double-clicking an Actor creation command selects its bank, animation, variant,
coordinates and facing. Commands select the layered or world reference view.
This does not execute scene cameras or terrain.

scene_runtime_tests actorPrimitiveColorsAgainstMips actorLayerProjectionAndComposition
actorRendererUsesBodyVariant passes (196ms).216 primitive-color cases execute the
original callback prefix with metadata/texture services controlled. Independent
geometry, alpha, overlap and malformed-input checks cover the desktop compositor;
a real class-body case distinguishes its image variant from its body flag.
ui_tests projectedActorPreview cutsceneCatalogSelection actorPosePreview passes
(1806ms), covering pixel movement, camera changes, seek restoration, shadow/layer
controls and real-command navigation. The rendered panel was inspected. These
checks establish the bounded inspector, not full scene or RSP/RDP pixel accuracy.

## Scene depth queues and mode-zero draw passes

Mode-two pass selection now reconstructs ROM2AA62C..2AAB88. Each eligible Actor
draws both nonzero-opacity shadows in queue order, then the main queue draws
world objects, projected objects and visible Actors. Channels do not filter this
mode. Signed16 shadow opacity is tested before narrowing to a byte, preserving
the original nonzero256-to-zero-alpha callback. Unknown queue kinds are ignored;
invalid/missing references reject before returning a partial pass list.
scene_runtime_tests sceneWorldPassesAgainstMips sceneDrawPassesAgainstMips passes
(681ms):192 complete original world-driver cases/3904 callbacks agree in kind,
index, shadow matrix and alpha; the existing288 layered-driver cases also pass.
Queue, projection, material and raster services are controlled in this check.

SceneDrawQueue reconstructs ROM29D634 construction,29D4F0 flattened-camera depth,
2A17E0 comparison and20FEEC..2101DC sorting in maintained portable C. Objects with
kind1..3 enter first; present Actors follow with coordinates multiplied by30 and
truncated to signed32-bit. Depth uses the selected camera's horizontal view.
ScenePlayback selects the registered bank in mode0 and Actor bank otherwise.

The comparator orders depth ascending, then height descending. Exact ties also
consume the queue and temporary allocation addresses. The API requires those
virtual addresses explicitly; it never dereferences them on the PC or silently
substitutes a different tie rule. The original quick/insertion algorithm remains.
The complete752-byte sorter matches saved RAM801CCA5C..801CCD4C; another ROM copy
has the same prefix but different later branches, so prefix equality is insufficient.

Mode-zero passes reproduce ROM2AFC68 selection: queue kind1 shadows with Actor141
below2, followed by objects and nonzero-kind Actor main passes with bit0 clear.
Both Actor passes filter channel13E; scene objects share their supplied channel.
Invalid record sizes/references and nonfinite/out-of-range coordinates reject.
Pass lists are returned only after complete validation; material updates and final
rasterization remain separate. Queue kinds2/3 retain the original nonzero Actor
main dispatch rule, with checked slot references instead of unsafe dereferences.

scene_runtime_tests sceneDrawQueueAgainstMips sceneDrawPassesAgainstMips
sceneDrawQueueBounds passes (3623ms). It compares648 full original queues/43722
entries, three allocator layouts, both camera branches, tied and crowded inputs.
It also checks288 original-driver callback sequences with channels, flags and
reversed queues. Queue tests control only allocation/free/copy; pass tests control
projection/material/draw callbacks. Neither establishes rasterized game pixels.


## Mode-zero Actor matrices

Portable C reconstructs the complete ROM 2AE654 prepass and 2AEB48..2AEFB8
main/shadow construction using original assembly at Decomp 62cc7ef4. It retains
packed intermediate quantization, registered-camera projection, independent
origin/size-probe constants, channel counterrotation and Actor-camera channels.
Host trigonometry replaces target libm; this is not accepted matching C.
Current-ROM constants use qualified resident and overlay placements.

Shadows use the ten-entry facing table, camera bearing, repeated float angle
wrapping and side-dependent rotation. Flag145 bit2 selects shadow height128;
bit4 adds Actor height120. Otherwise shadows use the ordinary main geometry.
The two native shadow matrix records are identical. Malformed facing, nonfinite
values, packing overflow and nonterminating angle wrapping reject safely.

ScenePlayback render preparation is separate from updates. Channels and all
present Actors commit together after validation. Only Actor20..DF andFC..103
change. Other fields and update counts remain intact. Draw eligibility and final
composition remain separate work.

The instruction reference now executes the entire original matrix function,
with balanced stack and only host trig/memory-copy services controlled. All468
camera/position/rotation/scale/facing/height-mode cases match both matrices exactly.
Independent size-probe aspect constants are included. Late-slot channel and
shadow validation failures preserve prior Actor/channel state. These checks do
not establish displayed game frames or target libm/RSP pixel equivalence.


Scene transform inspection now has a desktop preview with resource, sequence,
duration and channel selection, play/step/restart, configurable playback rate and
backward seeking. Double-clicking command 08 loads its edited operands and the
preceding actor-camera command in source order. The UI explicitly identifies this
as isolated playback: it does not evaluate branches or other scene actions. The
selected image is a channel reference; absent imagery uses a labeled grid.
Long or negative-duration sequences have a 10,000-update preview budget. Seeking
uses snapshots every 120 updates and does not modify project data.

Channel matrix preparation reconstructs original rotation, translation, camera
normalization, concatenation and 16.16 packing in maintained C. Host trigonometry
replaces the target math library. Normalization constants come from the current
mode-zero ROM image; the resident rotation constant has a separate saved-image
placement reference. Background uniform scale is packed and concatenated after
the channel matrix, scaling translation too. Invalid packing rejects atomically.
Desktop plane projection clips against near/far and viewport planes before
homogeneous division; Qt performs image sampling, not the N64 RSP/RDP.

`scene_runtime_tests sceneMatricesAgainstMips sceneMatrixProjectionAndBounds`
passes (315 ms): 36 camera/count cases, 300 channel matrices and 300 background
matrices match original instructions with trigonometric services controlled.
The observed maximum fixed-point difference is zero. Independently calculated
near-plane corners, complete near/far rejection and invalid packing/geometry
checks pass. The existing camera-placement/picking check passes (94 ms).
`ui_tests transformSequencePreview cutsceneCatalogSelection` passes (1,770 ms),
covering actual pixel movement, stepping, seeking, channel changes, timed completion,
invalid sequence recovery and command navigation. Restart recovery after an invalid
sequence also passes in the focused preview check (368 ms). The rendered panel was inspected.
This establishes the bounded preview workflow, not original-game pixel accuracy.

Scene-transform command 08 and query 0D now execute in mode zero. The current-ROM
resource directory supplies 20 channels with six fixed-thousandths fields per
keyframe. Maintained portable C preserves integer delta division before float
conversion, 32-bit countdowns, endpoint snaps and immediate single-frame completion.
The query adapts accepted func_002B1014.c; the other routines reconstruct original
assembly at Decomp 62cc7ef4. Sequence entries are frame/continuation pairs, with
FFFFFFFF terminating a group. Slot 36 is absent. Resource 33 retains three invalid
groups alongside valid groups; starting those invalid groups rejects atomically.

`scene_runtime_tests sceneTransformsAgainstMips sceneTransformInvalidInputsAreAtomic`
passes (1,047 ms): 61 resources, 523 sequences and 2,033 full channel/job snapshots
match original constructor, updater and query instructions. Allocation/free,
zero-fill and resource I/O are controlled. Invalid frames, division, oversized
sequences and nonfinite supplied fields leave prior state unchanged. The composed
`sceneTransformPlaybackTiming` test checks immediate start, before-Director updates,
completion waiting, alternate suspension and budget/retry resumption. It and divider/
playback regressions pass (444 ms). Transform fields and packed channel matrices are integrated. Final full-scene Actor
and background composition remains in progress.

Actor layer assignment now handles 1C, the mode-zero automatic divider 5F/75,
and the stored default-channel byte 7C. The divider uses maintained portable C
reconstructed from ROM 2AF470..2AF6C0 in the mode-zero overlay, preserving signed
coordinate narrowing, single-precision arithmetic, axis and zero-intercept cases,
low-byte channel writes and reused record padding. Release retains Actor channels.
ScenePlayback runs classification in MODE_ZERO_3 before movement. Transform animation is connected; default-channel placement consumers and final
layer rendering remain to connect.

`scene_runtime_tests actorDividerAgainstMips actorDividerPrecedesMovement` passes
(1,496 ms): 72 original-instruction sequences/1,440 complete snapshots match divider
and Actor records with allocation/free controlled. A composed mode-zero check stops
before and after movement to verify classification uses the prior position. Other
empty domain callbacks are explicitly supplied in that check. Invalid Actor slot
assignment leaves its record unchanged. Material, composed playback and pose-sweep
regressions pass (1,053 ms). No original displayed-frame comparison is claimed.

Actor tint and material execution now use a maintained portable C reconstruction
of ROM 29B78C..29BEF4, referenced to Decomp assembly at 62cc7ef4. SceneActors
owns the slot records and handles commands 1E, 1F, 28, 29 and A7. Rendering advances
tint once per eligible draw, then applies the material override. Skipped draws do
not advance progress. Completion retains the record; material release retains
Actor RGB, while single-slot tint cleanup restores white even without a tint.
Full-word all-tint creation and signed-halfword slot narrowing follow the original
code. Unsafe all-cleanup crossing into adjacent tables and dangling empty-slot
all-tint replacement reject without changing state.

`scene_runtime_tests actorMaterialsAgainstMips actorMaterialOwnership` passes
(1,277 ms): 200 original-instruction sequences and 3,600 complete snapshots agree
on 12-byte tint/seven-byte material records, Actor bytes, queries and ordered
display-list material commands. Only allocation/free are controlled. Tests cover
duration narrowing, skipped Actor draws, both combiner routes, masking, replacement,
empty slots and cleanup. Invalid arithmetic and selectors preserve state. Existing
composed playback/suspension and pose-sweep checks pass (204 ms). These functions
are PC reconstructions, not accepted matching C. They establish material behavior
at an eligible draw; final Actor eligibility, geometry and scene composition still
need integration.

Dialogue window records now use the shared UI pool with atomic creation and
continuous actor tracking. All 560 original-instruction pool/tracking comparisons
pass. Current-ROM Serifu loading preserves 4,569 exact entries across 348 archives;
malformed resources reject. Text execution, name context, queue scheduling and
rendering still require integration. See [dialogue-runtime.md](dialogue-runtime.md)
for ownership, source mapping and the unresolved ordinary portrait lookup.

SceneResources owns the shared six 168-byte UI records independently of any
particular callback. Maintained C reconstructs ordinary registration (ROM735C)
and clipped window registration (ROMFA48). It preserves untouched record bytes,
first-free allocation, signed-halfword clipping and the caller's explicit context,
parent and margin values. A full pool raises an error rather than reproducing the
game's infinite wait. All 1,116 complete pool comparisons match original code with
no controlled helpers; slot exhaustion, reuse and bounds checks pass (151 ms).
Dialogue integration and the shared resource scheduler remain to connect.

Entrance command 3F now loads its current-ROM coordinate preset and builds
missing army rows through the native three-object constructor. Existing actors
retain the original skip rules and presentation transitions. Creation preserves
first-free allocation, the constructor's signed terrain-height narrowing, special
body states, sparse output-list behavior and immediate movement/body selection.
Launch variant and roster state are explicit; no cast or destination is fabricated.
The special roster predicate uses side group, member value and flags, including
its exact override byte. Shared presentation opacity becomes 128.

Twenty original-instruction entrance sequences match 2,000 complete actor/motion/
vector snapshots across four presets, existing actors, three members, sparse rows,
special classes and both filtered launch variants. Original constructors, selection,
poses, movement and predicates run; resource loading/allocation, terrain and audio
sinks are controlled. The combined lifecycle suite passes in 2,196 ms. The expanded
missing-context/slot-exhaustion check passes separately in 136 ms; failed allocation
leaves actors, opacity and the lifecycle vector unchanged. This corrects the earlier
two-object assumption. Row type 1 is owner-class 1, not row flag bit 10.

The shared actor-presentation lifecycle now runs in ScenePlayback's mode-two
stage. Filtered command A4 preserves the 29-byte vector, applies side/subgroup
flags, snaps only within the native tolerances, and starts the body return
sequence. Commands 40/A5 query the vector lifetime. Completion distinguishes
stationary state 5 from movement state 2 and releases only on the next empty
sweep. Army rows and linked objects are explicit inputs; missing context rolls
back the whole operation. Speed lookup adapts accepted func_00201670/2016F4 at
Decomp 497181d8, using current class data and resource 315736's two byte lanes.
The alternate-body busy query uses the counted body decoder, not ordinary 0F.

All 44 original-instruction return sequences finish and match 3,520 full actor,
motion and vector snapshots. Another 168 state/variant/delay boundary sequences
and 964 valid speed samples agree. Original selection, registration, pose,
movement and predicates execute; allocation/resource handles, flat terrain and
audio sinks are controlled. Missing-object rollback and bounds checks pass;
the existing composed scheduler check passes. Initial fixture failures were a
missing zero-fill service, an unlinked actor without its held pose, presentation
context mismatch and an invalid class-zero owner 255 speed lookup. The host
rejects out-of-range speed references. This does not establish scene rendering
or dynamic army launch construction. The separate rendering and army launch extraction remain incomplete.

The pose inspector now selects ordinary actors, class bodies or the independent
2D sprite sequencer, with stepping, seeking, per-layer opacity and optional sepia.
Layer origins receive their metadata scale before pose translation, matching the
native matrix order. The UI check uses a retail opcode-12 program to distinguish
2D no-op behavior from Actor movement, verifies backward seeking and displayed
sepia pixels, then switches back to Actor playback. The existing actor/body UI
check also passes. A visible sepia soldier sample was rendered and inspected.
Full-scene camera, lighting, focus/cache history and audio remain separate.

Sepia arithmetic compiles a portable adaptation of accepted func_00203030.c at
Decomp 497181d8. Explicit byte extraction replaces its big-endian struct alias.
All 65,536 RGBA16 values, 4,096 RGBA32 colors and padded-row samples match original
instructions, with no game services replaced. Sprite decoding applies the 16-bit
conversion before channel expansion (including indexed palettes), preserving
separate alpha lanes. The delta/fallback regression passes. ROM 2A2718 confirms
the variant participates both in cache identity and in child selection, with
out-of-range children clamped to zero; older prose describing identical content
across all variants was incomplete.

Sprite draw preparation follows the original pool's layer and special-animation
filters, slot order, signed integer-to-float translation, rotation-180 identity and
signed scale percentage. All 44 pass/transform cases match original calls to the
matrix helpers. Primitive RGBA matches 120 original display-list writes, including
signed global RGB and byte narrowing. The transition's first current-color byte
scales all three global RGB channels; per-layer alpha is multiplied by incoming
alpha/255. Matrix constructors, frame drawing and texture metadata are controlled
in these checks. Final geometry, frame-child selection and PC rasterization remain
to connect. The display-list pointer is RAM 800E9BA0 (the signed load displacement
matters); an initial fixture used 800F9BA0 and was corrected before passing.

SceneSprites owns the thirty animated 2D records. Commands 46/63 create or replace,
64 restarts, 65 queries the current program opcode, and 66 releases one/all slots.
Full records, replacement, narrow constructor slots, full-word query/release slots,
rotation reuse, immediate poses and preserved restart fields agree with original
instructions. Allocation/free/resource registration and the audio sink are controlled.
Only full-word FFFFFFFF inherits the restart animation; 0000FFFF replaces it with
signed halfword -1 and normally fails state resolution. Unsafe fresh-allocation
preserve forms reject without replacing the existing sprite.

ScenePlayback now advances the sprite pool after actors, within the same native
pause guard and resumable stage. The composed Director check includes both end
slots and compares whole records under one-instruction budgets and alternate mode.
Direct rotations execute internally. Sampled/resource rotations require the path
owner's explicit callback, including its temporary resource-path slot 19 lifecycle;
that path implementation remains pending. Final sprite transforms/drawing are pending.

The independent 2D SpritePosePlayer reconstructs ROM 29E7E0..29EAB0 with signed
integer X/Y, the native cursor/delay rules and lossless 80-byte records. Unlike
Actor poses, opcode 12 does nothing and the second material block is not added
into the first. Positional sound selection reads stored Y. All 54,600 retail
invocations and 440 constructed boundary invocations match complete records,
return values and sound requests from the original instructions. Every comparison
also passes with one-command budgets; malformed field writes and unavailable
sound services preserve the current continuation. The original resource registry
and audio consumer are controlled, so this does not establish final sound output.

Actor pose sweeps retain their current slot and instruction continuation across
budget limits or unavailable context. Earlier actors are not replayed on resume;
ScenePlayback retains the sweep's entry guard and reports a pose budget distinctly.
Audio requests accumulate across parts of one invocation and clear at the next
invocation. Three focused checks pass: 64 three-actor sweeps at one-command budgets,
a retail sound-first program resumed after its sound request, and composed scene
pause/resume behavior. Intermediate scene state must not be presented as a completed
update; completedUpdates advances only after the scheduler finishes.

SceneEffects implements the twenty 80-byte scene-color transitions (0x33),
shared RGBA/mask writes, slot-zero mirroring, signed duration/progress and retained
completion. Imported unknown bytes remain intact; fresh allocations initialize
unspecified bytes to zero. All 88 original-instruction sequences match complete
records and shared state, with allocation/free controlled. Atomic malformed-input
checks pass. The mode-two special black fade (0xA9) retains its four-byte record,
60-update ceiling and renderer alpha; 70 original update/render samples agree.
The render test controls camera preparation and has no actors. All 259 source
predicate cases match: ROM 2AA608 checks Actor +E8, the art/class source, not pose
State as the older research prose called it. Full special-pass drawing is pending.

ScenePlayback owns those effect updates. Its mode-two preparation callback is an
intentional no-op: ROM 2AB53C calls 2AB558's empty 28-iteration loop. The earlier
`DIVIDER` service label was wrong for this image. The automatic divider belongs to
a different scene path. ROM 2071F4's resource-cache callback only ages/free native
allocation handles, as recovered pure C at Decomp 5ae68e14 shows. Host ActorArt and
ActorPosePlayer retain resources through Qt ownership rather than emulating that
20-record eviction pool. The composition/suspension check remains passing.

Timed facing turns (0x15/0x16) now read the current ROM's cardinal/phase tables,
retain ten-byte job records and advance through the original cadence, halfword
countdown and lifetime rules. Each phase invokes the existing Actor State path,
including immediate pose execution; a missing animation/facing combination still
consumes the turn phase. Scene-wide failures roll back jobs and actors together.
All 266 original-instruction sequences match full job/query/actor records, including
both directions, zero/negative/wrapped cadence, missing poses and retargeting.
Original State, resolver and pose code execute; allocation/resource registration
and audio consumption are controlled. The test-only instruction shim supports the
observed full-word aligned LWL/LWR/SWL/SWR pairs and rejects other alignments.
ScenePlayback now owns the facing stage; its composition/suspension check passes.

SceneActors now executes the complete movement updater reconstructed from ROM
2A0EF0..2A135C, including terrain calls, slot-motion cleanup, nearby-actor depth
attachments and their second-pass updates/releases. Mode-two proximity requires
explicit current scene state flags; terrain requires a supplied height query.
The two mode-two ground reads, including the paused/expired second read, retain
their native order. Depth tests use truncated coordinate differences, a square
20-unit neighborhood, integer radial bias and the original Z sign. Imported
four-byte records preserve padding; fresh host allocations initialize it to zero.
All 234 original-instruction sequences match 936 complete actor/motion/attachment
snapshots and terrain-call lists. Terrain, allocation and freeing are controlled;
missing flags/terrain reject without partial scene changes. ScenePlayback now
owns this stage directly, and its pause/resume composition check still passes.
This establishes the depth records, not their final render-matrix integration.

ScenePlayback now composes the resumable scheduler, DirectorSession, actor
height/pose updates, projection, camera commands and fade commands. It preserves
partial Director and pose-sweep continuations across host budgets and unavailable
services. The registered counter follows ROM 283664's bounded unsigned increment;
ten original-instruction edge cases pass. Alternate scheduling freezes normal
updates, and a scene-release request finishes the current scheduler invocation
before preventing another one. Fade updates remain an explicit independent call.

A scripted integration check verifies creation, height cadence, projection delay,
registered waits, alternate entry/exit, release and identical complete actor records
under repeated suspension with one-instruction/one-service budgets. Its absent
supplemental pools are explicitly acknowledged. Other domain stages remain supplemental
services; missing ones suspend with their identity. This is an integrated execution
boundary, not a complete retail-scene player or a new UI preview yet.

Director action mode guards now consume their full width without invoking a
service when disabled. Query mode guards retain their separate word-wise behavior.
Command 0x7D marks the fade through its service, then returns a distinct scene-slot
release result without executing later commands; DirectorSession retains that
terminal result. This differs from 0x7E, which releases only the fade and continues,
and the terminal-hold word, which requests no slot release. The original-dispatcher
comparison now covers 51 cases, including every mode-gated action in both disabled
modes and fade-present/absent release. Continuation and bounded-dispatch checks pass.

Actor height jobs (0x7B) now retain their native 12-byte records and slot ownership.
Periodic cadence, signed-halfword wrap, target-mode division, current-ROM tolerances,
expiry-before-application and base/secondary-height flags follow ROM 29A59C..29A7C4.
Target cancellation and job retargeting preserve the original behavior. Imported
padding remains intact; new host allocations initialize unspecified padding to zero.
All 247 original-instruction sequences match 3,705 full actor/job snapshots with
allocation/free controlled. Zero-divisor, missing-actor and whole-scene rollback
checks pass, along with existing actor-record and binding/motion checks. Updates
are connected to SceneActors but still require the scene scheduler's height stage.

The scene scheduler now has a resumable portable control adaptation of original
ROM 2A0680..2A0804. Its reference C at Decomp b5ee7cb3 is HYBRID_C; the host
implementation contains no assembly. Mode guards are evaluated at their original
points, and alternate scheduling selects only counter then Director at entry.
Unavailable services and host budgets retain the current stage without replaying
successful callbacks. The delayed signed-byte counter preserves wrap and callback
reset order. All 265 original-instruction callback traces and state snapshots pass,
both uninterrupted and with a pause before each callback plus one-service budgets.
The callbacks are controlled, so this verifies orchestration rather than every
subsystem's game behavior. Unimplemented services remain explicit boundaries.
The executable places Y-step before the Director; sampled paths follow it. This
corrects the earlier research reference's same-update claim for new Y-step jobs.

SceneFade owns the full-screen overlay independently of Director state. Its
12-byte record, scheduled-slot activity, retarget handshake, signed countdown,
integer alpha interpolation, inherited alpha and explicit cleanup follow the
original constructor/update/query/release paths. A completed timed fade remains
visible until separately released. All 220 original-instruction sequences match
3,960 complete record/query/lifecycle snapshots; allocation, slot registration
and freeing are controlled services. RGB/alpha output and atomic rejection of an
inconsistent zero-divisor record also pass. Callback order relative to the rest
of the scene remains the composition layer's responsibility.

SceneActors also executes uniform scale (0x1D) and opacity (0x48) without
reconstructing actors or resetting their pose invocation. It retains the native
all-slot sentinel, signed-halfword range narrowing and empty-slot behavior.
Scale follows word-to-float-to-double division by the current ROM constant;
opacity stores the low byte. All 160 original-instruction cases match complete
336-byte records with no helper doubles, including large values and empty ranges.

Scene projection changes (Director 0x2C) compile an adaptation of accepted
func_002A91E4 from f53a12e53f3f6e7e389cd6e011b5be25d31bde15. Initialization and
advancement reconstruct ROM 2A90EC and 2A9258. The complete 76-byte record is
preserved, including untouched axes and padding. A full signed duration sets
velocities; its low halfword controls updates, including wrap and zero-count
inactivity. Zero-duration division and nonfinite effects reject atomically.
Thirty-nine changes and 585 updates match original instructions, along with the
initializer's Actor-camera writes. Allocation is the only controlled helper.

SceneCamera keeps the fourteen native camera floats, current-ROM mode-two
initialization and independent Actor/registered/shared banks. Commands 0x35/0x36
preserve untouched fields and their differing shared-bank rules. Projection and
height-plane picking use desktop perspective math without adding GUI dependencies
to the engine. Four game-derived packed-matrix origin cases agree within 0.01 pixel;
four default-camera height-plane round trips, clipping and atomic-invalid-camera
checks pass. This is useful camera geometry, not RSP rasterization or complete
actor/channel matrix preparation. These components await scene-clock/UI integration.

Class-body art now follows the current ROM's handle resource (key 0x315736)
and separate descriptor root (0x3B6CD0). The handle table extent comes from its
header (retail offsets 0x24..0x584), not the entire resource's later tables.
Compiled `lordly_actor_body_handle` adapts accepted `func_002015C8.c` from Decomp
`497181d8a6ba4bc32c35a465fccd261abe2e0c69`: source class record byte 0x45 selects
source versus owner; flags index the handle; its low 12 bits select the descriptor
and its high nibble supplies the body image variant. ClassData supplies current
records, including recognized relocated class tables. Shared descriptor parsing
preserves metadata, programs, palettes and art as for ordinary cutscene banks.

All 108,900 retail source/owner/flag combinations match the original resolver.
The 660 same-source/owner routes include four absent class-zero handles. Across
381 distinct descriptor/variant combinations, 17,541 programs, 16,201 frames and
54,539 layers match the independent prior decoder. Alternate pose execution matches
141,064 original MIPS invocations across 17,633 states, including special-class
state clamping. Its resource registry and audio consumer are controlled; the linked
object is explicitly absent. Unknown linked-object state still suspends effects
18/20. Explicit linked state emits a deferred sound-service request containing
the native object identity, source/owner context, key and opcode; the audio
consumer can resolve spatial/equipment effects independently of pose execution.
Another 141,064 original-MIPS invocations with a linked object present match all
pose fields and sound-service arguments. Final sound selection/synthesis is not
part of this comparison. The pose query remains an ordinary-bank lookup even for alternate actors.

The actor preview exposes class source, owner and flag selection, uses the handle's
image variant, and supports alternate pose stepping/seeking. It identifies its
standalone context: no linked equipment actors, muted audio and material values
without their rendering effects. UI switching/invalid-route checks pass and the
General body preview was inspected. This does not yet implement the Director's
final linked sound resolution, equipment drawing substitutions, army cast construction
or camera projection. Ordinary actor runtime and frame corpus regressions still pass.


`SceneActors` implements Director body selection (0x2A in mode 2) through
compiled C record transformation and explicit caller appearance rows. It inherits
sentinels, allocates the first free identity among 28 actors, reads current class
appearance defaults and all 278 equipment records, registers the selected body,
and executes its immediate pose. Primary actors propagate the original arguments
to paired secondary actors; secondary states below 50 gain 50, while requests
already at 50 or above leave that secondary unchanged. Motion stays slot-owned.
Missing context or failed resources roll back the entire command, including rows
and previously updated actors. Other modes consume 0x2A without changing actors.

The appearance getters adapt accepted func_00043e88/43edc/43f30/43f84 at Decomp
4123be2bb9912bb8278dbe360b3f3b742c0c182a. Registration's equipment variants adapt
func_00207658 at 469a1416918592749d61dc34e3e079796f7b673c. The record initializer,
linked classification and first-eligible-item selection are portable reconstructions
of ROM 2A9364, 20C120 and 45934 respectively. All 675 original-MIPS initializer
cases pass: pre-pose record fields, complete post-pose records, caller rows and
registration inputs. Original registration/appearance/selection/pose code runs;
allocation and pose-resource access are controlled, with linked sound objects
absent. Separate context, mode, motion and rollback checks pass. Equipment drawing
and the deferred sound consumer still require connections. The PC resource
cache does not reproduce the original twenty-slot allocation wait.

`ActorInstance` now preserves the complete 336-byte native record while exposing
its typed pose state. The State adapter compiles recovered `func_002A08C0.c`
(Decomp `9955a5cdcf73a35e4ccf57ab9bb3b6d85dab40b3`). Direct construction follows
accepted `func_0029D790.c` (`ef3343c31854d42e3093a3ac0c2c6a23b3d9f921`) and a
portable reconstruction of its ordinary initializer at ROM 0x29DC0C. Both execute
the required immediate pose. State retains inherited fields, material deltas and
unknown bytes; its native X/Y coordinate predicate intentionally ignores Z alone.
Terrain calls remain explicit. All 82 tested record cases match original bytes
before and after the immediate pose, with controlled allocation/zero-fill/terrain
services and the pose resources supplied through their registry boundary.

`SceneActors` owns 28 actor slots separately from their motion objects and supplies
State, creation, Move, pose/countdown query and current-unit-member binding services
to the Director. Binding requires complete caller rows and unit-member inputs.
It updates both actors' native slot fields while motion stays with each slot.
Eight binding cases match original pointer/record results with no helper doubles;
composition checks cover empty slots, swaps, temporary vacancy, resumed motion,
queries and atomic rejection. Planar and pose advancement are explicit scheduler
components, not complete scene ticks: terrain, proximity, secondary actors and
other normal-scheduler services still require integration.


The Cutscenes workspace now includes a native actor-pose preview. It reads the
current bank directory and animation vocabulary, supports pose/variant selection,
stepping, deterministic seeking, adjustable preview speed, zoom and layer inspection.
The native C pose sequencer preserves counted-record cursors, delays, loops, state
changes, signed coordinate effects, clamped material updates and explicit suspension.
It is a portable reconstruction of ROM 0x29E218..0x29E70C, not an accepted matching-C
source. Equal pose offsets alias the following program at runtime; physical empty
slices remain unchanged in the resource model. Ordinary audio requests use current
ROM tables. The projector writes a separate vector; the native audio branch reads
the original template input. Playback records these requests without producing sound.

All 54,600 checked sequencer invocations (24 for each of 2,275 programs) match original
MIPS execution in 4 MiB memory: frame, cursor, delay, current/previous state, float
coordinate bits, material/delta bytes, return and audio requests. The original counted
lookup also runs; resource registration, the output-only projector and audio consumer
are controlled services. Budget/resume, unavailable service, material bounds and
zero-delay-cycle checks pass. Three focused UI checks pass, including engine-aligned
stepping/seeking and surrounding scene/desktop regressions. A rendered native actor
preview was inspected. This is ordinary pose execution; complete scene scheduling,
alternate body bindings, camera projection and material rendering remain unfinished.


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
Qualified ROM terrain images now use the approximate calibration described above;
other images retain a separate reference. Special placement differences and dynamic
group behavior are explicit limits; this is an initial-placement editor, not live mission execution.

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
Browser scenario projects (versions 1ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“4) and scenario sections in the main project
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

The promotion model also supports 165ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“256 class IDs. Growing from the original
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
Its special secondary-class rule remains limited to original IDs 42ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“164;
new IDs use their authored compatibility and requirement tables.
The Army action checks likewise apply their special-class restrictions only to
original IDs 81ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“164. Both alias routes retain their original default-class
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

Captured-frame transitions now retain the original16-byte state separately from
persistent sepia. Normal scheduling advances unsigned16 progress after parsing;
selectors2/3 release on exact duration equality,4 holds, and other selectors keep
their native inactive/no-progress behavior. Zero duration for2..4 is rejected.
The desktop connects2/3 to the previous completed frame, RGBA5551 quantization,
background conversion/reload, Actor/sprite/Stage sepia decoding, and the vignette.
Selector4 remains unavailable in the desktop until its special Actor passes join.

Vignette geometry follows29B388/29B660: ten interleaved vertices form four
gradient strips and the inner quad, with alpha10/200. World drawing uses the
fixed translation atZ-100; layered drawing retains the final channel matrix.
The captured frame overlays the vignette before the independent color fade.
Snapshots retain image and effect state; repaint does not recapture or advance.
Source images detach before conversion. Mode2 restoration reloads Stage props
using the continuing desktop seed. Game cache lifetimes, repeated sepia on
already decoded Actor/prop images, RDP dithering/filtering and blending remain
approximations. Opcode73 cleanup and80 cache-sensitive focus are not yet wired.

Validation executes112 original constructor/update/render sequences,96 unsigned
boundary updates and all10 vignette vertices. Allocation and presentation service
calls are controlled. Pixel tests check storage quantization, immutable snapshots,
RGBA16 sepia arithmetic and vignette intensity. Desktop tests enter and restore
both scene modes, cross a120-step checkpoint and reproduce earlier frames exactly.
These are not original-framebuffer equivalence tests.

The special black pass now composes after the vignette and before the captured
frame in world scenes. Original2ADAF4 exits after the first eligible Actor
(source81..83 or163) in slot order;2ADF00 jumps directly to the epilogue. This
corrects the broader all-Actor interpretation in the parent command reference.
Without root1CAC the entire pass is absent, including transition selector4.
The callback always uses main geometry. Selector4 adds an image-processed full
opacity pass, then a normal pass at255-floor(progress*255/duration). Otherwise
the normal pass overrides presentation opacity to255. Tint/material jobs do not
advance in this pass. Processing bit0 clears afterward; vignette presence is
separate and persists. Actor record opacity remains unchanged.

The126 original-driver cases check first-match selection, hidden flags, opacity
restoration and callback arguments with graphics/image services controlled. A
real body rendering check verifies main geometry, two layersets, immutable Actor
and tint state and repeatability. Desktop checks verify60-update black-fade
completion, crossfade acceptance and backward seeking. Build and focused checks
pass; cache lifetime and RDP rasterization remain the limits stated above.

Ordinary Actor image-processing decisions now live in the scene snapshot, apart
from the renderer's pixel LRU. They are keyed by source, variant and art index.
Access resets age to0; each completed draw increments it and expires an unused
image at3, following2A2718/2A2AD0. Expiry can therefore change visible color.
The desktop reclaims empty metadata owners; target allocation bookkeeping is not
emulated. At most40 source variants are retained.

Opcode73 now converts backgrounds, enables image processing and recreates focus
at-1. Its parser2857EC calls29AF24 only; it does not call the separate2A2BC8
ordinary-image converter. Previously loaded ordinary images retain their color.
Capture selector2 additionally converts retained ordinary images, including
repeated conversions. RGBA16/32 processing preserves the separate alpha lanes.
Counts saturate at64 because the monotone conversion has already mapped even
white to black; both pixel formats' bounds are checked. Stage/body/sprite cache
lifetimes and repeated conversions remain separate approximations.

Opcode80 is dispatched by ScenePlayback. It narrows the selector to a byte,
requires the focus controller and a valid0..27 slot, and invalidates the selected
Actor's source/variant. An empty slot is safe. Other previously full-color images
remain cached. New loads suppress processing for the selected slot and during
selector3. No focus controller suppresses ordinary Actor processing. Shared
source/variant images keep the first request's decoding until invalidated/expired.
The selected slot and cache decisions persist through backward seeking.

The original loader/aging/refresh routines pass240 flag-and-hit cases plus a
conversion/expiry/focus sequence with decompression/allocation/pixel conversion
controlled. Rendering checks retain a previous focus, reload it after expiry,
restore a snapshot, and preserve alpha during repeated conversion. Desktop
focus changes, checkpoint seeking and earlier capture/black/color checks pass.


The standalone Actor inspector now offers named equipment and explicit launch
palette selection. Class defaults initialize both flat and projected body art;
equipment changes redraw without restarting the pose. The ordinary image-variant
control stays available for ordinary Actors/sprites and is disabled for bodies.
`ui_tests equipmentBodyPreview projectedActorPreview actorPosePreview
spritePosePreview` passed1027ms after build56484; equipment changes in both views,
class-default restoration, pose preservation and seeking agree. The engine
render/rollback check passed107ms after extracting shared default appearance.
A layout-only adjustment made the launch-palette placeholder fully readable;
build66264 and equipmentBodyPreview passed200ms. The complete equipment inspector
screenshot was inspected. These controls preview appearance, not persistent
inventory edits or automatic dynamic-army launch selection.


## Scripted oversized-image motion

SceneImageMotion retains the16-byte pan job and updates the explicitly supplied
848-byte image context and channel-one88-byte transform. The compiled C starter
adapts accepted2B4D58; update functions reconstruct2B3FE4/2B4FB4. Both start words
must equal-1 to use current coordinates. The inclusive one-unit deadband produces
zero rate. Starting/retargeting derives rates without changing current position.
Zoom reversals retain the original counter complement. Signed interpretation and
unsigned counter increments preserve word wrap; nonfinite PC arithmetic rejects
atomically. Native constants0.3/1/0.3/0.7 come from current ROM2B8AC8.

The pan updater advances zoom first, moves exactlyN times for positive durationN,
and releases on the following update. ScenePlayback handles76/77 and scheduled
MODE_ZERO_4 before Director dispatch; alternate updates omit it. Image context
must be supplied by its owner, so an absent launch image suspends76 explicitly.
Snapshots retain the job, context and channel. This component does not yet create
or draw the image presentation itself.

After build82953, imageMotionAgainstMips/imageMotionPlayback and existing playback
composition/suspension checks passed:288 original sequences with retargeting,
5271 updates and five counter-boundary sequences. Complete records, queries,
untouched bytes, guarded allocations, missing-context resume, delayed release,
alternate omission and snapshot replay are checked. Only allocation/free are
controlled in the original-instruction fixture. The same run found an older body
variant test expected raw frames; it now includes the independently qualified
body/equipment substitution. Build39118 and actorRendererUsesBodyVariant passed
114ms, retaining class19's body variant2 across cached redraw. No production image
motion code changed after its passing check.

Original code2B3FE4..2B4104 and2B4FB4..2B5044 match retained opening RAM at
8023F1E4 and802401B4. The constants' whole80-byte blockRAM80243C98..80243CE8
uniquely matchesROM2B8A98..2B8AE8. Data placement is not inferred from code offsets.

## Earlier checked portable development package

The local package native/dist/LordlyCaliber-0.5-dev-20261005-234219.zip contains
d5608b1 (action ordering), before the later Actor control additions. The isolated
check-package.ps1 passed startup, bundled user-time compilation, export, independent
Knight STR300 readback, reopen and source preservation. check-scene-import-package.ps1
also passed browser scene conversion, bundled compilation/export/reopen and2648
expected scene bytes. Neither check claims a clean Windows installation or a release.
Evidence remains in parent Test ROMs/native-portable-8ce057af4f004b86b870a0ccd79d513b
and portable-scene-import-bc9a98241986451d8e791481747b5091.


## Saved squad carried items

Regular battery slots expose all ten carried-item positions beside formation and
reserve controls. Named choices apply immediately as one save undo action; the
capacity readout uses the current project's paired class getter, including the
Soldier1/1 rule. Pending formation/Unity/tactic drafts disable supply changes so a
refresh cannot discard them. Multiline game names are flattened for dropdowns.
Changes edit carried slots directly and do not consume/return global inventory.

SaveDocument preserves slot order and gaps. Original Army load copies ten saved
bytes at squad+13..22 to active halfwords+34..52; its save routine compacts nonzero
entries. The original inventory selector accepts nonzero IDs whose current item
record byte5 is zero: retail1..20 and24..31. Product choices additionally respect
the loaded save codec's field widths (retail five bits), rather than assuming the
runtime byte width. Existing unusual IDs remain readable/preservable; new invalid
choices, over-capacity results, suspend records and active runtime states refuse.

Save export revalidates newly edited carried slots against the current project,
including edits in the other normal slot. Later metadata/capacity edits cannot
silently export invalid newly edited supplies; untouched unusual source slots
remain unchanged. Showing the squad panel refreshes choices and capacities while
preserving a pending formation draft. The generic packed-save writer keeps other
bits and the original checksum/container rules.

`save_document_tests` passed in7075ms with the new squadSupplies check and existing
formation/reserve/transfer/save-format checks. The new case checks three battery
layouts, both regular slots, whole-memory preservation including inventories,
exact undo/reopen, current metadata/width/capacity refusal and export revalidation.
Original-MIPS list selection matches retail and changed metadata; the original
codec and Army load/store blocks confirm six edited snapshots. The UI checks
`saveSquadEditing saveWorkspaceEditing` passed in3009ms, including actual named
choice changes, immediate undo/redo, export refusal/restoration, draft protection,
reopening and read-only runtime controls. The supplies capture was inspected.
These are offline checks; no natural Army load, item-use behavior or emulator
operation is claimed. Parent save-squad-layout-20261005.md records exact code/RAM
identity and the corrected five-bit storage limit.


## Unapplied saved squad drafts

Formation/Unity/tactic fields retain explicit Apply and now offer Discard squad
changes. A pending draft counts as unsaved save work even before an undo command
exists. It remains visible when leaving and revisiting the Save workspace. While
pending, squad and save-slot selection, other save edits, export, restore and save
Undo/Redo are disabled; direct export/edit/slot/squad-selection paths also refuse
without discarding the draft. Apply remains available after invalid input is
rejected, and Discard reloads the stored squad. MainWindow's global Undo group
suspends the Save stack while that workspace has a pending draft; project Undo in
another workspace stays independent. Existing open/close discard handling uses
SaveEditor::modified(), so it also includes unapplied squad changes.

The new `ui_tests saveSquadDraftProtection` uses an original-encoder two-slot save
and the actual MainWindow. It checks unchanged bytes while pending, dirty state,
selection/slot/export guards, no output file on refusal, local/global Undo routing,
workspace revisit, Apply as one command, explicit Discard, and a rejected unknown-
member formation retaining its draft for correction. Together with existing
`saveSquadEditing saveWorkspaceEditing`, it passed4346ms. Save engine results from
the carried-item change remain applicable; this is an application workflow change.

The full application build passed without compiler warnings. The final capture
and `saveSquadDraftProtection loadDesktopWorkspace` check passed2271ms; the
pending-state screenshot was inspected. Existing squad/save checks above remain
passing. No game-code, save codec, dependencies or ROM rebuild code changed here.


## Current checked portable development package

The local ZIP `native/dist/LordlyCaliber-0.5-dev-20261006-022437.zip` contains
2152fdc, including carried-item editing and pending squad draft protection.
`check-package.ps1` passed in2.57s: isolated startup, bundled user-time compilation,
export, independent KnightSTR300 readback, reopening and source preservation.
`check-scene-import-package.ps1` passed in6.91s with2648 expected scene bytes,
browser conversion and bundled compile/export/reopen. Evidence is in parent
Test ROMs/native-portable-cd9229aebfab43e19092d2a600b93734 and
portable-scene-import-d5beac1f0d7e49eeb368954995d1fc16. This is a local development
package, not a clean-machine validation or published release. No new dependency.


## Ordinary scene sprite Actions

The Actions inspector now creates ordinary scene sprites, changes their poses and
releases them (Director46/64/66). Creation exposes slot, art bank, animation,
facing, X/Y, drawing channel, scale and variant. Pose changes preserve-1 inheritance;
release supports one slot or-1for all. These use the existing native sprite engine
and existing source-built Director resource export, with no new game-code hook.

Sprite slots0..29 are independent of Actor slots0..27. Automatic allocation checks
all constructor/restart/query/release references. It honors low-byte constructor
slots, so0x100 occupies slot0, while query/release retain full words. It refuses to
assume a free slot when a launch translation selects one dynamically; pose/release
templates preserve the preceding symbolic reference. Existing opaque source operands
remain intact. UI edits validate changed explicit art/pose/facing against the current
ROM; missing poses, bank0 and inheriting both coordinates during creation refuse
without changing the stored project. Unresolved inherited pose context remains the
runtime's responsibility rather than being replaced with invented data.

`scene_action_tests sceneSpriteActions authoringBoundaries` passed1411ms. It checks
native creation/advance/restart/release, slot isolation/aliases/exhaustion/symbolic
references, bounds, unchanged opaque words, insert/remove/reorder, project folders
and user-time compilation/export/reopen with all three sprite actions. Candidate:
parent Test ROMs/native-sprite-actions-DaIbFK under the master filename; reopened
Director references validate. `ui_tests sceneSpriteActionControls
sceneSpriteComposition sceneActionInsertion` passed2977ms, including actual Add,
position changes producing different rendered frames, exact Undo/Redo, invalid
pose/bank/coordinate refusal, discard and removal. Screenshot inspected. Fullbuild
30428 passed without compiler warnings. Existing sprite instruction-comparison
results remain applicable because its C/runtime implementation did not change.
These are offline authoring/export/preview checks, not natural game loads.

### Special sprite800 investigation boundary

A current-ROM census through the existing browser codec found two retail opcode63
requests for animation800, bank56: streams01F62730 and01F635DC, both at word749.
There is no corresponding8000 key in the standard vocabulary (all68banks checked).
The original constructor probe creates state-1. Original counted decoder z64
2A3198/live80233968 indexes the program directory directly, so this reads the word
before that directory. Changing the controlled word changes subsequent pose state.
A blank result with a zeroed fixture prefix therefore does not establish a valid
fallback. The desktop retains its explicit missing-pose pause for this unresolved
launch context. The earlier supplied-record rendering checks for800/801 establish
drawing only. Probe generator native/build/research-special-sprite.cjs reuses the
existing scene-sprites-mips.cjs harness and read-only master ROM; its JSON retains
both controlled-prefix cases. No fresh emulator or natural launch was performed.


## Selecting visible scene sprites

The live-object picker and rendered-frame selection now distinguish Actor and
sprite slots, including identical numeric slot IDs. Sprite layers enter the
selection snapshot in their actual render order in layered rooms, world flat
passes and image-owner special passes. Foreground scenery, later Actors, dialogue,
crops and screen covers retain their occlusion. This adds selection bookkeeping
without changing frame composition or advancing game state.

Selected sprites expose current art, animation, script-sign XY, channel and scale.
Click pauses and selects; double-click navigates to creation or a supported direct
action. Extended placement commands without Actions fields open the exact Commands
row instead. Symbolic launch operands are not guessed. A yellow UI-only outline
marks sprites; Actors retain blue. Selection survives absent steps and seek replay,
and an open dropdown is rebuilt only when membership changes.

`scene_runtime_tests visibleSpriteSelection visibleActorSelection
sceneActorCompositionAndRollback` passed in106ms: overlapping typed targets,
transparent holes, scenery, clipping/fades and bounds, plus existing Actor/render
rollback cases. `ui_tests spritePreviewSelection classActorCreationControls
savedStoryCastPreview sceneSpriteComposition sceneRoomComposition` passed in6232ms.
Actual Actor0/Sprite0 click and double-click routing, readback, absent-step/replay,
raw extended-command navigation and exact project/frame preservation pass. The
captured sprite selection was inspected. This checks desktop picking and interaction;
no new game-code or natural-launch qualification is claimed.


## Direct sprite rotation Actions

Director63's direct-angle form now has Create rotated scene sprite and Rotate
scene sprite Actions. Creation allocates an unused independent sprite slot and
exposes art, animation, variant, XY and signed16-bit rotation. Native defaults
remain facing0, channel1 and scale100. Reuse changes only rotation and retains
all other record state; inactive source words and unusual non-minus-one reuse
sentinels remain byte-exact. Native rotation180 is unrotated. Path-oriented or
launch-selected creation forms remain in Commands pending their path context.

Changed explicit creation poses validate at facing0. Missing poses, invalid
angles and uninitialized coordinates refuse without publishing invalid input.
The common action field reader now also treats088800xx launch translations as
read-only, including full-width coordinates; unrelated edits preserve them.
Visible-sprite navigation recognizes direct63 creations and reuse, retaining the
raw Commands fallback for path-oriented forms.

`scene_action_tests rotatedSpriteActions sceneSpriteActions authoringBoundaries
exactFields bodyActionFields` passed4997ms. Direct-create records, reuse identity
and whole-record preservation, native field widths, symbolic/ignored operands,
ordering/removal, project folders and compiled export/reopening pass. References,
allocation bounds, CIC CRC and master preservation pass for the rotated candidate
in parent Test ROMs/native-rotated-sprites-QixZnr. Existing ordinary-sprite and
Actor-action regressions pass. `ui_tests rotatedSpriteActionControls
spritePreviewSelection sceneSpriteActionControls bodyActionEditing` passed3358ms:
actual creation and rotation fields change the rendered frame; exact Undo/Redo,
invalid input, read-only launch values, selection routing and raw-path fallback
pass. Capture inspected. The underlying sprite runtime and source remain unchanged,
so their existing original-instruction comparisons apply. Natural event launch is
not established by these authoring/export checks.


## Sprite orientation from native paths

Scene playback now supplies Director63 with sampled-path and resource-path headings.
The visible ribbon owner retains native starting/ending headings in record+74/+76;
resource lookup evaluates a temporary path without replacing a visible ribbon.
Both derive the starting tangent from sample2 minus sample0 after the original
400-sample length pass and dense resampling. Separate current-ROM degree constants
remain distinct. Missing sampled paths, invalid/empty resources and missing linked
markers suspend explicitly and preserve the prior sprite.

The heading calculation uses maintained C reconstructed from resident ROM2D3E0 /
RAM8009CFE0, including its single-precision continued fraction and ROM constants.
Host atan2 is not equivalent at integer truncation boundaries: numerator1,
denominator1 produces44 degrees with the original routine, versus45 using host
libm. This C is a desktop adaptation, not an accepted matching-Decomp claim.
Resource sampler ROM2B669C and sampled builder ROM2B7424 use overlay delta7FF8B200;
the early-boot mapping is used only for the resident angle routine/constants.

Path lookup receives the staged new/replacement sprite after its immediate pose
update, so linked points can see the correct position before commit. The callback
is synchronous and not retained in snapshots. Resource-path evaluation owns no
persistent native slot19 in this implementation; other unimplemented path owners
continue to suspend when their commands/services are requested.

`image_tests pathRibbonsAgainstMips` passed1208ms. The fixture executes the actual
angle, both path samplers and existing ribbon lifecycle instructions.121 finite
axis/diagonal/ratio cases match the C's float bits; all121 nonempty retail paths
match starting/ending sampled headings and temporary-resource headings. Existing
curve and45 update snapshots per path still agree. Allocation, decoding, sin/cos
and vertex packing remain controlled; the original atan2 is no longer substituted.
`scene_runtime_tests pathSpritePlayback imageMotionPlayback irisPlaybackLifecycle`
passed504ms: both heading routes, independent copied owners, missing-path retention,
prepared-sprite rollback and a custom in-memory resource linked to the replaced
sprite pass. The last two older fixtures now select a non-title script; their old
first-resource selector0 ran the title scheduler and skipped the expected updates.
`ui_tests pathSpritePreviewSeek pathRibbonPreviewSeek` passed2963ms; visible sprites
and ribbons replay exactly through35->124->35 and restart. Earlier direct-rotation
and sprite-selection UI regressions also pass. Capture inspected. This qualifies
offline runtime and desktop preview, not a natural event launch or N64 rasterization.


## Authoring sprite path headings

Actions now supports Create sprite with path heading and Set sprite heading from
path, both encoding native Director63. Creation exposes source/pose/variant/XY
and path group/entry; reuse exposes only slot and path selection, preserving its
other words and live state. Group-1 means an active sampled path slot0..19;
nonnegative groups select current-ROM resources. Unknown launch-selected route or
creation modes remain in Commands, while other symbolic operands remain read-only.
Sprite selection navigation recognizes both forms.

Edited resource references validate through the same decoder as playback, including
nonempty directory entries, point shape and linked-slot bounds. Active-path existence
remains a runtime condition since control flow determines which paths have been
created. A valid but absent slot pauses clearly. The resource validator does not
invent positions for context-dependent linked points.

`scene_action_tests pathSpriteActions rotatedSpriteActions authoringBoundaries`
passed3166ms: independent slot allocation, both native encodings, field bounds,
ignored-word preservation, actual path-to-sprite record updates, reuse identity,
project folders and user-time compilation/export/reopening. References, allocation
bounds, CRC and master preservation pass. Path candidate: parent Test ROMs/
native-path-sprite-actions-oPSqtx. `ui_tests pathSpriteActionControls
rotatedSpriteActionControls spritePreviewSelection pathSpritePreviewSeek` passed
3934ms: changed orientation, exact Undo/Redo, invalid group/entry/slot refusal,
missing active-path pause and correction, selection routing and seeking. Capture
inspected. `image_tests pathRibbonsAgainstMips` passed1343ms after the shared-reader
refactor, and `sprite_runtime_tests lifecycleAgainstMips
lifecycleBoundsAndMissingContext` passed352ms. These remain offline authoring,
preview and exported-resource checks rather than a natural game load.
