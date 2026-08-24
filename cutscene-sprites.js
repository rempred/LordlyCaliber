// Lordly Caliber - Cutscene actor-sprite preview adapter.
//
// Descriptor, metadata, pose, configuration, lookup, and 0x5554 art resources
// are byte-mapped. Actor record +0x146 supplies the runtime descriptor variant
// and child selector; an out-of-range child selector falls back to child 0.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var BODY_DESCRIPTOR_TABLE_KEY = 0x003B6CD0;
  var BODY_DESCRIPTOR_COUNT = 208;
  var BODY_CLASS_HANDLE_RESOURCE_KEY = 0x00315736;
  var BODY_CLASS_HANDLE_TABLE_OFFSET = 0x24;
  var BODY_CLASS_HANDLE_COUNT = 688;

  function SpriteError(message) {
    this.name = 'CutsceneSpriteError';
    this.message = message;
  }
  SpriteError.prototype = Object.create(Error.prototype);
  SpriteError.prototype.constructor = SpriteError;

  function fail(message) { throw new SpriteError(message); }

  function readU16(bytes, offset) {
    if (offset < 0 || offset + 2 > bytes.length) fail('Sprite u16 read is outside its source.');
    return (bytes[offset] << 8) | bytes[offset + 1];
  }

  function readU32(bytes, offset) {
    if (offset < 0 || offset + 4 > bytes.length) fail('Sprite u32 read is outside its source.');
    return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
      (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
  }

  function create(z64, catalog) {
    if (!(z64 instanceof Uint8Array)) fail('Cutscene sprite rendering requires normalized ROM bytes.');
    if (!catalog || typeof catalog.getPoseProgram !== 'function') {
      fail('Cutscene sprite rendering requires the validated Cutscene catalog.');
    }
    return {
      z64: z64,
      catalog: catalog,
      banks: {},
      programs: {},
      frames: {},
      bodyAnimations: null,
      bodyRoutes: {},
      bodyPrograms: {},
      errors: {}
    };
  }

  function paletteBanks(bytes, count) {
    if (bytes.length !== count * 0x200) fail('Cutscene lookup resource has an invalid extent.');
    var output = [];
    for (var bank = 0; bank < count; bank++) {
      var words = new Uint16Array(256);
      for (var index = 0; index < 256; index++) {
        words[index] = readU16(bytes, bank * 0x200 + index * 2);
      }
      output.push(words);
    }
    return output;
  }

  function parseConfig(bytes, artCount) {
    if (bytes.length < 0x20 || readU32(bytes, 0) !== artCount) {
      fail('Cutscene art configuration does not match its descriptor.');
    }
    var mapOffset = readU32(bytes, 0x1C);
    if (mapOffset + artCount > bytes.length) fail('Cutscene lookup-bank map is truncated.');
    return { mapOffset: mapOffset, bankMap: bytes.slice(mapOffset, mapOffset + artCount) };
  }

  function prepareBank(state, source) {
    var cached = state.banks[source.bank];
    if (cached) return cached;
    var art = OB64.art;
    var animation = OB64.animationArt;
    if (!art || !animation) fail('The Art and Animation Art decoders are unavailable.');
    var descriptor = art.readResource(state.z64, source.descriptorKey);
    if (descriptor.storedLength !== source.descriptorMemberCount * 4) {
      fail('Actor Art Source ' + source.bank + ' descriptor member count changed.');
    }
    var members = [];
    for (var member = 0; member < source.descriptorMemberCount; member++) {
      members.push(readU32(descriptor.stored, member * 4));
    }
    var controls = [source.metadataKey, source.poseKey, source.configKey, source.lookupKey];
    for (var control = 0; control < controls.length; control++) {
      if (members[control] !== controls[control]) {
        fail('Actor Art Source ' + source.bank + ' control member ' + control + ' changed.');
      }
    }
    var metadata = art.readCompressedResource(state.z64, source.metadataKey).decoded;
    var pose = art.readCompressedResource(state.z64, source.poseKey).decoded;
    var configBytes = art.readCompressedResource(state.z64, source.configKey).decoded;
    var lookup = art.readCompressedResource(state.z64, source.lookupKey).decoded;
    if (metadata.length !== source.metadataDecodedLength ||
        pose.length !== source.poseDecodedLength ||
        configBytes.length !== source.configDecodedLength ||
        lookup.length !== source.lookupDecodedLength) {
      fail('Actor Art Source ' + source.bank + ' control resource extent changed.');
    }
    cached = {
      source: source,
      descriptor: descriptor,
      members: members,
      metadata: metadata,
      pose: pose,
      config: parseConfig(configBytes, source.artCount),
      palettes: paletteBanks(lookup, source.lookupBankCount),
      sprites: {}
    };
    state.banks[source.bank] = cached;
    return cached;
  }

  function prepareProgram(state, program) {
    var cached = state.programs[program.programId];
    if (cached) return cached;
    var source = state.catalog.getActorArtSource(program.bank);
    if (!source) fail('Pose program has no Actor Art Source.');
    var bank = prepareBank(state, source);
    if (!program.frames.length || !program.durationFrames) {
      fail('Pose program has no renderable frame records.');
    }
    var originX = 0;
    var originY = 0;
    var endX = 0;
    var endY = 0;
    var bounded = false;
    var frames = program.frames.map(function(frame, sequenceIndex) {
      var parsed = OB64.animationArt.parseMetadataFrame(bank.metadata, frame.frameToken);
      parsed.layers.forEach(function(layer) {
        if (layer.artId >= source.artCount) fail('Pose frame selects art outside its descriptor.');
        var left = layer.drawOffsetX;
        var top = layer.drawOffsetY;
        var right = left + layer.width;
        var bottom = top + layer.height;
        if (!bounded) {
          originX = left; originY = top; endX = right; endY = bottom; bounded = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
      });
      return {
        sequenceIndex: sequenceIndex,
        frameToken: frame.frameToken,
        durationFrames: frame.durationFrames,
        layers: parsed.layers
      };
    });
    if (!bounded || endX <= originX || endY <= originY) fail('Pose program has no drawable bounds.');
    cached = {
      program: program,
      bank: bank,
      frames: frames,
      originX: originX,
      originY: originY,
      width: endX - originX,
      height: endY - originY
    };
    state.programs[program.programId] = cached;
    return cached;
  }

  function prepareStagePropProgram(state, source, placement) {
    var programId = 'stage-prop:' + source.bank + ':selector-' + placement.poseSelector;
    var cached = state.programs[programId];
    if (cached) return cached;
    var bank = prepareBank(state, source);
    var pose = OB64.animationArt.parsePoseProgram(
      bank.pose, placement.poseSelector, 'Mode-2 Stage prop');
    if (!pose.frames.length) {
      fail('Mode-2 Stage prop selector ' + placement.poseSelector +
        ' has no renderable frame records.');
    }
    var originX = 0;
    var originY = 0;
    var endX = 0;
    var endY = 0;
    var bounded = false;
    var frames = pose.frames.map(function(frame, sequenceIndex) {
      var parsed = OB64.animationArt.parseMetadataFrame(bank.metadata, frame[0]);
      parsed.layers.forEach(function(layer) {
        if (layer.artId >= source.artCount) {
          fail('Mode-2 Stage prop selects art outside its descriptor.');
        }
        var left = layer.drawOffsetX;
        var top = layer.drawOffsetY;
        var right = left + layer.width;
        var bottom = top + layer.height;
        if (!bounded) {
          originX = left; originY = top; endX = right; endY = bottom; bounded = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
      });
      return {
        sequenceIndex: sequenceIndex,
        frameToken: frame[0],
        durationFrames: Math.max(1, frame[1]),
        layers: parsed.layers
      };
    });
    if (!bounded || endX <= originX || endY <= originY) {
      fail('Mode-2 Stage prop has no drawable bounds.');
    }
    cached = {
      program: {
        programId: programId,
        durationFrames: frames.reduce(function(total, frame) {
          return total + frame.durationFrames;
        }, 0),
        selectedChildOrdinal: Number.isInteger(source.selectedChildOrdinal)
          ? source.selectedChildOrdinal : 0
      },
      bank: bank,
      frames: frames,
      originX: originX,
      originY: originY,
      width: endX - originX,
      height: endY - originY,
      poseLoop: pose.records.some(function(record) { return record.opcode === 0x04; })
    };
    state.programs[programId] = cached;
    return cached;
  }

  function loadSprite(state, programState, artId, requestedChildOrdinal) {
    var bank = programState.bank;
    requestedChildOrdinal = Number.isInteger(requestedChildOrdinal)
      ? requestedChildOrdinal : programState.program.selectedChildOrdinal;
    var cacheIdentity = artId + ':' + requestedChildOrdinal;
    var cached = bank.sprites[cacheIdentity];
    if (cached) return cached;
    var resourceKey = bank.members[artId + 4];
    var resource = OB64.art.readCompressedResource(state.z64, resourceKey);
    var sprite = OB64.animationArt.parseSpriteObject(resource.decoded, resourceKey);
    var childOrdinal = requestedChildOrdinal;
    if (!Number.isInteger(childOrdinal) || childOrdinal < 0 || childOrdinal >= sprite.childCount) {
      childOrdinal = 0;
    }
    var lanes = OB64.animationArt.materializeChildLanes(sprite, childOrdinal);
    var palette = null;
    if (lanes.lookup) {
      palette = new Uint16Array(256);
      for (var index = 0; index < 256; index++) palette[index] = readU16(lanes.lookup, index * 2);
    } else {
      var lookupBank = bank.config.bankMap[artId];
      if (lookupBank >= bank.palettes.length) fail('Sprite selects a missing lookup bank.');
      palette = bank.palettes[lookupBank];
    }
    cached = {
      resourceKey: resourceKey,
      sprite: sprite,
      requestedChildOrdinal: requestedChildOrdinal,
      childOrdinal: childOrdinal,
      childFallback: childOrdinal !== requestedChildOrdinal,
      lanes: lanes,
      palette: palette,
      rgba: null
    };
    bank.sprites[cacheIdentity] = cached;
    return cached;
  }

  function alphaAt(sprite, lanes, x, y, fallback) {
    if (!lanes.second) return fallback;
    var row = y * sprite.secondStride;
    if (sprite.secondFormat === 0) {
      var packed = lanes.second[row + (x >>> 1)];
      return (x & 1 ? packed & 15 : packed >>> 4) * 17;
    }
    if (sprite.secondFormat === 1) return lanes.second[row + x];
    fail('Sprite alpha lane format is unsupported.');
  }

  function spritePixels(source) {
    if (source.rgba) return source.rgba;
    var sprite = source.sprite;
    var lanes = source.lanes;
    if (!lanes.first) fail('Selected sprite child has no color lane.');
    var output = new Uint8ClampedArray(sprite.width * sprite.height * 4);
    for (var y = 0; y < sprite.height; y++) {
      for (var x = 0; x < sprite.width; x++) {
        var pixel = y * sprite.width + x;
        var offset = pixel * 4;
        var color;
        if (sprite.firstFormat === 1) {
          var paletteIndex = lanes.first[y * sprite.firstStride + x];
          color = OB64.cutsceneAssets.rgba5551(source.palette[paletteIndex], false);
        } else if (sprite.firstFormat === 2) {
          color = OB64.cutsceneAssets.rgba5551(
            readU16(lanes.first, y * sprite.firstStride + x * 2), false);
        } else if (sprite.firstFormat === 3) {
          var direct = y * sprite.firstStride + x * 4;
          color = [lanes.first[direct], lanes.first[direct + 1],
            lanes.first[direct + 2], lanes.first[direct + 3]];
        } else {
          fail('Sprite color lane format is unsupported.');
        }
        output[offset] = color[0];
        output[offset + 1] = color[1];
        output[offset + 2] = color[2];
        output[offset + 3] = alphaAt(sprite, lanes, x, y, color[3]);
      }
    }
    source.rgba = output;
    return output;
  }

  function displayWindowPixels(source, width, height) {
    var sprite = source.sprite;
    var rgba = spritePixels(source);
    if (sprite.width === width && sprite.height === height) return rgba;
    if (width <= 0 || height <= 0 || width > sprite.width || height > sprite.height) {
      fail('Frame metadata exceeds the stored sprite dimensions.');
    }
    for (var sourceY = 0; sourceY < sprite.height; sourceY++) {
      for (var sourceX = 0; sourceX < sprite.width; sourceX++) {
        if (sourceX < width && sourceY < height) continue;
        if (rgba[(sourceY * sprite.width + sourceX) * 4 + 3] !== 0) {
          fail('Frame metadata would crop visible sprite pixels.');
        }
      }
    }
    var output = new Uint8ClampedArray(width * height * 4);
    for (var y = 0; y < height; y++) {
      var sourceStart = y * sprite.width * 4;
      output.set(rgba.subarray(sourceStart, sourceStart + width * 4), y * width * 4);
    }
    return output;
  }

  function orientLayerPixels(rgba, width, height, flags) {
    var flipX = (flags & 0x01) !== 0;
    var flipY = (flags & 0x02) !== 0;
    if (!flipX && !flipY) return rgba;
    var output = new Uint8ClampedArray(rgba.length);
    for (var y = 0; y < height; y++) {
      var sourceY = flipY ? height - 1 - y : y;
      for (var x = 0; x < width; x++) {
        var sourceX = flipX ? width - 1 - x : x;
        var sourceOffset = (sourceY * width + sourceX) * 4;
        output.set(rgba.subarray(sourceOffset, sourceOffset + 4),
          (y * width + x) * 4);
      }
    }
    return output;
  }

  function frameIndexAt(programState, previewFrame, loop) {
    var position = Math.max(0, Math.floor(previewFrame || 0));
    position = loop === false
      ? Math.min(position, programState.program.durationFrames - 1)
      : position % programState.program.durationFrames;
    for (var index = 0; index < programState.frames.length; index++) {
      if (position < programState.frames[index].durationFrames) return index;
      position -= programState.frames[index].durationFrames;
    }
    return programState.frames.length - 1;
  }

  function renderProgramFrame(state, programState, frameIndex, childSelector) {
    childSelector = Number.isInteger(childSelector)
      ? childSelector : programState.program.selectedChildOrdinal;
    var cacheKey = programState.program.programId + ':' + frameIndex + ':child-' + childSelector;
    if (state.frames[cacheKey]) return state.frames[cacheKey];
    var frame = programState.frames[frameIndex];
    var fallbackCount = 0;
    var layers = frame.layers.map(function(layer) {
      var source = loadSprite(state, programState, layer.artId, childSelector);
      if (source.childFallback) fallbackCount++;
      var rgba = orientLayerPixels(
        displayWindowPixels(source, layer.width, layer.height),
        layer.width, layer.height, layer.flags);
      return {
        rgba: rgba,
        width: layer.width,
        height: layer.height,
        x: layer.drawOffsetX - programState.originX,
        y: layer.drawOffsetY - programState.originY,
        depth: layer.ordinal,
        drawOffsetX: layer.drawOffsetX,
        drawOffsetY: layer.drawOffsetY,
        scaleX: layer.scaleXRaw / 1024,
        scaleY: layer.scaleYRaw / 1024,
        flags: layer.flags
      };
    });
    var result = {
      width: programState.width,
      height: programState.height,
      rgba: OB64.cutsceneAssets.compositeLayers(layers, programState.width, programState.height),
      anchorX: -programState.originX,
      anchorY: -programState.originY,
      nativeLayers: layers.map(function(layer) {
        return {
          rgba: layer.rgba,
          width: layer.width,
          height: layer.height,
          drawOffsetX: layer.drawOffsetX,
          drawOffsetY: layer.drawOffsetY,
          scaleX: layer.scaleX,
          scaleY: layer.scaleY,
          depth: layer.depth,
          flags: layer.flags
        };
      }),
      frameToken: frame.frameToken,
      capability: 'preview-only',
      requestedChildOrdinal: childSelector,
      childFallbackCount: fallbackCount,
      warning: fallbackCount
        ? fallbackCount + ' layer resources lack child ' + childSelector + ' and use the native child-0 fallback.'
        : 'Actor variant selector ' + childSelector + ' is in range for every rendered layer.'
    };
    state.frames[cacheKey] = result;
    return result;
  }

  function bodyRouteIdentity(program) {
    return [program.artSource, program.flagA, program.flagB,
      program.ownerContext].join(':');
  }

  function bodyAnimationState(state) {
    if (state.bodyAnimations) return state.bodyAnimations;
    if (!OB64.animationArt || typeof OB64.animationArt.initialize !== 'function') {
      fail('The combat-style body-pose resource resolver is unavailable.');
    }
    state.bodyAnimations = OB64.animationArt.initialize(state.z64);
    if (!state.bodyAnimations.supported) {
      fail(state.bodyAnimations.unavailableReason ||
        'The combat-style body-pose resource corpus is unavailable.');
    }
    return state.bodyAnimations;
  }

  function directClassBodyRoute(state, program) {
    if (program.artSource !== program.ownerContext ||
        !Number.isInteger(program.artSource) || program.artSource < 0 ||
        program.flagA < 0 || program.flagA > 1 ||
        program.flagB < 0 || program.flagB > 1) {
      return null;
    }
    var handleIndex = program.artSource * 4 + program.flagA * 2 + program.flagB;
    if (handleIndex < 0 || handleIndex >= BODY_CLASS_HANDLE_COUNT) return null;
    var handles = OB64.art.readResource(state.z64, BODY_CLASS_HANDLE_RESOURCE_KEY);
    var handleOffset = BODY_CLASS_HANDLE_TABLE_OFFSET + handleIndex * 2;
    if (handleOffset + 2 > handles.storedLength) {
      fail('Body-pose class-handle table is truncated.');
    }
    var rawHandle = readU16(handles.stored, handleOffset);
    var descriptorSlot = (rawHandle & 0x0FFF) - 1;
    if (descriptorSlot < 0 || descriptorSlot >= BODY_DESCRIPTOR_COUNT) {
      fail('Body-pose class handle selects an invalid descriptor slot.');
    }
    var descriptorTable = OB64.art.readResource(state.z64, BODY_DESCRIPTOR_TABLE_KEY);
    if (descriptorTable.storedLength !== BODY_DESCRIPTOR_COUNT * 4) {
      fail('Body-pose descriptor table has an invalid extent.');
    }
    var descriptorKey = readU32(descriptorTable.stored, descriptorSlot * 4);
    var descriptor = OB64.art.readResource(state.z64, descriptorKey);
    if (descriptor.storedLength < 16 || descriptor.storedLength % 4) {
      fail('Body-pose descriptor is not a raw u32 member list.');
    }
    var controls = [0, 4, 8, 12].map(function(offset) {
      return readU32(descriptor.stored, offset);
    });
    if (controls.some(function(key) { return !key; })) {
      fail('Body-pose descriptor lacks a control resource.');
    }
    return {
      spec: {
        classId: program.artSource,
        className: 'Class 0x' + program.artSource.toString(16).toUpperCase(),
        rawMode: 0,
        descriptorKey: descriptorKey,
        descriptorMemberCount: descriptor.storedLength / 4,
        metadataKey: controls[0],
        poseKey: controls[1],
        configKey: controls[2],
        lookupKey: controls[3],
        selectedBodyChild: rawHandle >>> 12,
        route: { actorFields: {
          sourceArtId: program.artSource,
          rawOwnerContext: program.ownerContext,
          flagA: program.flagA,
          flagB: program.flagB
        } }
      },
      metadata: OB64.art.readCompressedResource(state.z64, controls[0]).decoded,
      pose: OB64.art.readCompressedResource(state.z64, controls[1]).decoded
    };
  }

  function prepareBodyRoute(state, program) {
    var identity = bodyRouteIdentity(program);
    if (state.bodyRoutes[identity]) return state.bodyRoutes[identity];
    var row = directClassBodyRoute(state, program);
    if (!row) {
      var animations = bodyAnimationState(state);
      var routes = animations.specs.concat(animations.dynamicArtRouteTemplates || []);
      row = routes.find(function(candidate) {
        var spec = candidate.spec || {};
        var fields = spec.route && spec.route.actorFields || {};
        return spec.rawMode === 0 && fields.sourceArtId === program.artSource &&
          fields.rawOwnerContext === program.ownerContext &&
          fields.flagA === program.flagA && fields.flagB === program.flagB;
      });
    }
    if (!row) {
      fail('Body-pose route ' + identity + ' is absent from the byte-mapped class-art corpus.');
    }
    var spec = row.spec;
    var config = OB64.art.readCompressedResource(state.z64, spec.configKey).decoded;
    var lookup = OB64.art.readCompressedResource(state.z64, spec.lookupKey).decoded;
    var source = {
      bank: 'body-route:' + identity,
      descriptorKey: spec.descriptorKey,
      descriptorMemberCount: spec.descriptorMemberCount,
      metadataKey: spec.metadataKey,
      poseKey: spec.poseKey,
      configKey: spec.configKey,
      lookupKey: spec.lookupKey,
      metadataDecodedLength: row.metadata.length,
      poseDecodedLength: row.pose.length,
      configDecodedLength: config.length,
      lookupDecodedLength: lookup.length,
      artCount: spec.descriptorMemberCount - 4,
      lookupBankCount: lookup.length / 0x200,
      selectedChildOrdinal: spec.selectedBodyChild
    };
    var prepared = {
      identity: identity,
      row: row,
      source: source,
      selectedChildOrdinal: spec.selectedBodyChild,
      className: spec.className
    };
    state.bodyRoutes[identity] = prepared;
    return prepared;
  }

  function prepareBodyProgram(state, bodyProgram) {
    var identity = bodyRouteIdentity(bodyProgram) + ':' + bodyProgram.selector + ':' +
      bodyProgram.displayedFrameToken;
    if (state.bodyPrograms[identity]) return state.bodyPrograms[identity];
    var route = prepareBodyRoute(state, bodyProgram);
    var bank = prepareBank(state, route.source);
    var pose = OB64.animationArt.parsePoseProgram(bank.pose, bodyProgram.selector,
      route.className + ' body pose');
    var sourceFrames = pose.frames.length ? pose.frames : [[
      bodyProgram.displayedFrameToken, 1
    ]];
    var originX = 0;
    var originY = 0;
    var endX = 0;
    var endY = 0;
    var bounded = false;
    var frames = sourceFrames.map(function(frame, sequenceIndex) {
      var parsed = OB64.animationArt.parseMetadataFrame(bank.metadata, frame[0]);
      parsed.layers.forEach(function(layer) {
        if (layer.artId >= route.source.artCount) {
          fail('Body-pose frame selects art outside its descriptor.');
        }
        var left = layer.drawOffsetX;
        var top = layer.drawOffsetY;
        var right = left + layer.width;
        var bottom = top + layer.height;
        if (!bounded) {
          originX = left; originY = top; endX = right; endY = bottom; bounded = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
      });
      return {
        sequenceIndex: sequenceIndex,
        frameToken: frame[0],
        durationFrames: Math.max(1, frame[1]),
        layers: parsed.layers
      };
    });
    if (!bounded || endX <= originX || endY <= originY) {
      fail('Body-pose displayed frame has no drawable bounds.');
    }
    var durationFrames = frames.reduce(function(total, frame) {
      return total + frame.durationFrames;
    }, 0);
    var prepared = {
      program: {
        programId: 'body-pose:' + identity,
        durationFrames: durationFrames,
        selectedChildOrdinal: route.selectedChildOrdinal
      },
      bank: bank,
      frames: frames,
      originX: originX,
      originY: originY,
      width: endX - originX,
      height: endY - originY,
      emptyPoseProgram: pose.recordCount === 0,
      poseLoop: pose.records.some(function(record) { return record.opcode === 0x04; }),
      route: route
    };
    state.bodyPrograms[identity] = prepared;
    return prepared;
  }

  function frameForBodyActor(state, actorState) {
    var bodyProgram = actorState.bodyPoseProgram;
    if (!Number.isInteger(bodyProgram.artSource) ||
        !Number.isInteger(bodyProgram.ownerContext)) return null;
    try {
      var prepared = prepareBodyProgram(state, bodyProgram);
      var frameIndex = frameIndexAt(prepared, actorState.poseFrame, prepared.poseLoop);
      var rendered = renderProgramFrame(state, prepared, frameIndex,
        prepared.route.selectedChildOrdinal);
      delete state.errors[actorState.poseId];
      return Object.assign({}, rendered, {
        capability: 'preview-only',
        bodyPoseProgram: Object.assign({}, bodyProgram),
        warning: prepared.emptyPoseProgram
          ? 'The native selector is empty; the initializer-cleared frame token 0 is displayed.'
          : 'The alternate body-pose decoder supplies this native frame sequence.'
      });
    } catch (error) {
      state.errors[actorState.poseId] = error && error.message || String(error);
      return null;
    }
  }

  function frameForActor(state, actorState) {
    if (actorState && actorState.bodyPoseProgram) {
      return frameForBodyActor(state, actorState);
    }
    if (!actorState || !actorState.poseId) return null;
    var program = Number.isInteger(actorState.bank) && Number.isInteger(actorState.animationKey) &&
      Number.isInteger(actorState.nativeFacing) && Number.isInteger(actorState.variantSelector) &&
      state.catalog.getPhysicalPoseProgram
      ? state.catalog.getPhysicalPoseProgram(actorState.bank, actorState.animationKey,
        actorState.nativeFacing, actorState.variantSelector) : null;
    program = program || state.catalog.getPoseProgram(actorState.poseId);
    if (!program || !program.frames.length) return null;
    try {
      var prepared = prepareProgram(state, program);
      var frameIndex = Number.isInteger(actorState.displayedFrameToken)
        ? prepared.frames.findIndex(function(frame) {
          return frame.frameToken === actorState.displayedFrameToken;
        }) : -1;
      if (frameIndex < 0) {
        frameIndex = frameIndexAt(prepared, actorState.poseFrame, actorState.poseLoop);
      }
      var rendered = renderProgramFrame(state, prepared, frameIndex, actorState.variantSelector);
      delete state.errors[actorState.poseId];
      return rendered;
    } catch (error) {
      state.errors[actorState.poseId] = error && error.message || String(error);
      return null;
    }
  }

  function framesForPreview(state, previewState) {
    var output = {};
    (previewState && previewState.actors || []).forEach(function(actor) {
      var frame = frameForActor(state, actor);
      if (frame) output[actor.id] = frame;
    });
    return output;
  }

  function framesForStageProps(state, stageProjection, previewFrame) {
    var stageProps = stageProjection && stageProjection.nativeSceneProps;
    if (!stageProps) return [];
    var placements = []
      .concat(stageProps.orthographicPlacements || [])
      .concat(stageProps.perspectivePlacements || []);
    return placements.map(function(placement) {
      var errorId = 'stage-prop:' + placement.id;
      try {
        var source = placement.source || stageProps.source;
        if (!source) fail('Mode-2 Stage prop lacks its native sprite descriptor.');
        var prepared = prepareStagePropProgram(state, source, placement);
        var elapsed = Math.max(0, Math.floor(previewFrame || 0) -
          (Number.isInteger(placement.startFrame) ? placement.startFrame : 0));
        var frameIndex = frameIndexAt(prepared, elapsed, prepared.poseLoop);
        var frame = renderProgramFrame(state, prepared, frameIndex,
          prepared.program.selectedChildOrdinal);
        delete state.errors[errorId];
        return { placement: placement, frame: frame };
      } catch (error) {
        state.errors[errorId] = error && error.message || String(error);
        return null;
      }
    }).filter(Boolean);
  }

  OB64.cutsceneSprites = {
    Error: SpriteError,
    create: create,
    frameForActor: frameForActor,
    framesForPreview: framesForPreview,
    framesForStageProps: framesForStageProps
  };
})(window.OB64);
