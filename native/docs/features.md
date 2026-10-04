# Feature inventory and completion tracking

Source reference: the browser Editor preserved at `559b231`, including its
README, interface, parsers, exports, and tests. Preserve behavior and data
compatibility while redesigning implementation and interaction for native use.
An entry is complete only after its editing, persistence, export, and relevant
preview behavior work; a visible tab alone does not count.

| Area | Required capabilities | Native status |
|---|---|---|
| Workspace | ROM byte orders/revisions, project folders/archive/import, undo, validation, changelog, safe export | In progress |
| Classes | Complete records, names/descriptions, growth, attacks, equipment, promotion requirements, family selection/creation, new class IDs, dynamic rebuild allocation | Record fields, names/descriptions, promotion, compatibility, equipment-list availability, families and class creation through ID 255 now persist, undo and export through coordinated source rebuilds. Dynamic allocations and reopened-ROM growth verified; full-game validation and remaining feature audit pending |
| Items | Equipment stats/prices/resistances, packed growth, descriptions, preserved raw fields | Fields and description editing/export implemented; integrated workflow polish remains |
| Shops | All 35 shops, equipment/consumable inventories, expanded capacity and reopen | Search, add/remove/reorder, copy/restore, undo, project persistence/import and source-built export implemented. Supports 50 equipment/30 consumables in the original RAM arrays. Export/reopen and instruction checks pass, including empty stock; full-game validation remains |
| Consumables | Values/descriptions, supported behavior/effect editing, rebuild ownership | 45 price/metadata records and ten magnitude/range models edit, undo, persist, import and compile/export. Healing description numbers can update with the effect. Shared stat boosters remain one model; unsupported item behaviors retain their source. Integrated export/reopen and instruction checks implemented; full-game validation remains |
| Encounters | Pools, terrain/scenario/global rates, custom squads, persuasion/retreat/rewards/card text | All 45 pools, 32 terrain rates/slots, global and independent scenario rates, weighted class rewards, and custom squads now edit, undo, persist, import and compile/export, including added class IDs. Squads support independent members/equipment, formation, card text, persuasion, retreat and rewards. Selectors and callers rebuild from source; the custom runtime uses reclaimed resident memory within 4 MiB. Full pools decline custom encounters before spawning. Full-game validation remains |
| Scenarios | Enemy squads/formations/equipment, placements/routes, triggers/treasure/towns, added squads | Squad/cohort records, equipment, formation, base levels, placement, drops, supplies, added squads, movement nodes, conditions and groups edit with undo, persist and rebuild/reopen. Browser imports preserve these edits and explicit level intent. Global site metadata, per-scenario allegiance and treasure edits/additions also persist and compile/export; original collection links support independent scenario resources. Class-size/capacity changes receive cross-feature checks. Unknown extended gates are preserved; original condition identities cannot yet be deleted because mission scripts may consume them. Browser site/treasure import, map preview, alternate-overlay key 54 and full-game validation remain |
| Cutscenes | Resource catalog, native scheduler/cast/camera/rendering, seeking/selection, actor/action/dialogue editing, new/wiped scenes, events/backgrounds, export | Pending |
| Art/animation | Portraits/item/army/combat sprites, frames/layers/sequences, pixel/asset import/export; current animation patch semantics | Pending |
| Sprite workspace | Reusable assets, pixel tools, layers, transparency, undo, PNG/JPEG imports and exports | Pending |
| Save editor | Supported save formats/slots, characters/stats/equipment/inventories/Goth/Chaos Frame, valid save export | Pending |
| Tools | Experience weight, Chaos Frame, luck, alignment, attack stream split | Pending |
| Delivery | Reproducible current dependencies, portable Windows ZIP, notices, clean-machine validation | Bootstrap, package and isolated-environment checks implemented; clean-machine validation remains |

Promotion compatibility: native loading recognizes and validates the earlier
direct-loader OBSG-v2 export, reads its actual requirements, and migrates an
edited table to the source-built family resource. Unchanged exports retain the
existing bytes. OBSG-v1 shared-DMA redirects remain unsupported and are explicitly
rejected by the promotion loader; it does not silently show retail requirements.

## Additional native capabilities

Dockable workspaces, searchable model-based tables, independent engine API,
background jobs with useful failures, explicit allocation ownership, deterministic
runtime snapshots, and performance measurements on representative workloads.
Unknown game behavior remains identified rather than silently approximated.
