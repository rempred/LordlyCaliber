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
service. Context-sensitive references still need their native execution context.
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
These checks establish editor/rebuild behavior, not full-scene gameplay. New scenes,
event binding, full browser scene conversion and final scene playback remain active
work. The dynamic scene at z64 payload 0x01FA8BF4 has no local camera command, so
camera editing coverage uses a class-five stream that defines its own camera.
