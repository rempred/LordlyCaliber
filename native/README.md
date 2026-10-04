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
layout. Animation changes and the remaining browser workspaces still need native
workflows. Full-game boot and hardware memory/device behavior remain unverified
by the isolated instruction and simulated save tests. Imported unsupported sections remain
in the project and block export so they cannot be silently dropped.

Projects are folders containing `project.json`. Sharing archives use
`.lcproject` and omit the ROM. Existing browser Editor JSON projects can be
imported through Open project after opening the matching source ROM.

Shops supports all 35 shops with up to 50 equipment items and 30 consumables,
including empty stock, search, reordering and undo. Consumables provides prices,
metadata and ten supported effect models in the Effects inspector. Items 11–16
share one stat-increase range. Healing amounts can update the description's HP
number; turn that option off when a custom description has no numeric amount.
Other item behaviors remain unchanged. Export rebuilds maintained sources in
the game's original RAM allocations and reopens the results before saving.

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
