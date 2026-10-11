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

### City palette

Map's **City pieces** tool offers 19 reusable models from Fort Romulus (key 39),
Latium (47), and Talpaea (51): houses, roofed towers, keeps, gates and wall
sections. The separate dropdown includes oblique thumbnails decoded from the
current ROM. Click to place, use Select / move to drag a piece, and use its
sidebar to rotate or remove it. Rotation applies to the complete geometry and
normals. A footprint preview and selected outline show its extent.

The four city brushes are cobblestone paving, cobblestone road, Romulus road and
Talpaea road. Broad paving repeats the inner stones without road curbs. These
visual styles use Highway movement/encounter terrain and participate in the
ordinary contour/border recalculation. Their project paint IDs 32–35 never enter
the game's terrain grid; export maps them to native terrain 1.

City models are static scenery: they do not create town/site records, shops,
mission events, gate-opening behavior or movement-obstacle masks. Movement still
uses the terrain underneath. Their base follows the edited ground height; users
can clear existing foliage by painting the desired ground first. Closed gates
retain the source model's closed appearance. The catalog uses explicit native
culling groups, combining separate roof/facade groups, rather than slicing a
whole fortress by proximity. Talpaea's outer wall is a complete long wall piece.

Project/Undo data stores the piece ID, world position and rotation. Exports append
bounded native display lists, geometry, RGBA16 textures and sampling settings;
source textures come from the loaded ROM. Repeated texture bytes are shared
within the new city geometry. UV coordinates, vertex colors, transformed normals
and source filtering are retained; colors use the existing static material
interpretation. The 512 KiB model limit remains, with an additional 256-piece
authoring cap. Original bytes and resource references stay intact. The generated
briefing includes the pieces. Keep the project to edit individual placements;
reopening only the ROM retains their rendered geometry as part of the map.

### Existing towns and strongholds

Buildings are mesh pieces in the map model, positioned by its transforms.
Wordcode command 25, callback 0, invokes the town texture helper at ROM
`0xF5BC4` / RAM `0x801A8E64`. Its one-based site argument selects a runtime
site record; allegiance selects texture children 170, 171 or 172 from
`0x0203C53E` (allied, enemy, neutral). The editor derives the initial flags
from town metadata bytes 25–26 plus the scenario's site addend. Apply and Undo
change the preview's cache key so building colours refresh.

The Map tab provides Select / move, Paint terrain, Draw road, and Erase painting.
Brush radius accepts half steps, including 1.5; preview and raster coverage use
the fractional value without rounding to an integer.
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
* Every brush recomputes its contour from the final terrain mask. A visible
  terrain raster derives from material/group coverage, using the original gameplay
  grid to identify native materials. Painting wholly inside matching terrain leaves
  its mesh and height unchanged. A changed cell inside or adjoining an original
  forest/highland region rebuilds that connected region with its new outline:
  replacement terrain cuts away the old surface, and matching extensions share
  one border. Both plain and snowy variants use this path. Flat brush edges receive
  a narrow coverage feather; raised forest/highlands retain opaque feet, avoiding
  the sawtooth of per-face fog alpha. Forest borders retain their separate side
  texture. Raised distance fields receive half-strength 3-by-3 smoothing before
  tracing, retaining one-cell islands without increasing mesh density.
* Original faces are identified by wordcode draw and face ordinal, including
  ungrouped mountains. Changed display lists are cloned with only affected terrain
  triangles omitted; state, matrix, vertex, unknown commands, site geometry and
  unrelated faces are retained. The remaining forest/ridge is generated over the
  exposed base surface. Erase and Undo recompute from the source and current mask.
* Terrain and brush-material classification samples all covered gameplay cells.
  The previous single-centroid vote could classify a large plain triangle as a
  hill lying above it, then rebuild the plains when a nearby forest was painted.
  Sub-cell triangles retain a centroid fallback weighted by their actual area.
* Generated shapes are approximations of native authored landforms, not copied
  artist meshes. Rebuilding an affected connected region can change its slope and
  texture mapping beyond the immediate stroke. Arbitrary terrain-border artwork
  is not synthesized. Water uses a static material phase.
  Stock maps have no type-10 cells; Snowy Bridge borrows bridge artwork from a
  snowy map and writes the separate type-10 gameplay classification.
* Site point collection `0x02092506` has stored members with a 28-byte header and
  float X/Z pairs; scenario byte `+7` selects its member. Moving a site changes
  those floats without resizing the member or altering other sites.
* Field member 1 is an RLE height grid. Row offsets are halfwords from byte 28;
  `FFnn` repeats the following u16 value `nn` times. Highland painting updates
  this movement surface with the visible rise. Forest movement follows half the
  canopy rise, an approximation based on stock canopy/height-grid comparisons.
  Replaced native regions first lower to the exposed base before the new terrain's
  rise is applied, so roads through forests and mountains have matching movement
  heights. Buildings follow the updated movement surface.
  If a summit exceeds the original height range, the grid is requantized;
  otherwise untouched sample values retain their original encoding values.
  The original reader is
  `0xF3704..0xF37D4`; triangle interpolation scales by 65279 (`0xF33C8`).
* New display lists, textures and matrices append inside the model. Compact
  resource members preserve their original bytes when unchanged; edited model
  and terrain collections relocate into bounded cartridge space and update
  their original directory entries. Unknown members remain intact. The export
  ceiling is 64 MiB, with 512 KiB per edited model. Vertices are shared within
  32-entry RSP batches; small spatial blocks reuse neighbouring rows without
  reducing mesh resolution. TRI2 commands encode pairs of faces. There is no separate
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

## Briefing artwork and minimap updates

The small named `s_map*.n64` images are separate resources. The former Tenne
backdrop was `s_map06.n64`, 224 by 160 pixels. Their collection is `0x018BD022`
(ROM envelope `0x1E512A2`), selected by model/media ID minus 4. The scene
image/briefing path loads and pans/zooms these images; it does not render the
battlefield into them. The corresponding scene-overlay loader is ROM `0x2B506C`,
RAM `0x8024026C`.

Map export now regenerates this image when terrain is painted or a building is
moved. It renders the exported battlefield, downsamples to the original image
dimensions, and quantizes to its existing CI8 palette. Original framing is retained
where calibrated; otherwise the map's world rectangle is used. This is a rendered
overview, not a recreation of the original artist's painted image. Boundary-only
edits keep the original artwork because no scenery changed.

The LHA archive retains its name and framing, is appended through the bounded field
resource allocator, and replaces the matching directory entry. Other maps stay
untouched. A 24-byte `LCMAP001` footer after the native pixels/palette records four
big-endian floats (left, top, right, bottom) so reopening a generated ROM retains
its framing. Image data readers consume the header-sized pixels/palette; the footer
is editor metadata. Scene image lookup follows the current directory by archive
name, including after relocation. A no-edit export does not regenerate images.

The minimap is different: original z64 `0x115DE4..0x116004` (scenario overlay delta
`0x800AB8C0`) reads the fine-terrain slot at RAM `0x800E7AC2`, resolves its pointer
through RAM `0x800E7A90`, and submits the entire 256 by 256 grid as 32 CI8 strips,
each 256 by 8. It is a palette view of the terrain grid, not another stored picture.
Terrain exports therefore update it directly; moving sites updates the separate
world-coordinate placements used by map markers. The parent tool
`tools/verify_minimap_terrain.cjs` executes the original texture loop against an
exported grid. Matrix helpers are stubbed; it does not verify onscreen projection,
marker animation, or a full game load.

## Road flow and map-specific brushes

Brush material selection first samples the current battlefield and falls back to
another loaded-ROM map only when that terrain has no usable local material. The
former global first-map cache could apply Tenne materials to other maps.
Road selection prefers the long clamped strips over small end caps. Even the long
native strip contains a tapered end, so highway brushes use its middle half and
mirror successive T repeats to avoid crosswise cuts. S still covers the full width.
A thinned centerline of the combined native/painted road mask supplies accumulated
lengthwise T coordinates, replacing world-X-plus-Z T which collapsed on diagonal
strokes. Local projections are blended with a continuous three-cell kernel;
choosing one nearest centerline sample previously caused visible jumps at bends.
Branches and closed loops can still distort the texture flow. Brush width controls
the road width; native baked-in edge colours remain part of its material.

Bridges use a separate path. Their small atlas (8x16 on Tenne) supplies separate
deck, rail and support faces, so treating it as a repeating road texture caused
blurred bands and rounded ends. The editor extracts one complete connected native
bridge mesh from a single-material group, retains its UVs and texture filtering,
and fits it to each connected painted bridge region. Tenne batches three bridges
in one draw group; vertex connectivity isolates one 116-face bridge. Matching
local artwork is preferred, with loaded-ROM fallback for absent/unsupported local
bridge templates and snowy variants. No fixed external bridge asset is bundled.

Bridge dragging previews and places only the straight start-to-cursor line;
intermediate mouse detours do not paint terrain. Each connected bridge stroke
becomes a straight, level span; separate bridges
need a gap. Length and width follow the painted footprint and height follows the
exposed ground. The source mesh's height proportions scale with width. Branched
or curved masks are fitted to one straight span. The coarse movement-height grid
also updates surrounding interpolation corners so narrow bridges do not fall
between samples. This can slightly raise the immediate approach around a span.
Existing project masks regenerate with the new geometry on export.

`bridgePainting` checks distinct 1/1.5/2 dab coverage (5/9/13 cells), one connected
native bridge, ordinary/snowy spans, deck versus decoded movement height,
project/ROM reopening, matching rendered pixels and unaffected maps. The
`scenarioMapEditing` UI workflow selects 1.5, paints a road and bridge, and checks
that a mouse detour leaves no off-line painting, Undo/restore, full export/reopening
and CIC checksum. Render comparisons are offline;
this does not establish an in-game movement or weather-lighting comparison.

`map_tests curvedRoadContinuity` reproduces a broad curved road crossing Tenne's
forest and an existing road. It checks local UV continuity, 495 road-center pixels,
mesh clearance within vertex quantization, ROM export/reopen image identity, and
that reimported body tiles are not cropped a second time. The original nearest
sample mapping jumped by 0.892 at a 0.0002-cell step; the blended mapping changes
by at most 0.000161. The reproduced centerline's 34 visible breaks became zero.
The before/after render was inspected. This is an offline rendering/export check.

Generated mountains retain procedural distance-based heights, UVs and normals.
They are not the original artist-authored landforms. The top-down editor uses
approximate daylight; it does not reproduce runtime weather or lighting.
The supplied Zenobian export exposed a concrete runtime difference: native terrain
uses geometry `0x230405`, two cycles and fog blender `0xC8112078`; the painted
mesh forced geometry `0x220405` and the non-fog blender `0x00552078`. Export now
retains the current map's fog and its opaque/translucent blend path. Because fog
uses vertex SHADE alpha, feathered joins use 16-level per-face primitive alpha on
fog maps. Non-fog maps retain per-vertex coverage. Fog parameters and lighting
remain under the game's control. This addresses a confirmed rendering-state
mismatch; the final appearance still needs an in-game comparison.

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
add mesh/height, renders an adjoining extension, and encodes 36 dabs including
the rebuilt native borders within the 512 KiB model limit. It checks scenery painting without movement
changes, expanded/shrunk fine and coarse grids, cursor-table export/reopening,
shared-key limits, protected fortress cells and unchanged object/world coordinates.
`ui_tests scenarioMapEditing` additionally drives boundary visibility, numeric
Apply, corner dragging, Undo/Redo, project persistence, full ROM export and CIC
checksum verification. These are offline checks; expanded boundaries have not
had a new in-game movement playthrough.

`terrainBorderReplacement` cuts a curved road through native forest, then replaces
native forest and highlands with each of the other 13 brush types. It checks
preserved native faces, removal of the overwritten faces, the exposed flat surface
and raised rim, independently decoded lower movement heights, native model/height
export and reopening, unaffected maps and full restoration. The curved-road and
matching-extension renders were inspected. This is offline validation; the new
terrain shapes have not had an in-game movement/lighting comparison.
`battlefieldEditingCoverage` also cuts a native raised region on all 38 distinct
supported maps, decodes the filtered drawing lists and checks retained vertex
positions and model capacity. Final focused map checks: 12 Qt passes including
setup/cleanup; the map editing UI workflow: 3 passes including setup/cleanup.

`paintedTerrainFog`, `roadTextureFlow`, `briefingArtworkExport`, and the opt-in `suppliedMapProject`
check diagonal road coordinates, regenerated image dimensions/framing, relocated
scene lookup, unaffected map artwork, ROM reimport, and the supplied Mylesia/
Zenobian project. Fog checks compare the native Tenne/Zenobian drawing modes,
the appended mesh's modes, and combiner opacity independence from fog alpha.
The full project export and the original minimap texture loop
pass for its edited terrain. `ui_tests scenarioMapEditing scenarioWorldEditing`
also pass complete editing/export/reopen and checksum checks. Updated map and
briefing previews were inspected. These remain offline checks.
