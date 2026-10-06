# Lordly Caliber 0.5 — native development build

Windows x64, Qt 6.12 and C++. Extract the portable ZIP and run
`LordlyCaliber.exe`. Open a US Ogre Battle 64 ROM, edit its records or
descriptions, save a project, and choose Build and export ROM. The source ROM
is read-only. The included N64 toolchain compiles game sources and edited data
during export; no Decomp checkout is required.

This is an unfinished migration, not the 0.5 release. Classes and Items provide
record editing, descriptions, undo, project persistence and source-built export.
US Rev 0 Classes also provides promotion requirements, family selection and
creation, compatibility masks, and new classes through ID 255. Select a class and
choose Add class from selected to copy its records, appearance, requirements and
description. Rebuilds grow the owned tables and description resources, rebuild
their game readers, and install compatible save conversion within the 4 MB RAM
layout. Scenarios, Cutscenes, Art, Sprites, Tools and Save workspaces are also
available. Cutscene playback and some save operations remain incomplete; see
`docs/features.md` for current coverage. Full-game boot and hardware memory/device behavior remain unverified
by the isolated instruction and simulated save tests. Imported unsupported sections remain
in the project and block export so they cannot be silently dropped.

Projects are folders containing `project.json`. Sharing archives use
`.lcproject` and omit the ROM. Existing browser Editor JSON projects can be
imported through Open project after opening the matching source ROM.

Choose Review project changes from the File menu or toolbar to inspect the current
changes, find text, copy the report or save a text changelog. The comparison uses
the loaded source ROM, so project saves retain the changelog. It includes project
assets and summarizes binary scene/artwork changes. Apply pending form edits first;
the independent Save workspace is outside this project report.

Shops supports all 35 shops with up to 50 equipment items and 30 consumables,
including empty stock, search, reordering and undo. Consumables provides prices,
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
persuasion, retreat and weighted rewards, including added classes.

In Cutscenes, Background preview offers the scene's available background groups or
environments. Choose one to preview its image and foreground pieces, then Apply
background. Restore original and Undo preserve your other scene edits. Actor positions
do not move with the setting. Inherited or gameplay-chosen scenery remains controlled
by the game; those scenes explain why direct selection is unavailable.

The Cutscenes Actions tab edits movement, placement, cameras, fades, scale, opacity
and recognized waits with named fields. Coordinates use game units and durations use
update counts. The list follows script order; conditions can change when actions run.
Actions and Commands share Undo. Complete or discard unfinished input before saving,
exporting or choosing another scene. New action insertion and timeline scheduling
are still being migrated.

For scripting:

```powershell
.\LordlyCaliber-cli.exe --rom 'source.v64'
.\LordlyCaliber-cli.exe --rom 'source.v64' --project 'project.json' --output 'edited.v64' --report 'build.json'
```

For development, install Visual Studio's MSVC 2022 14.44 C++ tools and Windows
SDK, then run `native/tools/bootstrap.ps1` and `native/tools/build.ps1` from
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

The Cutscenes Actions page also adds and removes actions at named script positions.
Waits preserve native counter spans and adjust later deadlines when possible;
unsupported control layouts explain why a structured wait cannot be inserted.

Dialogue actions keep window creation, completion, resume and close together.
Their text link opens the referenced entry in the dialogue workspace.

Select an action and a destination to move it. Moves retain complete waits and
dialogue sequences; separate native control sections remain in place.

Actor actions include turning, room drawing-channel assignment and release. New
follow-up actions inherit a nearby Actor slot; all values remain editable.
