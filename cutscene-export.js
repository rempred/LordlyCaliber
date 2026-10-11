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
  var RELOCATION_ALIGN = 2;
  var DIRECTOR_TABLE_Z64 = 0x01F3CA88;
  var DIRECTOR_TABLE_ROWS = 1693;
  var DIALOGUE_DIRECTORY_Z64 = RESOURCE_BASE_Z64 + 0x1A3B7B2;
  // Parent event directory: a length word, then 115 resource keys.
  var EVENT_DIRECTORY_Z64 = 0x009398E8;
  var EVENT_ROWS = 115;
  function eventWordZ64(row) { return EVENT_DIRECTORY_Z64 + 4 + row * 4; }
  // Arena record kinds live in the top nibble of word 0 (reserved and zero in
  // version 1): (kind << 28) | owner. Words 1-6 keep their version-1 meaning.
  var KIND = Object.freeze({ RETAIL: 0, AUTHORED: 1, EVENT_ROW: 2, MANIFEST: 3 });
  function recordKind(row) { return row[0] >>> 28; }
  function recordOwner(row) { return row[0] & 0x0FFFFFFF; }

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

  function readRelocationArena(z64) {
    if (z64.length < RELOCATION_ARENA_END) {
      fail('The ROM has no Cutscene allocation arena.', 'relocation-owner');
    }
    if (allByte(z64, RELOCATION_ARENA_START, RELOCATION_ARENA_END, RELOCATION_FILL)) {
      return { version: 0, records: [], dataEnd: RELOCATION_ARENA_START };
    }
    var start = RELOCATION_DESCRIPTOR_START;
    var version = readU32(z64, start + 4);
    var count = readU32(z64, start + 8);
    var dataEnd = readU32(z64, start + 12);
    if (readU32(z64, start) !== 0x4F424353 || (version !== 1 && version !== 2) ||
        readU32(z64, start + 16) !== RESOURCE_BASE_Z64 || count > 144 ||
        dataEnd < RELOCATION_ARENA_START || dataEnd > start || (dataEnd & 1) ||
        !allByte(z64, dataEnd, start, RELOCATION_FILL)) {
      fail('The Cutscene arena contains unrecognized data.', 'relocation-owner');
    }
    var records = [];
    for (var index = 0; index < count; index++) {
      var row = [];
      for (var word = 0; word < 7; word++) row.push(readU32(z64, start + 0x40 + index * 28 + word * 4));
      var kind = recordKind(row), owner = recordOwner(row);
      // Uniqueness is per (kind, owner): word 0 carries both.
      if ((version === 1 && kind !== KIND.RETAIL) ||
          row[2] !== row[1] + RESOURCE_BASE_Z64 ||
          row[2] < RELOCATION_ARENA_START || (row[2] & 1) || row[3] < 1 ||
          row[2] + 4 + row[3] > dataEnd || readU32(z64, row[2]) !== row[3] ||
          records.some(function(other) {
            return other[0] === row[0] || other[2] < row[2] + 4 + row[3] && row[2] < other[2] + 4 + other[3];
          })) {
        fail('A Cutscene allocation record is invalid.', 'relocation-owner');
      }
      if (kind === KIND.EVENT_ROW) {
        if (owner >= EVENT_ROWS || readU32(z64, eventWordZ64(owner)) !== row[1] || row[4] !== 1) {
          fail('A relocated event row does not own its event directory row.', 'relocation-owner');
        }
      } else if (kind === KIND.AUTHORED) {
        if (owner >= DIRECTOR_TABLE_ROWS || readU32(z64, DIRECTOR_TABLE_Z64 + owner * 4) !== row[1] ||
            row[4] !== 1) {
          fail('An authored Cutscene record does not own its selector row.', 'relocation-owner');
        }
      } else if (kind === KIND.MANIFEST) {
        if (owner !== 0 || row[4] !== 0 || records.some(function(other) { return recordKind(other) === KIND.MANIFEST; })) {
          fail('The Cutscene arena manifest record is invalid.', 'relocation-owner');
        }
      } else if (kind !== KIND.RETAIL) {
        fail('A Cutscene allocation record has an unknown kind.', 'relocation-owner');
      }
      records.push(row);
    }
    return { version: version, records: records, dataEnd: dataEnd };
  }

  // Every word that references an arena resource lives in the Director
  // top-level selector table, the dialogue archive directory or the parent
  // event directory. Arena keys point inside the arena, so no retail word can
  // match one by accident.
  function referenceWords(z64, key) {
    var words = [];
    for (var eventRow = 0; eventRow < EVENT_ROWS; eventRow++) {
      if (readU32(z64, eventWordZ64(eventRow)) === key) words.push(eventWordZ64(eventRow));
    }
    for (var row = 0; row < DIRECTOR_TABLE_ROWS; row++) {
      if (readU32(z64, DIRECTOR_TABLE_Z64 + row * 4) === key) words.push(DIRECTOR_TABLE_Z64 + row * 4);
    }
    var dialogueBytes = readU32(z64, DIALOGUE_DIRECTORY_Z64);
    if (dialogueBytes % 4 || dialogueBytes > 0x10000) fail('The dialogue archive directory is invalid.', 'relocation-owner');
    for (var at = DIALOGUE_DIRECTORY_Z64 + 4; at < DIALOGUE_DIRECTORY_Z64 + 4 + dialogueBytes; at += 4) {
      if (readU32(z64, at) === key) words.push(at);
    }
    return words;
  }

  function utf8(text) {
    if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(text);
    var escaped = unescape(encodeURIComponent(text));
    return Uint8Array.from(escaped, function(character) { return character.charCodeAt(0); });
  }

  function readManifest(z64, row) {
    var bytes = z64.slice(row[2] + 4, row[2] + 4 + row[3]);
    var text = typeof TextDecoder !== 'undefined' ? new TextDecoder().decode(bytes)
      : decodeURIComponent(escape(String.fromCharCode.apply(null, bytes)));
    var manifest;
    try { manifest = JSON.parse(text); } catch (error) {
      fail('The Cutscene arena manifest is not readable.', 'relocation-owner');
    }
    if (!manifest || manifest.format !== 'ob64-cutscene-arena-manifest' || manifest.version !== 1 ||
        !Array.isArray(manifest.authoredScenes) || manifest.authoredScenes.some(function(entry) {
          return !entry || !/^[0-9a-f]{32}$/.test(entry.authoredId) || typeof entry.name !== 'string' ||
            !Number.isInteger(entry.selectorRow) || entry.selectorRow < 0 || entry.selectorRow >= DIRECTOR_TABLE_ROWS ||
            entry.sceneClass !== 5 || typeof entry.templateSceneId !== 'string';
        })) {
      fail('The Cutscene arena manifest is malformed.', 'relocation-owner');
    }
    if (manifest.eventBindings !== undefined && (!Array.isArray(manifest.eventBindings) ||
        manifest.eventBindings.some(function(binding) {
          var target = binding && binding.target;
          return typeof binding.launchSiteId !== 'string' || !target ||
            !(target.kind === 'authored' && /^[0-9a-f]{32}$/.test(target.authoredId)) &&
            !(target.kind === 'physical' && typeof target.sceneId === 'string' && Number.isInteger(target.selectorRow));
        }))) {
      fail('The Cutscene arena manifest has malformed event bindings.', 'relocation-owner');
    }
    if (manifest.wipedScenes !== undefined && (!Array.isArray(manifest.wipedScenes) ||
        manifest.wipedScenes.some(function(entry) {
          var recipe = entry && entry.actorRecipe;
          return !entry || typeof entry.sceneId !== 'string' ||
            ['director-release', 'presentation-handoff'].indexOf(entry.exitKind) < 0 ||
            (recipe !== undefined && (!recipe || typeof recipe !== 'object' ||
              ['bank', 'animationKey', 'nativeFacing', 'renderMode', 'variantSelector'].some(function(key) {
                return !Number.isInteger(recipe[key]);
              }) || ['x', 'y', 'z'].some(function(key) { return typeof recipe[key] !== 'number' || !isFinite(recipe[key]); }) ||
              ['transformChannel', 'uniformScalePercent'].some(function(key) {
                return recipe[key] !== null && recipe[key] !== undefined && !Number.isInteger(recipe[key]);
              })));
        }))) {
      fail('The Cutscene arena manifest has malformed wiped scenes.', 'relocation-owner');
    }
    var ids = {}, rows = {};
    manifest.authoredScenes.forEach(function(entry) {
      if (ids[entry.authoredId] || rows[entry.selectorRow]) fail('The Cutscene arena manifest repeats an authored scene.', 'relocation-owner');
      ids[entry.authoredId] = rows[entry.selectorRow] = true;
    });
    return manifest;
  }

  function alignedExtent(start, size) {
    var entry = (start + RELOCATION_ALIGN - 1) & ~(RELOCATION_ALIGN - 1);
    return { entry: entry, endExclusive: (entry + 4 + size + 1) & ~1 };
  }

  // Build a detached base ROM whose arena keeps every valid owned record that
  // this export does not replace, packed from the arena start, with each of its
  // references repointed through one relocation map. Nothing is published here.
  function compactArena(z64, arena, supersededKeys) {
    var base = z64.slice();
    base.fill(RELOCATION_FILL, RELOCATION_ARENA_START, RELOCATION_DESCRIPTOR_START);
    var cursor = RELOCATION_ARENA_START, kept = [], moves = [], referenceWrites = [];
    arena.records.forEach(function(row) {
      if (supersededKeys.has(row[1])) return;
      var kind = recordKind(row);
      var references = kind === KIND.MANIFEST ? [] : referenceWords(z64, row[1]);
      if ((kind === KIND.RETAIL && references.length !== row[4]) ||
          (kind === KIND.AUTHORED && (references.length !== 1 ||
            references[0] !== DIRECTOR_TABLE_Z64 + recordOwner(row) * 4)) ||
          (kind === KIND.EVENT_ROW && (references.length !== 1 ||
            references[0] !== eventWordZ64(recordOwner(row))))) {
        fail('A Cutscene arena record does not match its references.', 'relocation-owner');
      }
      var extent = alignedExtent(cursor, row[3]);
      if (extent.endExclusive > RELOCATION_DESCRIPTOR_START) {
        fail('Edited scenes and dialogue exceed the shared Cutscene allocation area.', 'relocation-capacity');
      }
      base.set(z64.subarray(row[2], row[2] + 4 + row[3]), extent.entry);
      var next = row.slice();
      next[2] = extent.entry;
      next[1] = relocationKey(extent.entry);
      references.forEach(function(at) {
        writeU32(base, at, next[1]);
        if (next[1] !== row[1]) referenceWrites.push(at);
      });
      if (next[1] !== row[1]) moves.push({ from: row[1], to: next[1] });
      kept.push(next);
      cursor = extent.endExclusive;
    });
    return { base: base, records: kept, dataEnd: cursor, moves: moves, referenceWrites: referenceWrites };
  }

  function buildRelocationDescriptor(records, dataEnd) {
    var descriptor = new Uint8Array(RELOCATION_DESCRIPTOR_SIZE);
    descriptor.fill(RELOCATION_FILL);
    descriptor.set([0x4F, 0x42, 0x43, 0x53], 0); // OBCS
    // Version 1 unless an authored or manifest record needs the kind nibble.
    writeU32(descriptor, 4, records.some(function(record) { return recordKind(record) !== KIND.RETAIL; }) ? 2 : 1);
    writeU32(descriptor, 8, records.length);
    writeU32(descriptor, 12, dataEnd);
    writeU32(descriptor, 16, RESOURCE_BASE_Z64);
    writeU32(descriptor, 20, 0x01F3CA88);
    var cursor = 0x40;
    records.forEach(function(record) {
      if (cursor + 28 > descriptor.length) {
        fail('Cutscene relocation descriptor has no room for every allocation.',
          'relocation-descriptor-capacity');
      }
      record.forEach(function(value, index) { writeU32(descriptor, cursor + index * 4, value); });
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
      fail('This background route is available for preview only. Native selection needs a scene group (classes 4 and 5) or a fresh mode-two environment (classes 1 and 8).',
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
    // A group's image members keep their native traversal ordinals, which can
    // skip non-image members (scene group 0 draws ordinals 0, 3, 4, 5, 6).
    var ordinals = Array.isArray(group.members) && group.members.length === group.archiveAssetIds.length
      ? group.members.map(function(member) { return member.ordinal; }) : group.archiveAssetIds.map(function(_, index) { return index; });
    after.layers.forEach(function(layer, index) {
      var source = layer.source || {};
      if (layer.assetId !== group.archiveAssetIds[index] || layer.visible !== true ||
          layer.depth !== ordinals[index] || layer.capability !== 'native' ||
          source.sourceKind !== (afterProjection.route === 'native-environment' ? 'native-environment' : 'native-scene-group') ||
          source.selector !== group.selector ||
          source.groupResourceKey !== group.groupResourceKey ||
          source.traversalOrdinal !== ordinals[index]) {
        fail('Native background layers cannot be hidden, reordered, or split from their scene group.',
          'background-group-shape', { layer: index });
      }
    });
  }

  function assertFixedSlotDelta(scene, baseline, document) {
    OB64.cutsceneModel.validateSceneDocument(document);
    var identity=Object.assign({},document.identity,{friendlyName:baseline.identity.friendlyName});
    if (!equal(baseline.identity, identity)) {
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
    var reservedDialogueWindowIds = {};
    var authoredDialogueWindowIds = {};
    baseline.native.commands.forEach(function(command) {
      if (command.words[0] === 0xBF) reservedDialogueWindowIds[command.words[1]] = true;
    });
    Object.keys(before).forEach(function(id) {
      var oldRow = before[id];
      var nextRow = after[id];
      var definition = sourceDefinition(scene, oldRow.clip);
      if(oldRow.clip.kind==='dialogue'&&oldRow.clip.payload.nativeDialogueEditable){
        if(!nextRow)fail('Native dialogue lifecycle cannot be deleted independently.','dialogue-lifecycle');
        var check=JSON.parse(JSON.stringify(nextRow.clip));check.payload.rawText=oldRow.clip.payload.rawText;
        if(!equal(check,oldRow.clip)||nextRow.track.actorId!==oldRow.track.actorId)fail('Only native dialogue text can change; keep its lifecycle and selectors.','dialogue-edit');
        return;
      }
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
      } else if (oldRow.clip.kind === 'fade' && oldRow.clip.payload.nativeOpcode === '0x1B') {
        expectedPolicy = 'typed-fade';
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
      if (['pose', 'movement', 'wait', 'camera', 'enter', 'exit', 'effect', 'dialogue',
          'opacity', 'fade'].indexOf(row.clip.kind) === -1 ||
          row.clip.capability !== 'native' ||
          !row.clip.source || typeof row.clip.source.insertBeforeNodeId !== 'string') {
        fail('“' + (row.track.label || row.clip.kind) +
          '” is available in preview but has no native Director command adapter.',
        'preview-only-clip', { clipId: id, kind: row.clip.kind });
      }
      var target = insertionDefinition(scene, row.clip.source.insertBeforeNodeId);
      var dialogueBoundary = row.clip.kind === 'dialogue' &&
        baseline.native.commands.find(function(command) {
          return command.source.nodeId === row.clip.source.insertBeforeNodeId &&
            [0x01, 0xBF, 0x80000001].indexOf(command.words[0]) !== -1 &&
            command.source.editPolicy !== 'immutable-gap' && !command.source.insertionWithheld;
        });
      if (!dialogueBoundary && (!target || target.insertBefore !== true ||
          target.nodeType === 'gap' || target.editPolicy === 'immutable-gap' ||
          target.controlEntryAlias)) {
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
      if (row.clip.kind === 'fade') {
        var fade = row.clip.payload || {};
        var channel = function(value) { return Number.isInteger(value) && value >= 0 && value <= 255; };
        if (!Number.isInteger(row.clip.durationFrames) || row.clip.durationFrames < 1 ||
            row.clip.durationFrames > 0xFFFF || !channel(fade.red) || !channel(fade.green) ||
            !channel(fade.blue) || !channel(fade.targetAlpha) ||
            (fade.startAlpha != null && !channel(fade.startAlpha))) {
          fail('A fade needs a duration from 1 through 65535 updates, an overlay color, and a target opacity.',
            'fade-values', { clipId: id });
        }
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
      if (row.clip.kind === 'dialogue') {
        var dialogue = row.clip.payload || {};
        var recipe = dialogue.nativeWindow != null && OB64.cutsceneCodec.dialogueWindowRecipes[dialogue.nativeWindow];
        var template = recipe ? { words: recipe } : baseline.native.commands.find(function(command) {
          return command.source.nodeId === dialogue.nativeTemplateNodeId &&
            command.words[0] === 0xBF && command.words.length === 14;
        });
        var windowId = dialogue.windowId;
        if (dialogue.nativeDialogueAuthored !== true ||
            dialogue.sourceSystem !== 'serifu-authored-native' ||
            !template || dialogue.presentationArchiveSelector !== template.words[2] ||
            !Number.isInteger(dialogue.presentationEntrySelector) ||
            dialogue.presentationEntrySelector < 0 ||
            !Number.isInteger(dialogue.nativeDelayTicks) ||
            dialogue.nativeDelayTicks < 0 || dialogue.nativeDelayTicks > 0x7FFFFFFF ||
            !Number.isInteger(windowId) || windowId < 0 || windowId > 255 ||
            reservedDialogueWindowIds[windowId] || authoredDialogueWindowIds[windowId]) {
          fail('A new dialogue box needs a unique window ID and a valid native template.',
            'dialogue-insertion', { clipId: id });
        }
        authoredDialogueWindowIds[windowId] = true;
      }
    });
  }

  function assessFixedSlotDelta(scene, baseline, document, source) {
    try {
      assertFixedSlotDelta(scene, baseline, document);
      if (OB64.cutsceneAuthoring && source.rom) {
        OB64.cutsceneAuthoring.dialogueResources(source.rom, [document]);
      }
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
    return OB64.cutsceneProject.editedScenes(state).filter(function(scene) {
      return !scene.authored;
    }).map(function(scene) {
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

  // Authored scenes are discovered asynchronously from the loaded ROM's arena.
  // Export waits for that to finish and refuses a ROM whose records failed it.
  function prepare(sourceRom, candidateRom, options) {
    var studio = sourceRom && sourceRom.cutsceneStudio;
    return Promise.resolve(studio && studio.authoredReady).then(function() {
      return planEventRows(studio, candidateRom, options || {});
    }).then(function(eventWork) {
      return prepareReady(sourceRom, candidateRom, options, eventWork);
    });
  }

  function bindingKey(bindings) {
    return stable((bindings || []).slice().sort(function(left, right) {
      return left.launchSiteId < right.launchSiteId ? -1 : left.launchSiteId > right.launchSiteId ? 1 : 0;
    }));
  }

  // Event bindings. First re-verify the untouched candidate's own event rows
  // (directory ownership, digests, the diff against each retail preimage and
  // the manifest) and require them to be the rows Cutscene Studio opened, so a
  // row changed since then is refused rather than silently replaced. Then
  // validate the binding set and rebuild each bound row from its verified
  // retail preimage. Null when there are no bindings and no relocated rows.
  function planEventRows(state, candidateRom, options) {
    var events = OB64.cutsceneEvents;
    if (!events || !state || !state.catalog || !candidateRom || !(candidateRom.z64 instanceof Uint8Array)) return null;
    if (state.authoredDiscovery && state.authoredDiscovery.status === 'failed') return null;
    var hashOptions = { hashBytes: options.hashBytes };
    var verifyOptions = { hashBytes: options.hashBytes, deletedAuthored: state.deletedAuthored || [] };
    return events.discover(candidateRom.z64, state.catalog, verifyOptions).catch(function(error) {
      fail('The export ROM\'s event rows could not be verified: ' + (error && error.message || String(error)), 'event-row-owner');
    }).then(function(current) {
      if (bindingKey(current) !== bindingKey(state.romEventBindings)) {
        fail('The export ROM\'s event rows differ from the ones Cutscene Studio opened.', 'event-row-owner');
      }
      var bindings = events.normalize(state.catalog, state.eventBindings || []);
      var changedRows = events.changedDirectoryRows(candidateRom.z64);
      if (!bindings.length && !changedRows.length) return null;
      return events.buildRows(candidateRom.z64, bindings, hashOptions).then(function(rows) {
        return { bindings: bindings, rows: rows, changedRows: changedRows };
      });
    });
  }

  function prepareReady(sourceRom, candidateRom, options, eventWork) {
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
    if (state && state.authoredDiscovery && state.authoredDiscovery.status === 'failed') {
      return Promise.reject(new CutsceneExportError('This ROM has authored Cutscene records that could not be verified: ' +
        state.authoredDiscovery.message, 'authored-discovery'));
    }
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
    var original = candidateRom.z64;
    var authoredWork = [], oldManifest = null, manifest = null, manifestWrite = null, eventRestores = [], deletedWork = [];
    var arena = null, compaction = null, base = original, records = null, cursor = RELOCATION_ARENA_START;
    var relocationCount = 0;
    var textResources = [];
    var textWrites = [];
    var relocatedDialogueCount = 0;
    function inArena(offset) {
      return Number.isInteger(offset) && offset >= RELOCATION_ARENA_START && offset < RELOCATION_DESCRIPTOR_START;
    }
    function keyOf(scene) {
      return typeof scene.directorKey === 'string'
        ? parseInt(scene.directorKey.replace(/^0x/i, ''), 16) : scene.directorKey;
    }
    function allocate(kind, owner, size, selectorCount, decodedSize) {
      var extent = alignedExtent(cursor, size);
      if (extent.endExclusive > RELOCATION_DESCRIPTOR_START) {
        fail('Edited scenes and dialogue exceed the shared Cutscene allocation area.', 'relocation-capacity');
      }
      var row = [((kind << 28) | owner) >>> 0, relocationKey(extent.entry), extent.entry, size, selectorCount, decodedSize, 0];
      if (records.some(function(other) { return other[0] === row[0]; })) {
        fail('Two Cutscene arena records claim the same owner.', 'relocation-owner');
      }
      records.push(row);
      if (records.length > 144) fail('The Cutscene allocation directory is full.', 'relocation-descriptor-capacity');
      cursor = extent.endExclusive;
      relocationCount++;
      return { entry: extent.entry, endExclusive: extent.endExclusive, key: row[1] };
    }
    function write(start, patchedBytes, label) {
      return { start: start, endExclusive: start + patchedBytes.length,
        originalBytes: base.slice(start, start + patchedBytes.length), patchedBytes: patchedBytes, label: label };
    }
    try {
      if (OB64.cutsceneAuthoring) {
        // Dialogue text comes from edited retail scenes and from new scenes.
        var authoredDocuments = ((state.catalog && state.catalog.scenes) || []).filter(function(scene) {
          return !!scene.authored && !!state.histories[scene.storageId];
        }).map(function(scene) { return state.histories[scene.storageId].present; });
        textResources = OB64.cutsceneAuthoring.dialogueResources(original,
          entries.map(function(entry) { return entry.document; }).concat(authoredDocuments));
      }
      // A resource already in the arena is rebuilt there on every export, so the
      // compacted layout never keeps an in-place write to an old arena position.
      entries.forEach(function(entry) {
        entry.ownedRelocation = inArena(entry.scene.source.z64PrefixStart);
        entry.needsAllocation = entry.ownedRelocation ||
          entry.assessment.allocationBytes > entry.scene.source.storedPayloadLength;
      });
      textResources.forEach(function(resource) {
        resource.ownedRelocation = inArena(resource.archive.prefix);
        resource.needsAllocation = resource.ownedRelocation || resource.bytes.length > resource.archive.capacity;
      });
      // Every authored scene open in the editor is rebuilt from its current
      // document (or its initial stream when it was never opened) on each export.
      authoredWork = ((state.catalog && state.catalog.scenes) || []).filter(function(scene) {
        return !!scene.authored;
      }).map(function(scene) {
        var history = state.histories[scene.storageId];
        var source = state.sourceByAssetId[scene.assetId];
        var decoded = history && source
          ? OB64.cutsceneCodec.compileSceneDocument(scene, source, history.present).decodedBytes
          : scene.authored.initialDecodedBytes;
        OB64.cutsceneAuthored.validateAuthoredStream(decoded);
        var encoded = (options.encoder || OB64.cutsceneCodec.encodeCustomLzOptimal)(decoded);
        return { scene: scene, row: scene.authored.selectorRow, decoded: decoded, encoded: encoded };
      }).sort(function(left, right) { return left.row - right.row; });
      authoredWork.forEach(function(work, index) {
        if (index && authoredWork[index - 1].row === work.row) {
          fail('Two authored scenes claim selector row ' + work.row + '.', 'authored-row');
        }
      });
      // Every wiped original scene is named in the manifest, so a ROM-only
      // reopen treats it as explicitly wiped.
      var wipedList = ((state.catalog && state.catalog.scenes) || []).filter(function(scene) {
        return !scene.authored && !!scene.wiped;
      }).map(function(scene) {
        // The Actor recipe is recorded because an empty wiped stream no
        // longer contains the cast it was derived from.
        var entry = { sceneId: scene.sceneId, exitKind: scene.wiped.exitKind };
        var recipe = scene.wiped.actorRecipe;
        if (recipe) {
          entry.actorRecipe = {};
          ['bank', 'animationKey', 'nativeFacing', 'x', 'y', 'z', 'renderMode', 'variantSelector',
            'transformChannel', 'uniformScalePercent', 'provenance'].forEach(function(key) {
            if (recipe[key] !== undefined) entry.actorRecipe[key] = recipe[key];
          });
        }
        return entry;
      }).sort(function(left, right) { return left.sceneId < right.sceneId ? -1 : left.sceneId > right.sceneId ? 1 : 0; });
      var needsArena = entries.some(function(entry) { return entry.needsAllocation; }) ||
        textResources.some(function(resource) { return resource.needsAllocation; }) ||
        authoredWork.length > 0 || !!eventWork || wipedList.length > 0 || (state.deletedAuthored || []).length > 0;
      var superseded = new Map();
      if (needsArena) {
        arena = readRelocationArena(original);
        // Authored records of scenes being rebuilt are replaced; any other
        // authored record is kept and must stay listed in the manifest.
        arena.records.forEach(function(row) {
          var kind = recordKind(row);
          if (kind === KIND.MANIFEST) {
            oldManifest = readManifest(original, row);
            superseded.set(row[1], row);
          } else if (kind === KIND.AUTHORED && (authoredWork.some(function(work) { return work.row === recordOwner(row); }) ||
              (state.deletedAuthored || []).some(function(deleted) { return deleted.selectorRow === recordOwner(row); }))) {
            superseded.set(row[1], row);
          }
        });
        if (eventWork) {
          arena.records.forEach(function(row) {
            if (recordKind(row) === KIND.EVENT_ROW) superseded.set(row[1], row);
          });
          eventWork.changedRows.forEach(function(change) {
            var owned = arena.records.some(function(row) {
              return recordKind(row) === KIND.EVENT_ROW && recordOwner(row) === change.row && row[1] === change.current;
            });
            if (!owned) fail('Event row ' + change.row + ' was changed outside Cutscene Studio.', 'event-row-owner');
          });
        }
        entries.forEach(function(entry) {
          if (!entry.ownedRelocation) return;
          var key = keyOf(entry.scene), row = arena.records.find(function(record) { return record[1] === key; });
          if (!row || recordKind(row) !== KIND.RETAIL) fail('A relocated Cutscene has no arena owner record.', 'relocation-owner');
          superseded.set(key, row);
        });
        textResources.forEach(function(resource) {
          if (!resource.ownedRelocation) return;
          var row = arena.records.find(function(record) { return record[1] === resource.archive.key; });
          if (!row || recordKind(row) !== KIND.RETAIL) fail('A relocated dialogue archive has no arena owner record.', 'relocation-owner');
          superseded.set(resource.archive.key, row);
        });
        compaction = compactArena(original, arena, new Set(superseded.keys()));
        base = compaction.base;
        records = compaction.records;
        cursor = compaction.dataEnd;
        entries.forEach(function(entry) {
          if (!entry.needsAllocation) return;
          var key = keyOf(entry.scene), owner = superseded.has(key) ? recordOwner(superseded.get(key)) : key;
          entry.allocation = allocate(KIND.RETAIL, owner, entry.assessment.allocationBytes,
            entry.scene.source.directorSelectorRows.length, 0);
        });
        deletedWork = (state.deletedAuthored || []).map(function(deleted) {
          var rowWord = DIRECTOR_TABLE_Z64 + deleted.selectorRow * 4;
          var owned = arena.records.find(function(row) {
            return recordKind(row) === KIND.AUTHORED && recordOwner(row) === deleted.selectorRow && row[1] === readU32(original, rowWord);
          });
          if (!owned) fail('A deleted scene is no longer the one this ROM holds at selector row ' + deleted.selectorRow + '.', 'authored-row');
          return { row: deleted.selectorRow, authoredId: deleted.authoredId,
            writes: [write(rowWord, new Uint8Array(4), 'deleted scene selector row ' + deleted.selectorRow)] };
        });
        authoredWork.forEach(function(work) {
          var rowWord = DIRECTOR_TABLE_Z64 + work.row * 4, current = readU32(base, rowWord);
          var owned = arena.records.some(function(row) {
            return recordKind(row) === KIND.AUTHORED && recordOwner(row) === work.row && row[1] === current;
          });
          if (current !== 0 && !owned) {
            fail('Selector row ' + work.row + ' already points at another Director resource.', 'authored-row');
          }
          work.allocation = allocate(KIND.AUTHORED, work.row, work.encoded.length, 1, work.decoded.length);
          var envelope = new Uint8Array(work.allocation.endExclusive - work.allocation.entry);
          envelope.fill(RELOCATION_FILL);
          writeU32(envelope, 0, work.encoded.length);
          envelope.set(work.encoded, 4);
          var keyBytes = new Uint8Array(4);
          writeU32(keyBytes, 0, work.allocation.key);
          work.writes = [write(work.allocation.entry, envelope, 'authored Director stream'),
            write(rowWord, keyBytes, 'authored scene selector row ' + work.row)];
        });
        if (eventWork) {
          eventWork.rows.forEach(function(work) {
            work.encoded = (options.encoder || OB64.cutsceneCodec.encodeCustomLzOptimal)(work.decoded);
            var check = OB64.cutsceneCodec.decodeCustomLz(work.encoded, { requireExact: true }).bytes;
            if (!equalBytes(check, work.decoded)) fail('Event row ' + work.row + ' does not re-encode exactly.', 'event-row-encode');
            work.allocation = allocate(KIND.EVENT_ROW, work.row, work.encoded.length, 1, work.decoded.length);
            var envelope = new Uint8Array(work.allocation.endExclusive - work.allocation.entry);
            envelope.fill(RELOCATION_FILL);
            writeU32(envelope, 0, work.encoded.length);
            envelope.set(work.encoded, 4);
            var keyBytes = new Uint8Array(4);
            writeU32(keyBytes, 0, work.allocation.key);
            work.writes = [write(work.allocation.entry, envelope, 'relocated event row ' + work.row),
              write(eventWordZ64(work.row), keyBytes, 'event directory row ' + work.row)];
          });
          eventRestores = eventWork.changedRows.filter(function(change) {
            return !eventWork.rows.some(function(work) { return work.row === change.row; });
          }).map(function(change) {
            var keyBytes = new Uint8Array(4);
            writeU32(keyBytes, 0, change.retail);
            return { row: change.row, retailKey: change.retail,
              writes: [write(eventWordZ64(change.row), keyBytes, 'event directory row ' + change.row + ' restored to retail')] };
          });
        }
        // One manifest names every authored record in the arena (rebuilt scenes
        // from the editor, and kept records carried over from the old manifest)
        // and every event binding. It is written only when it names something.
        var eventBindings = eventWork && eventWork.bindings.length
          ? OB64.cutsceneEvents.publicBindings(eventWork.bindings) : null;
        if (authoredWork.length || oldManifest || eventBindings || wipedList.length || deletedWork.length) {
          var listed = authoredWork.map(function(work) {
            var authored = work.scene.authored;
            return { authoredId: authored.authoredId, name: authored.name, selectorRow: work.row,
              sceneClass: authored.sceneClass, templateSceneId: authored.templateSceneId };
          });
          records.forEach(function(row) {
            if (recordKind(row) !== KIND.AUTHORED) return;
            if (listed.some(function(entry) { return entry.selectorRow === recordOwner(row); })) return;
            var carried = oldManifest && oldManifest.authoredScenes.find(function(entry) {
              return entry.selectorRow === recordOwner(row);
            });
            if (!carried) fail('An authored Cutscene record has no manifest entry.', 'authored-owner');
            listed.push(carried);
          });
          listed.sort(function(left, right) { return left.selectorRow - right.selectorRow; });
          manifest = { format: 'ob64-cutscene-arena-manifest', version: 1, authoredScenes: listed };
          if (eventBindings) manifest.eventBindings = eventBindings;
          if (wipedList.length) manifest.wipedScenes = wipedList;
        }
        if (manifest && !manifest.authoredScenes.length && !manifest.eventBindings && !manifest.wipedScenes) manifest = null;
        if (manifest) {
          var manifestBytes = utf8(JSON.stringify(manifest));
          var manifestAllocation = allocate(KIND.MANIFEST, 0, manifestBytes.length, 0, manifestBytes.length);
          var manifestEnvelope = new Uint8Array(manifestAllocation.endExclusive - manifestAllocation.entry);
          manifestEnvelope.fill(RELOCATION_FILL);
          writeU32(manifestEnvelope, 0, manifestBytes.length);
          manifestEnvelope.set(manifestBytes, 4);
          manifestWrite = write(manifestAllocation.entry, manifestEnvelope, 'Cutscene arena manifest');
        }
      }
      textResources.forEach(function(resource) {
        var archive = resource.archive;
        var label = 'Shared dialogue archive ' + resource.selector;
        if (!resource.needsAllocation) {
          textWrites.push(write(archive.start, resource.bytes, label));
        } else {
          var owner = superseded.has(archive.key) ? recordOwner(superseded.get(archive.key)) : archive.key;
          var allocation = allocate(KIND.RETAIL, owner, resource.bytes.length, archive.selectorWords.length, resource.data.length);
          var envelope = new Uint8Array(allocation.endExclusive - allocation.entry);
          writeU32(envelope, 0, resource.bytes.length);
          envelope.set(resource.bytes, 4);
          textWrites.push(write(allocation.entry, envelope, label + ' relocated resource'));
          archive.selectorWords.forEach(function(at) {
            var keyBytes = new Uint8Array(4);
            writeU32(keyBytes, 0, allocation.key);
            textWrites.push(write(at, keyBytes, label + ' selector'));
          });
          relocatedDialogueCount++;
        }
      });
    } catch (error) {
      return Promise.reject(error);
    }
    var planned = [];
    var textAttached = false;
    var chain = Promise.resolve();
    entries.forEach(function(entry) {
      chain = chain.then(function() {
        var knownSource = state.sourceByAssetId[entry.scene.assetId];
        var planOptions = Object.assign({}, options, {
          allowZeroPadding: true,
          expectedDecodedSha256: knownSource && knownSource.decodedSha256 ||
            entry.scene.source.decodedSha256
        });
        if (entry.ownedRelocation) planOptions.relocateOwned = true;
        // Compaction plans on a detached base whose old arena bytes are cleared.
        // Verify each source strictly against the untouched original candidate
        // first (selector ownership, size header, decoded length and expected
        // hash), then give the planner only that just-verified source. A cached
        // UI source is never trusted on its own.
        var wipe = entry.scene.wiped && entry.scene.wiped.pending ? entry.scene.wiped : null;
        var verified = compaction || wipe
          ? OB64.cutsceneCodec.loadSceneSource(original, wipe ? entry.scene.wipeBackup.romScene : entry.scene, {
            allowZeroPadding: true,
            expectedDecodedSha256: wipe ? wipe.romDecodedSha256 : planOptions.expectedDecodedSha256,
            hashBytes: options.hashBytes })
          : Promise.resolve(null);
        return verified.then(function(source) {
          if (wipe) {
            // The ROM matches the stream the wipe started from; plan from the
            // wiped source and write it even without further edits.
            planOptions.preloadedSource = knownSource;
            planOptions.forceWrite = true;
          } else if (source) planOptions.preloadedSource = source;
          return entry.allocation
            ? OB64.cutsceneCodec.planRelocatedExport(
              base, entry.scene, entry.document, entry.allocation, planOptions)
            : OB64.cutsceneCodec.planFixedCapacityExport(
              base, entry.scene, entry.document, planOptions);
        }).then(function(result) {
          if (!entry.allocation) result.placement = 'fixed';
          result.writes = resultWrites(result);
          if (textWrites.length && !textAttached) {
            textAttached = true;
            result.noOp = false;
            result.changes.push({ operation: 'edit-dialogue', archives: textResources.length });
            result.writes = result.writes.concat(textWrites);
          }
          if (entry.allocation) {
            var record = records.find(function(row) { return row[1] === result.resourceKey; });
            record[5] = result.intendedDecodedBytes.length;
            record[6] = parseInt(result.intendedDecodedSha256.slice(0, 8), 16);
          }
          // The final candidate is assembled once by apply(); retain only each
          // resource's writes, rather than one full ROM copy per edited scene.
          delete result.candidateZ64;
          planned.push({
            scene: entry.scene,
            baseline: entry.baseline,
            document: entry.document,
            result: result
          });
        });
      });
    });
    authoredWork.forEach(function(work) {
      chain = chain.then(function() {
        return OB64.cutsceneCodec.hashBytes(work.decoded, options.hashBytes).then(function(hash) {
          var record = records.find(function(row) { return row[1] === work.allocation.key; });
          record[6] = parseInt(hash.slice(0, 8), 16);
          planned.push({
            scene: work.scene,
            baseline: null,
            document: state.histories[work.scene.storageId] ? state.histories[work.scene.storageId].present : null,
            result: {
              sceneId: work.scene.sceneId,
              assetId: work.scene.assetId,
              placement: 'relocated',
              authored: true,
              noOp: false,
              resourceKey: work.allocation.key,
              relocationEntry: work.allocation.entry,
              selectorWordZ64: [DIRECTOR_TABLE_Z64 + work.row * 4],
              encodedBytes: work.encoded.length,
              capacityBytes: work.allocation.endExclusive - work.allocation.entry - 4,
              intendedDecodedBytes: work.decoded,
              intendedDecodedSha256: hash,
              changes: [{ operation: 'authored-scene', selectorRow: work.row }],
              writes: work.writes
            }
          });
        });
      });
    });
    (eventWork ? eventWork.rows : []).forEach(function(work) {
      chain = chain.then(function() {
        return OB64.cutsceneCodec.hashBytes(work.decoded, options.hashBytes).then(function(hash) {
          var record = records.find(function(row) { return row[1] === work.allocation.key; });
          record[6] = parseInt(hash.slice(0, 8), 16);
          planned.push({ scene: null, baseline: null, document: null, result: {
            sceneId: 'event-row:' + work.row, placement: 'event-row', noOp: false,
            eventRow: work.row, resourceKey: work.allocation.key, relocationEntry: work.allocation.entry,
            directoryWordZ64: eventWordZ64(work.row), encodedBytes: work.encoded.length,
            capacityBytes: work.allocation.endExclusive - work.allocation.entry - 4,
            intendedDecodedBytes: work.decoded, intendedDecodedSha256: hash,
            changes: work.bindings.map(function(binding) {
              return { operation: 'bind-event-site', launchSiteId: binding.launchSiteId, selectorRow: binding.selector };
            }),
            writes: work.writes } });
        });
      });
    });
    return chain.then(function() {
      // Dialogue written only by new scenes has no retail entry to carry it.
      if (textWrites.length && !textAttached) {
        textAttached = true;
        planned.push({ scene: null, baseline: null, document: null, result: {
          sceneId: 'cutscene-dialogue', placement: 'dialogue', noOp: false,
          changes: [{ operation: 'edit-dialogue', archives: textResources.length }], writes: textWrites } });
      }
      deletedWork.forEach(function(deleted) {
        planned.push({ scene: null, baseline: null, document: null, result: {
          sceneId: 'scene:authored:' + deleted.authoredId, placement: 'authored-delete', noOp: false,
          selectorRow: deleted.row, changes: [{ operation: 'delete-authored-scene', selectorRow: deleted.row }],
          writes: deleted.writes } });
      });
      eventRestores.forEach(function(restore) {
        planned.push({ scene: null, baseline: null, document: null, result: {
          sceneId: 'event-row:' + restore.row, placement: 'event-row-restore', noOp: false,
          eventRow: restore.row, retailKey: restore.retailKey, directoryWordZ64: eventWordZ64(restore.row),
          changes: [{ operation: 'restore-event-row', row: restore.row }], writes: restore.writes } });
      });
      if (manifestWrite) {
        planned.push({ scene: null, baseline: null, document: null, result: {
          sceneId: 'cutscene-arena-manifest', placement: 'manifest', noOp: false,
          changes: [{ operation: 'arena-manifest', authoredScenes: manifest.authoredScenes.length }],
          writes: [manifestWrite] } });
      }
      var changed = planned.filter(function(entry) { return !entry.result.noOp; });
      var relocated = changed.filter(function(entry) {
        return entry.result.placement === 'relocated';
      });
      var descriptorWrite = null, referenceWrites = [];
      if (compaction) {
        // Fold every arena-internal write into one arena image, so the packed
        // layout, the new allocations and the descriptor publish together.
        var arenaImage = base.slice(RELOCATION_ARENA_START, RELOCATION_ARENA_END);
        planned.forEach(function(entry) {
          entry.result.writes = entry.result.writes.filter(function(row) {
            if (row.start >= RELOCATION_ARENA_START && row.endExclusive <= RELOCATION_DESCRIPTOR_START) {
              arenaImage.set(row.patchedBytes, row.start - RELOCATION_ARENA_START);
              return false;
            }
            if (row.start < RELOCATION_ARENA_END && row.endExclusive > RELOCATION_ARENA_START) {
              fail('A Cutscene write crosses the arena boundary.', 'physical-alias');
            }
            return true;
          });
        });
        arenaImage.set(buildRelocationDescriptor(records, cursor), RELOCATION_DESCRIPTOR_START - RELOCATION_ARENA_START);
        descriptorWrite = {
          start: RELOCATION_ARENA_START,
          endExclusive: RELOCATION_ARENA_END,
          originalBytes: original.slice(RELOCATION_ARENA_START, RELOCATION_ARENA_END),
          patchedBytes: arenaImage,
          label: 'Cutscene relocation arena and ownership descriptor'
        };
        referenceWrites = compaction.referenceWrites.map(function(at) {
          return { start: at, endExclusive: at + 4, originalBytes: original.slice(at, at + 4),
            patchedBytes: base.slice(at, at + 4), label: 'Cutscene arena reference moved by compaction' };
        });
      }
      planned.forEach(function(entry) {
        entry.result.changedRanges = entry.result.writes.map(function(write) {
          return { start: write.start, endExclusive: write.endExclusive };
        });
      });
      var claims = [];
      changed.forEach(function(entry) {
        entry.result.writes.forEach(function(write) {
          claims.push({ write: write, assetId: entry.scene ? entry.scene.assetId : entry.result.sceneId });
        });
      });
      if (descriptorWrite) claims.push({ write: descriptorWrite, assetId: 'descriptor' });
      referenceWrites.forEach(function(write) { claims.push({ write: write, assetId: 'arena-reference' }); });
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
        dialogueResources: textResources,
        relocatedDialogueCount: relocatedDialogueCount,
        relocationDescriptorWrite: descriptorWrite,
        arenaReferenceWrites: referenceWrites,
        arenaManifest: manifest,
        eventBindings: eventWork ? OB64.cutsceneEvents.publicBindings(eventWork.bindings) : [],
        relocationArena: compaction ? {
          start: RELOCATION_ARENA_START,
          dataEnd: cursor,
          movedRecords: compaction.moves.length,
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
          label: (entry.scene ? OB64.cutsceneCatalog.displayName(entry.scene) : 'Cutscene arena') + ' · ' + write.label
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
    (plan.arenaReferenceWrites || []).forEach(function(write) {
      regions.push({ kind: 'rom', start: write.start, size: write.endExclusive - write.start, label: write.label });
    });
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
    (plan.arenaReferenceWrites || []).forEach(function(write) {
      writes.push({ write: write, sceneId: 'arena-reference' });
    });
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
      entry.result.writes.forEach(function(write){if(!equalBytes(candidateRom.z64.slice(write.start,write.endExclusive),write.patchedBytes))fail('Finished ROM differs from a planned Cutscene or dialogue write.','semantic-readback');});
      if (entry.result.placement === 'manifest' || entry.result.placement === 'dialogue') return;
      if (entry.result.placement === 'authored-delete') {
        if (readU32(candidateRom.z64, DIRECTOR_TABLE_Z64 + entry.result.selectorRow * 4) !== 0) {
          fail('A deleted scene still owns selector row ' + entry.result.selectorRow + '.', 'semantic-readback');
        }
        return;
      }
      if (entry.result.placement === 'event-row-restore') {
        if (readU32(candidateRom.z64, entry.result.directoryWordZ64) !== entry.result.retailKey) {
          fail('Event row ' + entry.result.eventRow + ' did not return to its retail program.', 'semantic-readback');
        }
        return;
      }
      if (entry.result.placement === 'event-row') {
        var eventResult = entry.result;
        if (readU32(candidateRom.z64, eventResult.directoryWordZ64) !== eventResult.resourceKey ||
            readU32(candidateRom.z64, eventResult.relocationEntry) !== eventResult.encodedBytes) {
          fail('Event row ' + eventResult.eventRow + ' does not point at its relocated program.', 'semantic-readback');
        }
        var eventDecoded = OB64.cutsceneCodec.decodeCustomLz(candidateRom.z64.slice(eventResult.relocationEntry + 4,
          eventResult.relocationEntry + 4 + eventResult.encodedBytes), { requireExact: true });
        if (!equalBytes(eventDecoded.bytes, eventResult.intendedDecodedBytes)) {
          fail('Finished ROM event row ' + eventResult.eventRow + ' differs from the planned program.', 'semantic-readback');
        }
        return;
      }
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
    (plan.dialogueResources || []).forEach(function(resource) {
      var decoded = OB64.cutsceneAuthoring.readDialogue(candidateRom.z64, resource.selector);
      if (!equalBytes(decoded.data, resource.data)) fail('Exported dialogue differs from the edited text.', 'semantic-readback');
    });
    if (plan.relocationDescriptorWrite) {
      [plan.relocationDescriptorWrite].concat(plan.arenaReferenceWrites || []).forEach(function(write) {
        if (!equalBytes(candidateRom.z64.slice(write.start, write.endExclusive), write.patchedBytes)) {
          fail('Finished ROM differs from the planned Cutscene arena.', 'semantic-readback');
        }
      });
      var finalArena = readRelocationArena(candidateRom.z64);
      var manifestRow = finalArena.records.find(function(row) { return recordKind(row) === KIND.MANIFEST; });
      if (!!manifestRow !== !!plan.arenaManifest ||
          (manifestRow && stable(readManifest(candidateRom.z64, manifestRow)) !== stable(plan.arenaManifest))) {
        fail('The exported Cutscene arena manifest differs from the plan.', 'semantic-readback');
      }
      // Every owned record must still be reachable from exactly its references.
      finalArena.records.forEach(function(row) {
        var kind = recordKind(row);
        if (kind === KIND.MANIFEST) return;
        var references = referenceWords(candidateRom.z64, row[1]);
        if ((kind === KIND.RETAIL && references.length !== row[4]) ||
            ((kind === KIND.AUTHORED || kind === KIND.EVENT_ROW) && references.length !== 1)) {
          fail('An exported Cutscene arena record lost a reference.', 'semantic-readback');
        }
      });
    }
    return {
      summary: plan.changedEntries.length + ' Cutscene Director payload' +
        (plan.changedEntries.length === 1 ? '' : 's') + ' reparsed exactly; ' +
        (plan.relocatedSceneCount || 0) + ' relocated.',
      details: {
        sceneCount: plan.changedEntries.length,
        relocatedSceneCount: plan.relocatedSceneCount || 0,
        scenes: plan.changedEntries.filter(function(entry) { return !!entry.scene; }).map(function(entry) {
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
    (plan.arenaReferenceWrites || []).forEach(function(write) {
      rom.z64.set(write.originalBytes, write.start);
    });
  }

  OB64.cutsceneExport = {
    CutsceneExportError: CutsceneExportError,
    arena: {
      KIND: KIND,
      start: RELOCATION_ARENA_START,
      descriptorStart: RELOCATION_DESCRIPTOR_START,
      endExclusive: RELOCATION_ARENA_END,
      directorTableZ64: DIRECTOR_TABLE_Z64,
      directorTableRows: DIRECTOR_TABLE_ROWS,
      eventDirectoryZ64: EVENT_DIRECTORY_Z64,
      eventRows: EVENT_ROWS,
      read: readRelocationArena,
      readManifest: readManifest,
      referenceWords: referenceWords,
      compact: compactArena,
      descriptor: buildRelocationDescriptor
    },
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
