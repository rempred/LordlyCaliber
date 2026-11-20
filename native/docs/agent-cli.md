# Using Lordly Caliber from an agent

Use the portable editor and your own game ROM. The CLI runs locally. Its structured commands and the desktop share the project model, compiler and ROM export checks. An independent desktop session gives agents access to dialogs, preview controls, file imports, drawing tools, Undo and Save. A running graphical editor can open the resulting project folder or sharing archive.

## Discover and edit

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
