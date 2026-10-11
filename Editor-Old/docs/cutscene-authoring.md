# Editing cutscenes

Load a US Rev 0 ROM and choose a scene in Cutscene Studio.
Supported scenes build their starting state from the ROM and play without an imported profile.
The Stage identifies simulated dialogue timing and other preview approximations.

Class-five scenes can start directly when their stream supplies a scene group and both cameras.
This includes First Cutscene Flashback, Opening Title, and Traveling Cutscene 3.
Party scenes use a sample unit selected from class requests in the stream.
Class data comes from the ROM. Missing playthrough names use preview names.
The sample unit supplies party-dependent Actors; it does not reconstruct a saved game's cast.
Event scenes on the world map also take a second unit from the game state: the unit the player met.
The preview rebuilds that unit from the faces of the dialogue boxes its Actors own, so Jeal appears when Jeal speaks.
Those Actors are named by class and marked **(inferred)**; their members and formation are not the saved game's.
Roster Actors created at launch are named by class once the scene has played, for example Archer (Liedel).
Automatic dialogue choices confirm the first option. Returns to sample party positions use approximate movement.
Actor-derived dialogue portraits, roster rebuilding, leader selection, and class-seven playback are supported.
Class-six chapter titles display ROM artwork with a smooth reveal. Their timing follows the image widths.
Hugo's Report is labeled as an interface resource. Its People, Events, Miscellany, and Tips menus are outside timed cutscene playback.
Event-linked scenes can run their predecessors to obtain shared Actors, dialogue, and background.
The selected timeline starts after this setup. Opening dialogue checks select entry; event timing remains approximate.
Ending streams can also inherit the scene that calls them. The preview selects the branch leading to the chosen ending.
Caller-selected battle terrain uses sample environment zero in previews. Export preserves the original terrain request.
Sepia transitions and highlighted-Actor fades now have visual previews. Their raster effects remain approximate.
ROM preview frames retain drawing data without copying the interpreter's working memory into each frame.
Unresolved event branches and other external scene dependencies can still show a playback boundary.
Some party-dependent casts remain incomplete with this sample unit.

1. Choose a scene in the scene list, or use **+ New scene** to start an empty one of your own.
   **Wipe scene…** empties an original scene so you can rebuild it.
2. Select an Actor on the Stage or beneath it to change its appearance, facing, and size.
3. Use **+ Actor**, in the cast row beneath the Stage, to add a cast member. Its art and animation controls open there too.
   Move, Pose, Appear, and Disappear act on the selected Actor, so a new or wiped scene needs an Actor first.
4. Use **+ Add action** to add **Move**, **Pose**, **Speak**, **Wait**, **Camera pan**, or **Fade**.
   **More actions** holds Appear, Disappear, Effect, and **Choice**.
5. Select an action card to expand its controls. For movement, **Choose destination on Stage** accepts a Stage click.
6. Use **Event launch** to choose which place in the game's events starts the scene, and **Background** to choose what it is set against.
7. Preview the change, then use **Save Project** and **Export ROM** in the app toolbar.
8. Test the exported scene through its normal game entry.

The Stage lets you select an Actor. Dragging an Actor adds or edits a timed pose at the playhead.
Use the starting position fields to change a native Place command when one exists.
The default workspace contains the Stage and one action list. Active cards are highlighted during preview.
Selecting a card pauses playback and moves the Stage to the moment that action runs, using the executed scene's own timing.
**+ Add action** then inserts at that same moment.
A Wait placed inside one of the game's timed sequences becomes a gate on that sequence's counter, and the later steps of the sequence move back by the same amount.
Action cards show start and duration in preview seconds. The game stores them as whole update steps at thirty per second.
**Advanced** contains the timeline, native update counts, opcode views, runtime diagnostics, background settings, and playback inputs.
Action cards retain the template's native command structure. **Together** groups are not available yet.
Added actions can be reordered: drag a card to a new place in the list, or open it and use **Move earlier** / **Move later**.

The preview layers the scene the way the game does: each transform channel is one pass, drawn as background, then that layer's animated scene sprites (`0x46`/`0x63`), then that channel's Actors. When a scene installs the automatic channel divider (`0x5F`), Actors are re-sorted every tick by which side of its X/Z line they stand on, so a character in front of a desk sprite draws over it while one behind it draws under it.
A moved action takes the start of the card it lands before and keeps a safe insertion point there, so the exported scene runs the actions in the order the list shows.
The scene's own native commands keep their places; added actions move around them.

Every Director scene in the ROM accepts these actions, including the 1,438 scenes without a reviewed timeline.
A new action enters the scene in front of the nearest complete native step at the preview position.
It never lands inside a native wait, condition, or dialogue handshake, before the scene's first setup command, or in front of a branch target.
Existing Move, Pose, Camera pan, Place, and opacity commands in those scenes are editable; other native commands are kept byte-for-byte.
The editor's table of native commands is checked against the game's own command parser every time the catalog loads.
That parser is the byte-exact C recovered in the decompilation (`OB64 Decomp/src/lib/func_00284288.c`); `tools/generate-cutscene-director-parser-model.js` reads it into `cutscene-director-parser-model.js`, and a disagreement on any command's width or query form stops the catalog rather than mis-tiling a scene.
Some commands act only in one director mode: the game still reads their words in other modes but does nothing.
Such cards say **No effect in this scene** in their summary; 991 retail `0x1C` transform-channel assignments in mode-2 scenes are of this kind.

**Fade** compiles to the game's full-screen color overlay command.
Choose fade to black, fade in from black, fade to white, or fade in from white, and set its duration.
A fade in starts fully covered and reveals the scene; a fade out keeps the current cover level as its starting point.
Fade, Wait, Camera pan, Move, and Speak with a chosen avatar were watched in Project64 in the opening ceremony on 2026-09-26.
In original scenes, no action can be placed directly before the scene's first wait, before the first wait of a later segment, or at the very end.
New and wiped scenes are different: they take actions in front of their native exit, which is protected by a barrier.
There the game's failure scan finds no barrier and restarts the scene from the top each update until it freezes.
If a preview ever shows "The game would restart this scene from the top" in the toolbar, move the action to a later insertion point.

## New scenes

**+ New scene** creates a scene of your own. It never replaces an original scene.
It gets its own identity and one free Director selector row (the retail ROM has 145, numbered 319 and up).
It starts empty: its setup and native exit come from a traveling cutscene, and its first Actor recipe makes new Actors visible at a sensible size.
Name it in the toolbar. **Delete scene…** removes it; one already in the loaded ROM is removed on the next export, and its row becomes free again.
A new scene is not played by the game until an event launches it (see **Event launch**).
New scenes are saved in the Project and in the exported ROM, and they come back when that ROM is opened on its own.

## Event launch

**Event launch** lists the places in the game's event scripts that start a cutscene.
Search by event row, site, mission, or scene name, then **Assign** a site to the scene you are editing.
Assigning a site back to the scene it originally launched removes the assignment.
Sites marked **Shared program** belong to an event script that several missions run.
Export keeps each original event script and writes a relocated copy with only the launch changed.
Long launch sites change two bytes; short ones jump to a small stub added to the copy.
The preview plays an assigned scene with that site's context, and scenes that followed the site follow the new scene.
Removing every assignment in an event script restores the original.

## Background

**Background** shows the backgrounds the scene's own background command can load, as a grid of pictures.
Select one to use it; **Restore original** returns the scene's own.
Export changes only that command's number, so the rest of the scene is untouched.
- Room scenes (terminal classes 4 and 5) choose one of the game's scene groups. Every layer of the group comes together.
- Other fresh scenes (classes 1 and 8) choose one of 63 pictured environments; rows 63 to 79 are empty and are not offered.
  The environment's foreground pieces, drawn over the Actors, and its scenery follow the same choice.
- New scenes use the room route; a wiped scene keeps its own route.

Some scenes cannot choose, and the drawer says why:
the game picks the background from the battlefield, the scene keeps the background of the scene before it, or it has no background command.
Moving to a different background does not move any Actor; adjust positions yourself if the new setting needs it.
The picture choice under **Advanced** is still a visual preview only.

## Preview cast

Eighteen scenes (event rows 66 and 67, such as 01FA8BF4) take up to five story characters from the player's army.
The game fills their identities, classes, portraits, and lines when the scene starts; their positions are the scene's own.
The army is not in the ROM, so these scenes show **Preview cast**: five characters the preview uses, Destin, Debonair, Gilbert, Aisha, and Saradin by default.
Change any of them, or choose Fighter for the ordinary filler the game uses when the army has fewer story characters.
The preview cast is never exported: the scene keeps its placeholders and the game still chooses from the real army.
Their launch-chosen lines appear as cards that say the game chooses the line.
Imported playback inputs and known launch values take precedence over the preview cast.

## Wipe

**Wipe scene…** removes every Actor, line, and action from an original scene.
It keeps what the scene needs to start and end: its setup records, the camera pair of classes 4 and 5, and its native exit.
The confirmation names every selector row and launch site that plays the stream, because all of them play the emptied scene.
Rebuild it like a new scene: start with **+ Actor** beneath the Stage. New Actors copy the scene's own first cast member and size.
**Restore original** undoes a wipe made in this session.
26 scenes cannot be wiped, and the button explains why: 24 end by restarting the event that called them, row 542 ends the same way, and Hugo's Report is an interface.

## Order and timing in new and wiped scenes

Actions run in the order of the list.
**Move**, **Camera pan**, and **Fade** have **Wait until it finishes**, on by default in these scenes.
A Move then waits until the Actor arrives, using the game's own arrival check; Camera pan and Fade wait their duration.
With it off, the next action starts at once and the two run together.
Each card shows when it starts in the executed scene and what happens next.
The scene's native exit stops anything still running. The action list warns when that would cut an action short, and **Hold until finished** adds the Wait that prevents it.
A **Pose** changes appearance and keeps the Actor where it is.

## Choices

**Choice** shows the player two options. Edit both options on its card.
Other actions after it can be set to **Runs: After option 1** or **After option 2**; the rest run for either option.
**Preview this choice as option** plays option 1 or 2 for that choice; each choice has its own setting.
The exported scene encodes both branches with the game's own choice window and answer check. Both options play in the preview; game validation is still pending.
Reopening an exported ROM brings the choice back with its options and branches.

## Use as template

**Use as template** starts a named replacement of the selected scene.
It inherits the template's Actors, resources, and action sequence.
Export replaces that scene at every existing game trigger.
The Project name does not add a new game trigger or resource-selector entry.

Editing an existing entry's raw source writes the shared dialogue archive.
Keep the text's `@` control tokens and use printable ASCII characters.
Changing an entry affects every scene that uses it.
Edits to different entries in the same archive combine into one resource.
Conflicting edits to the same entry stop export with an explanation.
Dialogue cards and the selected-box preview show readable text without ROM control codes.
Open **Raw ROM dialogue source · advanced** to inspect or edit an existing entry's exact script.
The readable preview labels unresolved game markers; it does not replace the raw source.

**Speak** exports a new box wherever the scene has an insertion point.
It copies a dialogue window from the scene when there is one; otherwise it uses a fixed window at the bottom of the screen.
The new box gets its own archive entry, so editing it leaves existing lines intact.
Edit its speaker, **Avatar**, text, and **Delay before opening** inside the selected action card.
The avatar is chosen directly, as the original scenes do: **No face**, any class with a portrait, or the assigned Actor's own face.
A box copied from the scene's own dialogue window is placed near the Actor it is assigned to; the fixed window used when the scene has none sits at the bottom of the screen. The avatar does not depend on that Actor.
Use printable ASCII for the speaker and text. New lines are supported.
The editor wraps long lines, adds page breaks, and waits for A before closing the box.
The delay counts native Director updates; preview seconds and box duration are approximate.
A scene with no insertion point at the preview position still creates a preview-only box.
In new and wiped scenes a line starts in list order and has no opening delay.

Larger Director streams and dialogue archives use the shared Cutscene allocation area.
The current allocation budget is 188 KiB, with up to 144 relocated resources.
Each decoded Director stream or dialogue archive must fit within 64 KiB.
An exported ROM can be reopened, edited, and exported again.
Load a saved Project against the same base ROM used to create it.

Patch ROM and the experimental Rebuild Editor use the same cutscene exporter.
Rebuild retains its existing native components and adds the edited scene resources.
Preview timing, effects, and some game-dependent starting states remain approximate or unsupported.
New scenes, event launch assignments, Wipe, and choices are checked in the preview and by export readback.
They have not yet been confirmed in the game.
Background changes and the preview cast are checked in the preview and by export readback, and have not yet been confirmed in the game.
Unsupported edits show a storyboard-only or research status and cannot silently enter the exported ROM.
