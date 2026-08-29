// OB64 combat-animation route assignment and isolated sequence copies.
//
// A shared assignment adds one exact class/action/body-route OBSO record.
// A separated assignment also clones the selected sequence into the bounded
// native-art arena, adds it to a copy-on-write descriptor, and repoints only
// the selected class/body route to that descriptor.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var A = OB64.art;
  var M = OB64.animationArt;
  var DESCRIPTOR_ROOT_KEY = 0x003B6CD0;
  var DESCRIPTOR_ROOT_COUNT = 208;
  var CLASS_HANDLE_RESOURCE_KEY = 0x00315736;
  var CLASS_HANDLE_TABLE_OFFSET = 0x24;
  var CLASS_HANDLE_COUNT = 688;
  var MAX_TRANSFORM_DIMENSION = 512;
  var MAX_TRANSFORM_PIXELS = MAX_TRANSFORM_DIMENSION * MAX_TRANSFORM_DIMENSION;

  function SequenceCopyError(message) {
    this.name = 'SequenceCopyError';
    this.message = message;
  }
  SequenceCopyError.prototype = Object.create(Error.prototype);
  SequenceCopyError.prototype.constructor = SequenceCopyError;

  function fail(message) { throw new SequenceCopyError(message); }

  function touchState(state) {
    if (!state) return;
    state.revision = (Number(state.revision) || 0) + 1;
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function equalWords(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function concat(parts) {
    var size = parts.reduce(function(total, part) { return total + part.length; }, 0);
    var output = new Uint8Array(size), cursor = 0;
    parts.forEach(function(part) { output.set(part, cursor); cursor += part.length; });
    return output;
  }

  function clonePair(pair) {
    return {
      normalSelector: Number(pair.normalSelector),
      blockedSelector: Number(pair.blockedSelector)
    };
  }

  function bodyFlagsFor(animation) {
    var fields = animation && animation.spec && animation.spec.route &&
      animation.spec.route.actorFields;
    if (fields && Number.isInteger(fields.flagA) && Number.isInteger(fields.flagB)) {
      return fields.flagA * 2 + fields.flagB;
    }
    var match = String(animation && animation.spec && animation.spec.variantLabel || '')
      .match(/flags\s+(\d)\/(\d)/);
    if (!match) fail('selected animation does not expose its body-route flags');
    return Number(match[1]) * 2 + Number(match[2]);
  }

  function flagLabel(bodyFlags) {
    return Math.floor(bodyFlags / 2) + '/' + (bodyFlags & 1);
  }

  function laneFor(animation) {
    if (animation && animation.spec && animation.spec.idleSequence) return 'idle';
    return animation.spec.rawMode === 2 ? 'blocked' : 'normal';
  }

  function routeId(classId, actionId, bodyFlags, laneKey) {
    return [Number(classId), Number(actionId), Number(bodyFlags), laneKey].join(':');
  }

  function rowId(classId, actionId, bodyFlags) {
    return [Number(classId), Number(actionId), Number(bodyFlags)].join(':');
  }

  function groupId(classId, bodyFlags) {
    return [Number(classId), Number(bodyFlags)].join(':');
  }

  function donorRef(animation) {
    while (animation && animation.spec && animation.spec.separatedCopy &&
        animation.donorAnimation) {
      animation = animation.donorAnimation;
    }
    return {
      key: animation.effectiveMapping && animation.effectiveMapping.candidateKey
        ? animation.effectiveMapping.candidateKey : animation.key,
      classId: Number(animation.spec.classId),
      actionId: Number(animation.spec.actionId),
      bodyFlags: bodyFlagsFor(animation),
      rawMode: Number(animation.spec.rawMode),
      selector: Number(animation.spec.selector),
      descriptorKey: Number(animation.spec.descriptorKey),
      selectedBodyChild: Number(animation.spec.selectedBodyChild),
      idleSequence: !!animation.spec.idleSequence
    };
  }

  function idleLoopFrames(animation) {
    var records = animation && animation.poseProgram &&
      animation.poseProgram.records;
    if (!Array.isArray(records) || !records.length) {
      fail('selector 0x00 has no readable pose records');
    }
    var loop = records[records.length - 1];
    if (loop.opcode !== 0x04 || !loop.operands ||
        !Number.isInteger(loop.operands[0])) {
      fail('selector 0x00 does not end with a readable idle-loop jump');
    }
    var frameRecords = records.filter(function(record) {
      return record.opcode === 0x01;
    });
    var firstLoopFrame = frameRecords.findIndex(function(record) {
      return record.ordinal >= loop.operands[0];
    });
    if (firstLoopFrame < 0 || firstLoopFrame >= animation.frames.length) {
      fail('selector 0x00 idle-loop jump does not reach a frame');
    }
    return animation.frames.slice(firstLoopFrame).map(function(frame, index) {
      return Object.assign({}, frame, { sequenceIndex: index });
    });
  }

  function routeReferenceView(animation, reference) {
    if (Number(animation.spec.classId) === Number(reference.classId) &&
        Number(animation.spec.actionId) === Number(reference.actionId) &&
        Number(animation.spec.rawMode) === Number(reference.rawMode) &&
        Number(animation.spec.selector) === Number(reference.selector) &&
        !!animation.spec.idleSequence === !!reference.idleSequence) {
      return animation;
    }
    var actionInfo = OB64.combatAnimationOverrides &&
      OB64.combatAnimationOverrides.actionInfo
      ? OB64.combatAnimationOverrides.actionInfo(Number(reference.actionId)) : null;
    var actionName = OB64.ACTION_TEMPLATE_LABELS &&
      OB64.ACTION_TEMPLATE_LABELS[Number(reference.actionId)]
      ? OB64.ACTION_TEMPLATE_LABELS[Number(reference.actionId)]
      : (actionInfo ? actionInfo.name : 'Action ' +
        Number(reference.actionId).toString(16).padStart(2, '0'));
    var idle = !!reference.idleSequence;
    return Object.assign({}, animation, {
      frames: idle ? idleLoopFrames(animation) : animation.frames,
      spec: Object.assign({}, animation.spec, {
        classId: Number(reference.classId),
        actionId: Number(reference.actionId),
        actionName: idle ? 'Idle / Rest' : actionName,
        rawMode: Number(reference.rawMode),
        modeLabel: idle ? 'Idle loop' : 'Raw mode ' + Number(reference.rawMode),
        selector: Number(reference.selector),
        idleSequence: idle
      })
    });
  }

  function resolveRef(animationState, reference) {
    if (!reference) fail('animation sequence reference is missing');
    var direct = animationState.byKey[reference.key] ||
      (animationState.idleAnimationsByKey &&
        animationState.idleAnimationsByKey[reference.key]) ||
      animationState.selectorCandidates[reference.key];
    if (direct && Number(direct.spec.classId) === Number(reference.classId) &&
        bodyFlagsFor(direct) === Number(reference.bodyFlags) &&
        Number(direct.spec.selectedBodyChild) ===
          Number(reference.selectedBodyChild) &&
        direct.spec.descriptorKey === reference.descriptorKey &&
        direct.spec.selector === reference.selector &&
        direct.spec.rawMode === reference.rawMode) {
      return routeReferenceView(direct, reference);
    }
    var candidates = animationState.specs.concat(
      animationState.dynamicArtRouteTemplates || []).filter(function(animation) {
      return animation.spec.classId === reference.classId &&
        bodyFlagsFor(animation) === reference.bodyFlags &&
        animation.spec.descriptorKey === reference.descriptorKey;
    });
    var exact = candidates.find(function(animation) {
      return animation.spec.actionId === reference.actionId &&
        animation.spec.rawMode === reference.rawMode &&
        animation.spec.selector === reference.selector;
    });
    if (exact) return routeReferenceView(exact, reference);
    var template = candidates[0];
    if (!template || typeof animationState.resolveSelectorCandidate !== 'function') {
      fail('referenced animation sequence is not present in the loaded ROM corpus');
    }
    return routeReferenceView(animationState.resolveSelectorCandidate(
      template, reference.selector, reference.rawMode), reference);
  }

  function baselineDecoded(animationState, source) {
    var edit = animationState.edits[source.key];
    return source.editable && edit
      ? M.buildDecoded(source, edit.children)
      : source.sprite.decoded.slice();
  }

  function normalizedBodyDecoded(source, decoded, childOrdinal) {
    var sprite = M.parseSpriteObject(decoded, source.resourceKey);
    childOrdinal = M.childOrdinalOrFallback(source, childOrdinal);
    var sourceChild = sprite.children[childOrdinal];
    var lanes = M.materializeChildLanes(sprite, childOrdinal);
    var outer = decoded.slice(0, 8);
    outer[2] = 1;
    var header = new Uint8Array(8);
    A.writeU16(header, 0, 0x5554);
    header[2] = 0;
    header[3] = (lanes.first ? 1 : 0) | (lanes.second ? 2 : 0) |
      (lanes.lookup ? 4 : 0);
    A.writeU16(header, 4, sourceChild.widthField);
    A.writeU16(header, 6, sourceChild.heightField);
    return concat([outer, header].concat(
      lanes.first ? [lanes.first] : [],
      lanes.second ? [lanes.second] : [],
      lanes.lookup ? [lanes.lookup] : []));
  }

  function cloneSource(animationState, separation, source, selectedChild, ordinal) {
    var decoded = baselineDecoded(animationState, source);
    var normalizeBody = source.sourceRole === 'body';
    if (normalizeBody) {
      decoded = normalizedBodyDecoded(source, decoded, selectedChild);
    }
    var sprite = M.parseSpriteObject(decoded, source.resourceKey);
    var key = 'separated:' + separation.id + ':source:' + ordinal;
    var editableOrdinals = source.editable
      ? sprite.children.map(function(child) { return child.ordinal; }) : [];
    var clone = Object.assign({}, source, {
      key: key,
      bindingId: key,
      physicalSourceId: key,
      separationId: separation.id,
      separationSourceOrdinal: ordinal,
      onDemandBinding: false,
      descriptorEntryOffset: null,
      descriptorMemberIndex: null,
      resource: {
        key: source.resourceKey,
        entry: -1,
        storedLength: 0,
        stored: new Uint8Array(0),
        decoded: decoded
      },
      sprite: sprite,
      childOrdinal: normalizeBody ? 0 : M.childOrdinalOrFallback(source, selectedChild),
      selectableChildOrdinals: source.weaponSelectable
        ? sprite.children.map(function(child) { return child.ordinal; }) : [],
      editableChildOrdinals: editableOrdinals,
      originalChildren: {},
      displayChildren: {},
      embeddedPalettes: {},
      visibleChildren: {},
      animationKeys: [],
      animationLabels: [],
      legacyKeys: [],
      legacyAnimationKeys: [],
      usageFrames: [],
      usageFramesByAnimation: {},
      palette: source.palette && source.palette.slice
        ? source.palette.slice() : source.palette
    });
    return clone;
  }

  function jsonClone(value) {
    if (value === undefined) return null;
    var text = JSON.stringify(value);
    return text === undefined ? null : JSON.parse(text);
  }

  function sourceStructure(animationState, source) {
    var children = {};
    (source.editableChildOrdinals || []).forEach(function(childOrdinal) {
      var pixels = M.currentEdit(animationState, source.key, childOrdinal);
      children[childOrdinal] = {
        indices: pixels.indices.slice(), intensity: pixels.intensity.slice()
      };
    });
    return {
      resourceKey: Number(source.resourceKey),
      artId: Number.isInteger(source.artId) ? source.artId : 0,
      sourceRole: String(source.sourceRole || 'unknown'),
      selectorPolicy: Number.isInteger(source.selectorPolicy)
        ? source.selectorPolicy : 0,
      childSelectionPolicy: jsonClone(source.childSelectionPolicy),
      palettePolicy: jsonClone(source.palettePolicy),
      elementSelection: jsonClone(source.elementSelection),
      formatKind: String(source.formatKind || ''),
      editable: !!source.editable,
      lockedReason: String(source.lockedReason || ''),
      childOrdinal: Number.isInteger(source.childOrdinal) ? source.childOrdinal : 0,
      weaponSelectable: !!source.weaponSelectable,
      width: source.sprite.width,
      height: source.sprite.height,
      childCount: source.sprite.childCount,
      decoded: source.sprite.decoded.slice(),
      palette: source.palette && source.palette.slice
        ? source.palette.slice() : source.palette,
      children: children
    };
  }

  function layerStructure(layer, source) {
    return {
      sourceOrdinal: source.separationSourceOrdinal,
      artId: Number.isInteger(layer.artId) ? layer.artId : 0,
      drawOffsetX: Number(layer.drawOffsetX),
      drawOffsetY: Number(layer.drawOffsetY),
      width: Number(layer.width),
      height: Number(layer.height),
      flags: Number(layer.flags),
      scaleXRaw: Number(layer.scaleXRaw),
      scaleYRaw: Number(layer.scaleYRaw),
      requestedChildOrdinal: Number.isInteger(layer.requestedChildOrdinal)
        ? layer.requestedChildOrdinal : 0,
      selectedChildOrdinal: Number.isInteger(layer.selectedChildOrdinal)
        ? layer.selectedChildOrdinal : 0
    };
  }

  function captureStructure(animationState, separation) {
    var animation = separation && separation.syntheticAnimation;
    if (!animation) fail('private animation sequence is missing');
    var sources = {};
    Object.keys(animation.artByKey).forEach(function(key) {
      var source = animation.artByKey[key];
      if (!Number.isInteger(source.separationSourceOrdinal)) {
        fail('private animation source lacks its stable ordinal');
      }
      sources[source.separationSourceOrdinal] = sourceStructure(animationState, source);
    });
    return {
      sources: sources,
      poseProgram: animation.poseProgram.program.slice(),
      frames: animation.frames.map(function(frame) {
        return {
          sequenceIndex: frame.sequenceIndex,
          sourceFrameIndex: stableFrameIndex(frame),
          token: frame.token,
          ticks: frame.ticks,
          layers: frame.layers.map(function(layer) {
            var source = animation.artByKey[layer.sourceKey];
            if (!source) fail('private animation layer lacks its source');
            return layerStructure(layer, source);
          })
        };
      })
    };
  }

  function unregisterSyntheticSources(animationState, separation) {
    (separation.syntheticSourceKeys || []).forEach(function(key) {
      delete animationState.artByKey[key];
      delete animationState.edits[key];
      Object.keys(animationState.history).forEach(function(historyKey) {
        if (historyKey.indexOf(key + '#child-') === 0) {
          delete animationState.history[historyKey];
        }
      });
    });
    separation.syntheticSourceKeys = [];
  }

  function sourceFromStructure(animationState, separation, ordinal, row) {
    var key = 'separated:' + separation.id + ':source:' + ordinal;
    var decoded = row.decoded.slice();
    var sprite = M.parseSpriteObject(decoded, row.resourceKey);
    var source = {
      key: key,
      bindingId: key,
      physicalSourceId: key,
      separationId: separation.id,
      separationSourceOrdinal: ordinal,
      onDemandBinding: false,
      binding: null,
      physicalSource: null,
      sourceRole: row.sourceRole,
      selectorPolicy: row.selectorPolicy,
      childSelectionPolicy: jsonClone(row.childSelectionPolicy),
      palettePolicy: jsonClone(row.palettePolicy),
      elementSelection: jsonClone(row.elementSelection),
      formatKind: row.formatKind,
      editable: !!row.editable,
      lockedReason: row.lockedReason || '',
      animationKey: '',
      animationLabel: '',
      animationKeys: [],
      animationLabels: [],
      legacyKeys: [],
      legacyAnimationKeys: [],
      artId: row.artId,
      descriptorKey: null,
      descriptorMemberIndex: null,
      descriptorEntryOffset: null,
      resourceKey: row.resourceKey,
      resource: {
        key: row.resourceKey,
        entry: -1,
        storedLength: 0,
        stored: new Uint8Array(0),
        decoded: decoded
      },
      sprite: sprite,
      childOrdinal: M.childOrdinalOrFallback({ sprite: sprite }, row.childOrdinal),
      weaponSelectable: !!row.weaponSelectable,
      selectableChildOrdinals: row.weaponSelectable
        ? sprite.children.map(function(child) { return child.ordinal; }) : [],
      editableChildOrdinals: row.editable
        ? sprite.children.map(function(child) { return child.ordinal; }) : [],
      originalChildren: {},
      displayChildren: {},
      embeddedPalettes: {},
      visibleChildren: {},
      lookupBank: 0,
      palette: row.palette && row.palette.slice ? row.palette.slice() : row.palette,
      usageFrames: [],
      usageFramesByAnimation: {}
    };
    animationState.artByKey[key] = source;
    separation.syntheticSourceKeys.push(key);
    return source;
  }

  function renumberLayers(frame) {
    frame.layers.forEach(function(layer, ordinal) { layer.ordinal = ordinal; });
  }

  function stableFrameIndex(frame) {
    return Number.isInteger(frame && frame.sourceFrameIndex)
      ? frame.sourceFrameIndex : Number(frame && frame.sequenceIndex);
  }

  function poseProgramForFrames(donorAnimation, frames) {
    var pose = donorAnimation && donorAnimation.poseProgram;
    if (!pose || !Array.isArray(pose.records) ||
        !Array.isArray(donorAnimation.frames)) {
      fail('private animation donor lacks its body program');
    }
    var donorOrdinalByFrame = {}, wanted = {}, previous = -1;
    donorAnimation.frames.forEach(function(frame, ordinal) {
      var stable = stableFrameIndex(frame);
      if (!Number.isInteger(stable) || Object.prototype.hasOwnProperty.call(
          donorOrdinalByFrame, stable)) {
        fail('private animation donor has an invalid stable frame identity');
      }
      donorOrdinalByFrame[stable] = ordinal;
    });
    frames.forEach(function(frame) {
      var stable = stableFrameIndex(frame);
      var ordinal = donorOrdinalByFrame[stable];
      if (!Number.isInteger(ordinal) || ordinal <= previous) {
        fail('private animation frames no longer follow their body program');
      }
      previous = ordinal;
      wanted[stable] = true;
    });
    if (!frames.length) fail('private animation sequence must retain at least one frame');

    var idle = !!(donorAnimation.spec && donorAnimation.spec.idleSequence);
    var loopRecord = idle ? pose.records[pose.records.length - 1] : null;
    if (idle && (!loopRecord || loopRecord.opcode !== 0x04 ||
        !loopRecord.operands || !Number.isInteger(loopRecord.operands[0]))) {
      fail('private idle donor lacks its final loop jump');
    }
    var loopStart = idle ? loopRecord.operands[0] : null;
    var recordBytes = [], frameOrdinal = 0, firstKeptLoopRecord = null;
    pose.records.forEach(function(record) {
      var keep = true;
      var exposedFrame = false;
      if (record.opcode === 0x01) {
        exposedFrame = !idle || record.ordinal >= loopStart;
        if (exposedFrame) {
          var donorFrame = donorAnimation.frames[frameOrdinal++];
          if (!donorFrame) {
            fail('private animation body program has too many editable frame commands');
          }
          keep = !!wanted[stableFrameIndex(donorFrame)];
        }
      }
      if (!keep) return;
      var bytes = new Uint8Array(record.width);
      bytes[0] = record.opcode;
      bytes.set(record.operands, 1);
      if (idle && exposedFrame && firstKeptLoopRecord === null) {
        firstKeptLoopRecord = recordBytes.length;
      }
      recordBytes.push({ bytes: bytes, exposedFrame: exposedFrame });
    });
    if (frameOrdinal !== donorAnimation.frames.length || recordBytes.length > 0xFF) {
      fail('private animation body program no longer matches its frames');
    }
    if (idle && firstKeptLoopRecord === null) {
      fail('private idle animation must retain at least one loop frame');
    }
    if (idle) {
      var finalRecord = recordBytes[recordBytes.length - 1];
      if (!finalRecord || finalRecord.bytes[0] !== 0x04 ||
          finalRecord.bytes.length < 2) {
        fail('private idle animation lost its final loop jump');
      }
      finalRecord.bytes[1] = firstKeptLoopRecord;
    }
    var length = 1 + recordBytes.reduce(function(total, row) {
      return total + row.bytes.length;
    }, 0);
    var program = new Uint8Array(length), records = [], poseFrames = [];
    program[0] = recordBytes.length;
    var cursor = 1, start = Number(pose.start) || 0;
    recordBytes.forEach(function(row, ordinal) {
      var bytes = row.bytes;
      program.set(bytes, cursor);
      var operands = Array.prototype.slice.call(bytes, 1);
      records.push({
        ordinal: ordinal, offset: start + cursor, opcode: bytes[0],
        width: bytes.length, operands: operands
      });
      if (bytes[0] === 0x01 && row.exposedFrame) {
        poseFrames.push([operands[0], operands[1]]);
      }
      cursor += bytes.length;
    });
    if (poseFrames.length !== frames.length) {
      fail('private animation body program did not retain every selected frame');
    }
    return {
      selector: pose.selector, stateCount: pose.stateCount,
      start: start, end: start + program.length, program: program,
      recordCount: records.length, records: records, frames: poseFrames
    };
  }

  function visiblePoseFrameRecords(pose, idle) {
    var records = pose && pose.records;
    if (!Array.isArray(records)) fail('private animation lacks readable pose records');
    var frameRecords = records.filter(function(record) {
      return record.opcode === 0x01;
    });
    if (!idle) return frameRecords;
    var loop = records[records.length - 1];
    if (!loop || loop.opcode !== 0x04 || !loop.operands ||
        !Number.isInteger(loop.operands[0])) {
      fail('private idle animation lacks its final loop jump');
    }
    return frameRecords.filter(function(record) {
      return record.ordinal >= loop.operands[0];
    });
  }

  function validatePrivatePoseControls(template, pose, idle) {
    if (!template || !Array.isArray(template.records)) {
      fail('private animation donor lacks readable control records');
    }
    var expected = template.records.filter(function(record) {
      return record.opcode !== 0x01;
    });
    var actual = pose.records.filter(function(record) {
      return record.opcode !== 0x01;
    });
    if (actual.length !== expected.length) {
      fail('private animation changed its non-frame control records');
    }
    actual.forEach(function(record, index) {
      var donorRecord = expected[index];
      if (record.opcode !== donorRecord.opcode ||
          record.width !== donorRecord.width ||
          record.operands.length !== donorRecord.operands.length) {
        fail('private animation changed a non-frame control record');
      }
      record.operands.forEach(function(value, operand) {
        if (record.opcode === 0x04 && operand === 0) {
          var target = pose.records[value];
          if (!target || target.opcode !== 0x01) {
            fail('private animation jump does not target a frame command');
          }
          return;
        }
        if (value !== donorRecord.operands[operand]) {
          fail('private animation changed a non-frame control operand');
        }
      });
    });
    if (!idle) return;
    var donorVisible = visiblePoseFrameRecords(template, true);
    var actualVisible = visiblePoseFrameRecords(pose, true);
    var donorHiddenFrames = template.records.filter(function(record) {
      return record.opcode === 0x01;
    }).length - donorVisible.length;
    var actualHiddenFrames = pose.records.filter(function(record) {
      return record.opcode === 0x01;
    }).length - actualVisible.length;
    var loop = pose.records[pose.records.length - 1];
    if (donorHiddenFrames !== actualHiddenFrames || !actualVisible.length ||
        loop.operands[0] !== actualVisible[0].ordinal) {
      fail('private idle animation changed its loop boundary');
    }
  }

  function decodePrivatePoseProgram(template, program, idle, frames) {
    if (!(program instanceof Uint8Array) || program.length < 2) {
      fail('private animation body program is invalid');
    }
    var wrapped = new Uint8Array(4 + program.length);
    A.writeU32(wrapped, 0, 4);
    wrapped.set(program, 4);
    var parsed;
    try {
      parsed = M.parsePoseProgram(wrapped, 0, 'private animation body program');
    } catch (error) {
      fail(error && error.message ? error.message : String(error));
    }
    var start = Number(template && template.start) || 0;
    parsed.records.forEach(function(record) {
      record.offset = start + record.offset - 4;
    });
    parsed.selector = template.selector;
    parsed.stateCount = template.stateCount;
    parsed.start = start;
    parsed.end = start + program.length;
    parsed.program = program.slice();
    var visible = visiblePoseFrameRecords(parsed, idle);
    if (!Array.isArray(frames) || visible.length !== frames.length) {
      fail('private animation body program does not match its visible frame count');
    }
    parsed.frames = visible.map(function(record, index) {
      var frame = frames[index];
      if (!frame || !Number.isInteger(frame.token) || frame.token < 0 ||
          frame.token > 255 || !Number.isInteger(frame.ticks) ||
          frame.ticks < 0 || frame.ticks > 255 ||
          record.operands[0] !== frame.token ||
          record.operands[1] !== frame.ticks) {
        fail('private animation frame ' + (index + 1) +
          ' does not match its body-program command');
      }
      return [frame.token, frame.ticks];
    });
    validatePrivatePoseControls(template, parsed, idle);
    return parsed;
  }

  function poseProgramWithFrames(pose, frames, idle) {
    var records = visiblePoseFrameRecords(pose, idle);
    if (records.length !== frames.length) {
      fail('private animation frame order no longer matches its body program');
    }
    var program = pose.program.slice();
    records.forEach(function(record, index) {
      var frame = frames[index];
      var relative = record.offset - pose.start;
      program[relative + 1] = integerInRange(
        frame.token, 0, 255, 'frame token');
      program[relative + 2] = integerInRange(
        frame.ticks, 0, 255, 'frame ticks');
    });
    return decodePrivatePoseProgram(pose, program, idle, frames);
  }

  function poseProgramWithInsertedFrame(pose, frames, idle, afterFrameIndex) {
    if (pose.recordCount >= 255) {
      fail('this body program already contains the maximum 255 records');
    }
    var visible = visiblePoseFrameRecords(pose, idle);
    var anchor = visible[afterFrameIndex];
    if (!anchor) fail('selected frame command is unavailable');
    var insertAt = anchor.offset - pose.start + anchor.width;
    var insertOrdinal = anchor.ordinal + 1;
    var program = new Uint8Array(pose.program.length + 3);
    program.set(pose.program.slice(0, insertAt), 0);
    program.set([0x01, 0, 0], insertAt);
    program.set(pose.program.slice(insertAt), insertAt + 3);
    program[0] = pose.recordCount + 1;
    pose.records.forEach(function(record) {
      if (record.opcode !== 0x04 || !record.operands ||
          !Number.isInteger(record.operands[0]) ||
          record.operands[0] < insertOrdinal) return;
      var relative = record.offset - pose.start;
      if (relative >= insertAt) relative += 3;
      program[relative + 1] = record.operands[0] + 1;
    });
    var addedFrame = frames[afterFrameIndex + 1];
    program[insertAt + 1] = integerInRange(
      addedFrame && addedFrame.token, 0, 255, 'frame token');
    program[insertAt + 2] = integerInRange(
      addedFrame && addedFrame.ticks, 0, 255, 'frame ticks');
    return decodePrivatePoseProgram(pose, program, idle, frames);
  }

  function poseProgramWithoutFrame(pose, frames, idle, frameIndex) {
    var visible = visiblePoseFrameRecords(pose, idle);
    var removed = visible[frameIndex];
    if (!removed) fail('selected frame command is unavailable');
    var removeAt = removed.offset - pose.start;
    var program = new Uint8Array(pose.program.length - removed.width);
    program.set(pose.program.slice(0, removeAt), 0);
    program.set(pose.program.slice(removeAt + removed.width), removeAt);
    program[0] = pose.recordCount - 1;
    pose.records.forEach(function(record) {
      if (record === removed || record.opcode !== 0x04 ||
          !record.operands || !Number.isInteger(record.operands[0])) return;
      var relative = record.offset - pose.start;
      if (relative > removeAt) relative -= removed.width;
      var target = record.operands[0];
      if (target === removed.ordinal) {
        var nextFrame = visible.slice(frameIndex + 1).find(function(candidate) {
          return candidate !== removed;
        });
        var previousFrame = visible.slice(0, frameIndex).reverse()
          .find(function(candidate) { return candidate !== removed; });
        if (nextFrame) target = nextFrame.ordinal - 1;
        else if (previousFrame) target = previousFrame.ordinal;
        else fail('private animation jump would lose its only frame target');
      } else if (target > removed.ordinal) {
        target--;
      }
      program[relative + 1] = target;
    });
    return decodePrivatePoseProgram(pose, program, idle, frames);
  }

  function syntheticLayerBounds(source, layer) {
    if (source &&
        source.childSelectionPolicy === 'cutscene-actor-appearance') {
      var scaleX = Number(layer.scaleXRaw) / 1024;
      var scaleY = Number(layer.scaleYRaw) / 1024;
      if (Number.isFinite(scaleX) && scaleX >= 0 &&
          Number.isFinite(scaleY) && scaleY >= 0) {
        var left = Math.round(layer.drawOffsetX * scaleX);
        var top = Math.round(layer.drawOffsetY * scaleY);
        var width = Math.max(1, Math.round(layer.width * scaleX));
        var height = Math.max(1, Math.round(layer.height * scaleY));
        return { left: left, top: top, right: left + width, bottom: top + height };
      }
    }
    return {
      left: layer.drawOffsetX,
      top: layer.drawOffsetY,
      right: layer.drawOffsetX + layer.width,
      bottom: layer.drawOffsetY + layer.height
    };
  }

  function rebuildSyntheticIndexes(animation) {
    var hasBounds = false, originX = 0, originY = 0, endX = 0, endY = 0;
    var used = {}, artById = {};
    Object.keys(animation.artByKey).forEach(function(key) {
      var source = animation.artByKey[key];
      source.animationKeys = [animation.key];
      source.animationLabels = [animation.spec.className + ' ' +
        animation.spec.actionName];
      source.usageFrames = [];
      source.usageFramesByAnimation = {};
      source.usageFramesByAnimation[animation.key] = source.usageFrames;
    });
    animation.frames.forEach(function(frame) {
      renumberLayers(frame);
      frame.layers.forEach(function(layer) {
        var source = animation.artByKey[layer.sourceKey];
        if (!source) fail('private animation layer points to a missing source');
        used[layer.sourceKey] = true;
        artById[layer.artId] = source;
        if (source.usageFrames.indexOf(frame.sequenceIndex) < 0) {
          source.usageFrames.push(frame.sequenceIndex);
        }
        var bounds = syntheticLayerBounds(source, layer);
        var left = bounds.left, top = bounds.top;
        var right = bounds.right, bottom = bounds.bottom;
        if (!hasBounds) {
          originX = left; originY = top; endX = right; endY = bottom;
          hasBounds = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
      });
    });
    if (!hasBounds) fail('private animation sequence has no drawable layers');
    Object.keys(animation.artByKey).forEach(function(key) {
      if (!used[key]) delete animation.artByKey[key];
    });
    animation.artById = artById;
    animation.canvas = {
      originX: originX, originY: originY, endX: endX, endY: endY,
      width: endX - originX, height: endY - originY
    };
    animation.spec.canvas = Object.assign({}, animation.canvas);
  }

  function layerFromStructure(row, source, ordinal) {
    return {
      ordinal: ordinal,
      artId: row.artId,
      drawOffsetX: row.drawOffsetX,
      drawOffsetY: row.drawOffsetY,
      width: row.width,
      height: row.height,
      flags: row.flags,
      scaleXRaw: row.scaleXRaw,
      scaleYRaw: row.scaleYRaw,
      metadataOffset: null,
      sourceKey: source.key,
      bindingId: source.bindingId,
      physicalSourceId: source.physicalSourceId,
      sourceRole: source.sourceRole,
      resourceKey: source.resourceKey,
      lookupBank: 0,
      childCount: source.sprite.childCount,
      requestedChildOrdinal: row.requestedChildOrdinal,
      selectedChildOrdinal: M.childOrdinalOrFallback(
        source, row.selectedChildOrdinal)
    };
  }

  function applyStructure(rom, separation, structure) {
    var animationState = rom.art.animations;
    var animation = separation.syntheticAnimation;
    if (!animation || !structure || !structure.sources || !structure.frames) {
      fail('private animation structure is incomplete');
    }
    unregisterSyntheticSources(animationState, separation);
    animation.artByKey = {};
    var sources = {};
    Object.keys(structure.sources).sort(function(left, right) {
      return Number(left) - Number(right);
    }).forEach(function(ordinalText) {
      var ordinal = Number(ordinalText);
      var source = sourceFromStructure(
        animationState, separation, ordinal, structure.sources[ordinalText]);
      animation.artByKey[source.key] = source;
      sources[ordinal] = source;
    });
    var donor = animation.donorAnimation, donorFrames = {};
    donor.frames.forEach(function(frame) {
      donorFrames[stableFrameIndex(frame)] = frame;
    });
    var storedPose = structure.poseProgram instanceof Uint8Array
      ? structure.poseProgram : null;
    var stableFrames = {};
    animation.frames = structure.frames.map(function(row, frameIndex) {
      var donorFrame = donorFrames[row.sourceFrameIndex];
      var stable = Number(row.sourceFrameIndex);
      var validStoredFrame = storedPose && Number.isInteger(stable) &&
        stable >= 0 && stable <= 0xFFFF && !stableFrames[stable] &&
        Number.isInteger(row.token) && row.token >= 0 && row.token <= 255 &&
        Number.isInteger(row.ticks) && row.ticks >= 0 && row.ticks <= 255;
      var validLegacyFrame = !storedPose && donorFrame &&
        row.token === donorFrame.token && row.ticks === donorFrame.ticks;
      if (row.sequenceIndex !== frameIndex || !row.layers.length ||
          (!validStoredFrame && !validLegacyFrame)) {
        fail('private animation frame ' + (frameIndex + 1) +
          ' no longer matches its body program');
      }
      stableFrames[stable] = true;
      return {
        sequenceIndex: frameIndex,
        sourceFrameIndex: row.sourceFrameIndex,
        token: row.token,
        ticks: row.ticks,
        metadataTarget: donorFrame ? donorFrame.metadataTarget : null,
        layers: row.layers.map(function(layer, ordinal) {
          var source = sources[layer.sourceOrdinal];
          if (!source) {
            fail('private animation frame references a missing source ordinal');
          }
          return layerFromStructure(layer, source, ordinal);
        })
      };
    });
    animation.poseProgram = storedPose
      ? decodePrivatePoseProgram(donor.poseProgram, storedPose,
        separation.laneKey === 'idle', animation.frames)
      : poseProgramForFrames(donor, animation.frames);
    animation.spec.frames = animation.frames.map(function(frame) {
      return [frame.token, frame.ticks];
    });
    rebuildSyntheticIndexes(animation);
    Object.keys(structure.sources).forEach(function(ordinalText) {
      var source = sources[Number(ordinalText)];
      var children = structure.sources[ordinalText].children || {};
      Object.keys(children).forEach(function(childText) {
        M.setEdit(animationState, source.key, Number(childText),
          children[childText].indices, children[childText].intensity,
          { history: false });
      });
    });
    separation.syntheticSourceKeys = Object.keys(animation.artByKey);
    return animation;
  }

  function snapshotModifiedPixels(animationState, animation) {
    if (!animation || !animation.spec || !animation.spec.separatedCopy) return null;
    var snapshot = {};
    Object.keys(animation.artByKey).forEach(function(key) {
      var source = animation.artByKey[key];
      if (!source.editable || !Number.isInteger(source.separationSourceOrdinal)) return;
      var children = {};
      source.editableChildOrdinals.forEach(function(childOrdinal) {
        var pixels = M.currentEdit(animationState, source.key, childOrdinal);
        children[childOrdinal] = {
          indices: pixels.indices.slice(), intensity: pixels.intensity.slice()
        };
      });
      snapshot[source.separationSourceOrdinal] = children;
    });
    return snapshot;
  }

  function restoreModifiedPixels(animationState, separation, snapshot) {
    if (!snapshot || !separation || !separation.syntheticAnimation) return;
    var byOrdinal = {};
    Object.keys(separation.syntheticAnimation.artByKey).forEach(function(key) {
      var source = separation.syntheticAnimation.artByKey[key];
      if (Number.isInteger(source.separationSourceOrdinal)) {
        byOrdinal[source.separationSourceOrdinal] = source;
      }
    });
    Object.keys(snapshot).forEach(function(ordinalText) {
      var source = byOrdinal[Number(ordinalText)];
      if (!source) fail('modified sequence source no longer matches its copied donor');
      Object.keys(snapshot[ordinalText]).forEach(function(childText) {
        var pixels = snapshot[ordinalText][childText];
        M.setEdit(animationState, source.key, Number(childText),
          pixels.indices, pixels.intensity, { history: false });
      });
    });
  }

  function cleanupSynthetic(animationState, separation) {
    unregisterSyntheticSources(animationState, separation);
    delete separation.syntheticAnimation;
  }

  function buildSynthetic(rom, separation, donorAnimation) {
    var animationState = rom.art.animations;
    cleanupSynthetic(animationState, separation);
    var target = resolveRef(animationState, separation.targetRef);
    var donor = donorAnimation || resolveRef(animationState, separation.donorRef);
    var selectedBySource = {};
    donor.frames.forEach(function(frame) {
      frame.layers.forEach(function(layer) {
        if (!Object.prototype.hasOwnProperty.call(selectedBySource, layer.sourceKey)) {
          selectedBySource[layer.sourceKey] = layer.selectedChildOrdinal;
        }
      });
    });
    var sourceMap = {}, artByKey = {}, syntheticKeys = [];
    Object.keys(donor.artByKey).sort().forEach(function(sourceKey, ordinal) {
      var clone = cloneSource(animationState, separation, donor.artByKey[sourceKey],
        selectedBySource[sourceKey], ordinal);
      sourceMap[sourceKey] = clone;
      artByKey[clone.key] = clone;
      animationState.artByKey[clone.key] = clone;
      syntheticKeys.push(clone.key);
    });
    var frames = donor.frames.map(function(frame, frameIndex) {
      return {
        sequenceIndex: frameIndex,
        sourceFrameIndex: stableFrameIndex(frame),
        token: frame.token,
        ticks: frame.ticks,
        metadataTarget: frame.metadataTarget,
        layers: frame.layers.map(function(layer) {
          var clone = sourceMap[layer.sourceKey];
          return Object.assign({}, layer, {
            sourceKey: clone.key,
            bindingId: clone.bindingId,
            physicalSourceId: clone.physicalSourceId,
            resourceKey: clone.resourceKey,
            childCount: clone.sprite.childCount,
            requestedChildOrdinal: clone.sourceRole === 'body'
              ? 0 : layer.requestedChildOrdinal,
            selectedChildOrdinal: clone.sourceRole === 'body'
              ? 0 : M.childOrdinalOrFallback(clone, layer.selectedChildOrdinal)
          });
        })
      };
    });
    var syntheticKey = 'separated:' + separation.id;
    Object.keys(artByKey).forEach(function(sourceKey) {
      var source = artByKey[sourceKey];
      source.animationKeys = [syntheticKey];
      source.animationLabels = [target.spec.className + ' ' + target.spec.actionName];
      source.usageFramesByAnimation[syntheticKey] = [];
      frames.forEach(function(frame) {
        if (frame.layers.some(function(layer) { return layer.sourceKey === sourceKey; })) {
          source.usageFramesByAnimation[syntheticKey].push(frame.sequenceIndex);
        }
      });
    });
    var spec = Object.assign({}, donor.spec, {
      key: syntheticKey,
      id: syntheticKey,
      classId: separation.classId,
      className: target.spec.className,
      actionId: separation.actionId,
      actionName: target.spec.actionName,
      rawMode: separation.laneKey === 'blocked' ? 2 : 0,
      modeLabel: separation.laneKey === 'idle' ? 'Idle loop' :
        (separation.laneKey === 'blocked' ? 'Raw mode 2' : 'Raw mode 0'),
      variantLabel: target.spec.variantLabel,
      selector: separation.selector,
      descriptorKey: target.spec.descriptorKey,
      descriptorMemberCount: target.spec.descriptorMemberCount,
      metadataKey: target.spec.metadataKey,
      poseKey: target.spec.poseKey,
      configKey: target.spec.configKey,
      lookupKey: target.spec.lookupKey,
      selectedBodyChild: target.spec.selectedBodyChild,
      selectedChildOrdinal: target.spec.selectedBodyChild,
      route: target.spec.route,
      frames: frames.map(function(frame) { return [frame.token, frame.ticks]; }),
      frozenParity: null,
      idleSequence: separation.laneKey === 'idle',
      separatedCopy: true
    });
    var synthetic = {
      key: syntheticKey,
      corpusId: syntheticKey,
      spec: spec,
      descriptor: target.descriptor,
      members: target.members,
      metadata: target.metadata,
      pose: target.pose,
      poseProgram: poseProgramForFrames(donor, frames),
      config: target.config,
      lookupBanks: target.lookupBanks,
      frames: frames,
      artByKey: artByKey,
      artById: {},
      equipmentGroup: donor.equipmentGroup,
      canvas: Object.assign({}, donor.canvas),
      mappingStatus: target.mappingStatus,
      separationId: separation.id,
      targetAnimation: target,
      donorAnimation: donor,
      effectiveMapping: {
        source: 'separated', overridden: true,
        classId: separation.classId, actionId: separation.actionId,
        selector: separation.selector, laneKey: separation.laneKey,
        ranks: [], assigned: true, candidateKey: donor.key
      }
    };
    frames.forEach(function(frame) {
      frame.layers.forEach(function(layer) {
        synthetic.artById[layer.artId] = artByKey[layer.sourceKey];
      });
    });
    separation.syntheticSourceKeys = syntheticKeys;
    separation.syntheticAnimation = synthetic;
    if (separation.structure) {
      return applyStructure(rom, separation, separation.structure);
    }
    return synthetic;
  }

  function initialize(rom) {
    var supported = !!(rom && rom.art && rom.art.supported &&
      rom.art.animations && rom.art.animations.supported &&
      rom.combatAnimationOverrides && rom.combatAnimationOverrides.supported &&
      !rom.combatAnimationOverrides.readOnly);
    var state = {
      supported: supported,
      unavailableReason: supported ? '' :
        'Separated animation sequences require the verified Rev 0 art and selector lanes.',
      separations: {},
      routeBaselines: {},
      revision: 0,
      dirty: false
    };
    rom.animationSequences = state;
    if (rom.art) rom.art.sequenceCopies = state;
    return state;
  }

  function separationsForGroup(state, classId, bodyFlags) {
    return Object.keys(state.separations).map(function(key) {
      return state.separations[key];
    }).filter(function(row) {
      return row.classId === Number(classId) && row.bodyFlags === Number(bodyFlags);
    }).sort(function(left, right) {
      return left.actionId - right.actionId ||
        (left.laneKey === right.laneKey ? 0 : (left.laneKey === 'normal' ? -1 : 1));
    });
  }

  function separationsForRow(state, classId, actionId, bodyFlags) {
    return separationsForGroup(state, classId, bodyFlags).filter(function(row) {
      return row.actionId === Number(actionId);
    });
  }

  function repointSelectorConsumers(rom, classId, bodyFlags, selectorMoves) {
    if (!Object.keys(selectorMoves).length) return;
    var state = rom.combatAnimationOverrides;
    state.desired = state.desired.map(function(row) {
      if (row.classId !== Number(classId) || row.bodyFlags !== Number(bodyFlags)) {
        return row;
      }
      var next = Object.assign({}, row);
      if (Object.prototype.hasOwnProperty.call(selectorMoves, row.normalSelector)) {
        next.normalSelector = selectorMoves[row.normalSelector];
      }
      if (Object.prototype.hasOwnProperty.call(selectorMoves, row.blockedSelector)) {
        next.blockedSelector = selectorMoves[row.blockedSelector];
      }
      return next;
    });
    OB64.combatAnimationOverrides.refresh(state);
    Object.keys(rom.animationSequences.routeBaselines).forEach(function(id) {
      var parts = id.split(':').map(Number);
      if (parts[0] !== Number(classId) || parts[2] !== Number(bodyFlags)) return;
      var baseline = rom.animationSequences.routeBaselines[id];
      ['entry', 'pair'].forEach(function(key) {
        var row = baseline[key];
        if (!row) return;
        if (Object.prototype.hasOwnProperty.call(
          selectorMoves, row.normalSelector)) {
          row.normalSelector = selectorMoves[row.normalSelector];
        }
        if (Object.prototype.hasOwnProperty.call(
          selectorMoves, row.blockedSelector)) {
          row.blockedSelector = selectorMoves[row.blockedSelector];
        }
      });
    });
  }

  function updateSelectorsAndSynthetic(rom, classId, bodyFlags) {
    var rows = separationsForGroup(rom.animationSequences, classId, bodyFlags);
    if (!rows.length) return;
    var snapshots = {}, oldSelectors = {};
    rows.forEach(function(row) {
      oldSelectors[row.id] = row.selector;
      snapshots[row.id] = snapshotModifiedPixels(
        rom.art.animations, row.syntheticAnimation);
    });
    var target = resolveRef(rom.art.animations, rows[0].targetRef);
    var base = target.poseProgram.stateCount;
    var appendedOrdinal = 0;
    rows.forEach(function(row) {
      row.selector = row.laneKey === 'idle' ? 0 : base + appendedOrdinal++;
      if (row.selector > 255) fail('separated selector exceeds the game u8 range');
    });
    var selectorMoves = {};
    rows.forEach(function(row) {
      var previous = oldSelectors[row.id];
      if (Number.isInteger(previous) && previous >= base && previous !== row.selector) {
        selectorMoves[previous] = row.selector;
      }
    });
    repointSelectorConsumers(rom, classId, bodyFlags, selectorMoves);
    rows.forEach(function(row) {
      buildSynthetic(rom, row);
      restoreModifiedPixels(rom.art.animations, row, snapshots[row.id]);
    });
  }

  function syncRow(rom, classId, actionId, bodyFlags) {
    if (Number(actionId) < 0) return;
    var state = rom.animationSequences;
    var id = rowId(classId, actionId, bodyFlags);
    var baseline = state.routeBaselines[id];
    var rows = separationsForRow(state, classId, actionId, bodyFlags);
    var api = OB64.combatAnimationOverrides;
    if (!rows.length) {
      if (!baseline) return;
      if (baseline.entry) api.setEntry(rom.combatAnimationOverrides, baseline.entry);
      else api.removeEntry(rom.combatAnimationOverrides, classId, actionId, bodyFlags);
      delete state.routeBaselines[id];
      return;
    }
    var pair = clonePair(baseline.entry || baseline.pair);
    rows.forEach(function(row) {
      if (row.laneKey === 'blocked') pair.blockedSelector = row.selector;
      else pair.normalSelector = row.selector;
    });
    api.setEntry(rom.combatAnimationOverrides, {
      classId: classId, actionId: actionId, bodyFlags: bodyFlags,
      normalSelector: pair.normalSelector, blockedSelector: pair.blockedSelector
    });
  }

  function captureBaseline(rom, classId, actionId, bodyFlags, pair) {
    var state = rom.animationSequences, id = rowId(classId, actionId, bodyFlags);
    if (state.routeBaselines[id]) return;
    var exact = OB64.combatAnimationOverrides.exactEntry(
      rom.combatAnimationOverrides, classId, actionId, bodyFlags);
    state.routeBaselines[id] = {
      entry: exact ? Object.assign({}, exact) : null,
      pair: clonePair(pair)
    };
  }

  function capacityCheck(rom, classId, actionId, bodyFlags) {
    var api = OB64.combatAnimationOverrides;
    if (api.exactEntry(rom.combatAnimationOverrides, classId, actionId, bodyFlags)) return;
    if (rom.combatAnimationOverrides.desired.length >= api.capacity) {
      fail('Combat animation assignment capacity is full. Remove an assignment first.');
    }
  }

  function assignmentTarget(donorAnimation, targetAnimation) {
    var target = targetAnimation || donorAnimation;
    if (!target || !target.spec) fail('animation assignment target is missing');
    return {
      animation: target,
      classId: Number(target.spec.classId),
      actionId: Number(target.spec.actionId),
      bodyFlags: bodyFlagsFor(target),
      laneKey: laneFor(target)
    };
  }

  function sharedAssignmentIssue(donorAnimation, targetAnimation) {
    if (!donorAnimation || !donorAnimation.spec ||
        !targetAnimation || !targetAnimation.spec) {
      return 'animation sequence or assignment target is missing';
    }
    if (Number(donorAnimation.spec.classId) !==
        Number(targetAnimation.spec.classId)) {
      return 'shared assignment requires the same class';
    }
    if (Number(donorAnimation.spec.descriptorKey) !==
          Number(targetAnimation.spec.descriptorKey) ||
        Number(donorAnimation.spec.selectedBodyChild) !==
          Number(targetAnimation.spec.selectedBodyChild)) {
      return 'direct assignment requires the same sprite resource and body appearance';
    }
    return '';
  }

  function validateSharedAssignment(rom, donorAnimation, target) {
    var targetAnimation = target.animation;
    var issue = sharedAssignmentIssue(donorAnimation, targetAnimation);
    if (issue) fail(issue);
    if (donorAnimation.spec.separatedCopy) {
      var modified = selectorAnimation(rom, target.classId,
        target.bodyFlags, donorAnimation.spec.selector);
      if (!modified || modified.separationId !== donorAnimation.separationId) {
        fail('modified sequence selector is not present in the target art route');
      }
      return;
    }
    if (rom && rom.art && rom.art.animations &&
        typeof rom.art.animations.resolveSelectorCandidate === 'function') {
      rom.art.animations.resolveSelectorCandidate(targetAnimation,
        donorAnimation.spec.selector, targetAnimation.spec.rawMode);
    }
  }

  function separateAndAssign(rom, animation, pair, targetAnimation) {
    var state = rom.animationSequences;
    if (!state || !state.supported) fail(state && state.unavailableReason ||
      'Separated animation sequences are unavailable');
    var target = assignmentTarget(animation, targetAnimation);
    var classId = target.classId;
    var actionId = target.actionId;
    var bodyFlags = target.bodyFlags;
    var laneKey = target.laneKey;
    var idle = laneKey === 'idle';
    var id = routeId(classId, actionId, bodyFlags, laneKey);
    var donorSnapshot = snapshotModifiedPixels(rom.art.animations, animation);
    var donorSeparation = animation && animation.spec &&
      animation.spec.separatedCopy
      ? separationFor(animation, state) : null;
    var donorStructure = donorSeparation
      ? captureStructure(rom.art.animations, donorSeparation) : null;
    if (!idle) {
      capacityCheck(rom, classId, actionId, bodyFlags);
      captureBaseline(rom, classId, actionId, bodyFlags, pair);
    }
    var existing = state.separations[id];
    if (existing) cleanupSynthetic(rom.art.animations, existing);
    state.separations[id] = {
      id: id,
      classId: classId,
      actionId: actionId,
      bodyFlags: bodyFlags,
      laneKey: laneKey,
      selector: 0,
      targetRef: donorRef(target.animation),
      donorRef: donorRef(animation),
      syntheticSourceKeys: []
    };
    updateSelectorsAndSynthetic(rom, classId, bodyFlags);
    if (donorStructure) {
      applyStructure(rom, state.separations[id], donorStructure);
    }
    restoreModifiedPixels(rom.art.animations, state.separations[id], donorSnapshot);
    state.separations[id].structure = captureStructure(
      rom.art.animations, state.separations[id]);
    separationsForGroup(state, classId, bodyFlags).filter(function(row) {
      return row.laneKey !== 'idle';
    }).forEach(function(row) {
      syncRow(rom, row.classId, row.actionId, row.bodyFlags);
    });
    state.dirty = true;
    touchState(state);
    return state.separations[id];
  }

  function separationConsumers(rom, separation) {
    var rows = rom && rom.combatAnimationOverrides &&
      rom.combatAnimationOverrides.desired || [];
    var consumers = [];
    rows.forEach(function(row) {
      if (row.classId !== separation.classId ||
          row.bodyFlags !== separation.bodyFlags) return;
      [['normal', row.normalSelector], ['blocked', row.blockedSelector]]
        .forEach(function(lane) {
          if (lane[1] !== separation.selector) return;
          if (row.actionId === separation.actionId &&
              lane[0] === separation.laneKey) return;
          consumers.push({ actionId: row.actionId, laneKey: lane[0] });
        });
    });
    return consumers;
  }

  function removeSeparation(rom, separation) {
    var state = rom.animationSequences;
    var consumers = separationConsumers(rom, separation);
    if (consumers.length) {
      fail('Modified sequence is still assigned to ' + consumers.length +
        ' other action or mode target' + (consumers.length === 1 ? '' : 's'));
    }
    cleanupSynthetic(rom.art.animations, separation);
    delete state.separations[separation.id];
    updateSelectorsAndSynthetic(rom, separation.classId, separation.bodyFlags);
    var rowKeys = {};
    rowKeys[rowId(separation.classId, separation.actionId, separation.bodyFlags)] = true;
    separationsForGroup(state, separation.classId, separation.bodyFlags)
      .forEach(function(row) { rowKeys[rowId(row.classId, row.actionId, row.bodyFlags)] = true; });
    Object.keys(rowKeys).forEach(function(id) {
      var parts = id.split(':').map(Number);
      syncRow(rom, parts[0], parts[1], parts[2]);
    });
    state.dirty = true;
    touchState(state);
  }

  function assignShared(rom, animation, pair, targetAnimation) {
    var state = rom.animationSequences;
    var target = assignmentTarget(animation, targetAnimation);
    if (target.laneKey === 'idle') {
      fail('Idle loops require a separated descriptor copy');
    }
    validateSharedAssignment(rom, animation, target);
    var classId = target.classId;
    var actionId = target.actionId;
    var bodyFlags = target.bodyFlags;
    var laneKey = target.laneKey;
    var id = routeId(classId, actionId, bodyFlags, laneKey);
    var existing = state && state.separations[id];
    if (existing) removeSeparation(rom, existing);
    capacityCheck(rom, classId, actionId, bodyFlags);
    var api = OB64.combatAnimationOverrides;
    var remaining = state
      ? separationsForRow(state, classId, actionId, bodyFlags) : [];
    if (remaining.length) {
      var baseline = state.routeBaselines[rowId(classId, actionId, bodyFlags)];
      if (!baseline) fail('separated animation route lacks its selector baseline');
      var sharedPair = clonePair(baseline.entry || baseline.pair);
      if (laneKey === 'blocked') sharedPair.blockedSelector = animation.spec.selector;
      else sharedPair.normalSelector = animation.spec.selector;
      baseline.entry = {
        classId: classId, actionId: actionId, bodyFlags: bodyFlags,
        normalSelector: sharedPair.normalSelector,
        blockedSelector: sharedPair.blockedSelector
      };
      syncRow(rom, classId, actionId, bodyFlags);
      state.dirty = true;
      return api.exactEntry(rom.combatAnimationOverrides,
        classId, actionId, bodyFlags);
    }
    var current = api.exactEntry(rom.combatAnimationOverrides,
      classId, actionId, bodyFlags);
    var next = current ? clonePair(current) : clonePair(pair);
    if (laneKey === 'blocked') next.blockedSelector = animation.spec.selector;
    else next.normalSelector = animation.spec.selector;
    api.setEntry(rom.combatAnimationOverrides, {
      classId: classId, actionId: actionId, bodyFlags: bodyFlags,
      normalSelector: next.normalSelector, blockedSelector: next.blockedSelector
    });
    return api.exactEntry(rom.combatAnimationOverrides, classId, actionId, bodyFlags);
  }

  function copyFrom(rom, separation, donorAnimation) {
    if (!separation || !rom.animationSequences.separations[separation.id]) {
      fail('Copy From requires a separated sequence');
    }
    var donorSnapshot = snapshotModifiedPixels(
      rom.art.animations, donorAnimation);
    cleanupSynthetic(rom.art.animations, separation);
    delete separation.structure;
    separation.donorRef = donorRef(donorAnimation);
    buildSynthetic(rom, separation, donorAnimation);
    restoreModifiedPixels(rom.art.animations, separation, donorSnapshot);
    separation.structure = captureStructure(rom.art.animations, separation);
    rom.animationSequences.dirty = true;
    touchState(rom.animationSequences);
    return separation.syntheticAnimation;
  }

  function requirePrivateSequence(rom, separation) {
    if (!rom || !rom.animationSequences || !separation ||
        rom.animationSequences.separations[separation.id] !== separation ||
        !separation.syntheticAnimation) {
      fail('structural frame editing requires a separated private sequence');
    }
    return separation.syntheticAnimation;
  }

  function integerInRange(value, minimum, maximum, label) {
    value = Number(value);
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail(label + ' must be an integer from ' + minimum + ' through ' + maximum);
    }
    return value;
  }

  function privateFrame(animation, frameIndex) {
    frameIndex = integerInRange(frameIndex, 0, animation.frames.length - 1,
      'frame index');
    return animation.frames[frameIndex];
  }

  function privateLayer(frame, layerOrdinal) {
    layerOrdinal = integerInRange(layerOrdinal, 0, frame.layers.length - 1,
      'layer index');
    return frame.layers[layerOrdinal];
  }

  function nextSourceOrdinal(animation) {
    return Object.keys(animation.artByKey).reduce(function(maximum, key) {
      var source = animation.artByKey[key];
      return Number.isInteger(source.separationSourceOrdinal)
        ? Math.max(maximum, source.separationSourceOrdinal + 1) : maximum;
    }, 0);
  }

  function appendClonedSource(rom, separation, donorAnimation, donorLayer) {
    var animation = requirePrivateSequence(rom, separation);
    var donorSource = donorAnimation && donorAnimation.artByKey &&
      donorAnimation.artByKey[donorLayer.sourceKey];
    if (!donorSource) fail('selected source layer is unavailable');
    var ordinal = nextSourceOrdinal(animation);
    var clone = cloneSource(rom.art.animations, separation, donorSource,
      donorLayer.selectedChildOrdinal, ordinal);
    animation.artByKey[clone.key] = clone;
    rom.art.animations.artByKey[clone.key] = clone;
    separation.syntheticSourceKeys.push(clone.key);
    return clone;
  }

  function transformDimensions(width, height, label) {
    width = integerInRange(width, 1, MAX_TRANSFORM_DIMENSION,
      label + ' width');
    height = integerInRange(height, 1, MAX_TRANSFORM_DIMENSION,
      label + ' height');
    if (width * height > MAX_TRANSFORM_PIXELS) {
      fail(label + ' cannot exceed ' + MAX_TRANSFORM_PIXELS + ' pixels');
    }
    return { width: width, height: height };
  }

  function validateIndexedPixels(indices, intensity, width, height, label) {
    var pixels = width * height;
    if (!(indices instanceof Uint8Array) || indices.length !== pixels ||
        !(intensity instanceof Uint8Array) || intensity.length !== pixels) {
      fail(label + ' pixels do not match ' + width + 'x' + height);
    }
    for (var pixel = 0; pixel < intensity.length; pixel++) {
      if (intensity[pixel] > 15) {
        fail(label + ' contains an intensity above 15');
      }
    }
  }

  function normalizedRotationDegrees(value) {
    if (value === 'left') value = -90;
    if (value === 'right') value = 90;
    value = Number(value);
    if (!Number.isFinite(value)) fail('rotation angle must be a number');
    value %= 360;
    if (value < 0) value += 360;
    return value;
  }

  function rotateIndexedPixels(indices, intensity, width, height, degrees) {
    var dimensions = transformDimensions(width, height, 'source sprite');
    width = dimensions.width;
    height = dimensions.height;
    validateIndexedPixels(indices, intensity, width, height, 'source sprite');
    degrees = normalizedRotationDegrees(degrees);
    if (degrees === 0) {
      return {
        width: width, height: height,
        indices: indices.slice(), intensity: intensity.slice()
      };
    }
    var targetWidth, targetHeight;
    if (degrees === 90 || degrees === 270) {
      targetWidth = height; targetHeight = width;
    } else if (degrees === 180) {
      targetWidth = width; targetHeight = height;
    } else {
      var radians = degrees * Math.PI / 180;
      var absoluteCosine = Math.abs(Math.cos(radians));
      var absoluteSine = Math.abs(Math.sin(radians));
      targetWidth = Math.ceil((width - 1) * absoluteCosine +
        (height - 1) * absoluteSine - 1e-10) + 1;
      targetHeight = Math.ceil((width - 1) * absoluteSine +
        (height - 1) * absoluteCosine - 1e-10) + 1;
      transformDimensions(targetWidth, targetHeight, 'rotated sprite');
    }
    var targetIndices = new Uint8Array(targetWidth * targetHeight);
    var targetIntensity = new Uint8Array(targetWidth * targetHeight);
    if (degrees === 90 || degrees === 180 || degrees === 270) {
      for (var exactY = 0; exactY < height; exactY++) {
        for (var exactX = 0; exactX < width; exactX++) {
          var exactSourcePixel = exactY * width + exactX;
          var exactTargetX, exactTargetY;
          if (degrees === 90) {
            exactTargetX = height - 1 - exactY; exactTargetY = exactX;
          } else if (degrees === 180) {
            exactTargetX = width - 1 - exactX;
            exactTargetY = height - 1 - exactY;
          } else {
            exactTargetX = exactY; exactTargetY = width - 1 - exactX;
          }
          var exactTargetPixel = exactTargetY * targetWidth + exactTargetX;
          targetIndices[exactTargetPixel] = indices[exactSourcePixel];
          targetIntensity[exactTargetPixel] = intensity[exactSourcePixel];
        }
      }
      return {
        width: targetWidth,
        height: targetHeight,
        indices: targetIndices,
        intensity: targetIntensity
      };
    }
    var angle = degrees * Math.PI / 180;
    var cosine = Math.cos(angle), sine = Math.sin(angle);
    var sourceCenterX = (width - 1) / 2;
    var sourceCenterY = (height - 1) / 2;
    var targetCenterX = (targetWidth - 1) / 2;
    var targetCenterY = (targetHeight - 1) / 2;
    for (var targetY = 0; targetY < targetHeight; targetY++) {
      for (var targetX = 0; targetX < targetWidth; targetX++) {
        var relativeX = targetX - targetCenterX;
        var relativeY = targetY - targetCenterY;
        var sourceX = Math.round(cosine * relativeX +
          sine * relativeY + sourceCenterX);
        var sourceY = Math.round(-sine * relativeX +
          cosine * relativeY + sourceCenterY);
        if (sourceX < 0 || sourceX >= width ||
            sourceY < 0 || sourceY >= height) continue;
        var sourcePixel = sourceY * width + sourceX;
        var targetPixel = targetY * targetWidth + targetX;
        targetIndices[targetPixel] = indices[sourcePixel];
        targetIntensity[targetPixel] = intensity[sourcePixel];
      }
    }
    return {
      width: targetWidth,
      height: targetHeight,
      indices: targetIndices,
      intensity: targetIntensity
    };
  }

  function resizeIndexedPixels(indices, intensity, width, height,
      targetWidth, targetHeight) {
    var sourceDimensions = transformDimensions(width, height, 'source sprite');
    var targetDimensions = transformDimensions(
      targetWidth, targetHeight, 'resized sprite');
    width = sourceDimensions.width;
    height = sourceDimensions.height;
    targetWidth = targetDimensions.width;
    targetHeight = targetDimensions.height;
    validateIndexedPixels(indices, intensity, width, height, 'source sprite');
    var targetIndices = new Uint8Array(targetWidth * targetHeight);
    var targetIntensity = new Uint8Array(targetWidth * targetHeight);
    for (var y = 0; y < targetHeight; y++) {
      var sourceY = Math.min(height - 1,
        Math.floor(y * height / targetHeight));
      for (var x = 0; x < targetWidth; x++) {
        var sourceX = Math.min(width - 1,
          Math.floor(x * width / targetWidth));
        var sourcePixel = sourceY * width + sourceX;
        var targetPixel = y * targetWidth + x;
        targetIndices[targetPixel] = indices[sourcePixel];
        targetIntensity[targetPixel] = intensity[sourcePixel];
      }
    }
    return {
      width: targetWidth,
      height: targetHeight,
      indices: targetIndices,
      intensity: targetIntensity
    };
  }

  function indexedSpriteDecoded(width, height, children, sharedPalette) {
    width = integerInRange(width, 1, 0xFFFF, 'generated sprite width');
    height = integerInRange(height, 1, 0xFFFF, 'generated sprite height');
    if (!Array.isArray(children) || !children.length || children.length > 255) {
      fail('generated sprite must contain 1 through 255 children');
    }
    if (!(sharedPalette instanceof Uint16Array) || sharedPalette.length !== 256) {
      fail('generated sprite must contain one complete 256-entry palette');
    }
    var firstStride = M.rowBytes(1, width);
    var secondStride = M.rowBytes(0, width);
    var firstSize = firstStride * height;
    var secondSize = secondStride * height;
    var childRows = children.map(function(child, ordinal) {
      validateIndexedPixels(child.indices, child.intensity,
        width, height, 'generated sprite child ' + ordinal);
      var palette = child.palette;
      if (!(palette instanceof Uint16Array) || palette.length !== 256) {
        fail('generated sprite child ' + ordinal +
          ' must contain one complete 256-entry palette');
      }
      return {
        indices: child.indices,
        intensity: child.intensity,
        palette: palette,
        embeddedPalette: !equalWords(palette, sharedPalette)
      };
    });
    var length = 8 + childRows.reduce(function(total, child) {
      return total + 8 + firstSize + secondSize +
        (child.embeddedPalette ? 0x200 : 0);
    }, 0);
    var decoded = new Uint8Array(length);
    A.writeU16(decoded, 0, 0x5554);
    decoded[2] = childRows.length;
    decoded[3] = 0x05;
    A.writeU16(decoded, 4, width);
    A.writeU16(decoded, 6, height);
    var cursor = 8;
    childRows.forEach(function(child, ordinal) {
      A.writeU16(decoded, cursor, 0x5554);
      decoded[cursor + 2] = ordinal;
      decoded[cursor + 3] = 0x03 | (child.embeddedPalette ? 0x04 : 0);
      A.writeU16(decoded, cursor + 4, width);
      A.writeU16(decoded, cursor + 6, height);
      cursor += 8;
      var firstStart = cursor;
      var secondStart = firstStart + firstSize;
      for (var y = 0; y < height; y++) {
        var sourceRow = y * width;
        decoded.set(child.indices.subarray(sourceRow, sourceRow + width),
          firstStart + y * firstStride);
        for (var x = 0; x < width; x += 2) {
          var high = child.intensity[sourceRow + x];
          var low = x + 1 < width ? child.intensity[sourceRow + x + 1] : 0;
          decoded[secondStart + y * secondStride + (x >>> 1)] =
            (high << 4) | low;
        }
      }
      cursor = secondStart + secondSize;
      if (child.embeddedPalette) {
        for (var paletteIndex = 0; paletteIndex < 256; paletteIndex++) {
          A.writeU16(decoded, cursor + paletteIndex * 2,
            child.palette[paletteIndex]);
        }
        cursor += 0x200;
      }
    });
    return decoded;
  }

  function importedSpriteDecoded(width, height, indices, intensity) {
    var palette = new Uint16Array(256);
    return indexedSpriteDecoded(width, height, [{
      indices: indices, intensity: intensity, palette: palette
    }], palette);
  }

  function appendImportedFrameSource(rom, separation, prepared, templateLayer) {
    var animation = requirePrivateSequence(rom, separation);
    var width = integerInRange(prepared && prepared.targetWidth,
      1, 0xFFFF, 'imported frame width');
    var height = integerInRange(prepared && prepared.targetHeight,
      1, 0xFFFF, 'imported frame height');
    if (!(prepared.paletteWords instanceof Uint16Array) ||
        prepared.paletteWords.length !== 256) {
      fail('imported frame must contain one complete 256-entry palette');
    }
    var decoded = importedSpriteDecoded(
      width, height, prepared.indices, prepared.intensity);
    var ordinal = nextSourceOrdinal(animation);
    var key = 'separated:' + separation.id + ':source:' + ordinal;
    var sprite = M.parseSpriteObject(decoded, 0);
    var source = {
      key: key,
      bindingId: key,
      physicalSourceId: key,
      separationId: separation.id,
      separationSourceOrdinal: ordinal,
      onDemandBinding: false,
      binding: null,
      physicalSource: null,
      sourceRole: 'body',
      selectorPolicy: 0,
      childSelectionPolicy: null,
      palettePolicy: null,
      elementSelection: null,
      formatKind: 'indexed-ci8',
      editable: true,
      lockedReason: '',
      animationKey: '',
      animationLabel: '',
      animationKeys: [],
      animationLabels: [],
      legacyKeys: [],
      legacyAnimationKeys: [],
      artId: Number.isInteger(templateLayer && templateLayer.artId)
        ? templateLayer.artId : 0,
      descriptorKey: null,
      descriptorMemberIndex: null,
      descriptorEntryOffset: null,
      resourceKey: 0,
      resource: {
        key: 0, entry: -1, storedLength: 0,
        stored: new Uint8Array(0), decoded: decoded
      },
      sprite: sprite,
      childOrdinal: 0,
      weaponSelectable: false,
      selectableChildOrdinals: [],
      editableChildOrdinals: [0],
      originalChildren: {
        0: {
          indices: prepared.indices.slice(),
          intensity: prepared.intensity.slice()
        }
      },
      displayChildren: {},
      embeddedPalettes: {},
      visibleChildren: {},
      lookupBank: 0,
      palette: prepared.paletteWords.slice(),
      usageFrames: [],
      usageFramesByAnimation: {}
    };
    animation.artByKey[key] = source;
    rom.art.animations.artByKey[key] = source;
    separation.syntheticSourceKeys.push(key);
    return source;
  }

  function appendTransformedSource(rom, separation, source, transform) {
    var animation = requirePrivateSequence(rom, separation);
    if (!source || !source.editable || source.formatKind !== 'indexed-ci8') {
      fail('only editable CI8 plus I4 sprite layers can be transformed');
    }
    var sharedPalette = source.palette instanceof Uint16Array &&
      source.palette.length === 256 ? source.palette.slice() : null;
    if (!sharedPalette) fail('selected sprite lacks its complete 256-entry palette');
    var childRows = source.sprite.children.map(function(child) {
      if (source.editableChildOrdinals.indexOf(child.ordinal) < 0) {
        fail('selected sprite child ' + child.ordinal + ' is not editable');
      }
      var pixels = M.currentEdit(
        rom.art.animations, source.key, child.ordinal);
      var result = transform(pixels, child.ordinal);
      if (!result || !Number.isInteger(result.width) ||
          !Number.isInteger(result.height)) {
        fail('sprite transform returned invalid dimensions');
      }
      return {
        width: result.width,
        height: result.height,
        indices: result.indices,
        intensity: result.intensity,
        palette: new Uint16Array(M.childPalette(source, child.ordinal))
      };
    });
    var width = childRows[0].width, height = childRows[0].height;
    childRows.forEach(function(child, ordinal) {
      if (child.width !== width || child.height !== height) {
        fail('sprite transform gave child ' + ordinal +
          ' different dimensions');
      }
    });
    var decoded = indexedSpriteDecoded(
      width, height, childRows, sharedPalette);
    var sprite = M.parseSpriteObject(decoded, source.resourceKey);
    var ordinal = nextSourceOrdinal(animation);
    var key = 'separated:' + separation.id + ':source:' + ordinal;
    var childOrdinals = sprite.children.map(function(child) {
      return child.ordinal;
    });
    var originalChildren = {};
    childRows.forEach(function(child, childOrdinal) {
      originalChildren[childOrdinal] = {
        indices: child.indices.slice(),
        intensity: child.intensity.slice()
      };
    });
    var transformed = Object.assign({}, source, {
      key: key,
      bindingId: key,
      physicalSourceId: key,
      separationId: separation.id,
      separationSourceOrdinal: ordinal,
      onDemandBinding: false,
      binding: null,
      physicalSource: null,
      descriptorKey: null,
      descriptorMemberIndex: null,
      descriptorEntryOffset: null,
      resource: {
        key: source.resourceKey,
        entry: -1,
        storedLength: 0,
        stored: new Uint8Array(0),
        decoded: decoded
      },
      sprite: sprite,
      childOrdinal: M.childOrdinalOrFallback(source, source.childOrdinal),
      selectableChildOrdinals: source.weaponSelectable
        ? childOrdinals.slice() : [],
      editableChildOrdinals: childOrdinals.slice(),
      originalChildren: originalChildren,
      displayChildren: {},
      embeddedPalettes: {},
      visibleChildren: {},
      animationKey: '',
      animationLabel: '',
      animationKeys: [],
      animationLabels: [],
      legacyKeys: [],
      legacyAnimationKeys: [],
      usageFrames: [],
      usageFramesByAnimation: {},
      palette: sharedPalette
    });
    animation.artByKey[key] = transformed;
    rom.art.animations.artByKey[key] = transformed;
    separation.syntheticSourceKeys.push(key);
    return transformed;
  }

  function replaceLayerWithTransform(rom, separation, frameIndex,
      layerOrdinal, transform) {
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, frameIndex);
    var layer = privateLayer(frame, layerOrdinal);
    var source = animation.artByKey[layer.sourceKey];
    if (!source) fail('selected private layer lacks its sprite source');
    if (layer.width !== source.sprite.width ||
        layer.height !== source.sprite.height) {
      fail('selected sprite dimensions do not match its frame layer');
    }
    var transformed = appendTransformedSource(
      rom, separation, source, transform);
    var replacement = clonedLayer(layer, transformed, layerOrdinal);
    replacement.width = transformed.sprite.width;
    replacement.height = transformed.sprite.height;
    replacement.drawOffsetX = integerInRange(
      layer.drawOffsetX + Math.floor((layer.width - replacement.width) / 2),
      -0x8000, 0x7FFF, 'transformed layer X position');
    replacement.drawOffsetY = integerInRange(
      layer.drawOffsetY + Math.floor((layer.height - replacement.height) / 2),
      -0x8000, 0x7FFF, 'transformed layer Y position');
    frame.layers[layerOrdinal] = replacement;
    finishStructuralEdit(rom, separation);
    return layerOrdinal;
  }

  function rotateLayer(rom, separation, frameIndex, layerOrdinal, degrees) {
    var animation = requirePrivateSequence(rom, separation);
    var layer = privateLayer(privateFrame(animation, frameIndex), layerOrdinal);
    var source = animation.artByKey[layer.sourceKey];
    if (!source) fail('selected private layer lacks its sprite source');
    return replaceLayerWithTransform(rom, separation, frameIndex,
      layerOrdinal, function(pixels) {
        return rotateIndexedPixels(pixels.indices, pixels.intensity,
          source.sprite.width, source.sprite.height,
          degrees);
      });
  }

  function resizeLayer(rom, separation, frameIndex, layerOrdinal,
      width, height) {
    var animation = requirePrivateSequence(rom, separation);
    var layer = privateLayer(privateFrame(animation, frameIndex), layerOrdinal);
    var source = animation.artByKey[layer.sourceKey];
    if (!source) fail('selected private layer lacks its sprite source');
    var target = transformDimensions(width, height, 'resized sprite');
    return replaceLayerWithTransform(rom, separation, frameIndex,
      layerOrdinal, function(pixels) {
        return resizeIndexedPixels(pixels.indices, pixels.intensity,
          source.sprite.width, source.sprite.height,
          target.width, target.height);
      });
  }

  function paletteWordsForBlankLayer(animation, frame, templateLayer) {
    var candidates = [];
    if (templateLayer) candidates.push(templateLayer);
    (frame && frame.layers || []).forEach(function(layer) {
      if (candidates.indexOf(layer) < 0) candidates.push(layer);
    });
    for (var index = 0; index < candidates.length; index++) {
      var layer = candidates[index];
      var source = animation.artByKey[layer.sourceKey];
      if (!source || source.formatKind !== 'indexed-ci8') continue;
      var palette = M.childPalette(source,
        M.childOrdinalOrFallback(source, layer.selectedChildOrdinal));
      if (palette && palette.length === 256) return new Uint16Array(palette);
    }
    fail('this frame has no editable 256-color palette for a blank layer');
  }

  function blankFrameSourceData(animation, frame, templateLayer) {
    var width = integerInRange(animation.canvas && animation.canvas.width,
      1, 0xFFFF, 'private animation canvas width');
    var height = integerInRange(animation.canvas && animation.canvas.height,
      1, 0xFFFF, 'private animation canvas height');
    var pixels = width * height;
    return {
      targetWidth: width,
      targetHeight: height,
      paletteWords: paletteWordsForBlankLayer(
        animation, frame, templateLayer),
      indices: new Uint8Array(pixels),
      intensity: new Uint8Array(pixels)
    };
  }

  function blankLayerRecord(animation, source, templateLayer, ordinal) {
    return {
      ordinal: ordinal,
      artId: source.artId,
      drawOffsetX: animation.canvas.originX,
      drawOffsetY: animation.canvas.originY,
      width: animation.canvas.width,
      height: animation.canvas.height,
      flags: Number(templateLayer && templateLayer.flags) || 0,
      scaleXRaw: Number(templateLayer && templateLayer.scaleXRaw) || 0,
      scaleYRaw: Number(templateLayer && templateLayer.scaleYRaw) || 0,
      metadataOffset: null,
      sourceKey: source.key,
      bindingId: source.bindingId,
      physicalSourceId: source.physicalSourceId,
      sourceRole: source.sourceRole,
      resourceKey: source.resourceKey,
      lookupBank: 0,
      childCount: 1,
      requestedChildOrdinal: 0,
      selectedChildOrdinal: 0
    };
  }

  function unusedFrameIdentity(animation) {
    var used = {};
    (animation.donorAnimation && animation.donorAnimation.frames || [])
      .concat(animation.frames).forEach(function(frame) {
      used[stableFrameIndex(frame)] = true;
    });
    for (var value = 0; value <= 0xFFFF; value++) {
      if (!used[value]) return value;
    }
    fail('this private sequence has no free frame identity');
  }

  function unusedFrameToken(animation) {
    var used = {};
    animation.poseProgram.records.forEach(function(record) {
      if (record.opcode === 0x01 && record.operands) {
        used[record.operands[0]] = true;
      }
    });
    for (var value = 0; value <= 0xFF; value++) {
      if (!used[value]) return value;
    }
    fail('this private sequence has no free frame token');
  }

  function clonedLayer(donorLayer, source, ordinal) {
    return Object.assign({}, donorLayer, {
      ordinal: ordinal,
      sourceKey: source.key,
      bindingId: source.bindingId,
      physicalSourceId: source.physicalSourceId,
      sourceRole: source.sourceRole,
      resourceKey: source.resourceKey,
      lookupBank: 0,
      childCount: source.sprite.childCount,
      requestedChildOrdinal: source.sourceRole === 'body'
        ? 0 : donorLayer.requestedChildOrdinal,
      selectedChildOrdinal: source.sourceRole === 'body'
        ? 0 : M.childOrdinalOrFallback(source, donorLayer.selectedChildOrdinal),
      metadataOffset: null
    });
  }

  function pruneUnusedSources(rom, separation) {
    var animation = separation.syntheticAnimation, used = {};
    animation.frames.forEach(function(frame) {
      frame.layers.forEach(function(layer) { used[layer.sourceKey] = true; });
    });
    Object.keys(animation.artByKey).forEach(function(key) {
      if (used[key]) return;
      delete animation.artByKey[key];
      delete rom.art.animations.artByKey[key];
      delete rom.art.animations.edits[key];
      Object.keys(rom.art.animations.history).forEach(function(historyKey) {
        if (historyKey.indexOf(key + '#child-') === 0) {
          delete rom.art.animations.history[historyKey];
        }
      });
    });
    separation.syntheticSourceKeys = Object.keys(animation.artByKey);
  }

  function finishStructuralEdit(rom, separation) {
    pruneUnusedSources(rom, separation);
    rebuildSyntheticIndexes(separation.syntheticAnimation);
    separation.syntheticSourceKeys = Object.keys(
      separation.syntheticAnimation.artByKey);
    separation.structure = captureStructure(rom.art.animations, separation);
    rom.animationSequences.dirty = true;
    touchState(rom.animationSequences);
    rom.art.animations.editRevision =
      (Number(rom.art.animations.editRevision) || 0) + 1;
  }

  function addLayerFrom(rom, separation, targetFrameIndex, donorAnimation,
      donorFrameIndex, donorLayerOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var targetFrame = privateFrame(animation, targetFrameIndex);
    var donorFrame = privateFrame(donorAnimation, donorFrameIndex);
    var donorLayer = privateLayer(donorFrame, donorLayerOrdinal);
    if (targetFrame.layers.length >= 0xFFFF) {
      fail('this frame already contains the maximum 65535 layers');
    }
    var source = appendClonedSource(rom, separation, donorAnimation, donorLayer);
    targetFrame.layers.push(clonedLayer(donorLayer, source,
      targetFrame.layers.length));
    finishStructuralEdit(rom, separation);
    return targetFrame.layers.length - 1;
  }

  function addBlankLayer(rom, separation, targetFrameIndex,
      templateLayerOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, targetFrameIndex);
    if (frame.layers.length >= 0xFFFF) {
      fail('this frame already contains the maximum 65535 layers');
    }
    templateLayerOrdinal = integerInRange(templateLayerOrdinal, 0,
      frame.layers.length - 1, 'template layer index');
    var templateLayer = frame.layers[templateLayerOrdinal];
    var prepared = blankFrameSourceData(animation, frame, templateLayer);
    var source = appendImportedFrameSource(
      rom, separation, prepared, templateLayer);
    frame.layers.push(blankLayerRecord(
      animation, source, templateLayer, frame.layers.length));
    finishStructuralEdit(rom, separation);
    return frame.layers.length - 1;
  }

  function addBlankFrame(rom, separation, afterFrameIndex,
      templateLayerOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var templateFrame = privateFrame(animation, afterFrameIndex);
    templateLayerOrdinal = integerInRange(templateLayerOrdinal, 0,
      templateFrame.layers.length - 1, 'template layer index');
    var templateLayer = templateFrame.layers[templateLayerOrdinal];
    var insertedIndex = afterFrameIndex + 1;
    var frame = {
      sequenceIndex: insertedIndex,
      sourceFrameIndex: unusedFrameIdentity(animation),
      token: unusedFrameToken(animation),
      ticks: integerInRange(templateFrame.ticks, 0, 255, 'frame ticks'),
      metadataTarget: null,
      layers: []
    };
    var frames = animation.frames.slice();
    frames.splice(insertedIndex, 0, frame);
    var poseProgram = poseProgramWithInsertedFrame(
      animation.poseProgram, frames, separation.laneKey === 'idle',
      afterFrameIndex);
    var prepared = blankFrameSourceData(
      animation, templateFrame, templateLayer);
    var source = appendImportedFrameSource(
      rom, separation, prepared, templateLayer);
    frame.layers.push(blankLayerRecord(animation, source, templateLayer, 0));
    animation.frames = frames;
    animation.frames.forEach(function(row, index) {
      row.sequenceIndex = index;
    });
    animation.poseProgram = poseProgram;
    animation.spec.frames = animation.frames.map(function(row) {
      return [row.token, row.ticks];
    });
    finishStructuralEdit(rom, separation);
    return insertedIndex;
  }

  function copyLayerFrom(rom, separation, targetFrameIndex, targetLayerOrdinal,
      donorAnimation, donorFrameIndex, donorLayerOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var targetFrame = privateFrame(animation, targetFrameIndex);
    var targetLayer = privateLayer(targetFrame, targetLayerOrdinal);
    var donorFrame = privateFrame(donorAnimation, donorFrameIndex);
    var donorLayer = privateLayer(donorFrame, donorLayerOrdinal);
    var source = appendClonedSource(rom, separation, donorAnimation, donorLayer);
    source.childSelectionPolicy = null;
    var replacement = clonedLayer(donorLayer, source, targetLayerOrdinal);
    replacement.drawOffsetX = targetLayer.drawOffsetX;
    replacement.drawOffsetY = targetLayer.drawOffsetY;
    replacement.flags = targetLayer.flags;
    replacement.scaleXRaw = targetLayer.scaleXRaw;
    replacement.scaleYRaw = targetLayer.scaleYRaw;
    targetFrame.layers[targetLayerOrdinal] = replacement;
    finishStructuralEdit(rom, separation);
    return targetLayerOrdinal;
  }

  function copyFrameFrom(rom, separation, targetFrameIndex, donorAnimation,
      donorFrameIndex) {
    var animation = requirePrivateSequence(rom, separation);
    var targetFrame = privateFrame(animation, targetFrameIndex);
    var donorFrame = privateFrame(donorAnimation, donorFrameIndex);
    if (!donorFrame.layers.length) fail('selected source frame has no layers');
    var sourceMap = {};
    targetFrame.layers = donorFrame.layers.map(function(donorLayer, ordinal) {
      var source = sourceMap[donorLayer.sourceKey];
      if (!source) {
        source = appendClonedSource(rom, separation, donorAnimation, donorLayer);
        sourceMap[donorLayer.sourceKey] = source;
      }
      return clonedLayer(donorLayer, source, ordinal);
    });
    finishStructuralEdit(rom, separation);
    return 0;
  }

  function importFrame(rom, separation, targetFrameIndex, prepared, options) {
    options = options || {};
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, targetFrameIndex);
    var expectedWidth = animation.canvas.width;
    var expectedHeight = animation.canvas.height;
    if (!prepared || prepared.targetWidth !== expectedWidth ||
        prepared.targetHeight !== expectedHeight) {
      fail('imported frame must match the current sequence canvas ' +
        expectedWidth + 'x' + expectedHeight);
    }
    var keepEquipment = options.keepEquipment !== false;
    var firstBodyOrdinal = frame.layers.findIndex(function(layer) {
      var source = animation.artByKey[layer.sourceKey];
      return !source || source.sourceRole !== 'equipment';
    });
    var templateLayer = frame.layers[
      firstBodyOrdinal >= 0 ? firstBodyOrdinal : 0];
    var source = appendImportedFrameSource(
      rom, separation, prepared, templateLayer);
    var importedLayer = {
      ordinal: 0,
      artId: source.artId,
      drawOffsetX: animation.canvas.originX,
      drawOffsetY: animation.canvas.originY,
      width: expectedWidth,
      height: expectedHeight,
      flags: Number(templateLayer && templateLayer.flags) || 0,
      scaleXRaw: Number(templateLayer && templateLayer.scaleXRaw) || 0,
      scaleYRaw: Number(templateLayer && templateLayer.scaleYRaw) || 0,
      metadataOffset: null,
      sourceKey: source.key,
      bindingId: source.bindingId,
      physicalSourceId: source.physicalSourceId,
      sourceRole: source.sourceRole,
      resourceKey: source.resourceKey,
      lookupBank: 0,
      childCount: 1,
      requestedChildOrdinal: 0,
      selectedChildOrdinal: 0
    };
    if (!keepEquipment) {
      frame.layers = [importedLayer];
    } else {
      var inserted = false;
      var layers = [];
      frame.layers.forEach(function(layer, ordinal) {
        var oldSource = animation.artByKey[layer.sourceKey];
        if (oldSource && oldSource.sourceRole === 'equipment') {
          layers.push(layer);
          return;
        }
        if (!inserted && ordinal === firstBodyOrdinal) {
          layers.push(importedLayer);
          inserted = true;
        }
      });
      if (!inserted) layers.push(importedLayer);
      frame.layers = layers;
    }
    renumberLayers(frame);
    finishStructuralEdit(rom, separation);
    return frame.layers.indexOf(importedLayer);
  }

  function removeLayer(rom, separation, frameIndex, layerOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, frameIndex);
    layerOrdinal = integerInRange(layerOrdinal, 0, frame.layers.length - 1,
      'layer index');
    if (frame.layers.length <= 1) {
      fail('a frame must retain at least one sprite layer');
    }
    frame.layers.splice(layerOrdinal, 1);
    renumberLayers(frame);
    finishStructuralEdit(rom, separation);
    return Math.min(layerOrdinal, frame.layers.length - 1);
  }

  function removeFrame(rom, separation, frameIndex) {
    var animation = requirePrivateSequence(rom, separation);
    frameIndex = integerInRange(frameIndex, 0, animation.frames.length - 1,
      'frame index');
    if (animation.frames.length <= 1) {
      fail('an animation sequence must retain at least one frame');
    }
    var frames = animation.frames.slice();
    frames.splice(frameIndex, 1);
    var poseProgram = poseProgramWithoutFrame(
      animation.poseProgram, frames, separation.laneKey === 'idle', frameIndex);
    animation.frames = frames;
    animation.frames.forEach(function(frame, index) {
      frame.sequenceIndex = index;
    });
    animation.poseProgram = poseProgram;
    animation.spec.frames = animation.frames.map(function(frame) {
      return [frame.token, frame.ticks];
    });
    finishStructuralEdit(rom, separation);
    return Math.min(frameIndex, animation.frames.length - 1);
  }

  function moveFrame(rom, separation, fromIndex, toIndex) {
    var animation = requirePrivateSequence(rom, separation);
    fromIndex = integerInRange(fromIndex, 0, animation.frames.length - 1,
      'source frame index');
    toIndex = integerInRange(toIndex, 0, animation.frames.length - 1,
      'destination frame index');
    if (fromIndex === toIndex) return toIndex;
    var frames = animation.frames.slice();
    var frame = frames.splice(fromIndex, 1)[0];
    frames.splice(toIndex, 0, frame);
    var poseProgram = poseProgramWithFrames(
      animation.poseProgram, frames, separation.laneKey === 'idle');
    animation.frames = frames;
    animation.frames.forEach(function(row, index) {
      row.sequenceIndex = index;
    });
    animation.poseProgram = poseProgram;
    animation.spec.frames = animation.frames.map(function(row) {
      return [row.token, row.ticks];
    });
    finishStructuralEdit(rom, separation);
    return frame.sequenceIndex;
  }

  function setFrameTicks(rom, separation, frameIndex, ticks) {
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, frameIndex);
    ticks = integerInRange(ticks, 0, 255, 'frame ticks');
    if (frame.ticks === ticks) return false;
    var candidateFrames = animation.frames.map(function(row, index) {
      return index === frameIndex ? Object.assign({}, row, { ticks: ticks }) : row;
    });
    var poseProgram = poseProgramWithFrames(
      animation.poseProgram, candidateFrames, separation.laneKey === 'idle');
    frame.ticks = ticks;
    animation.poseProgram = poseProgram;
    animation.spec.frames = animation.frames.map(function(row) {
      return [row.token, row.ticks];
    });
    finishStructuralEdit(rom, separation);
    return true;
  }

  function moveLayer(rom, separation, frameIndex, fromOrdinal, toOrdinal) {
    var animation = requirePrivateSequence(rom, separation);
    var frame = privateFrame(animation, frameIndex);
    fromOrdinal = integerInRange(fromOrdinal, 0, frame.layers.length - 1,
      'source layer index');
    toOrdinal = integerInRange(toOrdinal, 0, frame.layers.length - 1,
      'destination layer index');
    if (fromOrdinal === toOrdinal) return toOrdinal;
    var layer = frame.layers.splice(fromOrdinal, 1)[0];
    frame.layers.splice(toOrdinal, 0, layer);
    renumberLayers(frame);
    finishStructuralEdit(rom, separation);
    return layer.ordinal;
  }

  function setLayerPosition(rom, separation, frameIndex, layerOrdinal, x, y) {
    var animation = requirePrivateSequence(rom, separation);
    var layer = privateLayer(privateFrame(animation, frameIndex), layerOrdinal);
    x = integerInRange(x, -0x8000, 0x7FFF, 'layer X position');
    y = integerInRange(y, -0x8000, 0x7FFF, 'layer Y position');
    if (layer.drawOffsetX === x && layer.drawOffsetY === y) return false;
    layer.drawOffsetX = x;
    layer.drawOffsetY = y;
    finishStructuralEdit(rom, separation);
    return true;
  }

  function separationFor(animationOrRoute, state) {
    if (!state) return null;
    if (animationOrRoute && animationOrRoute.separationId) {
      return state.separations[animationOrRoute.separationId] || null;
    }
    if (!animationOrRoute || !animationOrRoute.spec) return null;
    return state.separations[routeId(animationOrRoute.spec.classId,
      animationOrRoute.spec.actionId, bodyFlagsFor(animationOrRoute),
      laneFor(animationOrRoute))] || null;
  }

  function routeSeparationFor(animation, state) {
    if (!state || !animation || !animation.spec) return null;
    return state.separations[routeId(animation.spec.classId,
      animation.spec.actionId, bodyFlagsFor(animation), laneFor(animation))] || null;
  }

  function routeAnimation(rom, classId, actionId, bodyFlags, laneKey) {
    var state = rom && rom.animationSequences;
    var separation = state && state.separations[
      routeId(classId, actionId, bodyFlags, laneKey)];
    return separation && separation.syntheticAnimation || null;
  }

  function selectorAnimation(rom, classId, bodyFlags, selector) {
    var state = rom && rom.animationSequences;
    if (!state || !Number.isInteger(Number(selector))) return null;
    var separation = separationsForGroup(state, classId, bodyFlags)
      .find(function(row) {
        return row.laneKey !== 'idle' && row.selector === Number(selector);
      });
    return separation && separation.syntheticAnimation || null;
  }

  function sourcePaletteBank(source) {
    var words = source.palette;
    if (!words || words.length !== 256) fail(source.key + ' lacks a 256-color lookup bank');
    return words;
  }

  function bankBytes(words) {
    var output = new Uint8Array(0x200);
    for (var i = 0; i < 256; i++) A.writeU16(output, i * 2, words[i]);
    return output;
  }

  function buildLookup(target, sources) {
    var base = A.readCompressedResource(target.sourceBytes, target.animation.spec.lookupKey).decoded;
    if (base.length % 0x200) fail('target lookup resource has a partial bank');
    var banks = [];
    for (var offset = 0; offset < base.length; offset += 0x200) {
      var words = new Uint16Array(256);
      for (var index = 0; index < 256; index++) words[index] = A.readU16(base, offset + index * 2);
      banks.push(words);
    }
    sources.forEach(function(row) {
      var wanted = sourcePaletteBank(row.source);
      var bank = banks.findIndex(function(candidate) { return equalWords(candidate, wanted); });
      if (bank < 0) { bank = banks.length; banks.push(wanted.slice()); }
      if (bank > 255) fail('separated descriptor requires more than 256 lookup banks');
      row.lookupBank = bank;
    });
    return concat(banks.map(bankBytes));
  }

  function buildConfig(target, sources) {
    var base = A.readCompressedResource(target.sourceBytes, target.animation.spec.configKey).decoded;
    var oldCount = target.animation.members.length - 4;
    var storedCount = A.readU32(base, 0), oldMapOffset = A.readU32(base, 0x1C);
    if (storedCount !== oldCount || oldMapOffset + oldCount > base.length) {
      fail('target art configuration does not match its descriptor');
    }
    var newCount = oldCount + sources.length;
    var mapOffset = (0x20 + newCount + 3) & ~3;
    var length = (mapOffset + newCount + 3) & ~3;
    var output = new Uint8Array(length);
    output.set(base.slice(0, 0x20), 0);
    A.writeU32(output, 0, newCount);
    A.writeU32(output, 0x1C, mapOffset);
    output.set(base.slice(0x20, 0x20 + oldCount), 0x20);
    output.set(base.slice(oldMapOffset, oldMapOffset + oldCount), mapOffset);
    sources.forEach(function(row, index) {
      output[0x20 + oldCount + index] = row.source.selectorPolicy;
      output[mapOffset + oldCount + index] = row.lookupBank;
    });
    return output;
  }

  function metadataRecord(frame, artIdBySource) {
    var output = new Uint8Array(2 + frame.layers.length * 16);
    A.writeU16(output, 0, frame.layers.length);
    frame.layers.forEach(function(layer, ordinal) {
      var offset = 2 + ordinal * 16;
      var artId = artIdBySource[layer.sourceKey];
      if (!Number.isInteger(artId)) fail('separated frame layer lacks its cloned art ID');
      A.writeU16(output, offset, artId);
      A.writeU16(output, offset + 2, layer.drawOffsetX & 0xFFFF);
      A.writeU16(output, offset + 4, layer.drawOffsetY & 0xFFFF);
      A.writeU16(output, offset + 6, layer.width);
      A.writeU16(output, offset + 8, layer.height);
      A.writeU16(output, offset + 10, layer.flags);
      A.writeU16(output, offset + 12, layer.scaleXRaw);
      A.writeU16(output, offset + 14, layer.scaleYRaw);
    });
    return output;
  }

  function buildMetadata(target, rows) {
    var base = A.readCompressedResource(target.sourceBytes, target.animation.spec.metadataKey).decoded;
    var oldDirectoryBytes = A.readU32(base, 0);
    if (!oldDirectoryBytes || oldDirectoryBytes % 4 || oldDirectoryBytes > base.length) {
      fail('target metadata directory is invalid');
    }
    var oldCount = oldDirectoryBytes / 4, records = [];
    rows.forEach(function(row) {
      var byToken = {}, order = [];
      row.animation.frames.forEach(function(frame) {
        if (!Object.prototype.hasOwnProperty.call(byToken, frame.token)) {
          byToken[frame.token] = frame;
          order.push(frame.token);
        }
      });
      row.tokenMap = {};
      order.forEach(function(token) {
        var nextToken = oldCount + records.length;
        if (nextToken > 255) fail('separated frame token exceeds the game u8 range');
        row.tokenMap[token] = nextToken;
        records.push(metadataRecord(byToken[token], row.artIdBySource));
      });
    });
    var shift = records.length * 4;
    var payloadBytes = records.reduce(function(total, record) { return total + record.length; }, 0);
    var output = new Uint8Array(base.length + shift + payloadBytes);
    for (var token = 0; token < oldCount; token++) {
      A.writeU32(output, token * 4, A.readU32(base, token * 4) + shift);
    }
    output.set(base.slice(oldDirectoryBytes), oldDirectoryBytes + shift);
    var cursor = base.length + shift;
    records.forEach(function(record, index) {
      A.writeU32(output, oldDirectoryBytes + index * 4, cursor);
      output.set(record, cursor); cursor += record.length;
    });
    return output;
  }

  function remappedProgram(row) {
    var poseProgram = row.animation.poseProgram;
    var program = poseProgram.program.slice();
    poseProgram.records.forEach(function(record) {
      if (record.opcode !== 1) return;
      var relative = record.offset - poseProgram.start;
      var replacement = row.tokenMap[record.operands[0]];
      if (Number.isInteger(replacement)) program[relative + 1] = replacement;
    });
    return program;
  }

  function buildPose(target, rows) {
    var base = A.readCompressedResource(target.sourceBytes, target.animation.spec.poseKey).decoded;
    var oldDirectoryBytes = A.readU32(base, 0);
    if (!oldDirectoryBytes || oldDirectoryBytes % 4 || oldDirectoryBytes > base.length) {
      fail('target pose directory is invalid');
    }
    var oldCount = oldDirectoryBytes / 4;
    var idleRows = rows.filter(function(row) {
      return row.separation.laneKey === 'idle';
    });
    if (idleRows.length > 1) fail('one art route cannot contain two private idle loops');
    var appendedRows = rows.filter(function(row) {
      return row.separation.laneKey !== 'idle';
    });
    var idleProgram = idleRows.length ? remappedProgram(idleRows[0]) : null;
    var oldProgram0End = oldCount > 1 ? A.readU32(base, 4) : base.length;
    if (oldProgram0End < oldDirectoryBytes || oldProgram0End > base.length) {
      fail('target pose selector 0 has invalid bounds');
    }
    var oldProgram0Length = oldProgram0End - oldDirectoryBytes;
    var replacementLength = idleProgram ? idleProgram.length : oldProgram0Length;
    var appendedPrograms = appendedRows.map(remappedProgram);
    var directoryShift = appendedRows.length * 4;
    var selector0Delta = replacementLength - oldProgram0Length;
    var appendedBytes = appendedPrograms.reduce(function(total, program) {
      return total + program.length;
    }, 0);
    var output = new Uint8Array(base.length + directoryShift +
      selector0Delta + appendedBytes);
    A.writeU32(output, 0, oldDirectoryBytes + directoryShift);
    for (var selector = 1; selector < oldCount; selector++) {
      A.writeU32(output, selector * 4,
        A.readU32(base, selector * 4) + directoryShift + selector0Delta);
    }
    var cursor = oldDirectoryBytes + directoryShift;
    if (idleProgram) output.set(idleProgram, cursor);
    else output.set(base.slice(oldDirectoryBytes, oldProgram0End), cursor);
    cursor += replacementLength;
    output.set(base.slice(oldProgram0End), cursor);
    cursor += base.length - oldProgram0End;
    appendedPrograms.forEach(function(program, index) {
      var wanted = oldCount + index;
      if (appendedRows[index].separation.selector !== wanted) {
        fail('separated selector ordering changed during export planning');
      }
      A.writeU32(output, oldDirectoryBytes + index * 4, cursor);
      output.set(program, cursor); cursor += program.length;
    });
    if (cursor !== output.length) fail('separated pose resource size changed during assembly');
    return output;
  }

  function handleIndex(rom, animation) {
    var fields = animation.spec.route && animation.spec.route.actorFields;
    if (!fields) fail('target animation lacks its accepted actor route fields');
    var classDef = rom && rom.classDefs &&
      rom.classDefs[fields.physicalClassRecord];
    if (!classDef || !Number.isInteger(classDef.classCopyMatch)) {
      fail('target animation lacks its class-copy selector byte');
    }
    var base = classDef.classCopyMatch === fields.rawOwnerContext
      ? fields.sourceArtId : fields.rawOwnerContext;
    var index = 4 * base + 2 * fields.flagA + fields.flagB;
    if (index < 0 || index >= CLASS_HANDLE_COUNT) fail('target class-handle index is out of range');
    return index;
  }

  function buildGroupDecoded(rom, cleanBase, separations, rootSlot, ordinal) {
    var targetAnimation = resolveRef(rom.art.animations, separations[0].targetRef);
    separations.forEach(function(row) {
      var target = resolveRef(rom.art.animations, row.targetRef);
      if (target.spec.descriptorKey !== targetAnimation.spec.descriptorKey ||
          target.spec.poseKey !== targetAnimation.spec.poseKey) {
        fail('one class/body route resolved to incompatible target descriptors');
      }
    });
    var sourceRows = [], sequenceRows = [];
    separations.forEach(function(separation) {
      var animation = separation.syntheticAnimation || buildSynthetic(rom, separation);
      var row = { separation: separation, animation: animation, artIdBySource: {} };
      Object.keys(animation.artByKey).sort().forEach(function(sourceKey) {
        var source = animation.artByKey[sourceKey];
        var edits = rom.art.animations.edits[sourceKey];
        var decoded = source.editable && edits
          ? M.buildDecoded(source, edits.children) : source.sprite.decoded.slice();
        var sourceRow = {
          name: 'animation-sequence-' + ordinal + '-art-' + sourceRows.length,
          source: source, decoded: decoded,
          separation: separation
        };
        row.artIdBySource[sourceKey] = targetAnimation.members.length - 4 + sourceRows.length;
        sourceRows.push(sourceRow);
      });
      sequenceRows.push(row);
    });
    if (targetAnimation.members.length - 4 + sourceRows.length > 0x10000) {
      fail('separated descriptor exceeds the game u16 art-ID range');
    }
    var target = { animation: targetAnimation, sourceBytes: cleanBase };
    var lookupDecoded = buildLookup(target, sourceRows);
    var configDecoded = buildConfig(target, sourceRows);
    var metadataDecoded = buildMetadata(target, sequenceRows);
    var poseDecoded = buildPose(target, sequenceRows);
    var prefix = 'animation-sequence-' + ordinal;
    var controls = [
      { name: prefix + '-metadata', decoded: metadataDecoded },
      { name: prefix + '-pose', decoded: poseDecoded },
      { name: prefix + '-config', decoded: configDecoded },
      { name: prefix + '-lookup', decoded: lookupDecoded }
    ];
    var descriptorName = prefix + '-descriptor';
    var descriptorLength = (targetAnimation.members.length + sourceRows.length) * 4;
    return {
      id: groupId(separations[0].classId, separations[0].bodyFlags),
      ordinal: ordinal,
      rootSlot: rootSlot,
      targetAnimation: targetAnimation,
      separations: separations,
      sequenceRows: sequenceRows,
      sourceRows: sourceRows,
      controls: controls,
      descriptorName: descriptorName,
      descriptorStored: new Uint8Array(descriptorLength),
      handleIndex: handleIndex(rom, targetAnimation),
      originalHandle: targetAnimation.spec.route.rawHandleU16
    };
  }

  function buildGroup(rom, cleanBase, separations, rootSlot, ordinal) {
    var group = buildGroupDecoded(
      rom, cleanBase, separations, rootSlot, ordinal);
    group.sourceRows.forEach(function(row) {
      row.stored = A.bootLzCompress(row.decoded);
    });
    group.controls.forEach(function(row) {
      row.stored = A.bootLzCompress(row.decoded);
    });
    return group;
  }

  async function compressGroupAsync(group, progressState, onProgress) {
    var rows = group.sourceRows.concat(group.controls);
    for (var index = 0; index < rows.length; index++) {
      var row = rows[index];
      var label = 'separated sequence resource ' +
        (progressState.completed + 1) + ' of ' + progressState.total;
      row.stored = await A.bootLzCompressAsync(row.decoded, function(fraction) {
        if (onProgress) {
          onProgress(label, (progressState.completed + fraction) /
            Math.max(1, progressState.total));
        }
      });
      progressState.completed++;
    }
    return group;
  }

  function buildPlan(rom, cleanBase) {
    var state = rom.animationSequences;
    if (!state || !Object.keys(state.separations).length) return null;
    var root = A.readResource(cleanBase, DESCRIPTOR_ROOT_KEY);
    var handles = A.readResource(cleanBase, CLASS_HANDLE_RESOURCE_KEY);
    if (root.storedLength !== DESCRIPTOR_ROOT_COUNT * 4) {
      fail('combat descriptor root has an unexpected size');
    }
    var freeSlots = [];
    for (var slot = 0; slot < DESCRIPTOR_ROOT_COUNT; slot++) {
      if (A.readU32(root.stored, slot * 4) === 0) freeSlots.push(slot);
    }
    var groupKeys = {}, groups = [];
    Object.keys(state.separations).forEach(function(key) {
      var row = state.separations[key], id = groupId(row.classId, row.bodyFlags);
      groupKeys[id] = true;
    });
    Object.keys(groupKeys).sort(function(left, right) {
      var a = left.split(':').map(Number), b = right.split(':').map(Number);
      return a[0] - b[0] || a[1] - b[1];
    }).forEach(function(id, ordinal) {
      if (!freeSlots.length) fail('combat descriptor root has no free slot for another separated route');
      var parts = id.split(':').map(Number);
      groups.push(buildGroup(rom, cleanBase,
        separationsForGroup(state, parts[0], parts[1]), freeSlots.shift(), ordinal));
    });
    return finishBuildPlan(groups, root, handles);
  }

  function finishBuildPlan(groups, root, handles) {
    var relocatedResources = [];
    groups.forEach(function(group) {
      group.sourceRows.forEach(function(row) {
        relocatedResources.push({ name: row.name, stored: row.stored });
      });
      group.controls.forEach(function(row) {
        relocatedResources.push({ name: row.name, stored: row.stored });
      });
      relocatedResources.push({ name: group.descriptorName, stored: group.descriptorStored });
    });
    return {
      groups: groups, relocatedResources: relocatedResources,
      root: root, handles: handles
    };
  }

  function yieldBuildTask() {
    return new Promise(function(resolve) { setTimeout(resolve, 0); });
  }

  async function buildPlanAsync(rom, cleanBase, onProgress) {
    var state = rom.animationSequences;
    if (!state || !Object.keys(state.separations).length) return null;
    var root = A.readResource(cleanBase, DESCRIPTOR_ROOT_KEY);
    var handles = A.readResource(cleanBase, CLASS_HANDLE_RESOURCE_KEY);
    if (root.storedLength !== DESCRIPTOR_ROOT_COUNT * 4) {
      fail('combat descriptor root has an unexpected size');
    }
    var freeSlots = [];
    for (var slot = 0; slot < DESCRIPTOR_ROOT_COUNT; slot++) {
      if (A.readU32(root.stored, slot * 4) === 0) freeSlots.push(slot);
    }
    var groupKeys = {};
    Object.keys(state.separations).forEach(function(key) {
      var row = state.separations[key];
      groupKeys[groupId(row.classId, row.bodyFlags)] = true;
    });
    var orderedIds = Object.keys(groupKeys).sort(function(left, right) {
      var a = left.split(':').map(Number);
      var b = right.split(':').map(Number);
      return a[0] - b[0] || a[1] - b[1];
    });
    var groups = [];
    for (var ordinal = 0; ordinal < orderedIds.length; ordinal++) {
      if (!freeSlots.length) {
        fail('combat descriptor root has no free slot for another separated route');
      }
      var parts = orderedIds[ordinal].split(':').map(Number);
      if (onProgress) {
        onProgress('assembling separated route ' + (ordinal + 1) + ' of ' +
          orderedIds.length, 0);
      }
      groups.push(buildGroupDecoded(rom, cleanBase,
        separationsForGroup(state, parts[0], parts[1]),
        freeSlots.shift(), ordinal));
      await yieldBuildTask();
    }
    var progressState = {
      completed: 0,
      total: groups.reduce(function(total, group) {
        return total + group.sourceRows.length + group.controls.length;
      }, 0)
    };
    for (var groupIndex = 0; groupIndex < groups.length; groupIndex++) {
      await compressGroupAsync(groups[groupIndex], progressState, onProgress);
    }
    if (onProgress) onProgress('separated sequences complete', 1);
    return finishBuildPlan(groups, root, handles);
  }

  function finalizeAllocations(plan, allocationByName) {
    if (!plan) return;
    plan.groups.forEach(function(group) {
      group.sourceRows.forEach(function(row) { row.allocation = allocationByName[row.name]; });
      group.controls.forEach(function(row) { row.allocation = allocationByName[row.name]; });
      group.descriptorAllocation = allocationByName[group.descriptorName];
      if (!group.descriptorAllocation) fail('separated descriptor allocation is missing');
      var members = group.targetAnimation.members.slice();
      for (var control = 0; control < 4; control++) {
        members[control] = group.controls[control].allocation.key;
      }
      group.sourceRows.forEach(function(row) { members.push(row.allocation.key); });
      var descriptor = new Uint8Array(members.length * 4);
      members.forEach(function(key, index) { A.writeU32(descriptor, index * 4, key); });
      if (descriptor.length !== group.descriptorAllocation.stored.length) {
        fail('separated descriptor allocation changed size after pointer resolution');
      }
      group.descriptorStored = descriptor;
      group.descriptorAllocation.stored = descriptor;
      group.newHandle = ((group.originalHandle & 0xF000) |
        (group.rootSlot + 1)) & 0xFFFF;
    });
  }

  function applyPlan(plan, bytes, ranges, log) {
    if (!plan) return;
    var root = A.readResource(bytes, DESCRIPTOR_ROOT_KEY);
    var handles = A.readResource(bytes, CLASS_HANDLE_RESOURCE_KEY);
    plan.groups.forEach(function(group) {
      var rootOffset = root.entry + 4 + group.rootSlot * 4;
      var observedRoot = A.readU32(bytes, rootOffset);
      if (observedRoot !== 0) fail('combat descriptor root slot ' + group.rootSlot +
        ' is no longer empty');
      A.writeU32(bytes, rootOffset, group.descriptorAllocation.key);
      ranges.push([rootOffset, rootOffset + 4]);
      var handleOffset = handles.entry + 4 + CLASS_HANDLE_TABLE_OFFSET +
        group.handleIndex * 2;
      var observedHandle = A.readU16(bytes, handleOffset);
      if (observedHandle !== group.originalHandle) fail('class/body handle preimage is ' +
        A.hex(observedHandle, 4) + '; expected ' + A.hex(group.originalHandle, 4));
      A.writeU16(bytes, handleOffset, group.newHandle);
      ranges.push([handleOffset, handleOffset + 2]);
      log.push(group.targetAnimation.spec.className + ' body flags ' +
        flagLabel(group.separations[0].bodyFlags) + ': separated ' +
        group.separations.length + ' animation sequence' +
        (group.separations.length === 1 ? '' : 's') + ' into descriptor slot ' +
        group.rootSlot + ' and class handle ' + A.hex(group.newHandle, 4));
    });
  }

  function verifyPlan(plan, bytes) {
    if (!plan) return;
    var root = A.readResource(bytes, DESCRIPTOR_ROOT_KEY);
    var handles = A.readResource(bytes, CLASS_HANDLE_RESOURCE_KEY);
    plan.groups.forEach(function(group) {
      var rootOffset = root.entry + 4 + group.rootSlot * 4;
      if (A.readU32(bytes, rootOffset) !== group.descriptorAllocation.key) {
        fail('separated descriptor root readback differs');
      }
      var descriptor = A.readResource(bytes, group.descriptorAllocation.key);
      if (!equalBytes(descriptor.stored, group.descriptorStored)) {
        fail('separated descriptor readback differs');
      }
      group.controls.forEach(function(row) {
        var decoded = A.readCompressedResource(bytes, row.allocation.key).decoded;
        if (!equalBytes(decoded, row.decoded)) fail(row.name + ' compressed readback differs');
      });
      group.sourceRows.forEach(function(row) {
        var decoded = A.readCompressedResource(bytes, row.allocation.key).decoded;
        if (!equalBytes(decoded, row.decoded)) fail(row.name + ' compressed readback differs');
      });
      var handleOffset = handles.entry + 4 + CLASS_HANDLE_TABLE_OFFSET +
        group.handleIndex * 2;
      if (A.readU16(bytes, handleOffset) !== group.newHandle) {
        fail('separated class/body handle readback differs');
      }
    });
  }

  function currentRanges(plan) {
    if (!plan) return [];
    return plan.groups.reduce(function(output, group) {
      var rootOffset = plan.root.entry + 4 + group.rootSlot * 4;
      var handleOffset = plan.handles.entry + 4 + CLASS_HANDLE_TABLE_OFFSET +
        group.handleIndex * 2;
      output.push([rootOffset, rootOffset + 4],
        [handleOffset, handleOffset + 2]);
      return output;
    }, []);
  }

  function toBase64(bytes) {
    var chunks = [];
    for (var start = 0; start < bytes.length; start += 0x8000) {
      var text = '', slice = bytes.subarray(start, Math.min(bytes.length, start + 0x8000));
      for (var i = 0; i < slice.length; i++) text += String.fromCharCode(slice[i]);
      chunks.push(text);
    }
    return btoa(chunks.join(''));
  }

  function fromBase64(text, length, label) {
    var raw;
    try { raw = atob(text); } catch (error) { fail(label + ' is not valid base64'); }
    if (raw.length !== length) fail(label + ' has ' + raw.length + ' bytes; expected ' + length);
    var output = new Uint8Array(length);
    for (var i = 0; i < length; i++) output[i] = raw.charCodeAt(i);
    return output;
  }

  function fromBase64Variable(text, label) {
    if (typeof text !== 'string' || !text.length) fail(label + ' is not base64 text');
    var raw;
    try { raw = atob(text); } catch (error) { fail(label + ' is not valid base64'); }
    var output = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) output[i] = raw.charCodeAt(i);
    return output;
  }

  function sourceProjectRecord(row) {
    var children = {};
    Object.keys(row.children || {}).sort(function(left, right) {
      return Number(left) - Number(right);
    }).forEach(function(childOrdinal) {
      children[childOrdinal] = {
        ci8IndicesBase64: toBase64(row.children[childOrdinal].indices),
        i4IntensityBase64: toBase64(row.children[childOrdinal].intensity)
      };
    });
    return {
      resourceKey: row.resourceKey,
      artId: row.artId,
      sourceRole: row.sourceRole,
      selectorPolicy: row.selectorPolicy,
      childSelectionPolicy: row.childSelectionPolicy,
      palettePolicy: row.palettePolicy,
      elementSelection: row.elementSelection,
      formatKind: row.formatKind,
      editable: row.editable,
      lockedReason: row.lockedReason,
      childOrdinal: row.childOrdinal,
      weaponSelectable: row.weaponSelectable,
      width: row.width,
      height: row.height,
      childCount: row.childCount,
      decodedBase64: toBase64(row.decoded),
      paletteRgba5551BeBase64: toBase64(bankBytes(row.palette)),
      children: children
    };
  }

  function frameProjectRecord(row) {
    return {
      sequenceIndex: row.sequenceIndex,
      sourceFrameIndex: row.sourceFrameIndex,
      token: row.token,
      ticks: row.ticks,
      layers: row.layers.map(function(layer) { return Object.assign({}, layer); })
    };
  }

  function exactKeys(row, allowed) {
    return row && typeof row === 'object' && !Array.isArray(row) &&
      !Object.keys(row).some(function(key) { return allowed.indexOf(key) < 0; });
  }

  function paletteWordsFromProject(text, label) {
    var bytes = fromBase64(text, 0x200, label);
    var words = new Uint16Array(256);
    for (var index = 0; index < 256; index++) {
      words[index] = A.readU16(bytes, index * 2);
    }
    return words;
  }

  function prepareSourceProject(row, ordinal, label) {
    var allowed = ['resourceKey', 'artId', 'sourceRole', 'selectorPolicy',
      'childSelectionPolicy', 'palettePolicy', 'elementSelection', 'formatKind',
      'editable', 'lockedReason', 'childOrdinal', 'weaponSelectable', 'width',
      'height', 'childCount', 'decodedBase64', 'paletteRgba5551BeBase64',
      'children'];
    if (!exactKeys(row, allowed) || !Number.isInteger(row.resourceKey) ||
        row.resourceKey < 0 || row.resourceKey > 0xFFFFFFFF ||
        !Number.isInteger(row.artId) || row.artId < 0 || row.artId > 0xFFFF ||
        typeof row.sourceRole !== 'string' || !row.sourceRole ||
        !Number.isInteger(row.selectorPolicy) || row.selectorPolicy < 0 ||
        row.selectorPolicy > 2 || typeof row.formatKind !== 'string' ||
        typeof row.editable !== 'boolean' || typeof row.lockedReason !== 'string' ||
        !Number.isInteger(row.childOrdinal) ||
        typeof row.weaponSelectable !== 'boolean' ||
        !Number.isInteger(row.width) || !Number.isInteger(row.height) ||
        !Number.isInteger(row.childCount) ||
        !row.children || typeof row.children !== 'object' ||
        Array.isArray(row.children)) {
      fail(label + ' source ' + ordinal + ' metadata is invalid');
    }
    var decoded = fromBase64Variable(row.decodedBase64,
      label + ' source ' + ordinal + ' decoded object');
    var sprite;
    try { sprite = M.parseSpriteObject(decoded, row.resourceKey); }
    catch (error) { fail(label + ' source ' + ordinal + ': ' + error.message); }
    var formatKind = sprite.firstFormat === 1 && sprite.secondFormat === 0
      ? 'indexed-ci8' : (sprite.firstFormat === 1 && sprite.secondFormat === 1
        ? 'indexed-ci8-alpha8' : (sprite.firstFormat === 2
          ? 'direct-rgba5551' : 'unsupported'));
    if (formatKind === 'unsupported' ||
        row.width !== sprite.width || row.height !== sprite.height ||
        row.childCount !== sprite.childCount || row.formatKind !== formatKind ||
        row.editable !== (formatKind === 'indexed-ci8') ||
        row.childOrdinal < 0 || row.childOrdinal >= sprite.childCount) {
      fail(label + ' source ' + ordinal + ' sprite structure does not match');
    }
    var palette = paletteWordsFromProject(row.paletteRgba5551BeBase64,
      label + ' source ' + ordinal + ' palette');
    var expectedChildren = row.editable
      ? sprite.children.map(function(child) { return child.ordinal; }) : [];
    var childKeys = Object.keys(row.children).map(Number).sort(function(a, b) {
      return a - b;
    });
    if (childKeys.join(',') !== expectedChildren.join(',')) {
      fail(label + ' source ' + ordinal + ' does not contain every editable child');
    }
    var children = {}, pixels = sprite.width * sprite.height;
    expectedChildren.forEach(function(childOrdinal) {
      var child = row.children[childOrdinal];
      if (!exactKeys(child, ['ci8IndicesBase64', 'i4IntensityBase64'])) {
        fail(label + ' source ' + ordinal + ' child ' + childOrdinal + ' is invalid');
      }
      var indices = fromBase64(child.ci8IndicesBase64, pixels,
        label + ' source ' + ordinal + ' child ' + childOrdinal + ' CI8');
      var intensity = fromBase64(child.i4IntensityBase64, pixels,
        label + ' source ' + ordinal + ' child ' + childOrdinal + ' I4');
      for (var pixel = 0; pixel < intensity.length; pixel++) {
        if (intensity[pixel] > 15) {
          fail(label + ' source ' + ordinal + ' child ' + childOrdinal +
            ' contains an intensity above 15');
        }
      }
      children[childOrdinal] = { indices: indices, intensity: intensity };
    });
    return {
      resourceKey: row.resourceKey, artId: row.artId,
      sourceRole: row.sourceRole, selectorPolicy: row.selectorPolicy,
      childSelectionPolicy: jsonClone(row.childSelectionPolicy),
      palettePolicy: jsonClone(row.palettePolicy),
      elementSelection: jsonClone(row.elementSelection),
      formatKind: row.formatKind, editable: row.editable,
      lockedReason: row.lockedReason, childOrdinal: row.childOrdinal,
      weaponSelectable: row.weaponSelectable,
      width: sprite.width, height: sprite.height, childCount: sprite.childCount,
      decoded: decoded, palette: palette, children: children
    };
  }

  function prepareLayerProject(row, sources, label) {
    var allowed = ['sourceOrdinal', 'artId', 'drawOffsetX', 'drawOffsetY',
      'width', 'height', 'flags', 'scaleXRaw', 'scaleYRaw',
      'requestedChildOrdinal', 'selectedChildOrdinal'];
    if (!exactKeys(row, allowed)) fail(label + ' layer record is invalid');
    var sourceOrdinal = integerInRange(row.sourceOrdinal, 0, 0xFFFF,
      label + ' source ordinal');
    var source = sources[sourceOrdinal];
    if (!source) fail(label + ' references missing source ' + sourceOrdinal);
    var layer = {
      sourceOrdinal: sourceOrdinal,
      artId: integerInRange(row.artId, 0, 0xFFFF, label + ' art ID'),
      drawOffsetX: integerInRange(row.drawOffsetX, -0x8000, 0x7FFF,
        label + ' X position'),
      drawOffsetY: integerInRange(row.drawOffsetY, -0x8000, 0x7FFF,
        label + ' Y position'),
      width: integerInRange(row.width, 1, 0xFFFF, label + ' width'),
      height: integerInRange(row.height, 1, 0xFFFF, label + ' height'),
      flags: integerInRange(row.flags, 0, 0xFFFF, label + ' flags'),
      scaleXRaw: integerInRange(row.scaleXRaw, 0, 0xFFFF, label + ' X scale'),
      scaleYRaw: integerInRange(row.scaleYRaw, 0, 0xFFFF, label + ' Y scale'),
      requestedChildOrdinal: integerInRange(row.requestedChildOrdinal,
        0, 0xFFFF, label + ' requested child'),
      selectedChildOrdinal: integerInRange(row.selectedChildOrdinal,
        0, source.childCount - 1, label + ' selected child')
    };
    var dimensionsMatch = layer.height === source.height &&
      ((source.formatKind === 'indexed-ci8' ||
        source.formatKind === 'indexed-ci8-alpha8')
        ? layer.width === source.width
        : layer.width <= source.width && source.width - layer.width <= 3);
    if (!dimensionsMatch) fail(label + ' dimensions differ from its copied sprite');
    return layer;
  }

  function prepareStructureProject(entry, donor, label, schemaVersion) {
    if (!entry.sources || typeof entry.sources !== 'object' ||
        Array.isArray(entry.sources) || !Object.keys(entry.sources).length ||
        !Array.isArray(entry.frames) || !entry.frames.length ||
        (schemaVersion < 4 && entry.frames.length > donor.frames.length) ||
        (schemaVersion === 2 && entry.frames.length !== donor.frames.length)) {
      fail(label + ' lacks its complete private frame structure');
    }
    var poseProgram = schemaVersion >= 4
      ? fromBase64Variable(entry.poseProgramBase64,
        label + ' body program')
      : null;
    var sources = {};
    Object.keys(entry.sources).forEach(function(ordinalText) {
      if (!/^(0|[1-9][0-9]*)$/.test(ordinalText)) {
        fail(label + ' contains a non-canonical source ordinal');
      }
      var ordinal = integerInRange(Number(ordinalText), 0, 0xFFFF,
        label + ' source ordinal');
      sources[ordinal] = prepareSourceProject(
        entry.sources[ordinalText], ordinal, label);
    });
    var donorByStable = {}, donorOrdinalByStable = {};
    donor.frames.forEach(function(frame, ordinal) {
      var stable = stableFrameIndex(frame);
      donorByStable[stable] = frame;
      donorOrdinalByStable[stable] = ordinal;
    });
    var used = {}, usedFrameIdentities = {}, previousDonorOrdinal = -1;
    var frames = entry.frames.map(function(row, frameIndex) {
      var allowedKeys = schemaVersion >= 3
        ? ['sequenceIndex', 'sourceFrameIndex', 'token', 'ticks', 'layers']
        : ['sequenceIndex', 'token', 'ticks', 'layers'];
      var sourceFrameIndex = schemaVersion >= 3
        ? integerInRange(row && row.sourceFrameIndex, 0, 0xFFFF,
          label + ' frame ' + (frameIndex + 1) + ' source index')
        : frameIndex;
      var donorFrame = donorByStable[sourceFrameIndex];
      var donorOrdinal = donorOrdinalByStable[sourceFrameIndex];
      var frameMatchesProgram = schemaVersion >= 4
        ? !Object.prototype.hasOwnProperty.call(
          usedFrameIdentities, sourceFrameIndex) &&
          Number.isInteger(row.token) && row.token >= 0 && row.token <= 255 &&
          Number.isInteger(row.ticks) && row.ticks >= 0 && row.ticks <= 255
        : donorFrame && Number.isInteger(donorOrdinal) &&
          donorOrdinal > previousDonorOrdinal &&
          row.token === donorFrame.token && row.ticks === donorFrame.ticks;
      if (!exactKeys(row, allowedKeys) || !frameMatchesProgram ||
          row.sequenceIndex !== frameIndex ||
          !Array.isArray(row.layers) || !row.layers.length ||
          row.layers.length > 0xFFFF) {
        fail(label + ' frame ' + (frameIndex + 1) +
          ' no longer matches its body program');
      }
      usedFrameIdentities[sourceFrameIndex] = true;
      if (schemaVersion < 4) previousDonorOrdinal = donorOrdinal;
      return {
        sequenceIndex: row.sequenceIndex,
        sourceFrameIndex: sourceFrameIndex,
        token: row.token, ticks: row.ticks,
        layers: row.layers.map(function(layer, layerIndex) {
          var prepared = prepareLayerProject(layer, sources,
            label + ' frame ' + (frameIndex + 1) + ' layer ' + (layerIndex + 1));
          used[prepared.sourceOrdinal] = true;
          return prepared;
        })
      };
    });
    if (Object.keys(sources).some(function(ordinal) { return !used[ordinal]; })) {
      fail(label + ' contains an unused copied sprite source');
    }
    if (poseProgram) {
      decodePrivatePoseProgram(donor.poseProgram, poseProgram,
        entry.laneKey === 'idle', frames);
    }
    return { sources: sources, frames: frames, poseProgram: poseProgram };
  }

  function collectProject(rom) {
    var state = rom && rom.animationSequences;
    if (!state || !Object.keys(state.separations).length) return null;
    var entries = {};
    Object.keys(state.separations).sort().forEach(function(id) {
      var separation = state.separations[id];
      var structure = captureStructure(rom.art.animations, separation);
      var sources = {};
      Object.keys(structure.sources).sort(function(left, right) {
        return Number(left) - Number(right);
      }).forEach(function(sourceOrdinal) {
        sources[sourceOrdinal] = sourceProjectRecord(
          structure.sources[sourceOrdinal]);
      });
      entries[id] = {
        classId: separation.classId,
        actionId: separation.actionId,
        bodyFlags: separation.bodyFlags,
        laneKey: separation.laneKey,
        targetRef: separation.targetRef,
        donorRef: separation.donorRef,
        sources: sources,
        frames: structure.frames.map(frameProjectRecord),
        poseProgramBase64: toBase64(structure.poseProgram)
      };
    });
    var baselines = {};
    Object.keys(state.routeBaselines).sort().forEach(function(id) {
      var row = state.routeBaselines[id];
      baselines[id] = {
        entry: row.entry ? Object.assign({}, row.entry) : null,
        pair: clonePair(row.pair)
      };
    });
    return { schemaVersion: 4, entries: entries, routeBaselines: baselines };
  }

  function prepareProject(rom, payload) {
    if (payload === undefined || payload === null) return null;
    if (!payload || typeof payload !== 'object' || Array.isArray(payload) ||
        (payload.schemaVersion !== 1 && payload.schemaVersion !== 2 &&
          payload.schemaVersion !== 3 && payload.schemaVersion !== 4) ||
        !payload.entries ||
        typeof payload.entries !== 'object' || Array.isArray(payload.entries) ||
        Object.keys(payload).some(function(key) {
          return ['schemaVersion', 'entries', 'routeBaselines'].indexOf(key) < 0;
        })) {
      fail('separated animation Project data must use schemaVersion 1, 2, 3, or 4');
    }
    if (!rom.animationSequences || !rom.animationSequences.supported) {
      fail('this ROM cannot load separated animation sequences');
    }
    var prepared = { entries: [], routeBaselines: {} }, requiredBaselines = {};
    function selectorPair(row, label) {
      if (!row || !Number.isInteger(Number(row.normalSelector)) ||
          Number(row.normalSelector) < 0 || Number(row.normalSelector) > 255 ||
          !Number.isInteger(Number(row.blockedSelector)) ||
          Number(row.blockedSelector) < 0 || Number(row.blockedSelector) > 255) {
        fail(label + ' must contain u8 Normal and Blocked selectors');
      }
      return {
        normalSelector: Number(row.normalSelector),
        blockedSelector: Number(row.blockedSelector)
      };
    }
    Object.keys(payload.entries).sort().forEach(function(id) {
      var entry = payload.entries[id];
      var entryKeys = payload.schemaVersion >= 4
        ? ['classId', 'actionId', 'bodyFlags', 'laneKey', 'targetRef',
          'donorRef', 'sources', 'frames', 'poseProgramBase64']
        : (payload.schemaVersion >= 2
        ? ['classId', 'actionId', 'bodyFlags', 'laneKey', 'targetRef',
          'donorRef', 'sources', 'frames']
        : ['classId', 'actionId', 'bodyFlags', 'laneKey', 'targetRef',
          'donorRef', 'sources']);
      var idleEntry = entry && entry.laneKey === 'idle';
      if (!entry || !Number.isInteger(entry.classId) || entry.classId < 0 ||
          entry.classId > 255 || !Number.isInteger(entry.actionId) ||
          (idleEntry ? entry.actionId !== -1 :
            (entry.actionId < 0 || entry.actionId > 255)) ||
          !Number.isInteger(entry.bodyFlags) || entry.bodyFlags < 0 ||
          entry.bodyFlags > 3 || id !== routeId(entry.classId, entry.actionId,
          entry.bodyFlags, entry.laneKey) ||
          (entry.laneKey !== 'normal' && entry.laneKey !== 'blocked' &&
            entry.laneKey !== 'idle') ||
          Object.keys(entry).some(function(key) {
            return entryKeys.indexOf(key) < 0;
          })) {
        fail('separated animation Project route ' + id + ' is invalid');
      }
      var target = resolveRef(rom.art.animations, entry.targetRef);
      var donor = resolveRef(rom.art.animations, entry.donorRef);
      if (Number(entry.classId) !== Number(target.spec.classId) ||
          Number(entry.actionId) !== Number(target.spec.actionId) ||
          Number(entry.bodyFlags) !== bodyFlagsFor(target) ||
          entry.laneKey !== laneFor(target)) {
        fail('separated animation Project route ' + id +
          ' does not match its target reference');
      }
      if (payload.schemaVersion >= 2) {
        var structure = prepareStructureProject(entry, donor,
          'separated animation Project route ' + id, payload.schemaVersion);
        prepared.entries.push({
          id: id,
          classId: Number(entry.classId), actionId: Number(entry.actionId),
          bodyFlags: Number(entry.bodyFlags), laneKey: entry.laneKey,
          targetRef: JSON.parse(JSON.stringify(entry.targetRef)),
          donorRef: JSON.parse(JSON.stringify(entry.donorRef)),
          sources: {}, structure: structure
        });
        if (!idleEntry) {
          requiredBaselines[rowId(
            entry.classId, entry.actionId, entry.bodyFlags)] = true;
        }
        return;
      }
      var sources = entry.sources;
      if (!sources || typeof sources !== 'object' || Array.isArray(sources)) {
        fail('separated animation Project route ' + id + ' lacks source pixels');
      }
      var expected = {};
      Object.keys(donor.artByKey).sort().forEach(function(sourceKey, ordinal) {
        var source = donor.artByKey[sourceKey];
        if (!source.editable) return;
        expected[ordinal] = source;
      });
      if (Object.keys(sources).sort().join(',') !== Object.keys(expected).sort().join(',')) {
        fail('separated animation Project route ' + id +
          ' does not contain every editable cloned source');
      }
      var preparedSources = {};
      Object.keys(expected).forEach(function(sourceOrdinal) {
        var source = expected[sourceOrdinal], sourcePayload = sources[sourceOrdinal];
        var childCount = source.sourceRole === 'body' ? 1 : source.sprite.childCount;
        if (!sourcePayload || typeof sourcePayload !== 'object' ||
            Array.isArray(sourcePayload) ||
            Object.keys(sourcePayload).some(function(key) {
              return ['width', 'height', 'childCount', 'children'].indexOf(key) < 0;
            }) || sourcePayload.width !== source.sprite.width ||
            sourcePayload.height !== source.sprite.height ||
            sourcePayload.childCount !== childCount ||
            !sourcePayload.children || typeof sourcePayload.children !== 'object' ||
            Array.isArray(sourcePayload.children)) {
          fail('separated animation source ' + sourceOrdinal + ' no longer matches');
        }
        var childOrdinals = source.sourceRole === 'body'
          ? [0] : source.sprite.children.map(function(child) { return child.ordinal; });
        if (Object.keys(sourcePayload.children).map(Number).sort(function(a, b) {
          return a - b;
        }).join(',') !== childOrdinals.join(',')) {
          fail('separated animation source ' + sourceOrdinal +
            ' does not contain every editable child');
        }
        var pixels = source.sprite.width * source.sprite.height;
        preparedSources[sourceOrdinal] = { children: {} };
        childOrdinals.forEach(function(child) {
          var childPayload = sourcePayload.children[child];
          if (!childPayload || typeof childPayload !== 'object' ||
              Array.isArray(childPayload) ||
              Object.keys(childPayload).some(function(key) {
                return ['ci8IndicesBase64', 'i4IntensityBase64'].indexOf(key) < 0;
              })) {
            fail('separated animation source ' + sourceOrdinal +
              ' child ' + child + ' is invalid');
          }
          preparedSources[sourceOrdinal].children[child] = {
            indices: fromBase64(childPayload.ci8IndicesBase64, pixels,
              'separated source ' + sourceOrdinal + ' CI8 child ' + child),
            intensity: fromBase64(childPayload.i4IntensityBase64, pixels,
              'separated source ' + sourceOrdinal + ' I4 child ' + child)
          };
        });
      });
      prepared.entries.push({
        id: id,
        classId: Number(entry.classId), actionId: Number(entry.actionId),
        bodyFlags: Number(entry.bodyFlags), laneKey: entry.laneKey,
        targetRef: JSON.parse(JSON.stringify(entry.targetRef)),
        donorRef: JSON.parse(JSON.stringify(entry.donorRef)),
        sources: preparedSources
      });
      if (!idleEntry) {
        requiredBaselines[rowId(
          entry.classId, entry.actionId, entry.bodyFlags)] = true;
      }
    });
    var baselines = payload.routeBaselines;
    if (!baselines || typeof baselines !== 'object' || Array.isArray(baselines) ||
        Object.keys(baselines).sort().join(',') !==
          Object.keys(requiredBaselines).sort().join(',')) {
      fail('separated animation Project route baselines do not match its routes');
    }
    Object.keys(requiredBaselines).forEach(function(id) {
      var row = baselines[id], parts = id.split(':').map(Number);
      if (!row || typeof row !== 'object' || Array.isArray(row) ||
          Object.keys(row).some(function(key) {
            return key !== 'entry' && key !== 'pair';
          })) {
        fail('separated animation baseline ' + id + ' is invalid');
      }
      var baseline = { entry: null, pair: selectorPair(row.pair,
        'separated animation baseline ' + id + ' pair') };
      if (row.entry !== null) {
        var pair = selectorPair(row.entry,
          'separated animation baseline ' + id + ' entry');
        if (Number(row.entry.classId) !== parts[0] ||
            Number(row.entry.actionId) !== parts[1] ||
            Number(row.entry.bodyFlags) !== parts[2]) {
          fail('separated animation baseline ' + id +
            ' exact entry targets another route');
        }
        baseline.entry = {
          classId: parts[0], actionId: parts[1], bodyFlags: parts[2],
          normalSelector: pair.normalSelector,
          blockedSelector: pair.blockedSelector
        };
      }
      prepared.routeBaselines[id] = baseline;
    });
    return prepared;
  }

  function applyProject(rom, prepared) {
    if (!prepared) return 0;
    var state = rom.animationSequences;
    Object.keys(state.separations).forEach(function(id) {
      cleanupSynthetic(rom.art.animations, state.separations[id]);
    });
    state.separations = {};
    state.routeBaselines = {};
    Object.keys(prepared.routeBaselines || {}).forEach(function(id) {
      var row = prepared.routeBaselines[id];
      state.routeBaselines[id] = {
        entry: row.entry ? Object.assign({}, row.entry) : null,
        pair: clonePair(row.pair)
      };
    });
    prepared.entries.forEach(function(entry) {
      state.separations[entry.classId + ':' + entry.actionId + ':' +
        entry.bodyFlags + ':' + entry.laneKey] = {
        id: routeId(entry.classId, entry.actionId, entry.bodyFlags, entry.laneKey),
        classId: Number(entry.classId), actionId: Number(entry.actionId),
        bodyFlags: Number(entry.bodyFlags), laneKey: entry.laneKey,
        selector: 0, targetRef: entry.targetRef, donorRef: entry.donorRef,
        syntheticSourceKeys: [],
        structure: entry.structure || null,
        projectSources: entry.structure ? null : (entry.sources || {})
      };
    });
    var groups = {};
    Object.keys(state.separations).forEach(function(id) {
      var row = state.separations[id]; groups[groupId(row.classId, row.bodyFlags)] = true;
    });
    Object.keys(groups).forEach(function(id) {
      var parts = id.split(':').map(Number);
      updateSelectorsAndSynthetic(rom, parts[0], parts[1]);
    });
    Object.keys(state.separations).forEach(function(id) {
      var separation = state.separations[id], animation = separation.syntheticAnimation;
      Object.keys(separation.projectSources || {}).forEach(function(sourceOrdinal) {
        var source = Object.keys(animation.artByKey).map(function(key) {
          return animation.artByKey[key];
        }).find(function(row) {
          return row.separationSourceOrdinal === Number(sourceOrdinal);
        });
        var payload = separation.projectSources[sourceOrdinal];
        if (!source || !payload) {
          fail('separated animation source ' + sourceOrdinal + ' no longer matches');
        }
        Object.keys(payload.children || {}).forEach(function(childText) {
          var child = Number(childText);
          var indices = payload.children[childText].indices;
          var intensity = payload.children[childText].intensity;
          M.setEdit(rom.art.animations, source.key, child, indices, intensity,
            { history: false });
        });
      });
      delete separation.projectSources;
      syncRow(rom, separation.classId, separation.actionId, separation.bodyFlags);
    });
    state.dirty = true;
    touchState(state);
    return prepared.entries.length;
  }

  function count(state) {
    return state ? Object.keys(state.separations).length : 0;
  }

  function resetAll(rom) {
    var state = rom && rom.animationSequences;
    if (!state || !count(state)) return false;
    var rows = Object.keys(state.separations).map(function(id) {
      return state.separations[id];
    });
    rows.forEach(function(row) { cleanupSynthetic(rom.art.animations, row); });
    state.separations = {};
    Object.keys(state.routeBaselines).forEach(function(id) {
      var parts = id.split(':').map(Number);
      syncRow(rom, parts[0], parts[1], parts[2]);
    });
    state.dirty = true;
    touchState(state);
    return true;
  }

  OB64.animationSequences = {
    SequenceCopyError: SequenceCopyError,
    initialize: initialize,
    bodyFlagsFor: bodyFlagsFor,
    flagLabel: flagLabel,
    laneFor: laneFor,
    routeId: routeId,
    donorRef: donorRef,
    resolveRef: resolveRef,
    separationFor: separationFor,
    routeSeparationFor: routeSeparationFor,
    routeAnimation: routeAnimation,
    selectorAnimation: selectorAnimation,
    sharedAssignmentIssue: sharedAssignmentIssue,
    assignShared: assignShared,
    separateAndAssign: separateAndAssign,
    separationConsumers: separationConsumers,
    removeSeparation: removeSeparation,
    copyFrom: copyFrom,
    addLayerFrom: addLayerFrom,
    addBlankLayer: addBlankLayer,
    addBlankFrame: addBlankFrame,
    copyLayerFrom: copyLayerFrom,
    copyFrameFrom: copyFrameFrom,
    importFrame: importFrame,
    removeLayer: removeLayer,
    removeFrame: removeFrame,
    moveFrame: moveFrame,
    setFrameTicks: setFrameTicks,
    moveLayer: moveLayer,
    setLayerPosition: setLayerPosition,
    rotateIndexedPixels: rotateIndexedPixels,
    resizeIndexedPixels: resizeIndexedPixels,
    rotateLayer: rotateLayer,
    resizeLayer: resizeLayer,
    captureStructure: captureStructure,
    handleIndex: handleIndex,
    buildPlan: buildPlan,
    buildPlanAsync: buildPlanAsync,
    finalizeAllocations: finalizeAllocations,
    applyPlan: applyPlan,
    verifyPlan: verifyPlan,
    currentRanges: currentRanges,
    collectProject: collectProject,
    prepareProject: prepareProject,
    applyProject: applyProject,
    count: count,
    resetAll: resetAll
  };
})();
