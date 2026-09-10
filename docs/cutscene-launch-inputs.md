# Actor launch snapshots

Cutscene Studio can import a JSON Actor snapshot for one Director resource.
The snapshot supplies explicit caller state for preview playback.
It does not edit the ROM or become part of the Project.
Project load clears imported snapshots.

The inspector's **Actor launch snapshot** control accepts files up to 128 KiB.
An invalid file preserves the previous snapshot.
Import and Clear cancel obsolete playback preparation.
The selected resource must match the snapshot's `assetId`.

The top-level fields are:

| Field | Required value |
|---|---|
| `schema` | `ob64-cutscene-launch-inputs.v1` |
| `assetId` | Exact catalog resource identity, such as `rom-director:01F3EAD2` |
| `invocationId` | Nonempty identity of the selected launch or diagnostic fixture |
| `sourceIdentity` | Nonempty source reference, including capture or file hash when available |
| `evidenceGrade` | `Candidate`, `Supported`, `Verified`, or `Editor-ready` |

The evidence grade is the supplier's declaration. Import does not independently verify or accept that declaration.
Every supplied group inherits the top-level source and invocation identities.
Each group has `status: "known"` and `value`, or `status: "unknown"`.
Omitted groups remain unknown.

| Group | Known value |
|---|---|
| `actorInputRows` | Array of exactly 20 hexadecimal strings, each containing all `0xF8` row bytes |
| `currentUnitMembers` | Five unsigned byte member IDs from the selected current gameplay unit |
| `schedulerBranch` | `"normal"` or `"alternate"` |
| `existingActors` | Object containing `slots` and `otherJobsEmpty: true` |

All byte strings use native big-endian RAM order. They do not use V64 ROM byte order.
`existingActors.slots` contains exactly 28 elements.
A null element declares an empty slot.
Each occupied element contains a unique `identity`, a complete `0x150`-byte `recordHex`, and `movementHex`.
`movementHex` contains a complete 16-byte movement record or null for no movement owner.
The Actor's stored slot must match its array position.
Coordinates and movement velocities must be finite.

`otherJobsEmpty: true` explicitly declares that unsupported job owners are absent.
Snapshots with active unsupported jobs cannot use this representation.
Raw record preservation does not resolve external pointer graphs, shared effects, terrain, or concurrent scheduler ownership.

Current-unit binding preserves Actor identity and swaps occupied records between slots.
Slot-owned movement remains in its original slot.
Complete empty input rows establish a supported roster no-op.
Nonempty roster construction requires linked-object inputs, terrain samples, final State setup, and the final slot-normalization producer contract.
Class binding selects the first present matching row, then its first occupied Actor slot.
Equal unsigned class bytes match. Unequal bytes match only within hexadecimal families `51/52/53`, `54/55`, `56/57`, `5F/60`, and `63/64`.
If that first row has no Actor, binding does not try another matching row.
Only the selected component moves. Source-row identity and slot-owned movement remain unchanged.
Neither operation constructs a guessed party.

Ordinary and qualified alternate pose playback use counted records, per-record delays, and the current frame token.
Shared controls and unavailable alternate setup stop strict playback with a named boundary.
The diagnostic toggle can retain explicitly labeled legacy query estimates for unsupported Actor state.
Those estimates do not prove native completion.

Movement uses single-precision arithmetic and a wrapping 16-bit countdown.
Immediate placement preserves an existing movement owner.
Mode-two terrain and proximity behavior remains outside the planar movement contract.
Normal Actor update eligibility does not establish a universal conversion to video frames or seconds.

## Qualified Actor services

Three optional groups extend the same snapshot format: `poseRegistry`, `bodyPoseSetups`, and `subordinateServices`.
They use the same `known` or `unknown` status wrapper and inherit source identity, invocation identity, and evidence grade.
An omitted group is unavailable. Import validates bytes and identities; it does not authenticate supplied evidence.
These inputs describe qualified memory and service results. They do not implement the game's resource loader.

`poseRegistry.value` contains `alternate` and `ordinary` arrays.
An alternate entry contains signed-word `sourceArt` and `ownerContext`, byte `flagA`, signed-halfword `flagB`, and a nonzero 12-bit `handle`.
The handle identifies a prepared registry descriptor. It is not a ROM resource key.
An ordinary entry contains `sourceArt` and `flagA` for the same scene's ordinary pose directory.
Each entry contains `programs`, an array of `{ state, programHex }` records.
`state` is a signed-halfword directory index. `programHex` contains exactly one native counted program, including its leading record count.
Unknown states remain unavailable. A known `00` program is empty.
Duplicate tuples, duplicate states, incomplete records, and unsupported opcode widths reject the import.

Alternate advancement and ordinary pose queries use separate arrays.
An alternate empty program does not establish a zero ordinary-query result.
Delay-only calls need no lookup. An alternate lookup increments the cursor before an unavailable registry stop.
Special source-art values preserve the native current-State clamp.
No frame rate or natural-launch claim follows from these inputs.

`bodyPoseSetups.value` is an array of successful initializer outputs before their immediate pose call.
Each entry contains `nodeId`, zero-based `occurrence`, `words`, `recordHex`, and `otherActorsUnchanged: true`.
The occurrence counts setup requests after the occupied-Actor check.
`words` contains the exact seven words of the body-pose command, including its opcode.
The complete Actor record must retain the selected slot and the successful initializer's cleared cursor, delay, frame, and material seed.
The supplied record includes qualified helper results and successful resource preparation.
The runtime invokes the immediate pose call itself, then applies later scheduler eligibility separately.
If setup would change other Actors, this representation is unavailable.

`subordinateServices.value` is an array keyed by `nodeId` and zero-based `occurrence`.
The occurrence counts service attempts after row, anchor, topology, capacity, and complete-record checks pass.
Earlier input stops and empty returns consume no service entry.
Each entry contains `allocations` and `preparations` arrays in native call order.
`allocations` supplies one Boolean result for each later present component. A false result models failed allocation.
Each preparation contains `sourceArt`, `ownerContext`, `flagA`, `flagB`, `equipment`, and `status`.
Both flags contain the same constructor orientation because its two preparation arguments alias one word.
`equipment` is the selected item after the accepted art-specific override.
The runtime selects equipment from the loaded ROM's native item-type table.
Preparation status is `ready`, `unavailable`, or `cache-full`.
`ready` declares that native-equivalent preparation returned with qualified resources; a matching alternate registry entry is still required.

Subordinate construction needs all caller rows, complete slot occupancy, and a complete qualified anchor record.
It uses only the three linked pointers' presence; it does not need their object coordinates or terrain samples.
Invalid topology and exhausted paired-context capacity stop before mutation.
Later allocation, preparation, or pose failures preserve earlier component changes.
Clones copy the preceding post-pose record and use one fixed clone slot.
A second clone replaces the first clone in that slot. Slot-owned jobs remain in their original slots.

Complete-record qualification ends after an older partially modeled Actor write, mode-two movement, or a concurrent Actor-context merge.
A new qualified setup or snapshot is required before dependent shallow-copy construction.
These stops prevent retained initial bytes from being mistaken for the complete current Actor.
Ordinary roster construction, final slot normalization, and deployed-unit reset remain unavailable in this subset.
