# Lordly Caliber 0.5 — native development build

Windows x64, Qt 6.12 and C++. Extract the portable ZIP and run
`LordlyCaliber.exe`. Open a US Ogre Battle 64 ROM, edit its records or
descriptions, save a project, and choose Build and export ROM. The source ROM
is read-only. The included N64 toolchain compiles game sources and edited data
during export; no Decomp checkout is required.

For local iteration, run `native/live/LordlyCaliber.exe`. The maintainer command
`native/tools/package.ps1 -Live` updates that stable folder without creating a ZIP.
Close the live editor after saving your work before updating it.

Open, Save, Build and Review are at the right of the File/Edit menu bar. The
workspace tabs and their section tabs remain below it. Advanced animations uses
the selector at the right of the Art tab row to choose its editing tool.

Art / Combat animations provides class, action, side, mode and artwork-variant
filters. Choosing an animation only previews it; **Assign** applies it. Normal
assignments cover game modes0 and1; Blocked is mode2. Enable frame and sprite editing
to create an independent class copy, then change timing/order, edit layers or paint
a frame's sprite. Select a frame to inspect it; Play resumes sequence playback.
The prior detailed controls remain under Art / Advanced animations.

Art / Army sprites lists classes separately. Editing shared artwork automatically
copies both facings for that class, leaving others unchanged. Shared-model editing
is available explicitly in the advanced view. Legacy boss labels identify classes
without changing their in-game names. Save and enemy formation boards both show
cells7–9 in the front row.

This is a local development build, not the published 0.5 release. Classes and Items provide
record editing, descriptions, undo, project persistence and source-built export.
US Rev 0 Classes also provides promotion requirements, family selection and
creation, compatibility masks, and new classes through ID 255. Choose New class
for fresh gameplay settings: 1 HP and 1 in each base stat, no equipment, attacks,
family or stat requirements, and alignment range 0–100. It initially uses Knight
artwork and animations, which can be changed in Art. Copy Class
remains available to copy a class's records, appearance, requirements and
description. Create and assign family creates an independent family containing
only the selected class. Choose its class-change rules directly: within family,
within family plus Lich, within family plus Lich/Vampire, or no manual changes.
Other members are assigned through Class family; compatibility, stat, equipment
and story requirements still apply. Rebuilds grow the owned tables and description resources, rebuild
their game readers, and install compatible save conversion within the 4 MB RAM
layout. Scenarios, Cutscenes, Art, Sprites, Tools and Save workspaces are also
available. Revision 0 is the implemented rebuild baseline. Revision 1 currently
offers basic records and text; its remaining browser workflows, including Shops,
Scenarios, encounter overrides and the Chaos Frame counter, still need porting.
Full-game boot and hardware memory/device behavior remain unverified
by the isolated instruction and save-file checks. Imported unsupported sections remain
in the project and block export so they cannot be silently dropped.

Projects are folders containing `project.json`. Sharing archives use
`.lcproject` and omit the ROM. Existing browser Editor JSON projects can be
imported through Open project after opening the matching source ROM.

Choose Review project changes from the File menu or toolbar to inspect the current
changes, find text, copy the report or save a text changelog. The comparison uses
the loaded source ROM, so project saves retain the changelog. It includes project
assets and summarizes binary scene/artwork changes. Apply pending form edits first;
the independent Save workspace is outside this project report.

Items / Add item from selected creates a new equipment ID, with editable name,
independent description and ROM-sourced icon/weapon appearance. The current
nine-bit equipment format supports IDs through 510 (511 remains reserved for
class visual policy). Export relocates the complete item records and names with
the existing 4 MB loader, expands description/icon/appearance resources, and
updates their game references. Inventory slot capacity is unchanged. New items
appear immediately in equipment and reward pickers and can be saved in inventory.
Special behavior hardcoded to a particular original item ID stays with that ID;
copying an item copies its editable data and artwork, not new scripted effects.

Shops starts with the 35 original shops and supports adding shops through ID 255,
with up to 50 equipment items and 30 consumables per shop. Add opens a searchable,
multi-select modal with icons decoded from the ROM and current project artwork.
Create stock with Add shop, then assign it to a stronghold. Scenarios / Sites also
provides Create new shop here and Remove shop from site; removing a facility keeps
its stock available for other strongholds. The Witch’s den checkbox adds/removes
that facility when Apply site changes is used. Facility metadata affects every
scenario using that stronghold. Stock editing, assignment and creation support Undo. Consumables provides prices,
metadata and ten supported effect models in the Effects inspector. Items 11–16
share one stat-increase range. Healing amounts can update the description's HP
number; turn that option off when a custom description has no numeric amount.
Other item behaviors remain unchanged. Export rebuilds maintained sources in
the game's original RAM allocations and reopens the results before saving.

Encounters exposes all45 pools, their shared scenario references, and32 terrain
rates/slots. Pools can use added classes, be copied, or restored. Global rates
provide independent normal/alternate rolls, including zero and full chance for
that check; terrain and unit checks still follow. Their dispatcher rebuild fits
its original code allocation. Class rewards provide three weighted outcomes per
class, including added classes, with copy/restore and undo. Choose equipment,
consumables, or None; zero weights exclude an outcome. Their selector rebuild
also fits its original function and uses temporary stack space for one ROM row.
Scenario rates provide independent overrides for Area Investigation and missions,
or inherit the corresponding global setting. Shared creature pools do not force
shared rates. Removing every override restores the global-only dispatcher.
Custom neutral squads support independent members, equipment, formation, card text,
persuasion, retreat and weighted rewards, including added classes. Their square
formation cells use ROM portraits; drag to move or swap members, with the enemy
front row at the bottom. Apply squad saves the draft as one Undo step. Use this
squad for encounters controls whether it replaces the normal pool for its scenario
and terrain. Configured persuasion uses the entered chance and matching leader
bonus; it can only succeed for the last eligible target. Original game calculation
retains the native persuasion rules. Class rewards lists classes used in pools or
custom squads first. Global rates names the mission and area-investigation rolls;
terrain rates identifies the verified terrain names alongside numeric IDs.

In Cutscenes, Background preview offers the scene's available background groups or
environments. Choose one to preview its image and foreground pieces, then Apply
background. Restore original and Undo preserve your other scene edits. Actor positions
do not move with the setting. Inherited or gameplay-chosen scenery remains controlled
by the game; those scenes explain why direct selection is unavailable.

The Cutscenes Actions tab edits movement, placement, cameras, fades, scale, opacity
and recognized waits with named fields. Coordinates use game units and durations use
update counts. The list follows script order; conditions can change when actions run.
Actions and Commands share Undo. Add actions at the listed positions, move them to
another position, or remove them as complete units. Move, Fade and Camera pan offer
Wait until this action finishes. New Dialogue has speaker/text fields and its own
text entry; Choice provides two options and branch-specific insertion positions.
For placement and movement actions, Choose in preview lets you click or drag to set
the Actor's position or destination while preserving its height. Cancel leaves the
project unchanged. Complete or discard unfinished input before saving, exporting
or choosing another scene.

Party-dependent cutscenes now preview with sample members selected from class
requests and dialogue portraits. The preview labels these inferred participants;
they are not a claim about a player's saved formation. Class equipment and body
routes use current project edits, including added classes. Preserved battle stages
and caller-selected terrain use sample environment zero and flat ground. Party
entrance, binding and return operations execute in the native engine. Returning
to gameplay ends the preview without starting full-game emulation.

Scenes that inherit their cast, dialogue and scenery now prepare their preceding
scenes before starting the selected timeline. Launch context selects among known
event routes or ending callers. Changing an event assignment and Undo/Redo update
these choices. Setup can be paused; choosing another scene cancels it. Event timing
and setup dialogue acknowledgement are approximated. Saved story flags default to
clear and ending previews choose the branch leading to the selected scene. Stage
prop animation resets, explicit Actor height and the room camera scale are native.
Audio requests remain silent, terrain is flat, and unsupported gameplay services
show a boundary instead of modifying the source. Full-game menus are outside the
cutscene preview.

Animation sequences can be replaced from the Sprite workspace, copied between
classes, compared at a shared origin, and translated in batches. Single-frame
copy offers art only or art and duration. Prepared imports retain animation
controls and create independent artwork where needed.

Cutscene Actions display their executed times and seek from action selection.
Drag an Actor to insert placement at the playhead; Control-drag pans the view.
Add at playhead retains supported timed waits. If a scene exits with movement,
fade or camera work pending, Hold until finished inserts completion waits.
Use as template names an existing replacement while preserving its game triggers.

Import playback inputs accepts browser input JSON for the selected scene. These
are transient preview inputs, separate from project edits. Fresh room launches,
Actor rows, paired cameras, recorded controller inputs and qualified captured
movement resumes are supported, including Graduation's continuous capture with
dialogue. Clear playback inputs returns to the normal editor startup. Specialized
files containing recorded external callback outcomes or active inherited dialogue
payloads currently report that unsupported context. Normal native scene playback
computes its own callbacks; it does not require those research captures.

For scripting:

```powershell
.\LordlyCaliber-cli.exe --rom 'source.v64'
.\LordlyCaliber-cli.exe --rom 'source.v64' --project 'project.json' --output 'edited.v64' --report 'build.json'
```

For development, install Visual Studio's MSVC 2022 14.44 C++ tools and Windows
SDK, then run `native/tools/bootstrap.ps1`, `native/tools/bootstrap-n64.ps1` and
`native/tools/build.ps1` from
the repository. The bootstrap downloads authenticated Qt/CMake archives and
uses Windows tar; Node/Python are not application dependencies. The build uses
the installed Ninja 1.13.2. `native/dependencies.json` records official source
URLs and compatibility decisions.

Run checks with `native/tools/build.ps1 -Test -RomPath <private-master.v64>`.
Private-ROM cases need the authenticated tools under
`native/.local/n64-toolchain`; the portable package carries those same tools.
`native/tools/package.ps1` creates a fresh development ZIP. Nothing is published
by these commands. Qt and other dependency notices, sources and toolchain build
recipes accompany the ZIP. See `THIRD-PARTY.md`.

The [GitHub release workflow](https://github.com/rempred/LordlyCaliber/blob/main/native/docs/releases.md) builds the full ZIP from Main,
checks package integrity and isolated startup, and attests the ZIP and both editor
executables. See [VERIFY-BUILD.md](VERIFY-BUILD.md) for download verification.

The Cutscenes Actions page also adds and removes actions at named script positions.
Waits preserve native counter spans and adjust later deadlines when possible;
unsupported control layouts explain why a structured wait cannot be inserted.

Dialogue actions keep window creation, completion, resume and close together.
Their text link opens the referenced entry in the dialogue workspace.

Select an action and a destination to move it. Moves retain complete waits and
dialogue sequences; separate native control sections remain in place.

Actor actions include turning, room drawing-channel assignment and release. New
follow-up actions inherit a nearby Actor slot; all values remain editable.


In the Save workspace, regular battery slots support squad creation/disbanding,
member and Soldier reserve transfers, leader changes, carried-item stock checks,
and legion creation, joining, detachment and disbanding. Legion capacity follows
the command squad's Soldier groups. These edits have a separate Undo history;
export the edited save separately from the ROM project. Active runtime and
suspend-save squad membership remains read-only.
