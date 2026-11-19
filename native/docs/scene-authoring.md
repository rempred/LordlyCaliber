# Native scene authoring

## Play a story decision from its event

In Logic, open **Player decisions**, select the cast or question you want, and
choose **Preview decision…**. If you change the outcomes, save the decision
before opening its preview. Cutscenes opens the question at the chosen event
request. Use **Play** or **Step**, then **Up**, **Down** and **Confirm** to answer.
For a conversation with several questions, such as Sheen's offer, use **Edit
cutscene…** on the scene request in its event row. This also opens the scene
with that event's launch inputs.

When the scene asks gameplay for its successor, the preview resumes the selected
mission rule at that request. It follows the answer through branches and jumps
to the next scene, carrying the current cast and the story flags written along
that path. **View event outcome…** lists the actions for the answer you chose,
including the next destination when the event identifies one. The timeline
continues to edit the question. To edit a reaction, open its scene from Logic.

Wentinus, Sheen, Liedel and Paul use this handoff. For Sheen, the last answer
decides whether he joins. The same question launched from a different event
can have a different outcome. Playback follows each answer independently. If one
answer needs a campaign value the preview lacks, choosing it explains the
handoff. You can still play the other answer when its reaction is known.
Later campaign actions shown in the outcome come from the event program. Use
the exported game to play the resulting battlefield.

While a dialogue window is on screen, its source appears below the preview.
**ROM dialogue** identifies an entry in the open ROM. **Project dialogue**
identifies an entry added or edited in the current project. For example, in
The Southern Reaches, accepting Dio's challenge plays scene 12. Dio says
“we're not pest control” from archive 5, entry 9. Declining plays scene 13
and “we're not janitors” from archive 5, entry 20. Open the challenge from
its event in Logic to try the two paths.

Leave **Preview reward** on **Use event reward** to use the item selected by
the parent event. The Bacchus exchange gives the Pedra of Bane. If the item
depends on an unknown campaign value, choose a sample to see its icon and
name. A sample changes only the preview. To change what happens in the game,
edit the event's selection and its inventory award together.

## Replace a scene or write your own

To keep a retail scene's place in the story, open it from its event in Logic.
Choose **Clear scene content** to keep the supported background setup and exit.
**Undo** restores the original performance. Use **Add step** to add dialogue,
actors or movement before the exit. Use the timeline and **Play** to check
who is on stage when the speaker's line opens. A scene with an unresolved
exit explains why clearing is unavailable. Individual actions remain editable.

A spoken line waits for its window to close before the next action can run.
Earlier saved projects with a generated line immediately before release gain
that wait when opened. If you have only an older exported ROM, edit the final
line in **Step details** and export it again. This fixes the case where the line
stayed on screen as the game entered the name keyboard.

To give a scene its own identity, choose **New scene**, select a supported
setup and name the scene. Choose its background, place the cast and add their
actions. To make an answer matter on a later visit, add a two-answer choice
and put **Set story flag** inside the relevant answer. Give the flag a name
you will recognize in Logic. Use **Add event trigger** to choose the event
and target. A later rule can check that same flag and play another scene.
In a rule with nested conditions, each scene step has its own **Edit cutscene…**
button. It opens that occurrence with its launch context.

Save the project to retain names and editable rules. Export the ROM and enter
the event in game to check its timing, sound and campaign consequences.
The desktop preview is silent. When a sound envelope needs a starting volume
from game memory, playback uses a compatible capture when available. Otherwise
it identifies the missing volume and continues the dialogue and action.

## Scene timeline workspace (November 14)

The default Cutscenes workspace has an event-row filter and named scene list on
the left, the scene and independent key preview in the center, a bank/key grid
on the right, and actor/sprite tracks below. The filter reads both original event
launches and scene actions inside authored trees. New scenes switch to All until
they have a launch. Add event trigger opens the existing event logic editor with
the selected scene already assigned; its WHEN, target and IF conditions use the
same event compiler as Storyline.

Select an Actor or Sprite key, then click the scene to create it. Place drags
change position at the playhead; Move drags add movement with an editable duration
in native updates and an optional completion wait. Empty-space dragging pans the
preview. The key grid auditions independently and supports replay and looping.
Tiles show the number of distinct artwork frames reached by the native pose
player, including linked states, and mark looping animations. This is an artwork
count, not a duration in game updates. Context-dependent sequences whose count
cannot be resolved say so explicitly. The scene view retains a 640x480 image;
smaller workspaces scroll instead of compressing the canvas or overlaying controls.
The Appearance selector exposes the sprite children stored in each bank, with
thumbnails. Some banks intentionally have blank child zero; the picker initially
selects a visible appearance and labels empty appearances/keys explicitly. The
same chosen child reaches audition, placement and Play key on selected object.
For example, the original Hugo assignment scene uses bank3 / appearance6 for
Dio. Sprite preview opacity uses its native color bytes; Actor preview uses its
separate material-opacity bytes. Neither mode treats the Sprite material controls
as opacity.
Play key on selected object changes its animation; changing a sprite key recreates
that sprite at its current position and ends its previous authored movement.

Timeline clips use observed native command times. Simultaneous commands occupy
separate lanes. Pending commands are explicitly marked: their time has not been
established through the current waits or choices. Click a reached clip to seek,
double-click to edit its fields in Step details. Add step can insert at the
playhead, before/after a step, or into either choice branch. The timeline is a
projection of the native program, not a second saved script. Raw commands and
inspectors remain available behind their toggle.

Dialogue and two-option choices use the existing native dialogue compiler.
Set story flag supports OFF/ON/toggle and named project flags, including creating
one directly in Step details. Wait for story flag uses the same saved flag IDs.
Choice branches can set different flags and contain their own scene actions.
Project undo covers flag metadata and scene operands together.

New flag commands and explicit sprite trajectories opt into a generated
adaptation of accepted `func_00284288`. The generator authenticates the accepted
source and original ROM owner; accepted sources are not edited. Tagged 8E/8F
commands call the existing saved-flag services or the sprite trajectory helper.
Untagged instructions preserve their original behavior. Sprite trajectories add
position deltas after the original pose sweep and cancel when the slot is replaced
or released or the scene exits. Exact (-1,-1) sprite placement allocates at zero
then places explicitly, avoiding the native creation sentinel.

The runtime and its motion records occupy bounded permanent class storage.
`LCSCLOG1` owns the original dispatcher prologue, relocated-code checksum and pose
sweep entry. Export authenticates existing hooks before replacing them and updates
the ROM checksum. An unedited original ROM adds no hook and remains byte-identical.

Verification lives in `scene_workspace_tests`, the existing scene-action and
Director checks, and the parent task note. A copied battery-save run entered an
authored Mulsuk scene through town arrival, observed movement and both saved flag
writes, then returned to the map. That run required one observed liberation-dialog
confirmation. An independently authored Zemio courier was also played through
both answers in Project64. Accepting the dispatch set the named flag and played
the delivery line on the next visit. Leaving it played the line about keeping
the dispatch. Drawing approximations are identified in the expanded preview
context.

The Commands page edits complete Director programs in the project. It supports
operand changes, insertion, removal, restore and undo. Valid values enter the
project immediately; invalid input blocks navigation and save/export. Signed
decimal and hexadecimal words preserve the native bit pattern. Actor position
labels follow SceneActors' qualified conversion from milliunits; camera labels
come from the existing C-derived grammar. The grammar generator owns those labels.

Edits belong to physical resources, so selector aliases remain shared. Framing
uses the maintained-C-derived command widths. The terminal command and resource
class remain intact. Unknown operand values and untouched commands are preserved;
this is a source-command editor, not a complete semantic validator for every game
service. Export checks changed static scene/continuation selectors, dialogue entry
references (including project additions), labels using the accepted C word-wise
scanner, and class-4/5 background groups and member bounds. Missing targets reject
with their scene key and command word. Existing unresolved references remain intact;
launch substitutions still require their native execution context. The reference
check and public export regression pass together (3,449 ms).
Background resolution and dialogue navigation consume the edited program.

Project manifests store changed programs as exact big-endian bytes. Export compiles
them with the bundled N64 toolchain, compresses LZSS resources and updates original
Director/continuation references. The 96-byte LC05SCPT descriptor at z64 0x27C4700
owns dynamically allocated space above 40 MiB, bounded by the 64 MiB cartridge
limit. Each member records its exact directory pointer words, including shared
aliases. Parent event references are included if they share the same resource.
Header/payload CRCs, nonoverlap and reference checks precede reuse. Unknown bytes
after each original compressed stream survive. Growth around foreign data appends
elsewhere and clears only the previous owned allocation. Ordinary command edits
add no runtime hook; the optional authored flag/sprite extensions are described above.

The engine check preserves all 1,498 original programs, compares an operand edit
against an exact expected byte change, checks insertion/removal/restore and invalid
terminal rejection, saves/reopens a project, and compiles/exports/reopens a ROM.
It also checks checksum stability, repeated rebuild identity, trailing bytes,
growth, foreign data, damaged ownership and shared aliases. The extended check
retains a continuation while a different member grows (2,705 ms). Candidate output
is under the parent's Test ROMs directory; the master ROM is read-only.

The UI check exercises decimal/hex edits, undo/redo, multiple invalid fields,
insert/remove, restore, navigation blocking and actual background resource
selection after a group edit (427 ms focused; 2,183 ms with source-navigation and
main-window dialogue guard regressions). The command-page screenshot was inspected.
These checks establish editor/rebuild behavior, not full-scene gameplay.
Legacy browser-ROM arena migration and final scene playback remain active work. The dynamic scene at z64 payload 0x01FA8BF4 has no local camera command, so
camera editing coverage uses a class-five stream that defines its own camera.


## New scenes

The UI creates blank room or qualified fresh environment scenes from current-ROM
setup templates, duplicates complete scenes, renames them and removes authored
scenes with undo. Room templates retain their group, both cameras and native exit;
environment templates retain their explicit background request and release exit.
Inherited scenery and unresolved handoffs are not offered as blank templates.
Users edit subsequent commands or add supported actions in the Actions page.

Create class Actor adds allocation and body selection together in fresh environment
scenes. Its slot, independent class pair, pose and placement edit as one action;
removal and ordering also keep both commands together. Preview equipment comes from
the class defaults, as stated in the preview context. Army member bindings and
event-selected cast still need their own launch data.

In Scene playback, click a visible Actor to select its live slot and pause, or
double-click to open its action. Selection follows the rendered layers, including
transparent holes and foreground occlusion. Dragging an object places or moves it;
dragging empty space pans. The outline shows the
selected projected extent; it is an editor overlay and is not part of the scene.
Typed SceneId values distinguish native resource keys from authored UUIDs. No
fabricated ROM key is assigned to a scene before it has an exported allocation.

Each new scene takes an empty, unclaimed top-level selector. The generated retail
catalog reserves 1,548 selectors; the vanilla directory has 145 available rows.
Clearing a retail pointer in another tool does not make that row unclaimed. UUID,
name, selector and source/current program survive project save and undo. Scene
references may target additions in the same project. Native references to an
exported authored resource resolve to that same authored identity on reopening.

LC05SCPT version two adds an owned UTF-8 JSON identity manifest between the pointer
lists and compressed members. Header words at offsets 28/32 identify its relative
offset/length. Version-one allocations remain readable and migrate on rebuild.
The manifest is covered by the payload CRC, and every authored scene exclusively
owns its recorded selector. Programs compile during export; both native and new
resources receive decoded readback. Retail names survive relocation through their
stable catalog selectors. Exported authored scenes can also be removed once their
static scene transfers and event launches have been retargeted or restored.

Creation/clone/rename, save/reopen, export/reopen, ownership collision and subsequent
edit checks passed with existing export/reference checks (9,114 ms). The explicit
version-one migration and authored export checks passed together (3,739 ms). UI
creation, duplication, rename, removal, command editing and undo passed with the
catalog/command regressions (2,230 ms); the new-scene screenshot was inspected.
Final checks for relocated retail names (2,736 ms) and tab-switch invalid-input
protection (412 ms) pass. No in-game launch is claimed by these checks.

## Event launches

The Event launches page edits the 1,998 catalogued game launch sites, including
402 short and 1,596 long forms. Search covers launch identity, scene, mission and
role; shared sites are identified. Target selection, restore, undo and project
persistence use actual scene selectors, including newly authored identities.
A bound scene cannot be removed before its launch is retargeted or restored.

The metadata-only catalog is generated by tools/generate-scene-events.cjs from
the preserved browser catalog and audited row hashes. Program bytes come from
the user's ROM. Original row preimages must match their pinned size and SHA256.
Long forms update the two selector bytes. Short forms retain their original
instructions for targets through 255; higher targets append the audited ten-byte
launch/return stub. Sixteen-bit word cursors bound growth. Full-program comparison
recognizes both this form and earlier browser exports that always used stubs;
unrecognized event edits reject instead of being overwritten.

Export compiles rebuilt event programs through the same N64 data compiler and
shares the scene allocation. LC05SCPT version three stores a JSON object with
`authored` and `eventBindings` at the existing metadata offsets. Versions one and
two remain readable. Each event member exclusively owns its original directory
word at z64 0x9398EC + row*4. Metadata bindings, decoded programs and live targets
must agree. Removing a row's last binding restores its original resource key;
foreign allocations stay intact. An empty owned allocation is cleared and, when
it is the ROM tail, removed. Original game loaders need no additional hooks.

`event_tests` compares all 1,998 high-target rewrites against the independent browser
exporter and verifies both low-target layouts, discovery and unrelated-byte
rejection. Its real-ROM workflow creates a scene, binds short/long launches,
saves/reopens, compiles/exports/reopens, restores launches, preserves foreign
bytes and removes an empty tail allocation (combined 6,421 ms). Existing authored,
native-script and version-one migration regressions pass (6,847 ms). Desktop
assignment, restore, filtering, invalid target, scene rename and undo checks pass
with scene-editing regressions (1,294 ms). Final target validation/export and
undo-history guard checks pass (3,357 ms and 332 ms). The rendered event page was
inspected. These checks do not constitute in-game launch or frame validation.

## Removing authored scenes

Removal checks all remaining primary and continuation programs for static
0x80000003 transfers, as well as event bindings. A failed operation reports the
remaining reference and leaves the model and undo history intact. Retail scene
identities are preserved; removal applies only to authored UUIDs. Deleted native
keys retain their original UUID mapping inside the project so they cannot reappear
as unnamed retail scenes. Saved manifests record the remaining authored set.

Project loading applies scene changes and event changes together before validating
the resulting references. Export independently checks the compiled scene set,
clears each removed selector, drops its owned member and repacks the retained
members. Released selectors become available when the exported ROM is reopened.
Deleting the last owned members clears the descriptor and frees only that owned
allocation; foreign data prevents tail truncation and is preserved.

The linked-scene test verifies static/event reference rejection, malformed saved
removal rejection, combined unbind/delete persistence, selector release, compiled
export/reopen, foreign bytes and exact return to the source ROM when the allocation
is the tail. It passed with binding export regressions (8,433 ms); the final focused
run passed in 7,918 ms. Desktop removal of an exported scene, undo/redo, event
assignment and scene-creation regressions passed in 1,385 ms. Existing authored
export checks passed in 6,855 ms. Dynamic runtime substitutions remain contextual;
these checks do not establish full-scene game playback.

## Browser project conversion

Standalone Cutscene projects and the `patches.cutscenes` section now convert scene
commands, authored UUIDs, deletions, event bindings and wiped scenes into native
models. Current ROM selector links resolve retail identities. Exact command words
and boundaries must match the imported document's source; conflicting aliases or
existing native edits reject. Dialogue resources are imported by the native
archive model. Native snapshots and a conversion marker preserve later edits and
restores without replaying the retained browser document on the next open.

The compatibility compiler embeds the existing model, Director and codec scripts
in Qt 6.12 QJSEngine. User documents enter as JSON data, with source boundaries
checked in C++; returned bytes are decoded again and must keep their terminal
class. This is an import adapter. Native execution/rendering and C/ASM export
compilation continue through their existing native paths. The metadata-only import
catalog is generated by tools/generate-scene-import-catalog.cjs and retains the
57 retail scenes' explicit wait groups. It contains no game program payloads.

The byte comparison covers an imported dialogue/choice program and all 57 explicit
wait-group scenes (6,802 ms). The first wrapper failure exposed a missing source
word count; a separate fixture failure came from optional browser dialogue text
enrichment, which the wait-only comparison does not require. Source mismatches,
missing boundaries and invalid documents reject; a failed conversion does not
poison the next document. Full project save/export/reopen matches expected scene
and dialogue bytes (6,637 ms); prior dialogue import checks pass (5,410 ms).

The native clear-content rule agrees with the browser on all 1,498 retail scenes:
1,472 supported wipes and 26 refusals. It retains leading background/media records,
the class-4/5 camera pair and the supported release/handoff ending. Parent-event
relaunch endings remain refused. Authored-scene creation plus a launch binding and
three wiped-scene imports save, export and reopen correctly (8,650 ms). The desktop
Clear scene content button supports undo and guards invalid command input (746 ms
with command regressions). Unfinished command drafts now survive unrelated event
undo until corrected or explicitly discarded (488 ms with the wipe regression).

Qt Qml's matching 6.12.0 binary/source archives were checked against official Qt
metadata and digests. Packages include its sources and notices. Deployment skips
QML debugging and Qt Quick imports; the runtime adds Qml and Network DLLs to the
existing Widgets application. The isolated package check passed startup, C export,
record readback and source preservation. A separate actual portable browser-project
import/export matched all 2,648 expected program bytes using the browser decoder,
with development PATH/QT/QML variables removed. That check found and corrected the
CLI's dispatch of standalone browser project formats. It is not a clean-machine
or full-game test. Existing browser-ROM authored arena identities still need
migration support; retail and native-rebuilt project sources are covered here.

## Browser-exported ROM migration

The native loader reads the browser OBCS arena at z64 0x2780000..0x27B0000.
It verifies descriptor bounds, record kinds, nonoverlap, authored/event exclusive
selector ownership, script digest prefixes, class-5 setup/exit and manifest
identities. Event bindings must match the actual decoded event programs. Unknown
foreign arena data is not claimed. Browser projects made from these ROMs retain
the same authored UUID and baseline when imported.

The next scene/event export migrates authored programs and event rows together.
LC05SCPT version four adds `legacyArenaRetired` to the owned JSON metadata; versions
one through three remain readable. All bytes of the browser arena, including its
manifest and any still-referenced retail resources, remain intact. The marker
prevents old authored records from being rediscovered after migration. When the
last migrated scene/event is deleted, a small metadata-only allocation retains
that marker. Ordinary native allocations still clear completely when emptied.
A no-edit rebuild leaves the original browser ROM unchanged.

`scene_import_tests browserRomMigration` passes (9,765 ms) using a fixture made by
the existing browser `prepare/apply/validateApplied` exporter. It checks project
import, edit/save/export/reopen, stable names/UUIDs and event launches, deletion
both before and after migration, unchanged old-arena bytes, retained foreign tail
data, and rejection of duplicate ownership, altered digest and damaged manifest.
The test exports to a separate candidate path and reopens it; it does not launch
the game. Existing native allocation upgrade, authored/export and event/deletion
checks pass (5,601 ms and 7,693 ms respectively).

Correction to the earlier migration research note: allocation record word six is
not always zero. The browser exporter fills it with the first 32 bits of the
script's decoded SHA-256 after encoding; plain manifest records keep zero.

## Visiting units, Hugo's Report and titles

For a scene that takes a visiting unit, open a save in **Save**, select the
slot you want, then choose its unit in **Cutscenes → Scene playback**. The
preview reads the members, formation, equipment, gender and alignment. The
sample visitor remains available when you want to try the scene without a
save. For the observed Zemio cast, use the [recorded launch](decomp-intake.md#november-17-actor-placement-and-briefing).

**Hugo's Report** is selector 0. On the timeline, **Replace Army help text**
edits the destination panel and the help section. The opening command uses
panel 1. Section 0 introduces the menu, and section 1 begins “This is the
past”. The categories and clock can use the open save. For a full report
page, open a 4 or 8 MiB native-order RAM dump taken while this report was
active. The preview starts the selected script with the captured report
records. Use **Up**, **Down**, **Confirm (A)** and **Cancel (B)** to navigate.
The preview reads the crest, Hugo portrait, papyrus, borders, menu tabs and
pointer from the open ROM. It draws the current lines with the game's bitmap
fonts, including the stacked labels in Tips. Project64 references cover the
opening, Tips list and both pages of Critical Attacks. Desktop texture sampling
can produce small pixel differences. People relationship diagrams and other
report histories still need visual verification.

For the shared chapter script, use the **chapter** picker to choose the
Prologue through Final Chapter. The sparkle pattern uses a repeatable
preview seed. The mission-entry script takes a scenario title image and a
mission number. These preview settings let you inspect the different launches
of the same script. The exported game supplies them from its campaign.
