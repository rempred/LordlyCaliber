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
Nonempty roster construction requires additional native helper contracts and linked-object inputs.
Class binding requires the native class-family predicate.
Neither operation constructs a guessed party.

Ordinary pose playback uses counted records, per-record delays, and the current frame token.
Shared controls and the alternate decoder stop strict playback with a named boundary.
The diagnostic toggle can retain explicitly labeled legacy query estimates for unsupported Actor state.
Those estimates do not prove native completion.

Movement uses single-precision arithmetic and a wrapping 16-bit countdown.
Immediate placement preserves an existing movement owner.
Mode-two terrain and proximity behavior remains outside the planar movement contract.
Normal Actor update eligibility does not establish a universal conversion to video frames or seconds.
