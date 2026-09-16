# Playback inputs

Cutscene Studio can import JSON playback inputs for one Director resource.
The snapshot supplies explicit caller state for preview playback.
It does not edit the ROM or become part of the Project.
Project load clears imported snapshots.

The inspector's **Playback inputs** control accepts files up to 128 KiB.
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
| `capturedSnapshot` | Static Actor snapshot with unknown resume state, described below |

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

### Static captured Actors

`capturedSnapshot` imports observed Actor records without declaring other jobs absent.
Its `slots` use the same 28-slot record format described above.
Its value also requires byte `sceneMode`, unsigned 32-bit `observedParserCursor`, `resumeState: "unknown"`, and `otherJobOwners: "unknown"`.
It must not contain `otherJobsEmpty` or accompany known `existingActors` or `externalProducers`.

Compilation retains one static snapshot and reports `captured-snapshot-resume-input`.
`capturedSnapshot.executedUpdates` is zero, and `clockUnit` identifies the absence of elapsed updates.
The parser cursor is retained as an observation; it does not resume Director execution.
The diagnostic toggle does not bypass this stop.
Actor coordinates and pose fields can be compared with the capture.
Camera, dialogue, menus, service history, cadence, and rendered agreement remain unqualified.

The normal import size, cancellation, preparation, and retained-state limits still apply.
This path does not execute Actor updates or external callbacks.
It does not establish resumed scene playback.

### Qualified single-update resume

An explicit known `capturedResume` group can qualify one prospective update from a captured snapshot.
The supported entry is `normal-director-held-movement-query`; `origin` must be `prospective-captured-state`, and `updates` must be one.
This entry starts inside the selected wrapper at the normal Director call, after resource staging.
It does not replay initialization, controller selection, or resource-pass helpers.
The supplied captured parser cursor must identify an enabled movement query that remains blocked after the update.

The value supplies `primaryOwnerAddress` and complete `primaryOwnerHex` (`0x1CB2` bytes), plus `secondaryOwnerAddress` and `secondaryOwnerHex` (`0x844` bytes).
It also supplies `registeredCounter: 0` and a negative signed-byte `tailTimer`.
The primary and secondary fields must select the qualified inactive non-Actor helper paths.
These checks describe inactive consumers in this update; they do not declare all game jobs absent.
All occupied Actors must use ordinary pose mode.
Movement jobs must advance without pause, countdown wrap, or allocator cleanup.
Shared pose controls require a later qualification and stop this bounded path.

`menuRootAddress` and complete `menuRootHex` (`0xD8` bytes) supply the selected list context.
The next-pointer at root offset four must be zero.
`menuOwners` contains fourteen null entries or `{address, recordHex}` records of 22 bytes each.
Their pointers must agree with the primary owner table.
Supplied memory regions must be valid and disjoint, including Actor and movement owners.

The product runs movement, pose, and the actual Director query before publishing the menu-selection result.
Inactive native helper guards and the held query establish selection preservation for this entry.
The preserved empty list then selects no menu entity; retained owner records do not become creation events or list membership.
The result reports `qualified-resume-update-complete`, one executed update, and the retained query boundary.
`resumedState` exposes the resulting movement slots, counter, and parser word.
`resumedMenuSelection` identifies its `after-director` phase and preservation basis.

Other entry phases, passing queries, active unsupported owners, and concurrent Director contexts require further inputs.
Omitting `capturedResume` retains the static behavior above.
Existing size, cancellation, trace, and retained-state limits still apply.
This prospective update does not establish historical cadence, service history, natural launch, camera agreement, or synchronized game pixels.

Current-unit binding preserves Actor identity and swaps occupied records between slots.
Slot-owned movement remains in its original slot.
Complete empty input rows skip construction; final slot normalization still requires known Actor occupancy and source-row memory.
Nonempty roster construction requires linked-object inputs, terrain samples, and qualified State services.
Class binding selects the first present matching row, then its first occupied Actor slot.
Equal unsigned class bytes match. Unequal bytes match only within hexadecimal families `51/52/53`, `54/55`, `56/57`, `5F/60`, and `63/64`.
If that first row has no Actor, binding does not try another matching row.
Only the selected component moves. Source-row identity and slot-owned movement remain unchanged.
Neither operation constructs a guessed party.

Ordinary and qualified alternate pose playback use counted records, per-record delays, and the current frame token.
Shared controls use qualified table reads and explicit caller inputs where required.
Unavailable alternate setup and shared-control inputs stop strict playback with a named boundary.
The diagnostic toggle can retain explicitly labeled legacy query estimates for unsupported Actor state.
Those estimates do not prove native completion.

Movement uses single-precision arithmetic and a wrapping 16-bit countdown.
Immediate placement preserves an existing movement owner.
Mode-two terrain and proximity behavior remains outside the planar movement contract.
Normal Actor update eligibility does not establish a universal conversion to video frames or seconds.

## External producer inputs

The optional `externalProducers` group supports native dialogue services, transient menus, color objects, and shared pose requests.
The combined implementation remains review pending.
See [Dialogue inputs](cutscene-dialogue-inputs.md) for memory, ownership, service history, and helper outcomes.
These inputs establish bounded product execution, not complete native scene timing, audible output, or image agreement.

The `known` value contains `throughTick`, `menuCreates`, `colorCreates`, `events`, and `poseCalls`.
All four lists must be present, including empty lists.
The existing 128 KiB import limit applies to the complete file.

`throughTick` is an integer from 0 through 29,999.
It declares the last Director update covered by the supplied external service history.
It does not convert an external callback into a Director update or a video frame.
Queries beyond that history require further inputs, even when the last recorded value would pass.

Each event specifies `tick`, `phase`, and `kind`.
The phase is `before-director` or `after-director`.
Events must appear in update and phase order; array order selects multiple calls within one phase.
Before events run before ordinary Actor updates and Director evaluation.
After events run after Director evaluation and before its retained snapshot.
An after event does not invent another Director query in the same update.

Only listed events run. Missing updates do not imply one callback per Director update.
`eligible: false` records a skipped menu or color callback without changing producer state.
The supplier must qualify eligibility, selected input masks, owner identity, and the completeness of this history.

### Initial state and creation

`initialMenusEmpty: true` declares empty initial transient ownership slots.
Otherwise, uncreated-slot queries require initial ownership input.
Inherited populated menus are not represented by that declaration.
Creation and explicit release establish ownership knowledge for their selected slot independently of other initial slots.

`initialColor` can be null for a known absent object.
An occupied value contains `ownerId` and a complete 12-byte `recordHex`.
Omission leaves color ownership unknown.
The record retains signed countdowns, RGB bytes, ownership flag, and current/start/target alpha.
Explicit color ownership and history take priority over presentation-only overlays imported from a concurrent context.

Every creation row identifies `nodeId`, zero-based `occurrence`, and a unique `ownerId` within its creation list.
The occurrence counts executions of that command, including repeated execution through a loop.
A new owner identifier distinguishes a replaced producer from its predecessor.

| List | Additional creation fields |
|---|---|
| `menuCreates` | `entityId`, preset `1..24`, byte `substate`, signed-byte `selection`, `cancel`, and `optionCount` |
| `colorCreates` | `allocationReady: true` and `registrationReturned: true` |

Menu fields are qualified initializer results. They do not implement preset setup or the opening helper.
Color outcomes declare successful native allocation and callback registration where required.
They do not infer successful allocation from a command alone.

### Menu callbacks

A menu event uses `kind: "menu"`, `slot`, `ownerId`, `entityId`, and `eligible`.
An eligible event must match the current creation generation and entity.
Its `readiness` contains byte `readyByte`, signed-halfword `alpha`, and unsigned-halfword `cooldown`.
Input-ready calls need unsigned-halfword `actionMask` and `directionMask`.
The opening substate needs `openingReturned: true`; subsequent readiness remains supplied separately.

Supported callbacks preserve neutral status, A-before-B priority, direction priority, and signed selection results.
Graceful close requires eight eligible close calls.
Immediate detach and ownership release remain separate commands.
The snapshot's `nativeExternal.menus` exposes the retained producer states.

### Color callbacks

A color event uses `kind: "color"`, `ownerId`, and `eligible`.
Positive signed countdowns decrement once per eligible event.
Zero and negative countdowns remain unchanged.
Cleanup always clears the countdown; only ownership flag 1 also removes the object.
Creation preserves native byte truncation and the explicit inherited alpha branch.

### Shared pose requests

Controls 17 and 19 read their qualified table entries directly from the loaded z64 ROM input.
Controls 18 and 20 consume `poseCalls` in execution order.
Each row identifies `actorId`, signed-halfword `bank` and `stateIndex`, `recordOrdinal`, and `opcode`.
Actor identity remains stable across primary-slot swaps.

Ordinary controls 18 and 20 need `projectionReturned: true`.
This qualifies the accepted zero-input-vector branch, valid nonaliasing buffers, and a normally returning projection helper.
It does not supply a projected coordinate or move the Actor.

Alternate controls 18 and 20 need an `alternate` object.
`childPresent: false` suppresses registration.
Otherwise, `metadataPresent` selects the accepted metadata branch.
Present metadata needs unsigned-halfword `type` and signed-word `projectedX`.
Types 29 and 30 also need unsigned-word `ownerMetaB`.
Unresolved classification branches need `classification` and, where applicable, qualified predicate result `predicateKey` of 4 or 5.
These values remain external owner, projector, and classifier inputs.

Shared registration replaces one scalar request in context A or B.
It adds no pose delay and does not replace the ordinary sequencer tail.
`initialRequests`, when supplied, contains signed-word `A` and `B` values.
An omitted slot starts unknown until registration or an explicit reset establishes its value.

Events with `kind: "request-dispatch"` and `context: "A"` or `"B"` construct the accepted audio requests.
They do not clear the scalar slot.
Separate `request-reset` events write -1.
Dispatch requires a known current slot; negative requests produce no dispatch record.
The snapshot records `nativeExternal.sharedRequests`, and dispatched request descriptions appear in `audio`.
These records do not prove audio queue acceptance, voice lifetime, or audible playback.

The runtime preserves its existing update, storage, trace, cancellation, and timeline-action limits.
Diagnostic assumptions remain labeled and do not turn missing native inputs into verified completion.

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
Ordinary construction, State setup, and final slot normalization support qualified inputs.

## Ordinary roster construction

`rosterConstruction.value` contains `route`, `presentationByte`, `links`, and optional `sceneKey`.
State selection requires the signed-halfword `sceneKey` when construction produces its first output Actor.
The runtime derives caller control from the command: opcode `0x45` supplies zero; `0xAB` supplies one.
`route` is the signed native launch-route byte. `presentationByte` is the shared presentation value narrowed to a byte.
Each linked object contains a unique unsigned `pointer`, signed-halfword `x`, `y`, and `z`, and Boolean `allocationSucceeded`.
Its `terrainHeight` is an exact finite single-precision result or null when unavailable.
Type-four construction uses the linked Y value. Other types require terrain truncation within the signed-word domain before signed-halfword narrowing.
The qualified pointer connects this object to a current input-row pointer; it does not authorize arbitrary memory access.

Routes minus three and minus ten also require `excludedRows`, with 20 Boolean or null entries.
Each Boolean is the native exclusion predicate result for that row. Null remains unavailable.
Known complete slot occupancy and successful allocations are prerequisites. Exhausted native capacity has no safe native return.

Construction scans each row's three pointers and preserves output holes, linked ordinals, source-row identity, and first-free-slot placement.
The constructor's output and completed setup effects remain visible when a later prerequisite is unavailable.
Finalization reads the current slot at each ascending index. Earlier swaps can skip an Actor or cause repeated evaluation.
Only participating slot fields change. Separately owned jobs remain at their slots.

## Materializer State services

`rosterStateServices.value` contains entries keyed by `nodeId` and zero-based `occurrence`.
One entry supplies ordered services for one materializer command, including related-Actor propagation.
The occurrence counts the first appearance or marker service request. Earlier stops consume no entry.
Each entry requires `appearances` and `preparations` arrays.

Each appearance contains `slot`, four unsigned `halfwords`, a word `response`, and `status` (`returned` or `unavailable`).
The halfwords must match the current source row's appearance fields. Preparation receives the response's low halfword.
Each preparation contains `slot`, five unsigned `values`, optional five unsigned `localValues`, and the same status choices.
The values represent art, context, flag B, flag A, and appearance, in that order.
Changed local values do not replace the seeded Actor fields or original recursive arguments.
Returned preparation does not establish a pose resource. The existing qualified pose provider must also resolve.

Appearance and preparation entries can include `effects` describing qualified service outcomes.
`effects.actors` contains `slot`, `beforeRecordHex`, and `afterRecordHex` for each complete 336-byte Actor record.
`effects.rows` contains `ordinal`, `beforeHex`, and `afterHex` for each complete 248-byte source row.
Every preimage must match current memory before any effect applies. Actor effects must preserve slot identity and finite coordinates.
Pointer-table replacements and slot-owned job changes are outside this service representation.
An unavailable response preserves the seed and its qualified partial effects.

Optional `poseEffects` entries contain `call`, `slot`, `beforeRecordHex`, and `effects`.
`call` is the zero-based immediate-pose call within this materializer command.
These effects require the exact completed pose record. They describe qualified external effects, not inferred producer behavior.
The runtime classifies the current Actor and row after pose, then visits current related slots in ascending order.
Related setup receives original arguments. A sentinel therefore inherits each related Actor's own field.
Secondary State values at least 50 return before seeding. Caller marker scanning can still follow that return.

Optional `markerLookups` entries contain `slot`, `decoderMode`, `cursor`, `state`, `art`, `context`, `flagA`, `flagB`, `opcode`, and optional `effects`.
Every lookup must match current Actor arguments. Without these entries, scanning uses the qualified current pose program.
Scanning clears delay and advances the cursor before lookup. Only low-byte opcode four terminates scanning.
Termination subtracts two from the current cursor. Scanning preserves the frame token and does not dispatch pose controls.
Exhausted opcode zero is not completion. The lookup limit is 256; recursion depth is below 28 and total setup calls cannot exceed 256.
Missing appearance, preparation, pose resources, or terminating marker remains an explicit playback boundary.

## Deployed-unit reset

`rosterResets.value` contains entries keyed by `nodeId` and zero-based `occurrence`.
An occurrence counts a reset service request after the scene-mode and complete-row checks.
Scene mode two executes reset. Other modes consume the command without invoking it.
The operand does not choose the unit: successful reset publishes unit 30 only after all child calls return.

Each entry supplies these explicit memory and service fields:

| Field | Value and boundary |
|---|---|
| `sceneRoot` | Qualified unsigned scene-root pointer for child-to-row identity checks. |
| `currentUnit` | Current selector byte before reset. |
| `unitHex` | Exact 25-byte unit-30 row, or null when unavailable. |
| `records` | Object mapping selected deployed IDs 0–99 to complete 52-byte records. Special members read record zero. |
| `specialOverrides` | Object mapping special member IDs to unsigned `halfword` and `flags` bytes. |
| `objects` | Object mapping nonzero child pointers to complete 256-byte child records. |
| `primaryRegistry`, `secondaryRegistry` | Ordered active pointer arrays, including duplicates. This representation supports at most 256 entries per registry. |
| `releases` | Ordered resource results with exact `handle` and `status: "returned"`. Missing or nonreturning results stop before dependent cleanup. |
| `rowFinalizers` | Ordered qualified row-helper effects, described below. |
| `descriptions` | Per-row `row`, exact `rowHex`, unsigned resource `handle`, and integer `variant` narrowed to a halfword. |
| `scratch` | Six arrays named `art`, `handle`, `variant`, `orientationA`, `orientationB`, and `context`. Each supplies nine unsigned words or nulls. |
| `preparations` | Ordered grouped-service results with exact `count`, `values`, `status`, and optional returned `variants`. |
| `decodes` | Ordered child-pose decoder responses keyed by the exact request, described below. |
| `random` | Ordered unsigned native random results consumed only when child-pose flags require them. |

Reset clears only occupied bit-eight rows. It preserves other rows byte for byte.
Cleanup releases the resource, clears paired child activity words, removes the first registry match, and clears the selected row.
Unit members populate the first unused row in member order. Zero members skip; zero-art output remains unused.
The runtime copies the accepted deployed-record fields and applies special-member and formation overrides before the row helper.

A row-finalizer entry contains `row`, `mode`, `beforeRowHex`, `afterRowHex`, `objects`, and `status`.
`beforeRowHex` must equal the runtime's complete initialized row. `objects` supplies the helper's complete changed child records.
When the old unused row has bit nine set, `oldAppearanceClass` supplies its qualified classifier result.
Status `returned` permits registration and later work. Status `unavailable` retains the supplied partial row and child effects, then stops.
This interface supplies native row-helper effects; it does not implement or prove terrain, projection, appearance loading, allocation pools, or arbitrary initialization success.

The runtime collects and sorts distinct resource descriptions, then executes the native total-count grouping loop.
Grouping can read beyond initialized entries. Each actual residual read needs a supplied scratch word; null never becomes zero.
Preparation `values` contains five arrays in order: art, context, orientation B, orientation A, and variant.
An optional `variants` array supplies the service's exact variant writes, including partial writes before a nonreturning status.
Preparation status `returned` continues. Other statuses stop with prior effects preserved.
Successful loading remains a qualified service declaration.

Each decoder response contains `pointer`, `art`, `context`, `flag8`, `flag10`, signed `selection`, signed `cursor`, integer `code`, and three unsigned `bytes`.
The response must match the actual call after cursor preincrement. Ordinary decoding requires the primary-to-row identity graph.
The runtime advances primaries and extra children in native order, using halfword state, stack, material, and delay rules.
Secondary children do not receive independent pose calls. External sound controls stop before their unresolved request producer.
The bounded sequencer supports at most 256 decoded controls per call and four backed loop and return entries.

The final `nativeRosterResult` records current rows, objects, active registries, scratch words, selector, and completion status.
It is one bounded command-state result, separate from the retained frame timeline.
Later reset inputs must match previously established object, registry, and selector state.
Unavailable services can follow completed row, registry, scratch, or child changes. The selector remains unchanged until success.
These qualified synthetic paths do not establish a natural roster, universal timing, rendered-game agreement, or complete cutscene playback.
# Captured main-Actor presentation

`capturedPresentation` optionally supplies qualified camera and scene-channel state alongside a mode-zero `capturedSnapshot`.
Its status is `known` or `unknown`. Unknown or omitted presentation remains an explicit missing input; initializer geometry is not captured evidence.
It does not enable resume. The `capturedResume` group separately selects the qualified single update or candidate continuous entry.

The known value has `scope: "mode-zero-main-actor-geometry"`, `actorCamera`, `registeredCamera`, and twenty `channels` entries.
Each camera has fourteen finite `values` in native order: FOV, aspect, near, far, perspective scale, eye XYZ, target XYZ, and up XYZ.
Each camera also supplies `modelScale`. The composed Actor camera requires one; registered projection uses its independently qualified model scale.
Perspective scale and model scale are different inputs. The former uniformly scales homogeneous projection and does not change divided screen coordinates.

Each channel is null or contains `translateX`, `translateY`, `translateZ`, `rotationX`, `rotationY`, and `uniformScale`.
Every occupied Actor must select a supplied channel through its captured channel byte.
This bounded path currently requires zero channel rotations, channel scale one, and positive equal Actor X/Y/Z scales.
Captured Actor scale supplies main-sprite geometry without rewriting retained native matrix or shadow fields.
Invalid cameras, missing selected channels, other channel families, and anisotropic Actors reject before execution.

The chair fixture compares eighteen Actor observations across static sample zero and one prospective update against conditional sample one.
Its numerical limits are 0.05 pixels for registered anchors, 0.002 native matrix-translation units, and 0.0001 matrix-scale units.
These are bounded host-math comparisons to stored native packed matrices, not exact native trigonometry or fixed-point serialization.
The actual renderer consumes the supplied cameras, channel translations, and captured Actor scales.
Shadow passes, the matrix mirror, synchronized game pixels, historical cadence, and later creation remain outside this path.

## Candidate continuous chair playback

`capturedResume.value.entry: "normal-director-continuous"` enables repeated normal updates from the qualified captured entry.
This candidate remains pending independent review. The retained single-update entry and static default keep their existing boundaries.
`updates` must be an integer from 1 through 30000. `directorControllerMask` must be zero.
The runtime rejects a nonzero `options.controllerMask` override. Dialogue callback input remains separately declared in service events.
The existing execution, memory, trace, and cancellation limits still apply.

Known `externalProducers` can accompany this entry. Each event declares its tick, phase, owner, eligibility, inputs, and helper outcomes.
`events` accepts the existing array or `{templates, sequence}`.
Each template contains an event without `tick`. Each sequence row is `[tick, templateIndex]`.
Rows retain exact order, including multiple events in one phase. This representation does not imply any omitted callback.
The expanded event validation and 128 KiB import ceiling still apply.

Known `directActorCreates` supplies an array of exact direct-creation occurrences.
Each row requires `nodeId`, `occurrence`, ten `words`, `allocationAddress`, `presentationByte`, `stateIndex`, `registration: "existing"`, and `evidenceReference`.
The allocation must be aligned, inside captured RAM, and disjoint from retained Actor, owner, movement, and other creation records.
The runtime requires an empty destination slot, resolved operands, an ordinary matching ROM State, and explicit terrain-free height.
It initializes the record and executes the immediate pose. Allocation and existing registration remain supplied outcomes.

The preview executes JavaScript models of Director commands, Actor updates, sprite effects, and color updates.
Its dialogue interpreter executes the retained native instruction data, using supplied helper outcomes at the existing service boundaries.
It does not execute allocation, archive, registration, or storage implementations.
The continuous color-release command sets the native release-request flag. The chair reaches termination before a later cleanup callback.
The retained noncontinuous color-cleanup behavior is unchanged. This result does not validate other scenes or post-termination callbacks.

The corrected chair generator is `../docs/reviews/cutscene-chair-projection-correction-20260915/probe.js` in the parent repository.
It consumes native execution outcomes from sibling `native_probe.py` and `native_services.py`.
The research execution uses corrected paired floating-point registers and runs the native Actor matrix producer after each declared update.
Projection therefore consumes evolving matrices. No later captured matrix or dialogue payload becomes a full-run playback input.
The preview consumes the resulting projection-related service outcomes; it does not execute that native matrix producer.
The direct import artifact is `../docs/reviews/cutscene-chair-projection-correction-20260915/playback-input.json`.
Load the Rev0 ROM, open Cutscene Studio, and select resource `rom-director:01F4558C`.
In the scene inspector, use **Playback inputs → Import playback inputs**, select that artifact, then press **Play**.
The input is session-local. Loading a Project clears it.

This declared run uses neutral Director input and A on dialogue callbacks whose update index is divisible by 30.
It reaches termination after 616 updates. These inputs and callback order are prospective choices, not recovered historical timing.
The integration test exercises all 616 states through the Actor and effect renderer, with backgrounds omitted.
Existing camera bounds remain unchanged. Browser interaction, synchronized original-game pixels, shadows, and historical cadence remain unverified.
The separate 90-update neutral run matches all 48 complete stored dialogue payloads and window rectangles under conditional sample alignment.
The declared A input changes later dialogue progression. Neither run establishes unique historical controller input or universal scheduler cadence.

## Candidate fresh Director launch, iris, and image echo

The `directorLaunch` profile starts an active, uninitialized Director resource in mode zero.
It uses the selected ROM directory entry and declared caller state.
This candidate requires independent review before acceptance.
It is available through the existing import and Play controls.

Load the Rev0 ROM and open Cutscene Studio.
Select `rom-director:01F4558C`.
Use **Playback inputs → Import playback inputs** in the scene inspector.
Import `../docs/reviews/cutscene-image-echo-20260916/playback-input.json`, then press **Play**.
The compact file is 70,110 bytes, below the 128 KiB import limit.
The automated checks exercise the runtime and renderer; browser interaction was not performed.

The new `externalProducers.value.directorLaunch` object has these fields:

| Field | Contract |
|---|---|
| `kind` | `mode-zero-director-v1` |
| `sceneMode` | Zero |
| `selector` | Index in the current ROM Director directory; must resolve to the selected resource |
| `world` | Byte fields `mapKind`, `scenarioByte`, `red`, `green`, `blue`, and unsigned word `eventState` |
| `world.alternateContextPointer` | Optional current caller pointer for the alternate-context presence query |
| `actorPresentationWord` | Current unsigned caller word; Actor construction consumes its low byte |
| `cameraBaseHex` | Both camera banks, 144 bytes; native initialization replaces selected fields |
| `operandTranslations` | Required placeholder indexes mapped to current unsigned halfword values |
| `arena` | Exclusive aligned preview storage, with `address` and `byteLength`; maximum 65,536 bytes |
| `audioQueueHex` | Optional current sixteen-entry audio queue, 128 bytes |

The profile requires `initialDialogue`, its shared storage/lifecycle configuration, and `resourceSchedule`.
The Director resource must identify the supported initializer and remain uninitialized.
Captured Actor snapshots and concurrent Actor context are incompatible with this profile.
Recorded resource events and color creation outcomes are rejected.
Combined native launch and resource memory remains bounded to 128 KiB.

The preview executes qualified native initialization, scene-root setup, stream loading, camera initialization, color resource callbacks, and audio queue instructions.
It computes ROM resource access, decompression, allocation, and release through shared preview services.
The allocator uses a declared preview arena; it does not emulate the retail heap.
Director commands and ordinary Actor construction/updates use the existing JavaScript implementations.
Native graphics-cache preparation is not executed by Actor construction.

The input supplies caller resource memory, font metrics, camera fields, world state, roster rows, and initial audio queue state.
It supplies A for one resource pass every thirty passes through pass 3,999, with neutral masks otherwise.
This is an explicit playback experiment. It does not reproduce historical controller timing.
One tick means one resource pass, not a verified rendered frame.
The audio implementation updates its queue and emits request events.
It does not consume that queue through the audio engine or synthesize sound.

The echo candidate retains 1,435 states after 1,434 completed resource passes.
It captures the current product stage at pass 641, immediately before the iris constructor.
The iris closes, releases all seven Actors and six sprite effects, and retains the captured image.
Echo starts at pass 692 and completes after 53 updater calls, at pass 745.
The map dialogue then completes. Playback stops before transient preset 33, opcode `0x62`, at stream word `0x03C6`.
Its menu constructor and recurring service remain unsupported. Existing native sources describe the next implementation path.
Without `imageEcho`, the framebuffer profile retains its earlier stop at the echo constructor.
Without `framebuffer`, fresh launch retains its earlier stop at the iris constructor.

The candidate adds `externalProducers.value.framebuffer`:

| Field | Contract |
|---|---|
| `kind` | `product-framebuffer-v1` |
| `capturePolicy` | `constructor-current-state` |
| `backgroundPolicy` | `omit` or `require`; omitted means `require` |

The Editor renders the current stage when the constructor requests capture.
This service does not reuse the last displayed frame or a captured game image.
The supplied candidate explicitly omits background groups and retains the decoded vignette image.
Its preview uses the same omission policy before and after capture.
Editor movement guides do not enter the captured image.
Capture requires an opaque 320-by-240 render target with current Actor and effect sprites.
Active dialogue, unavailable images, and scene props without native layer ownership fail explicitly.
Dialogue glyphs and portraits are not supplied by this capture service.

Shared JavaScript computes layer rotation, the three-ring iris mesh, update/query state, and release from current operands.
Native controls compare both phases; an edited retail prefix exercises paired close/open through terminal hold.
That control is not the unmodified retail continuation after the echo boundary.
Opening restores saved layers and releases the live captured image; released Actors do not return.
The selected layer index is restored. The active layer count does not change.
The product preserves current layer transform fields. General layer projection remains a product rendering model.

Seeking retains at most eight runtime-owned image buffers, totaling 2,457,600 bytes, within the runtime's 128 MiB limit.
The ninth capture stops before retaining another image.
Recompilation owns separate buffers; cancellation prevents a pending capture from publishing runtime state.
Image history remains available after live iris release so earlier positions can still render.
The current profile requires fresh mode-zero launch and one background registration.
Additional registrations require cumulative layer ownership and stop explicitly.
Durations outside 1..30,000, unsupported phases, unsafe geometry, and changed qualified code also fail explicitly.

Checks compare initialization bytes, seven complete Actor constructor records, initial-parser camera and Actor fields, color lifecycle, and changed audio queues.
A second ROM directory entry and changed world inputs exercise the same initializer.
The launch-only regression consumes 642 states, 4,494 Actor frames, and 3,852 effect frames, with backgrounds omitted.
Dialogue glyphs remain approximate byte text; portrait states, shadows, native pixels, and historical cadence remain outside this validation.
The separate neutral control runs 120 passes; it is not the older historical capture window.

The framebuffer generators, native controls, checks, and render artifacts are listed in `../docs/reviews/cutscene-framebuffer-effects-20260916/handoff.md`.
Earlier captured playback inputs and their comparison evidence remain separate.

The echo profile adds `externalProducers.value.imageEcho: { "kind": "native-image-echo-v1" }`.
It requires the framebuffer profile and one initialized scene image.
Native instructions compute the vignette endpoints, echo matrices, cyclic matrix copies, query, fade, and release.
The source image comes from the current ROM-selected image. Echo does not capture another framebuffer.
The constructor selects the original image while the processed vignette and captured stage retain separate ownership.
The normal scene pass updates echo before iris and before the Director. Alternate scheduling skips both updates.
At most three 64-byte echo matrices remain live. Completion releases all three.
Combined launch, resource, and matrix execution memory remains bounded to 128 KiB.
The tested timeline retains 76,609,314 bytes, including its image, within the 128 MiB runtime limit.

The renderer uses the computed fixed-point matrices and native copy order.
Reverse mode can draw four trails by visiting one cyclic slot twice; this behavior is preserved.
Vignette projection now uses the current registered-camera producer under the echo profile.
The native current-image combiner and trail alpha rules remain distinct.
The renderer uses product perspective projection and texture sampling, not an N64 rasterizer.
Whole-scene pixel equivalence, near-plane clipping parity, backgrounds, shadows, dialogue glyphs, portraits, and historical cadence remain unverified.
The capture service still rejects active dialogue rather than inventing captured text pixels.

After iris cleanup, a known empty Actor slot leaves the native dialogue constructor's placement bytes intact.
Unknown Actor namespaces still require qualified context.
The shared lifecycle supplies the ROM-backed text-control dispatch table used by the later `@*` control.
This adds no recorded helper return or per-call outcome.
The following 2D sprite markers retain their computed render layer and scale in renderer payloads.

Echo checks and native comparisons are described in `../docs/reviews/cutscene-image-echo-20260916/handoff.md`.
The actual default initialize/import/load route, repeat compilation, seek rendering, and cancellation are tested offline.
The original framebuffer route regression remains unchanged and passes.
