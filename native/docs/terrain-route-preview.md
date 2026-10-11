# Terrain route preview

Selecting a squad in Map draws a gold planned path. The sidebar names the movement
type and explains an unavailable path. This follows starting orders, not a live
emulator state. Dashed connections retain their existing order-graph meaning.

`game/scenarios/route_path.c` adapts accepted `func_00107E38` and `func_00107D60`
from `accepted/library/src/lib/movement_107d60_107e38.c`. The library manifest pins
the October 11 accepted evidence (fingerprint `1E95BD03...`). The implementation
retains neighbor order, directed transition rejection, blocked-start diagonal
rules, the destination cost exception, cardinal/diagonal weights 10/14, strict
improvement and queue ordering by unsigned cost then cell index. A bounded host
heap and arrays replace the original tree, pointers, allocation and frame yield.
This runs in the editor and does not patch the game's pathfinder.

## Inputs

US revision 0 descriptor 7 maps ROM z64 `101CE0..145210` to RAM
`801AD5A0..801F0AD0`. Its initialization at ROM `102DA0` copies the 8 by 32
big-endian halfword cost table from ROM `13FA58` (RAM `801EB318`) into BSS
`801F36D8`. The 32 by 32 byte transition table is ROM `13FC60` (RAM `801EB520`).
Do not derive a ROM address directly from that BSS destination. The preview reads
these tables from the loaded ROM and checks the pathfinder/initialization code
identities before using the adaptation.

`MapEdits::movementTerrain` supplies the native 64 by 64 grid both to the preview
and export. Its existing fine-grid majority vote, untouched coarse cells,
fortified-cell preservation and boundary behavior remain shared. Squad movement
comes from current class-record byte 44, Soldier cohorts, native source-specific
immobility rules and priority bytes at ROM `5CEBC`. Movement groups use their
leader's route and aggregate movement type. Edited site positions resolve named
waypoints. Fixed destinations are followed until a wait, loop, conditional gate,
dynamic order or end. Unknown or invalid data produces an explanation.

## Limits

The gold line joins coarse path-cell centers. The game's fine-cell refinement and
spline smoothing are not reproduced. Live pursuit, evasion masks, collisions,
day/night class changes, group follower offsets and scripted gate/map transitions
can change movement. Keys 13 and 17 warn about story-controlled terrain passages.
No path is invented for a dynamic target or unresolved starting order. Previewing
a partially reachable multi-leg order retains the reachable part in orange.

## Checks

`route_preview_tests` compares complete paths and costs with original ROM MIPS
instructions, including original queue helpers: ties, costs, directed transitions,
impassable cells, destination exceptions, diagonal guards and masks. Private ROM
checks also paint a river, compare ground and flying routes, validate the exported
grid, checksum/reopen a candidate ROM and restore saved edits. `ui_tests
scenarioTerrainRoute` checks selection, order refresh, hiding and a screenshot.
Use `LORDLY_TEST_ROM` for private checks; the ROM is read-only and never bundled.
