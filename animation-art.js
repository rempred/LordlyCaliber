// OB64 Mod Editor - complete verified combat-sprite model and repacker
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
  var CLASS_HANDLE_RESOURCE_KEY = 0x00315736;
  var CLASS_HANDLE_TABLE_OFFSET = 0x24;
  var CLASS_HANDLE_COUNT = 688;
  var POSE_OPCODE_WIDTHS = [
    1, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 4, 3, 3, 2, 3, 3, 3, 3, 3, 4
  ];

  // Vanilla equipment items whose appearance-table ordinals address the
  // weapon children exposed by the frozen Project-v19 fixtures. Multiple items can
  // intentionally share one child. An ordinal outside a source's child count
  // is handled by the game as a child-zero fallback.
  var WEAPON_ITEM_FAMILIES = {
    'one-handed-sword': {
      label: 'One-handed sword',
      equipType: 0x01,
      items: [
        [0x01, 0], [0x02, 1], [0x03, 2], [0x04, 3], [0x05, 4],
        [0x06, 5], [0x07, 6], [0x08, 7], [0x09, 8], [0x0A, 8],
        [0x0B, 9], [0x0C, 10], [0x0D, 11], [0x0E, 11], [0x0F, 12],
        [0x10, 13], [0x11, 12], [0x12, 14], [0x13, 15], [0x14, 16],
        [0x15, 0]
      ]
    },
    'one-handed-axe': {
      label: 'One-handed axe/hammer',
      equipType: 0x04,
      items: [
        [0x30, 0], [0x31, 1], [0x32, 2], [0x33, 3], [0x34, 4],
        [0x35, 5], [0x36, 6], [0x37, 7], [0x38, 8], [0x39, 9],
        [0x3A, 10], [0x3B, 11], [0x3C, 11]
      ]
    },
    'two-handed-axe': {
      label: 'Two-handed axe/hammer',
      equipType: 0x05,
      items: [
        [0x3D, 0], [0x3E, 1], [0x3F, 2], [0x40, 3], [0x41, 4],
        [0x42, 5], [0x43, 6], [0x44, 7], [0x45, 8], [0x46, 9],
        [0x47, 10], [0x48, 11]
      ]
    },
    staff: {
      label: 'Staff',
      equipType: 0x0C,
      items: [
        [0x75, 0], [0x76, 6], [0x77, 1], [0x78, 3], [0x79, 5],
        [0x7A, 9], [0x7B, 8], [0x7C, 10], [0x7D, 11], [0x7E, 12]
      ]
    }
  };

  // Retained only as a Project-v19 compatibility ledger. Runtime selection
  // comes from the generated, independently reviewed complete corpus below.
  var LEGACY_SPECS = [
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
      weaponItemFamilyKey: 'one-handed-sword',
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
      weaponItemFamilyKey: 'one-handed-axe',
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
      weaponItemFamilyKey: 'two-handed-axe',
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
      weaponItemFamilyKey: 'one-handed-sword',
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
      weaponItemFamilyKey: 'one-handed-axe',
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
      weaponItemFamilyKey: 'two-handed-axe',
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
      weaponItemFamilyKey: 'two-handed-axe',
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
      weaponItemFamilyKey: 'staff',
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
      weaponItemFamilyKey: 'staff',
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

  var CORPUS = OB64.animationCorpusData || null;
  var SPECS = CORPUS && Array.isArray(CORPUS.sequences)
    ? CORPUS.sequences
    : [];

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

  function equalOptionalBytes(left, right) {
    return left === null && right === null || equalBytes(left, right);
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
      firstSize: firstSize, secondSize: secondSize, children: children,
      materializedChildren: {}
    };
  }

  function laneBytes(sprite, child, offset, size, label) {
    if (offset === null) return null;
    if (offset < 0 || offset + size > sprite.decoded.length) {
      fail('resource ' + hex(sprite.resourceKey) + ' child ' + child.ordinal +
        ' ' + label + ' lies outside its decoded object');
    }
    return sprite.decoded.slice(offset, offset + size);
  }

  function combineDeltaLane(base, delta, format, label, resourceKey, childOrdinal) {
    if (!(base instanceof Uint8Array) || !(delta instanceof Uint8Array) ||
        base.length !== delta.length) {
      fail('resource ' + hex(resourceKey) + ' child ' + childOrdinal + ' ' +
        label + ' lacks a matching discriminator base lane');
    }
    var output = new Uint8Array(delta.length);
    var offset, baseWord, deltaWord, value;
    if (format === 0) {
      for (offset = 0; offset < delta.length; offset++) {
        output[offset] = ((((base[offset] >>> 4) - (delta[offset] >>> 4)) & 15) << 4) |
          (((base[offset] & 15) - (delta[offset] & 15)) & 15);
      }
      return output;
    }
    if (format === 1) {
      for (offset = 0; offset < delta.length; offset++) {
        output[offset] = (base[offset] - delta[offset]) & 0xFF;
      }
      return output;
    }
    if (format === 2) {
      if (delta.length & 1) {
        fail('resource ' + hex(resourceKey) + ' child ' + childOrdinal + ' ' +
          label + ' has an odd direct-color lane length');
      }
      for (offset = 0; offset < delta.length; offset += 2) {
        baseWord = (base[offset] << 8) | base[offset + 1];
        deltaWord = (delta[offset] << 8) | delta[offset + 1];
        value = (baseWord - deltaWord) & 0xFFFF;
        output[offset] = value >>> 8;
        output[offset + 1] = value & 0xFF;
      }
      return output;
    }
    if (format === 3) {
      if (delta.length & 3) {
        fail('resource ' + hex(resourceKey) + ' child ' + childOrdinal + ' ' +
          label + ' has a non-word-aligned 32-bit lane length');
      }
      for (offset = 0; offset < delta.length; offset++) {
        output[offset] = base[offset] ^ delta[offset];
      }
      return output;
    }
    fail('resource ' + hex(resourceKey) + ' child ' + childOrdinal +
      ' uses unknown ' + label + ' format ' + format);
  }

  function materializeChildLanes(sprite, childOrdinal, visiting) {
    var cached = sprite.materializedChildren[childOrdinal];
    if (cached) return cached;
    var child = sprite.children[childOrdinal];
    if (!child) fail('resource ' + hex(sprite.resourceKey) + ' lacks child ' + childOrdinal);
    visiting = visiting || {};
    if (visiting[childOrdinal]) {
      fail('resource ' + hex(sprite.resourceKey) + ' has a cyclic child discriminator at ' +
        childOrdinal);
    }
    visiting[childOrdinal] = true;
    var first = laneBytes(sprite, child, child.firstOffset, sprite.firstSize, 'first lane');
    var second = laneBytes(sprite, child, child.secondOffset, sprite.secondSize, 'second lane');
    var lookup = laneBytes(sprite, child, child.lookupOffset, 0x200, 'embedded lookup');
    var transformed = child.discriminator !== child.ordinal;
    if (transformed) {
      if (child.discriminator < 0 || child.discriminator >= sprite.childCount) {
        fail('resource ' + hex(sprite.resourceKey) + ' child ' + childOrdinal +
          ' has invalid discriminator ' + child.discriminator);
      }
      var base = materializeChildLanes(sprite, child.discriminator, visiting);
      if (!(child.flags & 0x08)) {
        if (first) first = combineDeltaLane(base.first, first, sprite.firstFormat,
          'first lane', sprite.resourceKey, childOrdinal);
        if (second) second = combineDeltaLane(base.second, second, sprite.secondFormat,
          'second lane', sprite.resourceKey, childOrdinal);
        if (lookup) lookup = combineDeltaLane(base.lookup, lookup, 2,
          'embedded lookup', sprite.resourceKey, childOrdinal);
      }
    }
    delete visiting[childOrdinal];
    cached = {
      ordinal: childOrdinal,
      discriminator: child.discriminator,
      transformed: transformed,
      first: first,
      second: second,
      lookup: lookup
    };
    sprite.materializedChildren[childOrdinal] = cached;
    return cached;
  }

  function parseConfig(decoded, artCount, label) {
    if (decoded.length < 0x20) fail(label + ' is shorter than its fixed header');
    var storedCount = readU32(decoded, 0), mapOffset = readU32(decoded, 0x1C);
    if (storedCount !== artCount) {
      fail(label + ' describes ' + storedCount + ' art members; descriptor has ' + artCount);
    }
    if (0x20 + artCount > decoded.length) fail(label + ' selector-policy bytes exceed its resource');
    if (mapOffset + artCount > decoded.length) fail(label + ' lookup-bank map exceeds its resource');
    var selectorPolicies = decoded.slice(0x20, 0x20 + artCount);
    for (var artId = 0; artId < selectorPolicies.length; artId++) {
      if (selectorPolicies[artId] > 2) {
        fail(label + ' art ' + artId + ' has unknown selector policy ' +
          selectorPolicies[artId]);
      }
    }
    return {
      artCount: artCount,
      policyOffset: 0x20,
      selectorPolicies: selectorPolicies,
      mapOffset: mapOffset,
      bankMap: decoded.slice(mapOffset, mapOffset + artCount)
    };
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
        ' is outside the editable CI8 + I4 combat-sprite format');
    }
    var lanes = materializeChildLanes(sprite, childOrdinal);
    var indices = new Uint8Array(sprite.width * sprite.height);
    var intensity = new Uint8Array(sprite.width * sprite.height);
    var pixel = 0;
    for (var y = 0; y < sprite.height; y++) {
      var firstRow = y * sprite.firstStride;
      var secondRow = y * sprite.secondStride;
      for (var x = 0; x < sprite.width; x++, pixel++) {
        indices[pixel] = lanes.first[firstRow + x];
        var packed = lanes.second[secondRow + (x >>> 1)];
        intensity[pixel] = x & 1 ? packed & 15 : packed >>> 4;
      }
    }
    return { indices: indices, intensity: intensity };
  }

  function decodeDirectChild(sprite, childOrdinal) {
    var child = sprite.children[childOrdinal];
    if (!child) fail('resource ' + hex(sprite.resourceKey) + ' lacks child ' + childOrdinal);
    if (sprite.firstFormat !== 2 || child.firstOffset === null) {
      fail('resource ' + hex(sprite.resourceKey) +
        ' is not a direct RGBA5551 combat-sprite source');
    }
    var lanes = materializeChildLanes(sprite, childOrdinal);
    var words = new Uint16Array(sprite.width * sprite.height);
    var alpha = new Uint8Array(sprite.width * sprite.height);
    var pixel = 0;
    for (var y = 0; y < sprite.height; y++) {
      var firstRow = y * sprite.firstStride;
      var secondRow = child.secondOffset === null
        ? null : y * sprite.secondStride;
      for (var x = 0; x < sprite.width; x++, pixel++) {
        var word = readU16(lanes.first, firstRow + x * 2);
        words[pixel] = word;
        if (secondRow === null) {
          alpha[pixel] = word & 1 ? 255 : 0;
        } else if (sprite.secondFormat === 1) {
          alpha[pixel] = lanes.second[secondRow + x];
        } else if (sprite.secondFormat === 0) {
          var packed = lanes.second[secondRow + (x >>> 1)];
          alpha[pixel] = (x & 1 ? packed & 15 : packed >>> 4) * 17;
        } else {
          fail('resource ' + hex(sprite.resourceKey) +
            ' has unsupported direct-source alpha format ' + sprite.secondFormat);
        }
      }
    }
    return { words: words, alpha: alpha };
  }

  function decodeIndexedAlphaChild(source, childOrdinal) {
    var sprite = source.sprite;
    if (sprite.firstFormat !== 1 || sprite.secondFormat !== 1) {
      fail('resource ' + hex(sprite.resourceKey) +
        ' is not a CI8 + I8 sprite source');
    }
    var lanes = materializeChildLanes(sprite, childOrdinal);
    if (!lanes.first) {
      fail('resource ' + hex(sprite.resourceKey) +
        ' selected child has no CI8 color lane');
    }
    var palette = childPalette(source, childOrdinal);
    var words = new Uint16Array(sprite.width * sprite.height);
    var alpha = new Uint8Array(sprite.width * sprite.height);
    var pixel = 0;
    for (var y = 0; y < sprite.height; y++) {
      var firstRow = y * sprite.firstStride;
      var secondRow = y * sprite.secondStride;
      for (var x = 0; x < sprite.width; x++, pixel++) {
        var word = palette[lanes.first[firstRow + x]];
        words[pixel] = word;
        alpha[pixel] = lanes.second ? lanes.second[secondRow + x] :
          (word & 1 ? 255 : 0);
      }
    }
    return { words: words, alpha: alpha };
  }

  function childOrdinalOrFallback(source, childOrdinal) {
    return Number.isInteger(childOrdinal) && childOrdinal >= 0 &&
      childOrdinal < source.sprite.childCount ? childOrdinal : 0;
  }

  function childPalette(source, childOrdinal) {
    childOrdinal = childOrdinalOrFallback(source, childOrdinal);
    var child = source.sprite.children[childOrdinal];
    if (child.lookupOffset === null) return source.palette;
    var words = source.embeddedPalettes[childOrdinal];
    if (words) return words;
    var lookup = materializeChildLanes(source.sprite, childOrdinal).lookup;
    words = new Uint16Array(256);
    for (var index = 0; index < 256; index++) {
      words[index] = readU16(lookup, index * 2);
    }
    source.embeddedPalettes[childOrdinal] = words;
    return words;
  }

  function ensureOriginalChild(source, childOrdinal) {
    childOrdinal = childOrdinalOrFallback(source, childOrdinal);
    if (source.formatKind === 'indexed-ci8') {
      if (!source.originalChildren[childOrdinal]) {
        source.originalChildren[childOrdinal] = decodeChild(source.sprite, childOrdinal);
      }
      return source.originalChildren[childOrdinal];
    }
    if (!source.displayChildren[childOrdinal]) {
      source.displayChildren[childOrdinal] = source.formatKind === 'indexed-ci8-alpha8'
        ? decodeIndexedAlphaChild(source, childOrdinal)
        : decodeDirectChild(source.sprite, childOrdinal);
    }
    return source.displayChildren[childOrdinal];
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

  function cleanCorpusName(value) {
    return String(value || '')
      .replace(/\x10c/g, ' ')
      .replace(/[\x00-\x1F]/g, '')
      .replace(/\s+/g, ' ')
      .trim();
  }

  function sameNumberArray(left, right) {
    return Array.isArray(left) && Array.isArray(right) &&
      left.length === right.length && left.every(function(value, index) {
        return value === right[index];
      });
  }

  function validateCorpus() {
    if (!CORPUS || CORPUS.schemaVersion !== 'ob64-combat-animation-product-data-v2') {
      fail('the generated complete combat-animation corpus is missing or has an unknown schema');
    }
    if (CORPUS.corpusVersion !== '2026-08-15-r2' ||
        CORPUS.sourceProjectionSha256 !==
          '6D6FD9528E927B5EC5EFCBBB78CE8F098C049F8BFADE4E46551705E2BC10849F' ||
        SPECS.length !== 2712 ||
        Object.keys(CORPUS.physicalSources || {}).length !== 4707 ||
        Object.keys(CORPUS.equipmentGroups || {}).length !== 84) {
      fail('the generated complete combat-animation corpus identity or required counts changed');
    }
    var selectorZero = CORPUS.selectorZeroSourceClosure;
    if (!selectorZero || selectorZero.selector !== 0 ||
        selectorZero.poseProgramCount !== 129 ||
        selectorZero.physicalSourceCount !== 1228 ||
        selectorZero.addedPhysicalSourceCount !== 1016) {
      fail('the generated selector-0 source closure is missing or changed');
    }
    var bindingByDescriptorArt = {}, bindingById = {}, physicalByBindingId = {};
    Object.keys(CORPUS.physicalSources).forEach(function(physicalId) {
      var physical = CORPUS.physicalSources[physicalId];
      if (!physical || physical.id !== physicalId || physical.objectType !== '0x5554') {
        fail('physical source ' + physicalId + ' has an invalid identity');
      }
      physical.bindings.forEach(function(binding) {
        var identity = binding.descriptorKey + ':' + binding.artId;
        if (bindingByDescriptorArt[identity] || bindingById[binding.id]) {
          fail('combat-animation corpus repeats binding ' + binding.id);
        }
        if (binding.descriptorMemberOrdinal !== binding.artId + 4 ||
            binding.descriptorMemberEntryZ64 !==
              binding.descriptorEntryZ64 + 4 + binding.descriptorMemberOrdinal * 4) {
          fail('binding ' + binding.id + ' has an invalid descriptor relationship');
        }
        bindingByDescriptorArt[identity] = binding;
        bindingById[binding.id] = binding;
        physicalByBindingId[binding.id] = physical;
      });
    });
    if (Object.keys(bindingById).length !== 4900) {
      fail('the complete combat-animation corpus must contain 4,900 logical bindings');
    }
    var compatibility = {};
    SPECS.forEach(function(spec) {
      if (spec.compatibilityKey) compatibility[spec.compatibilityKey] = spec;
    });
    if (Object.keys(compatibility).length !== LEGACY_SPECS.length) {
      fail('the complete corpus does not expose all Project-v19 compatibility sequences');
    }
    LEGACY_SPECS.forEach(function(legacy) {
      var spec = compatibility[legacy.key];
      if (!spec || spec.classId !== legacy.classId || spec.selector !== legacy.selector ||
          spec.descriptorKey !== legacy.descriptorKey ||
          spec.descriptorMemberCount !== legacy.descriptorMemberCount ||
          spec.metadataKey !== legacy.metadataKey || spec.poseKey !== legacy.poseKey ||
          spec.configKey !== legacy.configKey || spec.lookupKey !== legacy.lookupKey ||
          spec.poseDecodedLength !== legacy.poseDecodedLength ||
          spec.selectedBodyChild !== legacy.selectedChildOrdinal ||
          spec.weaponChildCount !== legacy.weaponChildCount ||
          !sameNumberArray(spec.retailMappedWeaponOrdinals,
            legacy.retailMappedWeaponOrdinals) ||
          !sameNumberArray(spec.frames.flat(), legacy.frames.flat())) {
        fail('compatibility sequence ' + legacy.key + ' differs from its frozen Project-v19 ledger');
      }
    });
    return {
      bindingByDescriptorArt: bindingByDescriptorArt,
      bindingById: bindingById,
      physicalByBindingId: physicalByBindingId,
      bindingCount: Object.keys(bindingById).length
    };
  }

  function corpusContext(z64, index) {
    return {
      z64: z64,
      index: index,
      descriptorCache: {},
      resourceCache: {},
      spriteCache: {},
      configCache: {},
      lookupCache: {},
      poseCache: {},
      metadataFrameCache: {},
      equipmentGroupCache: {},
      dynamicPhysicalByResourceKey: {},
      sourceByBindingId: {}
    };
  }

  function cachedCompressed(context, key) {
    if (!context.resourceCache[key]) {
      context.resourceCache[key] = A.readCompressedResource(context.z64, key);
    }
    return context.resourceCache[key];
  }

  function corpusDescriptor(context, spec) {
    var descriptor = context.descriptorCache[spec.descriptorKey];
    if (!descriptor) {
      var resource = A.readResource(context.z64, spec.descriptorKey);
      if (resource.storedLength !== spec.descriptorMemberCount * 4) {
        fail(cleanCorpusName(spec.className) + ' descriptor has ' +
          (resource.storedLength / 4) + ' members; expected ' +
          spec.descriptorMemberCount);
      }
      var members = [];
      for (var member = 0; member < spec.descriptorMemberCount; member++) {
        members.push(readU32(resource.stored, member * 4));
      }
      descriptor = { resource: resource, members: members };
      context.descriptorCache[spec.descriptorKey] = descriptor;
    }
    if (descriptor.members.length !== spec.descriptorMemberCount) {
      fail('descriptor ' + hex(spec.descriptorKey) + ' changed member count between sequences');
    }
    var controls = [spec.metadataKey, spec.poseKey, spec.configKey, spec.lookupKey];
    for (var control = 0; control < controls.length; control++) {
      if (descriptor.members[control] !== controls[control]) {
        fail('descriptor ' + hex(spec.descriptorKey) + ' member ' + control +
          ' is ' + hex(descriptor.members[control]) + '; expected ' +
          hex(controls[control]));
      }
    }
    return descriptor;
  }

  function corpusConfig(context, spec) {
    var identity = spec.configKey + ':' + (spec.descriptorMemberCount - 4);
    if (!context.configCache[identity]) {
      context.configCache[identity] = parseConfig(
        cachedCompressed(context, spec.configKey).decoded,
        spec.descriptorMemberCount - 4,
        cleanCorpusName(spec.className) + ' art configuration');
    }
    return context.configCache[identity];
  }

  function corpusLookup(context, spec) {
    var lookup = context.lookupCache[spec.lookupKey];
    if (!lookup) {
      var resource = cachedCompressed(context, spec.lookupKey);
      if (resource.decoded.length % 0x200) {
        fail('lookup resource ' + hex(spec.lookupKey) +
          ' is not packed in 0x200-byte banks');
      }
      lookup = {
        resource: resource,
        banks: lookupWords(resource.decoded, resource.decoded.length / 0x200)
      };
      context.lookupCache[spec.lookupKey] = lookup;
    }
    return lookup;
  }

  function corpusPose(context, spec, label, acceptDynamicFrames) {
    var identity = spec.poseKey + ':' + spec.selector;
    var pose = context.poseCache[identity];
    if (!pose) {
      var resource = cachedCompressed(context, spec.poseKey);
      if (resource.decoded.length !== spec.poseDecodedLength) {
        fail(label + ' pose resource has ' + resource.decoded.length +
          ' decoded bytes; expected ' + spec.poseDecodedLength);
      }
      pose = {
        resource: resource,
        program: parsePoseProgram(resource.decoded, spec.selector, label)
      };
      context.poseCache[identity] = pose;
    }
    if (!acceptDynamicFrames && pose.program.frames.length !== spec.frames.length) {
      fail(label + ' pose program exposes ' + pose.program.frames.length +
        ' frames; expected ' + spec.frames.length);
    }
    if (!acceptDynamicFrames) {
      spec.frames.forEach(function(frame, index) {
        if (pose.program.frames[index][0] !== frame[0] ||
            pose.program.frames[index][1] !== frame[1]) {
          fail(label + ' pose frame ' + (index + 1) +
            ' differs from the accepted complete corpus');
        }
      });
    }
    return pose;
  }

  function corpusMetadataFrame(context, metadataKey, token) {
    var identity = metadataKey + ':' + token;
    if (!context.metadataFrameCache[identity]) {
      context.metadataFrameCache[identity] = parseMetadataFrame(
        cachedCompressed(context, metadataKey).decoded, token);
    }
    return context.metadataFrameCache[identity];
  }

  function validatePhysicalSprite(physical, resource, sprite) {
    if (resource.key !== physical.resourceKey || resource.entry !== physical.entryZ64 ||
        sprite.flags !== physical.objectSubtype || sprite.width !== physical.width ||
        sprite.height !== physical.height || sprite.childCount !== physical.childCount ||
        sprite.firstFormat !== physical.format.firstLaneFormat ||
        sprite.secondFormat !== physical.format.secondLaneFormat ||
        sprite.firstStride !== physical.format.firstRowBytes ||
        sprite.secondStride !== physical.format.secondRowBytes) {
      fail('physical source ' + physical.id +
        ' does not match the loaded ROM resource structure');
    }
    var expectedKind = sprite.firstFormat === 1 && sprite.secondFormat === 0
      ? 'indexed-ci8'
      : (sprite.firstFormat === 2 ? 'direct-rgba5551' : 'unsupported');
    if (expectedKind !== physical.format.kind) {
      fail('physical source ' + physical.id + ' has an unexpected pixel format');
    }
  }

  function dynamicCorpusBinding(context, spec, descriptor, config, lookup,
      artId) {
    var identity = spec.descriptorKey + ':' + artId;
    var resourceKey = descriptor.members[artId + 4];
    if (!resourceKey) return null;
    var resource = cachedCompressed(context, resourceKey);
    var sprite = parseSpriteObject(resource.decoded, resourceKey);
    var selectorPolicy = config.selectorPolicies[artId];
    if (selectorPolicy !== 0 && selectorPolicy !== 1) {
      fail('unprojected binding ' + identity +
        ' uses unsupported dynamic selector policy ' + selectorPolicy);
    }
    var formatKind = sprite.firstFormat === 1 && sprite.secondFormat === 0
      ? 'indexed-ci8' : (sprite.firstFormat === 2
        ? 'direct-rgba5551' : 'unsupported');
    var physicalId = 'dynamic-source-' +
      resourceKey.toString(16).padStart(8, '0');
    var physical = context.dynamicPhysicalByResourceKey[resourceKey];
    if (!physical) {
      physical = {
        id: physicalId,
        objectType: '0x5554',
        objectSubtype: sprite.flags,
        resourceKey: resourceKey,
        entryZ64: resource.entry,
        childCount: sprite.childCount,
        width: sprite.width,
        height: sprite.height,
        format: {
          kind: formatKind,
          firstLaneFormat: sprite.firstFormat,
          secondLaneFormat: sprite.secondFormat,
          firstRowBytes: sprite.firstStride,
          secondRowBytes: sprite.secondStride,
          rowPaddingRule: 'read directly from the loaded ROM resource'
        },
        children: sprite.children.map(function(child) {
          return { ordinal: child.ordinal, state: 'loaded-from-rom' };
        }),
        bindings: []
      };
      context.dynamicPhysicalByResourceKey[resourceKey] = physical;
    }
    var role = selectorPolicy === 1 ? 'equipment' : 'body';
    var bindingId = 'dynamic-binding-' +
      spec.descriptorKey.toString(16).padStart(8, '0') + '-' +
      artId.toString(16).padStart(3, '0') + '-' +
      resourceKey.toString(16).padStart(8, '0');
    var binding = {
      id: bindingId,
      artId: artId,
      descriptorKey: spec.descriptorKey,
      descriptorEntryZ64: descriptor.resource.entry,
      descriptorMemberOrdinal: artId + 4,
      descriptorMemberEntryZ64: descriptor.resource.entry + 4 +
        (artId + 4) * 4,
      lookupResourceKey: spec.lookupKey,
      lookupResourceDecodedLength: lookup.resource.decoded.length,
      lookupBankCount: lookup.banks.length,
      lookupBank: config.bankMap[artId],
      selectorPolicy: selectorPolicy,
      sourceRole: role,
      childSelectionPolicy: selectorPolicy === 1
        ? 'equipped-item-appearance-table' : 'class-handle-high-nibble',
      palettePolicy: 'descriptor-lookup-bank-or-child-embedded-lookup',
      elementSelection: null
    };
    physical.bindings.push(binding);
    context.index.bindingByDescriptorArt[identity] = binding;
    context.index.bindingById[bindingId] = binding;
    context.index.physicalByBindingId[bindingId] = physical;
    context.index.dynamicBindingCount =
      (context.index.dynamicBindingCount || 0) + 1;
    return binding;
  }

  function corpusSource(context, spec, descriptor, config, lookup, artId, label) {
    var identity = spec.descriptorKey + ':' + artId;
    var binding = context.index.bindingByDescriptorArt[identity];
    if (!binding) binding = dynamicCorpusBinding(
      context, spec, descriptor, config, lookup, artId);
    if (!binding) fail(label + ' art ' + hex(artId, 2) +
      ' has no readable ROM binding');
    var physical = context.index.physicalByBindingId[binding.id];
    var source = context.sourceByBindingId[binding.id];
    var resourceKey = descriptor.members[artId + 4];
    if (resourceKey !== physical.resourceKey ||
        descriptor.resource.entry !== binding.descriptorEntryZ64 ||
        binding.descriptorMemberEntryZ64 !==
          descriptor.resource.entry + 4 + binding.descriptorMemberOrdinal * 4 ||
        config.selectorPolicies[artId] !== binding.selectorPolicy ||
        config.bankMap[artId] !== binding.lookupBank ||
        binding.lookupResourceKey !== spec.lookupKey ||
        binding.lookupResourceDecodedLength !== lookup.resource.decoded.length ||
        binding.lookupBankCount !== lookup.banks.length ||
        binding.lookupBank >= lookup.banks.length) {
      fail('binding ' + binding.id + ' does not match its loaded descriptor, policy, or lookup data');
    }
    if (source) return source;

    var physicalCache = context.spriteCache[physical.id];
    if (!physicalCache) {
      var resource = cachedCompressed(context, physical.resourceKey);
      var sprite = parseSpriteObject(resource.decoded, physical.resourceKey);
      validatePhysicalSprite(physical, resource, sprite);
      physicalCache = { resource: resource, sprite: sprite };
      context.spriteCache[physical.id] = physicalCache;
    }
    var editable = physical.format.kind === 'indexed-ci8';
    source = {
      key: binding.id,
      bindingId: binding.id,
      physicalSourceId: physical.id,
      binding: binding,
      physicalSource: physical,
      sourceRole: binding.sourceRole,
      selectorPolicy: binding.selectorPolicy,
      childSelectionPolicy: binding.childSelectionPolicy,
      palettePolicy: binding.palettePolicy,
      elementSelection: binding.elementSelection,
      formatKind: physical.format.kind,
      editable: editable,
      lockedReason: editable ? '' :
        'Direct RGBA5551 combat art is visible but is not editable in this release.',
      animationKey: spec.key,
      animationLabel: label,
      animationKeys: [],
      animationLabels: [],
      legacyKeys: [],
      legacyAnimationKeys: [],
      artId: artId,
      descriptorKey: spec.descriptorKey,
      descriptorMemberIndex: binding.descriptorMemberOrdinal,
      descriptorEntryOffset: binding.descriptorMemberEntryZ64,
      resourceKey: physical.resourceKey,
      resource: physicalCache.resource,
      sprite: physicalCache.sprite,
      childOrdinal: null,
      weaponSelectable: binding.sourceRole === 'equipment',
      selectableChildOrdinals: binding.sourceRole === 'equipment'
        ? physicalCache.sprite.children.map(function(child) { return child.ordinal; })
        : [],
      editableChildOrdinals: [],
      originalChildren: {},
      displayChildren: {},
      embeddedPalettes: {},
      lookupBank: binding.lookupBank,
      palette: lookup.banks[binding.lookupBank],
      usageFrames: [],
      usageFramesByAnimation: {}
    };
    if (source.weaponSelectable && source.editable) {
      source.editableChildOrdinals = source.selectableChildOrdinals.slice();
    }
    context.sourceByBindingId[binding.id] = source;
    return source;
  }

  function addSourceUsage(source, spec, label, frameIndex, childOrdinal,
      trackRouteUsage) {
    if (trackRouteUsage !== false) {
      if (source.animationKeys.indexOf(spec.key) < 0) {
        source.animationKeys.push(spec.key);
        source.animationLabels.push(label);
        source.usageFramesByAnimation[spec.key] = [];
      }
      var usage = source.usageFramesByAnimation[spec.key];
      if (usage.indexOf(frameIndex) < 0) usage.push(frameIndex);
    }
    if (source.childOrdinal === null) source.childOrdinal = childOrdinal;
    if (source.editable && source.editableChildOrdinals.indexOf(childOrdinal) < 0) {
      source.editableChildOrdinals.push(childOrdinal);
      source.editableChildOrdinals.sort(function(left, right) { return left - right; });
    }
    if (source.editable && !source.originalIndices) {
      var original = ensureOriginalChild(source, childOrdinal);
      source.originalIndices = original.indices;
      source.originalIntensity = original.intensity;
    }
  }

  function compareCanvas(actual, expected, label) {
    ['originX', 'originY', 'endX', 'endY', 'width', 'height'].forEach(function(field) {
      if (actual[field] !== expected[field]) {
        fail(label + ' computed canvas ' + field + ' is ' + actual[field] +
          '; accepted corpus expects ' + expected[field]);
      }
    });
  }

  function corpusEquipmentGroup(context, groupId) {
    if (context.equipmentGroupCache[groupId]) {
      return context.equipmentGroupCache[groupId];
    }
    var accepted = CORPUS.equipmentGroups[groupId];
    if (!accepted) fail('unknown accepted equipment group ' + groupId);
    var group = Object.assign({}, accepted, {
      children: accepted.children.map(function(child) {
        return Object.assign({}, child, {
          mappedItems: child.mappedItems.map(function(item) {
            return Object.assign({}, item);
          })
        });
      }),
      fallbackItems: []
    });
    var equipmentTypes = {};
    group.children.forEach(function(child) {
      child.mappedItems.forEach(function(item) {
        equipmentTypes[item.equipmentType] = item.equipmentTypeName;
      });
    });
    var selection = group.selectionTable;
    var start = selection && selection.rangeZ64 && selection.rangeZ64[0];
    if (Number.isInteger(start)) {
      var maximum = Math.min(0x115, selection.entryCount - 1);
      for (var itemId = 1; itemId <= maximum; itemId++) {
        var equipmentTypeOffset = 0x62310 + itemId * 32;
        if (equipmentTypeOffset >= context.z64.length) break;
        var equipmentType = context.z64[equipmentTypeOffset];
        if (!Object.prototype.hasOwnProperty.call(equipmentTypes, equipmentType)) continue;
        var requestedOrdinal = context.z64[start + itemId];
        if (requestedOrdinal >= group.expectedChildCount) {
          group.fallbackItems.push({
            itemId: itemId,
            itemName: OB64.ITEM_NAMES && OB64.ITEM_NAMES[itemId]
              ? OB64.ITEM_NAMES[itemId] : 'Item ' + hex(itemId, 2),
            equipmentType: equipmentType,
            equipmentTypeName: equipmentTypes[equipmentType],
            requestedOrdinal: requestedOrdinal,
            renderedOrdinal: selection.outOfRangeFallbackChild
          });
        }
      }
    }
    context.equipmentGroupCache[groupId] = group;
    return group;
  }

  function parseCorpusSequence(context, corpusSpec, parseOptions) {
    parseOptions = parseOptions || {};
    var className = OB64.CLASS_NAMES && OB64.CLASS_NAMES[corpusSpec.classId]
      ? OB64.CLASS_NAMES[corpusSpec.classId]
      : cleanCorpusName(corpusSpec.className);
    var actionName = cleanCorpusName(corpusSpec.actionName);
    var spec = Object.assign({}, corpusSpec, {
      className: className,
      actionName: actionName,
      consumerSummary: cleanCorpusName(corpusSpec.consumerSummary),
      selectedChildOrdinal: corpusSpec.selectedBodyChild
    });
    var label = className + ' ' + actionName;
    var descriptor = corpusDescriptor(context, spec);
    var config = corpusConfig(context, spec);
    var lookup = corpusLookup(context, spec);
    var pose = corpusPose(context, spec, label, !!parseOptions.dynamicFrames);
    if (parseOptions.dynamicFrames) {
      spec.frames = pose.program.frames.map(function(frame) {
        return [frame[0], frame[1]];
      });
    }
    var artById = {}, artByKey = {}, frames = [];
    var originX = 0, originY = 0, endX = 0, endY = 0, hasBounds = false;

    spec.frames.forEach(function(frameSpec, sequenceIndex) {
      var parsed = corpusMetadataFrame(context, spec.metadataKey, frameSpec[0]);
      var layers = parsed.layers.map(function(parsedLayer) {
        if (parsedLayer.artId >= descriptor.members.length - 4) {
          fail(label + ' frame ' + (sequenceIndex + 1) + ' selects art ' +
            parsedLayer.artId + ' outside its descriptor');
        }
        var source = corpusSource(context, spec, descriptor, config, lookup,
          parsedLayer.artId, label);
        var requestedChild = source.sourceRole === 'body'
          ? spec.selectedBodyChild : 0;
        var selectedChild = childOrdinalOrFallback(source, requestedChild);
        if (source.sourceRole === 'element-effect' && selectedChild !== 0) {
          fail(source.key + ' element-effect binding must use physical child zero');
        }
        if (source.sprite.height !== parsedLayer.height ||
            (source.formatKind === 'indexed-ci8' &&
              source.sprite.width !== parsedLayer.width) ||
            (source.formatKind === 'direct-rgba5551' &&
              (parsedLayer.width > source.sprite.width ||
                source.sprite.width - parsedLayer.width > 3))) {
          fail(label + ' frame ' + (sequenceIndex + 1) + ' art ' +
            parsedLayer.artId + ' dimensions differ from its accepted 0x5554 source');
        }
        addSourceUsage(source, spec, label, sequenceIndex, selectedChild,
          parseOptions.trackRouteUsage);
        artById[parsedLayer.artId] = source;
        artByKey[source.key] = source;
        var left = parsedLayer.drawOffsetX, top = parsedLayer.drawOffsetY;
        var right = left + parsedLayer.width, bottom = top + parsedLayer.height;
        if (!hasBounds) {
          originX = left; originY = top; endX = right; endY = bottom; hasBounds = true;
        } else {
          originX = Math.min(originX, left); originY = Math.min(originY, top);
          endX = Math.max(endX, right); endY = Math.max(endY, bottom);
        }
        return Object.assign({}, parsedLayer, {
          sourceKey: source.key,
          bindingId: source.bindingId,
          physicalSourceId: source.physicalSourceId,
          sourceRole: source.sourceRole,
          resourceKey: source.resourceKey,
          lookupBank: source.lookupBank,
          childCount: source.sprite.childCount,
          requestedChildOrdinal: requestedChild,
          selectedChildOrdinal: selectedChild
        });
      });
      frames.push({
        sequenceIndex: sequenceIndex,
        token: frameSpec[0],
        ticks: frameSpec[1],
        metadataTarget: parsed.target,
        layers: layers
      });
    });
    if (!hasBounds) fail(label + ' has no drawable layers');
    var canvas = {
      originX: originX, originY: originY, endX: endX, endY: endY,
      width: endX - originX, height: endY - originY
    };
    if (parseOptions.dynamicFrames) spec.canvas = canvas;
    else compareCanvas(canvas, spec.canvas, label);
    var equipmentGroup = null;
    if (spec.equipmentGroupIds.length > 1) {
      fail(label + ' unexpectedly selects more than one equipment group');
    }
    if (spec.equipmentGroupIds.length) {
      equipmentGroup = corpusEquipmentGroup(context, spec.equipmentGroupIds[0]);
      if (!equipmentGroup || equipmentGroup.descriptorKey !== spec.descriptorKey ||
          equipmentGroup.expectedChildCount !== spec.weaponChildCount) {
        fail(label + ' equipment group does not match its descriptor or child count');
      }
    }
    return {
      key: spec.key,
      corpusId: spec.id,
      compatibilityKey: spec.compatibilityKey,
      spec: spec,
      descriptor: descriptor.resource,
      members: descriptor.members,
      metadata: cachedCompressed(context, spec.metadataKey).decoded,
      pose: pose.resource.decoded,
      poseProgram: pose.program,
      config: config,
      lookupBanks: lookup.banks,
      frames: frames,
      artById: artById,
      artByKey: artByKey,
      equipmentGroup: equipmentGroup,
      canvas: canvas
    };
  }

  function selectorFlagLabel(spec) {
    var match = String(spec.variantLabel || '').match(/flags\s+(\d)\/(\d)/);
    return match ? match[1] + '/' + match[2] : null;
  }

  function equipmentGroupIdsForDescriptor(descriptorKey) {
    return Object.keys(CORPUS.equipmentGroups).filter(function(groupId) {
      return CORPUS.equipmentGroups[groupId].descriptorKey === descriptorKey;
    });
  }

  function mappedWeaponOrdinals(group) {
    return group ? group.children.filter(function(child) {
      return child.mappedItems && child.mappedItems.length;
    }).map(function(child) { return child.ordinal; }) : [];
  }

  function buildClassArtRouteTemplates(state, context) {
    var handles = A.readResource(context.z64, CLASS_HANDLE_RESOURCE_KEY);
    var expectedLength = CLASS_HANDLE_TABLE_OFFSET + CLASS_HANDLE_COUNT * 2;
    if (handles.storedLength < expectedLength) {
      fail('class combat-art handle resource has ' + handles.storedLength +
        ' bytes; expected at least ' + expectedLength);
    }
    var descriptorRoot = A.readResource(
      context.z64, COMBAT_DESCRIPTOR_TABLE_KEY);
    if (descriptorRoot.storedLength !== COMBAT_DESCRIPTOR_COUNT * 4) {
      fail('combat descriptor table has ' +
        (descriptorRoot.storedLength / 4) + ' entries; expected ' +
        COMBAT_DESCRIPTOR_COUNT);
    }
    var byClass = {}, dynamic = [], failures = [];
    state.specs.slice().sort(function(left, right) {
      return left.spec.rawMode - right.spec.rawMode ||
        left.spec.displayOrder - right.spec.displayOrder;
    }).forEach(function(animation) {
      var classId = Number(animation.spec.classId);
      var flags = selectorFlagLabel(animation.spec);
      if (!flags) return;
      if (!byClass[classId]) byClass[classId] = {};
      if (!byClass[classId][flags]) byClass[classId][flags] = animation;
    });

    Object.keys(OB64.CLASS_NAMES || {}).map(Number).filter(function(classId) {
      return classId > 0 && classId * 4 + 3 < CLASS_HANDLE_COUNT;
    }).sort(function(left, right) { return left - right; })
      .forEach(function(classId) {
        if (!byClass[classId]) byClass[classId] = {};
        for (var bodyFlags = 0; bodyFlags < 4; bodyFlags++) {
          var flagA = Math.floor(bodyFlags / 2);
          var flagB = bodyFlags & 1;
          var flagLabel = flagA + '/' + flagB;
          if (byClass[classId][flagLabel]) continue;
          var handleIndex = classId * 4 + bodyFlags;
          var rawHandle = readU16(handles.stored,
            CLASS_HANDLE_TABLE_OFFSET + handleIndex * 2);
          var descriptorSlot = (rawHandle & 0x0FFF) - 1;
          try {
            if (descriptorSlot < 0 || descriptorSlot >= COMBAT_DESCRIPTOR_COUNT) {
              fail('class ' + hex(classId, 2) + ' flags ' + flagLabel +
                ' has invalid descriptor handle ' + hex(rawHandle, 4));
            }
            var descriptorKey = readU32(
              descriptorRoot.stored, descriptorSlot * 4);
            if (!descriptorKey) {
              fail('class ' + hex(classId, 2) + ' flags ' + flagLabel +
                ' selects an empty descriptor slot');
            }
            var descriptor = A.readResource(context.z64, descriptorKey);
            if (descriptor.storedLength < 16 || descriptor.storedLength % 4) {
              fail('class ' + hex(classId, 2) + ' flags ' + flagLabel +
                ' descriptor is not a raw u32 member list');
            }
            var descriptorMemberCount = descriptor.storedLength / 4;
            var controls = [0, 4, 8, 12].map(function(offset) {
              return readU32(descriptor.stored, offset);
            });
            if (controls.some(function(key) { return !key; })) {
              fail('class ' + hex(classId, 2) + ' flags ' + flagLabel +
                ' descriptor lacks a control resource');
            }
            var groupIds = equipmentGroupIdsForDescriptor(descriptorKey);
            if (groupIds.length > 1) {
              fail('descriptor ' + hex(descriptorKey) +
                ' has more than one accepted equipment group');
            }
            var group = groupIds.length ? CORPUS.equipmentGroups[groupIds[0]] : null;
            var key = 'class-art-route-' +
              classId.toString(16).padStart(3, '0') + '-flags-' +
              flagA + '-' + flagB;
            var className = OB64.CLASS_NAMES[classId] ||
              ('Class ' + hex(classId, 2));
            var spec = {
              key: key,
              id: key,
              compatibilityKey: null,
              classId: classId,
              className: className,
              actionId: -1,
              actionName: 'Idle / Rest',
              rawMode: 0,
              modeLabel: 'Idle loop source',
              selector: 0,
              descriptorKey: descriptorKey,
              descriptorMemberCount: descriptorMemberCount,
              metadataKey: controls[0],
              poseKey: controls[1],
              configKey: controls[2],
              lookupKey: controls[3],
              poseDecodedLength: cachedCompressed(
                context, controls[1]).decoded.length,
              displayOrder: 100000 + handleIndex,
              variantLabel: 'flags ' + flagLabel + ' · class-' +
                classId.toString(16).padStart(3, '0').toUpperCase() +
                '-table-flags-' + flagA + '-' + flagB,
              selectedBodyChild: rawHandle >>> 12,
              selectedChildOrdinal: rawHandle >>> 12,
              weaponChildCount: group ? group.expectedChildCount : 0,
              equipmentGroupIds: groupIds,
              retailMappedWeaponOrdinals: mappedWeaponOrdinals(group),
              consumerSummary: className + ' class combat-art handle table',
              frames: [],
              canvas: null,
              frozenParity: null,
              artRouteTemplate: true,
              route: {
                actorFields: {
                  flagA: flagA,
                  flagB: flagB,
                  physicalClassRecord: classId + 1,
                  rawOwnerContext: classId,
                  sourceArtId: classId
                },
                defaultDescriptorHandle: rawHandle,
                rawHandleU16: rawHandle,
                variantId: 'class-' +
                  classId.toString(16).padStart(3, '0').toUpperCase() +
                  '-table-flags-' + flagA + '-' + flagB
              }
            };
            var parsed = parseCorpusSequence(context, spec, {
              dynamicFrames: true,
              trackRouteUsage: false
            });
            parsed.artRouteHandleIndex = handleIndex;
            byClass[classId][flagLabel] = parsed;
            dynamic.push(parsed);
            Object.keys(parsed.artByKey).forEach(function(sourceKey) {
              if (!state.artByKey[sourceKey]) {
                state.artByKey[sourceKey] = parsed.artByKey[sourceKey];
              }
            });
          } catch (error) {
            failures.push({
              classId: classId,
              flags: flagLabel,
              handleIndex: handleIndex,
              rawHandleU16: rawHandle,
              message: error && error.message ? error.message : String(error)
            });
          }
        }
      });
    state.artRouteTemplatesByClass = byClass;
    state.artRouteTemplates = Object.keys(byClass).map(Number)
      .sort(function(left, right) { return left - right; })
      .reduce(function(rows, classId) {
        ['0/0', '0/1', '1/0', '1/1'].forEach(function(flags) {
          if (byClass[classId][flags]) rows.push(byClass[classId][flags]);
        });
        return rows;
      }, []);
    state.dynamicArtRouteTemplates = dynamic;
    state.artRouteFailures = failures;
  }

  function selectorCandidateIndexKey(classId, flags, rawMode, selector) {
    return [classId, flags, rawMode, selector].join(':');
  }

  function childHasVisiblePixels(source, childOrdinal) {
    if (!source.visibleChildren) source.visibleChildren = {};
    if (Object.prototype.hasOwnProperty.call(source.visibleChildren, childOrdinal)) {
      return source.visibleChildren[childOrdinal];
    }
    var child = ensureOriginalChild(source, childOrdinal);
    var values = source.editable ? child.intensity : child.alpha;
    var visible = false;
    for (var index = 0; index < values.length; index++) {
      if (values[index]) { visible = true; break; }
    }
    source.visibleChildren[childOrdinal] = visible;
    return visible;
  }

  function analyzeMappings(state) {
    var expectedFlags = ['0/0', '0/1', '1/0', '1/1'];
    var byClass = {};
    var soldierDescriptors = {};
    var routeRows = state.artRouteTemplates && state.artRouteTemplates.length
      ? state.artRouteTemplates : state.specs;
    routeRows.forEach(function(animation) {
      var spec = animation.spec;
      var flagLabel = selectorFlagLabel(spec);
      if (!byClass[spec.classId]) {
        byClass[spec.classId] = {
          classId: spec.classId,
          className: spec.className,
          presentFlags: {},
          missingFlags: []
        };
      }
      if (flagLabel) byClass[spec.classId].presentFlags[flagLabel] = true;
      if (spec.classId === 0x01) {
        soldierDescriptors[spec.descriptorKey + ':' + spec.selectedBodyChild] = true;
      }
    });
    Object.keys(byClass).forEach(function(classId) {
      var row = byClass[classId];
      row.missingFlags = expectedFlags.filter(function(flagLabel) {
        return !row.presentFlags[flagLabel];
      });
    });

    var counts = {
      mapped: 0,
      sharedSpecial: 0,
      dedicatedSpecial: 0,
      soldierAlias: 0,
      visibleFailure: 0,
      missingVariantRows: Object.keys(byClass).reduce(function(total, classId) {
        return total + byClass[classId].missingFlags.length;
      }, 0)
    };
    function classifyAnimation(animation, countStatus) {
      var spec = animation.spec;
      var emptyFrames = [];
      var emptySources = {};
      var transformedSources = {};
      animation.frames.forEach(function(frame) {
        var bodyLayers = 0, visibleBodyLayers = 0;
        frame.layers.forEach(function(layer) {
          var source = animation.artByKey[layer.sourceKey];
          if (source.sourceRole !== 'body') return;
          bodyLayers++;
          var childOrdinal = layer.selectedChildOrdinal;
          var identity = source.key + '#child-' + childOrdinal;
          var child = source.sprite.children[childOrdinal];
          if (child && child.discriminator !== child.ordinal) {
            transformedSources[identity] = true;
          }
          if (childHasVisiblePixels(source, childOrdinal)) {
            visibleBodyLayers++;
          } else {
            emptySources[identity] = true;
          }
        });
        if (!bodyLayers || !visibleBodyLayers) emptyFrames.push(frame.sequenceIndex);
      });

      var isSpecial = spec.classId >= 0x87;
      var soldierAlias = spec.classId !== 0x01 &&
        soldierDescriptors[spec.descriptorKey + ':' + spec.selectedBodyChild];
      var baseName = String(spec.className || '').replace(/\s+\([^)]*\)\s*$/, '');
      var ordinaryMatch = isSpecial && !soldierAlias
        ? state.specs.find(function(candidate) {
          return candidate.spec.classId < 0x87 &&
            candidate.spec.descriptorKey === spec.descriptorKey &&
            candidate.spec.selectedBodyChild === spec.selectedBodyChild &&
            candidate.spec.actionId === spec.actionId &&
            candidate.spec.className === baseName;
        }) || state.specs.find(function(candidate) {
          return candidate.spec.classId < 0x87 &&
            candidate.spec.descriptorKey === spec.descriptorKey &&
            candidate.spec.selectedBodyChild === spec.selectedBodyChild &&
            candidate.spec.actionId === spec.actionId;
        }) : null;
      var status;
      if (soldierAlias) {
        status = {
          state: 'soldier-alias',
          severity: 'alias',
          artClassId: 0x01,
          artClassName: 'Soldier',
          title: 'Soldier descriptor alias',
          detail: 'This ROM selector row points to the Soldier animation corpus. ' +
            'The alias remains visible and is not treated as missing art.'
        };
        if (countStatus) counts.soldierAlias++;
      } else if (emptyFrames.length) {
        status = {
          state: 'visible-failure',
          severity: 'failure',
          artClassId: ordinaryMatch
            ? ordinaryMatch.spec.classId : spec.classId,
          artClassName: ordinaryMatch
            ? ordinaryMatch.spec.className : spec.className,
          title: 'Visible mapping failure',
          detail: emptyFrames.length + ' of ' + animation.frames.length +
            ' frames contain no visible body pixels after child-delta reconstruction. ' +
            'The sequence and every layer remain available for inspection.'
        };
        if (countStatus) counts.visibleFailure++;
      } else if (ordinaryMatch) {
        status = {
          state: 'shared-special',
          severity: 'shared',
          artClassId: ordinaryMatch.spec.classId,
          artClassName: ordinaryMatch.spec.className,
          title: 'Shared ' + ordinaryMatch.spec.className + ' mapping',
          detail: 'This special-class row resolves to the same descriptor, action, and body child as ' +
            ordinaryMatch.spec.className + '. The ROM mapping is preserved.'
        };
        if (countStatus) counts.sharedSpecial++;
      } else if (isSpecial) {
        status = {
          state: 'dedicated-special',
          severity: 'mapped',
          artClassId: spec.classId,
          artClassName: spec.className,
          title: 'Dedicated special-class mapping',
          detail: 'This selector row resolves to a dedicated descriptor and body-child mapping.'
        };
        if (countStatus) counts.dedicatedSpecial++;
      } else {
        status = {
          state: 'mapped',
          severity: 'mapped',
          artClassId: spec.classId,
          artClassName: spec.className,
          title: 'ROM mapping resolved',
          detail: 'The descriptor, frames, layers, and selected body children resolve from the loaded ROM.'
        };
        if (countStatus) counts.mapped++;
      }
      status.selectorFlags = selectorFlagLabel(spec);
      status.emptyFrameIndices = emptyFrames;
      status.emptySourceCount = Object.keys(emptySources).length;
      status.transformedSourceCount = Object.keys(transformedSources).length;
      if (status.emptySourceCount && !emptyFrames.length) {
        status.detail += ' ' + status.emptySourceCount +
          ' selected source record' + (status.emptySourceCount === 1 ? ' is' : 's are') +
          ' empty and remain visible in the layer list.';
      }
      if (status.transformedSourceCount) {
        status.detail += ' ' + status.transformedSourceCount +
          ' source child' + (status.transformedSourceCount === 1 ? ' is' : 'ren are') +
          ' reconstructed from the ROM delta relationship.';
      }
      animation.mappingStatus = status;
    }
    state.specs.forEach(function(animation) {
      classifyAnimation(animation, true);
    });
    (state.dynamicArtRouteTemplates || []).forEach(function(animation) {
      classifyAnimation(animation, false);
    });
    state.mappingAudit = { byClass: byClass, counts: counts };
  }

  function initialize(z64) {
    var state = {
      supported: false, unavailableReason: '', specs: [], byKey: {}, artByKey: {},
      sourceAliases: {}, selectorCandidates: {}, edits: {}, history: {}, editRevision: 0,
      blocked: {}, corpus: null
    };
    try {
      var index = validateCorpus();
      var context = corpusContext(z64, index);
      SPECS.forEach(function(spec) {
        var parsed = parseCorpusSequence(context, spec);
        state.specs.push(parsed); state.byKey[parsed.key] = parsed;
        if (parsed.compatibilityKey) state.byKey[parsed.compatibilityKey] = parsed;
        Object.keys(parsed.artByKey).forEach(function(key) {
          if (!state.artByKey[key]) state.artByKey[key] = parsed.artByKey[key];
        });
        if (parsed.compatibilityKey) {
          Object.keys(parsed.artById).forEach(function(artId) {
            var source = parsed.artById[artId];
            var legacyKey = parsed.compatibilityKey + ':' + artId;
            state.sourceAliases[legacyKey] = source.key;
            if (source.legacyKeys.indexOf(legacyKey) < 0) source.legacyKeys.push(legacyKey);
            if (source.legacyAnimationKeys.indexOf(parsed.compatibilityKey) < 0) {
              source.legacyAnimationKeys.push(parsed.compatibilityKey);
            }
          });
        }
      });
      var acceptedAttackUsedBindingCount = Object.keys(state.artByKey).length;
      buildClassArtRouteTemplates(state, context);
      analyzeMappings(state);
      var references = descriptorReferenceCounts(z64);
      Object.keys(state.artByKey).forEach(function(key) {
        var source = state.artByKey[key];
        source.descriptorReferenceCount = references[source.resourceKey] || 0;
        if (!source.descriptorReferenceCount) {
          fail(source.key + ' is not referenced by the combat descriptor corpus');
        }
        source.inPlaceEligible = source.descriptorReferenceCount === 1;
      });
      state.corpus = {
        version: CORPUS.corpusVersion,
        projectionSha256: CORPUS.sourceProjectionSha256,
        sequenceCount: state.specs.length,
        physicalSourceCount: Object.keys(CORPUS.physicalSources).length,
        bindingCount: index.bindingCount,
        usedBindingCount: acceptedAttackUsedBindingCount,
        dormantBindingCount: index.bindingCount - acceptedAttackUsedBindingCount,
        equipmentGroupCount: Object.keys(CORPUS.equipmentGroups).length
      };
      var canonicalSelectorIndex = {};
      var templateByDescriptor = {};
      state.specs.concat(state.dynamicArtRouteTemplates || [])
        .forEach(function(animation) {
          if (!templateByDescriptor[animation.spec.descriptorKey]) {
            templateByDescriptor[animation.spec.descriptorKey] = animation;
          }
          canonicalSelectorIndex[selectorCandidateIndexKey(
            animation.spec.classId, selectorFlagLabel(animation.spec),
            animation.spec.rawMode, animation.spec.selector)] = animation;
        });
      state.resolveBindingSource = function(bindingId, childOrdinal) {
        if (state.artByKey[bindingId]) {
          var existingSource = state.artByKey[bindingId];
          if (!Number.isInteger(childOrdinal) || childOrdinal < 0 ||
              childOrdinal >= existingSource.sprite.childCount) {
            fail('binding ' + bindingId + ' child ordinal is outside its source');
          }
          addSourceUsage(existingSource, { key: existingSource.animationKey },
            existingSource.animationLabel, 0, childOrdinal, false);
          return existingSource;
        }
        var binding = index.bindingById[bindingId];
        if (!binding) return null;
        var templateAnimation = templateByDescriptor[binding.descriptorKey];
        if (!templateAnimation) {
          fail('binding ' + bindingId + ' has no accepted descriptor template');
        }
        var template = templateAnimation.spec;
        var descriptor = corpusDescriptor(context, template);
        var config = corpusConfig(context, template);
        var lookup = corpusLookup(context, template);
        var source = corpusSource(context, template, descriptor, config, lookup,
          binding.artId, cleanCorpusName(template.className) + ' dormant binding');
        if (!Number.isInteger(childOrdinal) || childOrdinal < 0 ||
            childOrdinal >= source.sprite.childCount) {
          fail('binding ' + bindingId + ' child ordinal is outside its source');
        }
        addSourceUsage(source, template,
          cleanCorpusName(template.className) + ' dormant binding', 0,
          childOrdinal, false);
        source.descriptorReferenceCount = references[source.resourceKey] || 0;
        if (!source.descriptorReferenceCount) {
          fail(source.key + ' is not referenced by the combat descriptor corpus');
        }
        source.inPlaceEligible = source.descriptorReferenceCount === 1;
        source.onDemandBinding = true;
        state.artByKey[source.key] = source;
        return source;
      };
      state.resolveSelectorCandidate = function(templateAnimation, selector, rawMode) {
        if (!templateAnimation || !templateAnimation.spec) {
          fail('selector candidate requires a mapped class-art template');
        }
        selector = Number(selector); rawMode = Number(rawMode);
        if (!Number.isInteger(selector) || selector < 0 || selector > 255) {
          fail('selector candidate must use a u8 selector');
        }
        if (rawMode !== 0 && rawMode !== 1 && rawMode !== 2) {
          fail('selector candidate raw mode must be 0, 1, or 2');
        }
        var template = templateAnimation.spec;
        var flags = selectorFlagLabel(template);
        var identity = selectorCandidateIndexKey(template.classId, flags,
          rawMode, selector);
        if (canonicalSelectorIndex[identity]) return canonicalSelectorIndex[identity];
        if (state.selectorCandidates[identity]) return state.selectorCandidates[identity];
        var groupIds = equipmentGroupIdsForDescriptor(template.descriptorKey);
        if (groupIds.length > 1) {
          fail('descriptor ' + hex(template.descriptorKey) +
            ' has more than one accepted equipment group');
        }
        var group = groupIds.length ? CORPUS.equipmentGroups[groupIds[0]] : null;
        var candidateKey = 'selector-candidate-' +
          Number(template.classId).toString(16).padStart(3, '0') + '-' +
          String(flags || 'unknown').replace('/', '-') + '-mode-' + rawMode +
          '-selector-' + selector.toString(16).padStart(2, '0');
        var candidateSpec = Object.assign({}, template, {
          key: candidateKey,
          id: candidateKey,
          compatibilityKey: null,
          rawMode: rawMode,
          modeLabel: 'Raw mode ' + rawMode,
          selector: selector,
          frames: [],
          canvas: null,
          frozenParity: null,
          equipmentGroupIds: groupIds,
          weaponChildCount: group ? group.expectedChildCount : 0,
          retailMappedWeaponOrdinals: mappedWeaponOrdinals(group),
          dynamicSelectorCandidate: true
        });
        var parsed = parseCorpusSequence(context, candidateSpec, {
          dynamicFrames: true,
          trackRouteUsage: false
        });
        var usesEquipment = Object.keys(parsed.artByKey).some(function(key) {
          return parsed.artByKey[key].weaponSelectable;
        });
        if (!usesEquipment) {
          parsed.equipmentGroup = null;
          parsed.spec.equipmentGroupIds = [];
          parsed.spec.weaponChildCount = 0;
          parsed.spec.retailMappedWeaponOrdinals = [];
        }
        parsed.mappingStatus = Object.assign({}, templateAnimation.mappingStatus || {}, {
          selectorFlags: flags
        });
        Object.keys(parsed.artByKey).forEach(function(key) {
          var source = parsed.artByKey[key];
          if (!source.descriptorReferenceCount) {
            source.descriptorReferenceCount = references[source.resourceKey] || 0;
            if (!source.descriptorReferenceCount) {
              fail(source.key + ' is not referenced by the combat descriptor corpus');
            }
            source.inPlaceEligible = source.descriptorReferenceCount === 1;
          }
          if (!state.artByKey[key]) {
            var stableTemplate = templateByDescriptor[source.descriptorKey];
            if (stableTemplate) {
              source.animationKey = stableTemplate.key;
              source.animationLabel = stableTemplate.spec.className + ' ' +
                stableTemplate.spec.actionName;
            }
            source.onDemandBinding = true;
            state.artByKey[key] = source;
          }
        });
        state.selectorCandidates[identity] = parsed;
        return parsed;
      };
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
    if (!source.editable || !Number.isInteger(childOrdinal) ||
        source.editableChildOrdinals.indexOf(childOrdinal) < 0) {
      fail(source.key + ' child ' + childOrdinal + ' is not editable');
    }
    return ensureOriginalChild(source, childOrdinal);
  }

  function displayChild(state, key, childOrdinal) {
    var source = state.artByKey[key];
    if (!source) fail('unknown combat-sprite source ' + key);
    childOrdinal = childOrdinalOrFallback(source,
      childOrdinal === undefined ? source.childOrdinal : childOrdinal);
    return source.editable
      ? currentEdit(state, key, childOrdinal)
      : ensureOriginalChild(source, childOrdinal);
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
    state.editRevision = (Number(state.editRevision) || 0) + 1;
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
      if (source && source.separationId) return;
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
        bindingId: source.bindingId,
        physicalSourceId: source.physicalSourceId,
        descriptorKey: hex(source.descriptorKey),
        descriptorMemberOrdinal: source.descriptorMemberIndex,
        sourceRole: source.sourceRole,
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
      fail('This ROM cannot load combat-sprite Project records');
    }
    Object.keys(payload).forEach(function(key) {
      var entry = payload[key];
      var requestedChildren = entry && entry.children;
      var requestedOrdinals = entry && Number.isInteger(entry.childOrdinal)
        ? [entry.childOrdinal] : [];
      if (!requestedOrdinals.length && requestedChildren &&
          typeof requestedChildren === 'object' &&
          !Array.isArray(requestedChildren)) {
        Object.keys(requestedChildren).forEach(function(requestedKey) {
          if (/^(0|[1-9][0-9]*)$/.test(requestedKey)) {
            requestedOrdinals.push(Number(requestedKey));
          }
        });
      }
      var canonicalKey = state.artByKey[key]
        ? key : state.sourceAliases[key];
      if (!canonicalKey && entry && typeof state.resolveBindingSource === 'function') {
        requestedOrdinals.forEach(function(requestedChild) {
          var resolvedSource = state.resolveBindingSource(entry.bindingId || key,
            requestedChild);
          if (resolvedSource) canonicalKey = resolvedSource.key;
        });
      }
      var source = canonicalKey && state.artByKey[canonicalKey];
      if (source && source.onDemandBinding &&
          typeof state.resolveBindingSource === 'function') {
        requestedOrdinals.forEach(function(requestedChild) {
          state.resolveBindingSource(source.bindingId, requestedChild);
        });
      }
      var animationMatches = source && entry &&
        (entry.animation === source.animationKey ||
          source.animationKeys.indexOf(entry.animation) >= 0 ||
          source.legacyAnimationKeys.indexOf(entry.animation) >= 0);
      var newIdentityMatches = source && entry &&
        (!entry.bindingId || entry.bindingId === source.bindingId) &&
        (!entry.physicalSourceId ||
          entry.physicalSourceId === source.physicalSourceId) &&
        (!entry.descriptorKey || entry.descriptorKey === hex(source.descriptorKey)) &&
        (entry.descriptorMemberOrdinal === undefined ||
          entry.descriptorMemberOrdinal === source.descriptorMemberIndex) &&
        (!entry.sourceRole || entry.sourceRole === source.sourceRole);
      if (!source || !entry || !animationMatches || !newIdentityMatches ||
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
      var group = prepared[canonicalKey] || { children: {} };
      Object.keys(childPayloads).forEach(function(childKeyText) {
        if (!/^(0|[1-9][0-9]*)$/.test(childKeyText)) {
          fail(key + ' child key ' + childKeyText + ' is not a canonical ordinal');
        }
        var childOrdinal = Number(childKeyText), childEntry = childPayloads[childKeyText];
        originalChild(source, childOrdinal);
        if (group.children[childOrdinal]) {
          fail(key + ' repeats binding ' + source.bindingId + ' child ' + childOrdinal);
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
      prepared[canonicalKey] = group;
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

  function cloneMaterializedLanes(sprite, childOrdinal) {
    var lanes = materializeChildLanes(sprite, childOrdinal);
    return {
      first: lanes.first ? lanes.first.slice() : null,
      second: lanes.second ? lanes.second.slice() : null,
      lookup: lanes.lookup ? lanes.lookup.slice() : null
    };
  }

  function applyVisibleEdit(sprite, target, edit) {
    var pixel = 0;
    for (var y = 0; y < sprite.height; y++) {
      var firstRow = y * sprite.firstStride;
      var secondRow = y * sprite.secondStride;
      for (var x = 0; x < sprite.width; x++, pixel++) {
        target.first[firstRow + x] = edit.indices[pixel];
        var packedOffset = secondRow + (x >>> 1);
        if (x & 1) {
          target.second[packedOffset] = (target.second[packedOffset] & 0xF0) |
            edit.intensity[pixel];
        } else {
          target.second[packedOffset] = (target.second[packedOffset] & 0x0F) |
            (edit.intensity[pixel] << 4);
        }
      }
    }
  }

  function encodedLane(sprite, child, target, base, lane, format) {
    if (!target[lane]) return null;
    if (child.discriminator === child.ordinal || child.flags & 0x08) {
      return target[lane];
    }
    return combineDeltaLane(base && base[lane], target[lane], format,
      lane === 'lookup' ? 'embedded lookup' : lane + ' lane',
      sprite.resourceKey, child.ordinal);
  }

  function buildDecoded(source, childEdits) {
    var sprite = source.sprite, decoded = sprite.decoded.slice();
    var targets = sprite.children.map(function(child) {
      return cloneMaterializedLanes(sprite, child.ordinal);
    });
    Object.keys(childEdits || {}).forEach(function(childOrdinalText) {
      var childOrdinal = Number(childOrdinalText), edit = childEdits[childOrdinalText];
      originalChild(source, childOrdinal);
      validatePixels(source, edit.indices, edit.intensity);
      applyVisibleEdit(sprite, targets[childOrdinal], edit);
    });
    sprite.children.forEach(function(child) {
      var target = targets[child.ordinal];
      var base = child.discriminator === child.ordinal
        ? null : targets[child.discriminator];
      var first = encodedLane(sprite, child, target, base, 'first', sprite.firstFormat);
      var second = encodedLane(sprite, child, target, base, 'second', sprite.secondFormat);
      var lookup = encodedLane(sprite, child, target, base, 'lookup', 2);
      if (first) decoded.set(first, child.firstOffset);
      if (second) decoded.set(second, child.secondOffset);
      if (lookup) decoded.set(lookup, child.lookupOffset);
    });
    var reparsed = parseSpriteObject(decoded, source.resourceKey);
    reparsed.children.forEach(function(child) {
      var rebuilt = materializeChildLanes(reparsed, child.ordinal);
      var target = targets[child.ordinal];
      if (!equalOptionalBytes(rebuilt.first, target.first) ||
          !equalOptionalBytes(rebuilt.second, target.second) ||
          !equalOptionalBytes(rebuilt.lookup, target.lookup)) {
        fail(source.key + ' child ' + child.ordinal +
          ' runtime-materialized readback differs after delta rebuild');
      }
    });
    return decoded;
  }

  function buildResources(state) {
    var resources = [];
    if (!state || !state.supported) return resources;
    Object.keys(state.edits).sort().forEach(function(key, ordinal) {
      var source = state.artByKey[key], edit = state.edits[key];
      if (source && source.separationId) return;
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

  async function buildResourcesAsync(state, onProgress) {
    var resources = [];
    if (!state || !state.supported) return resources;
    var keys = Object.keys(state.edits).sort();
    var buildKeys = keys.filter(function(key) {
      var source = state.artByKey[key];
      return !(source && source.separationId);
    });
    for (var buildIndex = 0; buildIndex < buildKeys.length; buildIndex++) {
      var key = buildKeys[buildIndex];
      var source = state.artByKey[key];
      var edit = state.edits[key];
      var ordinal = keys.indexOf(key);
      var decoded = buildDecoded(source, edit.children);
      var label = 'combat sprite ' + (buildIndex + 1) + ' of ' + buildKeys.length;
      var stored = await A.bootLzCompressAsync(decoded, function(fraction) {
        if (onProgress) {
          onProgress(label, (buildIndex + fraction) /
            Math.max(1, buildKeys.length));
        }
      });
      var verified = A.bootLzDecode(stored);
      if (verified.bytesConsumed !== stored.length ||
          !equalBytes(verified.output, decoded)) {
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
    }
    if (onProgress) onProgress('combat sprites complete', 1);
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
      if (state.artByKey[key] && state.artByKey[key].separationId) return;
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
    state.editRevision = (Number(state.editRevision) || 0) + 1;
  }

  function resetAll(state) {
    if (!state) return;
    state.edits = {}; state.blocked = {};
    state.editRevision = (Number(state.editRevision) || 0) + 1;
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
    weaponItemFamilies: WEAPON_ITEM_FAMILIES,
    AnimationArtError: AnimationArtError,
    rowBytes: rowBytes,
    parseSpriteObject: parseSpriteObject,
    parseMetadataFrame: parseMetadataFrame,
    parsePoseProgram: parsePoseProgram,
    descriptorReferenceCounts: descriptorReferenceCounts,
    decodeChild: decodeChild,
    decodeDirectChild: decodeDirectChild,
    materializeChildLanes: materializeChildLanes,
    childOrdinalOrFallback: childOrdinalOrFallback,
    childHasVisiblePixels: childHasVisiblePixels,
    childPalette: childPalette,
    displayChild: displayChild,
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
    buildResourcesAsync: buildResourcesAsync,
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
