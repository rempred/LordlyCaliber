// Lordly Caliber - normalized Cutscene Studio scene document.
//
// This module contains product concepts only. Native offsets, command words,
// compression, and ROM writes remain behind codec and export adapters.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var FORMAT = 'ob64-cutscene-scene';
  var SCHEMA_VERSION = 1;
  var PREVIEW_FPS = 30;
  var DEFAULT_HISTORY_LIMIT = 100;

  var CAPABILITIES = Object.freeze({
    NATIVE: 'native',
    CONVERTED: 'converted',
    PREVIEW_ONLY: 'preview-only',
    NEEDS_RESEARCH: 'needs-research'
  });

  var CAPABILITY_LABELS = Object.freeze({
    native: 'Native',
    converted: 'Converted',
    'preview-only': 'Preview only',
    'needs-research': 'Needs research'
  });

  var TRACK_TYPES = Object.freeze([
    'actor', 'pose', 'movement', 'dialogue', 'audio', 'camera', 'effect', 'flow'
  ]);

  var TRACK_CLIP_KINDS = Object.freeze({
    actor: ['enter', 'exit', 'visibility', 'opacity'],
    pose: ['pose'],
    movement: ['movement'],
    dialogue: ['dialogue'],
    audio: ['audio'],
    camera: ['camera'],
    effect: ['effect'],
    flow: ['wait', 'branch', 'marker', 'end', 'effect-remove', 'control-overlap']
  });

  function SceneDocumentError(message, path) {
    this.name = 'SceneDocumentError';
    this.message = path ? path + ': ' + message : message;
    this.path = path || '';
  }
  SceneDocumentError.prototype = Object.create(Error.prototype);
  SceneDocumentError.prototype.constructor = SceneDocumentError;

  function fail(message, path) {
    throw new SceneDocumentError(message, path);
  }

  function isPlainObject(value) {
    if (!value || Object.prototype.toString.call(value) !== '[object Object]') return false;
    var prototype = Object.getPrototypeOf(value);
    return prototype === Object.prototype || prototype === null;
  }

  function assertPlainObject(value, path) {
    if (!isPlainObject(value)) fail('must be an object', path);
  }

  function assertOnlyFields(value, allowed, path) {
    Object.keys(value).forEach(function(key) {
      if (allowed.indexOf(key) === -1) fail('unsupported field "' + key + '"', path);
    });
  }

  function assertString(value, path, allowEmpty) {
    if (typeof value !== 'string' || (!allowEmpty && !value.length)) {
      fail(allowEmpty ? 'must be a string' : 'must be a non-empty string', path);
    }
  }

  function assertNullableString(value, path) {
    if (value !== null) assertString(value, path, true);
  }

  function assertInteger(value, path, minimum) {
    if (!Number.isInteger(value)) fail('must be an integer', path);
    if (minimum != null && value < minimum) fail('must be at least ' + minimum, path);
  }

  function assertFiniteNumber(value, path) {
    if (typeof value !== 'number' || !Number.isFinite(value)) fail('must be a finite number', path);
  }

  function assertId(value, path) {
    assertString(value, path, false);
    if (!/^[A-Za-z0-9][A-Za-z0-9._:/-]*$/.test(value)) {
      fail('must use letters, numbers, dot, underscore, colon, slash, or dash', path);
    }
  }

  function assertCapability(value, path) {
    if (!Object.prototype.hasOwnProperty.call(CAPABILITY_LABELS, value)) {
      fail('must be native, converted, preview-only, or needs-research', path);
    }
  }

  function cloneJson(value, path) {
    path = path || 'value';
    if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
    if (typeof value === 'number') {
      if (!Number.isFinite(value)) fail('contains a non-finite number', path);
      return value;
    }
    if (Array.isArray(value)) {
      return value.map(function(entry, index) {
        return cloneJson(entry, path + '[' + index + ']');
      });
    }
    if (isPlainObject(value)) {
      var output = {};
      Object.keys(value).forEach(function(key) {
        if (typeof value[key] === 'undefined') fail('contains undefined', path + '.' + key);
        output[key] = cloneJson(value[key], path + '.' + key);
      });
      return output;
    }
    fail('must contain JSON-compatible values only', path);
  }

  function stableCopy(value) {
    if (Array.isArray(value)) return value.map(stableCopy);
    if (isPlainObject(value)) {
      var output = {};
      Object.keys(value).sort().forEach(function(key) {
        output[key] = stableCopy(value[key]);
      });
      return output;
    }
    return value;
  }

  function stableStringify(value, space) {
    return JSON.stringify(stableCopy(value), null, space == null ? 2 : space);
  }

  function defaultIdentity(seed) {
    seed = seed || {};
    var identity = {
      sceneId: seed.sceneId || 'scene:untitled',
      technicalName: seed.technicalName || 'Untitled scene',
      friendlyName: seed.friendlyName == null ? null : seed.friendlyName,
      engine: seed.engine || 'director',
      sourceRevision: seed.sourceRevision || 'us-rev0',
      directorKey: seed.directorKey == null ? null : seed.directorKey,
      aliases: cloneJson(seed.aliases || [], 'identity.aliases'),
      triggerStatus: seed.triggerStatus || 'unresolved'
    };
    if (Array.isArray(seed.captures) && seed.captures.length) {
      identity.captures = cloneJson(seed.captures, 'identity.captures');
    }
    return identity;
  }

  function defaultBackground(seed) {
    seed = seed || {};
    return {
      assetId: seed.assetId == null ? null : seed.assetId,
      capability: seed.capability || CAPABILITIES.NEEDS_RESEARCH,
      layers: cloneJson(seed.layers || [], 'background.layers'),
      projection: cloneJson(seed.projection || { mode: 'unresolved' }, 'background.projection')
    };
  }

  function defaultNative(seed) {
    seed = seed || {};
    return {
      sourceAssetId: seed.sourceAssetId == null ? null : seed.sourceAssetId,
      commands: cloneJson(seed.commands || [], 'native.commands'),
      gaps: cloneJson(seed.gaps || [], 'native.gaps')
    };
  }

  function defaultExportRequirements(seed) {
    seed = seed || {};
    return {
      capability: seed.capability || CAPABILITIES.PREVIEW_ONLY,
      reasons: cloneJson(seed.reasons || [], 'exportRequirements.reasons'),
      allocationBytes: seed.allocationBytes == null ? 0 : seed.allocationBytes,
      features: cloneJson(seed.features || [], 'exportRequirements.features')
    };
  }

  function createSceneDocument(seed) {
    seed = seed || {};
    assertPlainObject(seed, 'scene');
    assertOnlyFields(seed, [
      'format', 'schemaVersion', 'identity', 'background', 'actors', 'tracks',
      'branches', 'native', 'exportRequirements'
    ], 'scene');

    var document = {
      format: FORMAT,
      schemaVersion: SCHEMA_VERSION,
      identity: defaultIdentity(seed.identity),
      background: defaultBackground(seed.background),
      actors: cloneJson(seed.actors || [], 'actors'),
      tracks: cloneJson(seed.tracks || [], 'tracks'),
      branches: cloneJson(seed.branches || [
        { id: 'default', label: 'Default', condition: null }
      ], 'branches'),
      native: defaultNative(seed.native),
      exportRequirements: defaultExportRequirements(seed.exportRequirements)
    };

    if (seed.format != null && seed.format !== FORMAT) {
      fail('unsupported format "' + seed.format + '"', 'scene.format');
    }
    if (seed.schemaVersion != null && seed.schemaVersion !== SCHEMA_VERSION) {
      fail('unsupported schema version ' + seed.schemaVersion, 'scene.schemaVersion');
    }
    validateSceneDocument(document);
    return document;
  }

  function validateIdentity(identity) {
    assertPlainObject(identity, 'scene.identity');
    assertOnlyFields(identity, [
      'sceneId', 'technicalName', 'friendlyName', 'engine', 'sourceRevision',
      'directorKey', 'aliases', 'triggerStatus', 'captures'
    ], 'scene.identity');
    assertId(identity.sceneId, 'scene.identity.sceneId');
    assertString(identity.technicalName, 'scene.identity.technicalName', false);
    assertNullableString(identity.friendlyName, 'scene.identity.friendlyName');
    assertId(identity.engine, 'scene.identity.engine');
    assertId(identity.sourceRevision, 'scene.identity.sourceRevision');
    assertNullableString(identity.directorKey, 'scene.identity.directorKey');
    if (!Array.isArray(identity.aliases)) fail('must be an array', 'scene.identity.aliases');
    identity.aliases.forEach(function(alias, index) {
      assertString(alias, 'scene.identity.aliases[' + index + ']', false);
    });
    assertString(identity.triggerStatus, 'scene.identity.triggerStatus', false);
    if (identity.captures != null) {
      if (!Array.isArray(identity.captures)) fail('must be an array', 'scene.identity.captures');
      cloneJson(identity.captures, 'scene.identity.captures');
    }
  }

  function validateBackground(background) {
    assertPlainObject(background, 'scene.background');
    assertOnlyFields(background, ['assetId', 'capability', 'layers', 'projection'], 'scene.background');
    assertNullableString(background.assetId, 'scene.background.assetId');
    assertCapability(background.capability, 'scene.background.capability');
    if (!Array.isArray(background.layers)) fail('must be an array', 'scene.background.layers');
    var layerIds = {};
    background.layers.forEach(function(layer, index) {
      var path = 'scene.background.layers[' + index + ']';
      assertPlainObject(layer, path);
      assertOnlyFields(layer, [
        'id', 'assetId', 'label', 'visible', 'depth', 'role', 'capability', 'source'
      ], path);
      assertId(layer.id, path + '.id');
      if (layerIds[layer.id]) fail('duplicates layer id "' + layer.id + '"', path + '.id');
      layerIds[layer.id] = true;
      assertString(layer.assetId, path + '.assetId', false);
      assertString(layer.label, path + '.label', true);
      if (typeof layer.visible !== 'boolean') fail('must be a boolean', path + '.visible');
      assertFiniteNumber(layer.depth, path + '.depth');
      if (layer.role != null) assertString(layer.role, path + '.role', false);
      assertCapability(layer.capability, path + '.capability');
      cloneJson(layer.source || {}, path + '.source');
    });
    assertPlainObject(background.projection, 'scene.background.projection');
    assertString(background.projection.mode, 'scene.background.projection.mode', false);
    cloneJson(background.projection, 'scene.background.projection');
  }

  function validateActors(actors) {
    if (!Array.isArray(actors)) fail('must be an array', 'scene.actors');
    var ids = {};
    var slots = {};
    actors.forEach(function(actor, index) {
      var path = 'scene.actors[' + index + ']';
      assertPlainObject(actor, path);
      assertOnlyFields(actor, [
        'id', 'label', 'slot', 'artSourceId', 'capability', 'initial', 'source'
      ], path);
      assertId(actor.id, path + '.id');
      if (ids[actor.id]) fail('duplicates actor id "' + actor.id + '"', path + '.id');
      ids[actor.id] = true;
      assertString(actor.label, path + '.label', true);
      if (actor.slot !== null) {
        assertInteger(actor.slot, path + '.slot', 0);
        if (slots[actor.slot]) fail('duplicates native slot ' + actor.slot, path + '.slot');
        slots[actor.slot] = true;
      }
      assertNullableString(actor.artSourceId, path + '.artSourceId');
      assertCapability(actor.capability, path + '.capability');
      assertPlainObject(actor.initial, path + '.initial');
      assertOnlyFields(actor.initial, [
        'visible', 'x', 'y', 'z', 'facing', 'poseId'
      ], path + '.initial');
      if (typeof actor.initial.visible !== 'boolean') {
        fail('must be a boolean', path + '.initial.visible');
      }
      ['x', 'y', 'z'].forEach(function(axis) {
        assertFiniteNumber(actor.initial[axis], path + '.initial.' + axis);
      });
      assertString(actor.initial.facing, path + '.initial.facing', false);
      assertNullableString(actor.initial.poseId, path + '.initial.poseId');
      cloneJson(actor.source || {}, path + '.source');
    });
    return ids;
  }

  function validateBranches(branches) {
    if (!Array.isArray(branches) || !branches.length) {
      fail('must contain at least one preview path', 'scene.branches');
    }
    var ids = {};
    branches.forEach(function(branch, index) {
      var path = 'scene.branches[' + index + ']';
      assertPlainObject(branch, path);
      assertOnlyFields(branch, ['id', 'label', 'condition'], path);
      assertId(branch.id, path + '.id');
      if (ids[branch.id]) fail('duplicates path id "' + branch.id + '"', path + '.id');
      ids[branch.id] = true;
      assertString(branch.label, path + '.label', false);
      if (branch.condition !== null) cloneJson(branch.condition, path + '.condition');
    });
    return ids;
  }

  function validateTracks(tracks, actorIds, branchIds) {
    if (!Array.isArray(tracks)) fail('must be an array', 'scene.tracks');
    var trackIds = {};
    var clipIds = {};
    tracks.forEach(function(track, trackIndex) {
      var path = 'scene.tracks[' + trackIndex + ']';
      assertPlainObject(track, path);
      assertOnlyFields(track, ['id', 'type', 'actorId', 'label', 'clips'], path);
      assertId(track.id, path + '.id');
      if (trackIds[track.id]) fail('duplicates track id "' + track.id + '"', path + '.id');
      trackIds[track.id] = true;
      if (TRACK_TYPES.indexOf(track.type) === -1) fail('has unsupported track type', path + '.type');
      if (track.actorId !== null) {
        assertId(track.actorId, path + '.actorId');
        if (!actorIds[track.actorId]) fail('references an unknown actor', path + '.actorId');
      }
      if (['actor', 'pose', 'movement'].indexOf(track.type) !== -1 && track.actorId === null) {
        fail('requires an actorId', path + '.actorId');
      }
      assertString(track.label, path + '.label', true);
      if (!Array.isArray(track.clips)) fail('must be an array', path + '.clips');
      track.clips.forEach(function(clip, clipIndex) {
        var clipPath = path + '.clips[' + clipIndex + ']';
        assertPlainObject(clip, clipPath);
        assertOnlyFields(clip, [
          'id', 'kind', 'startFrame', 'durationFrames', 'pathIds', 'capability',
          'payload', 'source'
        ], clipPath);
        assertId(clip.id, clipPath + '.id');
        if (clipIds[clip.id]) fail('duplicates clip id "' + clip.id + '"', clipPath + '.id');
        clipIds[clip.id] = true;
        if (TRACK_CLIP_KINDS[track.type].indexOf(clip.kind) === -1) {
          fail('kind "' + clip.kind + '" is incompatible with ' + track.type + ' track', clipPath + '.kind');
        }
        assertInteger(clip.startFrame, clipPath + '.startFrame', 0);
        assertInteger(clip.durationFrames, clipPath + '.durationFrames', 0);
        if (!Array.isArray(clip.pathIds)) fail('must be an array', clipPath + '.pathIds');
        clip.pathIds.forEach(function(pathId, pathIndex) {
          assertId(pathId, clipPath + '.pathIds[' + pathIndex + ']');
          if (!branchIds[pathId]) fail('references an unknown preview path', clipPath + '.pathIds[' + pathIndex + ']');
        });
        assertCapability(clip.capability, clipPath + '.capability');
        assertPlainObject(clip.payload, clipPath + '.payload');
        cloneJson(clip.payload, clipPath + '.payload');
        assertPlainObject(clip.source, clipPath + '.source');
        cloneJson(clip.source, clipPath + '.source');
      });
    });
  }

  function validateNative(native) {
    assertPlainObject(native, 'scene.native');
    assertOnlyFields(native, ['sourceAssetId', 'commands', 'gaps'], 'scene.native');
    assertNullableString(native.sourceAssetId, 'scene.native.sourceAssetId');
    if (!Array.isArray(native.commands)) fail('must be an array', 'scene.native.commands');
    var ids = {};
    native.commands.forEach(function(command, index) {
      var path = 'scene.native.commands[' + index + ']';
      assertPlainObject(command, path);
      assertOnlyFields(command, ['id', 'boundaryId', 'words', 'kind', 'source'], path);
      assertId(command.id, path + '.id');
      if (ids[command.id]) fail('duplicates native command id "' + command.id + '"', path + '.id');
      ids[command.id] = true;
      assertId(command.boundaryId, path + '.boundaryId');
      assertString(command.kind, path + '.kind', false);
      if (!Array.isArray(command.words)) fail('must be an array', path + '.words');
      command.words.forEach(function(word, wordIndex) {
        assertInteger(word, path + '.words[' + wordIndex + ']', 0);
        if (word > 0xFFFFFFFF) fail('must fit an unsigned 32-bit word', path + '.words[' + wordIndex + ']');
      });
      assertPlainObject(command.source, path + '.source');
      cloneJson(command.source, path + '.source');
    });
    if (!Array.isArray(native.gaps)) fail('must be an array', 'scene.native.gaps');
    cloneJson(native.gaps, 'scene.native.gaps');
  }

  function validateExportRequirements(requirements) {
    assertPlainObject(requirements, 'scene.exportRequirements');
    assertOnlyFields(requirements, ['capability', 'reasons', 'allocationBytes', 'features'], 'scene.exportRequirements');
    assertCapability(requirements.capability, 'scene.exportRequirements.capability');
    if (!Array.isArray(requirements.reasons)) fail('must be an array', 'scene.exportRequirements.reasons');
    requirements.reasons.forEach(function(reason, index) {
      assertString(reason, 'scene.exportRequirements.reasons[' + index + ']', false);
    });
    assertInteger(requirements.allocationBytes, 'scene.exportRequirements.allocationBytes', 0);
    if (!Array.isArray(requirements.features)) fail('must be an array', 'scene.exportRequirements.features');
    requirements.features.forEach(function(feature, index) {
      assertId(feature, 'scene.exportRequirements.features[' + index + ']');
    });
  }

  function validateSceneDocument(document) {
    assertPlainObject(document, 'scene');
    assertOnlyFields(document, [
      'format', 'schemaVersion', 'identity', 'background', 'actors', 'tracks',
      'branches', 'native', 'exportRequirements'
    ], 'scene');
    if (document.format !== FORMAT) fail('unsupported format', 'scene.format');
    if (document.schemaVersion !== SCHEMA_VERSION) fail('unsupported schema version', 'scene.schemaVersion');
    validateIdentity(document.identity);
    validateBackground(document.background);
    var actorIds = validateActors(document.actors);
    var branchIds = validateBranches(document.branches);
    validateTracks(document.tracks, actorIds, branchIds);
    validateNative(document.native);
    validateExportRequirements(document.exportRequirements);
    return document;
  }

  function cloneSceneDocument(document) {
    validateSceneDocument(document);
    return cloneJson(document, 'scene');
  }

  function serializeSceneDocument(document, space) {
    validateSceneDocument(document);
    return stableStringify(document, space);
  }

  function parseSceneDocument(input) {
    var parsed = typeof input === 'string' ? JSON.parse(input) : cloneJson(input, 'scene');
    return createSceneDocument(parsed);
  }

  function createActor(options) {
    options = options || {};
    var initial = Object.assign({
      visible: true, x: 0, y: 0, z: 0, facing: 'default', poseId: null
    }, options.initial || {});
    var actor = {
      id: options.id || 'actor:new',
      label: options.label || 'Actor',
      slot: options.slot == null ? null : options.slot,
      artSourceId: options.artSourceId == null ? null : options.artSourceId,
      capability: options.capability || CAPABILITIES.NEEDS_RESEARCH,
      initial: cloneJson(initial, 'actor.initial'),
      source: cloneJson(options.source || {}, 'actor.source')
    };
    validateActors([actor]);
    return actor;
  }

  function createTrack(options) {
    options = options || {};
    return {
      id: options.id || 'track:new',
      type: options.type || 'flow',
      actorId: options.actorId == null ? null : options.actorId,
      label: options.label || '',
      clips: cloneJson(options.clips || [], 'track.clips')
    };
  }

  function createClip(options) {
    options = options || {};
    return {
      id: options.id || 'clip:new',
      kind: options.kind || 'marker',
      startFrame: options.startFrame == null ? 0 : options.startFrame,
      durationFrames: options.durationFrames == null ? 0 : options.durationFrames,
      pathIds: cloneJson(options.pathIds || [], 'clip.pathIds'),
      capability: options.capability || CAPABILITIES.NEEDS_RESEARCH,
      payload: cloneJson(options.payload || {}, 'clip.payload'),
      source: cloneJson(options.source || {}, 'clip.source')
    };
  }

  function findTrack(document, trackId) {
    for (var index = 0; index < document.tracks.length; index++) {
      if (document.tracks[index].id === trackId) return document.tracks[index];
    }
    return null;
  }

  function addActor(document, actor) {
    document.actors.push(cloneJson(actor, 'actor'));
    validateSceneDocument(document);
    return actor.id;
  }

  function removeActor(document, actorId) {
    var before = document.actors.length;
    document.actors = document.actors.filter(function(actor) { return actor.id !== actorId; });
    if (document.actors.length === before) return false;
    document.tracks = document.tracks.filter(function(track) { return track.actorId !== actorId; });
    validateSceneDocument(document);
    return true;
  }

  function addTrack(document, track) {
    document.tracks.push(cloneJson(track, 'track'));
    validateSceneDocument(document);
    return track.id;
  }

  function addClip(document, trackId, clip) {
    var track = findTrack(document, trackId);
    if (!track) fail('unknown track "' + trackId + '"', 'trackId');
    track.clips.push(cloneJson(clip, 'clip'));
    validateSceneDocument(document);
    return clip.id;
  }

  function updateClip(document, clipId, updater) {
    if (typeof updater !== 'function') fail('must be a function', 'updater');
    for (var trackIndex = 0; trackIndex < document.tracks.length; trackIndex++) {
      var clips = document.tracks[trackIndex].clips;
      for (var clipIndex = 0; clipIndex < clips.length; clipIndex++) {
        if (clips[clipIndex].id !== clipId) continue;
        updater(clips[clipIndex]);
        validateSceneDocument(document);
        return true;
      }
    }
    return false;
  }

  function removeClip(document, clipId) {
    for (var trackIndex = 0; trackIndex < document.tracks.length; trackIndex++) {
      var clips = document.tracks[trackIndex].clips;
      var next = clips.filter(function(clip) { return clip.id !== clipId; });
      if (next.length === clips.length) continue;
      document.tracks[trackIndex].clips = next;
      validateSceneDocument(document);
      return true;
    }
    return false;
  }

  function createHistory(document, limit) {
    validateSceneDocument(document);
    if (limit == null) limit = DEFAULT_HISTORY_LIMIT;
    assertInteger(limit, 'history.limit', 1);
    return {
      present: cloneSceneDocument(document),
      past: [],
      future: [],
      limit: limit,
      revision: 0
    };
  }

  function execute(history, label, mutator) {
    assertString(label, 'command.label', false);
    if (typeof mutator !== 'function') fail('must be a function', 'command.mutator');
    var before = cloneSceneDocument(history.present);
    var after = cloneSceneDocument(history.present);
    mutator(after);
    validateSceneDocument(after);
    history.past.push({ label: label, before: before, after: cloneSceneDocument(after) });
    if (history.past.length > history.limit) history.past.shift();
    history.present = after;
    history.future = [];
    history.revision++;
    return history.present;
  }

  function undo(history) {
    if (!history.past.length) return false;
    var command = history.past.pop();
    history.future.push(command);
    history.present = cloneSceneDocument(command.before);
    history.revision++;
    return true;
  }

  function redo(history) {
    if (!history.future.length) return false;
    var command = history.future.pop();
    history.past.push(command);
    history.present = cloneSceneDocument(command.after);
    history.revision++;
    return true;
  }

  function clearHistory(history) {
    history.past = [];
    history.future = [];
  }

  OB64.cutsceneModel = {
    format: FORMAT,
    schemaVersion: SCHEMA_VERSION,
    previewFps: PREVIEW_FPS,
    capabilities: CAPABILITIES,
    capabilityLabels: CAPABILITY_LABELS,
    trackTypes: TRACK_TYPES,
    trackClipKinds: TRACK_CLIP_KINDS,
    SceneDocumentError: SceneDocumentError,
    createSceneDocument: createSceneDocument,
    validateSceneDocument: validateSceneDocument,
    cloneSceneDocument: cloneSceneDocument,
    serializeSceneDocument: serializeSceneDocument,
    parseSceneDocument: parseSceneDocument,
    createActor: createActor,
    createTrack: createTrack,
    createClip: createClip,
    findTrack: findTrack,
    addActor: addActor,
    removeActor: removeActor,
    addTrack: addTrack,
    addClip: addClip,
    updateClip: updateClip,
    removeClip: removeClip,
    createHistory: createHistory,
    execute: execute,
    undo: undo,
    redo: redo,
    clearHistory: clearHistory,
    cloneJson: cloneJson,
    stableStringify: stableStringify
  };
})(window.OB64);
