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

The Map tab provides Select / move, Paint terrain, Draw road, and Erase painting.
Drag a brush stroke; each stroke is one Undo operation. Paint Plain over a road
to remove it. Erase painting restores the source ROM's terrain in the brushed
area. Restore map edits resets painting and building positions for this shared
map, and is undoable. The affected runtime keys are shown above the canvas.

Ordinary towns and standalone strongholds move as complete native drawing groups
and gameplay placement points. Flagless buildings are associated only when a
unique culling-box centre matches a site point within 0.00002 world units (the
source's stored coordinate precision). This is an anchor-based association,
not a nearest-building guess. Every drawing list starts with its original
incoming matrix translated to the new location; this includes flag lists that
inherit a matrix from an earlier material list. Cloned local matrices and culling
boxes also move. The original outgoing transform is restored after each moved
draw. Building height follows the edited height field at the new location; an independent
building on newly raised ground rises with it.
Point-based deployments referring to the site follow its new placement.

Fort Romulus, Latium and Talpaea have connected walls/gates and lack the normal
site callback association. Their independent building movement remains disabled
with an explanation in the site inspector. Do not infer a site-to-building link
from the nearest culling box: several sites are gates or plaza/entrance points
whose connected geometry spans multiple groups. Two additional special anchors
remain unmapped: site 8 on keys 31/32 and site 1 on internal keys 63/64.

### Terrain and exported data

* Terrain directory key `0x020712EA` uses scenario byte `+5`. The loader at z64
  `0xEBF88..0xEC024` loads member 0 (256×256 fine movement terrain) and member 1
  (64×64 coarse encounter/battle terrain). Fine indexing from a coarse cell at
  `0x149168..0x149180` multiplies both axes by four. Both grids use the placement
  rectangle (the second four floats in the bounds member).
* A brush paints the fine grid inside playable terrain. Type-zero scenery can
  also be painted visually; it stays type zero until a boundary edit opens it.
  Each touched coarse cell uses the dominant authored type among its 4×4 fine
  cells. Untouched coarse cells retain their original classifications. Fixed
  wall/gate codes 15 and above are protected in both grids, including alternate
  fine grids. A boundary cut clears wholly excluded cells but preserves partially
  included coarse structures. Untouched bytes remain intact.
* Brush masks generate contoured native meshes over the original land surface.
  Highlands form raised ridges, with their full 32x64 native texture mapped from
  dark foot to light summit using clamped T. Forests form a raised canopy and use
  the original 64x32 side-strip material when the source canopy group has one.
  Other terrain uses the full source image at a scale derived from native UV
  gradients. Roads map their edge-to-centre texture across the painted width.
  Normal lighting replaces the first brush's full-white vertex colours. Explicit
  filtered sampling is emitted, and the preview supports the RDP's three-point
  footprint when the material requests filtering.
* The previous 32x32 repeating material crop was wrong for these assets. It
  repeated mountain base/peak bands and discarded forest edge textures. Saved
  project masks are still compatible and regenerate using the corrected brushes.
* Matching native terrain is included when computing brush contours. A visible
  terrain raster derives from material/group coverage, using the original gameplay
  grid to identify native materials. Painting wholly inside matching terrain leaves
  its mesh and height unchanged. Extensions use the merged outline and the
  difference between merged/original height profiles, with a short feather onto
  the native surface. Original geometry stays intact underneath.
* Generated shapes are approximations of native authored landforms, not copied
  artist meshes. Other terrain still follows existing elevation: painting does not
  flatten/remove original mountains. Arbitrary terrain-border artwork is not
  synthesized. Water uses a static material phase.
  Stock maps have no type-10 cells; Snowy Bridge borrows bridge artwork from a
  snowy map and writes the separate type-10 gameplay classification.
* Site point collection `0x02092506` has stored members with a 28-byte header and
  float X/Z pairs; scenario byte `+7` selects its member. Moving a site changes
  those floats without resizing the member or altering other sites.
* Field member 1 is an RLE height grid. Row offsets are halfwords from byte 28;
  `FFnn` repeats the following u16 value `nn` times. Highland painting updates
  this movement surface with the visible rise. Forest movement follows half the
  canopy rise, an approximation based on stock canopy/height-grid comparisons.
  If a summit exceeds the original height range, the grid is requantized;
  otherwise untouched sample values retain their original encoding values.
  The original reader is
  `0xF3704..0xF37D4`; triangle interpolation scales by 65279 (`0xF33C8`).
* New display lists, textures and matrices append inside the model. Compact
  resource members preserve their original bytes when unchanged; edited model
  and terrain collections relocate into bounded cartridge space and update
  their original directory entries. Unknown members remain intact. The export
  ceiling is 64 MiB, with 512 KiB per edited model. Vertices are shared within
  32-entry RSP batches and TRI2 commands encode pairs of faces. There is no separate
  stroke count or 5,000-triangle cap. The Map view displays encoded detail usage.
* Project `mapEdits` stores brush masks, site overrides and an optional playable
  boundary; previews, review,
  Undo, project reopening and ROM export all use the same edit component.

## Playable boundary

The Boundary checkbox displays a gold outline. Edit boundary exposes draggable
corners and four numeric edges; Apply, Undo/Redo, Restore and project reopening
use the same edit. Edges range from 1 to 254, with a minimum span of eight cells.
The world-coordinate sampling rectangle is unchanged, so objects do not rescale.

The per-runtime-key byte rectangle is at z64 ROM `0x13C874 + key*4`, ordered
left/top/right/bottom. Scenario-loader ROM `0x103244` onward converts it through
the world-coordinate rectangle into RAM `0x801F0DB8..0x801F0DC4`; ROM `0x113168`
onward clamps the cursor with those limits. These are scenario-overlay addresses,
not early-boot mappings. Export writes the rectangle for every shared-map alias,
clips movement grids outside it, and fills newly opened zero cells from their
painted or visible terrain (plain when no native material is identified).
Fine/coarse and alternate movement grids update together. The bounds resource's
first four floats remain intact: their separate consumer is not qualified.

Key 39, Fort Romulus, bypasses the normal rectangle at ROM `0x10323C`; its outline
and resize controls are unavailable rather than showing an inapplicable rectangle.
Key 54 retains its existing unsupported alternate scenario loader.

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

`map_tests battlefieldEditing battlefieldEditingCoverage battlefieldRendering`
checks project/ROM round trips, unchanged resources, every vertex of 284 moved
buildings across 38 distinct maps, all 14 brush types, shared map aliases and
protected fortress terrain. `ui_tests scenarioMapEditing` drives the brush and
building drag controls, Undo/Redo, erase/restore, saving, complete ROM rebuilding,
reopening and checksum verification. `scenarioWorldEditing` remains passing.
These are offline checks; this update has not had an emulator playthrough.


`map_tests battlefieldBrushProfiles` checks full 32x64 rock/forest and 64x32 forest
edge textures, actual raised geometry, summit/foot brightness, a decoded exported
movement-height sample, and preview identity after ROM reopening. The editor
workflow check now exports both a mountain and forest, with height-member readback
and checksum verification. New brush shapes remain approximations; these checks
do not establish exact game-camera, weather, or microcode lighting equivalence.

`map_tests battlefieldJoiningAndBoundary` checks that matching forest does not
add mesh/height, renders an adjoining extension, and encodes 36 dabs / 8,975 added
triangles in a 312,564-byte model. It checks scenery painting without movement
changes, expanded/shrunk fine and coarse grids, cursor-table export/reopening,
shared-key limits, protected fortress cells and unchanged object/world coordinates.
`ui_tests scenarioMapEditing` additionally drives boundary visibility, numeric
Apply, corner dragging, Undo/Redo, project persistence, full ROM export and CIC
checksum verification. These are offline checks; expanded boundaries have not
had a new in-game movement playthrough.
