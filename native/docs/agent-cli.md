# Using Lordly Caliber from an agent

Use the portable editor and your own game ROM. The CLI runs locally. Its structured commands and the desktop share the project model, compiler and ROM export checks. An independent desktop session gives agents access to dialogs, preview controls, file imports, drawing tools, Undo and Save. A running graphical editor can open the resulting project folder or sharing archive.

## Give your agent a task

The editor’s Windows ZIP includes the CLI. Extract the whole ZIP and open the editor. [Lordly Caliber downloads](https://github.com/rempred/LordlyCaliber/releases)

In **Tools → Agents**, copy the **CLI path**. Paste it into a message like this for an agent that can run commands on this computer.

> Run `[CLI path]` with `--guide` and tell me what I can edit.

That first command works before you choose a ROM. When you’re ready to edit, give the agent your source ROM path. To continue an existing project, save it first and include the path to the project file.

> Use my ROM at [ROM path] and my project at [project path]. Save your edits as a new .lcproject and tell me where to open it.

### Example prompts

- **Inspect a project.** “List the edits and code patches in my saved project. Which edits still match, and what does each enabled patch change?”
- **Edit several classes.** “Increase the base strength of Knight and Berserker by 10 each. Show the old and new values, then reopen the exported ROM and check both classes.”
- **Assign a healing effect.** “Make Heal Leaf restore 47 HP and give it the Revive Stone animation. Reopen the exported ROM and check the amount and all four animation frames.”
- **Edit a story decision.** “Find the Wentinus choice between following orders and making peace. Show me the two outcomes in game order. Change the wording of the peace choice to ‘Seek a truce’ and keep its original outcome.”
- **Create artwork.** “Import my shield PNG into the Art Library, assign it to the equipment item I specify, and export a preview of the icon as it appears in the rebuilt ROM.”
- **Write a C patch.** “Make Heal Leaf restore half the target’s missing HP, rounded down and capped at 60. Check both the menu prediction and the actual heal. Save the editable C source in the project.”
- **Work with assembly.** “Walk me through the shipped actor-identity example. Build the assembly predicate and its C caller together, show which instructions changed, and save a project where I can toggle each patch.”
- **Check an export.** “Start a fresh project from my source ROM, export it with zero edits, and compare the files byte for byte. Report their SHA-256 hashes.”

Choose **Load project file** next to **Load ROM** to open the returned JSON file or archive. **Tools → Agents** shows its recorded edits and **Project code patches** opens the editable patch source.

## Report problems with the CLI or editor

Tell the user as soon as you notice an error or inconsistency, even if you find a workaround. This includes a CLI command that disagrees with the guide, a desktop control that behaves unexpectedly, or a preview that differs from the saved project or exported game. Explain what you observed and how it affects the user’s task.

Draft a short feedback report. Include the editor or CLI version, the command or editor steps, the expected and actual results, and any workaround you verified. Use placeholders for private file paths. Show the draft to the user and ask how they’d like to share it.

> Would you like to post this report in Discord yourself, or do you approve me filing it as a GitHub issue?

If the user chooses Discord, give them the text to post. File the report on GitHub only after the user explicitly approves that report, then give them the issue link. [Lordly Caliber issue tracker](https://github.com/rempred/LordlyCaliber/issues/new)

## Discover and edit

The same tab and **File → Review project changes…** show CLI activity saved in the project folder or sharing archive. Use `{"op":"agent.edits"}` to read it as JSON and `{"op":"changes"}` for the changelog. It records effective project changes, including bulk field edits, source patch changes, and edits made in the CLI desktop workspace. Reads and repeat saves create no entries. Individual fields and patches have their own status. Compound edits compare the saved section as a whole. A later edit can replace or revert a recorded value. An older project can still list its custom patches even when it has no attribution history. To retain source and activity, save the project as well as exporting the game.

Run `LordlyCaliber-cli --guide` to read this guide in a terminal, including before opening a ROM. The install includes the [game-code reference](agent-game-code.md) and a [runnable C and assembly example](agent-patch-example.py). With a ROM open, the `read` command accepts their paths under `reference/`. Search the supplied documentation by gameplay terms with `{"op":"lookup","query":"actor identity"}`. Each hit gives its file, line and passage. Search matches all supplied words within the same passage.

Run `LordlyCaliber-cli --capabilities` to list operations. Pass `--commands` a UTF-8 JSON file containing an array, or use `--commands -` to read the array from standard input. The result at each index in `results` belongs to the command at the same index. Add `"page":{"start":0,"limit":20}` to a list command to receive its total and one page. A failed command identifies its index and exits before writing a project or ROM.

Here is a PowerShell example that sets two classes' base strength to 300, keeps all other fields, saves a sharing archive and exports the ROM. Set `$rom` to your own input path.

```powershell
$cli = '.\LordlyCaliber-cli.exe'
& $cli --capabilities
@'
[
  {"op":"records","table":"classes","ids":[5,6],"fields":["strBase"]},
  {"op":"set","table":"classes","ids":[5,6],"values":{"strBase":300}}
]
'@ | & $cli --rom $rom --commands - --save-project .\TwoClasses.lcproject --output .\edited.v64
```

Add `--project <folder-or-archive>` when editing an existing project. `fields` lists record keys, labels, ranges and choices. `domains` lists the names supported by `domain` and `domain.set`. Read a domain, modify its JSON, and send the updated value to its existing setter. Some domain documents store edits relative to retail. Use `records`, `scene`, `text`, `icon`, the ROM inspection commands or a desktop session to inspect the underlying content. `manifest` and `apply` exchange complete native project documents. `changes` returns the editor's change report. `uses` finds project references. `issues` reviews export problems. Pass a returned location to the workspace `navigate` command to open the corresponding editor.

## Find game code and assets

| Operation | What to pass | Result |
| --- | --- | --- |
| `files`, `read` | Optional filename query and content substring, then a returned path | Packaged game C, assembly, headers, resource catalogs and editor references. |
| `owners` | Optional function, source path or linked symbol substring | Accepted C source and header paths, bindings, owned code and data ranges, addresses and original digests. A single source can produce several functions. |
| `regions`, `region` | Filter by name, kind or containing normalized ROM offset, then read the region by name | Physical Rev 0 source ranges covering the cartridge, including each upstream assembly range. Listings reconstruct the exact words from the supplied ROM. CPU code includes decoded instructions. When the location in RAM is ambiguous, branch operands use relative offsets and jumps show how the game constructs the address. |
| `assembly`, `bytes` | Normalized ROM offset and byte size, plus runtime RAM address for disassembly | Instruction words and decoded mnemonics, or lossless binary bytes. Unknown instructions retain their original word. ROM offsets use big-endian z64 order regardless of the input file's byte order. |
| `assets`, `asset` | Optional filename query, then the archive header offset | Named game archives and their decoded bytes. Unnamed resources remain addressable with `regions` and `bytes`. |
| `icon`, `use.animation` | An item or consumable ID | Current project artwork as PNG, including the four frames of a consumable use animation. |
| `toolchain`, `--build-plan`, `--build-dir` | The current project | Authenticated tools and paths, active compiled module source, header names, symbol bindings and RAM addresses. The build directory retains compiler inputs, assembly listings, linked ELF files and extracted binaries. |

Binary responses contain `base64`, `size` and a SHA-256 digest. Instructions and archive reads use the supplied ROM. To inspect the final installed bytes, export first and pass the emitted file as `--rom`.

## Author custom game code

A `patch.put` operation stores the source in the project. `patches` lists it, `patch.enable` toggles it, and `patch.remove` removes it. The desktop's **File → Project code patches…** shows the source and lets the user change which patches are enabled. Both front ends compile enabled project source during export.

Choose the target that corresponds to the intended behavior.

- **Matched function.** Supply a unique patch `id`, `owner` (the function symbol), `language` (`c` or `asm`) and `source`. The CLI resolves a single-entry, single-code-range owner against the accepted catalog and captures its expected bytes, bindings and headers. C uses the owner's function name as its entry by default.
- **Explicit code range.** Supply numeric `offset`, `address`, `size` and `expectedSha256`. Supply `entry` for C. Optional `bindings` map symbol names to game RAM addresses. Optional `headers` map relative include names to their source text. The replacement must fit the declared, word-aligned allocation. For a larger implementation, use a relocated build module or provide an independently verified executable allocation and an explicit call site. Only select a virtual address where the game actually loads that code.
- **Relocated C module.** Run a build plan for the active project. Choose a module and use its `name` as `module`, its `originalSourceSha256` as `expectedSourceSha256`, and its edited `source`. The ordinary feature builder supplies placement, headers and bindings, and connects the new entry addresses to its callers. Export checks that every original public entry still exists. An enabled module absent from the project's build produces an error.

The following PowerShell saves a build plan and all compiler products. Use the same source ROM and project when applying the resulting module patch.

```powershell
& $cli --rom $rom --project .\MyProject --build-plan --build-dir .\builds --report .\plan.json
$module = (Get-Content .\plan.json -Raw | ConvertFrom-Json).modules |
    Where-Object name -eq 'item_tree_execute' | Select-Object -First 1
```

The item-tree module is active when a project uses custom consumable effect trees. Its `lc_fixed_heal` entry predicts healing in the battlefield menu. `lc_tree_prepare` handles eligibility and prepares the amount, and the application code consumes that prepared result. A custom heal formula needs to agree across all three.

Assembly source is an instruction body assembled with the bundled big-endian MIPS assembler. It supports labels, branches, data words, alignment and symbol bindings. Headers for C must be carried in the project or supplied by the original module. Project sources compile for the game. Handwritten instructions belong in an assembly patch. A conflicting ROM change, unresolved symbol, oversized result or missing module stops export. An explicit range patch may name an earlier export step in `after` to compose with that step over its declared range. The expected digest must match the bytes produced by the earlier step.

Keep the project archive to retain editable source. The exported game contains the linked instructions. A desktop preview that implements a behavior separately can still differ from a custom game function. Use the exported game's relevant path to verify behavior.

## Operate the desktop workspace

Begin with `{"op":"pages"}`. Each entry has a path of tab titles. Use the exact array with `{"op":"openPage","path":["Campaign","Scenarios","Settings"]}`. A save, selected record, or advanced view can enable additional pages. Repeat `pages` after opening a document or changing modes. `controls` discovers the active tools and dialogs. Session IDs can change after an edit rebuilds a view. Resolve the control again if its ID expires.

For an independent set of desktop preferences, add `--settings-dir <folder>` to `--workspace`. For technical cutscene tabs, open **Stage → Playback details…**, enable **Show technical tabs**, and close the details dialog. In Scenarios, **Orders → Advanced routes and conditions** reveals the route pages. In Save Editor, load a save to enable the army, character, equipment, consumable and game-state pages.

| Workspace | How to find and verify an edit |
| --- | --- |
| Classes and Items | Use `fields` and `records` for bulk values. In the desktop table, `items` returns column headers and `cell` reports the raw value, whether it can be edited, and its viewport coordinates. Use `edit` with the returned row path and column. Inspect the same record after opening the exported ROM. |
| Attacks | Select an attack in `attackList` and inspect its calculation, power, targeting and class-action controls. Use control IDs to distinguish repeated fields. Reopen the exported ROM and select the same attack. |
| Consumables | Select an item before opening its effect and animation dialog. For conditional trees, `items` accepts a parent row path. Use `cell` for child count and drop coordinates. Read back the assignment by reopening the dialog on the exported ROM. |
| Campaign | Navigate by full tab path through World Map, Scenarios or Storyline. Discover the selected scenario, shop, event, scene or map object before changing its controls. The flowchart and stage controls also accept pointer and keyboard events. |
| Art and Animation | Open the required icon, portrait, sprite, pose or animation page. Use `image` for a preview, `pointer` for painting and `file` for import or export. A cell's `viewport`, `x` and `y` identify the target for a click or drop. |
| Tools | Settings are grouped into tabs. `inspect` on a spin box returns its value and bounds. Reopen the same Tools page on the exported ROM to check the installed setting. |
| Save Editor | Open a user battery save, or create one with `mission.seed`. Select a slot and page, make the edit, and use **Export edited save**. Reopen the resulting save against its ROM to read the value back. Project Save and ROM Export each operate on a separate document. |
| Testing | `missions` lists choices in the input ROM. `{"op":"mission.seed","choice":0}` returns the same seeder's battery bytes in `base64`. Decode to a new `.sra` file. To use a saved army, supply its base64 bytes as `army` and optionally a zero-based `slot`. For current project edits, export first and pass that ROM to both commands. The desktop Testing page also offers its Project64 installation and launch controls when a user has Project64. |
| Art Library | Create a named asset or use **Copy to Art Library** in an artwork editor. Save the project to keep its editable frames and layers. Assign the asset to a game resource to see its artwork in an exported ROM. |

For an internal drag, get the source view ID from `inspect`, get the destination coordinates with `cell`, then send `{"op":"transfer","from":12,"index":[0],"id":18,"x":60,"y":40,"action":"move"}`. Here 12 is the source view, and 18 is the destination viewport. The source supplies the real MIME data and the target's own drop handler decides whether to accept it.

For multi-file import, pass `paths` as an array of filenames to an open multi-file chooser. A single-file chooser accepts `path`. To identify a frame or inspect an icon-only row, pass `"decoration":true` to `cell` to receive its thumbnail as a base64 PNG if present.

The consumable domain's version 1 holds retail effect settings. When authoring a conditional `trees` entry through `domain.set`, use version 2 or preserve the higher version already returned by the project. The assignment dialog and tree editor handle this automatically.

`set` commits a text or number field as a completed desktop edit. Use `"readBack":true` to receive the control's resulting value, or `"commit":false` for an intermediate draft. For example, scenario day length accepts 25-point increments. Entering 117 produces 125. A table selection can add a row with `"extend":true` when the view supports multiple selections. Use `menuItems` on a menu bar, open menu or menu button to discover its actions. For a separate modeless window, `windows` lists visible window IDs and `useWindow` selects one. An open modal dialog or popup owns the next interaction until it closes.

Run `LordlyCaliber-cli --rom <rom> --project <project> --workspace -`. The project argument is optional. The process opens its own offscreen editor, emits one ready message and accepts one JSON object per line. Each command returns a JSON reply. Its dialogs, previews and Undo stack belong to this session. Close stdin to end it. Use `--workspace commands.jsonl` for a repeatable file of commands.

```jsonl
{"op":"items","name":"workspaceNavigation"}
{"op":"select","name":"workspaceNavigation","text":"Consumables"}
{"op":"controls","query":"consumablesTable"}
{"op":"select","name":"consumablesTable","index":[1]}
{"op":"activate","name":"assignConsumableUse"}
{"op":"items","name":"assignmentEffect"}
{"op":"select","name":"assignmentEffect","text":"Restore HP"}
{"op":"set","name":"assignmentAmount","value":43}
{"op":"controls","query":"Save"}
```

Use the returned Save button's numeric `id` with `activate`, then send `{"op":"save","path":"MyProject.lcproject"}`. Each open dialog becomes the active root. A reply with `state` set to `awaitingDesktopInteraction` means the action opened another desktop interaction. Inspect its controls to continue. `controls` returns IDs, names, labels, geometry, visibility through the current window and enabled state. IDs last for that object in that session. `items` pages through a combo, tabs or view model. A tree path uses zero-based row ordinals and an optional column. `expand`, `select` and `edit` operate on that model. `inspect` returns a control's state. `image` returns a PNG of a control or the current window.

Use `focus` and `key` for keyboard commands, `pointer` with `press`, `move`, `release`, `double` or `context` for canvas tools, and `wheel` with angle deltas for scrolling. Coordinates are local to the target widget. Item views and canvases commonly receive input on their viewport. Pointer and wheel commands accept `modifiers` from `shift`, `ctrl`, `alt` and `meta`. `drop` accepts file `paths`, `text`, or `mime` and `base64`. `file` selects a `path` in an open file dialog. `reject` cancels a dialog. `clipboard` reads or sets text. For long-running actions, inspect the active controls until the relevant result is ready.
