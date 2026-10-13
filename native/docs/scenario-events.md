# Scenario event instructions

Scenario → Events follows the selected runtime scenario. The script selector
separates story missions and Area Investigation. It identifies rows from the
retail scenario-initialization sequence; it does not assume event row equals
scenario key. The event directory remains the game's original directory.

The readable list groups reachable commands by the six scheduler groups and
their handler entry points. Mission entry (group0) and completion (group5) have
descriptive labels; remaining groups keep their numbers. The default view shows
flag instructions and cutscene calls. Show all commands includes register setup,
branches and commands whose meanings are not yet labeled. Branch destinations
are explicit instruction positions; the list is not a promise of linear execution.

Select a flag step, choose the flag, and select Set/Clear or Check ON/OFF.
Apply creates one Undo step. A check changes which Boolean result takes the
existing branch; its other result continues after the check. Go to branch
destination reveals that command. Restore loaded flag instructions restores the
input ROM's flag edits for this row without changing its cutscene assignments.
These edit script behavior, not current flags in a saved campaign.

Names are deliberately conservative. Flags1..4 are chapter progression flags,
consumed by the save's consecutive-chapter calculation and written by event
rows110/111/112/114. Flag102 is set in Tenne's completion handler (row4, decoded
offset0x2F0: 6866 6901 7160). Other flags remain Flag N. Selecting a different
flag does not alter any other reference to the old flag.

## Preservation and export

EventScript recognizes six-byte immediate Boolean writes (68 flag, 69 value,
71 60) and checks (69 flag, 60 61, AC/BC relative destination). A compound with
another branch or handler entry into its middle is read-only. Flag IDs remain
0..255. Unknown data, instruction widths, handler tables and branch offsets
remain intact. This page edits existing supported steps; it does not insert,
delete, reorder, or retarget arbitrary event commands.

SceneEvents project version2 adds sparse flag edits keyed by row and original
instruction offset. Empty flag sets retain version1. Export compiles the edited
bytes through the existing data compiler, composes launch bindings/stubs, and
uses LC05SCPT version5 metadata when flags are present. The allocation reader
validates flag controls against preserved retail programs and checks the exact
decoded output including launch changes. Flag-free allocations continue to use
version4; earlier allocation versions remain readable. Original event resources
and unrelated trailing data are retained. Legacy scene import keeps flag edits.

Tests and current validation results are recorded in the parent task note
docs/Plans/task-logs/scenario-event-editor-20261013.md. No new full-game execution
claim follows from desktop or export/readback checks.
