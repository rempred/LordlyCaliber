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

1. Select an Actor to change its initial position, appearance, or facing.
2. Use **Projected clips** to edit movement, poses, and existing dialogue actions.
3. Use **Hold**, **Move**, and **Set pose** to insert supported native actions.
4. Preview the change, then use **Save Project** to retain the editable scene.
5. Export the ROM and test the scene through its normal game entry.

**Create replacement from this template** starts a named replacement of the selected scene.
It inherits the template's Actors, resources, and action sequence.
Export replaces that scene at every existing game trigger.
The Project name does not add a new game trigger or resource-selector entry.

Existing **Native dialogue text** fields write the shared dialogue archive.
Keep the text's `@` control tokens and use printable ASCII characters.
Changing an entry affects every scene that uses it.
Edits to different entries in the same archive combine into one resource.
Conflicting edits to the same entry stop export with an explanation.
The **Speak (storyboard only)** action previews a new dialogue action but does not export it.

Larger Director streams and dialogue archives use the shared Cutscene allocation area.
The current allocation budget is 188 KiB, with up to 144 relocated resources.
Each decoded Director stream or dialogue archive must fit within 64 KiB.
An exported ROM can be reopened, edited, and exported again.
Load a saved Project against the same base ROM used to create it.

Patch ROM and the experimental Rebuild Editor use the same cutscene exporter.
Rebuild retains its existing native components and adds the edited scene resources.
Preview timing, effects, and some game-dependent starting states remain approximate or unsupported.
Unsupported edits show a storyboard-only or research status and cannot silently enter the exported ROM.
