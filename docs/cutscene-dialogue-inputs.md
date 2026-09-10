# Native dialogue inputs

Native dialogue playback uses explicit initial state and ordered service history.
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

`dialogueCreates` contains `{nodeId, occurrence, ownerId, slot, directorWords, recordHex}` entries.
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

Initialization and callbacks require `storage.saveReturned: true` and a nonzero `storage.saveHandle`.
The destination allocation must provide the six-byte header and complete payload extent.
Callbacks also require the matching `storage.restoreHandle`, `restoreReturned: true`, and `freeReturned: true`.
These fields qualify allocator outcomes; the runtime does not simulate the allocation pool.
Native payload bytes, length fields, and the current handle are published after the service.

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

Allocator, archive, formatter, copy, audio, registration, and optional position services retain their accepted external boundaries.
`releaseHelpers` supplies outcomes when callback flags request immediate native release after copyback.
`registeredOwners` supplies `{slot, ownerId}` identities for resources created by a native text registration control.
An unclaimed new active resource stops before later dependent services.

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
