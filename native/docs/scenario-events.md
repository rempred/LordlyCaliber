# Scenario event instructions

**Event tree** is the default. Select a row to edit its fields. **Scenario
flowchart** groups the selected location's Arrival, Mission and Area
Investigation events by trigger. Click an event to unfold it. Solid arrows
show execution, including branches and shared destinations. Click a flag to
highlight its uses and see a list of checks and writes. Dashed lines connect
visible checks with writes for the selected flag. **Find all campaign uses**
also searches other event programs and lists automatic clears on program
changes separately. Reused mission flags keep their program scope.

Double-click a step to open its controls in the tree. For an authored event
this selects the exact nested condition or action in the authoring dialog.
Changing views, expanding events and inspecting flags are navigation only.

The location view provides Arrival, Mission and Area Investigation. Named
mission variants come from current routing, including authored arrival rules;
an explicit All event programs option retains access to shared/global scripts.
Area Investigation opens its body by default and also exposes Entry conditions.

EventFlow tracks current register definitions into named predicates and retains
exact source fields for editing. EventTree projects their control flow into
WHEN / IF / THEN / ELSE branches and proven short-circuit AND/OR conditions.
It preserves shared exits, loops and calls; it does not generate a second script.
Pure value preparation may be grouped into its consumer, but side effects remain
explicit actions. Source fields, including numeric comparison relations, use
the same project/undo/export representation as Raw instructions.

The squad roster selector describes which character table the event checks.
Squad slots 0–29 use the player army roster. Slot 30 begins the opposing
roster. For example, Kinseya reads HP and petrification from the visiting
member's corresponding roster. A check for one particular squad still names
that squad. A custom numeric cutoff displays its slot number.

Shared character checks have names in both the caller and the destination.
In Gunther Piedmont, Katreda's confrontation checks the member's identity,
including equivalent class variants, then checks the current class separately.
The editor recognizes these routines from their complete instructions and
keeps their calls, return paths and original bytes available in Raw instructions.

Comparison overrides are stored in the operands object as
`comparison:<instruction byte offset>` with relation10..15. The original low
opcode nibble/register layout and branch displacement are preserved and checked.
Changing an explicit flag ON/OFF control removes a conflicting relation override.

Scenario → Events follows the selected runtime scenario. The script selector
separates story missions and Area Investigation. It identifies rows from the
retail scenario-initialization sequence; it does not assume event row equals
scenario key. The event directory remains the game's original directory.

World story routing runs on arrival; Area Investigation uses its own routing
program70. A row0 return now reads “Stay on the world map”; it does not load an
investigation program. Actual program-loading steps have an Open event program
button. For Tenne, Flag102 OFF loads4; ON stays on the world map; choosing Area
Investigation loads71. Native instruction checks cover these distinct paths.

Cutscenes uses the same reviewed default scene names as Events. Shared selector
names remain together on their single resource; project and authored names take
precedence. Unreviewed dormant IDs retain their existing identification. These
display names do not modify scripts or selector assignments.

The readable list names the original handlers and instructions using the reviewed
US Rev0 event corpus: 113 nonempty programs and 55,471 instruction words. Storyline
also includes direct world-routing, reports, birthdays and chapter-introduction
programs, which do not all use the mission's six-group layout. Instructions remain
in byte-offset order, under their physical handler entry points; shared calls and
branches can transfer elsewhere. The default scenario view shows flag instructions,
branches and cutscene calls. Show all commands includes register preparation and
the underlying instructions with their contextual meanings.

Select a flag step, choose the flag, and select Set/Clear or Check ON/OFF.
Apply creates one Undo step. A check changes which Boolean result takes the
existing branch; its other result continues after the check. Go to branch
destination reveals that command. Restore loaded flag instructions restores the
input ROM's flag edits for this row without changing its cutscene assignments.
These edit script behavior, not current flags in a saved campaign.

Names are reconciled across programs. Persistent flags have global names; scratch
flags use the relevant event or launch context. For example, Flag202 in Gunther
means Katreda was encountered, while Flag202 in Capitrium gates Biske's nighttime
conversation. R0..R7 remain register IDs, with their current argument role described
beside the register on that instruction, for example `R1 (flag to check)` and
`R0 (flag value: ON/OFF)`. The same register can receive a different label when
reused for an hour, dialogue answer or recruitment template. These roles come
from the reviewed instruction context; unclear roles keep their register IDs.
The importer also uses the verified native operand contracts to distinguish a
squad index, formation position and returned member, and to name inventory slots,
calendar/history arguments and saved register values. Names for scoped launches,
local helpers, dialogue choices and scenario squads resolve through the same
identity rules as the reviewed research catalog. Scene-launch scope does not
create a new scene identity; local-code and dialogue-word scopes remain separate.
Unknown custom IDs remain numeric. Selecting a different flag
does not alter any other reference to the old flag. These are descriptions of ROM
instructions, not a display of current saved-game flag values.

`native/tools/import-event-semantics.cjs` imports verified descriptions and approved
names from the parent `docs/event-semantics/` research into an embedded resource.
It validates the source hashes and complete corpus counts. Product builds do not
require the research checkout. Contextual narratives apply only when the entire
decoded event program matches its researched digest, the project uses the researched
US Rev0 base ROM, and no scene/dialogue edit invalidates that context. Projects
loaded over that base keep their descriptions for unchanged scripts. Reopened
modified ROMs conservatively use current-byte decoding until scene/dialogue
dependencies can be individually qualified. Changed scripts also use current-byte decoding;
supported edits keep their existing controls. Special layouts with unchanged
structural words remain inspectable with mechanical command names after same-size
edits. Unrecognized structural changes are not assigned original narrative labels.

## Events and Raw instructions

Events is the default view for scenario and Storyline event programs. It lists
native event handlers and projects current instructions into triggers, conditions
and actions. Raw instructions opens the same event/instruction and retains the
previous value editing tools. Leaving Raw applies valid pending flag edits; invalid
drafts keep the Raw view open. Both views use SceneEvents and the same undo stack.

The grouped view recognizes town guards, present/friendly/living/non-petrified
visiting-unit guards, flag requirements and branches, dialogue-answer comparisons,
property comparisons, flag writes, scene launches, recruitment and its immediate
processing request, property/scenario selection, game-mode requests, event-program
switches and local branches/calls. A complete town-check/scene/return handler is
one town event with a cutscene selector. Extra conditions and side effects are
retained; unsupported computations appear as explicit Additional game logic
blocks with raw navigation. This is not arbitrary instruction insertion/deletion
or conversion between unrelated action types.

The projection does not rewrite or move instructions. Each recognized group owns
its exact word locations. The audited VM edges (including non-adjacent local-call
producers) and branches decoded from current bytes protect internal entry points.
Closed guard patterns explicitly own their internal branches. Retargeting a guard
so its meaning changes prevents the simple-visit projection. Changing cutscenes
continues to use the relocatable launch-binding path. Large shared handlers show
40 actions per page; destination links navigate to the owning page and event.

`event_flow_tests` checks every instruction in all113 programs /55,471 words is
owned exactly once, tests Mulsuk and Gunther grouping and malformed/changed guards,
and exercises default view, both-way editing, undo/redo, project save, compiled
export and reopening. `tests/event-flow-mips.cjs` executes an exported edited town
handler in the original resident VM for matching/nonmatching towns, selector
preparation, the cutscene request and return. It does not render scenes or play
through a complete mission.

## Preservation and export

EventScript recognizes six-byte immediate Boolean writes (68 flag, 69 value,
71 60) and checks (69 flag, 60 61, AC/BC relative destination). A compound with
another branch or handler entry into its middle stays split into separately
editable instructions. Flag IDs remain 0..255.

The Editable values column opens controls inside the selected instruction row.
Named references use searchable choices; amounts use typeable number boxes.
Adjacent low/high literal loads can be edited as one 16-bit value. Register and
service choices retain unrelated operand bits. Short branches offer destinations
inside their signed-byte range. Wide jumps/calls with an adjacent R0 literal
offer destinations across the instruction inventory. A dynamic wide jump retains
its separate high operand and the instruction(s) producing R0; there is no single
constant destination to edit. Commands with no arguments have no value control;
their unused stored operands remain intact.

SceneEvents version4 adds an `operands` object inside each edited row, keyed by
original instruction byte offset. This also works for the direct/shared layouts
that the original flag-only decoder cannot walk. Instruction positions come from
the authenticated 113-row inventory. Edits cannot address table words, change
command bytes, select null/hanging native service slots or put a branch into data.
Existing flag edits apply first, followed by operand edits and Campaign extensions;
scene assignments use the existing binding/stub path. Custom world rules retain
their separate numeric location keys in row0. No instruction insertion, deletion
or reordering is added by this change.

SceneEvents project version2 adds sparse flag edits keyed by row and original
instruction offset. Empty flag sets retain version1. Export compiles the edited
bytes through the existing data compiler, composes launch bindings/stubs, and
uses LC05SCPT version5 metadata when flags are present. The allocation reader
validates flag controls against preserved retail programs and checks the exact
decoded output including launch changes. Flag-free allocations continue to use
version4; earlier allocation versions remain readable. Original event resources
and unrelated trailing data are retained. Legacy scene import keeps flag edits.

Tests and current validation results are recorded in the parent task note
docs/Plans/task-logs/all-event-semantics-20261103.md and the earlier
docs/Plans/task-logs/scenario-event-editor-20261013.md. No new full-game execution
claim follows from desktop or export/readback checks.

Value-edit coverage is in `event_value_tests` (inline entry, undo/redo, project
save, combined export and reopening) and `tests/event-values-mips.cjs` (execute
the exported resident VM against the exported event row). The latter observes
recruitment at the native queue boundary; it is not a full mission playthrough.

## Creating event logic (November 3)

Events offers **Create event** for ordinary mission and area-investigation
programs. Choose WHEN and its named town/squad, add IF / ELSE, and choose actions.
Conditions can combine ALL (AND), ANY (OR) and NOT. Selecting a THEN or ELSE branch
places new actions there; Move up/down changes their order. Added events can be
edited, duplicated and deleted, with normal project Undo/Redo.

Actions include cutscenes, persistent flags, recruitment templates, Goth,
calendar days, Chaos Frame, event-program changes and explicit event endings.
New events run before the original handlers unless Run after existing events
is selected. Continue with other events leaves the remaining handlers eligible;
Stop processing this event trigger consumes the trigger; Return to gameplay
requests the native caller-mode return. Scene rendering and complete mission
playthrough are outside the instruction-level tests.

An existing event's Change WHEN moves its native handler-table assignment.
Exact town/squad guards become the target selector, including current project
names and added squads. The compiler preserves original instruction addresses,
unknown words and unrelated scripts. Generated instructions are visible in Raw;
change their source through the tree editor instead of editing generated bytes.
Special shared programs and world-arrival routing retain their existing editors;
new arbitrary trees are not exposed for those distinct dispatch layouts.

Structured trees and trigger changes share SceneEvents metadata (project event
version5) and the existing ROM resource allocator. Conditional paths use bounded
long jumps; a program remains below65535 decoded bytes. Each program supports128
added events; each event supports1024 steps/conditions and16 nesting levels.
Export/reopen validates metadata against the actual decoded resource.

Verified value controls now separate squad presence and side, expose boolean
story tests as ON/OFF, and hide comparison opcode IDs. Identity/current-class,
naming-category and temporary-flag labels retain their native domains. The
F8 one-based town result remains16-bit until a native low-byte copy occurs.
Research references: parent docs/Plans/task-logs/event-value-followup-20261103.md
and event-action-followup-20261103.md. No permanent story meaning is invented
for a register whose producer differs across branches or yields.

## Named custom flags and visitor conditions

Flags in the Events view manages named custom flags. A flag can also be created
directly from a new tree's Flag selector. The dialog holds those definitions
locally until OK; Cancel changes neither the project nor the available flags.
Renaming retains the numeric identity. Deletion is blocked while a tree uses the
flag and retires its ID, rather than reassigning saved progress to another flag.

SceneEvents version6 stores definitions in row0's `customFlags` metadata. IDs
256–767 use a separate64-byte permanent bank. Only primary event VM service12
dispatch slots use its getter/setter; original native callers and their special
training/menu aliases retain the original implementation. New flags start OFF
and persist across event-program changes and game saves. Native temporary-flag
clearing still affects only its original byte-addressable range. The labels are
editor metadata; no additional in-game text is allocated for them.

The custom bank requires class/save relocation even with a retail class count.
Class descriptor6 records its bank and getter/setter addresses. Save formats
4/5/6 extend normal, capacity and large records at offset0x1870. Existing saves
are migrated with the new bytes cleared; new-game initialization clears the
live bank, while initializing a different save slot clears only that record.
Format/image/compression helpers share permanently loaded class storage to
leave the resident encounter extension allocation available.

The common town visitor guard exposes squad presence/side, member position,
HP and Petrification. Both army/opposing branches receive the same edit; Either
removes that comparison without relocating the surrounding instructions.
All original instruction bytes remain owned and visible in Raw.

Scenario flag picker names come from the audited generator input
`tools/event-semantics/flag-picker-labels.json` in the parent repository.
Reusable scratch flags have scenario-specific summaries rather than one global
story meaning. Exact instruction-context names remain available separately.

Branches shows editable entry conditions beside the selected route. Original
game rows expose their named flag, ON/OFF requirement, Chaos Frame threshold
and comparison where present, and destination event. These edit the original
instructions without moving them or adding a competing rule. Shared checks
are identified: changing one also changes the other routes that use it. Added
rules retain their priority and add/remove-condition controls. The route
summary and controls refresh together, including after Undo/Redo.

Flag conditions in Events, authored trees and Branches have **Where set ON**
and **Where cleared** links. They inspect current project instructions, not a
static original-ROM cross-reference. One result opens directly; several
results offer named events. Navigation opens the exact write within its event.
Temporary flags use their program's scope; persistent and custom flags search
across programs. Program-change resets are listed separately from explicit
clear actions. No-match text means no matching event write was found, not that
the game never changes the flag in native code.

Register descriptions are instruction-specific. The importer reanalyzes each
authenticated program to retain local-call, value-stack and comparison roles
even in older research rows without register metadata. Shared helpers retain
caller-specific roles; yields do not falsely carry values into another event.
Quoted successor instructions use the destination's roles, not the jump's.

All 256 game-operation request operands have native-consumer names. In
particular, request 27 is **Show post-mission save menu**. Request 4 opens
selected-character naming; 11/12/13 open cutscene, battlefield and world-map
child modes. Default request aliases return to the previous game mode.
These are presentation corrections; the original program bytes are preserved.
Parent proof: `docs/Plans/task-logs/event-request-closure-20261103.md`.

The related native training/menu traces are closed separately: the Final
Chapter Magnus training-fee query reads mission drop slot 8 bits, not extra
story flags; the unreachable world-map panel is a Pedra summon-name list.
Its six valid flag entries are 5–10. Padding/graphics overreads at entries
11–20 do not establish additional unlock meanings. These findings do not add
gameplay controls or change the custom-flag bank.

## Scenario creation and flowchart editing

The World Map's Create scenario action attaches a mission to a new or existing Map
Location. Its battlefield can copy a supported map and squads, copy the map
without squads, or start with flat paintable terrain and two bases. Opening
and victory templates create editable event trees. The setup connects a road,
adds an unlock after a chosen existing mission, and allocates a named custom
completion flag. Creating the scenario is one undo operation.

There are 64 additional mission programs (115–178) and 64 independent battlefield
keys (69–132). Earlier custom programs 2/3 and battlefield keys 10/22/35/62 remain
readable. Native media keys 65–68, including class media 67, are preserved. The
40 physical world-map locations are separate: successive missions can share a
location and use distinct prerequisite/completion flags. Special battlefield models 23, 24 and 42–44 are excluded
from the copy selector. Town definitions, including names and shops, retain
their shared ownership. Mission site state and treasure progress are separate.

The 179-entry event directory lives at z64 ROM `0x27c5800`; all four native
lookup paths use it. The extended mission descriptor at `0x27c5600` points to
the independent rows and their 4,928-byte runtime copy in the boot-loaded
permanent class-data allocation. The smaller model table and victory helper
remain resident. Each native table constructor is rebound and authenticated;
configured Goth, shops and expanded-army consumers share the same tables.

The flowchart provides Create event, Add step and Delete controls. Authored
conditions can retain either branch when removed. Double-clicking a step opens
its editing controls. Supported original comparisons can be wrapped in an
additional condition or bypassed toward a chosen outcome. Both the flowchart
and compact view project these changes from the same graph.

SceneEvents JSON version 7 records mission names and original-comparison
edits; version 8 adds the extended event rows. The ROM's LC05SCPT version 7
authenticates the relocated directory. An added comparison uses the verified unused VM opcode 8 and a bounded
resident bridge. The bridge saves and restores the eight temporary registers
without consuming the original interpreter's value stack. Original instruction
addresses remain stable. Removing the final added comparison restores the
original dispatcher. Flag references include the added predicates.

Opening templates select the private battlefield, play loading cutscene 1078,
optionally play an introduction, and request battle entry. Native settlement
awards mission rewards, restores existing undead and processes Angel Knight /
Zombie opportunities before dispatching the mission-completion event. The
template must not repeat those operations. Victory templates set the custom
completion flag, clear the mission marker (or retain it for a following mission),
restore world event program 0 and return to the physical map location. Native
location aliases remain intact. Request 27 is the save menu, not battle results;
older generated endings migrate to the corrected completion action. New custom
arrivals clear no retail temporary flags. Blank and map-only missions require capture of the
enemy base. Exact copies use their source mission's victory rules.

The compact completion view and Scenario flowchart show the standard native
closing flow separately from editable event instructions. It includes undead HP
recovery, roster/class eligibility, leader exclusion, Angel Knight spare gear,
permanent-stat/Alignment/Luck checks, the Zombie roll, Barrier protection, class
and identity writes, and equipment/HP updates. The shared flow appears once per
mission program in the chart. It is a presentation of game-run code, so it does
not emit extra transformation calls or offer false event-row edit/delete controls.
Descriptions are qualified against five reviewed native owners, normalizing
authenticated class/capacity relocations. Unknown modified owners are identified
instead of receiving stock branch descriptions. Equipment names follow the
project's current Angel Knight defaults. Authored compact trees are populated
from their saved nodes, including closing scenes, completion flags and returns.

Independent field resources are immutable project snapshots, with checked
relocated resource links and expanded model tables. Canonical accepted code is
emitted before those data references are rebound. Reopening an exported ROM
retains the snapshots and supports byte-stable rebuilding. Instruction-level
checks exercise mission entry, completion, town actions, loader paths and
Testing save routes. Full console rendering and the complete battle-results
screen lifecycle remain gameplay verification limits.
