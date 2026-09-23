# Editing cutscenes

Load a US Rev 0 ROM and choose a scene in Cutscene Studio.
Supported scenes build their starting state from the ROM and play without an imported profile.
The Stage identifies simulated dialogue timing and other preview approximations.

Class-five scenes can start directly when their stream supplies a scene group and both cameras.
This includes First Cutscene Flashback, Opening Title, and Traveling Cutscene 3.
Party scenes use a sample unit selected from class requests in the stream.
Class data comes from the ROM. Missing playthrough names use preview names.
The sample unit supplies party-dependent Actors; it does not reconstruct a saved game's cast.
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

1. Use **Choose scene** to select a scene. Use **Use as template** to name a replacement.
2. Select an Actor on the Stage or beneath it to change its appearance and facing.
3. Use **+ Actor** to add a cast member. Its art and animation controls open beneath the Stage.
4. Use **+ Add action** to add movement, pose, speech, wait, effect, camera, appearance, or disappearance actions.
5. Select an action card to expand its controls. For movement, **Choose destination on Stage** accepts a Stage click.
6. Preview the change, then use **Save Project** and **Export ROM** in the app toolbar.
7. Test the exported scene through its normal game entry.

The Stage lets you select an Actor. Dragging an Actor adds or edits a timed pose at the playhead.
Use the starting position fields to change a native Place command when one exists.
The default workspace contains the Stage and one action list. Active cards are highlighted during preview.
**Advanced** contains the timeline, runtime diagnostics, background settings, and playback inputs.
Action cards retain the template's native command structure. Drag reordering and **Together** groups are not available yet.

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

**Speak** exports a new box when the scene contains a native dialogue command and an insertion point.
The new box gets its own archive entry, so editing it leaves existing lines intact.
Edit its speaker, text, and **Delay before opening** inside the selected action card.
Use printable ASCII for the speaker and text. New lines are supported.
The editor wraps long lines, adds page breaks, and waits for A before closing the box.
The delay counts native Director updates; preview seconds and box duration are approximate.
Scenes without a usable native dialogue command or insertion point still create a preview-only box.

Larger Director streams and dialogue archives use the shared Cutscene allocation area.
The current allocation budget is 188 KiB, with up to 144 relocated resources.
Each decoded Director stream or dialogue archive must fit within 64 KiB.
An exported ROM can be reopened, edited, and exported again.
Load a saved Project against the same base ROM used to create it.

Patch ROM and the experimental Rebuild Editor use the same cutscene exporter.
Rebuild retains its existing native components and adds the edited scene resources.
Preview timing, effects, and some game-dependent starting states remain approximate or unsupported.
Unsupported edits show a storyboard-only or research status and cannot silently enter the exported ROM.
