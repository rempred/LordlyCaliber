# Native dialogue inputs

Native dialogue playback uses explicit initial state and either ordered service history or the optional computed resource pass.
The combined external-producer implementation remains review pending.
Director queries no longer infer native pauses or release from text length.
Diagnostic playback retains an approximate text presentation when these inputs are absent.

The input belongs inside the known `externalProducers.value` group.
Its existing `throughTick` bounds the complete supplied history.
Ticks order Director evaluations; they do not imply video frames or a universal producer cadence.
All existing preparation, trace, timeline, file-size, and retained-state limits remain active.

## Initial state

`initialDialogue.memory` contains non-overlapping `{address, hex, writable}` allocations.
Addresses identify qualified RAM virtual addresses.
Hexadecimal bytes provide exact allocation contents, including reached source tails and scratch bytes.
The combined allocation ceiling is 128 KiB.
An absent byte never becomes zero.
Writes require a writable allocation.

`initialDialogue.owners` contains six entries in resource-slot order.
Each entry is null or `{ownerId, payloadHex}`.
Active records require unique owner identities.
Initialized dialogue requires a complete 1,144-byte saved payload.
The supplied payload must match its current allocation header and contents.
The resource record retains the current saved-allocation handle; callbacks can replace that handle.

Initial memory includes the relevant resource pool, shared record, payload workspace, globals, source allocations, and stack.
The service stack pointer is RAM virtual address `0x807FF000`.
Only reached memory is required by execution.
The native functions retain distinct body, width, line, and measurement grammars.

Metric use requires current normal and alternate pointers with their current table bytes.
The supported retail table identity contains 166 bytes per table.
Supplying the original tables alone does not establish that a scene selected them.
Selected strings, source allocation tails, and reached scratch bytes remain separate inputs.

## Creation and services

Without shared lifecycle services, `dialogueCreates` contains `{nodeId, occurrence, ownerId, slot, directorWords, recordHex}` entries.
The words must match the resolved fourteen-word Director command.
The record supplies the qualified constructor and registration outcome.
Registration must select the first inactive resource slot.
Its flags must be `0xC800`, with the dialogue initializer and matching window identifier.
Creation clears the eight native control bytes.
The constructor outcome does not prove an allocation pool or Actor-linked placement service.

An event uses `kind: "dialogue"` and the existing tick, phase, eligibility, and owner fields.
`service` selects `initialize`, `callback`, `opening`, `closing`, or `priority`.
The first two services require the resource `slot` and its current `ownerId`.
The queue services execute their supplied invocation against the current pool and queue memory.
Provide the actual service order; the runtime does not create missing queue or callback invocations.

Without shared storage, initialization and callbacks require `storage.saveReturned: true` and a nonzero `storage.saveHandle`.
The destination allocation must provide the six-byte header and complete payload extent.
Callbacks also require the matching `storage.restoreHandle`, `restoreReturned: true`, and `freeReturned: true`.
These fields qualify allocator outcomes; the runtime does not simulate the allocation pool.
Native payload bytes, length fields, and the current handle are published after the service.

### Shared preview payload storage

The optional `initialDialogue.payloadStorage` selects a computed persistence service:

```json
{"kind":"preview-arena-v1","address":2150968560,"byteLength":1152}
```

This candidate service removes per-call `storage` outcomes from initialization and callbacks.
Events must omit `storage`, including ineligible events.
Inputs without this option retain the existing recorded-outcome behavior.

The address declares an exclusively owned preview arena, not a captured retail heap.
It must be 16-byte aligned within RAM virtual range `0x80200000..0x80700000`.
The complete arena must already exist in one writable `initialDialogue.memory` allocation.
Its size must be a positive multiple of 16, between 16 and 65,536 bytes.
No missing bytes become zero. The existing 128 KiB combined memory limit still applies.

The preview allocator selects the lowest available span and rounds allocations to 16 bytes.
It reserves six header bytes plus the current payload length.
Save copies current workspace bytes and writes the native flags and two length fields.
The unused header byte remains unchanged.
Restore validates the current owner and header, copies the saved bytes, and releases the span.
Resource removal also releases that owner's preview span.
These operations execute shared JavaScript; they do not execute the retail heap allocator.

Existing captured payloads reserve their current handles when the Engine starts.
Their complete aligned extents must fit the arena without overlapping another owner.
A live owner must restore or release before another save.
Exhaustion returns `dialogue-storage-exhaustion`; it does not invent a handle or report success.
Compressed payload mode remains unsupported.

Without shared lifecycle services, constructor records and archive lookup remain explicit outcomes.
Controller choices remain explicit inputs.
Without computed resource scheduling, service order and eligibility also remain explicit inputs.
Without shared lifecycle services, native closing and immediate-release helpers can require recorded retail free outcomes.
Those helpers retain their retail memory effects; preview span ownership is released separately.
The shared arena does not establish compatibility with arbitrary retail heap state.

The focused test compares native wrapper bytes, captured-payload adoption, allocation controls, and a second retail command path.
Run `node tests/cutscene-shared-storage.test.js` from the Editor directory.
The second path uses an extracted command slice and an explicit stop sentinel.
Its supplied constructor context and archive resolution do not establish a natural scene launch.

### Shared dialogue lifecycle

The optional `initialDialogue.lifecycle` selects candidate shared creation, archive resolution, and cleanup:

```json
{"kind":"shared-dialogue-v1","archiveArena":{"address":2150949088,"byteLength":4096}}
```

This option requires `payloadStorage` and omits all `dialogueCreates` outcomes.
Constructor owners use `dialogue:<nodeId>:<occurrence>`.
Events must omit archive-lookup and payload-free outcomes when the shared service executes those calls.
Unused outcomes still stop execution.
Inputs without this option retain the recorded constructor and helper contracts.

The archive arena follows the payload arena's address, alignment, writable-memory, and size requirements.
The arenas must not overlap.
Both arenas declare exclusive preview ownership; they are not retail heap captures.
The archive cache starts empty and retains decoded archives for this Engine's lifetime.
It uses aligned, sequential allocation and reports `dialogue-archive-exhaustion` without writing partial results.
This cache does not reproduce retail archive-cache eviction or heap placement.

Lookup reads the current ROM's Serifu selector table and resource size words.
It supports bounded LHA level-zero or level-two archives with `-lh5-` or `-lh0-` data.
It validates container bounds, decoded length, and the stored data checksum.
It computes decoded bytes with the existing LH5 decoder.
Archive lookup does not consume catalog text, saved archive bytes, or per-call results.
Native initialization still resolves the selected entry within those bytes.

Creation executes ROM-verified native core-constructor, window-registration, and resource-registration instructions.
Shared JavaScript applies the Director wrapper's supported flags, portrait selector, and placement fields.
Fixed placement mode one uses the command's coordinates.
Actor-linked mode zero uses the current Actor, transform channel, registered camera, and Actor camera.
The six changed-input controls compare complete records against native construction.
They establish the tested placements, not universal floating-point equivalence with the N64 matrix pipeline.
Continuous following of a moving dialogue anchor is outside this creation service.
The service does not execute the wrapper's retail Actor-link allocation or rebuild scene name/resource context.

The initial memory must supply reached scene context, resource ownership, queue state, text metrics, and constructor globals.
The parent resource slot, registration tag, and window edge globals are included in the generated chair input.
Missing bytes remain errors.
Portrait lookup through the command's `-1` sentinel and placement modes outside zero or one are unsupported.
Actor-linked creation requires qualified mode-zero cameras.

Native closing and immediate-release instructions still decide when a resource becomes inactive.
Their free call releases the current preview payload lease.
An unknown handle returns `dialogue-cleanup-owner`.
This replaces the retail heap operation; it does not reproduce retail free-list memory writes.
Other native text registration, formatting, audio, and optional position helper boundaries remain explicit.

Run `node tests/cutscene-dialogue-lifecycle.test.js` from the Editor directory.
The second retail slice runs 164 updates and releases its dialogue at update 142.
It uses declared saved Actor context, neutral controller samples, and explicit service cadence.
The extracted slice and stop sentinel do not establish a natural second-scene launch.

Callbacks supply `controller` with unsigned halfwords named `actionMask`, `directionMask`, `dummyMask`, `historyMask`, and `queueHead`.
The queue head must match current queue memory.
The head receives real action and direction buffers; other slots receive the supplied dummy mask.
History input uses its current indirect pointer.
Masks remain unchanged until another supplied service replaces them.
These inputs do not synthesize physical input edges, analog history, or repeat behavior.

`helpers` supplies ordered external outcomes reached during the selected native service.
Each outcome contains `address`, exact `args`, signed or unsigned word `result`, `writes`, and `preservesOtherRegisters: true`.
A write contains an address and an array of unsigned bytes.
Optional `registerWrites` contains `{register, value}` entries; registers zero, stack pointer, and return address cannot change.
Optional `hi` and `lo` provide arithmetic-register effects.
Preservation applies to registers outside the declared effects.
Unknown helper targets, unmatched arguments, and unused outcomes stop execution.

Helpers outside the selected shared services retain their accepted external boundaries.
`releaseHelpers` supplies outcomes when callback flags request immediate native release after copyback.
`registeredOwners` supplies `{slot, ownerId}` identities for resources created by a native text registration control.
An unclaimed new active resource stops before later dependent services.

### Computed resource passes

The optional `externalProducers.value.resourceSchedule` replaces recorded dialogue service events:

```json
{
  "kind": "resident-resource-pass-v1",
  "directorSlot": 0,
  "directorCallback": 2149735100,
  "controller": {
    "throughPass": 89,
    "changes": [{"pass": 0, "actionMask": 0, "directionMask": 0, "historyMask": 0, "dummyMask": 0}]
  },
  "helperOutcomes": []
}
```

This candidate profile requires shared dialogue lifecycle and payload storage.
Its Director binding identifies the initialized resource in the current six-slot pool.
The callback value identifies RAM virtual address `0x80225ABC`, qualified against the current ROM.
The profile rejects recorded dialogue events, including ineligible events.
Other external producer events remain separate inputs.
Inputs without this profile preserve their existing service history behavior.

Each preview tick represents one declared resource pass with explicit Actor projection preparation.
The result reports this clock through `clockUnit`; it does not establish video frames or seconds.
The profile runs the queue opening, closing, and priority services when the current queue count is nonzero.
It then performs the native circular initialization scan and rebuilds priority.
Each successful initialization changes the scan's stopping point.
An initializer can register a lower slot that initializes during the same pass.

Callbacks execute in ascending pool-slot order, subject to current active flags, initialization, callback pointers, and the callback budget.
Priority determines queue ownership, not callback order.
The declared Director executes at its actual slot between lower and higher callbacks.
Resources registered by a callback remain uninitialized until the next pass.
The final priority rebuild publishes the changed pool.
The current parent-resource slot follows the executing callback and controls registration context.

The dispatcher supplies the temporary queue-head flag and controller pointers before dialogue execution.
It clears that flag after execution and uses the native immediate-release flag.
Controller changes start at pass zero, increase strictly, and contain unsigned halfword masks.
Each change remains active until the next change or `throughPass`.
The action mask also drives reached Director controller queries during the same pass.
This declaration does not compute physical edges, analog sampling, or controller repeat behavior.

`helperOutcomes` contains a single ordered stream for remaining external helpers.
Each row uses the existing exact-address, arguments, writes, and register-effect contract.
Supported residual helpers cover audio, formatting, copying, optional position adjustment, and text allocation.
Shared archive and free services must not receive recorded outcomes.
Missing, mismatched, or unused terminal outcomes stop execution.
The generated chair input retains audio outcomes; the second retail slice retains one formatter outcome.

The scheduler implements the qualified native control flow in JavaScript.
Queue, dialogue initializer, and dialogue callback bodies still execute ROM-verified native instructions in the bounded machine.
The Director, Actor updates, projection, lifecycle, and preview storage retain their documented implementations and limits.
The native comparison executes the actual resource gate and dispatcher with declared allocator boundaries.
The second comparison substitutes a bounded retail-parser callback for the complete scene wrapper.

Unknown resource callbacks, uninitialized Director resources, render-node attachment, and pending bulk cleanup stop with explicit diagnostics.
This profile ends immediately after a modeled terminal Director return, before outer resource teardown or the next scene load.
Color callback opportunities remain externally supplied; the resource pool does not produce that wrapper-internal order.
Initial scene, Actor, queue, metrics, and projection context remain required.
This profile does not establish ordinary ROM-selected scene initialization.

Run `node tests/cutscene-resource-scheduling.test.js` from the Editor directory.
The second retail slice takes 164 passes with Director slot zero and 163 passes with Director slot three.
Both results match independently executed native resource records and payloads.
The moved-slot case exposes the callback-order dependency without changing dialogue outcomes in a fixture.

## Results and boundaries

Pause queries read persistent resource state.
Control-byte queries read the established eight-byte bank and require complete writer history.
Resume requests change persistent pause flags and priority.
NUL and close requests preserve the active resource until native release.
Closing and callback release publish the native close result before clearing the resource.

The snapshot retains mutable native memory, current owners, and saved payload bytes within the existing storage ceiling.
The window overlay uses native rectangle edges and the selected visible history bytes.
Its font, colors, and glyph appearance remain approximate.
The overlay does not establish native glyph-image agreement or portrait rendering.

Portrait continuation states nine and ten remain explicit unsupported producer boundaries.
Unknown helper effects, unsupported storage modes, and missing reached memory remain distinct from native waits and modeled endings.
No supplied fixture establishes a natural scene launch, universal timing, audible sound, or complete retail rendering.
