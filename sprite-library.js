// LordlyCaliber - reusable Sprite Editor asset library

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var PROJECT_SCHEMA_VERSION = 1;
  var FILE_FORMAT = 'lordlycaliber-sprite-asset';
  var FILE_VERSION = 1;
  var MAX_DIMENSION = 4096;
  var MAX_ASSETS = 512;
  var MAX_FRAMES = 1024;
  var MAX_LAYERS = 64;
  var MAX_TOTAL_PIXELS = 64 * 1024 * 1024;
  var KNOWN_SPRITE_FORMATS = [
    {
      id: 'class-avatar', label: 'Class avatar', kind: 'sprite',
      width: 40, height: 48, defaultName: 'Untitled Class Avatar',
      description: 'Creates a blank class-card avatar at its native dimensions.'
    },
    {
      id: 'item-icon', label: 'Item icon', kind: 'sprite',
      width: 16, height: 16, defaultName: 'Untitled Item Icon',
      description: 'Creates a blank item icon. Final import uses the selected pack\'s shared 255-color opaque palette.'
    },
    {
      id: 'army-formation-small',
      label: 'Army formation sprite (ordinary or special)', kind: 'sprite',
      width: 16, height: 24, defaultName: 'Untitled Army Formation Sprite',
      description: 'Creates a blank ordinary or special formation sprite. Import it into Army Sprites before ROM export.'
    },
    {
      id: 'army-formation-large',
      label: 'Army formation sprite (large)', kind: 'sprite',
      width: 32, height: 28, defaultName: 'Untitled Large Army Formation Sprite',
      description: 'Creates a blank large formation sprite. Import it into Army Sprites before ROM export.'
    }
  ];

  function SpriteLibraryError(message) {
    this.name = 'SpriteLibraryError';
    this.message = message;
  }
  SpriteLibraryError.prototype = new Error();

  function fail(message) {
    throw new SpriteLibraryError(message);
  }

  function integer(value, minimum, maximum, label) {
    value = Number(value);
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail(label + ' must be from ' + minimum + ' through ' + maximum);
    }
    return value;
  }

  function cleanName(value, fallback) {
    var name = String(value || '').trim().replace(/\s+/g, ' ');
    if (!name) name = fallback;
    if (name.length > 120) fail('Sprite asset names must contain 120 characters or fewer');
    return name;
  }

  function validId(value, label) {
    value = String(value || '');
    if (!/^[A-Za-z0-9._:-]{1,120}$/.test(value)) {
      fail(label + ' has an invalid identifier');
    }
    return value;
  }

  function pixelArray(value, width, height, label) {
    var expected = width * height * 4;
    if (!ArrayBuffer.isView(value) || value.length !== expected) {
      fail(label + ' must contain exactly ' + expected + ' RGBA bytes');
    }
    return new Uint8ClampedArray(value);
  }

  function bytesToBase64(bytes) {
    var binary = '';
    var block = 0x8000;
    for (var offset = 0; offset < bytes.length; offset += block) {
      binary += String.fromCharCode.apply(null,
        bytes.subarray(offset, Math.min(bytes.length, offset + block)));
    }
    return btoa(binary);
  }

  function bytesFromBase64(value, expectedLength, label) {
    if (typeof value !== 'string') fail(label + ' is missing its RGBA byte data');
    var expectedEncodedLength = Math.ceil(expectedLength / 3) * 4;
    if (value.length !== expectedEncodedLength) {
      fail(label + ' has an invalid encoded RGBA byte length');
    }
    var binary;
    try {
      binary = atob(value);
    } catch (error) {
      fail(label + ' has invalid base64 RGBA byte data');
    }
    if (binary.length !== expectedLength) {
      fail(label + ' has ' + binary.length + ' RGBA bytes; expected ' + expectedLength);
    }
    var output = new Uint8ClampedArray(binary.length);
    for (var index = 0; index < binary.length; index++) {
      output[index] = binary.charCodeAt(index);
    }
    return output;
  }

  function cloneLayer(layer) {
    return {
      id: layer.id,
      name: layer.name,
      visible: layer.visible !== false,
      pixels: new Uint8ClampedArray(layer.pixels)
    };
  }

  function cloneFrame(frame) {
    return {
      id: frame.id,
      name: frame.name,
      ticks: frame.ticks,
      layers: frame.layers.map(cloneLayer)
    };
  }

  function cloneAsset(asset) {
    return {
      id: asset.id,
      name: asset.name,
      kind: asset.kind,
      width: asset.width,
      height: asset.height,
      frames: asset.frames.map(cloneFrame),
      provenance: Object.assign({}, asset.provenance || {})
    };
  }

  function serializeLayer(layer) {
    return {
      id: layer.id,
      name: layer.name,
      visible: layer.visible !== false,
      pixelsRgbaBase64: bytesToBase64(layer.pixels)
    };
  }

  function serializeFrame(frame) {
    return {
      id: frame.id,
      name: frame.name,
      ticks: frame.ticks,
      layers: frame.layers.map(serializeLayer)
    };
  }

  function serializeAsset(asset) {
    validateAsset(asset, 'Sprite asset');
    return {
      id: asset.id,
      name: asset.name,
      kind: asset.kind,
      width: asset.width,
      height: asset.height,
      frames: asset.frames.map(serializeFrame),
      provenance: Object.assign({}, asset.provenance || {})
    };
  }

  function prepareLayer(payload, width, height, label) {
    if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
      fail(label + ' must be an object');
    }
    return {
      id: validId(payload.id, label),
      name: cleanName(payload.name, 'Layer'),
      visible: payload.visible !== false,
      pixels: bytesFromBase64(payload.pixelsRgbaBase64,
        width * height * 4, label)
    };
  }

  function prepareFrame(payload, width, height, label) {
    if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
      fail(label + ' must be an object');
    }
    if (!Array.isArray(payload.layers) || !payload.layers.length ||
        payload.layers.length > MAX_LAYERS) {
      fail(label + ' must contain from 1 through ' + MAX_LAYERS + ' layers');
    }
    var layerIds = {};
    var layers = payload.layers.map(function(layer, index) {
      var prepared = prepareLayer(layer, width, height,
        label + ' layer ' + (index + 1));
      if (layerIds[prepared.id]) fail(label + ' contains duplicate layer ID ' + prepared.id);
      layerIds[prepared.id] = true;
      return prepared;
    });
    return {
      id: validId(payload.id, label),
      name: cleanName(payload.name, 'Frame'),
      ticks: integer(payload.ticks, 1, 255, label + ' ticks'),
      layers: layers
    };
  }

  function prepareAsset(payload, label) {
    label = label || 'Sprite asset';
    if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
      fail(label + ' must be an object');
    }
    var width = integer(payload.width, 1, MAX_DIMENSION, label + ' width');
    var height = integer(payload.height, 1, MAX_DIMENSION, label + ' height');
    var kind = String(payload.kind || '');
    if (kind !== 'sprite' && kind !== 'frame' && kind !== 'sequence') {
      fail(label + ' kind must be sprite, frame, or sequence');
    }
    if (!Array.isArray(payload.frames) || !payload.frames.length ||
        payload.frames.length > MAX_FRAMES) {
      fail(label + ' must contain from 1 through ' + MAX_FRAMES + ' frames');
    }
    var assetPixels = 0;
    payload.frames.forEach(function(frame, frameIndex) {
      if (!frame || !Array.isArray(frame.layers) || !frame.layers.length ||
          frame.layers.length > MAX_LAYERS) {
        fail(label + ' frame ' + (frameIndex + 1) + ' has an invalid layer count');
      }
      assetPixels += width * height * frame.layers.length;
      if (assetPixels > MAX_TOTAL_PIXELS) {
        fail(label + ' exceeds the ' + MAX_TOTAL_PIXELS + '-pixel Project limit');
      }
    });
    if (kind !== 'sequence' && payload.frames.length !== 1) {
      fail(label + ' kind ' + kind + ' must contain exactly one frame');
    }
    var frameIds = {};
    var frames = payload.frames.map(function(frame, index) {
      var prepared = prepareFrame(frame, width, height,
        label + ' frame ' + (index + 1));
      if (frameIds[prepared.id]) fail(label + ' contains duplicate frame ID ' + prepared.id);
      frameIds[prepared.id] = true;
      return prepared;
    });
    if (kind === 'sprite' && frames[0].layers.length !== 1) {
      fail(label + ' kind sprite must contain exactly one layer');
    }
    var provenance = {};
    if (payload.provenance !== undefined) {
      if (!payload.provenance || typeof payload.provenance !== 'object' ||
          Array.isArray(payload.provenance)) {
        fail(label + ' provenance must be an object');
      }
      Object.keys(payload.provenance).forEach(function(key) {
        if (typeof payload.provenance[key] === 'string' ||
            typeof payload.provenance[key] === 'number' ||
            typeof payload.provenance[key] === 'boolean') {
          provenance[key] = payload.provenance[key];
        }
      });
    }
    return {
      id: validId(payload.id, label),
      name: cleanName(payload.name, 'Untitled Sprite'),
      kind: kind,
      width: width,
      height: height,
      frames: frames,
      provenance: provenance
    };
  }

  function validateAsset(asset, label) {
    var prepared = prepareAsset(serializeForValidation(asset), label);
    return prepared;
  }

  function serializeForValidation(asset) {
    return {
      id: asset && asset.id,
      name: asset && asset.name,
      kind: asset && asset.kind,
      width: asset && asset.width,
      height: asset && asset.height,
      frames: (asset && asset.frames || []).map(function(frame) {
        return {
          id: frame.id,
          name: frame.name,
          ticks: frame.ticks,
          layers: (frame.layers || []).map(function(layer) {
            return {
              id: layer.id,
              name: layer.name,
              visible: layer.visible !== false,
              pixelsRgbaBase64: bytesToBase64(layer.pixels || new Uint8ClampedArray())
            };
          })
        };
      }),
      provenance: Object.assign({}, asset && asset.provenance || {})
    };
  }

  function defaultUi() {
    return {
      assetId: null,
      frameIndex: 0,
      layerIndex: 0,
      tool: 'pencil',
      brushSize: 1,
      zoom: 10,
      showGrid: true,
      background: 'checkerboard',
      color: [255, 255, 255, 255],
      selection: null,
      clipboard: null,
      search: '',
      scroll: {}
    };
  }

  function createState() {
    return {
      schemaVersion: PROJECT_SCHEMA_VERSION,
      assets: [],
      byId: {},
      history: {},
      nextAssetId: 1,
      ui: defaultUi()
    };
  }

  function indexState(state) {
    state.byId = {};
    state.assets.forEach(function(asset) { state.byId[asset.id] = asset; });
    if (!state.ui) state.ui = defaultUi();
    if (!state.history) state.history = {};
    if (!Number.isInteger(state.nextAssetId) || state.nextAssetId < 1) {
      state.nextAssetId = 1;
    }
    if (state.ui.assetId && !state.byId[state.ui.assetId]) state.ui.assetId = null;
    if (!state.ui.assetId && state.assets.length) state.ui.assetId = state.assets[0].id;
    return state;
  }

  function initialize(rom) {
    if (!rom) fail('A loaded ROM session is required');
    if (!rom.spriteLibrary) rom.spriteLibrary = createState();
    return indexState(rom.spriteLibrary);
  }

  function nextAssetId(state) {
    var id;
    do {
      id = 'sprite-' + state.nextAssetId++;
    } while (state.byId[id]);
    return id;
  }

  function uniqueChildId(rows, prefix) {
    var used = {};
    rows.forEach(function(row) { used[row.id] = true; });
    var next = 1;
    while (used[prefix + '-' + next]) next++;
    return prefix + '-' + next;
  }

  function transparentPixels(width, height) {
    return new Uint8ClampedArray(width * height * 4);
  }

  function knownSpriteFormats() {
    return KNOWN_SPRITE_FORMATS.map(function(format) {
      return Object.assign({}, format);
    });
  }

  function blankAsset(state, options) {
    options = options || {};
    var width = integer(options.width || 32, 1, MAX_DIMENSION, 'Sprite width');
    var height = integer(options.height || 32, 1, MAX_DIMENSION, 'Sprite height');
    var id = nextAssetId(state);
    return {
      id: id,
      name: cleanName(options.name, 'Untitled Sprite'),
      kind: options.kind || 'sprite',
      width: width,
      height: height,
      frames: [{
        id: 'frame-1', name: 'Frame 1', ticks: 8,
        layers: [{
          id: 'layer-1', name: 'Layer 1', visible: true,
          pixels: transparentPixels(width, height)
        }]
      }],
      provenance: Object.assign({ source: 'blank' }, options.provenance || {})
    };
  }

  function assetFromRgba(state, options) {
    options = options || {};
    var width = integer(options.width, 1, MAX_DIMENSION, 'Sprite width');
    var height = integer(options.height, 1, MAX_DIMENSION, 'Sprite height');
    var asset = blankAsset(state, {
      name: options.name,
      kind: options.kind || 'sprite',
      width: width,
      height: height,
      provenance: options.provenance
    });
    asset.frames[0].ticks = integer(options.ticks || 8, 1, 255, 'Frame ticks');
    asset.frames[0].layers[0].pixels = pixelArray(
      options.rgba, width, height, 'Sprite pixels');
    return asset;
  }

  function assetFromFrames(state, options) {
    options = options || {};
    var width = integer(options.width, 1, MAX_DIMENSION, 'Sequence width');
    var height = integer(options.height, 1, MAX_DIMENSION, 'Sequence height');
    if (!Array.isArray(options.frames) || !options.frames.length ||
        options.frames.length > MAX_FRAMES) {
      fail('A sequence must contain from 1 through ' + MAX_FRAMES + ' frames');
    }
    var id = nextAssetId(state);
    var asset = {
      id: id,
      name: cleanName(options.name, 'Imported Sequence'),
      kind: options.kind || (options.frames.length > 1 ? 'sequence' : 'frame'),
      width: width,
      height: height,
      frames: options.frames.map(function(frame, frameIndex) {
        var layerRows = frame.layers || [{
          name: 'Complete frame', rgba: frame.rgba
        }];
        if (!layerRows.length || layerRows.length > MAX_LAYERS) {
          fail('Imported frame ' + (frameIndex + 1) + ' has an invalid layer count');
        }
        return {
          id: 'frame-' + (frameIndex + 1),
          name: cleanName(frame.name, 'Frame ' + (frameIndex + 1)),
          ticks: integer(frame.ticks || 8, 1, 255,
            'Frame ' + (frameIndex + 1) + ' ticks'),
          layers: layerRows.map(function(layer, layerIndex) {
            return {
              id: 'layer-' + (layerIndex + 1),
              name: cleanName(layer.name, 'Layer ' + (layerIndex + 1)),
              visible: layer.visible !== false,
              pixels: pixelArray(layer.rgba, width, height,
                'Frame ' + (frameIndex + 1) + ' layer ' + (layerIndex + 1))
            };
          })
        };
      }),
      provenance: Object.assign({}, options.provenance || {})
    };
    validateAsset(asset, 'Imported sprite asset');
    return asset;
  }

  function addAsset(state, asset, options) {
    options = options || {};
    indexState(state);
    if (state.assets.length >= MAX_ASSETS) {
      fail('The Project already contains the maximum ' + MAX_ASSETS + ' sprite assets');
    }
    asset = cloneAsset(asset);
    validateAsset(asset, 'Sprite asset');
    if (state.byId[asset.id]) {
      if (!options.renameCollision) fail('Sprite asset ID ' + asset.id + ' already exists');
      asset.id = nextAssetId(state);
    }
    state.assets.push(asset);
    state.byId[asset.id] = asset;
    state.ui.assetId = asset.id;
    state.ui.frameIndex = 0;
    state.ui.layerIndex = 0;
    state.ui.selection = null;
    return asset;
  }

  function assetFor(state, id) {
    indexState(state);
    var asset = state.byId[id];
    if (!asset) fail('Unknown sprite asset ' + id);
    return asset;
  }

  function historyFor(state, id) {
    if (!state.history[id]) state.history[id] = { undo: [], redo: [] };
    return state.history[id];
  }

  function mutate(state, id, action) {
    var asset = assetFor(state, id);
    var before = cloneAsset(asset);
    action(asset);
    validateAsset(asset, 'Sprite asset');
    var history = historyFor(state, id);
    history.undo.push(before);
    if (history.undo.length > 100) history.undo.shift();
    history.redo = [];
    return asset;
  }

  function restoreAsset(state, id, snapshot) {
    var asset = assetFor(state, id);
    var index = state.assets.indexOf(asset);
    state.assets[index] = cloneAsset(snapshot);
    indexState(state);
    return state.assets[index];
  }

  function undo(state, id) {
    var history = historyFor(state, id);
    if (!history.undo.length) return false;
    var current = cloneAsset(assetFor(state, id));
    var prior = history.undo.pop();
    history.redo.push(current);
    restoreAsset(state, id, prior);
    return true;
  }

  function redo(state, id) {
    var history = historyFor(state, id);
    if (!history.redo.length) return false;
    var current = cloneAsset(assetFor(state, id));
    var next = history.redo.pop();
    history.undo.push(current);
    restoreAsset(state, id, next);
    return true;
  }

  function removeAsset(state, id) {
    var asset = assetFor(state, id);
    state.assets.splice(state.assets.indexOf(asset), 1);
    delete state.byId[id];
    delete state.history[id];
    state.ui.assetId = state.assets.length ? state.assets[0].id : null;
    state.ui.frameIndex = 0;
    state.ui.layerIndex = 0;
    state.ui.selection = null;
    return true;
  }

  function duplicateAsset(state, id) {
    var duplicate = cloneAsset(assetFor(state, id));
    duplicate.id = nextAssetId(state);
    duplicate.name = cleanName(duplicate.name + ' Copy', 'Sprite Copy');
    duplicate.provenance = Object.assign({}, duplicate.provenance, {
      source: 'sprite-library-copy', sourceAssetId: id
    });
    return addAsset(state, duplicate);
  }

  function renameAsset(state, id, name) {
    return mutate(state, id, function(asset) {
      asset.name = cleanName(name, 'Untitled Sprite');
    });
  }

  function currentFrame(asset, frameIndex) {
    frameIndex = integer(frameIndex, 0, asset.frames.length - 1, 'Frame index');
    return asset.frames[frameIndex];
  }

  function currentLayer(asset, frameIndex, layerIndex) {
    var frame = currentFrame(asset, frameIndex);
    layerIndex = integer(layerIndex, 0, frame.layers.length - 1, 'Layer index');
    return frame.layers[layerIndex];
  }

  function replaceLayerPixels(state, id, frameIndex, layerIndex, pixels) {
    return mutate(state, id, function(asset) {
      currentLayer(asset, frameIndex, layerIndex).pixels = pixelArray(
        pixels, asset.width, asset.height, 'Edited sprite layer');
    });
  }

  function sourceOver(target, source) {
    var sourceAlpha = source[3] / 255;
    var targetAlpha = target[3] / 255;
    var outputAlpha = sourceAlpha + targetAlpha * (1 - sourceAlpha);
    if (!outputAlpha) return [0, 0, 0, 0];
    return [
      Math.round((source[0] * sourceAlpha + target[0] * targetAlpha *
        (1 - sourceAlpha)) / outputAlpha),
      Math.round((source[1] * sourceAlpha + target[1] * targetAlpha *
        (1 - sourceAlpha)) / outputAlpha),
      Math.round((source[2] * sourceAlpha + target[2] * targetAlpha *
        (1 - sourceAlpha)) / outputAlpha),
      Math.round(outputAlpha * 255)
    ];
  }

  function compositeFrame(asset, frameIndex) {
    var frame = currentFrame(asset, frameIndex);
    var output = transparentPixels(asset.width, asset.height);
    frame.layers.forEach(function(layer) {
      if (layer.visible === false) return;
      for (var offset = 0; offset < output.length; offset += 4) {
        if (!layer.pixels[offset + 3]) continue;
        var blended = sourceOver(
          [output[offset], output[offset + 1], output[offset + 2], output[offset + 3]],
          [layer.pixels[offset], layer.pixels[offset + 1],
            layer.pixels[offset + 2], layer.pixels[offset + 3]]);
        output[offset] = blended[0];
        output[offset + 1] = blended[1];
        output[offset + 2] = blended[2];
        output[offset + 3] = blended[3];
      }
    });
    return output;
  }

  function setFrameTicks(state, id, frameIndex, ticks) {
    return mutate(state, id, function(asset) {
      currentFrame(asset, frameIndex).ticks = integer(ticks, 1, 255, 'Frame ticks');
    });
  }

  function addFrame(state, id, afterIndex, duplicate) {
    return mutate(state, id, function(asset) {
      if (asset.frames.length >= MAX_FRAMES) fail('The sequence already has 1024 frames');
      var source = currentFrame(asset, afterIndex);
      var frame = duplicate ? cloneFrame(source) : {
        id: '', name: '', ticks: source.ticks,
        layers: [{ id: 'layer-1', name: 'Layer 1', visible: true,
          pixels: transparentPixels(asset.width, asset.height) }]
      };
      frame.id = uniqueChildId(asset.frames, 'frame');
      frame.name = duplicate ? source.name + ' Copy' : 'Frame ' + (asset.frames.length + 1);
      frame.layers.forEach(function(layer, index) { layer.id = 'layer-' + (index + 1); });
      asset.frames.splice(afterIndex + 1, 0, frame);
      asset.kind = 'sequence';
    });
  }

  function removeFrame(state, id, frameIndex) {
    return mutate(state, id, function(asset) {
      if (asset.frames.length <= 1) fail('A sprite asset must retain one frame');
      asset.frames.splice(frameIndex, 1);
    });
  }

  function moveFrame(state, id, fromIndex, toIndex) {
    return mutate(state, id, function(asset) {
      fromIndex = integer(fromIndex, 0, asset.frames.length - 1, 'Source frame index');
      toIndex = integer(toIndex, 0, asset.frames.length - 1, 'Target frame index');
      if (fromIndex === toIndex) return;
      var frame = asset.frames.splice(fromIndex, 1)[0];
      asset.frames.splice(toIndex, 0, frame);
    });
  }

  function addLayer(state, id, frameIndex, duplicateIndex) {
    return mutate(state, id, function(asset) {
      var frame = currentFrame(asset, frameIndex);
      if (frame.layers.length >= MAX_LAYERS) fail('The frame already has 64 layers');
      var layer = Number.isInteger(duplicateIndex)
        ? cloneLayer(currentLayer(asset, frameIndex, duplicateIndex))
        : { id: '', name: '', visible: true,
          pixels: transparentPixels(asset.width, asset.height) };
      layer.id = uniqueChildId(frame.layers, 'layer');
      layer.name = Number.isInteger(duplicateIndex)
        ? layer.name + ' Copy' : 'Layer ' + (frame.layers.length + 1);
      frame.layers.push(layer);
      if (asset.kind === 'sprite') asset.kind = 'frame';
    });
  }

  function removeLayer(state, id, frameIndex, layerIndex) {
    return mutate(state, id, function(asset) {
      var frame = currentFrame(asset, frameIndex);
      if (frame.layers.length <= 1) fail('A frame must retain one layer');
      frame.layers.splice(layerIndex, 1);
    });
  }

  function moveLayer(state, id, frameIndex, fromIndex, toIndex) {
    return mutate(state, id, function(asset) {
      var frame = currentFrame(asset, frameIndex);
      fromIndex = integer(fromIndex, 0, frame.layers.length - 1, 'Source layer index');
      toIndex = integer(toIndex, 0, frame.layers.length - 1, 'Target layer index');
      if (fromIndex === toIndex) return;
      var layer = frame.layers.splice(fromIndex, 1)[0];
      frame.layers.splice(toIndex, 0, layer);
    });
  }

  function renameLayer(state, id, frameIndex, layerIndex, name) {
    return mutate(state, id, function(asset) {
      currentLayer(asset, frameIndex, layerIndex).name = cleanName(name, 'Layer');
    });
  }

  function setLayerVisible(state, id, frameIndex, layerIndex, visible) {
    return mutate(state, id, function(asset) {
      currentLayer(asset, frameIndex, layerIndex).visible = !!visible;
    });
  }

  function shiftedPixels(pixels, width, height, dx, dy) {
    var output = transparentPixels(width, height);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var targetX = x + dx;
        var targetY = y + dy;
        if (targetX < 0 || targetY < 0 || targetX >= width || targetY >= height) continue;
        var sourceOffset = (y * width + x) * 4;
        var targetOffset = (targetY * width + targetX) * 4;
        output.set(pixels.subarray(sourceOffset, sourceOffset + 4), targetOffset);
      }
    }
    return output;
  }

  function shiftLayer(state, id, frameIndex, layerIndex, dx, dy) {
    return mutate(state, id, function(asset) {
      var layer = currentLayer(asset, frameIndex, layerIndex);
      layer.pixels = shiftedPixels(layer.pixels, asset.width, asset.height,
        integer(dx, -MAX_DIMENSION, MAX_DIMENSION, 'Horizontal shift'),
        integer(dy, -MAX_DIMENSION, MAX_DIMENSION, 'Vertical shift'));
    });
  }

  function flippedPixels(pixels, width, height, vertical) {
    var output = transparentPixels(width, height);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var sourceX = vertical ? x : width - 1 - x;
        var sourceY = vertical ? height - 1 - y : y;
        var sourceOffset = (sourceY * width + sourceX) * 4;
        output.set(pixels.subarray(sourceOffset, sourceOffset + 4),
          (y * width + x) * 4);
      }
    }
    return output;
  }

  function flipLayer(state, id, frameIndex, layerIndex, vertical) {
    return mutate(state, id, function(asset) {
      var layer = currentLayer(asset, frameIndex, layerIndex);
      layer.pixels = flippedPixels(layer.pixels, asset.width, asset.height, !!vertical);
    });
  }

  function nearestResize(pixels, width, height, targetWidth, targetHeight) {
    var output = transparentPixels(targetWidth, targetHeight);
    for (var y = 0; y < targetHeight; y++) {
      var sourceY = Math.min(height - 1, Math.floor(y * height / targetHeight));
      for (var x = 0; x < targetWidth; x++) {
        var sourceX = Math.min(width - 1, Math.floor(x * width / targetWidth));
        var sourceOffset = (sourceY * width + sourceX) * 4;
        output.set(pixels.subarray(sourceOffset, sourceOffset + 4),
          (y * targetWidth + x) * 4);
      }
    }
    return output;
  }

  function resizeAsset(state, id, width, height) {
    return mutate(state, id, function(asset) {
      width = integer(width, 1, MAX_DIMENSION, 'Canvas width');
      height = integer(height, 1, MAX_DIMENSION, 'Canvas height');
      asset.frames.forEach(function(frame) {
        frame.layers.forEach(function(layer) {
          layer.pixels = nearestResize(layer.pixels,
            asset.width, asset.height, width, height);
        });
      });
      asset.width = width;
      asset.height = height;
    });
  }

  function rotateAsset(state, id, clockwise) {
    return mutate(state, id, function(asset) {
      var oldWidth = asset.width;
      var oldHeight = asset.height;
      asset.frames.forEach(function(frame) {
        frame.layers.forEach(function(layer) {
          var output = transparentPixels(oldHeight, oldWidth);
          for (var y = 0; y < oldHeight; y++) {
            for (var x = 0; x < oldWidth; x++) {
              var targetX = clockwise ? oldHeight - 1 - y : y;
              var targetY = clockwise ? x : oldWidth - 1 - x;
              var sourceOffset = (y * oldWidth + x) * 4;
              output.set(layer.pixels.subarray(sourceOffset, sourceOffset + 4),
                (targetY * oldHeight + targetX) * 4);
            }
          }
          layer.pixels = output;
        });
      });
      asset.width = oldHeight;
      asset.height = oldWidth;
    });
  }

  function trimRgba(rgba, width, height) {
    var left = width;
    var top = height;
    var right = -1;
    var bottom = -1;
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        if (!rgba[(y * width + x) * 4 + 3]) continue;
        left = Math.min(left, x);
        top = Math.min(top, y);
        right = Math.max(right, x);
        bottom = Math.max(bottom, y);
      }
    }
    if (right < left || bottom < top) {
      return { width: 1, height: 1, rgba: new Uint8ClampedArray(4) };
    }
    var outputWidth = right - left + 1;
    var outputHeight = bottom - top + 1;
    var output = transparentPixels(outputWidth, outputHeight);
    for (var outputY = 0; outputY < outputHeight; outputY++) {
      for (var outputX = 0; outputX < outputWidth; outputX++) {
        var sourceOffset = ((top + outputY) * width + left + outputX) * 4;
        output.set(rgba.subarray(sourceOffset, sourceOffset + 4),
          (outputY * outputWidth + outputX) * 4);
      }
    }
    return { width: outputWidth, height: outputHeight, rgba: output };
  }

  function copyPart(state, id, kind, frameIndex, layerIndex) {
    var source = assetFor(state, id);
    var output;
    if (kind === 'sequence') {
      output = cloneAsset(source);
      output.kind = 'sequence';
      output.name = source.name + ' Sequence';
    } else if (kind === 'frame') {
      output = cloneAsset(source);
      output.kind = 'frame';
      output.frames = [cloneFrame(currentFrame(source, frameIndex))];
      output.name = source.name + ' Frame ' + (frameIndex + 1);
    } else if (kind === 'sprite') {
      var layer = currentLayer(source, frameIndex, layerIndex);
      var trimmed = trimRgba(layer.pixels, source.width, source.height);
      output = {
        id: source.id,
        name: source.name + ' ' + layer.name,
        kind: 'sprite',
        width: trimmed.width,
        height: trimmed.height,
        frames: [{
          id: 'frame-1', name: 'Frame 1', ticks: currentFrame(source, frameIndex).ticks,
          layers: [{ id: 'layer-1', name: layer.name, visible: true,
            pixels: trimmed.rgba }]
        }],
        provenance: {}
      };
    } else {
      fail('Sprite asset copy kind must be sprite, frame, or sequence');
    }
    output.id = nextAssetId(state);
    output.provenance = Object.assign({}, source.provenance || {}, {
      source: 'sprite-library-derived', sourceAssetId: source.id
    });
    return addAsset(state, output);
  }

  function count(state) {
    return state && Array.isArray(state.assets) ? state.assets.length : 0;
  }

  function collectProjectPayload(rom) {
    var state = rom && rom.spriteLibrary;
    return {
      schemaVersion: PROJECT_SCHEMA_VERSION,
      assets: state && Array.isArray(state.assets)
        ? state.assets.map(serializeAsset) : []
    };
  }

  function prepareProjectPayload(payload) {
    if (payload === undefined || payload === null) return { assets: [], count: 0 };
    if (!payload || typeof payload !== 'object' || Array.isArray(payload) ||
        payload.schemaVersion !== PROJECT_SCHEMA_VERSION ||
        !Array.isArray(payload.assets)) {
      fail('patches.spriteLibrary must use schemaVersion 1 with an assets array');
    }
    if (payload.assets.length > MAX_ASSETS) {
      fail('patches.spriteLibrary contains more than ' + MAX_ASSETS + ' assets');
    }
    var ids = {};
    var totalPixels = 0;
    var assets = payload.assets.map(function(asset, index) {
      var prepared = prepareAsset(asset, 'Sprite Library asset ' + (index + 1));
      if (ids[prepared.id]) fail('Sprite Library contains duplicate asset ID ' + prepared.id);
      ids[prepared.id] = true;
      prepared.frames.forEach(function(frame) {
        totalPixels += prepared.width * prepared.height * frame.layers.length;
      });
      if (totalPixels > MAX_TOTAL_PIXELS) {
        fail('Sprite Library exceeds the ' + MAX_TOTAL_PIXELS + '-pixel Project limit');
      }
      return prepared;
    });
    return { assets: assets, count: assets.length };
  }

  function applyPreparedProjectPayload(rom, prepared) {
    var state = initialize(rom);
    prepared.assets.forEach(function(asset) {
      var existing = state.byId[asset.id];
      if (existing) {
        state.assets[state.assets.indexOf(existing)] = cloneAsset(asset);
      } else {
        state.assets.push(cloneAsset(asset));
      }
    });
    state.history = {};
    indexState(state);
    return prepared.assets.length;
  }

  function assetFilePayload(asset) {
    return {
      format: FILE_FORMAT,
      version: FILE_VERSION,
      asset: serializeAsset(asset)
    };
  }

  function assetFileText(asset) {
    return JSON.stringify(assetFilePayload(asset), null, 2) + '\n';
  }

  function prepareAssetFile(value) {
    var payload = value;
    if (typeof value === 'string') {
      try {
        payload = JSON.parse(value);
      } catch (error) {
        fail('The sprite asset file is not valid JSON');
      }
    }
    if (!payload || payload.format !== FILE_FORMAT || payload.version !== FILE_VERSION) {
      fail('The file is not a LordlyCaliber sprite asset version 1 file');
    }
    return prepareAsset(payload.asset, 'Sprite asset file');
  }

  function filename(asset) {
    var stem = String(asset && asset.name || 'sprite-asset')
      .toLowerCase().replace(/[^a-z0-9._-]+/g, '-').replace(/^-+|-+$/g, '');
    return (stem || 'sprite-asset') + '.ob64-sprite.json';
  }

  OB64.spriteLibrary = {
    PROJECT_SCHEMA_VERSION: PROJECT_SCHEMA_VERSION,
    FILE_FORMAT: FILE_FORMAT,
    FILE_VERSION: FILE_VERSION,
    MAX_DIMENSION: MAX_DIMENSION,
    knownSpriteFormats: knownSpriteFormats,
    SpriteLibraryError: SpriteLibraryError,
    createState: createState,
    initialize: initialize,
    indexState: indexState,
    blankAsset: blankAsset,
    assetFromRgba: assetFromRgba,
    assetFromFrames: assetFromFrames,
    addAsset: addAsset,
    removeAsset: removeAsset,
    duplicateAsset: duplicateAsset,
    renameAsset: renameAsset,
    assetFor: assetFor,
    currentFrame: currentFrame,
    currentLayer: currentLayer,
    replaceLayerPixels: replaceLayerPixels,
    compositeFrame: compositeFrame,
    setFrameTicks: setFrameTicks,
    addFrame: addFrame,
    removeFrame: removeFrame,
    moveFrame: moveFrame,
    addLayer: addLayer,
    removeLayer: removeLayer,
    moveLayer: moveLayer,
    renameLayer: renameLayer,
    setLayerVisible: setLayerVisible,
    shiftLayer: shiftLayer,
    flipLayer: flipLayer,
    resizeAsset: resizeAsset,
    rotateAsset: rotateAsset,
    nearestResize: nearestResize,
    trimRgba: trimRgba,
    copyPart: copyPart,
    historyFor: historyFor,
    undo: undo,
    redo: redo,
    count: count,
    cloneAsset: cloneAsset,
    serializeAsset: serializeAsset,
    prepareAsset: prepareAsset,
    collectProjectPayload: collectProjectPayload,
    prepareProjectPayload: prepareProjectPayload,
    applyPreparedProjectPayload: applyPreparedProjectPayload,
    assetFilePayload: assetFilePayload,
    assetFileText: assetFileText,
    prepareAssetFile: prepareAssetFile,
    filename: filename,
    sourceOver: sourceOver
  };
})();
