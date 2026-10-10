# Battlefield maps and briefing artwork

The Scenario Map tab renders the current ROM's field geometry, textures, roads,
bridges and buildings. Its top-down camera uses the placement editor's world
rectangle, replacing the approximately aligned briefing image. Lighting is an
editor daylight approximation; water is at its initial phase, animated effects
are omitted, and unused/internal maps can report an omitted ground effect.

## Resource selection (US Rev 0)

All ROM addresses below use **z64 order**. The ROM reader normalizes v64 input
in memory; the master is never modified.

* Scenario row `0x654A0 + runtimeKey * 9`, signed byte `+3`: model/media ID.
* Model row `0x65710 + modelID * 12`: geometry selector at `+0`, scale at `+4`,
  render flags at `+8`.
* Geometry collection key `0x01E39DCE`. Resource envelopes are at
  `key + 0x594280`, with a big-endian length followed by the payload. The root
  contains child keys. Each child contains compact stored/LH5 members: mode
  `0x80`/`0x85`, little-endian stored and decoded lengths, then payload.
* Member 0 contains drawing wordcode, culling records, transforms, texture
  indices, F3DEX2 lists and vertices. Header word `+0x30`, rounded up to 16,
  gives the geometry start. Graphics segment 2 addresses that region.
* Texture indices start at header `+0x0C`. Concatenate LZSS blocks from collection
  `0x0203C53E` until header `+0x10` bytes are filled. Graphics segment 3 addresses
  the concatenated bytes.
* Optional member 1 is a height map, not a town sprite layer. Tenne's sample is
  a 128-by-128 grid with a 28-byte header.

The field overlay is ROM `0xEBBB0..0xF9FF0` to RAM
`0x8019EE50..0x801AD290`, delta `0x800B32A0`. Resource loading is at ROM
`0xED908..0xEDBB0`; segment setup at `0xEEA34..0xEEB10`; drawing wordcode at
`0xEF59C..0xEFD28`. The companion image helper overlay is separately qualified:
ROM `0xE48F0..0xEBBB0` to RAM `0x80197B70..0x8019EE30`, delta `0x800B3280`.
Neither uses the early-boot mapping.

`tools/generate-battlefield-materials.cjs` executes original material helpers
in the isolated MIPS test interpreter. Its generated
`game/scenarios/field-materials.json` contains commands and resource references,
not artwork. Textures and geometry come from the loaded ROM. The application
requires no parent repository, browser runtime or Decomp checkout. Regeneration
takes the private ROM path as its first argument. The recipes represent stock
helper behavior at animation phase zero.

## Buildings and editing

Buildings are mesh pieces in the map model, positioned by its transforms.
Wordcode command 25, callback 0, invokes the town texture helper at ROM
`0xF5BC4` / RAM `0x801A8E64`. Its one-based site argument selects a runtime
site record; allegiance selects texture children 170, 171 or 172 from
`0x0203C53E` (allied, enemy, neutral). The editor derives the initial flags
from town metadata bytes 25–26 plus the scenario's site addend. Apply and Undo
change the preview's cache key so building colours refresh.

Site and treasure inspectors, squad/waypoint/treasure dragging, and their project
and ROM export paths remain available. Site outlines are unfilled so buildings
remain visible. Grid lines are off initially.

This change does **not** author the mesh, height grid, terrain classifications,
roads or building transforms. Future terrain/road drawing must keep these data
consistent; moving a stronghold must also update its gameplay placement.
Changing site-type metadata alone does not replace embedded geometry. Do not
implement map painting as replacement of the rendered preview bitmap.

## Briefing is independent artwork

The small named `s_map*.n64` images are separate resources. The former Tenne
backdrop was `s_map06.n64`, 224 by 160 pixels. Their collection is `0x018BD022`
(ROM envelope `0x1E512A2`), selected by model/media ID minus 4. The scene
image/briefing path loads and pans/zooms these images; it does not render the
battlefield into them. The corresponding scene-overlay loader is ROM `0x2B506C`,
RAM `0x8024026C`.

Battlefield edits therefore do not automatically appear in briefing art. A
future map export flow needs a separate regenerate/replace briefing option,
with framing and annotation decisions. The regression test changes Tenne's
field resource selection and verifies that its briefing pixels remain identical.

## Checks and limits

`map_tests battlefieldRendering` decodes and renders all 63 currently supported
keys, checks non-flat output, confirms Tenne's eight town texture selections,
compares allied/enemy pixels, and rejects an invalid geometry selector. Key 54
retains its existing unsupported alternate-loader boundary.
`capturedBoundsAndPlacements` compares 64 captured rectangles and 522 site
positions. `ui_tests scenarioMapEditing scenarioWorldEditing` covers selection,
allegiance repaint and Undo, marker moves, project reopening, ROM export and
reopening, and checksum preservation. These are offline checks, not a new
emulator run or a claim of pixel-exact game lighting.
