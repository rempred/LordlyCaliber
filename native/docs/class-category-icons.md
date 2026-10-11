# List Classes category icons

## Editing

Open **Art and Animation → UI and Icons → Class category icons**. The eight
original symbols are available immediately. **New icon** starts a blank image;
**Copy icon** makes an independent copy. Paint, fill, pick a color, move pixels,
replace from PNG/JPEG with a fitting preview, or export PNG. The images are 14×14
and use the existing opaque menu palette; transparent imports use its nearest
white background. The game adds selected/unselected brightness automatically.

In **Classes**, choose a class and its family. **List Classes icon** assigns an
existing image. **Edit icon** opens its artwork, and **Create icon** creates and
assigns a new image in one Undo action. Shared images affect every assigned
family; copy an image to edit just one family. **Use existing categories** removes
a family's independent tab and restores native grouping for those classes.

Family tabs follow the original discovery rules. Linking families for manual
class changes does not merge their tabs. Left/right navigation changes the
selection and displays the corresponding page of eight icons. Limits are 255
images total and 247 assigned families, plus the original eight categories.
Supported ROM: US revision zero. ROM export remains explicit.

## Source and export contract

`CategoryIcons` owns project JSON version 2: native pixel edits, custom images
and names, and family-to-image assignments. Each image contains 196 CI8 indices.
`CategoryIconEditor` uses the shared indexed SpriteCanvas and Undo stack.
`PromotionEditor` supplies the family picker, preview and navigation shortcut.

The native 112×14 strip starts at decoded pixel-pack offset 0x5128 in resource
key 0x01DC873E. Its 256-entry RGBA5551 palette starts at decoded offset 0x5E0 in
key 0x01DCB92C. The Army loader operands at z64 ROM 0xAED4C/0xAED54 are the only
pixel-resource links changed. Export clones the pack and preserves every other
native texture and all palette bytes; other consumers retain the original pack.

Descriptor `LC05CATS` at z64 ROM 0x27C5200 owns 64 bytes: version 1, allocation
address/length, installed-family-runtime flag, eight reserved zero bytes and
SHA-256 of the padded compressed allocation. Payload allocations are aligned to
16 bytes, at or beyond 0x2800000, and must stay within 64 MiB. Reopen validates
bounds, padding, digest, catalog and pixel/table agreement. An unused custom icon
can be saved/exported without installing the family menu runtime.

The extended decoded pack follows its 23,496 original bytes. `LCCATTAB` heads a
256-byte class→tab map (zero means native grouping), a 256-byte tab→cell map,
metadata offset/length and independent 16×14 padded CI8 cells, then the catalog.
Assigned families use tab IDs 9–255; unassigned classes use original category
masks. Export derives class tabs from the current family assignments, including
new classes. Removing all assignments keeps the installed runtime and restores
native grouping through a zero class map.

Family tabs use modified-game C, not matching decompilation. The expanded class
menu allocation adds 256 category-list bytes. The controller's three category
readers follow that array. Category discovery/counts preserve native discovery,
current-member and eligibility semantics. The Army renderer fragment at z64
ROM 0x9C02C–0x9C358 draws paged cells through the original texture-loading service.
Army relocation uses RAM = z64 ROM + 0x801135A0 for these qualified owners.
The count routine's roster helper occupies the category-list owner's unused final
96 bytes at z64 ROM 0xBC1F8, with class/alias references recorded in the existing
runtime inventory. Default builds without family tabs retain the prior code.

## Validation and limits

`icon_artwork_tests categoryIconsAndFamilyTabs` checks real-ROM compilation,
project folder/archive persistence, unchanged adjacent art and palette, checksum,
reopen/re-export identity, unassigned images, removing assignments and later
class growth. It runs `category-tabs-mips.cjs` against exported instructions for
separate linked-family lists, current/eligible counts, selection readers, paging,
clipping and register/allocation preservation. Renderer and eligibility services
are doubles; these checks do not constitute an emulator playthrough or RDP image
comparison. `ui_tests categoryIconEditingAndNavigation promotionControlsAndUndo`
checks painting/Undo, creation and assignment, invalid names, replacement and the
family-to-artwork shortcut, with an offscreen small-window capture.
