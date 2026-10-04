# Native scene authoring

The Commands page edits complete Director programs in the project. It supports
operand changes, insertion, removal, restore and undo. Valid values enter the
project immediately; invalid input blocks navigation and save/export. Signed
decimal and hexadecimal words preserve the native bit pattern. Actor position
labels follow SceneActors' qualified conversion from milliunits; camera labels
come from the existing C-derived grammar. The grammar generator owns those labels.

Edits belong to physical resources, so selector aliases remain shared. Framing
uses the maintained-C-derived command widths. The terminal command and resource
class remain intact. Unknown operand values and untouched commands are preserved;
this is a source-command editor, not a complete semantic validator for every game
service. Export checks changed static scene/continuation selectors, dialogue entry
references (including project additions), labels using the accepted C word-wise
scanner, and class-4/5 background groups and member bounds. Missing targets reject
with their scene key and command word. Existing unresolved references remain intact;
launch substitutions still require their native execution context. The reference
check and public export regression pass together (3,449 ms).
Background resolution and dialogue navigation consume the edited program.

Project manifests store changed programs as exact big-endian bytes. Export compiles
them with the bundled N64 toolchain, compresses LZSS resources and updates original
Director/continuation references. The 96-byte LC05SCPT descriptor at z64 0x27C4700
owns dynamically allocated space above 40 MiB, bounded by the 64 MiB cartridge
limit. Each member records its exact directory pointer words, including shared
aliases. Parent event references are included if they share the same resource.
Header/payload CRCs, nonoverlap and reference checks precede reuse. Unknown bytes
after each original compressed stream survive. Growth around foreign data appends
elsewhere and clears only the previous owned allocation. No runtime hook is added.

The engine check preserves all 1,498 original programs, compares an operand edit
against an exact expected byte change, checks insertion/removal/restore and invalid
terminal rejection, saves/reopens a project, and compiles/exports/reopens a ROM.
It also checks checksum stability, repeated rebuild identity, trailing bytes,
growth, foreign data, damaged ownership and shared aliases. The extended check
retains a continuation while a different member grows (2,705 ms). Candidate output
is under the parent's Test ROMs directory; the master ROM is read-only.

The UI check exercises decimal/hex edits, undo/redo, multiple invalid fields,
insert/remove, restore, navigation blocking and actual background resource
selection after a group edit (427 ms focused; 2,183 ms with source-navigation and
main-window dialogue guard regressions). The command-page screenshot was inspected.
These checks establish editor/rebuild behavior, not full-scene gameplay. Event
binding, exported-scene deletion, full browser scene conversion and final scene
playback remain active work. The dynamic scene at z64 payload 0x01FA8BF4 has no local camera command, so
camera editing coverage uses a class-five stream that defines its own camera.


## New scenes

The UI creates blank class-five scenes from a selected current-ROM setup template,
duplicates complete scenes, renames them and removes project additions with undo.
A blank scene retains the template's group, both cameras and native exit sequence;
it does not manufacture actor or army context. Users edit its subsequent commands.
Typed SceneId values distinguish native resource keys from authored UUIDs. No
fabricated ROM key is assigned to a scene before it has an exported allocation.

Each new scene takes an empty, unclaimed top-level selector. The generated retail
catalog reserves 1,548 selectors; the vanilla directory has 145 available rows.
Clearing a retail pointer in another tool does not make that row unclaimed. UUID,
name, selector and source/current program survive project save and undo. Scene
references may target additions in the same project. Native references to an
exported authored resource resolve to that same authored identity on reopening.

LC05SCPT version two adds an owned UTF-8 JSON identity manifest between the pointer
lists and compressed members. Header words at offsets 28/32 identify its relative
offset/length. Version-one allocations remain readable and migrate on rebuild.
The manifest is covered by the payload CRC, and every authored scene exclusively
owns its recorded selector. Programs compile during export; both native and new
resources receive decoded readback. Retail names survive relocation through their
stable catalog selectors. Removal of an already-exported scene remains unavailable
until its event and scene references can be resolved as one transaction.

Creation/clone/rename, save/reopen, export/reopen, ownership collision and subsequent
edit checks passed with existing export/reference checks (9,114 ms). The explicit
version-one migration and authored export checks passed together (3,739 ms). UI
creation, duplication, rename, removal, command editing and undo passed with the
catalog/command regressions (2,230 ms); the new-scene screenshot was inspected.
Final checks for relocated retail names (2,736 ms) and tab-switch invalid-input
protection (412 ms) pass. No in-game launch is claimed by these checks.
