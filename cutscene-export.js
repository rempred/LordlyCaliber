// Lordly Caliber - transactional fixed-slot and relocated Cutscene Studio export.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var RESOURCE_BASE_Z64 = 0x00594280;
  var RELOCATION_ARENA_START = 0x02780000;
  var RELOCATION_DESCRIPTOR_START = 0x027AF000;
  var RELOCATION_ARENA_END = 0x027B0000;
  var RELOCATION_DESCRIPTOR_SIZE =
    RELOCATION_ARENA_END - RELOCATION_DESCRIPTOR_START;
  var RELOCATION_FILL = 0xFF;

  function CutsceneExportError(message, code, details) {
    this.name = 'CutsceneExportError';
    this.message = message;
    this.code = code || 'cutscene-export-error';
    this.details = details || null;
  }
  CutsceneExportError.prototype = Object.create(Error.prototype);
  CutsceneExportError.prototype.constructor = CutsceneExportError;

  function fail(message, code, details) {
    throw new CutsceneExportError(message, code, details);
  }

  function stable(value) { return OB64.cutsceneModel.stableStringify(value, 0); }
  function equal(left, right) { return stable(left) === stable(right); }

  function readU32(input, offset) {
    return ((input[offset] << 24) | (input[offset + 1] << 16) |
      (input[offset + 2] << 8) | input[offset + 3]) >>> 0;
  }

  function writeU32(output, offset, value) {
    value = Number(value) >>> 0;
    output[offset] = value >>> 24;
    output[offset + 1] = value >>> 16;
    output[offset + 2] = value >>> 8;
    output[offset + 3] = value;
  }

  function equalBytes(left, right) {
    return OB64.cutsceneCodec.equalBytes(left, right);
  }

  function allByte(input, start, end, value) {
    for (var offset = start; offset < end; offset++) {
      if (input[offset] !== value) return false;
    }
    return true;
  }

  function relocationKey(entry) {
    var key = entry - RESOURCE_BASE_Z64;
    if ((entry & 1) || key < 0 || key >= 0x10000000 || (key & 1)) {
      fail('Cutscene relocation entry is outside the aligned resource-key range.',
        'relocation-key', { entry: entry });
    }
    return key;
  }

  function buildRelocationDescriptor(entries, dataEnd) {
    var descriptor = new Uint8Array(RELOCATION_DESCRIPTOR_SIZE);
    descriptor.fill(RELOCATION_FILL);
    descriptor.set([0x4F, 0x42, 0x43, 0x53], 0); // OBCS
    writeU32(descriptor, 4, 1);
    writeU32(descriptor, 8, entries.length);
    writeU32(descriptor, 12, dataEnd);
    writeU32(descriptor, 16, RESOURCE_BASE_Z64);
    writeU32(descriptor, 20, 0x01F3CA88);
    var cursor = 0x40;
    entries.forEach(function(entry) {
      var result = entry.result;
      if (cursor + 28 > descriptor.length) {
        fail('Cutscene relocation descriptor has no room for every allocation.',
          'relocation-descriptor-capacity');
      }
      writeU32(descriptor, cursor, Number(entry.scene.directorKey));
      writeU32(descriptor, cursor + 4, result.resourceKey);
      writeU32(descriptor, cursor + 8, result.relocationEntry);
      writeU32(descriptor, cursor + 12, result.encodedBytes);
      writeU32(descriptor, cursor + 16, result.selectorRows.length);
      writeU32(descriptor, cursor + 20, result.intendedDecodedBytes.length);
      writeU32(descriptor, cursor + 24,
        parseInt(result.intendedDecodedSha256.slice(0, 8), 16));
      cursor += 28;
    });
    return descriptor;
  }

  function resultWrites(result) {
    if (Array.isArray(result.writes)) return result.writes;
    if (!result.changedRange) return [];
    return [{
      start: result.changedRange.start,
      endExclusive: result.changedRange.endExclusive,
      originalBytes: result.originalSlotBytes,
      patchedBytes: result.candidateZ64.slice(
        result.changedRange.start, result.changedRange.endExclusive),
      label: 'fixed Director resource slot'
    }];
  }

  function clips(document) {
    var output = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        output[clip.id] = { track: track, clip: clip };
      });
    });
    return output;
  }

  function sourceDefinition(scene, clip) {
    var nodeId = clip && clip.source && clip.source.nodeId;
    if (!nodeId) return null;
    return scene.source.nodes.find(function(node) { return node.id === nodeId; }) || null;
  }

  function actorPlaceDefinition(scene, actor) {
    var nodeId = actor && actor.source && actor.source.placeNodeId;
    if (!nodeId) return null;
    return scene.source.nodes.find(function(node) { return node.id === nodeId; }) || null;
  }

  function insertionDefinition(scene, nodeId) {
    if (typeof nodeId !== 'string') return null;
    return scene.source.nodes.find(function(node) { return node.id === nodeId; }) || null;
  }

  function actorNativeProjection(actor) {
    var source = actor.source || {};
    return {
      slot: actor.slot,
      artSourceId: actor.artSourceId,
      initial: {
        x: actor.initial.x, y: actor.initial.y, z: actor.initial.z,
        facing: actor.initial.facing
      },
      source: {
        bank: source.bank,
        animationKey: source.animationKey,
        variantSelector: source.variantSelector,
        renderMode: source.renderMode
      }
    };
  }

  function nativeAudioBlockIndex(requestValue) {
    if (!Number.isInteger(requestValue) || requestValue < 0 || requestValue > 65 ||
        [1, 5, 15].indexOf(requestValue) !== -1) return null;
    return requestValue - [1, 5, 15].filter(function(emptyRow) {
      return emptyRow < requestValue;
    }).length;
  }

  function assertNativeBackgroundDelta(baseline, document) {
    var before = baseline.background;
    var after = document.background;
    if (equal(before, after)) return;
    var beforeProjection = before.projection || {};
    var afterProjection = after.projection || {};
    if (beforeProjection.nativeEditable !== true) {
      fail('This background route is available for preview only. Native selection requires a runtime-observed mode-0 scene group.',
        'preview-only-background');
    }
    var beforeKeys = Object.keys(beforeProjection).sort();
    var afterKeys = Object.keys(afterProjection).sort();
    if (!equal(beforeKeys, afterKeys)) {
      fail('The native background route metadata cannot change.', 'background-source-boundary');
    }
    var mutable = {
      mode: true,
      selectedSelector: true,
      selectedGroupResourceKey: true,
      previewOverride: true
    };
    beforeKeys.forEach(function(key) {
      if (!mutable[key] && !equal(beforeProjection[key], afterProjection[key])) {
        fail('The native background route metadata cannot change.',
          'background-source-boundary', { field: key });
      }
    });
    var groups = beforeProjection.nativeGroups || [];
    var group = groups.find(function(candidate) {
      return candidate.selector === afterProjection.selectedSelector;
    });
    if (!group || afterProjection.previewOverride !== false ||
        afterProjection.selectedGroupResourceKey !== group.groupResourceKey) {
      fail('Choose one complete native scene group from the background picker.',
        'background-group-selection');
    }
    if (after.capability !== 'native' ||
        after.assetId !== (group.archiveAssetIds[0] || null) ||
        afterProjection.mode !== (group.archiveAssetIds.length ? 'stage-fit' : 'unresolved') ||
        after.layers.length !== group.archiveAssetIds.length) {
      fail('The selected background must keep the complete ordered native scene group.',
        'background-group-shape');
    }
    after.layers.forEach(function(layer, index) {
      var source = layer.source || {};
      if (layer.assetId !== group.archiveAssetIds[index] || layer.visible !== true ||
          layer.depth !== index || layer.capability !== 'native' ||
          source.sourceKind !== 'native-scene-group' ||
          source.selector !== group.selector ||
          source.groupResourceKey !== group.groupResourceKey ||
          source.traversalOrdinal !== index) {
        fail('Native background layers cannot be hidden, reordered, or split from their scene group.',
          'background-group-shape', { layer: index });
      }
    });
  }

  function assertFixedSlotDelta(scene, baseline, document) {
    OB64.cutsceneModel.validateSceneDocument(document);
    if (!equal(baseline.identity, document.identity)) {
      fail('Scene identity cannot change inside an existing physical cutscene.', 'identity-change');
    }
    if (!equal(baseline.native, document.native)) {
      fail('Preserved native commands or source boundaries changed.', 'native-boundary');
    }
    assertNativeBackgroundDelta(baseline, document);
    if (!equal(baseline.branches, document.branches)) {
      fail('Branch authoring is ready for preview, but native condition export needs a reviewed adapter.',
        'preview-only-branch');
    }

    var beforeActors = {};
    var afterActors = {};
    var reservedSlots = {};
    baseline.actors.forEach(function(actor) { beforeActors[actor.id] = actor; });
    baseline.actors.forEach(function(actor) {
      if (Number.isInteger(actor.slot)) reservedSlots[actor.slot] = true;
    });
    document.actors.forEach(function(actor) { afterActors[actor.id] = actor; });
    Object.keys(beforeActors).forEach(function(actorId) {
      var oldActor = beforeActors[actorId];
      var nextActor = afterActors[actorId];
      if (!nextActor) return;
      var oldSource = oldActor.source || {};
      var nextSource = nextActor.source || {};
      if (oldActor.slot !== nextActor.slot) {
        fail('A source actor cannot move to another native slot.', 'actor-slot-change', {
          actorId: actorId
        });
      }
      if (oldActor.initial.visible !== nextActor.initial.visible) {
        fail('Use an Enter or Exit step to change actor visibility.', 'actor-visibility-change', {
          actorId: actorId
        });
      }
      if ((oldSource.placeNodeId || null) !== (nextSource.placeNodeId || null) ||
          (oldSource.authored === true) !== (nextSource.authored === true) ||
          (oldSource.insertBeforeNodeId || null) !== (nextSource.insertBeforeNodeId || null)) {
        fail('Actor source ownership and native insertion boundaries cannot change.',
          'actor-source-boundary', { actorId: actorId });
      }
      if (!equal(actorNativeProjection(oldActor), actorNativeProjection(nextActor))) {
        var place = actorPlaceDefinition(scene, oldActor);
        if (!place || place.opcodeU32 !== 0x14 || place.editPolicy !== 'typed-place' ||
            place.controlEntryAlias) {
          fail('This actor has no exact editable Place command for direct placement or art changes.',
            'actor-place-boundary', { actorId: actorId });
        }
      }
    });
    Object.keys(afterActors).forEach(function(actorId) {
      if (!beforeActors[actorId]) {
        var actor = afterActors[actorId];
        var source = actor.source || {};
        var boundary = insertionDefinition(scene, source.insertBeforeNodeId);
        if (source.authored !== true || source.placeNodeId || !Number.isInteger(actor.slot) ||
            actor.slot < 0 || actor.slot > 19 || reservedSlots[actor.slot] ||
            actor.initial.visible !== true || actor.capability !== 'native' ||
            !boundary || boundary.insertBefore !== true || boundary.nodeType === 'gap' ||
            boundary.editPolicy === 'immutable-gap' || boundary.controlEntryAlias) {
          fail('This actor has no safe native Place insertion boundary.',
            'actor-place-insertion', { actorId: actorId });
        }
      }
    });

    var before = clips(baseline);
    var after = clips(document);
    var reservedEffectSlots = {};
    Object.keys(before).forEach(function(id) {
      var row = before[id];
      if (row.clip.kind === 'effect' &&
          row.clip.payload.sourceSystem === 'cutscene-sprite-native' &&
          Number.isInteger(row.clip.payload.nativeEffectSlot)) {
        reservedEffectSlots[row.clip.payload.nativeEffectSlot] = true;
      }
    });
    var authoredEffectSlots = {};
    Object.keys(before).forEach(function(id) {
      var oldRow = before[id];
      var nextRow = after[id];
      var definition = sourceDefinition(scene, oldRow.clip);
      var expectedPolicy = null;
      if (oldRow.clip.kind === 'pose' && oldRow.clip.payload.nativeApplied === true) {
        expectedPolicy = 'typed-state';
      }
      else if (oldRow.clip.kind === 'movement') expectedPolicy = 'typed-move';
      else if (oldRow.clip.kind === 'wait' &&
          oldRow.clip.payload.registeredCounterEditable === true) {
        expectedPolicy = 'registered-counter-wait';
      }
      else if (oldRow.clip.kind === 'camera' &&
          oldRow.clip.payload.presentationKind === 'projection-transform') {
        expectedPolicy = 'typed-projection-transform';
      } else if (oldRow.clip.kind === 'effect' &&
          oldRow.clip.payload.sourceSystem === 'cutscene-sprite-native') {
        expectedPolicy = 'typed-native-sprite-effect';
      } else if (oldRow.clip.kind === 'opacity' &&
          oldRow.clip.payload.nativeSlotSelector !== -1) {
        expectedPolicy = 'typed-actor-opacity';
      } else if (oldRow.clip.kind === 'audio' &&
          oldRow.clip.payload.nativeBlockRequestEditable === true) {
        expectedPolicy = 'typed-audio-block-request';
      }
      var editable = definition && (expectedPolicy === 'registered-counter-wait'
        ? definition.name === 'registered_counter_query' && definition.wordCount === 3
        : definition.editPolicy === expectedPolicy);
      if (!editable) {
        var removedActorClip = oldRow.track.actorId != null &&
          !afterActors[oldRow.track.actorId] && !nextRow;
        if (removedActorClip) return;
        if (!nextRow || !equal(oldRow.clip, nextRow.clip) ||
            oldRow.track.type !== nextRow.track.type ||
            oldRow.track.actorId !== nextRow.track.actorId) {
          fail('“' + (oldRow.track.label || oldRow.clip.kind) +
            '” comes from a display-only command and must remain unchanged.',
          'immutable-clip', { clipId: id });
        }
        return;
      }
      if (!nextRow) return;
      if (nextRow.clip.capability !== 'native') {
        fail('An edited source clip is not marked Native.', 'clip-capability', { clipId: id });
      }
      if (nextRow.clip.startFrame !== oldRow.clip.startFrame) {
        fail('A source-backed clip cannot move away from its native command boundary.',
          'source-timing', { clipId: id });
      }
      if (!equal(nextRow.clip.pathIds, oldRow.clip.pathIds)) {
        fail('Native branch assignment for source clips is not available yet.',
          'preview-only-branch', { clipId: id });
      }
      if (oldRow.clip.kind === 'pose' &&
          nextRow.clip.durationFrames !== oldRow.clip.durationFrames) {
        fail('Pose display length is derived from later commands. Add a Hold instead of resizing this pose.',
          'pose-duration', { clipId: id });
      }
      if (oldRow.clip.kind === 'effect' &&
          oldRow.clip.payload.sourceSystem === 'cutscene-sprite-native') {
        if (nextRow.clip.payload.sourceSystem !== 'cutscene-sprite-native') {
          fail('A source-backed native sprite effect cannot become a Preview-only effect source.',
            'effect-source-system', { clipId: id });
        }
        if (nextRow.clip.durationFrames !== oldRow.clip.durationFrames) {
          fail('A source sprite-effect lifetime comes from its later replacement or cleanup command.',
            'effect-duration', { clipId: id });
        }
        if (nextRow.clip.payload.nativeEffectSlot !== oldRow.clip.payload.nativeEffectSlot) {
          fail('A source sprite effect must keep its slot so existing cleanup commands remain exact.',
            'effect-slot-change', { clipId: id });
        }
      }
      if (oldRow.clip.kind === 'audio' &&
          oldRow.clip.payload.nativeBlockRequestEditable === true) {
        var audioPayload = nextRow.clip.payload || {};
        var blockIndex = nativeAudioBlockIndex(audioPayload.nativeRequestValue);
        if (audioPayload.sourceSystem !== 'director-native' ||
            audioPayload.nativeOpcode !== '0x6E' ||
            audioPayload.nativeBlockRequestEditable !== true ||
            !equal(audioPayload.originalNativeAudio,
              oldRow.clip.payload.originalNativeAudio) ||
            nextRow.clip.durationFrames !== oldRow.clip.durationFrames ||
            blockIndex === null || audioPayload.audioBlockId !==
              'sequenced-audio:' + String(blockIndex).padStart(2, '0') ||
            !Array.isArray(audioPayload.nativeOperands) ||
            audioPayload.nativeOperands[0] !== 0 || audioPayload.nativeOperands[1] !== 0 ||
            audioPayload.nativeOperands[2] !== audioPayload.nativeRequestValue) {
          fail('A native audio substitution must keep the selector-0 controller route and choose one non-empty request row.',
            'audio-block-request', { clipId: id });
        }
      }
      if (oldRow.clip.kind === 'movement' &&
          !equal(nextRow.clip.payload.from, oldRow.clip.payload.from)) {
        fail('A native Move stores its destination and speed, not an independent start point.',
          'movement-start', { clipId: id });
      }
    });

    Object.keys(after).forEach(function(id) {
      if (before[id]) return;
      var row = after[id];
      if (['pose', 'movement', 'wait', 'camera', 'enter', 'exit', 'effect',
          'opacity'].indexOf(row.clip.kind) === -1 ||
          row.clip.capability !== 'native' ||
          !row.clip.source || typeof row.clip.source.insertBeforeNodeId !== 'string') {
        fail('“' + (row.track.label || row.clip.kind) +
          '” is available in preview but has no native Director command adapter.',
        'preview-only-clip', { clipId: id, kind: row.clip.kind });
      }
      var target = insertionDefinition(scene, row.clip.source.insertBeforeNodeId);
      if (!target || target.insertBefore !== true || target.nodeType === 'gap' ||
          target.editPolicy === 'immutable-gap' || target.controlEntryAlias) {
        fail('An authored clip lost its approved insertion boundary.',
          'insertion-boundary', { clipId: id });
      }
      if ((row.clip.kind === 'enter' || row.clip.kind === 'exit' ||
          row.clip.kind === 'opacity') &&
          (row.track.actorId == null || !afterActors[row.track.actorId])) {
        fail('An actor lifecycle or presentation step must target an active actor slot.',
          'actor-lifecycle-target', { clipId: id });
      }
      if (row.clip.kind === 'camera' &&
          (row.clip.payload.presentationKind !== 'projection-transform' ||
            row.clip.payload.from != null)) {
        fail('A native projection transition derives its starting values from current projection state.',
          'projection-start', { clipId: id });
      }
      if (row.clip.kind === 'effect') {
        var effectSlot = row.clip.payload.nativeEffectSlot;
        if (row.clip.payload.sourceSystem !== 'cutscene-sprite-native' ||
            !Number.isInteger(effectSlot) || effectSlot < 0 || effectSlot > 29 ||
            reservedEffectSlots[effectSlot] || authoredEffectSlots[effectSlot]) {
          fail('An authored native sprite effect needs one unused effect slot from 0 through 29.',
            'effect-slot', { clipId: id, slot: effectSlot });
        }
        if (!Number.isInteger(row.clip.durationFrames) || row.clip.durationFrames < 1) {
          fail('An authored native sprite effect needs a positive native lifetime.',
            'effect-duration', { clipId: id });
        }
        authoredEffectSlots[effectSlot] = true;
      }
    });
  }

  function assessFixedSlotDelta(scene, baseline, document, source) {
    try {
      assertFixedSlotDelta(scene, baseline, document);
      var compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, document);
      var encodedBytes = compiled.noOp ? source.consumedEncodedBytes :
        OB64.cutsceneCodec.encodeCustomLzOptimal(compiled.decodedBytes).length;
      if (encodedBytes > scene.source.storedPayloadLength) {
        var maximumRelocatedPayload = RELOCATION_DESCRIPTOR_START -
          RELOCATION_ARENA_START - 4;
        if (encodedBytes <= maximumRelocatedPayload) {
          return {
            capability: 'native',
            reasons: ['This edit exceeds its retail slot and will use the Cutscene relocation arena.'],
            allocationBytes: encodedBytes,
            features: ['director-relocation-required', 'compressed-budget-measured']
          };
        }
        return {
          capability: 'preview-only',
          reasons: ['This edit needs ' + encodedBytes +
            ' compressed bytes and exceeds the Cutscene relocation arena.'],
          allocationBytes: encodedBytes,
          features: ['director-relocation-overflow', 'compressed-budget-measured']
        };
      }
      return {
        capability: 'native',
        reasons: baseline.exportRequirements.reasons.slice(),
        allocationBytes: encodedBytes,
        features: ['director-fixed-slot', 'source-preimage-verified', 'compressed-budget-measured']
      };
    } catch (error) {
      return {
        capability: 'preview-only',
        reasons: [error && error.message || String(error)],
        allocationBytes: 0,
        features: ['director-fixed-slot']
      };
    }
  }

  // The adapter rules are shared by in-place and relocated payloads. Keep the
  // older name as a compatibility alias for existing Projects and callers.
  function assessNativeDelta(scene, baseline, document, source) {
    return assessFixedSlotDelta(scene, baseline, document, source);
  }

  function editedEntries(state) {
    if (!state || !OB64.cutsceneProject) return [];
    return OB64.cutsceneProject.editedScenes(state).map(function(scene) {
      var history = state.histories[scene.storageId];
      var baselineText = state.originalSerialized[scene.storageId];
      if (!history || typeof baselineText !== 'string') {
        fail('Cutscene “' + OB64.cutsceneCatalog.displayName(scene) +
          '” has no verified Project baseline.', 'missing-baseline');
      }
      var baseline = OB64.cutsceneModel.parseSceneDocument(JSON.parse(baselineText));
      return {
        scene: scene,
        baseline: baseline,
        document: OB64.cutsceneModel.cloneSceneDocument(history.present)
      };
    });
  }

  function prepare(sourceRom, candidateRom, options) {
    options = options || {};
    if (!sourceRom || !candidateRom || sourceRom === candidateRom ||
        !(candidateRom.z64 instanceof Uint8Array) || candidateRom.z64 === sourceRom.z64) {
      return Promise.reject(new CutsceneExportError(
        'Cutscene export requires one detached normalized ROM candidate.', 'detached-candidate'));
    }
    if (!sourceRom.layout || sourceRom.layout.id !== 'us-rev0') {
      return Promise.reject(new CutsceneExportError(
        'Cutscene export currently supports only Ogre Battle 64 US Rev 0.', 'unsupported-revision'));
    }
    var state = sourceRom.cutsceneStudio;
    var entries;
    try {
      entries = editedEntries(state).sort(function(left, right) {
        return left.scene.source.z64PrefixStart - right.scene.source.z64PrefixStart;
      });
      entries.forEach(function(entry) {
        assertFixedSlotDelta(entry.scene, entry.baseline, entry.document);
        var knownSource = state.sourceByAssetId[entry.scene.assetId];
        if (!knownSource) {
          fail('Cutscene source bytes are unavailable for relocation planning.',
            'missing-source', { assetId: entry.scene.assetId });
        }
        entry.assessment = assessFixedSlotDelta(
          entry.scene, entry.baseline, entry.document, knownSource);
        if (entry.assessment.capability !== 'native') {
          fail(entry.assessment.reasons.join(' '), 'export-capability', {
            assetId: entry.scene.assetId
          });
        }
      });
    } catch (error) {
      return Promise.reject(error);
    }
    var relocationCursor = RELOCATION_ARENA_START;
    var relocationCount = 0;
    try {
      entries.forEach(function(entry) {
        if (entry.assessment.allocationBytes <= entry.scene.source.storedPayloadLength) return;
        relocationCursor = (relocationCursor + 1) & ~1;
        var end = relocationCursor + 4 + entry.assessment.allocationBytes;
        end = (end + 1) & ~1;
        if (end > RELOCATION_DESCRIPTOR_START) {
          fail('Edited Cutscenes exceed the shared relocation arena.',
            'relocation-capacity', { requiredEnd: end });
        }
        entry.allocation = {
          entry: relocationCursor,
          endExclusive: end,
          key: relocationKey(relocationCursor)
        };
        relocationCursor = end;
        relocationCount++;
      });
      if (relocationCount) {
        if (candidateRom.z64.length < RELOCATION_ARENA_END ||
            !allByte(candidateRom.z64, RELOCATION_ARENA_START,
              RELOCATION_ARENA_END, RELOCATION_FILL)) {
          fail('The Cutscene relocation arena is not unused retail 0xFF fill.',
            'relocation-owner');
        }
      }
    } catch (error) {
      return Promise.reject(error);
    }
    var planned = [];
    var chain = Promise.resolve();
    entries.forEach(function(entry) {
      chain = chain.then(function() {
        var knownSource = state.sourceByAssetId[entry.scene.assetId];
        var planOptions = Object.assign({}, options, {
          expectedDecodedSha256: knownSource && knownSource.decodedSha256 ||
            entry.scene.source.decodedSha256
        });
        var planner = entry.allocation
          ? OB64.cutsceneCodec.planRelocatedExport(
            candidateRom.z64, entry.scene, entry.document, entry.allocation, planOptions)
          : OB64.cutsceneCodec.planFixedCapacityExport(
            candidateRom.z64, entry.scene, entry.document, planOptions);
        return planner.then(function(result) {
          if (!entry.allocation) result.placement = 'fixed';
          result.writes = resultWrites(result);
          result.changedRanges = result.writes.map(function(write) {
            return { start: write.start, endExclusive: write.endExclusive };
          });
          planned.push({
            scene: entry.scene,
            baseline: entry.baseline,
            document: entry.document,
            result: result
          });
        });
      });
    });
    return chain.then(function() {
      var changed = planned.filter(function(entry) { return !entry.result.noOp; });
      var relocated = changed.filter(function(entry) {
        return entry.result.placement === 'relocated';
      });
      var descriptorWrite = null;
      if (relocated.length) {
        var descriptor = buildRelocationDescriptor(relocated, relocationCursor);
        descriptorWrite = {
          start: RELOCATION_DESCRIPTOR_START,
          endExclusive: RELOCATION_ARENA_END,
          originalBytes: candidateRom.z64.slice(
            RELOCATION_DESCRIPTOR_START, RELOCATION_ARENA_END),
          patchedBytes: descriptor,
          label: 'Cutscene relocation ownership descriptor'
        };
      }
      var claims = [];
      changed.forEach(function(entry) {
        entry.result.writes.forEach(function(write) {
          claims.push({ write: write, assetId: entry.scene.assetId });
        });
      });
      if (descriptorWrite) claims.push({ write: descriptorWrite, assetId: 'descriptor' });
      for (var left = 0; left < claims.length; left++) {
        for (var right = left + 1; right < claims.length; right++) {
          var a = claims[left].write;
          var b = claims[right].write;
          if (a.start < b.endExclusive && b.start < a.endExclusive) {
            fail('Two Cutscene writes claim overlapping ROM ranges.', 'physical-alias', {
              left: claims[left].assetId,
              right: claims[right].assetId
            });
          }
        }
      }
      return {
        schemaVersion: 2,
        entries: planned,
        changedEntries: changed,
        relocatedEntries: relocated,
        relocationDescriptorWrite: descriptorWrite,
        relocationArena: relocated.length ? {
          start: RELOCATION_ARENA_START,
          dataEnd: relocationCursor,
          descriptorStart: RELOCATION_DESCRIPTOR_START,
          endExclusive: RELOCATION_ARENA_END
        } : null,
        changedSceneCount: changed.length,
        relocatedSceneCount: relocated.length,
        editSceneCount: planned.length
      };
    });
  }

  function patchOwner(plan) {
    if (!plan || !plan.changedEntries || !plan.changedEntries.length) return null;
    var regions = [];
    plan.changedEntries.forEach(function(entry) {
      entry.result.writes.forEach(function(write) {
        regions.push({
          kind: 'rom',
          start: write.start,
          size: write.endExclusive - write.start,
          label: OB64.cutsceneCatalog.displayName(entry.scene) + ' · ' + write.label
        });
      });
    });
    if (plan.relocationDescriptorWrite) {
      regions.push({
        kind: 'rom',
        start: plan.relocationDescriptorWrite.start,
        size: plan.relocationDescriptorWrite.endExclusive -
          plan.relocationDescriptorWrite.start,
        label: plan.relocationDescriptorWrite.label
      });
    }
    return {
      id: 'cutscene-director-resources',
      name: 'Cutscene Studio Director resources',
      category: 'cutscenes',
      regions: regions
    };
  }

  function apply(candidateRom, plan) {
    if (!plan || !Array.isArray(plan.changedEntries)) {
      fail('A prepared Cutscene export plan is required.', 'missing-plan');
    }
    var writes = [];
    plan.changedEntries.forEach(function(entry) {
      entry.result.writes.forEach(function(write) {
        writes.push({ write: write, sceneId: entry.result.sceneId });
      });
    });
    if (plan.relocationDescriptorWrite) {
      writes.push({ write: plan.relocationDescriptorWrite, sceneId: 'relocation-descriptor' });
    }
    writes.forEach(function(row) {
      var write = row.write;
      var live = candidateRom.z64.slice(write.start, write.endExclusive);
      if (!equalBytes(live, write.originalBytes)) {
        fail('Another export feature changed a Cutscene-owned ROM range after planning.',
          'candidate-preimage', { sceneId: row.sceneId, label: write.label });
      }
    });
    writes.forEach(function(row) {
      candidateRom.z64.set(row.write.patchedBytes, row.write.start);
    });
    return {
      changedSceneCount: plan.changedEntries.length,
      relocatedSceneCount: plan.relocatedSceneCount || 0,
      changes: plan.changedEntries.reduce(function(total, entry) {
        return total + entry.result.changes.length;
      }, 0)
    };
  }

  function validateApplied(candidateRom, plan) {
    if (!plan || !Array.isArray(plan.changedEntries)) {
      fail('A prepared Cutscene export plan is required for readback.', 'missing-plan');
    }
    plan.changedEntries.forEach(function(entry) {
      var scene = entry.scene;
      var source = scene.source;
      var result = entry.result;
      var payload;
      if (result.placement === 'relocated') {
        if (readU32(candidateRom.z64, result.relocationEntry) !== result.encodedBytes) {
          fail('Relocated Cutscene size prefix changed after export.',
            'readback-prefix', { sceneId: scene.sceneId });
        }
        result.selectorWordZ64.forEach(function(offset) {
          if (readU32(candidateRom.z64, offset) !== result.resourceKey) {
            fail('A Director selector does not point to its relocated Cutscene.',
              'readback-selector', { sceneId: scene.sceneId, z64Offset: offset });
          }
        });
        payload = candidateRom.z64.slice(
          result.relocationEntry + 4,
          result.relocationEntry + 4 + result.encodedBytes);
      } else {
        if (readU32(candidateRom.z64, source.z64PrefixStart) !==
            source.storedPayloadLength) {
          fail('Cutscene capacity prefix changed after export.', 'readback-prefix', {
            sceneId: scene.sceneId
          });
        }
        payload = candidateRom.z64.slice(
          source.z64PayloadStart, source.z64PayloadStart + source.storedPayloadLength);
      }
      var decoded = OB64.cutsceneCodec.decodeCustomLz(payload, {
        requireExact: result.placement === 'relocated',
        allowZeroPadding: result.placement !== 'relocated'
      });
      if (!equalBytes(decoded.bytes, result.intendedDecodedBytes)) {
        fail('Finished ROM Cutscene readback differs from the planned scene.',
          'semantic-readback', { sceneId: scene.sceneId });
      }
    });
    return {
      summary: plan.changedEntries.length + ' Cutscene Director payload' +
        (plan.changedEntries.length === 1 ? '' : 's') + ' reparsed exactly; ' +
        (plan.relocatedSceneCount || 0) + ' relocated.',
      details: {
        sceneCount: plan.changedEntries.length,
        relocatedSceneCount: plan.relocatedSceneCount || 0,
        scenes: plan.changedEntries.map(function(entry) {
          return {
            sceneId: entry.scene.sceneId,
            assetId: entry.scene.assetId,
            placement: entry.result.placement,
            encodedBytes: entry.result.encodedBytes,
            capacityBytes: entry.result.capacityBytes,
            intendedDecodedSha256: entry.result.intendedDecodedSha256
          };
        })
      }
    };
  }

  function adopt(rom, plan) {
    var state = rom && rom.cutsceneStudio;
    if (!state || !plan) return;
    // Cutscene Projects remain detached authoring edits after download. Restore
    // every Cutscene-owned preimage in the in-memory source ROM so another
    // export starts from the same verified retail resource and selector table.
    // The downloaded candidate retains the compiled fixed or relocated bytes.
    plan.changedEntries.forEach(function(entry) {
      var result = entry.result;
      result.writes.forEach(function(write) {
        rom.z64.set(write.originalBytes, write.start);
      });
    });
    if (plan.relocationDescriptorWrite) {
      rom.z64.set(plan.relocationDescriptorWrite.originalBytes,
        plan.relocationDescriptorWrite.start);
    }
  }

  OB64.cutsceneExport = {
    CutsceneExportError: CutsceneExportError,
    assertFixedSlotDelta: assertFixedSlotDelta,
    assessFixedSlotDelta: assessFixedSlotDelta,
    assessNativeDelta: assessNativeDelta,
    prepare: prepare,
    patchOwner: patchOwner,
    apply: apply,
    validateApplied: validateApplied,
    adopt: adopt
  };
})(window.OB64);
