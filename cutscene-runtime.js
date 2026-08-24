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
  function lowS8(value) {
    value = unsigned(value) & 0xFF;
    return value & 0x80 ? value - 0x100 : value;
  }
  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }
  function mix(left, right, amount) { return left + (right - left) * amount; }
  function finite(value, fallback) { return Number.isFinite(value) ? value : fallback; }
  function fixed(value) { return signed(value) / 1000; }
  function launchTranslationIndex(value) {
    value = unsigned(value);
    return (value & 0xFFFFFF00) === 0x08880000 ? value & 0xFF : null;
  }
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
      evidenceStatus: 'external-unresolved',
      status: 'unresolved launch camera fallback'
    };
  }

  function modeZeroCamera(bank) {
    var camera = defaultCamera();
    camera.eye = bank === 'actor'
      ? { x: 360, y: 0, z: 0 }
      : { x: 0, y: 0, z: 360 };
    camera.sourceNodeId = 'mode-zero-camera-initializer';
    camera.evidenceStatus = 'native-static';
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
    camera.evidenceStatus = projection.evidenceStatus || 'runtime-observed';
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
      var projectedCamera = cameraFromProjection(profile.projection, profile.status,
        'launch-profile:' + profile.kind);
      projectedCamera.evidenceStatus = profile.evidenceStatus ||
        projectedCamera.evidenceStatus;
      return projectedCamera;
    }
    var camera = defaultCamera();
    camera.evidenceStatus = profile.evidenceStatus || 'external-unresolved';
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
      sourceNodeId: camera.sourceNodeId || null,
      evidenceStatus: camera.evidenceStatus || 'external-unresolved'
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

  function modeTwoStageProps(catalog, foregroundSelector) {
    return catalog && Number.isInteger(foregroundSelector) &&
      typeof catalog.getModeTwoStagePlacementProfile === 'function'
      ? catalog.getModeTwoStagePlacementProfile(foregroundSelector) : null;
  }

  function hasRenderableStageProps(stageProps) {
    return !!(stageProps && ((stageProps.orthographicPlacements || []).length ||
      (stageProps.perspectivePlacements || []).length));
  }

  function stagedBackground(stageLayers, document, metadata) {
    metadata = metadata || {};
    stageLayers = Array.isArray(stageLayers) ? stageLayers : [];
    if (!stageLayers.length && !hasRenderableStageProps(metadata.nativeSceneProps)) return null;
    var layers = stageLayers.map(function(layer, index) {
      var role = layer.role || (index ? 'ordered-layer' : 'environment-base');
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: layer.assetId,
        label: layer.assetId,
        visible: true,
        depth: Number.isFinite(layer.depth) ? layer.depth : index,
        nativeOrdinal: Number.isFinite(layer.nativeOrdinal)
          ? layer.nativeOrdinal : (Number.isFinite(layer.depth) ? layer.depth : index),
        role: role,
        capability: M.capabilities.PREVIEW_ONLY,
        source: {
          sourceKind: metadata.sourceKind || 'staged-background',
          evidenceStatus: layer.evidenceStatus,
          associationStatus: layer.associationStatus,
          traversalOrdinal: Number.isFinite(layer.nativeOrdinal)
            ? layer.nativeOrdinal : (Number.isFinite(layer.depth) ? layer.depth : index)
        }
      };
    });
    var projection = M.cloneJson(document.background.projection || {},
      'background.projection');
    if (metadata.nativeSceneProps) {
      projection.nativeSceneProps = M.cloneJson(metadata.nativeSceneProps,
        'mode-two native Stage placements');
    }
    return {
      assetId: layers.length ? layers[0].assetId : null,
      layers: layers,
      capability: M.capabilities.PREVIEW_ONLY,
      projection: projection,
      runtimeStatus: metadata.runtimeStatus || 'The launch profile supplies a complete Stage.',
      selectorTableId: metadata.selectorTableId || null,
      selector: Number.isInteger(metadata.selector) ? metadata.selector : null,
      environmentSelector: Number.isInteger(metadata.environmentSelector)
        ? metadata.environmentSelector : null,
      foregroundSelectorTableId: metadata.foregroundSelectorTableId || null,
      foregroundSelector: Number.isInteger(metadata.foregroundSelector)
        ? metadata.foregroundSelector : null,
      foregroundSelectorCandidates: Array.isArray(metadata.foregroundSelectorCandidates)
        ? metadata.foregroundSelectorCandidates.slice() : [],
      foregroundStatus: metadata.foregroundStatus || null
    };
  }

  function observationBackground(scene, document, catalog) {
    var observation = scene && scene.backgroundRuntimeObservation;
    if (!observation) return null;
    return stagedBackground(observation.stageLayers, document, {
      sourceKind: 'runtime-observation',
      runtimeStatus: observation.associationStatus,
      selectorTableId: observation.selectorTableId,
      selector: Number.isInteger(observation.environmentSelector)
        ? observation.environmentSelector : observation.commandOperand,
      environmentSelector: observation.environmentSelector,
      foregroundSelectorTableId: observation.directorMode === 2
        ? 'background-table:mode2-overlay:80' : null,
      foregroundSelector: observation.foregroundSelector,
      nativeSceneProps: observation.directorMode === 2
        ? modeTwoStageProps(catalog, observation.foregroundSelector) : null
    });
  }

  function profileBackground(request, document, catalog) {
    if (!request) return null;
    return stagedBackground(request.stageLayers, document, {
      sourceKind: request.sourceKind === 'parent-event-predecessor'
        ? 'parent-event-predecessor' : 'launch-profile',
      runtimeStatus: request.status,
      selectorTableId: request.selectorTableId,
      selector: request.selector,
      environmentSelector: request.environmentSelector,
      foregroundSelectorTableId: request.foregroundSelectorTableId,
      foregroundSelector: request.foregroundSelector,
      foregroundSelectorCandidates: request.foregroundSelectorCandidates,
      foregroundStatus: request.foregroundStatus,
      nativeSceneProps: modeTwoStageProps(catalog, request.foregroundSelector)
    });
  }

  function documentModeTwoBackground(document, catalog) {
    var projection = document && document.background && document.background.projection || {};
    var context = projection.launchContext;
    if (!context || context.override !== true || context.mode !== 2) return null;

    var environmentTableId = 'background-table:mode2-environment:80';
    var foregroundTableId = 'background-table:mode2-overlay:80';
    var environmentSelector = Number.isInteger(context.environmentSelector)
      ? context.environmentSelector : null;
    var foregroundSelector = Number.isInteger(context.foregroundSelector)
      ? context.foregroundSelector : null;
    var environmentEntry = catalog && environmentSelector !== null
      ? catalog.getBackgroundSelectorEntry(environmentTableId, environmentSelector) : null;
    var foregroundEntry = catalog && foregroundSelector !== null
      ? catalog.getBackgroundSelectorEntry(foregroundTableId, foregroundSelector) : null;
    var nativeSceneProps = modeTwoStageProps(catalog, foregroundSelector);
    var environmentStage = stagedBackground(
      environmentEntry && environmentEntry.stageLayers || [], document, {
        sourceKind: 'document-launch-context',
        selectorTableId: environmentTableId,
        selector: environmentSelector,
        environmentSelector: environmentSelector,
        foregroundSelectorTableId: foregroundTableId,
        foregroundSelector: foregroundSelector,
        nativeSceneProps: nativeSceneProps
      });
    var layers = environmentStage ? environmentStage.layers : [];
    var foregroundLayers = backgroundLayers(
      foregroundEntry, M.capabilities.PREVIEW_ONLY, catalog).map(function(layer, index) {
        layer.id = 'background:foreground:' + index;
        layer.role = 'foreground-mask';
        layer.depth = 100 + index;
        layer.nativeOrdinal = index;
        layer.source.sourceKind = 'document-launch-context-foreground';
        layer.source.role = 'foreground-mask';
        return layer;
      });
    layers = layers.concat(foregroundLayers);

    var issues = [];
    if (!environmentEntry || !layers.some(function(layer) {
      return layer.role !== 'foreground-mask';
    })) {
      issues.push(environmentSelector === null
        ? 'Mode-two launch environment selector is unresolved.'
        : 'Mode-two launch environment selector ' + environmentSelector +
          ' has no complete renderable Stage.');
    }
    if (foregroundSelector === null || !foregroundEntry) {
      issues.push('Mode-two launch foreground selector is unresolved.');
    }
    var runtimeProjection = M.cloneJson(projection, 'background.projection');
    runtimeProjection.mode = layers.length || hasRenderableStageProps(nativeSceneProps)
      ? 'stage-fit' : 'unresolved';
    runtimeProjection.evidenceStatus = 'user-supplied-launch-context';
    if (nativeSceneProps) {
      runtimeProjection.nativeSceneProps = M.cloneJson(nativeSceneProps,
        'document mode-two native Stage placements');
    }
    return {
      background: {
        assetId: environmentStage && environmentStage.assetId || null,
        layers: layers,
        capability: M.capabilities.PREVIEW_ONLY,
        projection: runtimeProjection,
        runtimeStatus: 'Document launch context selects mode-two environment ' +
          (environmentSelector === null ? 'unresolved' : environmentSelector) +
          ' and foreground ' +
          (foregroundSelector === null ? 'unresolved' : foregroundSelector) +
          '. Director bytes are unchanged.',
        selectorTableId: environmentTableId,
        selector: environmentSelector,
        environmentSelector: environmentSelector,
        foregroundSelectorTableId: foregroundTableId,
        foregroundSelector: foregroundSelector
      },
      issues: issues
    };
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
    var rootProgram = program;
    var activeProgram = rootProgram;
    var activeStreamAssetId = scene.assetId;
    var compositeIndexById = {};
    var directorLabelByMarker = {};
    var primitiveIndexById = {};
    var continuationProgramCache = {};

    function indexActiveProgram() {
      compositeIndexById = {};
      directorLabelByMarker = {};
      primitiveIndexById = {};
      activeProgram.composites.forEach(function(composite, index) {
        compositeIndexById[composite.id] = index;
      });
      activeProgram.primitives.forEach(function(node, index) {
        primitiveIndexById[node.id] = index;
        if (node.name !== 'director_label_marker' || !node.operands.length) return;
        var marker = node.operands[0].signed;
        if (directorLabelByMarker[marker]) return;
        var compositeId = activeProgram.compositeByNodeId[node.id];
        if (!Number.isInteger(compositeIndexById[compositeId])) return;
        directorLabelByMarker[marker] = {
          marker: marker,
          nodeId: node.id,
          startWord: node.startWord,
          primitiveIndex: index,
          compositeIndex: compositeIndexById[compositeId]
        };
      });
    }

    function continuationProgram(selector) {
      if (continuationProgramCache[selector]) return continuationProgramCache[selector];
      var entry = catalog && catalog.getDirectorContinuationStream
        ? catalog.getDirectorContinuationStream(selector) : null;
      if (!entry) return null;
      if (!(options.z64 instanceof Uint8Array) || !OB64.art ||
          typeof OB64.art.readCompressedResource !== 'function' ||
          !OB64.cutsceneCodec || typeof OB64.cutsceneCodec.createIr !== 'function') {
        return null;
      }
      var decoded = OB64.art.readCompressedResource(
        options.z64, entry.resourceKey).decoded;
      if (decoded.length !== entry.decodedLength) {
        fail('Director continuation ' + selector +
          ' no longer matches its generated decoded length.', 'continuation-source');
      }
      var continuationScene = {
        assetId: entry.assetId,
        source: {
          dynamicGrammar: true,
          terminalWithoutTrailer: true,
          decodedLength: entry.decodedLength,
          decodedWordCount: entry.decodedWordCount,
          runtimeNodeCount: entry.runtimeNodeCount
        }
      };
      var ir = OB64.cutsceneCodec.createIr(continuationScene, decoded);
      continuationProgramCache[selector] = ir.program;
      return ir.program;
    }

    indexActiveProgram();

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
    var translationProfile = launchProfile.operandTranslation || {
      required: false, tableIndexes: []
    };
    var suppliedTranslationTable = options.launchOperandTranslations || {};
    var suppliedTranslationIndexes = [];
    var missingTranslationIndexes = [];
    (translationProfile.tableIndexes || []).forEach(function(tableIndex) {
      var supplied = Object.prototype.hasOwnProperty.call(
        suppliedTranslationTable, String(tableIndex));
      if (!supplied) {
        missingTranslationIndexes.push(tableIndex);
        return;
      }
      var value = suppliedTranslationTable[tableIndex];
      if (!Number.isInteger(value) || value < 0 || value > 0xFFFF) {
        fail('Launch operand translation ' + tableIndex +
          ' must be an unsigned halfword.', 'launch-translation');
      }
      suppliedTranslationIndexes.push(tableIndex);
    });
    if (missingTranslationIndexes.length) {
      missing('The native Director loader requires launch operand translation table ' +
        (missingTranslationIndexes.length === 1 ? 'index ' : 'indexes ') +
        missingTranslationIndexes.join(', ') +
        '; commands using unresolved values remain byte-preserved and are withheld from preview state.');
    }

    function executionWords(node) {
      var words = node.rawWords.map(unsigned);
      var unresolvedWordOffsets = [];
      var translatedWordOffsets = [];
      words.forEach(function(word, wordOffset) {
        var tableIndex = launchTranslationIndex(word);
        if (tableIndex === null) return;
        if (Object.prototype.hasOwnProperty.call(
            suppliedTranslationTable, String(tableIndex))) {
          words[wordOffset] = suppliedTranslationTable[tableIndex];
          translatedWordOffsets.push(wordOffset);
        } else {
          unresolvedWordOffsets.push(wordOffset);
        }
      });
      return {
        words: words,
        unresolvedWordOffsets: unresolvedWordOffsets,
        translatedWordOffsets: translatedWordOffsets
      };
    }
    var observedBackground = observationBackground(scene, document, catalog);
    var documentBackground = documentModeTwoBackground(document, catalog);
    var directorMode = directorModeFromProfile(scene);
    var modeTwoCommandPreviewUsesFreshRoot = directorMode.value === 2 &&
      launchProfile.background && launchProfile.background.requestCount > 0;
    var suppliedContextRuntime = options.contextRuntime && (
      Array.isArray(options.contextRuntime.states) && options.contextRuntime.states.length ||
      Array.isArray(options.contextRuntime.contextFrames) &&
        options.contextRuntime.contextFrames.length)
      ? options.contextRuntime : null;
    var contextRuntime = modeTwoCommandPreviewUsesFreshRoot
      ? null : suppliedContextRuntime;
    if (modeTwoCommandPreviewUsesFreshRoot && suppliedContextRuntime) {
      assumption('The event route preserves resource-loader mode 0x8023A981, but its launch value is not statically known; this explicit mode-two background preview uses the native zero-mode fresh-root branch.');
    }
    var contextFrameCount = !contextRuntime ? 0 :
      (Array.isArray(contextRuntime.states)
        ? contextRuntime.states.length : contextRuntime.contextFrames.length);
    var contextTickOffset = Number.isInteger(options.contextTickOffset)
      ? options.contextTickOffset : 1;
    var launchInvocationContexts = (launchProfile.parentEventLaunches || [])
      .reduce(function(rows, launch) {
        return rows.concat(launch.eventInvocationContexts || []);
      }, []);
    var selectedLaunchContext = options.launchContext || null;
    var selectedContextOwner = selectedLaunchContext &&
      selectedLaunchContext.concurrentDirectorAssetId || null;
    var everyLaunchNeedsContext = !modeTwoCommandPreviewUsesFreshRoot &&
      launchInvocationContexts.length > 0 &&
      launchInvocationContexts.every(function(context) {
        return !!context.concurrentDirectorAssetId;
      });
    if (!contextRuntime && !modeTwoCommandPreviewUsesFreshRoot &&
        (selectedContextOwner || everyLaunchNeedsContext)) {
      var contextOwners = Array.from(new Set(launchInvocationContexts.map(function(context) {
        return context.concurrentDirectorAssetId;
      }).filter(Boolean)));
      missing('The selected parent-event invocation requires concurrent Director scene state' +
        (contextOwners.length ? ' from ' + contextOwners.join(' or ') : '') +
        '; a standalone stream cannot supply its shared Actors, dialogue, or transforms.');
    }
    var priorContextState = null;
    var priorContextFrameIndex = -1;
    var registeredCamera = cameraFromLaunchProfile(
      launchProfile.cameras.registered, 'registered');
    var actorCamera = cameraFromLaunchProfile(launchProfile.cameras.actor, 'actor');
    var initialStageTransform = M.cloneJson(
      launchProfile.stageTransform.initial, 'launch Stage transform');
    var directorSelectorRows = scene && scene.source &&
      Array.isArray(scene.source.directorSelectorRows)
      ? scene.source.directorSelectorRows.filter(Number.isInteger) : [];
    var screenTransitionVariant = Number.isInteger(options.directorSelector)
      ? options.directorSelector & 0x3FFF
      : (directorSelectorRows.length && directorSelectorRows.every(function(selector) {
        return selector === 0;
      }) ? 0 : (directorSelectorRows.length && directorSelectorRows.every(function(selector) {
        return selector !== 0;
      }) ? directorSelectorRows[0] : null));
    if (directorMode.evidenceStatus === 'external-unresolved') {
      missing('The launch profile cannot identify this stream\'s Director mode.');
    }
    if (launchProfile.cameras.actor.evidenceStatus === 'external-unresolved') {
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
      screenTransition: null,
      actorPresentationJob: null,
      overlayJob: null,
      sceneColorJob: null,
      sceneTransformJob: null,
      oversizedImageTransitionJob: null,
      sceneVignette: null,
      oversizedImageView: {
        x: 0,
        y: 0,
        scale: 1,
        zoomState: 4
      },
      titleJob: null,
      transientRenderEntities: {},
      armyManagementCursorLatch: 0,
      registeredCounter: null,
      transformDivider: null,
      transformChannels: identityTransformChannels(),
      directorMode: directorMode.value,
      directorModeStatus: directorMode.status,
      cameras: { registered: registeredCamera, actor: actorCamera },
      projectionTransform: initialStageTransform,
      background: documentBackground ? documentBackground.background :
        (observedBackground || M.cloneJson(document.background, 'background')),
      dialogues: {},
      spriteEffects: {},
      audioEvents: [],
      cameraEvents: [],
      effectEvents: [],
      flowEvents: [],
      scheduled: [],
      terminal: false,
      terminalReason: null,
      presentationLifecycleRequest: 0,
      alternateDirectorScheduling: null,
      textSpeed: 512,
      sceneColor: { red: 255, green: 255, blue: 255 },
      overlay: null,
      executedNodeIds: []
    };

    if (documentBackground) {
      trace.push({ tick: 0, kind: 'runtime-input', label: 'Document mode-two launch context' });
      documentBackground.issues.forEach(missing);
    } else if (observedBackground) {
      trace.push({ tick: 0, kind: 'runtime-input', label: 'Observed background route' });
    }
    if (contextRuntime) {
      trace.push({
        tick: 0,
        kind: 'runtime-input',
        label: 'Parent-event concurrent Director context',
        contextAssetId: contextRuntime.assetId,
        contextTickOffset: contextTickOffset
      });
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
      actor.poseProgramStatus = !poseProgram ? 'unresolved' :
        (Array.isArray(poseProgram.frames) && poseProgram.frames.length
          ? 'native-program' : 'empty-native-program');
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
      missing(purpose + ' for slot ' + slot +
        ' requires a launch-time Actor record that is not stored in the Director stream.');
      return null;
    }

    function sameContextValue(left, right) {
      return JSON.stringify(left) === JSON.stringify(right);
    }

    function contextActorMap(frameState) {
      var output = {};
      (frameState && frameState.actors || []).forEach(function(actor) {
        output[actor.slot] = actor;
      });
      return output;
    }

    function applyContextActor(actorRow, priorRow) {
      var actor = state.actors[actorRow.slot] || ensureActor(actorRow.slot);
      [
        ['id', 'id'], ['label', 'label'], ['artSourceId', 'artSourceId'],
        ['capability', 'capability'], ['visible', 'visible'],
        ['opacityByte', 'opacityByte'], ['renderModeByte', 'renderModeByte'],
        ['baseX', 'x'], ['baseY', 'y'], ['baseZ', 'z'],
        ['secondaryY', 'secondaryY'], ['heightModeByte', 'heightModeByte'],
        ['facing', 'facing'], ['poseId', 'poseId'], ['bank', 'bank'],
        ['animationKey', 'animationKey'], ['nativeFacing', 'nativeFacing'],
        ['variantSelector', 'variantSelector'], ['poseFrame', 'poseFrame'],
        ['poseProgramStatus', 'poseProgramStatus'], ['poseLoop', 'poseLoop'],
        ['poseDuration', 'poseDuration'], ['bodyPoseProgram', 'bodyPoseProgram'],
        ['movementFrame', 'movementFrame'], ['activeMovementId', 'activeMovementId'],
        ['nativeUniformScale', 'uniformScale'], ['tint', 'tint'],
        ['yawDegrees', 'yawDegrees'], ['transformChannel', 'transformChannel'],
        ['source', 'source']
      ].forEach(function(pair) {
        var sourceField = pair[0];
        var targetField = pair[1];
        if (!Object.prototype.hasOwnProperty.call(actorRow, sourceField)) return;
        if (priorRow && sameContextValue(actorRow[sourceField], priorRow[sourceField])) return;
        var value = actorRow[sourceField];
        actor[targetField] = value && typeof value === 'object'
          ? M.cloneJson(value, 'context actor ' + sourceField) : value;
      });
      actor.contextSourceAssetId = contextRuntime.assetId;
    }

    function contextDialogueMap(frameState) {
      var output = {};
      (frameState && frameState.dialogue || []).forEach(function(row) {
        var payload = row.payload || {};
        var labelMatch = String(row.label || '').match(/(\d+)$/);
        var windowId = Number.isInteger(payload.windowId)
          ? payload.windowId : Number(labelMatch && labelMatch[1]);
        if (Number.isInteger(windowId)) output[windowId] = row;
      });
      return output;
    }

    function applyContextDialogue(row, priorRow) {
      var payload = row.payload || {};
      var labelMatch = String(row.label || '').match(/(\d+)$/);
      var windowId = Number.isInteger(payload.windowId)
        ? payload.windowId : Number(labelMatch && labelMatch[1]);
      if (!Number.isInteger(windowId)) return;
      var window = state.dialogues[windowId];
      if (!window) {
        window = state.dialogues[windowId] = {
          windowId: windowId,
          selector: payload.presentationArchiveSelector,
          entrySelector: payload.presentationEntrySelector,
          archive: null,
          entry: null,
          segments: [payload.text || ''],
          segmentIndex: 0,
          paused: payload.paused === true,
          readyTick: Number.MAX_SAFE_INTEGER,
          closed: false,
          ownerActorSlot: payload.ownerActorSlot,
          layout: payload.layout || {},
          speaker: payload.speaker,
          rawText: payload.rawText,
          dialogueArchiveId: payload.dialogueArchiveId,
          dialogueEntryId: payload.dialogueEntryId,
          sourceNodeId: 'context:' + contextRuntime.assetId + ':window:' + windowId,
          contextSourceAssetId: contextRuntime.assetId
        };
        return;
      }
      if (!priorRow || !sameContextValue(payload.paused,
          priorRow.payload && priorRow.payload.paused)) {
        window.paused = payload.paused === true;
      }
      if (!priorRow || !sameContextValue(payload.text,
          priorRow.payload && priorRow.payload.text)) {
        window.segments = [payload.text || ''];
        window.segmentIndex = 0;
      }
      window.selector = payload.presentationArchiveSelector;
      window.entrySelector = payload.presentationEntrySelector;
      window.ownerActorSlot = payload.ownerActorSlot;
      window.layout = payload.layout || {};
      window.speaker = payload.speaker;
      window.rawText = payload.rawText;
      window.dialogueArchiveId = payload.dialogueArchiveId;
      window.dialogueEntryId = payload.dialogueEntryId;
      window.closed = false;
    }

    function applyContextDelta(delta) {
      if (!delta) return;
      (delta.actors || []).forEach(function(actor) {
        applyContextActor(actor, null);
      });
      (delta.removedActorSlots || []).forEach(function(slot) {
        if (state.actors[slot] &&
            state.actors[slot].contextSourceAssetId === contextRuntime.assetId) {
          delete state.actors[slot];
        }
      });
      (delta.dialogue || []).forEach(function(row) {
        applyContextDialogue(row, null);
      });
      (delta.removedDialogueWindowIds || []).forEach(function(windowId) {
        if (state.dialogues[windowId] &&
            state.dialogues[windowId].contextSourceAssetId === contextRuntime.assetId) {
          state.dialogues[windowId].closed = true;
        }
      });
      if (Object.prototype.hasOwnProperty.call(delta, 'background')) {
        state.background = M.cloneJson(delta.background, 'context background');
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'transformChannels')) {
        state.transformChannels = M.cloneJson(
          delta.transformChannels, 'context transform channels');
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'cameraState')) {
        state.projectionTransform = Object.assign({}, delta.cameraState);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'actorProjection')) {
        state.cameras.actor = cameraFromProjection(delta.actorProjection,
          'Concurrent parent-event Director Actor camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'registeredProjection')) {
        state.cameras.registered = cameraFromProjection(delta.registeredProjection,
          'Concurrent parent-event Director registered camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'sceneColor')) {
        state.sceneColor = Object.assign({}, delta.sceneColor);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'overlays')) {
        state.overlay = delta.overlays && delta.overlays.length
          ? M.cloneJson(delta.overlays[0], 'context overlay') : null;
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'sceneVignette')) {
        state.sceneVignette = delta.sceneVignette
          ? M.cloneJson(delta.sceneVignette, 'context scene vignette') : null;
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'oversizedImageView')) {
        state.oversizedImageView = Object.assign({}, delta.oversizedImageView);
      }
    }

    function applyContextTimeline(tick) {
      if (!contextRuntime) return;
      var contextIndex = clamp(tick + contextTickOffset, 0,
        contextFrameCount - 1);
      if (Array.isArray(contextRuntime.contextFrames)) {
        for (var deltaIndex = priorContextFrameIndex + 1;
            deltaIndex <= contextIndex; deltaIndex++) {
          applyContextDelta(contextRuntime.contextFrames[deltaIndex]);
        }
        priorContextFrameIndex = Math.max(priorContextFrameIndex, contextIndex);
        return;
      }
      var frameState = contextRuntime.states[contextIndex];
      var currentActors = contextActorMap(frameState);
      var priorActors = contextActorMap(priorContextState);
      Object.keys(currentActors).forEach(function(slot) {
        applyContextActor(currentActors[slot], priorActors[slot]);
      });

      var currentDialogue = contextDialogueMap(frameState);
      var priorDialogue = contextDialogueMap(priorContextState);
      Object.keys(currentDialogue).forEach(function(windowId) {
        if (priorDialogue[windowId] && sameContextValue(
            currentDialogue[windowId], priorDialogue[windowId])) return;
        applyContextDialogue(currentDialogue[windowId], priorDialogue[windowId]);
      });
      Object.keys(priorDialogue).forEach(function(windowId) {
        if (currentDialogue[windowId]) return;
        var window = state.dialogues[windowId];
        if (window && window.contextSourceAssetId === contextRuntime.assetId) {
          window.closed = true;
        }
      });

      if (!priorContextState ||
          !sameContextValue(frameState.background, priorContextState.background)) {
        state.background = M.cloneJson(frameState.background, 'context background');
      }
      if (!priorContextState ||
          !sameContextValue(frameState.transformChannels, priorContextState.transformChannels)) {
        state.transformChannels = M.cloneJson(
          frameState.transformChannels, 'context transform channels');
      }
      if (!priorContextState ||
          !sameContextValue(frameState.cameraState, priorContextState.cameraState)) {
        state.projectionTransform = Object.assign({}, frameState.cameraState);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.actorProjection, priorContextState.actorProjection)) {
        state.cameras.actor = cameraFromProjection(frameState.actorProjection,
          'Concurrent parent-event Director Actor camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.registeredProjection,
            priorContextState.registeredProjection)) {
        state.cameras.registered = cameraFromProjection(frameState.registeredProjection,
          'Concurrent parent-event Director registered camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.sceneColor, priorContextState.sceneColor)) {
        state.sceneColor = Object.assign({}, frameState.sceneColor);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.overlays, priorContextState.overlays)) {
        state.overlay = frameState.overlays && frameState.overlays.length
          ? M.cloneJson(frameState.overlays[0], 'context overlay') : null;
      }
      if (!priorContextState ||
          !sameContextValue(frameState.sceneVignette, priorContextState.sceneVignette)) {
        state.sceneVignette = frameState.sceneVignette
          ? M.cloneJson(frameState.sceneVignette, 'context scene vignette') : null;
      }
      if (!priorContextState ||
          !sameContextValue(frameState.oversizedImageView,
            priorContextState.oversizedImageView)) {
        state.oversizedImageView = Object.assign({}, frameState.oversizedImageView);
      }
      priorContextState = frameState;
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

    function executeActorCreate(node, words, unresolvedWordOffsets) {
      var slot = signed(words[1]);
      var actor = ensureActor(slot);
      var template = templateForSlot(slot);
      var variantUnresolved = unresolvedWordOffsets.indexOf(9) !== -1;
      var rawSelector = {
        bank: signed(words[2]), key: signed(words[3]), facing: signed(words[4]),
        variant: variantUnresolved && template && template.source &&
          Number.isInteger(template.source.variantSelector)
          ? template.source.variantSelector : unsigned(words[9]) & 0xFF
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
      if (variantUnresolved) {
        actor.source.launchTranslationPreviewFallback = true;
        actor.source.launchTranslationTableIndex = launchTranslationIndex(node.rawWords[9]);
      }
    }

    function executeActorState(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Actor State');
      if (!actor) return;
      var row = rowFor(node, 'pose');
      var payload = row && row.clip.payload || {};
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
      if (identity && lowS16(countWord) === 0) {
        var current = state.projectionTransform;
        var exactIdentity = current.translateX === 0 && current.translateY === 0 &&
          current.scaleX === 1 && current.scaleY === 1;
        if (exactIdentity) return;
        var translationCount = Math.trunc((current.translateX * current.translateX +
          current.translateY * current.translateY) / 80);
        var scaleCount = Math.trunc((Math.min(current.scaleX, current.scaleY) - 1) * 100);
        duration = lowU16(Math.max(lowS16(translationCount), lowS16(scaleCount)));
      }
      if (duration === 0) return;
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

    function screenEdgeValue(initial, final, progress, duration) {
      if (duration === 0) return final;
      return initial + Math.trunc((final - initial) * progress / duration);
    }

    function executeScreenTransition(node, words) {
      var initialFirst = lowS16(words[1]);
      var initialSecond = lowS16(words[2]);
      var finalFirst = lowS16(words[3]);
      var finalSecond = lowS16(words[4]);
      var duration = lowU16(words[5]);
      var persistence = lowS16(words[6]);
      var titleVariant = screenTransitionVariant === 0;
      var cutsceneVariant = Number.isInteger(screenTransitionVariant) &&
        screenTransitionVariant !== 0;
      var resolvedInitialFirst = cutsceneVariant && initialFirst === -1 &&
        initialSecond === -1 ? 24 : initialFirst;
      var resolvedInitialSecond = cutsceneVariant && initialFirst === -1 &&
        initialSecond === -1 ? 24 : initialSecond;
      state.screenTransition = {
        nodeId: node.id,
        initialFirst: resolvedInitialFirst,
        initialSecond: resolvedInitialSecond,
        authoredInitialFirst: initialFirst,
        authoredInitialSecond: initialSecond,
        finalFirst: finalFirst,
        finalSecond: finalSecond,
        progress: 0,
        duration: duration,
        persistence: persistence,
        currentFirst: screenEdgeValue(
          resolvedInitialFirst, finalFirst, 0, duration),
        currentSecond: screenEdgeValue(
          resolvedInitialSecond, finalSecond, 0, duration),
        directorSelector: screenTransitionVariant,
        presentationKind: titleVariant ? 'title-card-reveal' :
          (cutsceneVariant ? 'cutscene-crop' : 'external-unresolved')
      };
      if (!Number.isInteger(screenTransitionVariant)) {
        missing('Screen-edge transition ' + node.id +
          ' requires the launch Director selector to choose title reveal or cutscene crop rendering.');
      }
      state.effectEvents.push(eventRow(node, 'effect', 'Screen-edge transition', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3B',
        presentationKind: state.screenTransition.presentationKind,
        initialEdges: [resolvedInitialFirst, resolvedInitialSecond],
        finalEdges: [finalFirst, finalSecond],
        nativeDuration: duration,
        persistentFinalState: persistence !== 0
      }));
    }

    function executeActorPresentationBootstrap(node) {
      state.actorPresentationJob = {
        nodeId: node.id,
        kind: 'primary',
        evidenceStatus: 'external-unresolved'
      };
      missing('Actor-presentation bootstrap ' + node.id +
        ' requires the launch scene configuration, persistent-character records, and their target coordinates; the stream operand is not an Actor roster.');
      state.effectEvents.push(eventRow(node, 'effect', 'Actor-presentation bootstrap', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3F',
        presentationKind: 'persistent-character-actor-bootstrap',
        inputStatus: 'external-unresolved'
      }));
    }

    function executeSceneVignette(node, words) {
      var presentation = launchProfile.oversizedImagePresentation || null;
      var slot = signed(words[1]);
      var alphaCap = signed(words[7]);
      var orientationFlags = unsigned(words[8]);
      state.sceneVignette = {
        nodeId: node.id,
        sourceAssetId: presentation && presentation.assetId || null,
        sourceResourceKey: presentation && presentation.resourceKey || null,
        sourceArchiveIndex: presentation && presentation.archiveIndex || null,
        launchRowSelector: presentation && Number.isInteger(presentation.rowSelector)
          ? presentation.rowSelector : null,
        slot: slot,
        activeSlotByte: slot & 0xFF,
        translateX: signed(words[2]),
        translateY: signed(words[3]),
        ignoredPayload: signed(words[4]),
        scaleXPercent: signed(words[5]),
        scaleYPercent: signed(words[6]),
        alphaCap: alphaCap,
        transitionAlphaByte: unsigned(words[7]) & 0xFF,
        orientationFlags: orientationFlags,
        orientationBit08: (orientationFlags & 0x08) !== 0,
        baseRotationDegrees: (orientationFlags & 0x08) !== 0
          ? { x: 0, y: 0, z: -5 }
          : { x: 0, y: -90, z: 5 },
        sourceScaleX: 1,
        sourceScaleY: 1,
        outputScaleDivisor: 2,
        evidenceStatus: presentation && presentation.assetId
          ? 'native-static-exact' : 'launch-inputs-unresolved'
      };
      if (!presentation || !presentation.assetId) {
        missing('Scene vignette ' + node.id +
          ' requires the class-4 launch image selected through event property 0xE9.');
      }
      state.effectEvents.push(eventRow(node, 'effect', 'Scene vignette', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3A',
        sourceAssetId: state.sceneVignette.sourceAssetId,
        sceneImageSlot: slot,
        nativeTranslation: {
          x: state.sceneVignette.translateX,
          y: state.sceneVignette.translateY
        },
        nativeScalePercent: {
          x: state.sceneVignette.scaleXPercent,
          y: state.sceneVignette.scaleYPercent
        },
        alphaCap: alphaCap,
        orientationFlags: orientationFlags,
        inputStatus: state.sceneVignette.evidenceStatus
      }));
    }

    function executeOversizedImageTransition(node, words) {
      var startX = signed(words[1]);
      var startY = signed(words[2]);
      var targetX = signed(words[4]);
      var targetY = signed(words[5]);
      var zoomDirection = signed(words[6]);
      var duration = signed(words[7]);
      var currentPosition = startX === -1 && startY === -1;
      var rateStartX = currentPosition ? state.oversizedImageView.x : startX;
      var rateStartY = currentPosition ? state.oversizedImageView.y : startY;
      var deltaX = targetX - rateStartX;
      var deltaY = targetY - rateStartY;
      var rateX = deltaX >= -1 && deltaX <= 1 ? 0 : Math.fround(deltaX / duration);
      var rateY = deltaY >= -1 && deltaY <= 1 ? 0 : Math.fround(deltaY / duration);
      state.oversizedImageTransitionJob = {
        nodeId: node.id,
        progress: 0,
        duration: duration,
        rateX: rateX,
        rateY: rateY,
        targetX: targetX,
        targetY: targetY,
        currentPositionStart: currentPosition
      };
      if (zoomDirection === 0) {
        if (state.oversizedImageView.zoomState === 3 ||
            state.oversizedImageView.zoomState === 1) {
          state.oversizedImageView.zoomState = 2;
        }
      } else if (state.oversizedImageView.zoomState === 4 ||
          state.oversizedImageView.zoomState === 2) {
        state.oversizedImageView.zoomState = 1;
      }
      trace.push({
        tick: state.tick,
        kind: 'oversized-image-transition-start',
        nodeId: node.id,
        currentPositionStart: currentPosition,
        rateStartX: rateStartX,
        rateStartY: rateStartY,
        targetX: targetX,
        targetY: targetY,
        rateX: rateX,
        rateY: rateY,
        duration: duration,
        zoomDirection: zoomDirection
      });
      state.effectEvents.push(eventRow(node, 'effect',
        'Scripted oversized-image pan and zoom', {
          sourceSystem: 'director-native',
          nativeOpcode: '0x76',
          currentPositionStart: currentPosition,
          targetX: targetX,
          targetY: targetY,
          nativeDuration: duration,
          zoomDirection: zoomDirection
        }));
    }

    function executeCamera(node, words, bank) {
      var camera = {
        target: { x: fixed(words[1]), y: fixed(words[2]), z: fixed(words[3]) },
        eye: { x: fixed(words[4]), y: fixed(words[5]), z: fixed(words[6]) },
        fovYDegrees: fixed(words[7]),
        sourceNodeId: node.id,
        evidenceStatus: 'native-director-command',
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
      if (documentBackground && directorMode.value === 2) {
        state.background = M.cloneJson(documentBackground.background,
          'document launch background');
        return;
      }
      var observation = scene.backgroundRuntimeObservation || null;
      if (observation && Array.isArray(observation.stageLayers) &&
          observation.stageLayers.length) {
        state.background = observationBackground(scene, document, catalog);
        return;
      }
      var requestProfile = launchProfile.background.requests.find(function(request) {
        return request.wordStart === node.startWord;
      }) || null;
      var requestStageProps = requestProfile &&
        modeTwoStageProps(catalog, requestProfile.foregroundSelector);
      if (requestProfile && (Array.isArray(requestProfile.stageLayers) &&
          requestProfile.stageLayers.length || hasRenderableStageProps(requestStageProps))) {
        state.background = profileBackground(requestProfile, document, catalog);
        if (directorMode.value === 2) {
          if (requestProfile.foregroundSelector === null) {
            missing(requestProfile.foregroundStatus ||
              'The final mode-two foreground selector remains launch-owned.');
          }
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
          selectorTableId: requestProfile ? requestProfile.selectorTableId : null,
          selector: null,
          environmentSelector: null,
          foregroundSelectorTableId: requestProfile
            ? requestProfile.foregroundSelectorTableId : null,
          foregroundSelector: null
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
          selector: requestProfile.selector,
          environmentSelector: requestProfile.environmentSelector,
          foregroundSelectorTableId: requestProfile.foregroundSelectorTableId,
          foregroundSelector: requestProfile.foregroundSelector
        };
        missing('Mode-two background selector ' + requestProfile.selector +
          ' has no complete registered Stage; its raw resource is only a partial layer or is empty.');
        return;
      }
      var tableId = requestProfile.selectorTableId;
      var runtimeSelector = requestProfile.selector;
      if (requestProfile.selectorSource === 'director-command-operand') {
        runtimeSelector = commandOperand;
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
        selector: runtimeSelector,
        environmentSelector: requestProfile.environmentSelector,
        foregroundSelectorTableId: requestProfile.foregroundSelectorTableId,
        foregroundSelector: requestProfile.foregroundSelector
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

    function executeDialogueCreate(node, words, unresolvedWordOffsets) {
      var windowId = signed(words[1]);
      var selector = signed(words[2]);
      var entrySelector = unresolvedWordOffsets.indexOf(3) === -1
        ? signed(words[3]) : null;
      var archive = catalog && catalog.getSerifuArchiveForPresentationSelector
        ? catalog.getSerifuArchiveForPresentationSelector(selector) : null;
      var entry = entrySelector == null ? null :
        archive && archive.entries && archive.entries[entrySelector] || null;
      var segments = dialogueSegments(entry);
      if (!entry && entrySelector != null) missing('Serifu selector ' + selector + ', entry ' + entrySelector +
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
          portraitIdentity: unresolvedWordOffsets.indexOf(11) === -1
            ? signed(words[11]) : null,
          portraitVariant: signed(words[12]),
          placementMode: signed(words[13])
        },
        launchTranslationMissing: unresolvedWordOffsets.length > 0,
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

    function programForSpriteEffect(effect) {
      if (!catalog || !effect) return null;
      if (catalog.getPhysicalPoseProgramByStateIndex &&
          Number.isInteger(effect.stateIndex) && effect.stateIndex >= 0) {
        var stateProgram = catalog.getPhysicalPoseProgramByStateIndex(
          effect.bank, effect.stateIndex);
        if (stateProgram) return stateProgram;
      }
      return catalog.getPhysicalPoseProgram && Number.isInteger(effect.bank) &&
        Number.isInteger(effect.animationKey) && Number.isInteger(effect.stateFacing)
        ? catalog.getPhysicalPoseProgram(effect.bank, effect.animationKey,
          effect.stateFacing, effect.variantSelector) : null;
    }

    function syncSpriteEffectPayload(effect) {
      var payload = effect.payload;
      var program = programForSpriteEffect(effect);
      payload.bank = effect.bank;
      payload.animationKey = program ? program.animationKey : effect.animationKey;
      payload.nativeFacing = program ? program.facing : effect.stateFacing;
      payload.variantSelector = effect.variantSelector;
      payload.poseId = program ? program.poseId : poseId(
        effect.bank, effect.animationKey, effect.stateFacing);
      payload.nativeStateIndex = effect.stateIndex;
      payload.nativeProgramCursor = effect.programCursor;
      payload.nativeProgramOpcode = effect.currentOpcode;
      payload.nativeProgramDelay = effect.delay;
      payload.displayedFrameToken = effect.displayedFrameToken;
      payload.stageX = Number.isFinite(effect.positionX) ? 160 + effect.positionX : null;
      payload.stageY = Number.isFinite(effect.positionY) ? 120 + effect.positionY : null;
      payload.rotationDegrees = Number.isFinite(effect.rotationDegrees)
        ? effect.rotationDegrees : null;
      payload.nativeRotationValue = Number.isInteger(effect.rotationValue)
        ? effect.rotationValue : null;
      payload.rotationEvidence = effect.rotationEvidence;
      payload.colorBlock = effect.colorBlock.slice();
      payload.materialBlock = effect.materialBlock.slice();
      payload.poseFrame = effect.poseFrame;
    }

    function resetSpriteEffectProgram(effect) {
      var program = catalog && catalog.getPhysicalPoseProgram
        ? catalog.getPhysicalPoseProgram(effect.bank, effect.animationKey,
          effect.stateFacing, effect.variantSelector) : null;
      effect.stateIndex = program ? program.stateIndex : -1;
      effect.programCursor = -1;
      effect.delay = 0;
      effect.currentOpcode = 0;
      effect.displayedFrameToken = 0;
      effect.poseFrame = 0;
      effect.colorBlock = Array(16).fill(0xFF);
      effect.materialBlock = Array(16).fill(0);
      if (!program) {
        missing('Animated scene sprite slot ' + effect.slot + ' cannot resolve Actor Art Source ' +
          effect.bank + ', Animation ' + effect.animationKey + ', Facing ' +
          effect.stateFacing + '.');
      }
    }

    function updateSpriteEffectProgram(effect) {
      if (!effect || effect.nativeProgramInterpreter !== true) return;
      effect.poseFrame += 2;
      if (effect.delay > 0) {
        effect.delay -= 2;
        syncSpriteEffectPayload(effect);
        return;
      }
      var guard = 0;
      while (effect.delay <= 0) {
        if (++guard > 1024) {
          missing('Animated scene sprite slot ' + effect.slot +
            ' exceeded the native instantaneous-program guard.');
          effect.currentOpcode = 0;
          effect.delay = 2;
          break;
        }
        effect.programCursor += 1;
        var program = programForSpriteEffect(effect);
        var records = program && Array.isArray(program.records) ? program.records : [];
        var record = records[effect.programCursor] || { opcode: 0, operands: [] };
        var opcode = record.opcode & 0xFF;
        var operands = record.operands || [];
        effect.currentOpcode = opcode;
        if (opcode === 0x00) {
          effect.delay = 2;
        } else if (opcode === 0x01) {
          effect.displayedFrameToken = operands[0] || 0;
          effect.delay = operands[1] || 0;
        } else if (opcode === 0x02) {
          effect.positionX -= lowS8(operands[0] || 0);
          effect.positionY -= lowS8(operands[1] || 0);
        } else if (opcode === 0x03) {
          effect.delay = operands[0] || 0;
        } else if (opcode === 0x04) {
          effect.programCursor = (operands[0] || 0) - 1;
        } else if (opcode === 0x05) {
          effect.stateIndex = operands[0] || 0;
          effect.programCursor = -1;
        } else if (opcode === 0x0D || opcode === 0x10) {
          var blockBytes = opcode === 0x0D ? effect.colorBlock : effect.materialBlock;
          var blockIndex = operands[0] || 0;
          var blockValue = operands[1] || 0;
          if (blockIndex === 0xFF) blockBytes.fill(blockValue);
          else if (blockIndex < blockBytes.length) blockBytes[blockIndex] = blockValue;
        } else if (opcode === 0x15) {
          effect.displayedFrameToken = (operands[0] || 0) | ((operands[1] || 0) << 8);
          effect.delay = operands[2] || 0;
        }
      }
      if (effect.delay > 0) effect.delay -= 2;
      syncSpriteEffectPayload(effect);
    }

    function newSpriteEffect(node, slot, payload, fields) {
      var effect = {
        id: 'runtime-effect:' + node.id,
        kind: 'effect',
        trackId: 'track:runtime:effect',
        label: 'Native Cutscene sprite effect',
        startFrame: state.tick,
        durationFrames: 1,
        capability: fields.capability || M.capabilities.PREVIEW_ONLY,
        payload: payload,
        slot: slot,
        bank: fields.bank,
        animationKey: fields.animationKey,
        stateFacing: fields.stateFacing,
        variantSelector: fields.variantSelector,
        positionX: fields.positionX,
        positionY: fields.positionY,
        rotationValue: fields.rotationValue,
        rotationDegrees: fields.rotationDegrees,
        rotationEvidence: fields.rotationEvidence,
        renderPassSelector: fields.renderPassSelector,
        scale: fields.scale,
        nativeProgramInterpreter: true,
        poseFrame: 0
      };
      resetSpriteEffectProgram(effect);
      state.spriteEffects[slot] = effect;
      updateSpriteEffectProgram(effect);
      return effect;
    }

    function executeSpriteEffect(node, words) {
      var row = rowFor(node, 'effect');
      var payload = row && row.clip.payload || null;
      var slot = payload && Number.isInteger(payload.nativeEffectSlot)
        ? payload.nativeEffectSlot : unsigned(words[1]) & 0xFF;
      var runtimePayload = payload ? M.cloneJson(payload, 'effect.payload') : {
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
        };
      newSpriteEffect(node, slot, runtimePayload, {
        bank: signed(words[2]),
        animationKey: lowS16(words[3]),
        stateFacing: signed(words[4]),
        variantSelector: unsigned(words[9]) & 0xFF,
        positionX: signed(words[5]),
        positionY: -signed(words[6]),
        rotationValue: 180,
        rotationDegrees: 0,
        rotationEvidence: 'native-constructor-identity',
        renderPassSelector: unsigned(words[7]) & 0xFF,
        scale: signed(words[8]) / 100,
        capability: row ? row.clip.capability : M.capabilities.PREVIEW_ONLY
      });
    }

    function executeAnimatedSceneSprite(node, words) {
      var slot = unsigned(words[1]) & 0xFF;
      var creationSentinel = signed(words[10]);
      var effect = state.spriteEffects[slot];
      if (creationSentinel === -1) {
        effect = newSpriteEffect(node, slot, {
          sourceSystem: 'cutscene-sprite-native',
          nativeOpcode: '0x63',
          nativeEffectSlot: slot
        }, {
          bank: signed(words[2]),
          animationKey: signed(words[3]),
          stateFacing: 0,
          variantSelector: unsigned(words[4]) & 0xFF,
          positionX: signed(words[5]),
          positionY: -signed(words[6]),
          rotationValue: 180,
          rotationDegrees: 0,
          rotationEvidence: 'native-constructor-identity-before-rotation-route',
          renderPassSelector: 1,
          scale: 1
        });
      } else if (!effect) {
        missing('Animated scene sprite opcode 0x63 reuses absent slot ' + slot + '.');
        return;
      }
      if (signed(words[9]) === -1) {
        effect.rotationValue = lowS16(words[7]);
        effect.rotationDegrees = effect.rotationValue === 180
          ? 0 : effect.rotationValue + 180;
        effect.rotationEvidence = 'native-direct-rotation-operand';
        effect.payload.nativeRotationRoute = 'direct-operand';
        effect.payload.nativeRotationPathId = null;
        effect.payload.nativeRotationPathGroup = null;
        effect.payload.nativeRotationPathEntry = null;
      } else if (signed(words[8]) === -1) {
        effect.rotationValue = null;
        effect.rotationDegrees = null;
        effect.rotationEvidence = 'sampled-scene-path-heading-unresolved';
        effect.payload.nativeRotationRoute = 'sampled-scene-path';
        effect.payload.nativeRotationPathId = null;
        effect.payload.nativeRotationPathGroup = null;
        effect.payload.nativeRotationPathEntry = signed(words[9]);
        missing('Animated scene sprite slot ' + slot +
          ' uses a launch-built sampled scene path; its path record is not available.');
      } else {
        var pathGroup = signed(words[8]);
        var pathEntry = signed(words[9]);
        var resourcePath = catalog && catalog.getSceneResourcePath
          ? catalog.getSceneResourcePath(pathGroup, pathEntry) : null;
        effect.payload.nativeRotationRoute = 'resource-path';
        effect.payload.nativeRotationPathGroup = pathGroup;
        effect.payload.nativeRotationPathEntry = pathEntry;
        effect.payload.nativeRotationPathId = resourcePath && resourcePath.pathId || null;
        if (resourcePath && resourcePath.status === 'native-static-path-heading' &&
            Number.isInteger(resourcePath.nativeStoredHeading) &&
            Number.isInteger(resourcePath.rotationDegrees)) {
          effect.rotationValue = resourcePath.nativeStoredHeading;
          effect.rotationDegrees = resourcePath.rotationDegrees;
          effect.rotationEvidence = 'native-resource-path-spline-start-heading';
        } else {
          effect.rotationValue = null;
          effect.rotationDegrees = null;
          effect.rotationEvidence = resourcePath
            ? 'native-resource-path-empty-entry'
            : 'native-resource-path-selection-invalid';
          missing('Animated scene sprite slot ' + slot + ' selects unavailable native resource ' +
            'path ' + pathGroup + ':' + pathEntry + '.');
        }
      }
      syncSpriteEffectPayload(effect);
    }

    function executeAnimatedSceneSpriteRestart(node, words) {
      var slot = unsigned(words[1]) & 0xFF;
      var effect = state.spriteEffects[slot];
      if (!effect) {
        missing('Animated scene sprite opcode 0x64 retargets absent slot ' + slot + '.');
        return;
      }
      if (signed(words[2]) !== -1) effect.bank = signed(words[2]);
      var animationKey = lowS16(words[3]);
      if (animationKey !== -1) effect.animationKey = animationKey;
      effect.stateFacing = 0;
      effect.variantSelector = unsigned(words[4]) & 0xFF;
      effect.payload.nativeOpcode = '0x64';
      effect.payload.nativeRestartNodeId = node.id;
      resetSpriteEffectProgram(effect);
      updateSpriteEffectProgram(effect);
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
      var execution = executionWords(node);
      var words = execution.words;
      var opcode = unsigned(words[0]);
      state.executedNodeIds.push(node.id);
      trace.push({ tick: state.tick, kind: 'command', nodeId: node.id,
        opcode: node.opcodeHex, name: node.name });
      if (execution.translatedWordOffsets.length) {
        trace.push({
          tick: state.tick,
          kind: 'launch-translation',
          nodeId: node.id,
          wordOffsets: execution.translatedWordOffsets.slice()
        });
      }
      if (execution.unresolvedWordOffsets.length) {
        trace.push({
          tick: state.tick,
          kind: 'launch-translation-missing',
          nodeId: node.id,
          wordOffsets: execution.unresolvedWordOffsets.slice()
        });
        var previewableActorVariant = opcode === 0x14 &&
          execution.unresolvedWordOffsets.every(function(wordOffset) {
            return wordOffset === 9;
          });
        var previewableDialogueHandshake = opcode === 0xBF &&
          execution.unresolvedWordOffsets.every(function(wordOffset) {
            return wordOffset === 3 || wordOffset === 11;
          });
        if (!previewableActorVariant && !previewableDialogueHandshake) return;
      }

      if (node.name === 'handoff_marker') {
        parserResumeMarked = true;
        return;
      }
      if (node.name === 'branch_barrier') return;
      if (node.name === 'control_bridge_and_pending_substream_handoff') {
        if (parserResumeMarked) commitPersistentCursorAfter(node);
        consumePendingSubstream(node);
        block = {
          kind: 'parser-boundary',
          untilTick: state.tick + 1,
          label: node.label,
          clock: 'director-evaluation'
        };
        return;
      }

      if (node.query) {
        executeBranchQuery(node, words);
        return;
      }

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
      else if (opcode === 0x14) executeActorCreate(
        node, words, execution.unresolvedWordOffsets);
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
      else if (opcode === 0x3B) executeScreenTransition(node, words);
      else if (opcode === 0x3D) executeProjection(node, words, true);
      else if (opcode === 0x3F) executeActorPresentationBootstrap(node);
      else if (opcode === 0x3A) executeSceneVignette(node, words);
      else if (opcode === 0x45 || opcode === 0xAB) {
        missing('Actor-roster materializer ' + node.opcodeHex + ' at word ' +
          node.startWord + ' requires the caller\'s 20 Actor-input rows; ' +
          'catalog templates are not launch records.');
      }
      else if (opcode === 0x46) executeSpriteEffect(node, words);
      else if (opcode === 0x47) state.shadowLight = {
        x: signed(words[1]), y: signed(words[2]), z: signed(words[3])
      };
      else if (opcode === 0x48) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.opacityByte = unsigned(words[2]) & 0xFF;
      });
      else if (opcode === 0x56) parserResynchronization = true;
      else if (opcode === 0x59) {
        var marker = signed(words[1]);
        var destination = directorLabelByMarker[marker] || {
          marker: marker,
          nodeId: activeProgram.primitives[0] && activeProgram.primitives[0].id || null,
          startWord: 0,
          primitiveIndex: 0,
          compositeIndex: 0
        };
        persistentCursorPrimitiveIndex = destination.primitiveIndex;
        installCursorAtPrimitive(destination.primitiveIndex);
        block = {
          kind: 'cursor-replacement',
          untilTick: state.tick + 1,
          label: 'Jump to Director label ' + marker,
          clock: 'director-evaluation'
        };
        trace.push({
          tick: state.tick,
          kind: 'cursor-replacement',
          sourceNodeId: node.id,
          marker: marker,
          destinationNodeId: destination.nodeId,
          destinationWord: destination.startWord
        });
      }
      else if (opcode === 0x5F) {
        var x1 = signed(words[1]), z1 = signed(words[2]);
        var x2 = signed(words[3]), z2 = signed(words[4]);
        state.transformDivider = {
          x1: x1, z1: z1, x2: x2, z2: z2,
          trueChannel: signed(words[5]), falseChannel: signed(words[6])
        };
      }
      else if (opcode === 0x62) {
        var transientSlot = signed(words[1]);
        state.transientRenderEntities[transientSlot] = {
          slot: transientSlot,
          preset: signed(words[2]),
          status: 0,
          statusSource: 'native-creation-clear',
          createdTick: state.tick,
          detached: false,
          closing: false,
          closeRemaining: null
          };
      }
      else if (opcode === 0x63) executeAnimatedSceneSprite(node, words);
      else if (opcode === 0x64) executeAnimatedSceneSpriteRestart(node, words);
      else if (opcode === 0x66) {
        var effectSlot = signed(words[1]);
        if (effectSlot === -1) state.spriteEffects = {};
        else delete state.spriteEffects[effectSlot];
      }
      else if (opcode === 0x6B) {
        var releaseTransientSlot = signed(words[1]);
        if (releaseTransientSlot === -1) state.transientRenderEntities = {};
        else delete state.transientRenderEntities[releaseTransientSlot];
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
      else if (opcode === 0x76) executeOversizedImageTransition(node, words);
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
      else if (opcode === 0x39) {
        var lifecycleOperand = signed(words[1]);
        if (lifecycleOperand === 0) {
          state.presentationLifecycleRequest = 0xD7;
          state.terminalReason = 'presentation-reload-handoff';
          state.terminal = true;
          trace.push({
            tick: state.tick,
            kind: 'presentation-reload-handoff',
            nodeId: node.id,
            requestCode: 0xD7
          });
        } else {
          state.presentationLifecycleRequest = 0;
          state.alternateDirectorScheduling = false;
          trace.push({
            tick: state.tick,
            kind: 'presentation-lifecycle-switch',
            nodeId: node.id,
            operand: lifecycleOperand,
            alternateDirectorScheduling: false
          });
        }
      }
      else if (opcode === 0x7D) {
        state.terminalStateReleased = true;
        state.terminalReason = 'terminal-state-release';
        state.terminal = true;
      }
      else if (opcode === 0x7E) {
        state.overlay = null;
        state.overlayJob = null;
      }
      else if (opcode === 0x83) {
        var closingTransient = state.transientRenderEntities[signed(words[1])];
        if (closingTransient && !closingTransient.detached) {
          closingTransient.closing = true;
          closingTransient.closeRemaining = 8;
        }
      }
      else if (opcode === 0x8B) {
        var detachedTransient = state.transientRenderEntities[signed(words[1])];
        if (detachedTransient) {
          detachedTransient.detached = true;
          detachedTransient.closing = false;
          detachedTransient.closeRemaining = null;
        }
      }
      else if (opcode === 0x8C) state.armyManagementCursorLatch = signed(words[1]);
      else if (opcode === 0x99) pendingSubstreamSelector = unsigned(words[1]) & 0xFF;
      else if (opcode === 0x9A) pendingSubstreamSelector = 0xFE;
      else if (opcode === 0xAF) {
        state.titleJob = { nodeId: node.id, remaining: 30, duration: 30, kind: 'alpha' };
        assumption('Prologue title alpha uses a 30-tick preview envelope.');
      }
      else if (opcode === 0xB0) {
        state.titleJob = { nodeId: node.id, remaining: 85, duration: 85, kind: 'reveal' };
        assumption('Prologue secondary-title reveal uses its static 85-update Stage envelope.');
      }
      else if (opcode === 0xBB) {
        state.directorTeardown = true;
        state.terminalReason = 'director-teardown';
        state.terminal = true;
      }
      else if (opcode === 0xBF) executeDialogueCreate(
        node, words, execution.unresolvedWordOffsets);
      else if (opcode === 0x80000001) {
        state.terminalReason = 'terminal-hold';
        state.terminal = true;
      }
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

    function updateScreenTransition() {
      var transition = state.screenTransition;
      if (!transition || transition.progress === transition.duration) return;
      transition.progress = Math.min(transition.duration, transition.progress + 1);
      transition.currentFirst = screenEdgeValue(transition.initialFirst,
        transition.finalFirst, transition.progress, transition.duration);
      transition.currentSecond = screenEdgeValue(transition.initialSecond,
        transition.finalSecond, transition.progress, transition.duration);
      if (transition.progress === transition.duration && transition.persistence === 0) {
        state.screenTransition = null;
      }
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
        updateSpriteEffectProgram(state.spriteEffects[slot]);
      });
      Object.keys(state.actors).forEach(function(slot) {
        state.actors[slot].poseFrame += 2;
      });
      Object.keys(state.dialogues).forEach(function(windowId) {
        var window = state.dialogues[windowId];
        if (!window.closed && state.tick >= window.readyTick) window.paused = true;
      });
    }

    function updateOversizedImageTransition() {
      var view = state.oversizedImageView;
      if (view.zoomState === 1) {
        view.scale = Math.fround(Math.max(0.7, view.scale - 0.01));
        if (view.scale <= 0.7) view.zoomState = 3;
      } else if (view.zoomState === 2) {
        view.scale = Math.fround(Math.min(1, view.scale + 0.01));
        if (view.scale >= 1) view.zoomState = 4;
      }
      var job = state.oversizedImageTransitionJob;
      if (!job) return;
      if (job.progress === job.duration) {
        state.oversizedImageTransitionJob = null;
        return;
      }
      job.progress = (job.progress + 1) | 0;
      view.x = Math.fround(view.x + job.rateX);
      view.y = Math.fround(view.y + job.rateY);
    }

    function updateJobs() {
      updateOversizedImageTransition();
      updateMovementJobs();
      updateTurnJobs();
      updateProjectionJob();
      updateScreenTransition();
      updateTintJobs();
      updateColorJobs();
      updateOtherJobs();
      Object.keys(state.transientRenderEntities).forEach(function(slot) {
        var entity = state.transientRenderEntities[slot];
        if (!entity || entity.detached) return;
        if (entity.closing) {
          entity.closeRemaining -= 1;
          if (entity.closeRemaining <= 0) {
            entity.detached = true;
            entity.closing = false;
            entity.closeRemaining = null;
            trace.push({
              tick: state.tick,
              kind: 'transient-render-entity-detach',
              slot: entity.slot,
              preset: entity.preset,
              source: 'native-eight-update-graceful-close'
            });
          }
          return;
        }
        if (entity.statusSource === 'native-creation-clear' &&
            entity.createdTick < state.tick &&
            entity.preset >= 1 && entity.preset <= 3) {
          entity.status = -1;
          entity.statusSource = 'native-main-menu-neutral';
          trace.push({
            tick: state.tick,
            kind: 'transient-render-entity-status',
            slot: entity.slot,
            preset: entity.preset,
            status: entity.status,
            source: entity.statusSource
          });
        }
      });
      if (state.registeredCounter) {
        var registeredValue = state.registeredCounter.value >>> 0;
        if (registeredValue >= 1 && registeredValue <= 0x0FFFFFFE) {
          state.registeredCounter.value = (registeredValue + 1) >>> 0;
        }
      }
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

    function passingQueryValue(query) {
      var mode = query.query.compareMode;
      var target = query.query.target;
      if (mode === 1) return target === 0 ? 1 : 0;
      if (mode === 4) return target + 1;
      if (mode === 5) return target - 1;
      return target;
    }

    function incompleteLifecycleValue(query, actual, context, message) {
      if (!context || context.kind !== 'wait' ||
          compare(actual, query.query.compareMode, query.query.target)) return actual;
      assumption(message);
      return passingQueryValue(query);
    }

    function externalQueryValue(query) {
      var externalValues = options.externalQueryValues || {};
      var input = query.query && query.query.producerInput;
      var inputKey = query.name + ':' + String(input == null ? '' : input);
      if (Object.prototype.hasOwnProperty.call(externalValues, query.id)) {
        return externalValues[query.id];
      }
      if (Object.prototype.hasOwnProperty.call(externalValues, inputKey)) {
        return externalValues[inputKey];
      }
      if (Object.prototype.hasOwnProperty.call(externalValues, query.name)) {
        return externalValues[query.name];
      }
      return null;
    }

    function queryActual(query, context) {
      context = context || {};
      var input = query.query && query.query.producerInput;
      if (query.name === 'registered_counter_query' ||
          query.name === 'a_button_skippable_registered_wait_query') {
        if (query.name === 'a_button_skippable_registered_wait_query' &&
            Number.isInteger(options.controllerMask) &&
            (options.controllerMask & 0x8000) !== 0) {
          state.registeredCounter = {
            value: (query.query.target + 1) >>> 0,
            armTick: state.tick,
            source: 'a-button-skip'
          };
        }
        var registeredValue = state.registeredCounter
          ? state.registeredCounter.value >>> 0 : 0;
        return (registeredValue - 1) | 0;
      }
      if (query.name === 'actor_movement_countdown_query') {
        return state.movementJobs[input] ? lowS16(state.movementJobs[input].remaining) : 0;
      }
      if (query.name === 'actor_facing_turn_activity_query') return state.turnJobs[input] ? 1 : 0;
      if (query.name === 'dialogue_pause_query') {
        var window = state.dialogues[input];
        if (!window || window.closed) return 0;
        return window.paused ? 2 : 1;
      }
      if (query.name === 'scene_projection_transform_countdown_query_mode2' ||
          query.name === 'scene_projection_transform_countdown_query_unguarded') {
        return state.projectionJob ? state.projectionJob.remaining : 0;
      }
      if (query.name === 'screen_edge_transition_activity_query') {
        return state.screenTransition &&
          state.screenTransition.progress !== state.screenTransition.duration ? 1 : 0;
      }
      if (query.name === 'scripted_oversized_image_transition_query') {
        return state.oversizedImageTransitionJob ? 1 : 0;
      }
      if (query.name === 'actor_presentation_activity_query') {
        var suppliedPresentationStatus = externalQueryValue(query);
        if (Number.isInteger(suppliedPresentationStatus)) {
          if (suppliedPresentationStatus === 0) state.actorPresentationJob = null;
          return suppliedPresentationStatus;
        }
        if (context.kind === 'wait') {
          assumption('Actor-presentation lifecycle input is unavailable; the exact native wait uses an explicit completed-state assumption.');
          state.actorPresentationJob = null;
          return passingQueryValue(query);
        }
        return state.actorPresentationJob ? 1 : 0;
      }
      if (query.name === 'color_overlay_countdown_query') {
        return state.overlayJob ? state.overlayJob.remaining : 0;
      }
      if (query.name === 'actor_state_pose_opcode_query') {
        var actor = state.actors[input];
        return actor && state.tick < actor.poseReadyTick ? 1 : 0;
      }
      if (query.name === 'animated_scene_sprite_program_opcode_query') {
        var spriteEffect = state.spriteEffects[input];
        if (!spriteEffect) {
          missing('Animated scene sprite program query selects absent slot ' + input + '.');
          return 0;
        }
        return spriteEffect.currentOpcode & 0xFF;
      }
      if (query.name === 'scene_transform_sequence_query') {
        return state.sceneTransformJob ? 0 : 1;
      }
      if (query.name === 'prologue_title_reveal_query') {
        return state.titleJob && state.titleJob.kind === 'reveal' ? 1 : 0;
      }
      if (query.name === 'global_halfword_mask_query') {
        var controllerMask = Number.isInteger(options.controllerMask)
          ? options.controllerMask & 0xFFFF : 0;
        if (!Number.isInteger(options.controllerMask)) {
          var neutralMaskValue = (controllerMask & (input & 0xFFFF)) !== 0 ? 1 : 0;
          return incompleteLifecycleValue(query, neutralMaskValue, context,
            'No controller input is supplied; this native input wait uses an explicit completed-state assumption.');
        }
        return (controllerMask & (input & 0xFFFF)) !== 0 ? 1 : 0;
      }
      if (query.name === 'transient_render_entity_status_query') {
        var suppliedTransientStatus = externalQueryValue(query);
        if (Number.isInteger(suppliedTransientStatus)) return suppliedTransientStatus;
        var transientEntity = state.transientRenderEntities[input];
        var transientStatus = !transientEntity ? -5 :
          (transientEntity.detached ? -6 : transientEntity.status);
        if (context.kind === 'branch' && transientEntity &&
            transientEntity.statusSource === 'native-main-menu-neutral') {
          missing('Transient render-entity preset ' + transientEntity.preset +
            ' requires a controller/menu result; the native neutral status -1 is preserved.');
        }
        return incompleteLifecycleValue(query, transientStatus, context,
          'Transient render-entity preset ' +
            (transientEntity ? transientEntity.preset : 'absent') +
            ' has an external preset-specific updater; this native wait uses its completed-state assumption.');
      }
      if (query.name === 'army_management_cursor_latch_query') {
        var armyLatchStatus = state.armyManagementCursorLatch === 1 ? 0 : 1;
        return incompleteLifecycleValue(query, armyLatchStatus, context,
          'Army Management cursor input is not supplied; this native wait uses an explicit completed-state assumption.');
      }
      var externalValue = externalQueryValue(query);
      if (Number.isInteger(externalValue)) return externalValue;
      if (context.kind === 'wait') {
        assumption('Native wait input for ' + query.label +
          ' is outside the preview model; the wait uses an explicit completed-state assumption.');
        return passingQueryValue(query);
      }
      missing('Native query input for ' + query.label +
        ' is not supplied; the deterministic preview uses the producer\'s neutral value zero.');
      return 0;
    }

    function queryEnabled(query) {
      if (!query) return false;
      if (query.name === 'scene_transform_sequence_query') {
        return state.directorMode === 0;
      }
      if (query.name === 'actor_body_pose_cycle_query' ||
          query.name === 'scene_projection_transform_countdown_query_mode2') {
        return state.directorMode === 2;
      }
      return true;
    }

    function queryPasses(query, context) {
      if (!query || !query.query) return true;
      if (!queryEnabled(query)) return true;
      return compare(queryActual(query, context),
        query.query.compareMode, query.query.target);
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
      if (start.name === 'scripted_oversized_image_pan_zoom') {
        return !!state.oversizedImageTransitionJob;
      }
      if (/scene_projection_transform/.test(start.name)) return !!state.projectionJob;
      if (start.name === 'screen_edge_transition_start') {
        return !!state.screenTransition &&
          state.screenTransition.progress !== state.screenTransition.duration;
      }
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
          poseProgramStatus: actor.poseProgramStatus,
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
          renderPipeline: modeZeroStage ? 'mode-zero-registered-prepass-actor-camera' :
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
            windowId: window.windowId,
            sourceSystem: 'serifu-runtime',
            dialogueArchiveId: window.archive && window.archive.archiveId ||
              window.dialogueArchiveId || null,
            dialogueEntryId: entry && entry.entryId ||
              window.dialogueEntryId || null,
            presentationArchiveSelector: window.selector,
            presentationEntrySelector: window.entrySelector,
            speaker: entry && (entry.speakerLabel ||
              (entry.speakerId == null ? 'Narrator' : 'Speaker ' + entry.speakerId)) ||
              window.speaker || 'Narrator',
            text: window.segments[window.segmentIndex],
            rawText: entry && entry.rawText || window.rawText || '',
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
        screenTransition: state.screenTransition
          ? Object.assign({}, state.screenTransition) : null,
        sceneVignette: state.sceneVignette
          ? M.cloneJson(state.sceneVignette, 'scene vignette') : null,
        oversizedImageView: Object.assign({}, state.oversizedImageView),
        oversizedImageTransition: state.oversizedImageTransitionJob
          ? Object.assign({}, state.oversizedImageTransitionJob) : null,
        transformChannels: state.transformChannels.map(function(channel) {
          return Object.assign({}, channel);
        }),
        runtime: {
          engine: 'director-scheduler',
          directorMode: state.directorMode,
          directorModeStatus: state.directorModeStatus,
          directorSelector: screenTransitionVariant,
          tick: state.tick,
          activeStreamAssetId: activeStreamAssetId,
          savedParentStreamAssetId: savedStreamFrame && savedStreamFrame.assetId || null,
          compositeIndex: compositeIndex,
          blockKind: block && block.kind || null,
          blockLabel: block && block.label || null,
          assumptionCount: assumptions.length,
          missingInputCount: missingInputs.length,
          presentationLifecycleRequest: state.presentationLifecycleRequest,
          alternateDirectorScheduling: state.alternateDirectorScheduling,
          actorPresentationStatus: state.actorPresentationJob
            ? state.actorPresentationJob.evidenceStatus : null,
          terminalReason: state.terminalReason,
          status: missingInputs.length ? 'missing-inputs' :
            (assumptions.length ? 'assumed-inputs' : 'profiled')
        }
      };
    }

    function runtimeQuery(node, words) {
      var query = Object.assign({}, node.query || {});
      query.compareMode = signed(words[1]);
      query.target = signed(words[2]);
      if (query.recordKind === 'Q4') query.producerInput = signed(words[3]);
      return Object.assign({}, node, { query: query });
    }

    function installCursorAtPrimitive(primitiveIndex) {
      var node = activeProgram.primitives[primitiveIndex];
      if (!node) {
        state.terminal = true;
        return null;
      }
      var compositeId = activeProgram.compositeByNodeId[node.id];
      var destinationCompositeIndex = compositeIndexById[compositeId];
      if (!Number.isInteger(destinationCompositeIndex)) {
        fail('Director cursor destination ' + node.id +
          ' has no owning composite.', 'cursor-ownership');
      }
      compositeIndex = destinationCompositeIndex;
      compositeEntryNodeId = node.id;
      cursorRevision += 1;
      return node;
    }

    function nextBarrier(node) {
      var startIndex = primitiveIndexById[node.id] + 1;
      for (var index = startIndex; index < activeProgram.primitives.length; index++) {
        var candidate = activeProgram.primitives[index];
        if (candidate.name !== 'branch_barrier') continue;
        var compositeId = activeProgram.compositeByNodeId[candidate.id];
        return {
          node: candidate,
          compositeIndex: compositeIndexById[compositeId],
          resumePrimitiveIndex: index + 1,
          resumeNode: activeProgram.primitives[index + 1] || null
        };
      }
      return null;
    }

    function normalFailureBarrier(node, depth) {
      var startIndex = primitiveIndexById[node.id] + 1;
      var bridgeIndex = -1;
      for (var index = startIndex; index < activeProgram.primitives.length; index++) {
        if (activeProgram.primitives[index].name ===
            'control_bridge_and_pending_substream_handoff') {
          bridgeIndex = index;
          break;
        }
      }
      if (bridgeIndex < 0) return null;
      var remaining = Math.max(1, depth || 1);
      for (var cursor = bridgeIndex - 1; cursor > startIndex; cursor--) {
        var candidate = activeProgram.primitives[cursor];
        if (candidate.name !== 'branch_barrier') continue;
        remaining -= 1;
        if (remaining > 0) continue;
        var compositeId = activeProgram.compositeByNodeId[candidate.id];
        return {
          node: candidate,
          bridgeNode: activeProgram.primitives[bridgeIndex],
          compositeIndex: compositeIndexById[compositeId],
          resumePrimitiveIndex: cursor + 1,
          resumeNode: activeProgram.primitives[cursor + 1] || null
        };
      }
      return null;
    }

    function executeBranchQuery(node, words) {
      var query = runtimeQuery(node, words);
      if (!queryEnabled(query)) {
        trace.push({
          tick: state.tick,
          kind: 'query-mode-skip',
          nodeId: node.id,
          directorMode: state.directorMode
        });
        return;
      }
      branchDepth += 1;
      var actual = queryActual(query, { kind: 'branch' });
      var passes = compare(actual, query.query.compareMode, query.query.target);
      if (passes) {
        trace.push({
          tick: state.tick,
          kind: 'branch-query',
          nodeId: node.id,
          passed: true,
          actual: actual,
          compareMode: query.query.compareMode,
          target: query.query.target,
          scannerDepth: branchDepth,
          resynchronization: parserResynchronization
        });
        return;
      }
      var destination = parserResynchronization
        ? nextBarrier(node) : normalFailureBarrier(node, branchDepth);
      branchDepth -= 1;
      if (!destination) {
        missing('Failed Director query ' + node.id +
          ' has no following native branch barrier.');
        state.terminal = true;
        return;
      }
      var resumeNode = installCursorAtPrimitive(destination.resumePrimitiveIndex);
      trace.push({
        tick: state.tick,
        kind: 'branch-query',
        nodeId: node.id,
        passed: false,
        actual: actual,
        compareMode: query.query.compareMode,
        target: query.query.target,
        scannerDepth: branchDepth + 1,
        resynchronization: parserResynchronization,
        destinationNodeId: destination.node.id,
        destinationWord: destination.node.startWord,
        resumeNodeId: resumeNode && resumeNode.id || null,
        resumeWord: resumeNode && resumeNode.startWord || null,
        bridgeNodeId: destination.bridgeNode ? destination.bridgeNode.id : null
      });
    }

    var compositeIndex = 0;
    var compositeEntryNodeId = null;
    var cursorRevision = 0;
    var persistentCursorPrimitiveIndex = 0;
    var block = null;
    var parserResynchronization = false;
    var branchDepth = 0;
    var parserResumeMarked = false;
    var pendingSubstreamSelector = 0xFF;
    var savedStreamFrame = null;

    function activateStream(nextProgram, assetId, primitiveIndex) {
      activeProgram = nextProgram;
      activeStreamAssetId = assetId;
      indexActiveProgram();
      compositeIndex = 0;
      compositeEntryNodeId = null;
      persistentCursorPrimitiveIndex = primitiveIndex;
      return installCursorAtPrimitive(primitiveIndex);
    }

    function consumePendingSubstream(bridgeNode) {
      var selector = pendingSubstreamSelector;
      pendingSubstreamSelector = 0xFF;
      if (selector === 0xFF) return false;
      if (selector === 0xFE) {
        if (!savedStreamFrame) {
          missing('Director continuation return at ' + bridgeNode.id +
            ' has no saved parent stream.');
          state.terminal = true;
          return false;
        }
        var childAssetId = activeStreamAssetId;
        var restored = savedStreamFrame;
        savedStreamFrame = null;
        activateStream(restored.program, restored.assetId,
          restored.persistentCursorPrimitiveIndex);
        var returnDestination = persistentCursorNode();
        trace.push({
          tick: state.tick,
          kind: 'director-substream-return',
          sourceNodeId: bridgeNode.id,
          childAssetId: childAssetId,
          destinationAssetId: activeStreamAssetId,
          destinationNodeId: returnDestination ? returnDestination.id : null,
          destinationWord: returnDestination ? returnDestination.startWord : null
        });
        return true;
      }
      var childProgram = continuationProgram(selector);
      if (!childProgram) {
        missing('Director continuation selector ' + selector +
          ' at ' + bridgeNode.id + ' could not be materialized from the ROM.');
        return false;
      }
      var parentAssetId = activeStreamAssetId;
      if (!savedStreamFrame) {
        savedStreamFrame = {
          program: activeProgram,
          assetId: activeStreamAssetId,
          persistentCursorPrimitiveIndex: persistentCursorPrimitiveIndex
        };
      }
      activateStream(childProgram, 'director-continuation:' + selector, 0);
      var callDestination = persistentCursorNode();
      trace.push({
        tick: state.tick,
        kind: 'director-substream-call',
        sourceNodeId: bridgeNode.id,
        selector: selector,
        parentAssetId: parentAssetId,
        childAssetId: activeStreamAssetId,
        destinationNodeId: callDestination ? callDestination.id : null,
        destinationWord: callDestination ? callDestination.startWord : null
      });
      return true;
    }

    function persistentCursorNode() {
      return activeProgram.primitives[persistentCursorPrimitiveIndex] || null;
    }

    function commitPersistentCursorAfter(node) {
      var primitiveIndex = primitiveIndexById[node.id];
      persistentCursorPrimitiveIndex = Number.isInteger(primitiveIndex)
        ? primitiveIndex + 1 : persistentCursorPrimitiveIndex;
      var destination = persistentCursorNode();
      trace.push({
        tick: state.tick,
        kind: 'parser-resume-commit',
        sourceNodeId: node.id,
        destinationNodeId: destination && destination.id || null,
        destinationWord: destination && destination.startWord || null
      });
    }

    function restartAtPersistentCursor() {
      parserResumeMarked = false;
      return installCursorAtPrimitive(persistentCursorPrimitiveIndex);
    }

    function beginTick(tick) {
      state.tick = tick;
      parserResynchronization = false;
      branchDepth = 0;
      parserResumeMarked = false;
      pendingSubstreamSelector = 0xFF;
      state.audioEvents = [];
      state.cameraEvents = [];
      state.effectEvents = [];
      state.flowEvents = [];
      applyContextTimeline(tick);
      if (tick > 0) updateJobs();
      var scheduled = state.scheduled.filter(function(item) { return item.tick === tick; });
      state.scheduled = state.scheduled.filter(function(item) { return item.tick !== tick; });
      scheduled.forEach(function(item) { executePrimitive(item.node); });
    }

    function blockComplete(activeBlock) {
      if (!activeBlock) return true;
      if (activeBlock.kind === 'until') return state.tick >= activeBlock.untilTick;
      if (activeBlock.kind === 'parser-boundary') {
        return state.tick >= activeBlock.untilTick;
      }
      if (activeBlock.kind === 'cursor-replacement') {
        return state.tick >= activeBlock.untilTick;
      }
      if (activeBlock.kind === 'job') return !jobActiveFor(activeBlock.start);
      if (activeBlock.kind === 'query') {
        if (!queryEnabled(activeBlock.query)) return true;
        branchDepth += 1;
        var actual = queryActual(activeBlock.query, { kind: 'wait' });
        var passes = compare(actual, activeBlock.query.query.compareMode,
          activeBlock.query.query.target);
        if (!passes) branchDepth -= 1;
        return passes;
      }
      return true;
    }

    function executeNodes(nodes) {
      var revision = cursorRevision;
      for (var index = 0; index < nodes.length; index++) {
        executePrimitive(nodes[index]);
        if (block || state.terminal || cursorRevision !== revision) break;
      }
    }

    function activateQueryBlock(query, composite, resumeNodes) {
      block = {
        kind: 'query',
        query: query,
        label: composite.label,
        clock: composite.clock,
        resumeNodes: (resumeNodes || []).slice(),
        resumeComposite: composite
      };
      if (!blockComplete(block)) return;
      var completed = block;
      block = null;
      if (completed.resumeNodes.length) {
        processCompositeSuffix(completed.resumeNodes, completed.resumeComposite);
      }
    }

    function processCompositeSuffix(nodes, composite) {
      var revision = cursorRevision;
      for (var index = 0; index < nodes.length; index++) {
        var node = nodes[index];
        if (node.query) {
          activateQueryBlock(node, composite, nodes.slice(index + 1));
          return;
        }
        if (node.name === 'handoff_marker') {
          parserResumeMarked = true;
          continue;
        }
        if (node.name === 'branch_barrier') continue;
        if (node.name === 'control_bridge_and_pending_substream_handoff') {
          if (parserResumeMarked) commitPersistentCursorAfter(node);
          consumePendingSubstream(node);
          block = {
            kind: 'parser-boundary',
            untilTick: state.tick + 1,
            label: composite.label,
            clock: 'director-evaluation'
          };
          return;
        }
        executePrimitive(node);
        if (block || state.terminal || cursorRevision !== revision) return;
      }
    }

    function processComposite(composite, entryNodeId) {
      var allNodes = composite.nodeIds.map(function(id) {
        return activeProgram.primitiveById[id];
      });
      var entryOffset = entryNodeId
        ? allNodes.findIndex(function(node) { return node.id === entryNodeId; }) : 0;
      if (entryOffset < 0) {
        fail('Director cursor entry ' + entryNodeId + ' is outside ' + composite.id + '.',
          'cursor-ownership');
      }
      var nodes = allNodes.slice(entryOffset);
      var first = nodes[0];
      trace.push({
        tick: state.tick,
        kind: 'composite',
        compositeId: composite.id,
        compositeKind: composite.kind,
        label: composite.label,
        category: composite.category
      });
      if (entryOffset > 0) {
        processCompositeSuffix(nodes, composite);
        return;
      }
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
              node: activeProgram.primitiveById[nodeId]
            });
          });
        }
        block = { kind: 'until', untilTick: state.tick + Math.max(0, composite.nativeTicks),
          label: composite.label, clock: composite.clock };
        return;
      }
      if (composite.kind === 'start-and-completion-gate') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      if (composite.kind === 'dialogue-window-open') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      if (composite.kind === 'dialogue-window-resume-close') {
        executeNodes(nodes);
        return;
      }
      if (composite.kind === 'query-envelope') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      executeNodes(nodes);
    }

    for (var tick = 0; tick < maxTicks; tick++) {
      beginTick(tick);
      if (block && blockComplete(block)) {
        var completedBlock = block;
        if (completedBlock.kind === 'until') {
          state.registeredCounter = null;
          branchDepth = 1;
        }
        block = null;
        if (completedBlock.kind === 'parser-boundary') {
          restartAtPersistentCursor();
        } else if (completedBlock.resumeNodes && completedBlock.resumeNodes.length) {
          processCompositeSuffix(completedBlock.resumeNodes,
            completedBlock.resumeComposite);
        }
      }
      var instantGuard = 0;
      while (!block && !state.terminal &&
          compositeIndex < activeProgram.composites.length) {
        if (++instantGuard > activeProgram.composites.length + 8) {
          fail('Director runtime exceeded its instantaneous-dispatch guard.', 'dispatch-loop');
        }
        var composite = activeProgram.composites[compositeIndex++];
        var entryNodeId = compositeEntryNodeId;
        compositeEntryNodeId = null;
        processComposite(composite, entryNodeId);
      }
      states.push(snapshot(block));
      if (state.terminal || compositeIndex >= activeProgram.composites.length && !block &&
          !Object.keys(state.movementJobs).length && !state.projectionJob &&
          !state.oversizedImageTransitionJob) break;
    }

    if (!state.terminal && states.length >= maxTicks) {
      missing('Director preview reached the ' + maxTicks + '-tick safety limit.');
    }
    if (!states.length) states.push(snapshot(null));
    var previewFitBounds = null;
    states.forEach(function(frameState) {
      frameState.actors.forEach(function(actor) {
        if (!actor.visible || !Number.isFinite(actor.x) || !Number.isFinite(actor.z)) return;
        if (!previewFitBounds) {
          previewFitBounds = { xMin: actor.x, xMax: actor.x, zMin: actor.z, zMax: actor.z };
          return;
        }
        previewFitBounds.xMin = Math.min(previewFitBounds.xMin, actor.x);
        previewFitBounds.xMax = Math.max(previewFitBounds.xMax, actor.x);
        previewFitBounds.zMin = Math.min(previewFitBounds.zMin, actor.z);
        previewFitBounds.zMax = Math.max(previewFitBounds.zMax, actor.z);
      });
    });
    states.forEach(function(frameState) {
      frameState.durationFrames = states.length;
      frameState.runtime.assumptionCount = assumptions.length;
      frameState.runtime.missingInputCount = missingInputs.length;
      frameState.runtime.status = missingInputs.length ? 'missing-inputs' :
        (assumptions.length ? 'assumed-inputs' : 'profiled');
      if (previewFitBounds && frameState.actorProjection &&
          frameState.actorProjection.evidenceStatus === 'external-unresolved') {
        // The native launch camera is absent, so keep one honest preview fit
        // across the complete runtime instead of clamping moving Actors against
        // the static SceneDocument bounds or reframing every tick.
        frameState.actorProjection.previewFitBounds = Object.assign({}, previewFitBounds);
      }
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
      sourcePrimitiveCount: rootProgram.primitives.length,
      executedPrimitiveCount: state.executedNodeIds.length,
      concurrentContext: contextRuntime ? {
        assetId: contextRuntime.assetId,
        tickOffset: contextTickOffset,
        evidenceStatus: 'native-static-parent-event-request-order'
      } : null,
      launchSceneStatePolicy: modeTwoCommandPreviewUsesFreshRoot
        ? 'mode-two-zero-loader-preview-clears-scene-root'
        : 'launch-may-inherit-existing-scene-root',
      launchStageTransform: M.cloneJson(
        launchProfile.stageTransform, 'launch Stage transform profile'),
      launchOperandTranslation: {
        required: translationProfile.required === true,
        requiredIndexes: (translationProfile.tableIndexes || []).slice(),
        suppliedIndexes: suppliedTranslationIndexes.slice(),
        missingIndexes: missingTranslationIndexes.slice(),
        status: missingTranslationIndexes.length ? 'missing-inputs' :
          (translationProfile.required ? 'resolved' : 'not-required')
      },
      terminated: state.terminal,
      terminationReason: state.terminalReason,
      safetyLimited: states.length >= maxTicks && !state.terminal
    };
  }

  function compactContextRuntime(runtime) {
    if (!runtime) fail('A Director runtime is required for context compaction.',
      'context-runtime');
    if (Array.isArray(runtime.contextFrames) && runtime.contextFrames.length) {
      return runtime;
    }
    if (!Array.isArray(runtime.states) || !runtime.states.length) {
      fail('The Director runtime has no states to compact.', 'context-runtime');
    }
    var actorFields = [
      'id', 'label', 'artSourceId', 'capability', 'visible',
      'opacityByte', 'renderModeByte', 'baseX', 'baseY', 'baseZ',
      'secondaryY', 'heightModeByte', 'facing', 'poseId', 'bank',
      'animationKey', 'nativeFacing', 'variantSelector', 'poseFrame',
      'poseProgramStatus', 'poseLoop', 'poseDuration', 'bodyPoseProgram',
      'movementFrame', 'activeMovementId', 'nativeUniformScale', 'tint',
      'yawDegrees', 'transformChannel', 'source'
    ];

    function same(left, right) {
      if (left === right) return true;
      if (left == null || right == null ||
          typeof left !== 'object' || typeof right !== 'object') return false;
      return JSON.stringify(left) === JSON.stringify(right);
    }

    function bySlot(actors) {
      var output = {};
      (actors || []).forEach(function(actor) { output[actor.slot] = actor; });
      return output;
    }

    function dialogueWindowId(row) {
      var payload = row && row.payload || {};
      var match = String(row && row.label || '').match(/(\d+)$/);
      return Number.isInteger(payload.windowId)
        ? payload.windowId : Number(match && match[1]);
    }

    function byWindow(rows) {
      var output = {};
      (rows || []).forEach(function(row) {
        var windowId = dialogueWindowId(row);
        if (Number.isInteger(windowId)) output[windowId] = row;
      });
      return output;
    }

    var contextFrames = [];
    var prior = null;
    runtime.states.forEach(function(frameState) {
      var delta = {};
      var currentActors = bySlot(frameState.actors);
      var priorActors = bySlot(prior && prior.actors);
      var actorChanges = [];
      Object.keys(currentActors).forEach(function(slot) {
        var current = currentActors[slot];
        var previous = priorActors[slot];
        if (!previous) {
          actorChanges.push(current);
          return;
        }
        var actorDelta = { slot: current.slot };
        actorFields.forEach(function(field) {
          if (!same(current[field], previous[field])) actorDelta[field] = current[field];
        });
        if (Object.keys(actorDelta).length > 1) actorChanges.push(actorDelta);
      });
      if (actorChanges.length) delta.actors = actorChanges;
      var removedActorSlots = Object.keys(priorActors).filter(function(slot) {
        return !currentActors[slot];
      }).map(Number);
      if (removedActorSlots.length) delta.removedActorSlots = removedActorSlots;

      var currentDialogue = byWindow(frameState.dialogue);
      var priorDialogue = byWindow(prior && prior.dialogue);
      var dialogueChanges = Object.keys(currentDialogue).filter(function(windowId) {
        return !priorDialogue[windowId] ||
          !same(currentDialogue[windowId], priorDialogue[windowId]);
      }).map(function(windowId) { return currentDialogue[windowId]; });
      if (dialogueChanges.length) delta.dialogue = dialogueChanges;
      var removedDialogueWindowIds = Object.keys(priorDialogue).filter(function(windowId) {
        return !currentDialogue[windowId];
      }).map(Number);
      if (removedDialogueWindowIds.length) {
        delta.removedDialogueWindowIds = removedDialogueWindowIds;
      }

      [
        'background', 'transformChannels', 'cameraState', 'actorProjection',
        'registeredProjection', 'sceneColor', 'overlays', 'sceneVignette',
        'oversizedImageView'
      ].forEach(function(field) {
        if (!prior || !same(frameState[field], prior[field])) {
          delta[field] = frameState[field];
        }
      });
      contextFrames.push(Object.keys(delta).length ? delta : null);
      prior = frameState;
    });
    return {
      assetId: runtime.assetId,
      engine: 'director-context-delta-timeline',
      durationTicks: runtime.durationTicks,
      contextFrames: contextFrames,
      terminated: runtime.terminated,
      safetyLimited: runtime.safetyLimited,
      sourceRuntimeEngine: runtime.engine,
      evidenceStatus: 'lossless-runtime-state-delta'
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
    compactContextRuntime: compactContextRuntime,
    bind: bind,
    unbind: unbind,
    forDocument: forDocument,
    evaluate: evaluate,
    projectionFromCamera: projectionFromCamera,
    decodeSceneTransformResource: decodeSceneTransformResource
  });
})(window.OB64);
