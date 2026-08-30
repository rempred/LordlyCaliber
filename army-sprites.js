// OB64 Mod Editor - Army Management and battle formation sprite codec
//
// Each verified resource contains two fixed RGBA5551 palettes and a set of
// CI8 sprite planes. Existing planes can rebuild in place. Missing player
// planes use the same routed model IDs and require a relocated, expanded atlas.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var A = OB64.art;
  var PALETTE_WORDS = 256;
  var PALETTE_BYTES = 0x200;
  var PALETTE_COUNT = 2;
  var PIXEL_BASE = 0x400;
  var CLASS_ROUTE = 0x00064BE0;
  var CLASS_COUNT = 0xA5;
  var CLASS_ROUTE_SIZE = 6;
  var CLASS_SELECTOR_OFFSET = 4;
  var CLASS_DEFINITION = 0x0005DAD8;
  var CLASS_DEFINITION_SIZE = 72;
  var CLASS_MODEL_SIZE_OFFSET = 64;

  var SPECS = [
    {
      key: 'enemy-front-ordinary', label: 'Enemy / Front · Ordinary',
      lane: 'ordinary', side: 'enemy', orientation: 'front-facing',
      resourceKey: 0x01DBB2AC, sizeWord: 0x0234F52C,
      retailStoredLength: 0x3C5F, retailDecodedLength: 0x5800,
      retailModelCount: 56, targetModelCount: 56,
      width: 16, height: 24, tailLength: 0,
      transparentIndices: [0]
    },
    {
      key: 'player-back-ordinary', label: 'Player / Back · Ordinary',
      lane: 'ordinary', side: 'player', orientation: 'back-facing',
      resourceKey: 0x01DBEF10, sizeWord: 0x02353190,
      retailStoredLength: 0x3DFB, retailDecodedLength: 0x5A00,
      retailModelCount: 56, targetModelCount: 56,
      width: 16, height: 24, tailLength: 0x200,
      transparentIndices: [70]
    },
    {
      key: 'enemy-front-special', label: 'Enemy / Front · Special',
      lane: 'special', side: 'enemy', orientation: 'front-facing',
      resourceKey: 0x01DC3320, sizeWord: 0x023575A0,
      retailStoredLength: 0x2B47, retailDecodedLength: 0x3D00,
      retailModelCount: 38, targetModelCount: 38,
      width: 16, height: 24, tailLength: 0,
      transparentIndices: [255]
    },
    {
      key: 'player-back-special', label: 'Player / Back · Special',
      lane: 'special', side: 'player', orientation: 'back-facing',
      resourceKey: 0x01DC5E6C, sizeWord: 0x0235A0EC,
      retailStoredLength: 0x1C3C, retailDecodedLength: 0x2800,
      retailModelCount: 24, targetModelCount: 38,
      width: 16, height: 24, tailLength: 0,
      ownerSites: [
        { name: 'player-special-model-cache',
          lui: 0x0020942C, ori: 0x00209438 },
        { name: 'player-special-palette-cache',
          lui: 0x00209668, ori: 0x0020966C }
      ],
      transparentIndices: [255]
    },
    {
      key: 'enemy-front-large', label: 'Enemy / Front · Large',
      lane: 'large', side: 'enemy', orientation: 'front-facing',
      resourceKey: 0x01DB47A0, sizeWord: 0x02348A20,
      retailStoredLength: 0x37B7, retailDecodedLength: 0x5F00,
      retailModelCount: 26, targetModelCount: 26,
      width: 32, height: 28, tailLength: 0,
      transparentIndices: [0]
    },
    {
      key: 'player-back-large', label: 'Player / Back · Large',
      lane: 'large', side: 'player', orientation: 'back-facing',
      resourceKey: 0x01DB7F5C, sizeWord: 0x0234C1DC,
      retailStoredLength: 0x334C, retailDecodedLength: 0x5B80,
      retailModelCount: 25, targetModelCount: 26,
      width: 32, height: 28, tailLength: 0,
      ownerSites: [
        { name: 'player-large-model-cache',
          lui: 0x002093F4, ori: 0x00209400 },
        { name: 'player-large-palette-cache',
          lui: 0x00209728, ori: 0x0020972C }
      ],
      transparentIndices: [0, 177, 178, 179, 243, 244, 245, 246,
        247, 248, 249, 250, 251, 252, 253, 254, 255]
    }
  ];

  function ArmySpriteError(message) {
    this.name = 'ArmySpriteError';
    this.message = message;
  }
  ArmySpriteError.prototype = Object.create(Error.prototype);
  ArmySpriteError.prototype.constructor = ArmySpriteError;

  function fail(message) { throw new ArmySpriteError(message); }

  function hex(value, width) {
    return '0x' + (Number(value) >>> 0).toString(16).toUpperCase()
      .padStart(width || 8, '0');
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index] !== right[index]) return false;
    }
    return true;
  }

  function toBase64(bytes) {
    var parts = [];
    for (var start = 0; start < bytes.length; start += 0x8000) {
      var chars = '';
      var slice = bytes.subarray(start, Math.min(bytes.length, start + 0x8000));
      for (var index = 0; index < slice.length; index++) {
        chars += String.fromCharCode(slice[index]);
      }
      parts.push(chars);
    }
    return btoa(parts.join(''));
  }

  function fromBase64(text, length, label) {
    if (typeof text !== 'string' || !text.length) {
      fail(label + ' is not base64 text');
    }
    var raw;
    try { raw = atob(text); }
    catch (error) { fail(label + ' is not valid base64'); }
    if (raw.length !== length) {
      fail(label + ' has ' + raw.length + ' bytes; expected ' + length);
    }
    var bytes = new Uint8Array(length);
    for (var index = 0; index < length; index++) {
      bytes[index] = raw.charCodeAt(index);
    }
    return bytes;
  }

  function cleanName(value, fallback) {
    var text = String(value || '').replace(/[\x00-\x1F\x7F]/g, '').trim();
    return text || fallback;
  }

  function modelKey(atlasKey, modelId) {
    return atlasKey + ':' + Number(modelId).toString(16).padStart(2, '0');
  }

  function routeLane(selector, modelSize) {
    return modelSize === 2
      ? 'large' : ((selector & 0x80) ? 'special' : 'ordinary');
  }

  function classRoutes(z64) {
    var rows = [];
    for (var classId = 0; classId < CLASS_COUNT; classId++) {
      var selector = z64[CLASS_ROUTE + classId * CLASS_ROUTE_SIZE +
        CLASS_SELECTOR_OFFSET];
      var modelSize = z64[CLASS_DEFINITION + classId * CLASS_DEFINITION_SIZE +
        CLASS_MODEL_SIZE_OFFSET];
      var lane = routeLane(selector, modelSize);
      rows.push({
        classId: classId,
        className: cleanName(OB64.className ? OB64.className(classId) : '',
          'Class ' + hex(classId, 2)),
        lane: lane,
        modelId: selector & 0x3F,
        selector: selector,
        modelSize: modelSize
      });
    }
    return rows;
  }

  function paletteWords(decoded, paletteIndex) {
    var words = new Uint16Array(PALETTE_WORDS);
    var start = paletteIndex * PALETTE_BYTES;
    for (var index = 0; index < PALETTE_WORDS; index++) {
      words[index] = A.readU16(decoded, start + index * 2);
    }
    return words;
  }

  function transparentIndices(words) {
    var output = [];
    for (var index = 0; index < words.length; index++) {
      if (!(words[index] & 1)) output.push(index);
    }
    return output;
  }

  function parseAtlas(z64, spec, routes) {
    var activeKey = spec.ownerSites
      ? A.resolveSplitKey(z64, spec.ownerSites, spec.label)
      : spec.resourceKey;
    var relocatedSource = activeKey !== spec.resourceKey;
    var resource = A.readCompressedResource(z64, activeKey);
    if (!relocatedSource && resource.entry !== spec.sizeWord) {
      fail(spec.label + ' size word is ' + hex(resource.entry) +
        '; expected ' + hex(spec.sizeWord));
    }
    if (relocatedSource && (!spec.ownerSites ||
        resource.entry < A.constants.ARENA_START ||
        resource.entry >= A.constants.ARENA_END)) {
      fail(spec.label + ' relocated resource lies outside the native-art arena');
    }
    var resourceCapacity = relocatedSource
      ? resource.storedLength + (resource.storedLength & 1)
      : spec.retailStoredLength + (spec.retailStoredLength & 1);
    if (!relocatedSource && resource.storedLength > resourceCapacity) {
      fail(spec.label + ' stored length is ' + resource.storedLength +
        '; verified envelope is ' + resourceCapacity);
    }
    var planeSize = spec.width * spec.height;
    var expectedSourceLength = relocatedSource
      ? PIXEL_BASE + spec.targetModelCount * planeSize + spec.tailLength
      : spec.retailDecodedLength;
    if (resource.decoded.length !== expectedSourceLength) {
      fail(spec.label + ' decoded length is ' + resource.decoded.length +
        '; expected ' + expectedSourceLength);
    }
    var sourceModelCount = relocatedSource
      ? spec.targetModelCount : spec.retailModelCount;
    var expectedLength = PIXEL_BASE + sourceModelCount * planeSize +
      spec.tailLength;
    if (expectedLength !== resource.decoded.length) {
      fail(spec.label + ' plane layout does not cover its decoded resource');
    }
    var palettes = [];
    for (var paletteIndex = 0; paletteIndex < PALETTE_COUNT; paletteIndex++) {
      var palette = paletteWords(resource.decoded, paletteIndex);
      var observedTransparent = transparentIndices(palette);
      if (observedTransparent.join(',') !== spec.transparentIndices.join(',')) {
        fail(spec.label + ' palette ' + paletteIndex +
          ' transparent indices differ from the verified layout');
      }
      palettes.push(palette);
    }
    var models = [];
    for (var modelId = 0; modelId < spec.targetModelCount; modelId++) {
      var start = PIXEL_BASE + modelId * planeSize;
      var consumers = routes.filter(function(route) {
        return route.lane === spec.lane && route.modelId === modelId;
      });
      var sourcePresent = modelId < sourceModelCount;
      var blankIndices = new Uint8Array(planeSize);
      blankIndices.fill(spec.transparentIndices[0]);
      models.push({
        key: modelKey(spec.key, modelId),
        atlasKey: spec.key,
        modelId: modelId,
        decodedOffset: start,
        retailPresent: modelId < spec.retailModelCount,
        sourcePresent: sourcePresent,
        classIds: consumers.map(function(route) { return route.classId; }),
        classNames: consumers.map(function(route) { return route.className; }),
        originalIndices: sourcePresent
          ? resource.decoded.slice(start, start + planeSize) : null,
        blankIndices: blankIndices
      });
    }
    return {
      key: spec.key,
      label: spec.label,
      lane: spec.lane,
      side: spec.side,
      orientation: spec.orientation,
      retailResourceKey: spec.resourceKey,
      resourceKey: activeKey,
      retailSizeWord: spec.sizeWord,
      sizeWord: resource.entry,
      width: spec.width,
      height: spec.height,
      modelCount: spec.targetModelCount,
      retailModelCount: spec.retailModelCount,
      sourceModelCount: sourceModelCount,
      targetModelCount: spec.targetModelCount,
      tailLength: spec.tailLength,
      transparentIndices: spec.transparentIndices.slice(),
      ownerSites: (spec.ownerSites || []).map(function(site) {
        return Object.assign({}, site);
      }),
      relocatedSource: relocatedSource,
      resource: resource,
      originalCapacity: resourceCapacity,
      palettes: palettes,
      models: models,
      nearestCache: [{}, {}]
    };
  }

  function initialize(z64) {
    var state = {
      supported: false,
      unavailableReason: '',
      atlases: [],
      byKey: {},
      models: [],
      retailModels: [],
      sourceModels: [],
      byModelKey: {},
      edits: {},
      history: {}
    };
    try {
      var routes = classRoutes(z64);
      SPECS.forEach(function(spec) {
        var atlas = parseAtlas(z64, spec, routes);
        state.atlases.push(atlas);
        state.byKey[atlas.key] = atlas;
        atlas.models.forEach(function(model) {
          state.models.push(model);
          if (model.retailPresent) state.retailModels.push(model);
          if (model.sourcePresent) state.sourceModels.push(model);
          state.byModelKey[model.key] = model;
        });
      });
      state.classRoutes = routes;
      state.byClassId = {};
      routes.forEach(function(route) {
        route.playerAtlasKey = 'player-back-' + route.lane;
        route.enemyAtlasKey = 'enemy-front-' + route.lane;
        route.playerModel = state.byModelKey[
          modelKey(route.playerAtlasKey, route.modelId)];
        route.enemyModel = state.byModelKey[
          modelKey(route.enemyAtlasKey, route.modelId)];
        if (!route.playerModel || !route.enemyModel) {
          fail(route.className + ' Army sprite route lies outside the supported model range');
        }
        route.playerMissingInRetail = !route.playerModel.retailPresent;
        route.enemyMissingInRetail = !route.enemyModel.retailPresent;
        state.byClassId[route.classId] = route;
      });
      state.supported = state.atlases.length === SPECS.length;
    } catch (error) {
      state.unavailableReason = 'Army sprite editing is unavailable: ' +
        (error && error.message ? error.message : String(error));
    }
    return state;
  }

  function atlasFor(state, atlasKey) {
    var atlas = state && state.byKey && state.byKey[atlasKey];
    if (!atlas) fail('unknown Army sprite atlas ' + atlasKey);
    return atlas;
  }

  function modelFor(state, atlasKey, modelId) {
    var atlas = atlasFor(state, atlasKey);
    modelId = Number(modelId);
    if (!Number.isInteger(modelId) || modelId < 0 ||
        modelId >= atlas.targetModelCount) {
      fail(atlas.label + ' model ID is outside its routed plane range');
    }
    return atlas.models[modelId];
  }

  function removeRouteConsumer(model, classId) {
    var index = model.classIds.indexOf(classId);
    if (index < 0) return;
    model.classIds.splice(index, 1);
    model.classNames.splice(index, 1);
  }

  function addRouteConsumer(model, route) {
    if (model.classIds.indexOf(route.classId) >= 0) return;
    var index = 0;
    while (index < model.classIds.length &&
        model.classIds[index] < route.classId) index++;
    model.classIds.splice(index, 0, route.classId);
    model.classNames.splice(index, 0, route.className);
  }

  function routeModelsForLane(state, route, lane) {
    var playerAtlasKey = 'player-back-' + lane;
    var enemyAtlasKey = 'enemy-front-' + lane;
    var playerModel = state.byModelKey[modelKey(playerAtlasKey, route.modelId)];
    var enemyModel = state.byModelKey[modelKey(enemyAtlasKey, route.modelId)];
    if (!playerModel || !enemyModel) {
      fail(route.className + ' model ' + hex(route.modelId, 2) +
        ' is outside the ' + lane + ' Army sprite range');
    }
    return {
      playerAtlasKey: playerAtlasKey,
      enemyAtlasKey: enemyAtlasKey,
      playerModel: playerModel,
      enemyModel: enemyModel
    };
  }

  function setClassModelSize(state, classId, modelSize) {
    classId = Number(classId);
    modelSize = Number(modelSize);
    var route = state && state.byClassId && state.byClassId[classId];
    if (!route) fail('unknown Army sprite class route ' + hex(classId, 2));
    if (!Number.isInteger(modelSize) || modelSize < 0 || modelSize > 0xFF) {
      fail(route.className + ' class size is outside the byte range');
    }
    var lane = routeLane(route.selector, modelSize);
    var resolved = routeModelsForLane(state, route, lane);
    var changed = route.modelSize !== modelSize || route.lane !== lane;
    if (!changed) return false;
    if (route.playerModel) removeRouteConsumer(route.playerModel, route.classId);
    if (route.enemyModel) removeRouteConsumer(route.enemyModel, route.classId);
    route.modelSize = modelSize;
    route.lane = lane;
    route.playerAtlasKey = resolved.playerAtlasKey;
    route.enemyAtlasKey = resolved.enemyAtlasKey;
    route.playerModel = resolved.playerModel;
    route.enemyModel = resolved.enemyModel;
    route.playerMissingInRetail = !route.playerModel.retailPresent;
    route.enemyMissingInRetail = !route.enemyModel.retailPresent;
    addRouteConsumer(route.playerModel, route);
    addRouteConsumer(route.enemyModel, route);
    return true;
  }

  function syncClassModelSizes(state, classDefs) {
    if (!state || !state.supported || !Array.isArray(classDefs)) return 0;
    var changed = 0;
    state.classRoutes.forEach(function(route) {
      var classDef = classDefs[route.classId + 1];
      if (!classDef || !Number.isInteger(classDef.unitSize)) return;
      if (setClassModelSize(state, route.classId, classDef.unitSize)) changed++;
    });
    return changed;
  }

  function currentIndices(state, atlasKey, modelId) {
    var model = modelFor(state, atlasKey, modelId);
    return state.edits[model.key] || model.originalIndices || model.blankIndices;
  }

  function hasCurrentPlane(state, atlasKey, modelId) {
    var model = modelFor(state, atlasKey, modelId);
    return model.sourcePresent || !!state.edits[model.key];
  }

  function validateIndices(atlas, indices, label) {
    var length = atlas.width * atlas.height;
    if (!(indices instanceof Uint8Array) || indices.length !== length) {
      fail(label + ' must contain exactly ' + length + ' CI8 indices');
    }
  }

  function historyFor(state, atlasKey, modelId) {
    var model = modelFor(state, atlasKey, modelId);
    if (!state.history[model.key]) {
      state.history[model.key] = { undo: [], redo: [] };
    }
    return state.history[model.key];
  }

  function setEdit(state, atlasKey, modelId, indices, options) {
    options = options || {};
    var atlas = atlasFor(state, atlasKey);
    var model = modelFor(state, atlasKey, modelId);
    validateIndices(atlas, indices, model.key);
    var current = currentIndices(state, atlasKey, modelId);
    var currentPresent = hasCurrentPlane(state, atlasKey, modelId);
    if (equalBytes(current, indices) &&
        (currentPresent || options.create !== true)) return false;
    if (options.history !== false) {
      var history = historyFor(state, atlasKey, modelId);
      history.undo.push(current.slice());
      if (history.undo.length > 100) history.undo.shift();
      history.redo = [];
    }
    if (model.originalIndices && equalBytes(model.originalIndices, indices)) {
      delete state.edits[model.key];
    } else {
      state.edits[model.key] = indices.slice();
    }
    return true;
  }

  function undo(state, atlasKey, modelId) {
    var history = historyFor(state, atlasKey, modelId);
    if (!history.undo.length) return false;
    var current = currentIndices(state, atlasKey, modelId).slice();
    var prior = history.undo.pop();
    history.redo.push(current);
    return setEdit(state, atlasKey, modelId, prior, { history: false });
  }

  function redo(state, atlasKey, modelId) {
    var history = historyFor(state, atlasKey, modelId);
    if (!history.redo.length) return false;
    var current = currentIndices(state, atlasKey, modelId).slice();
    var next = history.redo.pop();
    history.undo.push(current);
    return setEdit(state, atlasKey, modelId, next, { history: false });
  }

  function createBlankPlane(state, atlasKey, modelId) {
    var model = modelFor(state, atlasKey, modelId);
    if (hasCurrentPlane(state, atlasKey, modelId)) return false;
    return setEdit(state, atlasKey, modelId, model.blankIndices,
      { create: true });
  }

  function resetModel(state, atlasKey, modelId) {
    var model = modelFor(state, atlasKey, modelId);
    if (model.originalIndices) {
      return setEdit(state, atlasKey, modelId, model.originalIndices);
    }
    if (!state.edits[model.key]) return false;
    delete state.edits[model.key];
    delete state.history[model.key];
    return true;
  }

  function renderWords(atlas, indices, paletteIndex) {
    paletteIndex = Number(paletteIndex);
    if (paletteIndex !== 0 && paletteIndex !== 1) {
      fail(atlas.label + ' palette index must be 0 or 1');
    }
    validateIndices(atlas, indices, atlas.label + ' display plane');
    var palette = atlas.palettes[paletteIndex];
    var words = new Uint16Array(indices.length);
    for (var pixel = 0; pixel < indices.length; pixel++) {
      words[pixel] = palette[indices[pixel]];
    }
    return words;
  }

  function rgbaPixels(atlas, indices, paletteIndex) {
    var words = renderWords(atlas, indices, paletteIndex);
    var output = new Uint8ClampedArray(words.length * 4);
    for (var pixel = 0; pixel < words.length; pixel++) {
      output.set(A.rgba5551(words[pixel]), pixel * 4);
    }
    return output;
  }

  function nearestPaletteIndex(atlas, paletteIndex, red, green, blue) {
    var palette = atlas.palettes[paletteIndex];
    var bestIndex = -1;
    var bestDistance = Infinity;
    for (var index = 0; index < palette.length; index++) {
      var word = palette[index];
      if (!(word & 1)) continue;
      var dr = red - ((word >>> 11) & 31);
      var dg = green - ((word >>> 6) & 31);
      var db = blue - ((word >>> 1) & 31);
      var distance = dr * dr + dg * dg + db * db;
      if (distance < bestDistance) {
        bestIndex = index;
        bestDistance = distance;
        if (!distance) break;
      }
    }
    if (bestIndex < 0) fail(atlas.label + ' palette has no opaque colors');
    return bestIndex;
  }

  function prepareImageImport(source, atlas, paletteIndex, options) {
    options = options || {};
    paletteIndex = Number(paletteIndex);
    if (paletteIndex !== 0 && paletteIndex !== 1) {
      fail(atlas.label + ' import palette must be 0 or 1');
    }
    var prepared = A.prepareSpriteImageImport(
      source.rgba, source.width, source.height, atlas.width, atlas.height, {
        resizeMode: options.resizeMode || 'nearest',
        panX: options.panX,
        panY: options.panY,
        zoom: options.zoom,
        maximumColors: 256,
        dither: false
      });
    var indices = new Uint8Array(atlas.width * atlas.height);
    var transparent = atlas.transparentIndices[0];
    var bayer = [
      0, 8, 2, 10,
      12, 4, 14, 6,
      3, 11, 1, 9,
      15, 7, 13, 5
    ];
    var transparentPixels = 0;
    for (var pixel = 0; pixel < indices.length; pixel++) {
      var offset = pixel * 4;
      if (prepared.rgba[offset + 3] < 128) {
        indices[pixel] = transparent;
        transparentPixels++;
        continue;
      }
      var ditherOffset = options.dither
        ? (bayer[(pixel % atlas.width) % 4 +
          (Math.floor(pixel / atlas.width) % 4) * 4] - 7.5) / 8
        : 0;
      indices[pixel] = nearestPaletteIndex(atlas, paletteIndex,
        prepared.rgba[offset] * 31 / 255 + ditherOffset,
        prepared.rgba[offset + 1] * 31 / 255 + ditherOffset,
        prepared.rgba[offset + 2] * 31 / 255 + ditherOffset);
    }
    return {
      indices: indices,
      crop: prepared.crop,
      resizeMode: prepared.resizeMode,
      paletteIndex: paletteIndex,
      dithered: !!options.dither,
      transparentPixels: transparentPixels,
      sourceNativeColorCount: prepared.sourceNativeColorCount
    };
  }

  function collectProject(state) {
    var output = {};
    if (!state || !state.supported) return output;
    Object.keys(state.edits).sort().forEach(function(key) {
      var model = state.byModelKey[key];
      var atlas = state.byKey[model.atlasKey];
      output[key] = {
        atlas: atlas.key,
        modelId: model.modelId,
        retailPlane: model.retailPresent,
        resourceKey: hex(atlas.retailResourceKey),
        width: atlas.width,
        height: atlas.height,
        paletteRgba5551BeBase64: toBase64(
          atlas.resource.decoded.slice(0, PIXEL_BASE)),
        ci8IndicesBase64: toBase64(state.edits[key])
      };
    });
    return output;
  }

  function prepareProject(state, payload, schemaVersion) {
    payload = payload || {};
    if (typeof payload !== 'object' || Array.isArray(payload)) {
      fail('Army sprite Project records must be an object');
    }
    if (Object.keys(payload).length && (!state || !state.supported)) {
      fail('This ROM cannot load Army sprite Project records');
    }
    var edits = {};
    Object.keys(payload).forEach(function(key) {
      var entry = payload[key];
      var model = state.byModelKey[key];
      var atlas = model && state.byKey[model.atlasKey];
      if (model && !model.retailPresent && Number(schemaVersion) < 5) {
        fail('Custom missing Army sprite plane ' + key +
          ' requires patches.art schemaVersion 5');
      }
      if (!entry || !model || !atlas || entry.atlas !== atlas.key ||
          entry.modelId !== model.modelId ||
          (entry.retailPlane !== undefined &&
            entry.retailPlane !== model.retailPresent) ||
          entry.resourceKey !== hex(atlas.retailResourceKey) ||
          entry.width !== atlas.width || entry.height !== atlas.height ||
          entry.paletteRgba5551BeBase64 !== toBase64(
            atlas.resource.decoded.slice(0, PIXEL_BASE))) {
        fail('Army sprite Project record ' + key +
          ' does not match its verified resource plane and fixed palettes');
      }
      edits[key] = fromBase64(entry.ci8IndicesBase64,
        atlas.width * atlas.height, 'Army sprite Project record ' + key);
    });
    return { edits: edits, count: Object.keys(edits).length };
  }

  function applyPrepared(state, prepared) {
    var applied = 0;
    Object.keys(prepared && prepared.edits || {}).forEach(function(key) {
      var model = state.byModelKey[key];
      if (setEdit(state, model.atlasKey, model.modelId,
          prepared.edits[key], { create: true })) applied++;
    });
    return applied;
  }

  function editedModelsForAtlas(state, atlas) {
    return atlas.models.filter(function(model) { return !!state.edits[model.key]; });
  }

  function buildDecoded(state, atlas) {
    var needsExpansion = editedModelsForAtlas(state, atlas).some(
      function(model) { return !model.sourcePresent; });
    var decoded;
    if (!needsExpansion) {
      decoded = atlas.resource.decoded.slice();
    } else {
      var planeSize = atlas.width * atlas.height;
      var targetPixelEnd = PIXEL_BASE + atlas.targetModelCount * planeSize;
      decoded = new Uint8Array(targetPixelEnd + atlas.tailLength);
      decoded.fill(atlas.transparentIndices[0]);
      decoded.set(atlas.resource.decoded.slice(0, PIXEL_BASE), 0);
      var sourcePixelEnd = PIXEL_BASE + atlas.sourceModelCount * planeSize;
      decoded.set(atlas.resource.decoded.slice(PIXEL_BASE, sourcePixelEnd),
        PIXEL_BASE);
      if (atlas.tailLength) {
        decoded.set(atlas.resource.decoded.slice(sourcePixelEnd),
          targetPixelEnd);
      }
    }
    editedModelsForAtlas(state, atlas).forEach(function(model) {
      decoded.set(state.edits[model.key], model.decodedOffset);
    });
    return decoded;
  }

  function verifyCompressed(atlas, decoded, stored, placement) {
    var verified = A.bootLzDecode(stored);
    if (verified.bytesConsumed !== stored.length ||
        !equalBytes(verified.output, decoded)) {
      fail(atlas.label + ' compressed resource failed exact readback');
    }
    if (placement === 'in-place' && stored.length > atlas.originalCapacity) {
      fail('[Art export blocked] asset=' + atlas.label +
        '; check=in-place compressed capacity; observed=' + stored.length +
        ' bytes; expected<=' + atlas.originalCapacity +
        ' bytes; action=simplify the edited planes or restore this atlas');
    }
  }

  function builtRow(atlas, models, decoded, stored) {
    var expanded = decoded.length > atlas.resource.decoded.length;
    var placement = expanded ? 'relocated' : 'in-place';
    if (expanded && (!atlas.ownerSites.length || atlas.relocatedSource)) {
      fail('[Art export blocked] asset=' + atlas.label +
        '; check=missing-plane relocation owner; observed=unavailable;' +
        ' expected=verified retail owner sites; action=load a clean US Rev 0 ROM');
    }
    verifyCompressed(atlas, decoded, stored, placement);
    return {
      name: 'army-sprite-' + atlas.key,
      atlas: atlas,
      models: models,
      built: { decoded: decoded, stored: stored },
      originalCapacity: atlas.originalCapacity,
      expanded: expanded,
      placement: placement
    };
  }

  function buildResources(state) {
    if (!state || !state.supported) return [];
    var rows = [];
    state.atlases.forEach(function(atlas) {
      var models = editedModelsForAtlas(state, atlas);
      if (!models.length) return;
      var decoded = buildDecoded(state, atlas);
      var stored = A.bootLzCompress(decoded);
      rows.push(builtRow(atlas, models, decoded, stored));
    });
    return rows;
  }

  async function buildResourcesAsync(state, onProgress) {
    if (!state || !state.supported) return [];
    var atlases = state.atlases.filter(function(atlas) {
      return editedModelsForAtlas(state, atlas).length;
    });
    var rows = [];
    for (var index = 0; index < atlases.length; index++) {
      var atlas = atlases[index];
      var models = editedModelsForAtlas(state, atlas);
      var decoded = buildDecoded(state, atlas);
      var stored = await A.bootLzCompressAsync(decoded, function(fraction) {
        if (onProgress) {
          onProgress('Army sprite atlas ' + (index + 1) + ' of ' +
            atlases.length, (index + fraction) / Math.max(1, atlases.length));
        }
      });
      rows.push(builtRow(atlas, models, decoded, stored));
    }
    if (onProgress) onProgress('Army sprite atlases complete', 1);
    return rows;
  }

  function applyResources(rows, bytes, ranges, log) {
    rows.forEach(function(row) {
      var atlas = row.atlas;
      if (row.placement === 'relocated') {
        if (!row.allocation) {
          fail(atlas.label + ' expanded resource has no relocation allocation');
        }
        A.patchSplitKey(bytes, atlas.ownerSites, atlas.resourceKey,
          row.allocation.key, ranges, log, atlas.label + ' Army sprite atlas');
        log.push(atlas.label + ': expanded to ' + atlas.targetModelCount +
          ' routed CI8 planes and relocated to ' +
          hex(row.allocation.entry) + ' (' + row.built.stored.length +
          ' bytes)');
        return;
      }
      var resource = A.readResource(bytes, atlas.resourceKey);
      if (resource.entry !== atlas.sizeWord ||
          resource.storedLength !== atlas.resource.storedLength ||
          !equalBytes(resource.stored, atlas.resource.stored)) {
        fail(atlas.label + ' in-place resource preimage differs from the loaded source');
      }
      A.writeU32(bytes, resource.entry, row.built.stored.length);
      bytes.set(row.built.stored, resource.entry + 4);
      bytes.fill(0, resource.entry + 4 + row.built.stored.length,
        resource.entry + 4 + row.originalCapacity);
      ranges.push([resource.entry, resource.entry + 4 + row.originalCapacity]);
      log.push(atlas.label + ': rebuilt ' + row.models.length +
        ' edited CI8 plane' + (row.models.length === 1 ? '' : 's') +
        ' in place; compressed ' + row.built.stored.length + '/' +
        row.originalCapacity + ' bytes');
    });
  }

  function verifyResources(rows, bytes) {
    rows.forEach(function(row) {
      var atlas = row.atlas;
      var key = row.placement === 'relocated'
        ? row.allocation.key : atlas.resourceKey;
      if (row.placement === 'relocated') {
        var ownerKey = A.resolveSplitKey(bytes, atlas.ownerSites,
          atlas.label + ' Army sprite atlas');
        if (ownerKey !== key) {
          fail(atlas.label + ' relocated owner key differs after readback');
        }
      }
      var decoded = A.readCompressedResource(bytes, key).decoded;
      if (!equalBytes(decoded, row.built.decoded)) {
        fail(atlas.label + ' exported resource differs after compressed readback');
      }
      row.models.forEach(function(model) {
        var observed = decoded.slice(model.decodedOffset,
          model.decodedOffset + atlas.width * atlas.height);
        if (!equalBytes(observed, row.built.decoded.slice(model.decodedOffset,
            model.decodedOffset + atlas.width * atlas.height))) {
          fail(model.key + ' exported CI8 plane differs from its staged edit');
        }
      });
    });
  }

  function currentRanges(rows) {
    var ranges = [];
    (rows || []).forEach(function(row) {
      if (row.placement === 'in-place') {
        ranges.push([row.atlas.sizeWord,
          row.atlas.sizeWord + 4 + row.originalCapacity]);
      } else {
        row.atlas.ownerSites.forEach(function(site) {
          ranges.push([site.lui, site.lui + 4], [site.ori, site.ori + 4]);
        });
      }
    });
    return ranges;
  }

  function snapshotEdits(state) {
    var output = {};
    Object.keys(state && state.edits || {}).forEach(function(key) {
      output[key] = state.edits[key].slice();
    });
    return output;
  }

  function restoreEdits(state, edits) {
    state.edits = {};
    Object.keys(edits || {}).forEach(function(key) {
      var model = state.byModelKey[key];
      if (!model) fail('invalid Army sprite edit snapshot ' + key);
      var atlas = state.byKey[model.atlasKey];
      validateIndices(atlas, edits[key], key);
      if (!model.originalIndices ||
          !equalBytes(model.originalIndices, edits[key])) {
        state.edits[key] = edits[key].slice();
      }
    });
  }

  function resetAll(state) {
    if (!state) return;
    state.edits = {};
  }

  function editCount(state) {
    return state && state.edits ? Object.keys(state.edits).length : 0;
  }

  OB64.armySprites = {
    specs: SPECS,
    ArmySpriteError: ArmySpriteError,
    initialize: initialize,
    modelKey: modelKey,
    atlasFor: atlasFor,
    modelFor: modelFor,
    setClassModelSize: setClassModelSize,
    syncClassModelSizes: syncClassModelSizes,
    currentIndices: currentIndices,
    hasCurrentPlane: hasCurrentPlane,
    setEdit: setEdit,
    createBlankPlane: createBlankPlane,
    resetModel: resetModel,
    historyFor: historyFor,
    undo: undo,
    redo: redo,
    renderWords: renderWords,
    rgbaPixels: rgbaPixels,
    nearestPaletteIndex: nearestPaletteIndex,
    prepareImageImport: prepareImageImport,
    collectProject: collectProject,
    prepareProject: prepareProject,
    applyPrepared: applyPrepared,
    buildDecoded: buildDecoded,
    buildResources: buildResources,
    buildResourcesAsync: buildResourcesAsync,
    applyResources: applyResources,
    verifyResources: verifyResources,
    currentRanges: currentRanges,
    snapshotEdits: snapshotEdits,
    restoreEdits: restoreEdits,
    resetAll: resetAll,
    editCount: editCount,
    equalBytes: equalBytes,
    hex: hex
  };
})();
