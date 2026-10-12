# Feature inventory and completion tracking

Review project changes includes a searchable entry list with before/after details
and Open selected edit navigation. Record links preserve identity through sorted
tables; town reports show name, population, morale, facility and shop values.
The comparison is explicitly against the loaded source ROM.
The project toolbar's ? button finds known uses of a selected class, item, shop,
Art Library asset or shared combat sprite and opens the consuming editor. Searches
include class fields, inventory, squads, encounters, treasures and art assignments;
story-computed IDs and untracked independent artwork copies are not live references.
Export now opens a summary before the output picker, highlights missing references
with edit links, and refuses to continue while blocking problems remain. The None
class's sentinel values are excluded. Compilation retains the existing final bounds,
resource and checksum checks.

Art and Animation → UI and Icons groups Item icons and Class category icons.
The category editor reads all eight original List Classes symbols from the ROM.
Create or copy an icon, paint pixels, replace it from an image, or export a PNG;
colors use the game's existing palette. Class family editing shows an icon
picker and Edit/Create shortcuts. Assigning an icon gives that family its own
in-game List Classes tab, including linked families. Unassigned families keep
the original category rules. Tabs appear when their classes are discovered and
page eight at a time with the game's left/right navigation. Undo/Redo, projects
and ROM export preserve the art and assignments. See
[class category icons](class-category-icons.md) for the export contract and limits.

Selecting a squad on the Map shows a gold terrain-aware planned route and its
movement type in the sidebar. It uses the accepted game pathfinder, current class
movement settings, movement groups and the same terrain grid used by export.
Terrain, boundary, squad and order edits refresh it. Dashed lines still show order
connections. Unreachable or unresolved destinations explain why no route appears.
The preview follows fixed starting orders up to a scripted condition; live enemy
reactions, follower offsets and the game's fine turn smoothing are not simulated.
See [terrain route preview](terrain-route-preview.md).

Scenario section navigation sits directly below the main workspaces, matching
Encounters and Art. Its shared scenario picker includes the search field and
retains selection across Map, Squads and Routes. Map opens first. Base-level controls live
inside Squads; map-to-field navigation also updates the selected section tab.

Site inspector fields apply directly to the project with Undo/Redo. Checkboxes
and choices update immediately; typed numbers commit on Enter or leaving the
field. Checking Shop creates stocked inventory when needed, or reuses that site's
inventory after toggling it off. Unchecking Shop or choosing Shop ID None removes
the facility; another Shop ID attaches existing inventory. Shop and Witch's den
replace each other. The separate Apply, create-shop and remove-shop buttons are
gone. Treasure item/type and coordinate fields also apply automatically, retain
their collection flag, and support Undo; changing item type selects its first
usable item in the same Undo action. ROM export is still explicit.

Map renders the ROM's actual battlefield geometry, textures, roads and buildings
in a top-down daylight overview. Town allegiance edits and Undo refresh building
colours. Terrain and road brushes update native surface geometry plus movement
and encounter terrain; highlands and forest raise the surface. Every brush rebuilds
the affected terrain borders. Painting through native forest or highlands cuts
away the replaced surface and updates movement heights; matching extensions share
one outline. Rebuilt landforms use approximate slopes and texture mapping. Painting
outside the playable edge changes scenery; expanding the visible, draggable
boundary opens its movement cells and cursor limits. Fort Romulus uses a separate
scripted boundary and cannot yet be resized. Ordinary towns and standalone
strongholds can be dragged with their complete geometry and gameplay position.
Undo/Redo, restore, project persistence and ROM export share these edits. Fixed
fortified interiors remain outside independent building movement. Export regenerates
the edited battlefield's briefing image, including moved buildings. The minimap
reads the edited terrain grid directly. Road textures follow bends and repeat the
continuous strip body; brushes prefer the current map's materials and preserve its fog. See
[battlefield maps](battlefield-maps.md) for remaining rendering approximations.
Brush radius supports half steps, including 1.5. Bridges form straight spans from
the ROM's bridge mesh, retaining deck, rails, support faces and atlas mapping;
the movement-height grid follows the deck.

Mountain peaks mode adds selectable peak markers. Drag a marker's base to move
the peak across the map; drag its diamond to change height, or enter a height in
the sidebar. No drag-mode selector is needed. Add peak on map places a
summit inside Highlands; Remove peak deletes the selected summit. Restore all
peaks keeps terrain painting and town edits. Release redraws slopes; Escape
cancels. Undo/Redo, saved projects and native geometry/movement-height export
share the same layout. The straight-down preview shows height through shading;
changing summit height/position leaves the terrain's painted outline in place.
Peaks in the same mountain automatically join with raised ridges and shallow
saddles. Connections follow the mountain's interior, recalculate after peak or
terrain edits, and stop at cuts that separate mountains. Rocky texture follows
the crest; exported geometry and movement heights share the connected profile.
Low points between higher peaks form saddles. Their rock texture darkens with
their height relative to the adjoining summits, instead of showing a bright
summit stripe through a nearly flat pass.

Map's separate City pieces dropdown adds 19 current-ROM houses, towers, keeps,
gates and wall pieces from keys 39/47/51, plus cobblestone paving and three road
brushes. Click placement, dragging, rotation, removal, Undo, projects and native
ROM export share the map editor. City pieces are static scenery; they do not
create town records, movement blocking or gate scripts. Paving/roads use Highway
gameplay terrain and rebuild affected terrain borders. Briefing regeneration
includes the placed pieces.

Scenario squads and custom Encounter squads share the same member editor:
individual class/level/equipment rows, member count, and drag/swap formation board
with compatible gear choices and large-unit checks. Scenario squads support one
through five members, including five distinct classes. New, Duplicate and Restore
loaded squad accompany Apply and Undo; deployment/orders/supplies/drops remain in
their own details tab. The source-built scenario member constructor reads appended
individual specifications when a composition exceeds the stock follower cohorts.
Original Soldier groups retain their cohort format.

Source reference: the browser Editor preserved at `559b231`, including its
README, interface, parsers, exports, and tests. Preserve behavior and data
compatibility while redesigning implementation and interaction for native use.
An entry is complete only after its editing, persistence, export, and relevant
preview behavior work; a visible tab alone does not count.

Scenario maps use circular squad-leader portraits and treasure-item icons from
the current ROM/artwork edits. Enemy/allied portrait selection falls back to the
other portrait slot when absent. Colored rings retain allegiance and selection;
icons draw above sites/waypoints. Icons also identify entries in the object list,
refresh with record edits and Undo, and preserve the grab offset during dragging.
Selecting a site or treasure opens its editor in the scrolling Map sidebar;
clicking its visible map label also selects it, including labels shifted around
nearby objects. Labels select without dragging; marker drags still move objects.
There is no separate Sites and treasure page or second object picker. Site
metadata/facilities/shops/allegiance and treasure items/coordinates apply
automatically with Undo; Restore and treasure removal remain explicit actions.
Squad selections show members and levels;
waypoints show route context, with links to their full editors. Add treasure on
map opens the new treasure's inspector immediately.

Town facilities follow the native menu's single facility entry: choosing Shop
clears Witch's den, and vice versa. Existing loaded metadata remains intact until
edited. New town shops start with the seven ordinary consumables; Shop ID zero
cannot be applied as a new shop. The inspector marks unapplied facility choices.
Apply site changes and export before testing. Fresh mission entry loads the ROM's
facilities; a suspended mission retains its previous facility flags.

Map encounters combines terrain names, per-terrain chances and the selected
map's two creature choices. Terrains sharing a pool occupy one row; pool IDs are
secondary labels. Rows explain missing assignments, zero terrain chances and
empty creature pairs, and identify active custom-squad overrides. Edit rules
opens the corresponding row under Terrain rules · All maps. That page previews
the selected map's creatures on assigned/available pool tiles while retaining
game-wide assignments, sharing/move/swap dragging and pool expansion.

Description editing starts with the selected record: Classes, Items and
Consumables show a preview and Edit description button in their inspector.
Consumables always shows all table columns and a fixed Effects inspector, without
column-group or inspector-section selectors.
Class attack fields and combat/advanced action selectors open the selected
action's shared description. Dialogs validate drafts before Save, support Cancel
and restoring the original, and commit one Undo step. New class IDs use their
expanded description indices. There is no standalone Descriptions tab;
description changes still use the same project text banks and Undo history.

The editor now also bundles the complete accepted pure-C source library, with
per-source provenance and dependencies. Imported source availability is distinct
from active use; existing animation and gameplay adaptation baselines remain
independent. This source import introduces no automatic ROM patches.

## Accepted C integration — 2026-10-09

The complete accepted pursuit/save batch is bundled as maintained original source:
33 functions in 29 owners, with required headers and included sources. Save-file
editing and ROM-side save exports now use the accepted-derived checksum kernels,
retaining expanded records, Controller Pak conversion, Undo and unknown data.
The pursuit source is imported for future behavior features; Routes currently
continues to edit/summarize authored orders. The function list distinguishes
those imported functions from active adaptations and detects included-source
updates in later acceptances.

## Accepted C integration — 2026-10-08

Save's Add reserve character and Recruit squad dialogs now accept levels 1–99,
using accepted-derived initialization and growth with the current class/item
tables. Existing atomic Undo, save export/reopening and expanded class IDs remain.
Shop exports compile the full accepted-derived stock function in its original
space, retaining expanded stock and the empty-category guard. Earlier native
shop descriptors remain readable and upgrade on export. Sources are bundled;
users need no Decomp checkout. See `game/README.md` for adaptation boundaries.

## Current checkpoint — 2026-10-07

Combat frame editing shows a numbered thumbnail for every authored frame, including
repeated artwork. The grid wraps and scrolls beside a large preview, beneath the
existing class/action/side/mode/variant assignment controls. Click a card to inspect
it; double-click to paint. Drag one card or use Earlier/Later to reorder. Ctrl/Shift
selects multiple frames for timing, duplicate/delete and artwork positioning.
Drag the artwork in the preview to position the selected frames together. Each
drag is one Undo step; Escape cancels it. The background or middle mouse button
pans, and the wheel zooms. The previous-frame overlay stays fixed while dragging.
Duration changes apply immediately; the previous-frame overlay helps with alignment.
Delete and Ctrl+D work while the frame grid has focus. Playback speed is adjustable;
a green outline follows the playing frame without changing the editing selection.

A fixed ground line and origin cross keep standing feet aligned between frames.
The preview defaults to moving Everything; choose a thumbnail-labelled body or
equipment layer to position it independently. Other layers fade for clarity and
stay fixed. The layer number applies across the selected frames.

The first frame edit creates an independent copy in the same Undo action. Canceling
painting or import does not leave an unused copy. Paint, Layers, Add frame and Copy
artwork are beside the frame tools; Animation tools holds whole-animation copying,
comparison, Art Library transfer and restoring the side/variant's animation set.
Thumbnails and the static preview use common sprite bounds. Playback follows the
original movement, waits and loops; thumbnails show authored order, and sound is
not played. The detailed instruction editor remains under Advanced animations.

Combat animations now provides class, assigned action, player/enemy side, mode,
artwork variant and animation selectors, an explicit Assign button, playback,
frame timing/order/layers and per-frame sprite painting. Existing detailed editors
remain under Advanced animations. Normal assignments retain the existing modes0/1
patch semantics; Blocked is mode2. Linked fixed motions copy both component
programs. Preview selection does not modify the project, and incomplete class
searches disable editing instead of leaving the prior class active.
The animation choices contain combat selector targets and the fixed combat
motions, including private attack assignments; unrelated bank poses are excluded.
Action descriptions are edited from Classes rather than this animation page.

Army artwork shows copied/new classes individually and separates both facings on
first edit. Legacy boss labels are restored without changing ROM names. Save's
front/rear display is corrected; the enemy boards already used front cells7–9.
The live development folder is native/live; package.ps1 -Live updates it without
a ZIP and refuses to replace an open live executable.

Focused class-name/Save, Army isolation/shared editing, combat artwork, guided
animation/linked-motion/pixel/Undo, existing sequence editing and full desktop
workflows pass. Real-ROM Army and animation exports pass allocation/reference/
checksum validation, project persistence and reopening. The live folder passes
isolated startup and bundled compile/export/reopen. Captures were inspected.
No new emulator session or complete in-game combat acceptance is claimed.

Boot/runtime correction: exported class relocation now preserves the original
BSS clear and protects persistent data during the lower title/new-game heap reset.
Old native exports are repairable by re-exporting. The reported black screen was
reproduced; a one-word write watch then identified image decompression overwriting
class data after the earlier partial repair. Focused entry-point execution and
normal/title heap checks now pass, as do original-export repair and source rebuild.
Project64 cold boot now passes the new-game ceremony into the throne room, with
correct portraits and the entire persistent data allocation unchanged. Later gameplay
and custom encounters are not covered by this focused boot/cutscene check.


Encounter authoring update: Copy Class replaces the longer copy label. Advanced
fields distinguish name pointers, unknown class bytes, combat LCK and partially
known anti-dragon item flags. Global context labels are corrected; mean waits show
0–10 eligible moving squads per terrain with an explicit cadence assumption.
Rewards use the shop's ROM-icon modal. Fixed terrain rows accept draggable Pool
1…32 tiles; the palette shares assignments, row dragging moves/swaps them, and
pool/custom-squad labels follow the current mapping. Add pool expands all scenario
groups and compiles/reopens through the maintained selector. New pools are empty.
Custom squad additions no longer reuse occupied cells, gear choices are filtered,
portraits follow dragging, and small windows scroll instead of overlapping cells.

Focused workflows pass for these controls, Undo, import and formation/layout.
Eleven pools without custom squads and 32 pools with a last-pool squad pass project
folder/archive, compilation, allocation/checksum validation, export, reopening
and repeat export. Exported selector checks pass both pair sides and squad capacity
refusal. Existing custom-core checks pass. Portable 20261007-164441 passes isolated
startup/export/reopen and bundled recompilation of the 32-pool candidate. No new
full-game validation or measured encounter polling cadence is claimed.



Authoring refinements: Promotion supports explicit bidirectional family links;
linked groups share manual-change destinations while masks, behavior, discovery,
stats and equipment still apply. Automatic death/attack transformations remain
separate and are explained in the panel. Items has New item (blank accessory)
as well as copy. Shop stock filters use the first ROM icon in each type; the
stronghold picker lists every stronghold by scenario and follows the selected
shop. Refreshing lists retains view positions; unfocused numeric/choice controls
ignore wheel edits. Custom rewards are edited independently under leader-based
squad names, with numbering for repeated leaders. Both squad editors support five
individual members; Scenario export extends the original follower-cohort format
when needed. Talk affinity has an explicit heading for neutral encounters.

The Map tab places new treasure with one click and one Undo step. Routes has a
squad/start selector and clickable behavior summary above the detailed node,
condition and group editors. Town objective/type fields explain their scope;
Treasure/Mine capability flags are marked advanced because their full gameplay
effects are unverified. They do not author hidden map treasure records.

Focused UI checks pass for creation/Undo, filters/assignment/scrolling, independent
squad rewards, routing and treasure placement/export/reopen. Existing item and
family export checks pass with blank items and linked families; the 256-class
family-data check passes. The existing five-distinct-class custom-constructor
and insufficient-capacity checks were reused. No full-game boot is claimed.
Portable: native/dist/LordlyCaliber-0.5-dev-20261007-102207.zip. Its isolated startup,
user-time compilation, export/reopen and source-preservation check passes.


The Items workspace now creates equipment IDs through 510, with independent
names, descriptions, icons and weapon appearance entries. Project folders,
archives, Undo, repeated export/reopen and original save-codec checks pass.
Shops now grow through ID 255 and attach/detach through Shops or Scenario Sites;
the stock picker shows ROM/project icons in a modal. Site facility bit 1 is labeled
Witch’s den. Terrain IDs include known names; global branches use In Mission and
In Area investigation. Reward classes used by pools/custom squads sort first.
Custom formations use square ROM portraits, drag movement/swapping and Front
at the bottom; Enabled and configured Talk rules have explicit labels/help.


Inherited scene launch is now implemented: current event bindings choose the
predecessor chain, shared Actors/dialogue/scenery survive entry, and ending callers
select the branch leading to the chosen resource. World, room and both ending
paths complete and reproduce their frames when seeking backward. Event assignment
and Undo/Redo refresh the launch context. Room scale, explicit Actor height and
Stage prop program resets are connected to native execution. Earlier milestone
notes below retain their historical limits; these launch gaps are now resolved.

Joe deferred revision 1 on 2026-10-06 and accepted the existing editor map
alignment for now. Neither is a blocker for completing the current revision 0
rewrite. The subsequent code audit found additional revision 0 gaps: complete
animation sequence import/replacement and comparison/batch movement; cutscene
playhead-linked authoring, named template replacements and playback-input import.
The earlier revision-1-only blocker report was incorrect. Implement these useful
workflows rather than broadening map calibration or validation infrastructure.

Complete Sprite studio sequence import/replacement now prepares every frame with
shared sizing/placement and color reduction, retaining durations and destination
control instructions. It creates a private copy when needed and commits one Undo
step. Cancellation or rejected instruction counts leave the project untouched.
Focused desktop/project/source-export/reopen check passes (2.3s), including exact
two-frame colors and placement; pose alias/control-loop check passes (7ms).
Evidence: `build/Release/sequence-batch-{ui,controls}.txt`.

Class-to-class pose copying now transfers independent layers/sprites, equipment
selection policies and optionally source control/dependent poses. Copy art and
Copy art and duration operate on one chosen frame. Side-by-side comparison uses
a common origin, scale and clock; native pose preview now exposes automatic fit
with stable origin as default. Batch translation covers selected frame rows or
checked poses, all layers or one layer, with before/after preview and one Undo.
`sequence_editor_tests copyCompareAndTranslate` passes (7.4s): two equipment
choices, dependent-pose playback, source/unselected-frame preservation, Undo,
copy-duration distinction, common origin and compiled export/reopen. Screenshot
`build/Release/sequence-comparison.png` inspected. The cutscene gaps remain.

Use as template now names an existing scene replacement without allocating a new
selector. Its actors/actions and existing game triggers remain attached to the
same resource. Names persist in the Project, import from browser friendlyName,
appear in preview/catalog and support Undo/Redo. Project names are not inserted
into game script bytes. Existing scene export checks retain all selector aliases
and pass (2.4s); creation/rename desktop checks pass (0.9s); browser authored/wiped
import including replacement names passes (8.4s). Logs:
`build/Release/scene-template-{engine,ui,import}.txt`.

Action selection now seeks to the actual reached command, with update/second
labels and an At playhead marker. The PC session's optional observer leaves the
recovered C dispatcher unchanged. Preview traces exclude other resources and
mode-skipped instructions, survive seeking, invalidate future times on input
branching, and respect inherited-entry time zero. Unreached actions report that
limit. `sceneActionExecutionTimeline` passes the five-update wait/next-action at6,
backward seeking, stale-scene rejection and unchanged project; existing
`storyCompleteReplay` completes1349 updates with exact seek samples (17.5s total).
Dispatcher snapshot/continuation checks pass (0.33s). Logs:
`build/Release/scene-action-timing-replay.txt`, `scene-observer-director.txt`.
Add at playhead and Actor dragging now insert native actions at reached boundaries
or split a timed wait while retaining its later deadline. Selected movement actions
update their destination instead. Drag previews preserve grab point/height, Escape
cancels, Control-drag pans, and a placement commits one Undo. The special native
(-1,-1) retain sentinel is avoided for deliberate placement at those coordinates.
Executed exits report pending movement/fade/pan; Hold until finished inserts native
completion queries plus the observed remaining pan duration. Paused motion must be
resumed first. Condition/completion-wait interiors require selecting an executed
action boundary instead of guessing a new branch.
`ui_tests sceneTimedPlacementAndHold classActorCreationControls
sceneActionExecutionTimeline sceneCompletionControls` passes (8.0s). The new flow
checks drag cancellation, class identity/height, update3 placement, unchanged
update6 exit, Undo, warning/hold, project persistence, compiled export/reopen,
identical reopened playback, checksum and master preservation. The added sentinel
field-edit check also passes (2.4s with the same export flow). Logs:
`build/Release/scene-timed-authoring.txt`, `scene-timed-sentinel.txt`.
Playback-input import now accepts browser-schema Actor/input rows, unit members,
scheduler branch, static captures and captured cameras. Inputs remain transient,
are keyed by source asset identity across scene edits, and Clear restores ROM
startup. Invalid JSON, wrong identity, oversize files and unavailable artwork
retain the previous preview. Native movement/seek and unchanged Project/Undo
pass in `importedPlaybackInputs` (0.74s initially). Schema validation is extracted
at build time from the browser validator; Qt evaluates only that pure validator,
not the browser playback runtime. No browser, Node, or source checkout is required.
Callback histories, explicit fresh-launch service memory and captured resume
profiles currently stop with a named adapter boundary; these are still being
integrated. The existing timed edit/export and complete 01FA8BF4 replay pass with
input import added (23.4s combined); `build/Release/scene-input-integration.txt`.
Captured movement-query resumes now restore an actual command boundary and run
only their declared update window. Mode-two inputs check native owner/Actor/job
agreement and supply cameras/projection; current-ROM pose registries are checked
before use. Missing terrain/proximity inputs remain boundaries. Captured bodies
can render with explicitly labeled class-default equipment when army rows are
absent; linked sound still requires its owner when consumed. Synthetic one-update
resume passes (0.77s); existing Graduation capture passes its69-update limit,
observed Actor1 position and exact seek replay (0.81s). Its screenshot was inspected.
Logs: `scene-input-resume.txt`, `scene-graduation-input.txt` in build/Release.
Captured continuous playback now imports dialogue context/text speed and declared
controller/page policy. Graduation resumes eight Actors, advances its dialogue,
and releases at update697; neutral input keeps its dialogue awaiting input.
Held69-update and synthetic import/rollback checks also pass (11.3s combined).
Audio B4 now reads all four indexed gain entries, including the actual closing
fade's index3; original-instruction comparison passes (119ms). Automatic page
acknowledgement is scoped to dialogue and does not skip Director input waits.
Logs: build/Release/scene-capture-dialogue-2.txt and scene-audio-index.txt.
Fresh room-launch input now runs the native camera reset, supplied cast/tint,
controller history and scene services. The existing room fixture reaches its exit
at update3391, with exact backward seek and background omission. Native0x57 now
implements the controller-mask wait; A/B controls work outside dialogue. Imported
automatic choices produce the same two visible outcomes as manual selection.
Input direction/history remain distinct, and playback honors the declared input
window up to30000 updates. Input-owned cast/launch controls are disabled until Clear.
The integrated check passes eight product cases (10 QtTest entries,92.3s), including
timed edit/source-export/reopen,01FA8BF4 through1349 updates with exact replay, and
MainWindow startup. Log: build/Release/native-integrated-scenes.txt.
Specialized research captures with recorded callback outcomes or active inherited
dialogue payloads still name that unsupported context; normal native playback
computes those services directly. These are not required inputs for scene editing.
The final invalid-import context/controls rollback check passes (742ms). Room
controls stay inside the settled layout; its final full-route check passes
(46.5s), and the settled screenshot was inspected. Portable ZIP: native/dist/LordlyCaliber-0.5-dev-20261006-161230.zip, product
commit b1c788a. GUI and CLI rebuilt together. Isolated package startup,
bundled compilation/export, independent class readback, reopening and source
preservation pass (2.6s); browser scene conversion/export/reopen matches all2648
expected bytes (7.5s). Logs: build/native-final-portable-check.log and
build/native-final-portable-scenes.log. No clean-machine or full-game run is
claimed. Nothing was pushed or published.

Preview approximations remain explicit: sample party/terrain where gameplay
context is unavailable, inherited event timing and setup acknowledgement, clear
saved story flags, silent audio and desktop rasterization. Full-game emulation,
Hugo's interactive menus, live save injection and exact hardware pixels are not
acceptance requirements for the editor rewrite.

| Area | Required capabilities | Native status |
|---|---|---|
| Workspace | ROM byte orders/revisions, project folders/archive/import, undo, validation, changelog, safe export | Readable project change review now supports background generation, search, copy and text export relative to the loaded source ROM, with persistence and source-preservation checks. Remaining workspace audit in progress |
| Classes | Complete records, names/descriptions, growth, attacks, equipment, promotion requirements, family selection/creation, new class IDs, dynamic rebuild allocation | Record fields, names/descriptions, promotion, compatibility, equipment-list availability, families and class creation through ID 255 now persist, undo and export through coordinated source rebuilds. Dynamic allocations and reopened-ROM growth verified; full-game validation and remaining feature audit pending |
| Items | Equipment creation, names/stats/prices/resistances, packed growth, descriptions, preserved raw fields | New IDs 278–510, independent ROM artwork, dynamic record/name/description allocation, dependent pickers, Undo/project/save/export reopening implemented and checked. Original-ID scripted effects stay with their original IDs. |
| Shops | Original shops plus new shops through ID 255, stronghold assignment, equipment/consumable inventories, expanded capacity and reopen | Search, add/remove/reorder, copy/restore, undo, project persistence/import and source-built export implemented. Supports 50 equipment/30 consumables in the original RAM arrays. Export/reopen and instruction checks pass, including empty stock; full-game validation remains |
| Consumables | Values/descriptions, supported behavior/effect editing, rebuild ownership | 45 price/metadata records and ten magnitude/range models edit, undo, persist, import and compile/export. Healing description numbers can update with the effect. Shared stat boosters remain one model; unsupported item behaviors retain their source. Integrated export/reopen and instruction checks implemented; full-game validation remains |
| Encounters | Pools, terrain/scenario/global rates, custom squads, persuasion/retreat/rewards/card text | All 45 pools, 32 terrain rates/slots, global and independent scenario rates, weighted class rewards, and custom squads now edit, undo, persist, import and compile/export, including added class IDs. Squads support independent members/equipment, formation, card text, persuasion, retreat and rewards. Selectors and callers rebuild from source; the custom runtime uses reclaimed resident memory within 4 MiB. Full pools decline custom encounters before spawning. Full-game validation remains |
| Scenarios | Enemy squads/formations/equipment, placements/routes, triggers/treasure/towns, added squads | Squad/cohort records, equipment, formation, base levels, placement, drops, supplies, added squads, movement nodes, conditions and groups edit with undo, persist and rebuild/reopen. Browser imports preserve these edits and explicit level intent. Global site metadata, per-scenario allegiance and treasure edits/additions also persist and compile/export; original collection links support independent scenario resources. Class-size/capacity changes receive cross-feature checks. Unknown extended gates are preserved; original condition identities cannot yet be deleted because mission scripts may consume them. Browser imports also preserve site/treasure edits and shared-resource semantics. The coordinate map supports selection, field navigation, route links and undoable movement, with ROM imagery alongside. Calibrated current-ROM terrain overlays now cover49 of57 image selections with an optional view toggle; eight uncertain image/reference matches retain the separate preview. Site lists now follow the original initializer count, exposing all six initial sites in key64 while preserving its one-row header. Dynamic special placements, alternate-overlay key54 and full-game validation remain |
| Cutscenes | Resource catalog, native scheduler/cast/camera/rendering, seeking/selection, actor/action/dialogue editing, new/wiped scenes, events/backgrounds, export | Native Director directory/stream reader, C condition scanner and bounded C dispatcher control core implemented; 51 original-MIPS control comparisons pass. Continuation ownership, scene replacement and explicit launch translations are implemented and checked. All 1,498 streams preserve 193,771 commands; CLI inspection works. Searchable desktop scene/image catalog and command fields with insert/remove/restore, undo, project persistence and compiled dynamic resource export are implemented. Background preview and dialogue navigation consume edited commands. A dedicated current-ROM background selector now supports preview-before-apply, restore and undo for qualified room and fresh environment launches, preserving every other command;172 original-MIPS selector checks and four scene rebuild/reopen checks pass. All 1,998 game event launch sites support target assignment, restore, undo, persistence and compiled export, including authored scenes and short launches. Blank scenes from current-ROM setups, duplication, naming and removal of authored scenes after static transfer and event reference checks support undo, persistence, compiled export and reopening with stable authored identities. Both dialogue archive tables support source editing, new entries, restore, undo, persistence and source-built ROM export with dynamic allocation. BF navigation and standalone native dialogue playback use edited text, stepping, input, script resume and current-ROM artwork. Browser scene documents, dialogue, authored UUIDs, event bindings and wipes convert with source validation into native models; explicit conversion snapshots preserve subsequent edits. Existing browser-ROM authored identities and event bindings now load and migrate together while preserving the old arena; deleted scenes stay deleted on reopen. Shared window tracking advances within ScenePlayback. Native 64/K/B5/HUFF/raw previews preserve signed layers and frame/palette banks; all 366 images and the six-part composite checked. Asynchronous decoding, layer selection/visibility and zoom work. Actor bodies, entrance/return presentation lifecycle, poses, height, scale/opacity, cameras, projection, fades and resumable update ordering now have native execution components. Manual Actor channel assignment and the automatic layer divider execute before movement, preserving native boundary rules and release behavior. Mode-zero transform keyframe resources and completion queries execute with native integer interpolation, verified against all 61 populated resources. Packed channel/background matrices and clipped projection now drive an isolated transform preview with command navigation, channel selection, playback and backward seeking. Mode-zero Actor main and shadow matrices preserve native packed intermediate steps, camera roles, facing and shadow-height modes, with atomic pool preparation and 468 original-instruction comparisons. World main and two-shadow matrices, camera basis, light modes, terrain-height input and depth attachments also have atomic preparation, matching 732 original-instruction cases; the inspector supports both placement paths. Depth queue construction and the original tie-sensitive sort now agree across 648 instruction cases; mode-zero per-channel shadow/main/object pass selection matches 288 original-driver cases. Mode-two shadow, Actor and world/projected-object pass selection matches 192 original-driver cases, including independent shadow opacities. Ordinary and class-body layers now project through native matrices with primitive color, opacity and shadows; a reference-camera placement inspector supports command navigation, layer controls and seeking. Native tint and material color programs now consume separately retained color/alpha textures, with command state and inspector color controls. Sampling, coverage and blending remain approximations. Composed Actor passes now join queue selection, matrices, draw-driven material updates and the native world environment gradient, with atomic rollback. The Scene playback panel executes current edits with step/play, cancellable seeking, draw snapshots and explicit reference-stage assumptions. Missing scene services pause. Layered backgrounds retain original channels and native image geometry, with all31 groups loaded and room occlusion checked; world backgrounds and regular sprites now use native orthographic matrices and common-launch camera defaults, including projection pans and independent foreground selection in the engine. Layered scene sprites now render before each channel’s Actors with verified packed transforms and overlap/seek checks. Orthographic Stage props now retain normal/randomized placement, draw-driven pose updates, three image-relative depth phases, intensity textures and exact desktop seeking. All 80 selectors render with 116 flat and 102 perspective instances. Perspective props retain world depth ordering, separate screen positions, below-ground pose changes and scrolling materials; original record/matrix and desktop seek checks pass. Authored scene-color transitions now reach layered backgrounds, sprites and mask-selected world images/props with exact desktop seeking and original mask-routing checks. Captured-frame sepia/restore transitions now render with native timing, persistent vignette, source color conversion, resource restoration and exact desktop seeking. The special black pass and conditional Actor crossfade now retain original first-match selection and opacity behavior. Sepia setup and Actor color selection now preserve ordinary image-cache behavior, repeated conversion, expiry and seeking. Linked secondary bodies now follow their primary before matrix preparation and depth sorting, verified against 240 original prepasses. Body layers now use native equipment-child and item-palette selection, replacement frames and warm tint; scene rendering consumes actual appearance rows. Scripted image pan/zoom executes recovered timing with explicit image context, retargeting and snapshot replay; the launch-selected source, native vignette constructor and image compositor now render with pan and exact desktop seeking. Image transitions now retain native interpolation, three trail matrices, alpha aging, query timing and draw order. The image owner renders special sprite records800/801 with the enclosing channel transform; their launch creation remains open. Dynamic launch context, gameplay lighting and special sprites remain incomplete. ScenePlayback connects supported stages with explicit pauses at missing domains; terrain data, other scene services, complete native scene playback and structured timeline authoring remain pending. The native Actions inspector adds, removes and reorders complete actions at checked script positions and edits creation/state/movement, turns, room channels, release, cameras, fades, scale/opacity and recognized waits. Shared counter waits shift later deadlines consistently when added, edited or removed. Complete dialogue actions retain linked window IDs, placement/portrait fields and direct text editing; their lifecycle and source export/reopen checks pass. It supports these edits with game-unit coordinates, shared selection/undo, project persistence and source rebuild/reopen; control compounds and unknown words remain intact Ordinary sprite creation, pose changes and release now have Actions fields with independent slot allocation, current-ROM pose checks, live preview undo and source-built export/reopening. |
| Art/animation | Portraits/item/army/combat sprites, frames/layers/sequences, pixel/asset import/export; current animation patch semantics | Native 0x5554 sprite decoding, child deltas, palettes and alpha implemented; 22,867 actor variants checked. Current-ROM bank/pose/frame reader and layer assembly cover all 68 cutscene banks, 2,275 poses and 5,696 frames. Compiled C animation-key resolution agrees with 4,620 original-MIPS cases. Ordinary C pose scheduling passes 54,600 original-MIPS invocations; independent 2D scheduling passes 55,040. Desktop selection, stepping, seeking, adjustable playback and layer inspection work for ordinary actors, class bodies and 2D sprites, with opacity and source-adapted sepia. Class-body assets and execution pass 108,900 resolver and 141,064 sequencer comparisons; body initialization passes 675 cases. Sprite lifecycle, native draw-pass selection, transform arguments and primitive color are implemented. Actor tint/material commands and render-driven color updates now match 3,600 original-instruction snapshots, including masking and cleanup. Linked sound requests preserve native arguments; final audio, equipment drawing and complete scene rendering remain pending. Item-icon models, browser/native project persistence and source-built dynamic ROM export/reopen now pass checks for both packs, all 321 images and coexistence with expanded Classes; native icon painting, palette selection, PNG/library transfers and quantized assignment previews now work with undo and project persistence; portrait models, browser/native persistence and source-built independent resource/class-selector export now pass reopening and growth checks; portrait painting, quantized image preparation, library/PNG transfers, undo and edited-project dialogue previews now work; Army atlas models, missing-plane creation, project/browser persistence and source-built paired-resource export now pass pixel, original-MIPS, class coexistence and allocation-growth checks; Army exact-index painting, dual-palette preview, missing-plane creation, PNG/library assignment, undo and persistence now work; dynamic model slots through 63 and source-built class assignments now support new classes, with authored artwork retained when cloning; combat artwork models, descriptor-local edits, dependent-frame preservation, project/browser persistence and source-built dynamic export/reopen now pass checks; combat descriptor/child browsing, exact index/opacity and combined painting, PNG/library transfers, image preparation and undo now work; staged artwork now reaches composed pose/scene previews with immediate undo and exact seeking; new classes now inherit all five class animation tables, with dynamic resource/C export and original-loader/selector checks; native class animation-set copying, body routes, speeds and default action selectors now edit with undo and source export; Normal/Blocked override models now support browser/native imports, source-built installation/removal and template inheritance; native override controls now edit and undo with class inheritance and project persistence; current class animation assignments and Normal/Blocked overrides now drive pose/scene previews with new-class rendering, shared undo and deterministic seeking; the shared descriptor directory now relocates and expands through 4095 entries with all six original loaders and new-class export checked; private per-class/body descriptors now persist, preview, clone to new classes and compile/export/reopen with allocation ownership; native private-copy controls now edit timing/frame order with shared undo and synchronized equipment/projected preview; private frame layers now edit sprite selection, position, dimensions, flips, scale and order with live preview and undo while retaining shared frames; wide equipment markers now rebuild within the original code allocation and preview correctly; private frame image imports from PNG/JPEG and the sprite library now append independent resources with native preview, undo, project persistence and source-built export/reopening; ordinary-body private attack copies now assign Normal/Blocked selectors with atomic undo, saved prior-assignment restoration and source-built export/reopening; linked-body private attacks now retain independent primary/companion programs with C rebuilt into the original function allocation, native preview, undo, project persistence and source-built export/reopening; fixed-pose private attacks and wide companion self-transfers now rebuild from C within the original code allocations, with matching native preview and export/reopen checks; leader normalization now rebuilds in its original allocation and preserves registered primaries while excluding companions; equipment artwork and item-layer policy now copy with animation sets and new classes, edit with undo, and rebuild their original selection blocks with native preview/export checks; body construction and fixed-pose policies now copy with animation sets and new classes, edit with undo, and rebuild their original runtime allocations with native roster/entrance parity and legacy-export upgrades; browser private-sequence schemas one through four now convert copied pixels, layers, timing, deleted frames, alternate body children, idle recovery and ordinary/linked attack assignments with project persistence and compiled export/reopening |
| Sprite workspace | Reusable assets, pixel tools, layers, transparency, undo, PNG/JPEG imports and exports | Native RGBA assets, frame/layer editing, paint/fill/erase/selection/move, transforms, clipboard, timing/playback, undo, PNG/JPEG and version 1/2/3 asset import/export now work and persist in project folders/archives. Browser comparisons and desktop checks pass. Artwork tabs support individual game-image transfers and assignment; class/cutscene pose frame lists now copy with independent layers, timing, source variants, undo and project persistence. Prepared PNG/JPEG import into selected layers preserves placement, partial alpha, hidden RGB, undo and project persistence. Cancellable MP4/H.264 video export now uses Windows media services, preserves frame timing and supports scale/repeats/background, with independent decode and desktop checks. Lossless transparent animation export uses APNG with exact pixels/timing, finite or continuous looping and independent decode checks. Used-color browsing now covers layer/frame/sequence scopes, hidden RGBA, background collection, cancellation, paging and exact paint-color selection; game artwork preparation now offers optional ordered color dithering while full-RGBA workspace imports retain exact pixels |
| Save editor | Supported save formats/slots, characters/stats/equipment/inventories/Goth/Chaos Frame, valid save export | Native/word-swapped and expanded SRAM, raw RDRAM/libretro and RZIP load with slot validation. Characters, independent classes, full-width stats/equipment, inventories, Goth/Chaos Frame and calendar edit with Undo/export/reopen. Regular battery squads support formation, Unity/tactics, stock-aware supplies, ordinary/Soldier exchanges and reserve transfers, leader changes, creation/disbanding, legion creation/disbanding, follower detachment/joining, and coordinated legion movement/capacity. Original-MIPS and desktop checks cover these workflows with draft protection. Current-Class export checks refresh affected regular-slot squad/legion movement and reject formation/carry/command conflicts before atomic writes. Regular slots also create level-one reserve characters from current class rules and remove unassigned characters. Active-state membership and raw time/progression/location semantics remain incomplete; no emulator injection. |
| Tools | Experience weight, Chaos Frame, luck, alignment, attack stream split | Experience size-weight scale now edits with undo, persists, imports browser projects, assembles maintained source during export, reopens and restores retail bytes. Unknown code is preserved. Chaos Frame now toggles with undo, persists/imports, assembles its maintained source and ROM-derived artwork during export, migrates packed version 3, restores retail code and reopens. Its existing Expansion Pak requirement is explicit. Luck and six-row Alignment also edit with independent undo, persist/import, assemble maintained loaders/renderers and current-ROM fonts, migrate current legacy modules, restore and reopen; unknown owners stay untouched. High Attack now edits, persists/imports, compiles maintained source, migrates v21 and restores/reopens with guarded overlay/context lifetime checks. It remains explicitly experimental, Expansion Pak/Project64 Interpreter only; full-game validation remains |
| Delivery | Reproducible current dependencies, portable Windows ZIP, notices, clean-machine validation | Bootstrap, package and isolated-environment checks implemented; clean-machine validation remains |

Promotion compatibility: native loading recognizes and validates the earlier
direct-loader OBSG-v2 export, reads its actual requirements, and migrates an
edited table to the source-built family resource. Unchanged exports retain the
existing bytes. OBSG-v1 shared-DMA redirects remain unsupported and are explicitly
rejected by the promotion loader; it does not silently show retail requirements.

## Additional native capabilities

Placement and movement actions now offer Choose in preview. Click or drag while
positioning to set that action's X/Z coordinates in one Undo, preserving height.
The picker inverts the native forward matrix path for the current Actor, camera
and layer transforms; linked secondary bodies direct users to their primary.
Cancel, seeking, restart, scene changes and incomplete-input guards preserve edits.
`scene_runtime_tests actorPlacementCoordinates` passed (146ms) for world/layered
mapping and source preservation. `ui_tests classActorCreationControls
sceneRoomComposition` passed (2554ms): actual point clicks, cancellation, placement
Undo, independent movement destination fields, visible replay and project reopening.
Existing ROM coordinate export is reused; no new ROM code is introduced.


Actions now expose world Camera pan/zoom and a completion checkbox for movement,
fade and camera pan. Movement uses the native arrival query, fade its completion
query, and camera pan a linked duration gate. Each action and wait remain one unit
for edits, moves, removal and Undo; unknown control boundaries remain guarded.
`scene_action_tests actionCompletion choiceActions insertedWaitTimingAndExport`
passed (4507ms), including real movement/fade/projection completion before the
next action and source-built export/reopening. `ui_tests sceneCompletionControls
sceneChoiceAuthoring` passed (2125ms); duration edits retain their gate, project
reopening works, and the integrated Actions capture was inspected.


New Dialogue actions now create independent text entries and complete window
lifecycles in one Undo operation. A speaker/text dialog wraps 30-character lines
and groups three lines per page, up to eight pages, preserving the browser text
format. Recognized authored lines reopen in this dialog; shared raw text remains
available explicitly. `scene_action_tests dialogueActions` passed (1816ms), with
three real A confirmations for a three-page line before and after source-built
export/reopen. `ui_tests sceneActionInsertion sceneChoiceAuthoring` passed (3359ms):
existing-entry preservation, atomic Undo, speaker/text edits, raw-source navigation,
project reopening and both choice previews. Actions capture inspected after layout.


Two-option Choices now have native Actions authoring, editable option text,
branch insertion positions and branch-aware move/remove. Creating a choice owns
its new dialogue entry and Director block in one Undo operation. Reopened ROMs
recover the native choice layout without editor-only metadata. Preview controls
select either option through the real text interpreter; only its actions execute.
`scene_action_tests choiceActions authoringBoundaries insertedWaitTimingAndExport
 actionOrdering` passed (5278ms), including source-built export/reopen, branch waits,
checksum and source preservation. `ui_tests sceneChoiceAuthoring` passed (956ms)
for cancellation, validation, atomic Undo, options, branch fields and two different
preview outcomes. Existing dialogue/action checks also pass. No natural-game
validation is claimed.


Existing class-body actions now expose class-pair and pose fields with native retain
sentinels, preserved body flags, readable symbolic launch values, undo, persistence
and compiled export/reopen. Generic body creation still requires its launch context.

Live Actor inspection shows current class pairs/art banks and XYZ, retains selected
slots across seeking, and navigates to the active script's Actor placement/action
with incomplete-input protection. This is slot-based navigation, not branch provenance.

Dynamic event66/67 scenes expose five paired primary/secondary preview selectors.
They can use the open Save workspace's selected-slot launch cast, with source labels,
validation and sample restore. Project class additions/names and undo reach the
selectors. Launch translations change bodies and dialogue branches while preserving
authored coordinates; these choices do not edit the game army or scene source.
Saved inputs do not rerun the parent event's current-army cast selection.
Scene playback now displays edited dialogue with native text/window execution,
current-ROM artwork, explicit preview names and recorded confirm/choice input.
Window state and input survive seeking and checkpoints. Actor anchors use current
packed drawing matrices; shared-pass cadence and composition remain desktop
assumptions. Automatic Actor portraits now use the recovered lookup with bounds
checks; unsafe ordinary-Actor resource reads require an explicit portrait. Audible
playback remains unfinished.
Scene audio commands now emit native service requests, including deferred program
replacement, gain envelopes and controller writes. The silent preview preserves
these requests with deterministic seeking; it assumes no resident voices. The
resident audio consumer and synthesis remain unfinished.

Scene images support native vignette construction, scripted pan/zoom, forward
and reverse trails, and manual direction/zoom controls with animated boundary
indicators. Keyboard/button input is recorded for deterministic seeking and can
be changed after rewinding. Image scenes can select an external launch image for
preview. Map-path ribbons read current-ROM paths and preserve native curve,
endpoint, reveal and release timing; strip texture/edge drawing is approximate.
Reward panels accept an explicit preview item and execute native dismissal/query
behavior with current-ROM names. The unchanged dynamic story01FA8BF4 now runs
through scene release and checkpoint replay. Reward icons now use the current
ROM equipment/consumable packs and native content placement.
Map help, briefing choices and confirmation panels now execute with native
animation, input, handle lifetimes and current-ROM text, including deterministic
seek/replay. Desktop frame/glyph drawing is approximate. Other panel services
and automatic event/army launch context remain unfinished. Roster materialization
now uses adapted accepted C for class/source selection and pose policy, with
native construction and leader normalization; upstream party/Stage building is
still required before the UI can supply these inputs.
Iris close/open transitions now retain captured frames and background layers,
move and release Actor/sprite owners, render the native mask mesh, and replay
across checkpoints. Desktop capture timing and edge blending remain explicit
approximations.
Authored screen crops now advance with native draw timing, retain or release their
final edges, and replay through seeking. The default game scissor is also visible.
The title variant now uses its separate updater and both resolution paths, with original-instruction and desktop seek comparisons. The original primitive-only combiner makes its edge rectangles opaque black even when a texture is loaded. Desktop rectangle coverage is approximate; title menus and the People board remain unfinished.

Dockable workspaces, searchable model-based tables, independent engine API,
background jobs with useful failures, explicit allocation ownership, deterministic
runtime snapshots, and performance measurements on representative workloads.
Unknown game behavior remains identified rather than silently approximated.

Display blackout/reveal commands now use an explicit graphics-completion boundary.
The preview hides displayed output while retaining the underlying scene, reports
blackout in the status line and restores pending state when seeking. Original
request/task/VI-bit comparisons and desktop blackout/reveal checks pass. Hardware
queue latency and PAL scaling remain outside the NTSC desktop preview.

Repeat-briefing confirmation (preset2) now joins end-briefing confirmation,
briefing choices, map help and reward panels. Duplicate-preset completion queries
match native identity rules, and Yes/No labels read the correct current-ROM strings.
Original multi-panel/input/lifecycle and desktop replay checks pass.

Visible scene sprites now join Actors in the live-object picker and rendered-frame
selection. Their identities remain separate, with real draw-order occlusion,
current-state readback, seek retention and action/source-command navigation.

Direct sprite rotation Actions now create or reuse native sprites with explicit
angles, safe source-field preservation, shared undo, visible preview and compiled
export/reopening. Launch-supplied action coordinates remain read-only.

Path-oriented sprites now obtain sampled and resource-path headings during playback,
including the game's angle approximation and linked replacement positions. Original
instruction comparisons and desktop checkpoint seeking pass; missing paths pause.

Sprite path-heading creation and reuse now have Actions fields, current-ROM
reference validation, active-path slot checks, preview/undo and compiled export
reopening. Context-dependent path availability is checked during playback.

Path ribbons now have creation, endpoint and release Actions with native precedence,
independent slots, current-ROM point limits, dependent-endpoint validation, visible
palette/reveal edits, undo and compiled export/reopening.

Image-scene Actions now expose native pan/zoom rates and duration, preserve unused
and symbolic operands, and explain transition prerequisites. Editing, preview,
Undo/Redo, project persistence and source-built ROM export/reopen are checked.

Image transition Actions now expose direction, byte-sized duration and echo tail;
editing, persistence, source-built export/reopen and preview replay are checked.

Image construction Actions now expose native position, signed scales, vignette
opacity, orientation and drawing channel. Construction order, pixel/source
preservation, editing, persistence and compiled export/reopen are checked.

Image completion waits now sequence later Actions against the native finished
state, including echo draining, with counter isolation and export/replay checks.

Normal battery squads now support Soldier reserve creation/disband with native
HP/count/allocation behavior, Unity preservation, atomic Undo and save export.
Direct Soldier transfer/swap and active/suspend lifecycle work remain open.

Normal battery squad leaders can now change with coordinated saved order and
character flags, Unity/position preservation, original fixed-leader and legion
command-type checks, atomic Undo and save export. Legion demotion remains guarded.


Direct saved-squad moves and swaps now include Soldier groups and retain both
squads' Unity, matching the original exchange/commit path. Ordinary reserve
moves still reset Unity. HP, companion state, group IDs and the Soldier reserve
are preserved by direct exchange; native instruction, save roundtrip, UI and
Undo checks pass. Squad creation/deletion, legion coordination and active-state
membership edits remain unfinished.


Saved player squads can now be created from eligible reserve leaders, including
an empty army, with native number/record allocation, appended Army order, leader
flags, initial fields, Undo and export/reopening. Lists display saved squad numbers.
Deletion, legion coordination and active-state membership remain unfinished.


Carried consumables now share owned stock across squads: choices require free
copies, removals restore availability, and ownership cannot fall below assigned
copies. Runtime inventory quantity editing preserves the separate assignment
byte. Original bookkeeping, save export/reopen and desktop workflows pass.


Saved squads outside legions can now be disbanded atomically: members and Soldiers
return to reserve, supplies release their stock, Army ranks compact, and the
inactive slot can be reused. Undo/export/reopen and original native lifecycle
checks pass. Coordinated legion changes remain the next squad workflow gap.

Whole saved legions can now be disbanded while keeping their squads and contents.
Membership labels, order compaction, Undo and export/reopen pass original-code
and desktop checks. Command restrictions follow actual legion references because
original saves retain old commander flags. Individual legion changes/creation
and active-state membership remain unfinished.

Existing legion followers can now detach independently, with native formation
column shifts and derived movement/count updates. All follower positions and
successive removals through a lone commander pass original save-codec checks,
Undo and desktop export/reopen. Adding followers and creating legions remain.

Independent squads can now join existing legions with Soldier-supported capacity,
original squad eligibility and first-free-position assignment. Native assignment/
commit/save, full preservation, Undo and desktop export/reopen checks pass.
New legion creation and command-squad membership coordination remain.

Squad membership and leader edits now refresh affected legion movement/count
inside the same Undo action and refuse changes that would undercut Soldier
support. Reserve creation/removal, exchanges, transfers and leader changes pass
native finalizer/save-codec and desktop workflow checks. New legions remain.

New saved legions now allocate records/numbers, retain complete order, coordinate
commander flags and support adding followers within Soldier capacity. Creation,
reused gaps, disband/recreate, Undo and export/reopen pass original-code and
desktop checks. Product order deliberately avoids the original vacant-slot
writer's duplicate-entry defect. Save consistency and roster lifecycle remain.

Regular battery primary/secondary character-class edits now coordinate formation,
carrying capacity, command roles, squad/legion movement and one-step Undo.
Replacement leader choices refuse non-command classes and incapacitated members.
Native finalizer/eligibility and table-edit/export checks pass. Cross-slot/current-
project export consistency remains under audit.


Dynamic story scenes now select their five class pairs from the current Save
workspace army using native event priority, aliases, HP/status restrictions and
fallbacks. Stored launch casts and manual choices remain available. The01FA8BF4
coordinate/seek workflow and existing complete-scene playback check pass. This
selects preview participants; it does not simulate the event's date/trigger gates.


Party preview launch data now connects class/formation/member bindings and roster
reset to the existing native body/presentation engines. Sample classes and inferred
encounter portraits follow the browser feature, using current class equipment and
animation routes, including new IDs. Source queries, secondary height and projection
reset/countdown now execute; presentation reload ends at the gameplay boundary.
Preserved and caller-selected battle stages use environment zero. Flat terrain and
sample formations are stated approximations. Event predecessors remain separate work.

## Testing and ROM artwork import (2026-10-10)

The native **Testing** tab incorporates the standalone Scenario Seeder without
Python or a Project64 debugger dependency. It keeps the ROM and emulator-folder
pickers, wiki-ordered mission list, Use my army, and Use my save game. Each row's
**Cold boot** button closes the selected Project64 installation, waits for its save
to flush, installs that row's save, then starts the selected ROM. Selecting a row
alone does not write a save. Imported full saves get their own launch row.
Generated world-entry saves retain normal story roads and use the game's
normal entry initialization. Existing saves are backed up before atomic replacement;
slot 2 survives generated seeds and stale suspend data is disabled. Retail seeds
can be installed into expanded saves; either side is upgraded when needed using
the game's save converter. Expanded saves can also supply the tester army. Full-save
installation preserves the supplied file unchanged. Later routes retain the
standalone tool's game-testing limits; this port adds file/readback qualification.

**Edit Tester Army** opens the Save Editor tab; **Use as Tester Army** stores that army
in per-user app data and returns to Testing. Imported and edited army stats are
not boosted again. Settings and edited defaults are not shipped with packages.

Save Editor supports dragging members directly between the left squad boards,
the selected formation, and Reserve. The left boards use the same geometry and
sprite silhouettes for hit detection. Empty-cell drops move members; occupied
member drops swap between squads. A nameplate drop uses the first compatible
empty cell. These changes use the existing atomic save operations and Undo;
five-place size limits, large-unit spacing, carried items, leaders, legions and
pending drafts remain validated for both squads.
Double-clicking a character on a squad board or in Reserve opens a stats and
equipment popup, leaving the squad boards in view. Save commits all fields as
one Undo action; Cancel discards the draft. Empty cells and Soldier groups do not open it;
pending squad edits still need to be applied or discarded first.
The Items table always shows Stats columns, with other field groups available
in its side panel.

Scenario **Orders** now starts with commands: Guard, Intercept nearby enemies,
Move, Move and attack, Hit and run, Evade, and Retreat. Destinations can be named
towns/strongholds or map positions, with waypoints movable on Map after Apply.
These same commands appear in the Map sidebar when a squad is selected. Choose
destination on map picks a draft target without moving the squad's start or
changing selection; Apply saves it. Escape or switching objects/tools cancels
picking. Advanced route controls scroll at smaller window sizes.
Applying replaces only the selected squad's starting behavior. Shared and gated
routes are retained, and a private simple order is reused for subsequent edits.
New orders respect the native 16-step capacity; group followers must be made
independent or commanded through their group leader. Original scripts, route
steps, conditions and group settings remain under **Advanced routes and conditions**.
Nearby-enemy commands enable native target acquisition; runtime target/state
checks still determine whether pursuit begins.

Main tabs now include **Art and Animation** and a final **Art Library**. Distinct
outlined buttons enclose the Cutscenes inline red W.I.P label. **Import from ROM**
copies current-ROM or another ROM's actor poses, item icons, portraits, army
sprites, and scene images into the reusable library. A bounded resource-key
loader also accepts native sprite containers; resources needing external palettes
must use their actor/pose selector. Imports retain pixels and independent pose
layers with Undo and project persistence. Game assignments remain separate.

### Scenario starting time and completion items

Scenario Settings edits the native ESET starting hour/minute and the primary
and fallback boss drops used by the completion screen. Changes apply immediately
and share Undo with squad edits. Starting time applies on mission entry; suspended
missions retain their saved clock. Map site selection already edits shared town
names/populations automatically. Export preserves unknown ESET/town bytes.

### Clock and battle rule controls

Scenario Settings has independent day/night duration percentages, global to all
missions (25–400%,25-point steps). Tools has named tactic alignment contributions
(−5..+5 per kill), movement fatigue and rest recovery rates (25–400%). Eligibility,
night/equipment modifiers, fatigue/HP caps and the native calendar remain. Cycle
rates are rounded to whole native update intervals; they are not real-time timers.
All controls apply immediately, support Undo and a Retail reset, and persist in
Tools schema5. Exports authenticate code owners, assemble in original allocations,
check exact output, update CRC and reopen. Unknown modified owners are preserved
and their controls disabled.
