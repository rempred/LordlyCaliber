// OB64 Mod Editor - bounded combat-sprite model and repacker
//
// Each supported corpus entry pins one verified class/action pose program and
// its complete metadata/art descriptor. Timeline records, metadata, art
// references, and child selection remain locked. Pixel edits rebuild only the
// selected child planes. A uniquely referenced object stays in place when it
// fits; otherwise the complete 0x5554 resource is copy-on-write relocated and
// its owning descriptor member is replaced.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var A = OB64.art;
  var COMBAT_DESCRIPTOR_TABLE_KEY = 0x003B6CD0;
  var COMBAT_DESCRIPTOR_COUNT = 208;
  var POSE_OPCODE_WIDTHS = [
    1, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 4, 3, 3, 2, 3, 3, 3, 3, 3, 4
  ];

  var SPECS = [
    {
      key: 'fighter-slash',
      classId: 0x02,
      className: 'Fighter',
      actionName: 'Slash',
      selector: 0x28,
      descriptorKey: 0x003D7E3C,
      descriptorMemberCount: 111,
      metadataKey: 0x003BEA46,
      poseKey: 0x003BEFA6,
      configKey: 0x003BF1B6,
      lookupKey: 0x003BF236,
      poseDecodedLength: 651,
      displayOrder: 10,
      variantLabel: 'Normal',
      selectedChildOrdinal: 0,
      weaponChildCount: 17,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16],
      consumerSummary: 'Fighter and Lycanthrope',
      frames: [
        [0x11, 6], [0x12, 14], [0x13, 2], [0x14, 2], [0x15, 2],
        [0x16, 4], [0x17, 4], [0x18, 2], [0x19, 8]
      ]
    },
    {
      key: 'soldier-thrust',
      classId: 0x01,
      className: 'Soldier',
      actionName: 'Thrust',
      selector: 0x28,
      descriptorKey: 0x003BAC1C,
      descriptorMemberCount: 25,
      metadataKey: 0x003B7014,
      poseKey: 0x003B7154,
      configKey: 0x003B736A,
      lookupKey: 0x003B738C,
      poseDecodedLength: 947,
      displayOrder: 20,
      variantLabel: 'Normal',
      selectedChildOrdinal: 0,
      weaponChildCount: 0,
      retailMappedWeaponOrdinals: [],
      consumerSummary: 'Soldier and multiple fallback-routed class variants',
      frames: [
        [0x00, 8], [0x01, 12], [0x02, 4], [0x02, 6],
        [0x03, 2], [0x03, 4], [0x03, 20], [0x04, 8]
      ]
    },
    {
      key: 'berserker-strike',
      classId: 0x06,
      className: 'Berserker',
      actionName: 'Strike',
      selector: 0x28,
      descriptorKey: 0x004A3180,
      descriptorMemberCount: 72,
      metadataKey: 0x004839DE,
      poseKey: 0x00483DD8,
      configKey: 0x00483FFA,
      lookupKey: 0x0048405E,
      poseDecodedLength: 696,
      displayOrder: 30,
      variantLabel: 'Normal',
      selectedChildOrdinal: 0,
      weaponChildCount: 12,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      consumerSummary: 'Berserker, Berserker (Asnabel), ' +
        'Berserker (Stabil), and Berserker (Makisi)',
      frames: [
        [0x11, 8], [0x12, 8], [0x13, 24], [0x15, 4], [0x17, 4],
        [0x19, 4], [0x1B, 4], [0x1D, 4], [0x1D, 2], [0x1E, 6],
        [0x1F, 6], [0x20, 8], [0x21, 8]
      ]
    },
    {
      key: 'black-knight-cleave',
      classId: 0x15,
      className: 'Black Knight',
      actionName: 'Cleave',
      selector: 0x28,
      descriptorKey: 0x007FAAF2,
      descriptorMemberCount: 90,
      metadataKey: 0x007CDA9C,
      poseKey: 0x007CDF70,
      configKey: 0x007CE1D2,
      lookupKey: 0x007CE24E,
      poseDecodedLength: 767,
      displayOrder: 40,
      variantLabel: 'Normal',
      selectedChildOrdinal: 0,
      weaponChildCount: 13,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      consumerSummary: 'Black Knight, Black Knight (Carth), and ' +
        'Black Knight (Jeal)',
      frames: [
        [0x1E, 6], [0x1F, 8], [0x20, 20], [0x21, 6], [0x23, 8],
        [0x25, 4], [0x27, 4], [0x28, 4], [0x2A, 4], [0x29, 4],
        [0x2B, 8], [0x2C, 8], [0x2D, 8], [0x2E, 6], [0x2F, 6]
      ]
    },
    {
      key: 'fighter-slash-blocked',
      classId: 0x02,
      className: 'Fighter',
      actionName: 'Slash',
      variantLabel: 'Blocked mode',
      displayOrder: 11,
      selector: 0x29,
      descriptorKey: 0x003D7E3C,
      descriptorMemberCount: 111,
      metadataKey: 0x003BEA46,
      poseKey: 0x003BEFA6,
      configKey: 0x003BF1B6,
      lookupKey: 0x003BF236,
      poseDecodedLength: 651,
      selectedChildOrdinal: 0,
      weaponChildCount: 17,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16],
      consumerSummary: 'Fighter and Lycanthrope',
      frames: [
        [0x11, 6], [0x12, 10], [0x12, 4], [0x13, 2], [0x14, 4],
        [0x14, 2], [0x14, 2], [0x13, 24], [0x11, 8]
      ]
    },
    {
      key: 'soldier-thrust-blocked',
      classId: 0x01,
      className: 'Soldier',
      actionName: 'Thrust',
      variantLabel: 'Blocked mode',
      displayOrder: 21,
      selector: 0x29,
      descriptorKey: 0x003BAC1C,
      descriptorMemberCount: 25,
      metadataKey: 0x003B7014,
      poseKey: 0x003B7154,
      configKey: 0x003B736A,
      lookupKey: 0x003B738C,
      poseDecodedLength: 947,
      selectedChildOrdinal: 0,
      weaponChildCount: 0,
      retailMappedWeaponOrdinals: [],
      consumerSummary: 'Soldier and multiple fallback-routed class variants',
      frames: [
        [0x00, 8], [0x01, 12], [0x02, 4], [0x03, 4], [0x03, 2],
        [0x03, 2], [0x02, 4], [0x01, 4], [0x01, 4], [0x01, 24], [0x00, 4]
      ]
    },
    {
      key: 'berserker-strike-blocked',
      classId: 0x06,
      className: 'Berserker',
      actionName: 'Strike',
      variantLabel: 'Blocked mode',
      displayOrder: 31,
      selector: 0x29,
      descriptorKey: 0x004A3180,
      descriptorMemberCount: 72,
      metadataKey: 0x004839DE,
      poseKey: 0x00483DD8,
      configKey: 0x00483FFA,
      lookupKey: 0x0048405E,
      poseDecodedLength: 696,
      selectedChildOrdinal: 0,
      weaponChildCount: 12,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      consumerSummary: 'Berserker, Berserker (Asnabel), ' +
        'Berserker (Stabil), and Berserker (Makisi)',
      frames: [
        [0x11, 8], [0x12, 8], [0x13, 22], [0x13, 2], [0x14, 4],
        [0x14, 2], [0x14, 2], [0x13, 4], [0x13, 4], [0x13, 12],
        [0x12, 8], [0x11, 8]
      ]
    },
    {
      key: 'black-knight-cleave-blocked',
      classId: 0x15,
      className: 'Black Knight',
      actionName: 'Cleave',
      variantLabel: 'Blocked mode',
      displayOrder: 41,
      selector: 0x29,
      descriptorKey: 0x007FAAF2,
      descriptorMemberCount: 90,
      metadataKey: 0x007CDA9C,
      poseKey: 0x007CDF70,
      configKey: 0x007CE1D2,
      lookupKey: 0x007CE24E,
      poseDecodedLength: 767,
      selectedChildOrdinal: 0,
      weaponChildCount: 13,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      consumerSummary: 'Black Knight, Black Knight (Carth), and ' +
        'Black Knight (Jeal)',
      frames: [
        [0x1E, 6], [0x1F, 8], [0x20, 20], [0x21, 8],
        [0x22, 4], [0x22, 2], [0x22, 2], [0x21, 20]
      ]
    },
    {
      key: 'black-knight-elemental-magic',
      classId: 0x15,
      className: 'Black Knight',
      actionName: 'Elemental Magic',
      displayOrder: 42,
      selector: 0x2A,
      descriptorKey: 0x007FAAF2,
      descriptorMemberCount: 90,
      metadataKey: 0x007CDA9C,
      poseKey: 0x007CDF70,
      configKey: 0x007CE1D2,
      lookupKey: 0x007CE24E,
      poseDecodedLength: 767,
      selectedChildOrdinal: 0,
      weaponChildCount: 13,
      retailMappedWeaponOrdinals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      consumerSummary: 'Black Knight, Black Knight (Carth), and ' +
        'Black Knight (Jeal)',
      frames: [
        [0x1E, 6], [0x1F, 8], [0x20, 50], [0x21, 6], [0x23, 8],
        [0x25, 4], [0x27, 4], [0x28, 4], [0x2A, 4], [0x29, 4],
        [0x2B, 8], [0x2C, 8], [0x2D, 8], [0x2E, 6], [0x2F, 6]
      ]
    },
    {
      key: 'wizard-elemental-magic',
      classId: 0x0C,
      className: 'Wizard',
      actionName: 'Elemental Magic',
      displayOrder: 50,
      selector: 0x28,
      descriptorKey: 0x0061B9B4,
      descriptorMemberCount: 94,
      metadataKey: 0x005FDCD2,
      poseKey: 0x005FE176,
      configKey: 0x005FE374,
      lookupKey: 0x005FE3DC,
      poseDecodedLength: 618,
      selectedChildOrdinal: 0,
      weaponChildCount: 12,
      retailMappedWeaponOrdinals: [0, 1, 3, 5, 6, 8, 9, 10, 11],
      consumerSummary: 'Wizard, Archmage, Warlock (Saradin), ' +
        'Wizard (Zhontac), and Archmage (Giolse)',
      frames: [
        [0x12, 8], [0x13, 16], [0x13, 8], [0x14, 8], [0x15, 8],
        [0x14, 6], [0x13, 6], [0x14, 4], [0x15, 4], [0x14, 2],
        [0x13, 2], [0x14, 2], [0x15, 12], [0x16, 4], [0x17, 32], [0x18, 8]
      ]
    },
    {
      key: 'siren-elemental-magic',
      classId: 0x1F,
      className: 'Siren',
      actionName: 'Elemental Magic',
      displayOrder: 60,
      selector: 0x29,
      descriptorKey: 0x007385B8,
      descriptorMemberCount: 73,
      metadataKey: 0x0070FF54,
      poseKey: 0x007103F8,
      configKey: 0x0071068C,
      lookupKey: 0x007106F2,
      poseDecodedLength: 817,
      selectedChildOrdinal: 2,
      weaponChildCount: 11,
      retailMappedWeaponOrdinals: [0, 1, 3, 5, 6, 8, 9, 10],
      consumerSummary: 'Sorceress, Siren, Siren (Meredia), and Siren (Eudika)',
      frames: [
        [0x29, 6], [0x2A, 8], [0x2B, 12], [0x2C, 10], [0x2D, 6],
        [0x2E, 6], [0x2A, 4], [0x2B, 4], [0x2C, 2], [0x2D, 2],
        [0x2E, 2], [0x2F, 4], [0x30, 4], [0x31, 4], [0x30, 4],
        [0x31, 6], [0x30, 6], [0x31, 8], [0x30, 8], [0x31, 8],
        [0x32, 8], [0x33, 8], [0x34, 8]
      ]
    }
  ];

  function AnimationArtError(message) {
    this.name = 'AnimationArtError';
    this.message = message;
  }
  AnimationArtError.prototype = Object.create(Error.prototype);
  AnimationArtError.prototype.constructor = AnimationArtError;

  function fail(message) { throw new AnimationArtError(message); }

  function hex(value, width) {
    return '0x' + (Number(value) >>> 0).toString(16).toUpperCase()
      .padStart(width || 8, '0');
  }

  function readU16(bytes, offset) {
    if (offset < 0 || offset + 2 > bytes.length) fail('u16 read lies outside its byte source');
    return (bytes[offset] << 8) | bytes[offset + 1];
  }

  function readS16(bytes, offset) {
    var value = readU16(bytes, offset);
    return value & 0x8000 ? value - 0x10000 : value;
  }

  function readU32(bytes, offset) {
    if (offset < 0 || offset + 4 > bytes.length) fail('u32 read lies outside its byte source');
    return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
      (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function rowBytes(formatIndex, width) {
    if (formatIndex === 0) return Math.ceil(width / 16) * 8;
    if (formatIndex === 1) return Math.ceil(width / 8) * 8;
    if (formatIndex === 2) return Math.ceil(width / 4) * 8;
    if (formatIndex === 3) return Math.ceil(width / 4) * 16;
    fail('unknown combat-sprite lane format ' + formatIndex);
  }

  function parseSpriteObject(decoded, resourceKey) {
    var label = 'combat art resource ' + hex(resourceKey);
    if (decoded.length < 8 || readU16(decoded, 0) !== 0x5554) {
      fail(label + ' does not begin with a 0x5554 outer header');
    }
    var childCount = decoded[2], flags = decoded[3];
    var width = readU16(decoded, 4), height = readU16(decoded, 6);
    var firstFormat = flags & 0x02 ? 3 : (flags & 0x04 ? 1 : 2);
    var secondFormat = flags & 0x01 ? 0 : 1;
    var firstStride = rowBytes(firstFormat, width);
    var secondStride = rowBytes(secondFormat, width);
    var firstSize = firstStride * height;
    var secondSize = secondStride * height;
    var cursor = 8, children = [];
    for (var ordinal = 0; ordinal < childCount; ordinal++) {
      var start = cursor;
      if (cursor + 8 > decoded.length || readU16(decoded, cursor) !== 0x5554) {
        fail(label + ' child ' + ordinal + ' lacks its 0x5554 header');
      }
      var discriminator = decoded[cursor + 2], childFlags = decoded[cursor + 3];
      var childWidth = readU16(decoded, cursor + 4);
      var childHeight = readU16(decoded, cursor + 6);
      cursor += 8;
      var firstOffset = null, secondOffset = null, lookupOffset = null;
      if (childFlags & 0x01) { firstOffset = cursor; cursor += firstSize; }
      if (childFlags & 0x02) { secondOffset = cursor; cursor += secondSize; }
      if (childFlags & 0x04) { lookupOffset = cursor; cursor += 0x200; }
      if (cursor > decoded.length) fail(label + ' child ' + ordinal + ' exceeds the decoded object');
      children.push({
        ordinal: ordinal, start: start, end: cursor,
        discriminator: discriminator, flags: childFlags,
        widthField: childWidth, heightField: childHeight,
        firstOffset: firstOffset, secondOffset: secondOffset,
        lookupOffset: lookupOffset
      });
    }
    if (cursor !== decoded.length) {
      fail(label + ' parsed ' + cursor + ' decoded bytes; expected ' + decoded.length);
    }
    return {
      decoded: decoded, resourceKey: resourceKey, childCount: childCount,
      flags: flags, width: width, height: height,
      firstFormat: firstFormat, secondFormat: secondFormat,
      firstStride: firstStride, secondStride: secondStride,
      firstSize: firstSize, secondSize: secondSize, children: children
    };
  }

  function parseConfig(decoded, artCount, label) {
    if (decoded.length < 0x20) fail(label + ' is shorter than its fixed header');
    var storedCount = readU32(decoded, 0), mapOffset = readU32(decoded, 0x1C);
    if (storedCount !== artCount) {
      fail(label + ' describes ' + storedCount + ' art members; descriptor has ' + artCount);
    }
    if (mapOffset + artCount > decoded.length) fail(label + ' lookup-bank map exceeds its resource');
    return { artCount: artCount, mapOffset: mapOffset, bankMap: decoded.slice(mapOffset, mapOffset + artCount) };
  }

  function parseMetadataFrame(decoded, token) {
    var pointerOffset = token * 4;
    if (pointerOffset + 4 > decoded.length) fail('frame token ' + hex(token, 2) + ' exceeds metadata table');
    var target = readU32(decoded, pointerOffset);
    if (target + 2 > decoded.length) fail('frame token ' + hex(token, 2) + ' has an invalid metadata pointer');
    var count = readU16(decoded, target), layers = [];
    for (var ordinal = 0; ordinal < count; ordinal++) {
      var start = target + 2 + ordinal * 16;
      if (start + 16 > decoded.length) fail('frame token ' + hex(token, 2) + ' metadata is truncated');
      layers.push({
        ordinal: ordinal,
        artId: readU16(decoded, start),
        drawOffsetX: readS16(decoded, start + 2),
        drawOffsetY: readS16(decoded, start + 4),
        width: readU16(decoded, start + 6),
        height: readU16(decoded, start + 8),
        flags: readU16(decoded, start + 10),
        scaleXRaw: readU16(decoded, start + 12),
        scaleYRaw: readU16(decoded, start + 14),
        metadataOffset: start
      });
    }
    return { target: target, layers: layers };
  }

  function decodeChild(sprite, childOrdinal) {
    var child = sprite.children[childOrdinal];
    if (!child) fail('resource ' + hex(sprite.resourceKey) + ' lacks child ' + childOrdinal);
    if (sprite.firstFormat !== 1 || sprite.secondFormat !== 0 ||
        child.firstOffset === null || child.secondOffset === null) {
      fail('resource ' + hex(sprite.resourceKey) +
        ' is outside the bounded CI8 + I4 combat-sprite editor format');
    }
    var indices = new Uint8Array(sprite.width * sprite.height);
    var intensity = new Uint8Array(sprite.width * sprite.height);
    var pixel = 0;
    for (var y = 0; y < sprite.height; y++) {
      var firstRow = child.firstOffset + y * sprite.firstStride;
      var secondRow = child.secondOffset + y * sprite.secondStride;
      for (var x = 0; x < sprite.width; x++, pixel++) {
        indices[pixel] = sprite.decoded[firstRow + x];
        var packed = sprite.decoded[secondRow + (x >>> 1)];
        intensity[pixel] = x & 1 ? packed & 15 : packed >>> 4;
      }
    }
    return { indices: indices, intensity: intensity };
  }

  function lookupWords(decoded, bankCount) {
    if (decoded.length !== bankCount * 0x200) fail('conversion lookup resource has an invalid bank extent');
    var banks = [];
    for (var bank = 0; bank < bankCount; bank++) {
      var words = new Uint16Array(256);
      for (var index = 0; index < 256; index++) {
        words[index] = readU16(decoded, bank * 0x200 + index * 2);
      }
      banks.push(words);
    }
    return banks;
  }

  function parsePoseProgram(decoded, selector, label) {
    if (decoded.length < 4) fail(label + ' is shorter than its pose directory');
    var first = readU32(decoded, 0);
    if (!first || first % 4 || first > decoded.length) {
      fail(label + ' has an invalid pose-directory extent');
    }
    var stateCount = first / 4, offsets = [];
    for (var state = 0; state < stateCount; state++) {
      var pointer = readU32(decoded, state * 4);
      if (pointer < first || pointer > decoded.length ||
          (state && pointer < offsets[state - 1])) {
        fail(label + ' has an invalid pose-directory pointer at state ' + state);
      }
      offsets.push(pointer);
    }
    if (!Number.isInteger(selector) || selector < 0 || selector >= stateCount) {
      fail(label + ' does not contain selector ' + hex(selector, 2));
    }
    var start = offsets[selector];
    var end = selector + 1 < stateCount ? offsets[selector + 1] : decoded.length;
    if (start >= end) fail(label + ' selector ' + hex(selector, 2) + ' is empty');
    var program = decoded.slice(start, end);
    var recordCount = program[0], cursor = 1, records = [], frames = [];
    for (var ordinal = 0; ordinal < recordCount; ordinal++) {
      if (cursor >= program.length) fail(label + ' pose program ends before its record count');
      var opcode = program[cursor];
      if (opcode >= POSE_OPCODE_WIDTHS.length) {
        fail(label + ' pose program contains unsupported opcode ' + hex(opcode, 2));
      }
      var width = POSE_OPCODE_WIDTHS[opcode];
      if (!width || cursor + width > program.length) {
        fail(label + ' pose opcode ' + hex(opcode, 2) + ' exceeds its program');
      }
      var operands = Array.prototype.slice.call(program, cursor + 1, cursor + width);
      records.push({
        ordinal: ordinal, offset: start + cursor, opcode: opcode,
        width: width, operands: operands
      });
      if (opcode === 0x01) {
        if (width !== 3) fail(label + ' frame opcode has an invalid width');
        frames.push([operands[0], operands[1]]);
      }
      cursor += width;
    }
    if (cursor !== program.length) {
      fail(label + ' pose program consumed ' + cursor + ' of ' + program.length + ' bytes');
    }
    return {
      selector: selector, stateCount: stateCount, start: start, end: end,
      program: program, recordCount: recordCount, records: records, frames: frames
    };
  }

  function parseSpec(z64, spec) {
    var descriptor = A.readResource(z64, spec.descriptorKey);
    if (descriptor.storedLength !== spec.descriptorMemberCount * 4) {
      fail(spec.className + ' descriptor has ' + (descriptor.storedLength / 4) +
        ' members; expected ' + spec.descriptorMemberCount);
    }
    var members = [];
    for (var member = 0; member < spec.descriptorMemberCount; member++) {
      members.push(readU32(descriptor.stored, member * 4));
    }
    var expectedControls = [spec.metadataKey, spec.poseKey, spec.configKey, spec.lookupKey];
    for (var control = 0; control < 4; control++) {
      if (members[control] !== expectedControls[control]) {
        fail(spec.className + ' descriptor member ' + control + ' is ' +
          hex(members[control]) + '; expected ' + hex(expectedControls[control]));
      }
    }
    var metadata = A.readCompressedResource(z64, spec.metadataKey).decoded;
    var pose = A.readCompressedResource(z64, spec.poseKey).decoded;
    if (pose.length !== spec.poseDecodedLength) {
      fail(spec.className + ' ' + spec.actionName + ' pose resource has ' + pose.length +
        ' decoded bytes; expected ' + spec.poseDecodedLength);
    }
    var poseProgram = parsePoseProgram(pose, spec.selector,
      spec.className + ' ' + spec.actionName);
    if (poseProgram.frames.length !== spec.frames.length) {
      fail(spec.className + ' ' + spec.actionName + ' pose program exposes ' +
        poseProgram.frames.length + ' frames; expected ' + spec.frames.length);
    }
    for (var poseFrame = 0; poseFrame < spec.frames.length; poseFrame++) {
      if (poseProgram.frames[poseFrame][0] !== spec.frames[poseFrame][0] ||
          poseProgram.frames[poseFrame][1] !== spec.frames[poseFrame][1]) {
        fail(spec.className + ' ' + spec.actionName + ' pose frame ' +
          (poseFrame + 1) + ' differs from the verified corpus');
      }
    }
    var configDecoded = A.readCompressedResource(z64, spec.configKey).decoded;
    var lookupDecoded = A.readCompressedResource(z64, spec.lookupKey).decoded;
    var config = parseConfig(configDecoded, members.length - 4,
      spec.className + ' art configuration');
    if (lookupDecoded.length % 0x200) fail(spec.className + ' lookup resource is not packed in 0x200-byte banks');
    var banks = lookupWords(lookupDecoded, lookupDecoded.length / 0x200);
    var artById = {}, artByKey = {}, frames = [];
    var originX = 0, originY = 0, endX = 0, endY = 0, hasBounds = false;

    spec.frames.forEach(function(frameSpec, sequenceIndex) {
      var token = frameSpec[0], ticks = frameSpec[1];
      var parsed = parseMetadataFrame(metadata, token);
      var layers = parsed.layers.map(function(layer) {
        if (layer.artId >= members.length - 4) {
          fail('frame ' + (sequenceIndex + 1) + ' selects art ID ' + layer.artId +
            ' outside the ' + spec.className + ' descriptor');
        }
        var resourceKey = members[layer.artId + 4];
        var source = artById[layer.artId];
        if (!source) {
          var resource = A.readCompressedResource(z64, resourceKey);
          var sprite = parseSpriteObject(resource.decoded, resourceKey);
          if (config.bankMap[layer.artId] >= banks.length) {
            fail('art ID ' + layer.artId + ' selects lookup bank ' +
              config.bankMap[layer.artId] + ' outside the lookup resource');
          }
          var weaponSelectable = !!spec.weaponChildCount &&
            sprite.childCount === spec.weaponChildCount;
          var editableChildOrdinals = weaponSelectable
            ? sprite.children.map(function(child) { return child.ordinal; })
            : [spec.selectedChildOrdinal];
          var originalChildren = {};
          editableChildOrdinals.forEach(function(childOrdinal) {
            originalChildren[childOrdinal] = decodeChild(sprite, childOrdinal);
          });
          var childPixels = originalChildren[spec.selectedChildOrdinal];
          source = {
            key: spec.key + ':' + layer.artId,
            animationKey: spec.key, artId: layer.artId,
            animationLabel: spec.className + ' ' + spec.actionName,
            animationKeys: [spec.key],
            animationLabels: [spec.className + ' ' + spec.actionName],
            descriptorMemberIndex: layer.artId + 4,
            descriptorEntryOffset: descriptor.entry + 4 + (layer.artId + 4) * 4,
            resourceKey: resourceKey, resource: resource, sprite: sprite,
            childOrdinal: spec.selectedChildOrdinal,
            weaponSelectable: weaponSelectable,
            editableChildOrdinals: editableChildOrdinals,
            originalChildren: originalChildren,
            lookupBank: config.bankMap[layer.artId],
            palette: banks[config.bankMap[layer.artId]],
            originalIndices: childPixels.indices,
            originalIntensity: childPixels.intensity,
            usageFrames: [],
            usageFramesByAnimation: {}
          };
          source.usageFramesByAnimation[spec.key] = source.usageFrames;
          artById[layer.artId] = source;
          artByKey[source.key] = source;
        }
        if (source.sprite.width !== layer.width || source.sprite.height !== layer.height) {
          fail('frame ' + (sequenceIndex + 1) + ' art ID ' + layer.artId +
            ' dimensions differ between metadata and the 0x5554 object');
        }
        if (source.usageFrames.indexOf(sequenceIndex) < 0) source.usageFrames.push(sequenceIndex);
        var left = layer.drawOffsetX, top = layer.drawOffsetY;
        var right = left + layer.width, bottom = top + layer.height;
        if (!hasBounds) {
          originX = left; originY = top; endX = right; endY = bottom; hasBounds = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
        layer.sourceKey = source.key;
        layer.resourceKey = resourceKey;
        layer.lookupBank = source.lookupBank;
        layer.childCount = source.sprite.childCount;
        return layer;
      });
      frames.push({
        sequenceIndex: sequenceIndex, token: token, ticks: ticks,
        metadataTarget: parsed.target, layers: layers
      });
    });

    if (!hasBounds) fail(spec.className + ' ' + spec.actionName + ' has no drawable layers');
    return {
      key: spec.key, spec: spec, descriptor: descriptor, members: members,
      metadata: metadata, pose: pose, poseProgram: poseProgram,
      config: config, lookupBanks: banks,
      frames: frames, artById: artById, artByKey: artByKey,
      canvas: {
        originX: originX, originY: originY, endX: endX, endY: endY,
        width: endX - originX, height: endY - originY
      }
    };
  }

  function descriptorReferenceCounts(z64) {
    var table = A.readResource(z64, COMBAT_DESCRIPTOR_TABLE_KEY);
    if (table.storedLength !== COMBAT_DESCRIPTOR_COUNT * 4) {
      fail('combat descriptor table has ' + (table.storedLength / 4) +
        ' entries; expected ' + COMBAT_DESCRIPTOR_COUNT);
    }
    var counts = {};
    for (var index = 0; index < COMBAT_DESCRIPTOR_COUNT; index++) {
      var descriptorKey = readU32(table.stored, index * 4);
      if (!descriptorKey) continue;
      var descriptor = A.readResource(z64, descriptorKey);
      if (descriptor.storedLength < 16 || descriptor.storedLength % 4) {
        fail('combat descriptor ' + hex(descriptorKey) + ' is not a raw u32 member list');
      }
      for (var offset = 16; offset < descriptor.storedLength; offset += 4) {
        var resourceKey = readU32(descriptor.stored, offset);
        counts[resourceKey] = (counts[resourceKey] || 0) + 1;
      }
    }
    return counts;
  }

  function initialize(z64) {
    var state = {
      supported: false, unavailableReason: '', specs: [], byKey: {}, artByKey: {},
      edits: {}, history: {}, blocked: {}
    };
    try {
      var sourceByDescriptorEntry = {};
      SPECS.forEach(function(spec) {
        var parsed = parseSpec(z64, spec);
        var mergedByKey = {};
        Object.keys(parsed.artById).forEach(function(artId) {
          var source = parsed.artById[artId];
          var identity = String(source.descriptorEntryOffset);
          var canonical = sourceByDescriptorEntry[identity];
          if (!canonical) {
            sourceByDescriptorEntry[identity] = source;
            canonical = source;
          } else {
            if (canonical.resourceKey !== source.resourceKey ||
                canonical.artId !== source.artId ||
                canonical.sprite.width !== source.sprite.width ||
                canonical.sprite.height !== source.sprite.height ||
                canonical.sprite.childCount !== source.sprite.childCount ||
                canonical.lookupBank !== source.lookupBank ||
                canonical.weaponSelectable !== source.weaponSelectable ||
                !equalBytes(canonical.palette, source.palette) ||
                canonical.editableChildOrdinals.join(',') !==
                  source.editableChildOrdinals.join(',')) {
              fail('shared combat descriptor member ' +
                hex(source.descriptorEntryOffset) +
                ' differs between bounded animation sequences');
            }
            canonical.animationKeys.push(spec.key);
            canonical.animationLabels.push(
              spec.className + ' ' + spec.actionName);
            canonical.usageFramesByAnimation[spec.key] = source.usageFrames;
          }
          parsed.artById[artId] = canonical;
          mergedByKey[canonical.key] = canonical;
        });
        parsed.frames.forEach(function(frame) {
          frame.layers.forEach(function(layer) {
            layer.sourceKey = parsed.artById[layer.artId].key;
          });
        });
        parsed.artByKey = mergedByKey;
        state.specs.push(parsed); state.byKey[parsed.key] = parsed;
        Object.keys(parsed.artByKey).forEach(function(key) {
          if (!state.artByKey[key]) state.artByKey[key] = parsed.artByKey[key];
        });
      });
      var references = descriptorReferenceCounts(z64);
      Object.keys(state.artByKey).forEach(function(key) {
        var source = state.artByKey[key];
        source.descriptorReferenceCount = references[source.resourceKey] || 0;
        if (!source.descriptorReferenceCount) {
          fail(source.key + ' is not referenced by the combat descriptor corpus');
        }
        source.inPlaceEligible = source.descriptorReferenceCount === 1;
      });
      state.supported = state.specs.length > 0;
    } catch (error) {
      state.unavailableReason = 'Combat sprite editing is unavailable for this ROM: ' +
        (error && error.message ? error.message : String(error));
    }
    return state;
  }

  function childKey(key, childOrdinal) {
    return key + '#child-' + childOrdinal;
  }

  function originalChild(source, childOrdinal) {
    if (!Number.isInteger(childOrdinal) ||
        source.editableChildOrdinals.indexOf(childOrdinal) < 0 ||
        !source.originalChildren[childOrdinal]) {
      fail(source.key + ' child ' + childOrdinal + ' is not editable in this bounded corpus');
    }
    return source.originalChildren[childOrdinal];
  }

  function currentEdit(state, key, childOrdinal) {
    var source = state.artByKey[key];
    if (!source) fail('unknown combat-sprite source ' + key);
    childOrdinal = childOrdinal === undefined ? source.childOrdinal : childOrdinal;
    var original = originalChild(source, childOrdinal);
    var group = state.edits[key];
    return group && group.children[childOrdinal]
      ? group.children[childOrdinal]
      : original;
  }

  function snapshot(edit) {
    return { indices: edit.indices.slice(), intensity: edit.intensity.slice() };
  }

  function validatePixels(source, indices, intensity) {
    var pixels = source.sprite.width * source.sprite.height;
    if (!(indices instanceof Uint8Array) || indices.length !== pixels) {
      fail(source.key + ' must contain exactly ' + pixels + ' CI8 indices');
    }
    if (!(intensity instanceof Uint8Array) || intensity.length !== pixels) {
      fail(source.key + ' must contain exactly ' + pixels + ' I4 intensity values');
    }
    for (var i = 0; i < intensity.length; i++) {
      if (intensity[i] > 15) fail(source.key + ' intensity pixel ' + i + ' exceeds 15');
    }
  }

  function historyFor(state, key, childOrdinal) {
    var source = state.artByKey[key];
    if (!source) fail('unknown combat-sprite source ' + key);
    childOrdinal = childOrdinal === undefined ? source.childOrdinal : childOrdinal;
    originalChild(source, childOrdinal);
    var identity = childKey(key, childOrdinal);
    if (!state.history[identity]) state.history[identity] = { undo: [], redo: [] };
    return state.history[identity];
  }

  function setEdit(state, key, childOrdinal, indices, intensity, options) {
    options = options || {};
    var source = state.artByKey[key];
    if (!source) fail('unknown combat-sprite source ' + key);
    var original = originalChild(source, childOrdinal);
    validatePixels(source, indices, intensity);
    var current = currentEdit(state, key, childOrdinal);
    if (equalBytes(current.indices, indices) && equalBytes(current.intensity, intensity)) return false;
    if (options.history !== false) {
      var history = historyFor(state, key, childOrdinal);
      history.undo.push(snapshot(current));
      if (history.undo.length > 100) history.undo.shift();
      history.redo = [];
    }
    if (equalBytes(original.indices, indices) &&
        equalBytes(original.intensity, intensity)) {
      if (state.edits[key]) {
        delete state.edits[key].children[childOrdinal];
        if (!Object.keys(state.edits[key].children).length) delete state.edits[key];
      }
    } else {
      if (!state.edits[key]) state.edits[key] = { children: {} };
      state.edits[key].children[childOrdinal] = {
        indices: indices.slice(), intensity: intensity.slice()
      };
    }
    delete state.blocked[childKey(key, childOrdinal)];
    return true;
  }

  function undo(state, key, childOrdinal) {
    var history = historyFor(state, key, childOrdinal);
    if (!history.undo.length) return false;
    var current = snapshot(currentEdit(state, key, childOrdinal));
    var prior = history.undo.pop(); history.redo.push(current);
    return setEdit(state, key, childOrdinal, prior.indices, prior.intensity,
      { history: false });
  }

  function redo(state, key, childOrdinal) {
    var history = historyFor(state, key, childOrdinal);
    if (!history.redo.length) return false;
    var current = snapshot(currentEdit(state, key, childOrdinal));
    var next = history.redo.pop(); history.undo.push(current);
    return setEdit(state, key, childOrdinal, next.indices, next.intensity,
      { history: false });
  }

  function toBase64(bytes) {
    var parts = [];
    for (var start = 0; start < bytes.length; start += 0x8000) {
      var chars = '', slice = bytes.subarray(start, Math.min(bytes.length, start + 0x8000));
      for (var i = 0; i < slice.length; i++) chars += String.fromCharCode(slice[i]);
      parts.push(chars);
    }
    return btoa(parts.join(''));
  }

  function fromBase64(text, label) {
    if (typeof text !== 'string' || !text.length) fail(label + ' is not base64 text');
    var raw;
    try { raw = atob(text); } catch (error) { fail(label + ' is not valid base64'); }
    var bytes = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
    return bytes;
  }

  function collectProject(state) {
    var output = {};
    if (!state || !state.supported) return output;
    Object.keys(state.edits).sort().forEach(function(key) {
      var source = state.artByKey[key], group = state.edits[key], children = {};
      Object.keys(group.children).sort(function(left, right) {
        return Number(left) - Number(right);
      }).forEach(function(childOrdinal) {
        var edit = group.children[childOrdinal];
        children[childOrdinal] = {
          ci8IndicesBase64: toBase64(edit.indices),
          i4IntensityBase64: toBase64(edit.intensity)
        };
      });
      output[key] = {
        animation: source.animationKey, artId: source.artId,
        resourceKey: hex(source.resourceKey),
        width: source.sprite.width, height: source.sprite.height,
        children: children
      };
    });
    return output;
  }

  function prepareProject(state, payload) {
    var prepared = {}, count = 0;
    payload = payload || {};
    if (Object.keys(payload).length && (!state || !state.supported)) {
      fail('This ROM cannot load bounded combat-sprite Project records');
    }
    Object.keys(payload).forEach(function(key) {
      var entry = payload[key], source = state.artByKey[key];
      if (!source || !entry || entry.animation !== source.animationKey ||
          entry.artId !== source.artId || entry.resourceKey !== hex(source.resourceKey) ||
          entry.width !== source.sprite.width || entry.height !== source.sprite.height) {
        fail('combat-sprite Project record ' + key + ' does not match its verified source');
      }
      var childPayloads = entry.children;
      if (!childPayloads && Number.isInteger(entry.childOrdinal)) {
        childPayloads = {};
        childPayloads[entry.childOrdinal] = {
          ci8IndicesBase64: entry.ci8IndicesBase64,
          i4IntensityBase64: entry.i4IntensityBase64
        };
      }
      if (!childPayloads || typeof childPayloads !== 'object' ||
          Array.isArray(childPayloads) || !Object.keys(childPayloads).length) {
        fail('combat-sprite Project record ' + key + ' has no edited children');
      }
      var group = { children: {} };
      Object.keys(childPayloads).forEach(function(childKeyText) {
        if (!/^(0|[1-9][0-9]*)$/.test(childKeyText)) {
          fail(key + ' child key ' + childKeyText + ' is not a canonical ordinal');
        }
        var childOrdinal = Number(childKeyText), childEntry = childPayloads[childKeyText];
        originalChild(source, childOrdinal);
        if (group.children[childOrdinal]) {
          fail(key + ' repeats child ' + childOrdinal);
        }
        if (!childEntry || typeof childEntry !== 'object') {
          fail(key + ' child ' + childOrdinal + ' is not a Project pixel record');
        }
        var indices = fromBase64(childEntry.ci8IndicesBase64,
          key + ' child ' + childOrdinal + ' CI8 indices');
        var intensity = fromBase64(childEntry.i4IntensityBase64,
          key + ' child ' + childOrdinal + ' I4 intensity');
        validatePixels(source, indices, intensity);
        group.children[childOrdinal] = { indices: indices, intensity: intensity };
        count++;
      });
      prepared[key] = group;
    });
    return { edits: prepared, count: count };
  }

  function applyPrepared(state, prepared) {
    var applied = 0;
    Object.keys(prepared.edits || {}).forEach(function(key) {
      var group = prepared.edits[key];
      Object.keys(group.children).forEach(function(childOrdinal) {
        var edit = group.children[childOrdinal];
        if (setEdit(state, key, Number(childOrdinal), edit.indices, edit.intensity)) {
          applied++;
        }
      });
    });
    return applied;
  }

  function buildDecoded(source, childEdits) {
    var sprite = source.sprite, decoded = sprite.decoded.slice();
    Object.keys(childEdits || {}).forEach(function(childOrdinalText) {
      var childOrdinal = Number(childOrdinalText), edit = childEdits[childOrdinalText];
      originalChild(source, childOrdinal);
      validatePixels(source, edit.indices, edit.intensity);
      var child = sprite.children[childOrdinal], pixel = 0;
      for (var y = 0; y < sprite.height; y++) {
        var firstRow = child.firstOffset + y * sprite.firstStride;
        var secondRow = child.secondOffset + y * sprite.secondStride;
        for (var x = 0; x < sprite.width; x++, pixel++) {
          decoded[firstRow + x] = edit.indices[pixel];
          var packedOffset = secondRow + (x >>> 1);
          if (x & 1) {
            decoded[packedOffset] = (decoded[packedOffset] & 0xF0) |
              edit.intensity[pixel];
          } else {
            decoded[packedOffset] = (decoded[packedOffset] & 0x0F) |
              (edit.intensity[pixel] << 4);
          }
        }
      }
    });
    var reparsed = parseSpriteObject(decoded, source.resourceKey);
    Object.keys(childEdits || {}).forEach(function(childOrdinalText) {
      var edit = childEdits[childOrdinalText];
      var rebuilt = decodeChild(reparsed, Number(childOrdinalText));
      if (!equalBytes(rebuilt.indices, edit.indices) ||
          !equalBytes(rebuilt.intensity, edit.intensity)) {
        fail(source.key + ' child ' + childOrdinalText +
          ' decoded-plane readback differs after rebuild');
      }
    });
    return decoded;
  }

  function buildResources(state) {
    var resources = [];
    if (!state || !state.supported) return resources;
    Object.keys(state.edits).sort().forEach(function(key, ordinal) {
      var source = state.artByKey[key], edit = state.edits[key];
      var decoded = buildDecoded(source, edit.children);
      var stored = A.bootLzCompress(decoded);
      var verified = A.bootLzDecode(stored);
      if (verified.bytesConsumed !== stored.length || !equalBytes(verified.output, decoded)) {
        fail(source.key + ' compressed resource failed exact readback');
      }
      var originalCapacity = source.resource.storedLength +
        (source.resource.storedLength & 1);
      resources.push({
        name: 'combat-sprite-' + ordinal, key: key, source: source,
        edit: edit,
        built: { decoded: decoded, stored: stored },
        originalCapacity: originalCapacity,
        placement: source.inPlaceEligible && stored.length <= originalCapacity
          ? 'in-place' : 'relocated'
      });
    });
    return resources;
  }

  function applyResources(rows, bytes, ranges, log) {
    rows.forEach(function(row) {
      if (row.placement === 'in-place') {
        var resource = A.readResource(bytes, row.source.resourceKey);
        if (resource.storedLength !== row.source.resource.storedLength ||
            !equalBytes(resource.stored, row.source.resource.stored)) {
          fail(row.key + ' in-place resource preimage differs from the loaded source');
        }
        A.writeU32(bytes, resource.entry, row.built.stored.length);
        bytes.set(row.built.stored, resource.entry + 4);
        bytes.fill(0, resource.entry + 4 + row.built.stored.length,
          resource.entry + 4 + row.originalCapacity);
        ranges.push([resource.entry, resource.entry + 4 + row.originalCapacity]);
        log.push(row.source.animationLabel + ' art ' + hex(row.source.artId, 2) +
          ': in-place ' + row.built.stored.length + '/' +
          row.originalCapacity + ' bytes; ' + Object.keys(row.edit.children).length +
          ' edited child sprite' +
          (Object.keys(row.edit.children).length === 1 ? '' : 's') + '; all ' +
          row.source.sprite.childCount + ' children rebuilt');
        return;
      }
      if (!row.allocation) fail(row.key + ' lacks its relocation allocation');
      var offset = row.source.descriptorEntryOffset;
      var observed = A.readU32(bytes, offset);
      if (observed !== row.source.resourceKey) {
        fail(row.key + ' descriptor preimage is ' + hex(observed) +
          '; expected ' + hex(row.source.resourceKey));
      }
      A.writeU32(bytes, offset, row.allocation.key);
      ranges.push([offset, offset + 4]);
      log.push(row.source.animationLabel + ' art ' + hex(row.source.artId, 2) +
        ': copied resource ' + hex(row.source.resourceKey) + ' to ' +
        hex(row.allocation.key) + '; descriptor member ' +
        row.source.descriptorMemberIndex + ' updated; ' +
        Object.keys(row.edit.children).length + ' edited child sprite' +
        (Object.keys(row.edit.children).length === 1 ? '' : 's') + '; all ' +
        row.source.sprite.childCount + ' children rebuilt');
    });
  }

  function verifyResources(rows, bytes) {
    rows.forEach(function(row) {
      var expectedKey = row.placement === 'in-place'
        ? row.source.resourceKey : row.allocation.key;
      var observed = A.readU32(bytes, row.source.descriptorEntryOffset);
      if (observed !== expectedKey) {
        fail(row.key + ' descriptor readback is ' + hex(observed) +
          '; expected ' + hex(expectedKey));
      }
      var decoded = A.readCompressedResource(bytes, expectedKey).decoded;
      if (!equalBytes(decoded, row.built.decoded)) {
        fail(row.key + ' exported resource differs after compressed readback');
      }
      var sprite = parseSpriteObject(decoded, expectedKey);
      Object.keys(row.edit.children).forEach(function(childOrdinalText) {
        var child = decodeChild(sprite, Number(childOrdinalText));
        var edit = row.edit.children[childOrdinalText];
        if (!equalBytes(child.indices, edit.indices) ||
            !equalBytes(child.intensity, edit.intensity)) {
          fail(row.key + ' child ' + childOrdinalText +
            ' exported visible planes differ from the staged edit');
        }
      });
    });
  }

  function snapshotEdits(state) {
    var output = {};
    if (!state) return output;
    Object.keys(state.edits).forEach(function(key) {
      output[key] = { children: {} };
      Object.keys(state.edits[key].children).forEach(function(childOrdinal) {
        output[key].children[childOrdinal] = snapshot(
          state.edits[key].children[childOrdinal]);
      });
    });
    return output;
  }

  function restoreEdits(state, edits) {
    state.edits = {};
    Object.keys(edits || {}).forEach(function(key) {
      var source = state.artByKey[key], group = edits[key];
      if (!source || !group || !group.children) {
        fail('invalid combat-sprite edit snapshot for ' + key);
      }
      state.edits[key] = { children: {} };
      Object.keys(group.children).forEach(function(childOrdinal) {
        originalChild(source, Number(childOrdinal));
        validatePixels(source, group.children[childOrdinal].indices,
          group.children[childOrdinal].intensity);
        state.edits[key].children[childOrdinal] = snapshot(
          group.children[childOrdinal]);
      });
      if (!Object.keys(state.edits[key].children).length) delete state.edits[key];
    });
    state.blocked = {};
  }

  function resetAll(state) {
    if (!state) return;
    state.edits = {}; state.blocked = {};
  }

  function sourceEditCount(state, key) {
    return state && state.edits && state.edits[key]
      ? Object.keys(state.edits[key].children).length : 0;
  }

  function hasEdit(state, key, childOrdinal) {
    return !!(state && state.edits && state.edits[key] &&
      state.edits[key].children[childOrdinal]);
  }

  function editCount(state) {
    if (!state || !state.edits) return 0;
    return Object.keys(state.edits).reduce(function(total, key) {
      return total + sourceEditCount(state, key);
    }, 0);
  }
  function blockedCount(state) { return state && state.blocked ? Object.keys(state.blocked).length : 0; }

  OB64.animationArt = {
    specs: SPECS,
    AnimationArtError: AnimationArtError,
    rowBytes: rowBytes,
    parseSpriteObject: parseSpriteObject,
    parseMetadataFrame: parseMetadataFrame,
    parsePoseProgram: parsePoseProgram,
    descriptorReferenceCounts: descriptorReferenceCounts,
    decodeChild: decodeChild,
    parseSpec: parseSpec,
    initialize: initialize,
    childKey: childKey,
    originalChild: originalChild,
    currentEdit: currentEdit,
    setEdit: setEdit,
    historyFor: historyFor,
    undo: undo,
    redo: redo,
    collectProject: collectProject,
    prepareProject: prepareProject,
    applyPrepared: applyPrepared,
    buildDecoded: buildDecoded,
    buildResources: buildResources,
    applyResources: applyResources,
    verifyResources: verifyResources,
    snapshotEdits: snapshotEdits,
    restoreEdits: restoreEdits,
    resetAll: resetAll,
    sourceEditCount: sourceEditCount,
    hasEdit: hasEdit,
    editCount: editCount,
    blockedCount: blockedCount,
    equalBytes: equalBytes,
    hex: hex
  };
})();
