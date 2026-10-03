# Feature inventory and completion tracking

Source reference: the browser Editor preserved at `559b231`, including its
README, interface, parsers, exports, and tests. Preserve behavior and data
compatibility while redesigning implementation and interaction for native use.
An entry is complete only after its editing, persistence, export, and relevant
preview behavior work; a visible tab alone does not count.

| Area | Required capabilities | Native status |
|---|---|---|
| Workspace | ROM byte orders/revisions, project folders/archive/import, undo, validation, changelog, safe export | In progress |
| Classes | Complete records, names/descriptions, growth, attacks, equipment, promotion requirements, family selection/creation, new class IDs, dynamic rebuild allocation | Record fields, descriptions, promotion requirements, compatibility masks and family creation/edit/export implemented for retail IDs; names, new IDs and dynamic placement remain |
| Items | Equipment stats/prices/resistances, packed growth, descriptions, preserved raw fields | Fields and description editing/export implemented; integrated workflow polish remains |
| Shops | All 35 shops, equipment/consumable inventories, expanded capacity and reopen | Pending |
| Consumables | Values/descriptions, supported behavior/effect editing, rebuild ownership | Pending |
| Encounters | Pools, terrain/scenario/global rates, custom squads, persuasion/retreat/rewards/card text | Pending |
| Scenarios | Enemy squads/formations/equipment, placements/routes, triggers/treasure/towns, added squads | Pending |
| Cutscenes | Resource catalog, native scheduler/cast/camera/rendering, seeking/selection, actor/action/dialogue editing, new/wiped scenes, events/backgrounds, export | Pending |
| Art/animation | Portraits/item/army/combat sprites, frames/layers/sequences, pixel/asset import/export; current animation patch semantics | Pending |
| Sprite workspace | Reusable assets, pixel tools, layers, transparency, undo, PNG/JPEG imports and exports | Pending |
| Save editor | Supported save formats/slots, characters/stats/equipment/inventories/Goth/Chaos Frame, valid save export | Pending |
| Tools | Experience weight, Chaos Frame, luck, alignment, attack stream split | Pending |
| Delivery | Reproducible current dependencies, portable Windows ZIP, notices, clean-machine validation | In progress |

## Additional native capabilities

Dockable workspaces, searchable model-based tables, independent engine API,
background jobs with useful failures, explicit allocation ownership, deterministic
runtime snapshots, and performance measurements on representative workloads.
Unknown game behavior remains identified rather than silently approximated.
