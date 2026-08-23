// Lordly Caliber - execution-driven Director preview runtime.
//
// The Director stream is a scheduler program, not a movie timeline. This
// module executes its corrected primitive/composite model on native update
// ticks and exposes immutable Stage snapshots for deterministic scrubbing.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var M = OB64.cutsceneModel;
  if (!M) throw new Error('cutscene-runtime.js requires cutscene-model.js');

  var DEFAULT_MAX_TICKS = 30000;
  var SCENE_TRANSFORM_TABLE_KEY = 0x019A63EC;
  var SCENE_TRANSFORM_CHANNEL_COUNT = 20;
  var SCENE_TRANSFORM_KEYFRAME_BYTES = 0x1E0;
  var SCENE_TRANSFORM_FIELDS = [
    { name: 'rotationX', offset: 0x000 },
    { name: 'rotationY', offset: 0x050 },
    { name: 'translateX', offset: 0x0A0 },
    { name: 'translateY', offset: 0x0F0 },
    { name: 'translateZ', offset: 0x140 },
    { name: 'uniformScale', offset: 0x190 }
  ];
  var PHASE_FOR_FACING = [0, 9, 6, 3];
  var FACING_FOR_PHASE = [0, 9, 8, 3, 7, 7, 2, 6, 5, 1, 4, 4];
  var bindings = typeof WeakMap === 'function' ? new WeakMap() : null;

  function RuntimeError(message, code) {
    this.name = 'CutsceneRuntimeError';
    this.message = message;
    this.code = code || 'director-runtime';
  }
  RuntimeError.prototype = Object.create(Error.prototype);
  RuntimeError.prototype.constructor = RuntimeError;

  function fail(message, code) { throw new RuntimeError(message, code); }
  function unsigned(value) { return Number(value) >>> 0; }
  function signed(value) { return unsigned(value) | 0; }
  function lowU16(value) { return unsigned(value) & 0xFFFF; }
  function lowS16(value) {
    value = lowU16(value);
    return value & 0x8000 ? value - 0x10000 : value;
  }
  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }
  function mix(left, right, amount) { return left + (right - left) * amount; }
  function finite(value, fallback) { return Number.isFinite(value) ? value : fallback; }
  function fixed(value) { return signed(value) / 1000; }
  function poseId(bank, key, facing) {
    return 'cutscene-pose:' + bank + ':' + key + ':' + facing;
  }
  function uniquePush(rows, value) {
    if (value && rows.indexOf(value) === -1) rows.push(value);
  }

  function readU32(bytes, offset, label) {
    if (!(bytes instanceof Uint8Array) || offset < 0 || offset + 4 > bytes.length) {
      fail((label || 'Director resource') + ' ends before byte ' + (offset + 4) + '.',
        'scene-transform-bounds');
    }
    return ((bytes[offset] << 24) | (bytes[offset + 1] << 16) |
      (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
  }

  function readS32(bytes, offset, label) {
    return readU32(bytes, offset, label) | 0;
  }

  function identityTransformChannel() {
    return {
      rotationX: 0,
      rotationY: 0,
      translateX: 0,
      translateY: 0,
      translateZ: 0,
      uniformScale: 1
    };
  }

  function identityTransformChannels() {
    var channels = [];
    for (var index = 0; index < SCENE_TRANSFORM_CHANNEL_COUNT; index++) {
      channels.push(identityTransformChannel());
    }
    return channels;
  }

  function decodeSceneTransformResource(z64, resourceIndex) {
    if (!(z64 instanceof Uint8Array)) {
      fail('Normalized z64 bytes are required to decode a scene transform.',
        'scene-transform-rom');
    }
    if (!OB64.art || typeof OB64.art.readResource !== 'function' ||
        typeof OB64.art.readCompressedResource !== 'function') {
      fail('art.js is required to decode scene-transform resources.',
        'scene-transform-codec');
    }
    if (!Number.isInteger(resourceIndex) || resourceIndex < 0) {
      fail('Scene-transform resource index must be a non-negative integer.',
        'scene-transform-index');
    }
    var table = OB64.art.readResource(z64, SCENE_TRANSFORM_TABLE_KEY).stored;
    var tableOffset = resourceIndex * 4;
    if (tableOffset + 4 > table.length) {
      fail('Scene-transform resource index ' + resourceIndex + ' exceeds the ' +
        Math.floor(table.length / 4) + '-entry table.', 'scene-transform-index');
    }
    var resourceKey = readU32(table, tableOffset, 'Scene-transform resource table');
    var decoded = OB64.art.readCompressedResource(z64, resourceKey).decoded;
    var keyframeCount = readU32(decoded, 0, 'Scene-transform resource');
    var groupCount = readU32(decoded, 4, 'Scene-transform resource');
    var keyframeEnd = 8 + keyframeCount * SCENE_TRANSFORM_KEYFRAME_BYTES;
    if (keyframeCount > 0x100 || groupCount > 0x100 || keyframeEnd > decoded.length) {
      fail('Scene-transform resource ' + resourceIndex + ' has an invalid header.',
        'scene-transform-header');
    }
    var keyframes = [];
    for (var keyframeIndex = 0; keyframeIndex < keyframeCount; keyframeIndex++) {
      var keyframeBase = 8 + keyframeIndex * SCENE_TRANSFORM_KEYFRAME_BYTES;
      var channels = [];
      for (var channelIndex = 0;
          channelIndex < SCENE_TRANSFORM_CHANNEL_COUNT; channelIndex++) {
        var channel = {};
        SCENE_TRANSFORM_FIELDS.forEach(function(field) {
          channel[field.name + 'Raw'] = readS32(decoded,
            keyframeBase + field.offset + channelIndex * 4,
            'Scene-transform keyframe');
          channel[field.name] = channel[field.name + 'Raw'] / 1000;
        });
        channels.push(channel);
      }
      keyframes.push(channels);
    }
    var groups = [];
    var cursor = keyframeEnd;
    for (var groupIndex = 0; groupIndex < groupCount; groupIndex++) {
      var keyframeIds = [];
      var continuationWords = [];
      while (keyframeIds.length < SCENE_TRANSFORM_CHANNEL_COUNT) {
        var keyframeId = readS32(decoded, cursor,
          'Scene-transform sequence group ' + groupIndex);
        var continuation = readS32(decoded, cursor + 4,
          'Scene-transform sequence group ' + groupIndex);
        cursor += 8;
        if (keyframeId < 0 || keyframeId >= keyframeCount) {
          fail('Scene-transform group ' + groupIndex + ' selects invalid keyframe ' +
            keyframeId + '.', 'scene-transform-keyframe');
        }
        keyframeIds.push(keyframeId);
        continuationWords.push(continuation);
        if (continuation === -1) break;
      }
      if (!keyframeIds.length || continuationWords[continuationWords.length - 1] !== -1) {
        fail('Scene-transform group ' + groupIndex + ' has no terminator within 20 IDs.',
          'scene-transform-sequence');
      }
      groups.push({ keyframeIds: keyframeIds, continuationWords: continuationWords });
    }
    if (cursor !== decoded.length) {
      fail('Scene-transform resource ' + resourceIndex + ' has ' +
        (decoded.length - cursor) + ' unowned decoded bytes.', 'scene-transform-layout');
    }
    return {
      resourceIndex: resourceIndex,
      resourceKey: resourceKey,
      keyframeCount: keyframeCount,
      groupCount: groupCount,
      keyframes: keyframes,
      groups: groups
    };
  }

  function clonePoint(value, fallback) {
    value = value || {};
    fallback = fallback || { x: 0, y: 0, z: 0 };
    return {
      x: finite(value.x, fallback.x),
      y: finite(value.y, fallback.y),
      z: finite(value.z, fallback.z)
    };
  }

  function defaultCamera() {
    return {
      target: { x: 0, y: 0, z: 0 },
      eye: { x: 0, y: 0, z: 340 },
      fovYDegrees: 38,
      modelScale: 0.1,
      aspect: 4 / 3,
      near: 1,
      far: 5000,
      up: { x: 0, y: 1, z: 0 },
      sourceNodeId: null,
      status: 'unresolved launch camera fallback'
    };
  }

  function modeZeroCamera(bank) {
    var camera = defaultCamera();
    camera.eye = bank === 'actor'
      ? { x: 360, y: 0, z: 0 }
      : { x: 0, y: 0, z: 360 };
    camera.sourceNodeId = 'mode-zero-camera-initializer';
    camera.status = 'exact mode-zero ' + bank + ' camera initializer';
    return camera;
  }

  function cameraFromProjection(projection, status, sourceNodeId) {
    var camera = defaultCamera();
    camera.target = clonePoint(projection.target, camera.target);
    camera.eye = clonePoint(projection.eye, camera.eye);
    camera.up = clonePoint(projection.up, camera.up);
    camera.fovYDegrees = finite(projection.fovYDegrees, camera.fovYDegrees);
    camera.modelScale = finite(projection.modelScale, camera.modelScale);
    camera.aspect = finite(projection.aspect, camera.aspect);
    camera.near = finite(projection.near, camera.near);
    camera.far = finite(projection.far, camera.far);
    camera.screenWidth = finite(projection.screenWidth, 320);
    camera.screenHeight = finite(projection.screenHeight, 240);
    camera.sourceNodeId = sourceNodeId || 'runtime-observation';
    camera.status = status || projection.calibrationStatus ||
      'runtime-observed Actor camera';
    return camera;
  }

  function directorModeFromProfile(scene) {
    var mode = scene && scene.launchProfile && scene.launchProfile.directorMode;
    if (!mode) fail('The Director scene has no launch profile.', 'missing-launch-profile');
    return {
      value: mode.value,
      status: mode.status,
      evidenceStatus: mode.evidenceStatus,
      source: mode.source
    };
  }

  function cameraFromLaunchProfile(profile, bank) {
    if (!profile) return defaultCamera();
    if (profile.kind === 'mode-zero-initializer') return modeZeroCamera(bank);
    if (profile.projection) {
      return cameraFromProjection(profile.projection, profile.status,
        'launch-profile:' + profile.kind);
    }
    var camera = defaultCamera();
    camera.status = profile.status;
    return camera;
  }

  function projectionFromCamera(camera, status) {
    camera = camera || defaultCamera();
    return {
      mode: 'native-perspective-runtime',
      status: status || camera.status || 'Director camera bank',
      modelScale: finite(camera.modelScale, 0.1),
      eye: clonePoint(camera.eye),
      target: clonePoint(camera.target),
      up: clonePoint(camera.up, { x: 0, y: 1, z: 0 }),
      fovYDegrees: clamp(finite(camera.fovYDegrees, 38), 1, 179),
      aspect: finite(camera.aspect, 4 / 3),
      near: finite(camera.near, 1),
      far: finite(camera.far, 5000),
      screenWidth: finite(camera.screenWidth, 320),
      screenHeight: finite(camera.screenHeight, 240),
      sourceNodeId: camera.sourceNodeId || null
    };
  }

  function backgroundLayerRole(asset, index) {
    return index === 0 ? 'environment-base' : 'ordered-layer';
  }

  function backgroundLayers(entry, capability, catalog) {
    if (!entry || !Array.isArray(entry.archiveAssetIds)) return [];
    var members = Array.isArray(entry.members) &&
      entry.members.length === entry.archiveAssetIds.length
      ? entry.members : entry.archiveAssetIds.map(function(assetId, index) {
        return { ordinal: index, assetId: assetId };
      });
    return members.map(function(member, index) {
      var nativeOrdinal = Number.isFinite(member.ordinal) ? member.ordinal : index;
      var asset = catalog && catalog.getImageAsset
        ? catalog.getImageAsset(member.assetId) : null;
      var role = backgroundLayerRole(asset, index);
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: member.assetId,
        label: member.assetId,
        visible: true,
        depth: nativeOrdinal,
        nativeOrdinal: nativeOrdinal,
        role: role,
        capability: capability || M.capabilities.PREVIEW_ONLY,
        source: {
          sourceKind: 'director-runtime-background',
          selector: entry.selector,
          groupResourceKey: entry.groupResourceKey || null,
          traversalOrdinal: nativeOrdinal,
          associationStatus: entry.associationStatus || 'selector-table association'
        }
      };
    });
  }

  function stagedBackground(stageLayers, document, metadata) {
    if (!Array.isArray(stageLayers) || !stageLayers.length) return null;
    metadata = metadata || {};
    var layers = stageLayers.map(function(layer, index) {
      var role = layer.role || (index ? 'ordered-layer' : 'environment-base');
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: layer.assetId,
        label: layer.assetId,
        visible: true,
        depth: Number.isFinite(layer.depth) ? layer.depth : index,
        role: role,
        capability: M.capabilities.PREVIEW_ONLY,
        source: {
          sourceKind: metadata.sourceKind || 'staged-background',
          evidenceStatus: layer.evidenceStatus,
          associationStatus: layer.associationStatus
        }
      };
    });
    return {
      assetId: layers[0].assetId,
      layers: layers,
      capability: M.capabilities.PREVIEW_ONLY,
      projection: M.cloneJson(document.background.projection || {}, 'background.projection'),
      runtimeStatus: metadata.runtimeStatus || 'The launch profile supplies a complete Stage.',
      selectorTableId: metadata.selectorTableId || null,
      selector: Number.isInteger(metadata.selector) ? metadata.selector : null
    };
  }

  function observationBackground(scene, document) {
    var observation = scene && scene.backgroundRuntimeObservation;
    if (!observation) return null;
    return stagedBackground(observation.stageLayers, document, {
      sourceKind: 'runtime-observation',
      runtimeStatus: observation.associationStatus,
      selectorTableId: observation.selectorTableId,
      selector: Number.isInteger(observation.mode2SelectorByte)
        ? observation.mode2SelectorByte : observation.commandOperand
    });
  }

  function profileBackground(request, document) {
    if (!request) return null;
    return stagedBackground(request.stageLayers, document, {
      sourceKind: 'launch-profile',
      runtimeStatus: request.status,
      selectorTableId: request.selectorTableId,
      selector: request.selector
    });
  }

  function documentRows(document) {
    var rowsByNode = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        var nodeId = clip.source && clip.source.nodeId;
        if (!nodeId) return;
        if (!rowsByNode[nodeId]) rowsByNode[nodeId] = [];
        rowsByNode[nodeId].push({ track: track, clip: clip });
      });
    });
    return rowsByNode;
  }

  function compile(document, program, scene, catalog, options) {
    options = options || {};
    M.validateSceneDocument(document);
    if (!program || !Array.isArray(program.primitives) ||
        !Array.isArray(program.composites)) {
      fail('A corrected Director program is required.', 'invalid-program');
    }
    if (!scene || scene.engine !== 'director') {
      fail('The execution runtime accepts Director scenes only.', 'invalid-scene');
    }

    var maxTicks = Number.isInteger(options.maxTicks) && options.maxTicks > 0
      ? options.maxTicks : DEFAULT_MAX_TICKS;
    var rowsByNode = documentRows(document);
    var actorTemplateBySlot = {};
    document.actors.forEach(function(actor) { actorTemplateBySlot[actor.slot] = actor; });
    var assumptions = [];
    var missingInputs = [];
    var trace = [];
    var states = [];
    var transformResourceCache = {};

    function assumption(text) { uniquePush(assumptions, text); }
    function missing(text) { uniquePush(missingInputs, text); }
    function rowsFor(node) { return rowsByNode[node.id] || []; }
    function rowFor(node, kind) {
      return rowsFor(node).find(function(row) { return !kind || row.clip.kind === kind; }) || null;
    }

    function transformResource(resourceIndex) {
      if (transformResourceCache[resourceIndex]) return transformResourceCache[resourceIndex];
      if (!(options.z64 instanceof Uint8Array)) return null;
      var decoded = decodeSceneTransformResource(options.z64, resourceIndex);
      transformResourceCache[resourceIndex] = decoded;
      return decoded;
    }

    var launchProfile = scene.launchProfile;
    var observedBackground = observationBackground(scene, document);
    var directorMode = directorModeFromProfile(scene);
    var registeredCamera = cameraFromLaunchProfile(
      launchProfile.cameras.registered, 'registered');
    var actorCamera = cameraFromLaunchProfile(launchProfile.cameras.actor, 'actor');
    if (directorMode.evidenceStatus === 'external-unresolved') {
      missing('The launch profile cannot identify this stream\'s Director mode.');
    }
    if (launchProfile.cameras.actor.evidenceStatus === 'corpus-family-inferred') {
      assumption(launchProfile.cameras.actor.status);
      missing('This scene\'s initial Actor camera is supplied outside the Director stream.');
    } else if (launchProfile.cameras.actor.evidenceStatus === 'external-unresolved') {
      missing('The launch profile does not contain this scene\'s initial Actor camera.');
    }
    var state = {
      tick: 0,
      actors: {},
      movementJobs: {},
      turnJobs: {},
      tintJobs: {},
      bodyPoseJobs: {},
      yJobs: {},
      projectionJob: null,
      overlayJob: null,
      sceneColorJob: null,
      sceneTransformJob: null,
      titleJob: null,
      registeredCounter: null,
      transformDivider: null,
      transformChannels: identityTransformChannels(),
      directorMode: directorMode.value,
      directorModeStatus: directorMode.status,
      cameras: { registered: registeredCamera, actor: actorCamera },
      projectionTransform: { translateX: 0, translateY: 0, scaleX: 1, scaleY: 1 },
      background: observedBackground || M.cloneJson(document.background, 'background'),
      dialogues: {},
      spriteEffects: {},
      audioEvents: [],
      cameraEvents: [],
      effectEvents: [],
      flowEvents: [],
      scheduled: [],
      terminal: false,
      textSpeed: 512,
      sceneColor: { red: 255, green: 255, blue: 255 },
      overlay: null,
      executedNodeIds: []
    };

    if (observedBackground) {
      trace.push({ tick: 0, kind: 'runtime-input', label: 'Observed background route' });
    }

    function templateForSlot(slot) { return actorTemplateBySlot[slot] || null; }

    function selectorFromTemplate(template, fallback) {
      fallback = fallback || {};
      var bankMatch = template && String(template.artSourceId || '').match(/cutscene-art-bank:(\d+)/);
      var facingMatch = template && String(template.initial.facing || '').match(/^native-(\d+)$/);
      return {
        bank: bankMatch ? Number(bankMatch[1]) : finite(fallback.bank, null),
        key: template && Number.isInteger(template.source.animationKey)
          ? template.source.animationKey : finite(fallback.key, null),
        facing: facingMatch ? Number(facingMatch[1]) : finite(fallback.facing, null),
        variant: template && Number.isInteger(template.source.variantSelector)
          ? template.source.variantSelector : finite(fallback.variant, 0)
      };
    }

    function programForActor(actor) {
      if (!catalog || !catalog.getPhysicalPoseProgram || !actor ||
          !Number.isInteger(actor.bank) || !Number.isInteger(actor.animationKey) ||
          !Number.isInteger(actor.nativeFacing)) return null;
      return catalog.getPhysicalPoseProgram(actor.bank, actor.animationKey,
        actor.nativeFacing, actor.variantSelector);
    }

    function startPose(actor) {
      actor.poseFrame = 0;
      var poseProgram = programForActor(actor);
      var controlOpcodes = poseProgram && poseProgram.controlOpcodes || [];
      actor.poseLoop = !poseProgram || controlOpcodes.indexOf(0x04) !== -1 ||
        controlOpcodes.indexOf('0x04') !== -1;
      actor.poseDuration = poseProgram ? poseProgram.durationFrames : 0;
      actor.poseReadyTick = state.tick + Math.max(1,
        Math.ceil((actor.poseDuration || 2) / 2));
    }

    function ensureActor(slot) {
      if (state.actors[slot]) return state.actors[slot];
      var template = templateForSlot(slot);
      var selector = selectorFromTemplate(template);
      var actor = {
        id: template ? template.id : 'actor:runtime:slot:' + String(slot).padStart(2, '0'),
        label: template ? template.label : 'Actor slot ' + slot,
        slot: slot,
        artSourceId: template ? template.artSourceId : null,
        capability: template ? template.capability : M.capabilities.NEEDS_RESEARCH,
        visible: false,
        opacityByte: 255,
        renderModeByte: template && template.source &&
          Number.isInteger(template.source.renderMode)
          ? template.source.renderMode & 0xFF : 0,
        x: template ? template.initial.x : 0,
        y: template ? template.initial.y : 0,
        z: template ? template.initial.z : 0,
        secondaryY: 0,
        heightModeByte: 0,
        facing: Number.isInteger(selector.facing) ? 'native-' + selector.facing : 'unresolved',
        poseId: Number.isInteger(selector.bank) && Number.isInteger(selector.key) &&
          Number.isInteger(selector.facing) ? poseId(selector.bank, selector.key, selector.facing) : null,
        bank: selector.bank,
        animationKey: selector.key,
        nativeFacing: selector.facing,
        variantSelector: selector.variant,
        poseFrame: 0,
        poseLoop: true,
        poseDuration: 0,
        poseReadyTick: 0,
        bodyPoseProgram: null,
        movementFrame: 0,
        activeMovementId: null,
        uniformScale: 1,
        tint: { red: 255, green: 255, blue: 255 },
        yawDegrees: 0,
        transformChannel: 0,
        source: template ? M.cloneJson(template.source || {}, 'actor.source') : {}
      };
      state.actors[slot] = actor;
      startPose(actor);
      return actor;
    }

    function actorForCommand(slot, purpose) {
      if (state.actors[slot]) return state.actors[slot];
      if (!templateForSlot(slot)) {
        missing(purpose + ' for slot ' + slot + ' has no actor record or scene template.');
        return null;
      }
      var actor = ensureActor(slot);
      actor.visible = true;
      assumption('Slot ' + slot + ' uses its scene actor template for missing launch-time roster state.');
      missing('Launch-time actor record for slot ' + slot + ' is not stored in the Director stream.');
      return actor;
    }

    function selectedActors(selector) {
      if (selector === -1) {
        return Object.keys(state.actors).map(function(slot) { return state.actors[slot]; });
      }
      var actor = state.actors[selector];
      return actor ? [actor] : [];
    }

    function setActorSelector(actor, bank, key, facing, variant) {
      if (!actor) return;
      actor.bodyPoseProgram = null;
      if (Number.isInteger(bank) && bank !== -1) {
        actor.bank = bank;
        actor.artSourceId = 'cutscene-art-bank:' + bank;
      }
      if (Number.isInteger(key) && key !== -1) actor.animationKey = key;
      if (Number.isInteger(facing) && facing !== -1) {
        actor.nativeFacing = facing;
        actor.facing = 'native-' + facing;
      }
      if (Number.isInteger(variant) && variant !== -1) actor.variantSelector = variant;
      if (Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey) &&
          Number.isInteger(actor.nativeFacing)) {
        actor.poseId = poseId(actor.bank, actor.animationKey, actor.nativeFacing);
      }
      startPose(actor);
    }

    function executeActorCreate(node, words) {
      var slot = signed(words[1]);
      var actor = ensureActor(slot);
      var template = templateForSlot(slot);
      var rawSelector = {
        bank: signed(words[2]), key: signed(words[3]), facing: signed(words[4]),
        variant: unsigned(words[9]) & 0xFF
      };
      var templateOwnsNode = template && template.source.placeNodeId === node.id;
      var selector = templateOwnsNode
        ? selectorFromTemplate(template, rawSelector) : rawSelector;
      actor.visible = true;
      actor.x = templateOwnsNode ? template.initial.x : fixed(words[5]);
      actor.y = templateOwnsNode ? template.initial.y : fixed(words[6]);
      actor.z = templateOwnsNode ? template.initial.z : fixed(words[7]);
      actor.uniformScale = 1;
      actor.opacityByte = 255;
      actor.renderModeByte = unsigned(words[8]) & 0xFF;
      actor.secondaryY = 0;
      actor.heightModeByte = 0;
      actor.tint = { red: 255, green: 255, blue: 255 };
      actor.yawDegrees = 0;
      setActorSelector(actor, selector.bank, selector.key, selector.facing, selector.variant);
    }

    function executeActorState(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Actor State');
      if (!actor) return;
      var row = rowFor(node, 'pose');
      var payload = row && row.clip.payload || {};
      if (payload.nativeApplied === false) {
        // The native command is a no-op only when the launch roster truly lacks
        // this record. actorForCommand has already materialized the external
        // launch record for preview, so apply the command's exact raw selector.
        assumption('Actor State applies to the synthesized launch-time record for slot ' + slot + '.');
      }
      var bank = Number.isInteger(payload.bank) ? payload.bank : signed(words[2]);
      var key = Number.isInteger(payload.animationKey) ? payload.animationKey : signed(words[3]);
      var facing = Number.isInteger(payload.nativeFacing)
        ? payload.nativeFacing : signed(words[4]);
      var variantWord = signed(words[8]);
      var variant = Number.isInteger(payload.variantSelector)
        ? payload.variantSelector : (variantWord === -1 ? actor.variantSelector : unsigned(words[8]) & 0xFF);
      var x = Number.isFinite(payload.x) ? payload.x : (signed(words[5]) === -1000 ? null : fixed(words[5]));
      var y = Number.isFinite(payload.y) ? payload.y : (signed(words[6]) === -1000 ? null : fixed(words[6]));
      var z = Number.isFinite(payload.z) ? payload.z : (signed(words[7]) === -1000 ? null : fixed(words[7]));
      if (x != null) actor.x = x;
      if (y != null) actor.y = y;
      if (z != null) actor.z = z;
      actor.yawDegrees = 0;
      setActorSelector(actor, bank, key, facing, variant);
    }

    function executeMove(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Movement');
      if (!actor) return;
      var row = rowFor(node, 'movement');
      var payload = row && row.clip.payload || {};
      var rawStartX = fixed(words[2]);
      var rawStartZ = fixed(words[3]);
      var keepCurrent = rawStartX === -1 && rawStartZ === -1;
      var startX = keepCurrent ? actor.x : rawStartX;
      var startZ = keepCurrent ? actor.z : rawStartZ;
      var targetX = payload.to && Number.isFinite(payload.to.x)
        ? payload.to.x : fixed(words[4]);
      var targetZ = payload.to && Number.isFinite(payload.to.z)
        ? payload.to.z : fixed(words[5]);
      var speedRaw = Number.isFinite(payload.nativeSpeed) &&
        payload.nativeSpeed !== fixed(words[7])
        ? Math.round(payload.nativeSpeed * 1000) : signed(words[7]);
      var distance = Math.hypot(targetX - startX, targetZ - startZ);
      var rawCount = speedRaw !== 0
        ? Math.trunc(distance * (1000 / speedRaw)) : signed(words[6]);
      var remaining = lowU16(rawCount);
      var denominator = speedRaw !== 0 ? lowS16(rawCount) : signed(words[6]);
      actor.x = startX;
      actor.z = startZ;
      if (!denominator || !remaining) {
        assumption('A zero-count movement is shown as an immediate target assignment.');
        actor.x = targetX;
        actor.z = targetZ;
        delete state.movementJobs[slot];
        return;
      }
      state.movementJobs[slot] = {
        nodeId: node.id,
        slot: slot,
        remaining: remaining,
        vx: (targetX - startX) / denominator,
        vz: (targetZ - startZ) / denominator,
        elapsed: 0
      };
      actor.activeMovementId = 'runtime-movement:' + node.id;
      actor.movementFrame = 0;
    }

    function executeTurn(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Facing turn');
      if (!actor) return;
      var startFacing = signed(words[2]);
      var targetFacing = signed(words[3]);
      var direction = signed(words[4]);
      var budget = signed(words[5]);
      var startPhase = PHASE_FOR_FACING[startFacing];
      var targetPhase = PHASE_FOR_FACING[targetFacing];
      if (!Number.isInteger(startPhase) || !Number.isInteger(targetPhase) ||
          (direction !== -1 && direction !== 1)) {
        missing('Facing turn ' + node.id + ' has an unsupported native phase selection.');
        actor.nativeFacing = targetFacing;
        actor.facing = 'native-' + targetFacing;
        return;
      }
      var distance = direction === -1
        ? (targetPhase - startPhase + 12) % 12
        : (startPhase - targetPhase + 12) % 12;
      if (!distance) return;
      var cadence = Math.trunc((Math.trunc(budget / 3) * 3) / distance);
      cadence = Math.max(1, cadence);
      state.turnJobs[slot] = {
        nodeId: node.id,
        slot: slot,
        startPhase: startPhase,
        targetFacing: targetFacing,
        phaseStep: -direction,
        phaseDistance: distance,
        cadence: cadence,
        elapsed: 0,
        completionCalls: 1 + (distance - 1) * cadence
      };
    }

    function executeProjection(node, words, identity) {
      var row = rowFor(node, 'camera');
      var payload = row && row.clip.payload || {};
      var target = identity ? { translateX: 0, translateY: 0, scaleX: 1, scaleY: 1 } : {
        translateX: payload.target && Number.isFinite(payload.target.translateX)
          ? payload.target.translateX : fixed(words[1]),
        translateY: payload.target && Number.isFinite(payload.target.translateY)
          ? payload.target.translateY : fixed(words[2]),
        scaleX: payload.target && Number.isFinite(payload.target.scaleX)
          ? payload.target.scaleX : fixed(words[3]),
        scaleY: payload.target && Number.isFinite(payload.target.scaleY)
          ? payload.target.scaleY : fixed(words[4])
      };
      var countWord = identity ? words[1] : words[5];
      var duration = row && Number.isInteger(row.clip.durationFrames)
        ? row.clip.durationFrames : lowU16(countWord);
      duration = Math.max(1, duration);
      state.projectionJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        from: Object.assign({}, state.projectionTransform),
        to: target
      };
      state.cameraEvents.push(eventRow(node, 'camera', 'Scene projection transition', {
        presentationKind: identity ? 'projection-identity-transition' : 'projection-transform',
        target: target,
        nativeCountdown: duration
      }));
    }

    function executeCamera(node, words, bank) {
      var camera = {
        target: { x: fixed(words[1]), y: fixed(words[2]), z: fixed(words[3]) },
        eye: { x: fixed(words[4]), y: fixed(words[5]), z: fixed(words[6]) },
        fovYDegrees: fixed(words[7]),
        sourceNodeId: node.id,
        status: bank === 'actor' ? 'opcode 0x36 actor-side camera' :
          'opcode 0x35 registered-object camera'
      };
      state.cameras[bank] = camera;
      state.cameraEvents.push(eventRow(node, 'camera', 'Camera pose', {
        presentationKind: 'camera-pose',
        cameraBank: bank,
        target: camera.target,
        eye: camera.eye,
        fovY: camera.fovYDegrees
      }));
    }

    function executeTint(node, words) {
      var selector = signed(words[1]);
      var duration = Math.max(1, signed(words[8]));
      selectedActors(selector).forEach(function(actor) {
        actor.tint = { red: signed(words[2]), green: signed(words[3]), blue: signed(words[4]) };
        state.tintJobs[actor.slot] = {
          nodeId: node.id,
          slot: actor.slot,
          duration: duration,
          remaining: duration,
          elapsed: 0,
          from: Object.assign({}, actor.tint),
          to: { red: signed(words[5]), green: signed(words[6]), blue: signed(words[7]) }
        };
      });
    }

    function executeOverlay(node, words) {
      var duration = Math.max(1, lowU16(words[1]));
      var startAlpha = signed(words[5]);
      if (startAlpha === -1 && state.overlay) startAlpha = state.overlay.alpha;
      if (startAlpha === -1) startAlpha = 0;
      state.overlay = {
        red: unsigned(words[2]) & 0xFF,
        green: unsigned(words[3]) & 0xFF,
        blue: unsigned(words[4]) & 0xFF,
        alpha: clamp(startAlpha, 0, 255),
        sourceNodeId: node.id
      };
      state.overlayJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        startAlpha: state.overlay.alpha,
        targetAlpha: unsigned(words[6]) & 0xFF
      };
    }

    function executeSceneColor(node, words) {
      var duration = Math.max(1, lowU16(words[7]));
      state.sceneColor = {
        red: unsigned(words[1]) & 0xFF,
        green: unsigned(words[2]) & 0xFF,
        blue: unsigned(words[3]) & 0xFF
      };
      state.sceneColorJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        from: Object.assign({}, state.sceneColor),
        to: {
          red: unsigned(words[4]) & 0xFF,
          green: unsigned(words[5]) & 0xFF,
          blue: unsigned(words[6]) & 0xFF
        }
      };
    }

    function executeBackground(node, words) {
      var commandOperand = signed(words[1]);
      var observation = scene.backgroundRuntimeObservation || null;
      if (observation && Array.isArray(observation.stageLayers) &&
          observation.stageLayers.length) {
        state.background = observationBackground(scene, document);
        return;
      }
      var requestProfile = launchProfile.background.requests.find(function(request) {
        return request.wordStart === node.startWord;
      }) || null;
      if (requestProfile && Array.isArray(requestProfile.stageLayers) &&
          requestProfile.stageLayers.length) {
        state.background = profileBackground(requestProfile, document);
        if (requestProfile.evidenceStatus === 'corpus-pattern-inferred') {
          assumption(requestProfile.status);
        }
        return;
      }
      if (!requestProfile || requestProfile.selectorTableId === null ||
          requestProfile.selector === null) {
        state.background = {
          assetId: null,
          layers: [],
          capability: M.capabilities.NEEDS_RESEARCH,
          projection: M.cloneJson(document.background.projection || {},
            'background.projection'),
          runtimeStatus: requestProfile ? requestProfile.status :
            'The launch profile has no matching background request.',
          selectorTableId: null,
          selector: null
        };
        missing(requestProfile ? requestProfile.status :
          'The launch profile has no matching background request.');
        return;
      }
      if (directorMode.value === 2) {
        state.background = {
          assetId: null,
          layers: [],
          capability: M.capabilities.NEEDS_RESEARCH,
          projection: M.cloneJson(document.background.projection || {},
            'background.projection'),
          runtimeStatus: requestProfile.status +
            ' The selected resource is not a complete environment.',
          selectorTableId: requestProfile.selectorTableId,
          selector: requestProfile.selector
        };
        if (requestProfile.evidenceStatus === 'corpus-pattern-inferred') {
          assumption(requestProfile.status);
        }
        missing('Mode-two background selector ' + requestProfile.selector +
          ' has no complete registered Stage; its raw resource is only a partial layer or is empty.');
        return;
      }
      var tableId = requestProfile.selectorTableId;
      var runtimeSelector = requestProfile.selector;
      if (requestProfile.selectorSource === 'director-command-operand' ||
          requestProfile.selectorSource === 'corpus-coordinated-command-mirror') {
        runtimeSelector = commandOperand;
      }
      if (requestProfile.evidenceStatus === 'corpus-pattern-inferred') {
        assumption(requestProfile.status);
        missing('The external mode-two background selector is represented by its authored command mirror.');
      }
      var entry = catalog && catalog.getBackgroundSelectorEntry
        ? catalog.getBackgroundSelectorEntry(tableId, runtimeSelector) : null;
      var layers = backgroundLayers(entry, M.capabilities.PREVIEW_ONLY, catalog);
      state.background = {
        assetId: layers.length ? layers[0].assetId : null,
        layers: layers,
        capability: M.capabilities.PREVIEW_ONLY,
        projection: M.cloneJson(document.background.projection || {}, 'background.projection'),
        runtimeStatus: requestProfile.status,
        selectorTableId: tableId,
        selector: runtimeSelector
      };
      if (!layers.length) missing('Background selector ' + runtimeSelector + ' in ' + tableId +
        ' has no renderable archive member.');
    }

    function dialogueSegments(entry) {
      var text = entry && String(entry.text || '') || '';
      var segments = text.split(/\[pause\]/i).map(function(segment) {
        return segment.replace(/\[clear\]/gi, '').trim();
      });
      return segments.length ? segments : [''];
    }

    function dialogueDuration(text) {
      var scalar = Math.max(64, state.textSpeed || 512);
      return clamp(Math.ceil(Math.max(1, String(text || '').length) * scalar / 512), 12, 480);
    }

    function executeDialogueCreate(node, words) {
      var windowId = signed(words[1]);
      var selector = signed(words[2]);
      var entrySelector = signed(words[3]);
      var archive = catalog && catalog.getSerifuArchiveForPresentationSelector
        ? catalog.getSerifuArchiveForPresentationSelector(selector) : null;
      var entry = archive && archive.entries && archive.entries[entrySelector] || null;
      var segments = dialogueSegments(entry);
      if (!entry) missing('Serifu selector ' + selector + ', entry ' + entrySelector +
        ' did not resolve to dialogue text.');
      state.dialogues[windowId] = {
        windowId: windowId,
        selector: selector,
        entrySelector: entrySelector,
        ownerActorSlot: signed(words[4]),
        archive: archive,
        entry: entry,
        segments: segments,
        segmentIndex: 0,
        readyTick: state.tick + dialogueDuration(segments[0]),
        paused: false,
        closed: false,
        layout: {
          x: signed(words[5]), y: signed(words[6]), portraitSide: signed(words[8]),
          mirrorPortrait: signed(words[9]), pointerEdge: signed(words[10]),
          portraitIdentity: signed(words[11]), portraitVariant: signed(words[12]),
          placementMode: signed(words[13])
        },
        sourceNodeId: node.id
      };
    }

    function executeDialogueResume(words) {
      var window = state.dialogues[signed(words[1])];
      if (!window || window.closed) return;
      window.segmentIndex = Math.min(window.segmentIndex + 1, window.segments.length - 1);
      window.paused = false;
      window.readyTick = state.tick + dialogueDuration(window.segments[window.segmentIndex]);
    }

    function executeDialogueClose(words) {
      var window = state.dialogues[signed(words[1])];
      if (window) window.closed = true;
    }

    function executeSpriteEffect(node, words) {
      var row = rowFor(node, 'effect');
      var payload = row && row.clip.payload || null;
      var slot = payload && Number.isInteger(payload.nativeEffectSlot)
        ? payload.nativeEffectSlot : unsigned(words[1]) & 0xFF;
      state.spriteEffects[slot] = {
        id: 'runtime-effect:' + node.id,
        kind: 'effect',
        trackId: 'track:runtime:effect',
        label: 'Native Cutscene sprite effect',
        startFrame: state.tick,
        durationFrames: 1,
        capability: row ? row.clip.capability : M.capabilities.PREVIEW_ONLY,
        payload: payload ? M.cloneJson(payload, 'effect.payload') : {
          sourceSystem: 'cutscene-sprite-native',
          nativeOpcode: '0x46',
          nativeEffectSlot: slot,
          bank: signed(words[2]),
          animationKey: signed(words[3]),
          nativeFacing: signed(words[4]),
          variantSelector: unsigned(words[9]) & 0xFF,
          renderPassSelector: unsigned(words[7]) & 0xFF,
          stageX: 160 + signed(words[5]),
          stageY: 120 - signed(words[6]),
          scale: signed(words[8]) / 100,
          poseId: poseId(signed(words[2]), signed(words[3]), signed(words[4]))
        },
        poseFrame: 0
      };
    }

    function executeBodyPose(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Body-pose program');
      if (!actor) return;
      var previous = actor.bodyPoseProgram || {};
      var artSource = signed(words[2]);
      var selector = signed(words[3]);
      var flagB = signed(words[4]);
      var flagA = signed(words[5]);
      var ownerContext = signed(words[6]);
      artSource = artSource === -1 ? actor.bank : artSource;
      selector = selector === -1 ? actor.animationKey : selector;
      flagB = flagB === -1
        ? (Number.isInteger(previous.flagB) ? previous.flagB : 0) : flagB;
      flagA = flagA === -1
        ? (Number.isInteger(previous.flagA) ? previous.flagA : actor.variantSelector) : flagA;
      ownerContext = ownerContext === -1
        ? (Number.isInteger(previous.ownerContext) ? previous.ownerContext : artSource)
        : ownerContext;
      actor.bank = artSource;
      actor.artSourceId = 'combat-actor-art-source:' + artSource;
      actor.animationKey = selector;
      actor.variantSelector = flagA & 0xFF;
      actor.poseId = 'body-pose:' + artSource + ':' + selector + ':' +
        flagA + ':' + flagB + ':' + ownerContext;
      actor.poseFrame = 0;
      actor.poseLoop = false;
      actor.poseDuration = 0;
      actor.poseReadyTick = state.tick;
      actor.bodyPoseProgram = {
        decoder: 'alternate-body-pose',
        artSource: artSource,
        selector: selector,
        flagB: flagB,
        flagA: flagA,
        ownerContext: ownerContext,
        displayedFrameToken: 0,
        initialization: 'native-cleared-frame-state'
      };
      delete state.bodyPoseJobs[slot];
    }

    function eventRow(node, kind, label, payload) {
      return {
        id: 'runtime:' + kind + ':' + node.id + ':' + state.tick,
        kind: kind,
        trackId: 'track:runtime:' + kind,
        label: label,
        startFrame: state.tick,
        durationFrames: 1,
        capability: M.capabilities.PREVIEW_ONLY,
        payload: payload || {}
      };
    }

    function setTransformChannels(keyframe) {
      state.transformChannels = keyframe.map(function(source) {
        var channel = {};
        SCENE_TRANSFORM_FIELDS.forEach(function(field) {
          channel[field.name] = source[field.name];
        });
        return channel;
      });
    }

    function executeSceneTransform(node, words) {
      var resourceIndex = signed(words[1]);
      var groupIndex = signed(words[2]);
      var duration = signed(words[3]);
      var resource;
      try {
        resource = transformResource(resourceIndex);
      } catch (error) {
        missing('Scene transform resource ' + resourceIndex + ' could not be decoded: ' +
          (error && error.message || String(error)));
      }
      if (!resource) {
        state.sceneTransformJob = {
          nodeId: node.id,
          resourceIndex: resourceIndex,
          groupIndex: groupIndex,
          remaining: Math.max(1, duration),
          decoded: false
        };
        missing('Scene transform resource ' + resourceIndex +
          ' needs normalized ROM bytes before Stage matrices can be decoded.');
        return;
      }
      var group = resource.groups[groupIndex];
      if (!group || !group.keyframeIds.length) {
        state.sceneTransformJob = null;
        missing('Scene transform resource ' + resourceIndex + ' has no sequence group ' +
          groupIndex + '.');
        return;
      }
      if (duration <= 0) {
        state.sceneTransformJob = null;
        missing('Scene transform ' + node.id + ' has a non-positive segment duration.');
        return;
      }
      setTransformChannels(resource.keyframes[group.keyframeIds[0]]);
      if (group.keyframeIds.length === 1) {
        state.sceneTransformJob = null;
        return;
      }
      state.sceneTransformJob = {
        nodeId: node.id,
        resourceIndex: resourceIndex,
        resourceKey: resource.resourceKey,
        groupIndex: groupIndex,
        duration: duration,
        segmentIndex: 0,
        elapsed: 0,
        remaining: (group.keyframeIds.length - 1) * duration,
        keyframeIds: group.keyframeIds.slice(),
        keyframes: resource.keyframes,
        decoded: true
      };
    }

    function executePrimitive(node) {
      if (!node) return;
      var words = node.rawWords.map(unsigned);
      var opcode = unsigned(words[0]);
      state.executedNodeIds.push(node.id);
      trace.push({ tick: state.tick, kind: 'command', nodeId: node.id,
        opcode: node.opcodeHex, name: node.name });

      if (opcode === 0x01) state.registeredCounter = { value: 1, armTick: state.tick };
      else if (opcode === 0x02) state.registeredCounter = null;
      else if (opcode === 0x03) executeActorState(node, words);
      else if (opcode === 0x05) executeDialogueClose(words);
      else if (opcode === 0x06) executeDialogueResume(words);
      else if (opcode === 0x07) executeMove(node, words);
      else if (opcode === 0x08) {
        if (state.directorMode === 0) executeSceneTransform(node, words);
      }
      else if (opcode === 0x13) {
        var releaseSelector = signed(words[1]);
        selectedActors(releaseSelector).forEach(function(actor) {
          actor.visible = false;
          delete state.movementJobs[actor.slot];
          delete state.turnJobs[actor.slot];
        });
      }
      else if (opcode === 0x14) executeActorCreate(node, words);
      else if (opcode === 0x15) executeTurn(node, words);
      else if (opcode === 0x1A) state.textSpeed = lowU16(words[1]);
      else if (opcode === 0x1B) executeOverlay(node, words);
      else if (opcode === 0x1C) {
        if (state.directorMode === 0) {
          selectedActors(signed(words[1])).forEach(function(actor) {
            actor.transformChannel = signed(words[2]);
          });
        }
      }
      else if (opcode === 0x1D) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.uniformScale = signed(words[2]) / 100;
      });
      else if (opcode === 0x1E) executeTint(node, words);
      else if (opcode === 0x22) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.yawDegrees = fixed(words[3]);
      });
      else if (opcode === 0x2A) {
        if (state.directorMode === 2) executeBodyPose(node, words);
      }
      else if (opcode === 0x2C) executeProjection(node, words, false);
      else if (opcode === 0x33) executeSceneColor(node, words);
      else if (opcode === 0x35) executeCamera(node, words, 'registered');
      else if (opcode === 0x36) executeCamera(node, words, 'actor');
      else if (opcode === 0x3A) state.effectEvents.push(eventRow(node, 'effect',
        'Scene vignette', { sourceSystem: 'director-native', nativeOpcode: '0x3A' }));
      else if (opcode === 0x45 || opcode === 0xAB) {
        Object.keys(actorTemplateBySlot).forEach(function(slot) {
          var actor = ensureActor(Number(slot));
          actor.visible = true;
        });
        assumption(launchProfile.roster.status);
        missing('Roster identities and formation positions are launch-profile inputs.');
      }
      else if (opcode === 0x46) executeSpriteEffect(node, words);
      else if (opcode === 0x47) state.shadowLight = {
        x: signed(words[1]), y: signed(words[2]), z: signed(words[3])
      };
      else if (opcode === 0x48) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.opacityByte = unsigned(words[2]) & 0xFF;
      });
      else if (opcode === 0x5F) {
        var x1 = signed(words[1]), z1 = signed(words[2]);
        var x2 = signed(words[3]), z2 = signed(words[4]);
        state.transformDivider = {
          x1: x1, z1: z1, x2: x2, z2: z2,
          trueChannel: signed(words[5]), falseChannel: signed(words[6])
        };
      }
      else if (opcode === 0x66) {
        var effectSlot = signed(words[1]);
        if (effectSlot === -1) state.spriteEffects = {};
        else delete state.spriteEffects[effectSlot];
      }
      else if (opcode === 0x69) {
        var secondary = state.actors[signed(words[1])];
        if (secondary) {
          secondary.secondaryY = fixed(words[2]);
          secondary.heightModeByte = signed(words[3]) === 0 ? 0x02 : 0x04;
        }
      }
      else if (opcode === 0x6E || opcode === 0x6F || opcode === 0x70 || opcode === 0xB4) {
        var audioRow = rowFor(node, 'audio');
        state.audioEvents.push(audioRow ? {
          id: audioRow.clip.id, kind: audioRow.clip.kind, trackId: audioRow.track.id,
          label: audioRow.track.label, startFrame: state.tick, durationFrames: 1,
          capability: audioRow.clip.capability,
          payload: M.cloneJson(audioRow.clip.payload, 'audio.payload')
        } : eventRow(node, 'audio', 'Native audio command', {
          sourceSystem: 'director-native', nativeOpcode: node.opcodeHex,
          nativeOperands: words.slice(1).map(signed)
        }));
      }
      else if (opcode === 0x73) state.effectEvents.push(eventRow(node, 'effect',
        'Sepia vignette cleanup', { sourceSystem: 'director-native', nativeOpcode: '0x73' }));
      else if (opcode === 0x7B) {
        var yActor = state.actors[signed(words[1])];
        if (yActor) {
          var interval = signed(words[2]);
          if (interval === -1) yActor.y = signed(words[3]);
          else state.yJobs[yActor.slot] = {
            nodeId: node.id, slot: yActor.slot, interval: Math.max(1, interval),
            delta: signed(words[3]), remaining: Math.max(0, lowU16(words[4])), elapsed: 0
          };
        }
      }
      else if (opcode === 0x7D) state.terminalStateReleased = true;
      else if (opcode === 0x7E) {
        state.overlay = null;
        state.overlayJob = null;
      }
      else if (opcode === 0xAF) {
        state.titleJob = { nodeId: node.id, remaining: 30, duration: 30, kind: 'alpha' };
        assumption('Prologue title alpha uses a 30-tick preview envelope.');
      }
      else if (opcode === 0xB0) {
        state.titleJob = { nodeId: node.id, remaining: 85, duration: 85, kind: 'reveal' };
        assumption('Prologue secondary-title reveal uses its static 85-update Stage envelope.');
      }
      else if (opcode === 0xBF) executeDialogueCreate(node, words);
      else if (opcode === 0x80000001) state.terminal = true;
      else if (opcode === 0x80000006) executeBackground(node, words);
    }

    function updateMovementJobs() {
      Object.keys(state.movementJobs).forEach(function(slot) {
        var job = state.movementJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.movementJobs[slot]; return; }
        actor.x += job.vx;
        actor.z += job.vz;
        job.elapsed += 1;
        job.remaining = (job.remaining - 1) & 0xFFFF;
        actor.movementFrame = job.elapsed;
        if (lowS16(job.remaining) === 0) {
          actor.activeMovementId = null;
          delete state.movementJobs[slot];
        }
      });
    }

    function updateTurnJobs() {
      Object.keys(state.turnJobs).forEach(function(slot) {
        var job = state.turnJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.turnJobs[slot]; return; }
        job.elapsed += 1;
        var step = Math.min(job.phaseDistance,
          1 + Math.floor((job.elapsed - 1) / job.cadence));
        var phase = (job.startPhase + job.phaseStep * step) % 12;
        if (phase < 0) phase += 12;
        var facing = FACING_FOR_PHASE[phase];
        actor.nativeFacing = facing;
        actor.facing = 'native-' + facing;
        actor.poseId = Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey)
          ? poseId(actor.bank, actor.animationKey, facing) : actor.poseId;
        if (job.elapsed >= job.completionCalls) {
          actor.nativeFacing = job.targetFacing;
          actor.facing = 'native-' + job.targetFacing;
          actor.poseId = Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey)
            ? poseId(actor.bank, actor.animationKey, job.targetFacing) : actor.poseId;
          startPose(actor);
          delete state.turnJobs[slot];
        }
      });
    }

    function updateTintJobs() {
      Object.keys(state.tintJobs).forEach(function(slot) {
        var job = state.tintJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.tintJobs[slot]; return; }
        job.elapsed += 1;
        job.remaining -= 1;
        var amount = clamp(job.elapsed / job.duration, 0, 1);
        actor.tint = {
          red: Math.round(mix(job.from.red, job.to.red, amount)),
          green: Math.round(mix(job.from.green, job.to.green, amount)),
          blue: Math.round(mix(job.from.blue, job.to.blue, amount))
        };
        if (job.remaining <= 0) delete state.tintJobs[slot];
      });
    }

    function updateProjectionJob() {
      var job = state.projectionJob;
      if (!job) return;
      job.elapsed += 1;
      job.remaining -= 1;
      var amount = clamp(job.elapsed / job.duration, 0, 1);
      state.projectionTransform = {
        translateX: mix(job.from.translateX, job.to.translateX, amount),
        translateY: mix(job.from.translateY, job.to.translateY, amount),
        scaleX: mix(job.from.scaleX, job.to.scaleX, amount),
        scaleY: mix(job.from.scaleY, job.to.scaleY, amount)
      };
      if (job.remaining <= 0) state.projectionJob = null;
    }

    function updateColorJobs() {
      if (state.overlayJob && state.overlay) {
        state.overlayJob.elapsed += 1;
        state.overlayJob.remaining -= 1;
        state.overlay.alpha = Math.round(mix(state.overlayJob.startAlpha,
          state.overlayJob.targetAlpha,
          clamp(state.overlayJob.elapsed / state.overlayJob.duration, 0, 1)));
        if (state.overlayJob.remaining <= 0) state.overlayJob = null;
      }
      if (state.sceneColorJob) {
        var colorJob = state.sceneColorJob;
        colorJob.elapsed += 1;
        colorJob.remaining -= 1;
        var amount = clamp(colorJob.elapsed / colorJob.duration, 0, 1);
        state.sceneColor = {
          red: Math.round(mix(colorJob.from.red, colorJob.to.red, amount)),
          green: Math.round(mix(colorJob.from.green, colorJob.to.green, amount)),
          blue: Math.round(mix(colorJob.from.blue, colorJob.to.blue, amount))
        };
        if (colorJob.remaining <= 0) state.sceneColorJob = null;
      }
    }

    function updateOtherJobs() {
      Object.keys(state.bodyPoseJobs).forEach(function(slot) {
        state.bodyPoseJobs[slot].remaining -= 1;
        if (state.bodyPoseJobs[slot].remaining <= 0) delete state.bodyPoseJobs[slot];
      });
      Object.keys(state.yJobs).forEach(function(slot) {
        var job = state.yJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.yJobs[slot]; return; }
        job.elapsed += 1;
        job.remaining -= 1;
        if (job.elapsed % job.interval === 0) actor.y += job.delta;
        if (job.remaining <= 0) delete state.yJobs[slot];
      });
      if (state.sceneTransformJob) {
        var transformJob = state.sceneTransformJob;
        if (!transformJob.decoded) {
          transformJob.remaining -= 1;
          if (transformJob.remaining <= 0) state.sceneTransformJob = null;
        } else {
          transformJob.elapsed += 1;
          transformJob.remaining -= 1;
          var from = transformJob.keyframes[
            transformJob.keyframeIds[transformJob.segmentIndex]];
          var to = transformJob.keyframes[
            transformJob.keyframeIds[transformJob.segmentIndex + 1]];
          state.transformChannels = from.map(function(fromChannel, channelIndex) {
            var toChannel = to[channelIndex];
            var output = {};
            SCENE_TRANSFORM_FIELDS.forEach(function(field) {
              var deltaRaw = Math.trunc((toChannel[field.name + 'Raw'] -
                fromChannel[field.name + 'Raw']) / transformJob.duration);
              output[field.name] = (fromChannel[field.name + 'Raw'] +
                deltaRaw * transformJob.elapsed) / 1000;
            });
            return output;
          });
          if (transformJob.elapsed >= transformJob.duration) {
            setTransformChannels(to);
            transformJob.segmentIndex += 1;
            transformJob.elapsed = 0;
            if (transformJob.segmentIndex >= transformJob.keyframeIds.length - 1) {
              state.sceneTransformJob = null;
            }
          }
        }
      }
      if (state.titleJob) {
        state.titleJob.remaining -= 1;
        if (state.titleJob.remaining <= 0) state.titleJob = null;
      }
      Object.keys(state.spriteEffects).forEach(function(slot) {
        state.spriteEffects[slot].poseFrame += 2;
        state.spriteEffects[slot].payload.poseFrame = state.spriteEffects[slot].poseFrame;
      });
      Object.keys(state.actors).forEach(function(slot) {
        state.actors[slot].poseFrame += 2;
      });
      Object.keys(state.dialogues).forEach(function(windowId) {
        var window = state.dialogues[windowId];
        if (!window.closed && state.tick >= window.readyTick) window.paused = true;
      });
    }

    function updateJobs() {
      updateMovementJobs();
      updateTurnJobs();
      updateProjectionJob();
      updateTintJobs();
      updateColorJobs();
      updateOtherJobs();
      if (state.registeredCounter) state.registeredCounter.value += 1;
    }

    function compare(actual, mode, target) {
      if (mode === 0) return actual === target;
      if (mode === 1) return actual !== target;
      if (mode === 2) return actual >= target;
      if (mode === 3) return actual <= target;
      if (mode === 4) return actual > target;
      if (mode === 5) return actual < target;
      return false;
    }

    function queryActual(query) {
      var input = query.query && query.query.producerInput;
      if (query.name === 'registered_counter_query' ||
          query.name === 'a_button_skippable_registered_wait_query') {
        return state.registeredCounter ? state.registeredCounter.value : 0;
      }
      if (query.name === 'actor_movement_countdown_query') {
        return state.movementJobs[input] ? lowS16(state.movementJobs[input].remaining) : 0;
      }
      if (query.name === 'actor_facing_turn_activity_query') return state.turnJobs[input] ? 1 : 0;
      if (query.name === 'dialogue_pause_query') {
        var window = state.dialogues[input];
        return window && !window.closed && window.paused ? query.query.target :
          (query.query.target === 0 ? 1 : 0);
      }
      if (query.name === 'scene_projection_transform_countdown_query_mode2' ||
          query.name === 'scene_projection_transform_countdown_query_unguarded') {
        return state.projectionJob ? state.projectionJob.remaining : 0;
      }
      if (query.name === 'color_overlay_countdown_query') {
        return state.overlayJob ? state.overlayJob.remaining : 0;
      }
      if (query.name === 'actor_state_pose_opcode_query') {
        var actor = state.actors[input];
        return actor && state.tick < actor.poseReadyTick ? 1 : query.query.target;
      }
      if (query.name === 'scene_transform_sequence_query') {
        return state.sceneTransformJob ? 0 : query.query.target;
      }
      if (query.name === 'prologue_title_reveal_query') {
        return state.titleJob ? 1 : query.query.target;
      }
      assumption('External query input for ' + query.label + ' was set to its passing value.');
      return query.query.target;
    }

    function queryPasses(query) {
      if (!query || !query.query) return true;
      return compare(queryActual(query), query.query.compareMode, query.query.target);
    }

    function jobActiveFor(start) {
      var slot = start.operands.length ? start.operands[0].signed : null;
      if (start.name === 'actor_move') return !!state.movementJobs[slot];
      if (start.name === 'actor_facing_turn_transition') return !!state.turnJobs[slot];
      if (start.name === 'actor_rgb_tint_transition') {
        return slot === -1 ? Object.keys(state.tintJobs).length > 0 : !!state.tintJobs[slot];
      }
      if (start.name === 'actor_body_pose_program_start') return !!state.bodyPoseJobs[slot];
      if (start.name === 'scene_transform_sequence_start') return !!state.sceneTransformJob;
      if (/scene_projection_transform/.test(start.name)) return !!state.projectionJob;
      if (start.name === 'full_screen_color_overlay_fade') return !!state.overlayJob;
      return false;
    }

    function snapshot(block) {
      var background = M.cloneJson(state.background, 'runtime background');
      if (state.directorMode === 0 && Array.isArray(background.layers)) {
        background.layers = background.layers.map(function(layer, index) {
          var nativeOrdinal = Number.isFinite(layer.nativeOrdinal)
            ? layer.nativeOrdinal
            : (layer.source && Number.isFinite(layer.source.traversalOrdinal)
              ? layer.source.traversalOrdinal
              : (Number.isFinite(layer.depth) ? layer.depth : index));
          var channel = state.transformChannels[nativeOrdinal] ||
            identityTransformChannel();
          layer.nativeOrdinal = nativeOrdinal;
          layer.transformChannel = nativeOrdinal;
          layer.sceneTransform = Object.assign({}, channel);
          layer.renderPipeline = 'mode-zero-b5-actor-camera';
          return layer;
        });
      }
      var actors = Object.keys(state.actors).map(function(slot) {
        var actor = state.actors[slot];
        var channel = state.transformChannels[actor.transformChannel] ||
          identityTransformChannel();
        var modeZeroStage = state.directorMode === 0;
        var renderedY = actor.heightModeByte & 0x04
          ? actor.y + actor.secondaryY
          : (actor.heightModeByte & 0x02 ? actor.secondaryY : actor.y);
        return {
          id: actor.id,
          label: actor.label,
          slot: actor.slot,
          artSourceId: actor.artSourceId,
          capability: actor.capability,
          visible: actor.visible,
          opacityByte: actor.opacityByte,
          renderModeByte: actor.renderModeByte,
          x: modeZeroStage ? actor.x : actor.x + channel.translateX,
          y: modeZeroStage ? renderedY : renderedY + channel.translateY,
          z: modeZeroStage ? actor.z : actor.z + channel.translateZ,
          baseX: actor.x,
          baseY: actor.y,
          baseZ: actor.z,
          secondaryY: actor.secondaryY,
          heightModeByte: actor.heightModeByte,
          facing: actor.facing,
          poseId: actor.poseId,
          bank: actor.bank,
          animationKey: actor.animationKey,
          nativeFacing: actor.nativeFacing,
          variantSelector: actor.variantSelector,
          poseFrame: actor.poseFrame,
          poseLoop: actor.poseLoop,
          poseDuration: actor.poseDuration,
          bodyPoseProgram: actor.bodyPoseProgram
            ? M.cloneJson(actor.bodyPoseProgram, 'actor.bodyPoseProgram') : null,
          movementFrame: actor.movementFrame,
          activeMovementId: actor.activeMovementId,
          uniformScale: actor.uniformScale * channel.uniformScale,
          nativeUniformScale: actor.uniformScale,
          tint: Object.assign({}, actor.tint),
          yawDegrees: actor.yawDegrees + channel.rotationY,
          pitchDegrees: channel.rotationX,
          transformChannel: actor.transformChannel,
          sceneTransform: Object.assign({}, channel),
          renderPipeline: modeZeroStage ? 'mode-zero-two-camera' :
            'actor-camera-direct',
          source: actor.source
        };
      }).sort(function(left, right) { return left.z - right.z || left.slot - right.slot; });
      var dialogue = Object.keys(state.dialogues).map(function(windowId) {
        var window = state.dialogues[windowId];
        if (window.closed) return null;
        var entry = window.entry;
        return {
          id: 'runtime-dialogue:' + window.sourceNodeId,
          kind: 'dialogue',
          trackId: 'track:runtime:dialogue',
          label: 'Dialogue window ' + window.windowId,
          startFrame: 0,
          durationFrames: 1,
          capability: M.capabilities.PREVIEW_ONLY,
          payload: {
            sourceSystem: 'serifu-runtime',
            dialogueArchiveId: window.archive && window.archive.archiveId || null,
            dialogueEntryId: entry && entry.entryId || null,
            presentationArchiveSelector: window.selector,
            presentationEntrySelector: window.entrySelector,
            speaker: entry && (entry.speakerLabel ||
              (entry.speakerId == null ? 'Narrator' : 'Speaker ' + entry.speakerId)),
            text: window.segments[window.segmentIndex],
            rawText: entry && entry.rawText || '',
            paused: window.paused,
            ownerActorSlot: window.ownerActorSlot,
            layout: window.layout
          }
        };
      }).filter(Boolean);
      var effects = Object.keys(state.spriteEffects).map(function(slot) {
        return M.cloneJson(state.spriteEffects[slot], 'runtime sprite effect');
      }).concat(state.effectEvents);
      var cameraProjection = projectionFromCamera(state.cameras.actor);
      var registeredProjection = projectionFromCamera(state.cameras.registered);
      return {
        frame: state.tick,
        timeSeconds: state.tick / M.previewFps,
        pathId: 'default',
        background: background,
        actors: actors,
        dialogue: dialogue,
        audio: state.audioEvents.slice(),
        camera: state.cameraEvents.slice(),
        cameraState: Object.assign({}, state.projectionTransform, {
          activeClipId: state.projectionJob && state.projectionJob.nodeId || null,
          timingStatus: state.projectionJob
            ? 'native projection updater invocation' : 'native projection state'
        }),
        actorProjection: cameraProjection,
        registeredProjection: registeredProjection,
        effects: effects,
        flow: block ? [{
          id: 'runtime-flow:' + state.tick,
          kind: 'wait',
          trackId: 'track:runtime:flow',
          label: block.label,
          startFrame: state.tick,
          durationFrames: 1,
          capability: M.capabilities.PREVIEW_ONLY,
          payload: { nativeClock: block.clock || 'director-evaluation' }
        }] : state.flowEvents.slice(),
        overlays: state.overlay ? [Object.assign({}, state.overlay)] : [],
        sceneColor: Object.assign({}, state.sceneColor),
        transformChannels: state.transformChannels.map(function(channel) {
          return Object.assign({}, channel);
        }),
        runtime: {
          engine: 'director-scheduler',
          directorMode: state.directorMode,
          directorModeStatus: state.directorModeStatus,
          tick: state.tick,
          compositeIndex: compositeIndex,
          blockKind: block && block.kind || null,
          blockLabel: block && block.label || null,
          assumptionCount: assumptions.length,
          missingInputCount: missingInputs.length,
          status: missingInputs.length ? 'missing-inputs' :
            (assumptions.length ? 'assumed-inputs' : 'profiled')
        }
      };
    }

    var compositeIndex = 0;
    var block = null;

    function beginTick(tick) {
      state.tick = tick;
      state.audioEvents = [];
      state.cameraEvents = [];
      state.effectEvents = [];
      state.flowEvents = [];
      if (tick > 0) updateJobs();
      var scheduled = state.scheduled.filter(function(item) { return item.tick === tick; });
      state.scheduled = state.scheduled.filter(function(item) { return item.tick !== tick; });
      scheduled.forEach(function(item) { executePrimitive(item.node); });
    }

    function blockComplete(activeBlock) {
      if (!activeBlock) return true;
      if (activeBlock.kind === 'until') return state.tick >= activeBlock.untilTick;
      if (activeBlock.kind === 'job') return !jobActiveFor(activeBlock.start);
      if (activeBlock.kind === 'query') return queryPasses(activeBlock.query);
      return true;
    }

    function processComposite(composite) {
      var nodes = composite.nodeIds.map(function(id) { return program.primitiveById[id]; });
      var first = nodes[0];
      trace.push({
        tick: state.tick,
        kind: 'composite',
        compositeId: composite.id,
        compositeKind: composite.kind,
        label: composite.label,
        category: composite.category
      });
      if (composite.kind === 'registered-wait') {
        state.registeredCounter = { value: 1, armTick: state.tick };
        block = { kind: 'until', untilTick: state.tick + Math.max(0, composite.nativeTicks),
          label: composite.label, clock: composite.clock };
        return;
      }
      if (composite.kind === 'skippable-registered-wait') {
        state.registeredCounter = { value: 1, armTick: state.tick };
        assumption('A-button input is not supplied; skippable waits use their authored maximum.');
        var details = composite.details || {};
        if (details.shape === 'staged-actor-action') {
          (details.actionNodeIds || []).forEach(function(nodeId, index) {
            state.scheduled.push({
              tick: state.tick + Math.max(0, details.openingTargets[index] || 0),
              node: program.primitiveById[nodeId]
            });
          });
        }
        block = { kind: 'until', untilTick: state.tick + Math.max(0, composite.nativeTicks),
          label: composite.label, clock: composite.clock };
        return;
      }
      if (composite.kind === 'start-and-completion-gate') {
        executePrimitive(first);
        block = { kind: 'job', start: first, query: nodes[nodes.length - 1],
          label: composite.label, clock: composite.clock };
        if (blockComplete(block)) block = null;
        return;
      }
      if (composite.kind === 'dialogue-window-open') {
        nodes.forEach(function(node) {
          if (node.name === 'dialogue_window_create' ||
              node.name === 'text_display_speed_override') executePrimitive(node);
        });
        var dialogueQuery = nodes[nodes.length - 1];
        block = { kind: 'query', query: dialogueQuery,
          label: composite.label, clock: composite.clock };
        if (blockComplete(block)) block = null;
        return;
      }
      if (composite.kind === 'dialogue-window-resume-close') {
        nodes.forEach(executePrimitive);
        return;
      }
      if (composite.kind === 'query-envelope') {
        var query = nodes[nodes.length - 1];
        block = { kind: 'query', query: query, label: composite.label, clock: composite.clock };
        if (blockComplete(block)) block = null;
        return;
      }
      nodes.forEach(executePrimitive);
    }

    for (var tick = 0; tick < maxTicks; tick++) {
      beginTick(tick);
      if (block && blockComplete(block)) {
        if (block.kind === 'until') state.registeredCounter = null;
        block = null;
      }
      var instantGuard = 0;
      while (!block && !state.terminal && compositeIndex < program.composites.length) {
        if (++instantGuard > program.composites.length + 8) {
          fail('Director runtime exceeded its instantaneous-dispatch guard.', 'dispatch-loop');
        }
        var composite = program.composites[compositeIndex++];
        processComposite(composite);
      }
      states.push(snapshot(block));
      if (state.terminal || compositeIndex >= program.composites.length && !block &&
          !Object.keys(state.movementJobs).length && !state.projectionJob) break;
    }

    if (!state.terminal && states.length >= maxTicks) {
      missing('Director preview reached the ' + maxTicks + '-tick safety limit.');
    }
    if (!states.length) states.push(snapshot(null));
    states.forEach(function(frameState) {
      frameState.durationFrames = states.length;
      frameState.runtime.assumptionCount = assumptions.length;
      frameState.runtime.missingInputCount = missingInputs.length;
      frameState.runtime.status = missingInputs.length ? 'missing-inputs' :
        (assumptions.length ? 'assumed-inputs' : 'profiled');
    });

    return {
      assetId: scene.assetId,
      engine: 'director-scheduler',
      directorMode: state.directorMode,
      directorModeStatus: state.directorModeStatus,
      clockUnit: 'native scheduler update',
      durationTicks: states.length,
      states: states,
      assumptions: assumptions,
      missingInputs: missingInputs,
      trace: trace,
      executedNodeIds: state.executedNodeIds.slice(),
      sourcePrimitiveCount: program.primitives.length,
      executedPrimitiveCount: state.executedNodeIds.length,
      terminated: state.terminal,
      safetyLimited: states.length >= maxTicks && !state.terminal
    };
  }

  function bind(document, runtime) {
    if (!bindings) fail('This environment cannot bind Director runtime state.', 'weak-map');
    if (!document || !runtime) fail('A SceneDocument and runtime are required.', 'binding');
    bindings.set(document, runtime);
    return runtime;
  }

  function unbind(document) { if (bindings && document) bindings.delete(document); }
  function forDocument(document) { return bindings && document ? bindings.get(document) || null : null; }

  function evaluate(runtime, requestedTick) {
    if (!runtime || !Array.isArray(runtime.states) || !runtime.states.length) {
      fail('Compiled Director runtime state is unavailable.', 'invalid-runtime');
    }
    if (!Number.isInteger(requestedTick)) fail('Director tick must be an integer.', 'invalid-tick');
    var tick = clamp(requestedTick, 0, runtime.states.length - 1);
    var output = M.cloneJson(runtime.states[tick], 'runtime state');
    output.frame = tick;
    output.timeSeconds = tick / M.previewFps;
    output.durationFrames = runtime.states.length;
    return output;
  }

  OB64.cutsceneRuntime = Object.freeze({
    RuntimeError: RuntimeError,
    defaultMaxTicks: DEFAULT_MAX_TICKS,
    compile: compile,
    bind: bind,
    unbind: unbind,
    forDocument: forDocument,
    evaluate: evaluate,
    projectionFromCamera: projectionFromCamera,
    decodeSceneTransformResource: decodeSceneTransformResource
  });
})(window.OB64);
