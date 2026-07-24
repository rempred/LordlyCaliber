// OB64 Mod Editor - evidence-backed consumable effect ranges.
//
// The only source identity authorized by this module is the verified US retail
// rev0 .v64 named below. All ROM offsets are normalized z64 offsets. The guard
// constants and immutable context hashes were read from that source without
// mutation (SHA-256 6CA0A1AF...A07B12).
(function(root, factory) {
  var namespace = root && root.OB64 ? root.OB64 : {};
  var api = factory(namespace);
  if (typeof module === 'object' && module.exports) {
    module.exports = api;
    return;
  }
  if (root) {
    root.OB64 = root.OB64 || {};
    root.OB64.consumableEffects = api;
  }
})(typeof window !== 'undefined' ? window : globalThis, function(OB64) {
  'use strict';

  var PROJECT_VERSION = 12;
  var EDITOR_VERSION = '2026-07-24';
  var CATALOG_MAX_ID = 31;
  var SOURCE_DESCRIPTOR = Object.freeze({
    filename: 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64',
    size: 41943040,
    sha256: '6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12',
    byteOrder: 'v64',
    imageName: 'OgreBattle64',
    gameId: 'NOBE',
    country: 0x45,
    version: 0x00,
    crc1: 0xE6419BC5,
    crc2: 0x69011DE3
  });

  var MODEL_ORDER = [
    'cupOfLife',
    'sharedStatBoosters11To16',
    'scrollOfDiscipline',
    'urnOfChaos',
    'gobletOfDestiny'
  ];

  var MODEL_DEFS = Object.freeze({
    cupOfLife: Object.freeze({
      projectKey: '10',
      itemIds: Object.freeze([10]),
      widthOffset: 0x4134C,
      minimumOffset: 0x41388,
      vanillaMin: 5,
      vanillaMax: 10,
      domainMin: 0,
      domainMax: 999,
      target: 'Maximum HP (C+0x16)',
      retail: '+5..+10'
    }),
    sharedStatBoosters11To16: Object.freeze({
      projectKey: '11-16',
      itemIds: Object.freeze([11, 12, 13, 14, 15, 16]),
      widthOffset: 0x41444,
      minimumOffset: 0x41480,
      vanillaMin: 2,
      vanillaMax: 4,
      domainMin: 0,
      domainMax: 999,
      target: 'Shared STR / VIT / INT / MEN / AGI / DEX range',
      retail: '+2..+4'
    }),
    scrollOfDiscipline: Object.freeze({
      projectKey: '17',
      itemIds: Object.freeze([17]),
      widthOffset: 0x4149C,
      minimumOffset: 0x414D8,
      vanillaMin: 1,
      vanillaMax: 3,
      domainMin: 1,
      domainMax: 100,
      target: 'Alignment (C+0x1B)',
      retail: '+1..+3'
    }),
    urnOfChaos: Object.freeze({
      projectKey: '18',
      itemIds: Object.freeze([18]),
      widthOffset: 0x41518,
      minimumOffset: 0x41554,
      vanillaMin: -3,
      vanillaMax: -1,
      domainMin: -100,
      domainMax: -1,
      target: 'Alignment (C+0x1B)',
      retail: '-3..-1'
    }),
    gobletOfDestiny: Object.freeze({
      projectKey: '19',
      itemIds: Object.freeze([19]),
      widthOffset: 0x41570,
      minimumOffset: 0x415AC,
      vanillaMin: -1,
      vanillaMax: 1,
      domainMin: -100,
      domainMax: 100,
      target: 'Luck (C+0x28)',
      retail: '-1..+1'
    })
  });

  var PROJECT_TO_MODEL = {
    '10': 'cupOfLife',
    '11-16': 'sharedStatBoosters11To16',
    '17': 'scrollOfDiscipline',
    '18': 'urnOfChaos',
    '19': 'gobletOfDestiny'
  };

  var ITEM_TO_MODEL = {
    10: 'cupOfLife',
    11: 'sharedStatBoosters11To16',
    12: 'sharedStatBoosters11To16',
    13: 'sharedStatBoosters11To16',
    14: 'sharedStatBoosters11To16',
    15: 'sharedStatBoosters11To16',
    16: 'sharedStatBoosters11To16',
    17: 'scrollOfDiscipline',
    18: 'urnOfChaos',
    19: 'gobletOfDestiny'
  };

  var SHARED_TARGETS = Object.freeze({
    11: 'STR (C+0x1C)',
    12: 'VIT (C+0x1E)',
    13: 'INT (C+0x20)',
    14: 'MEN (C+0x22)',
    15: 'AGI (C+0x24)',
    16: 'DEX (C+0x26)'
  });

  var EDITABLE_WORD_GUARDS = Object.freeze([
    Object.freeze({ modelKey: 'cupOfLife', role: 'width', offset: 0x4134C, vanilla: 0x24030006, form: 0x24030000 }),
    Object.freeze({ modelKey: 'cupOfLife', role: 'minimum', offset: 0x41388, vanilla: 0x24680005, form: 0x24680000 }),
    Object.freeze({ modelKey: 'sharedStatBoosters11To16', role: 'width', offset: 0x41444, vanilla: 0x24030003, form: 0x24030000 }),
    Object.freeze({ modelKey: 'sharedStatBoosters11To16', role: 'minimum', offset: 0x41480, vanilla: 0x24680002, form: 0x24680000 }),
    Object.freeze({ modelKey: 'scrollOfDiscipline', role: 'width', offset: 0x4149C, vanilla: 0x24030003, form: 0x24030000 }),
    Object.freeze({ modelKey: 'scrollOfDiscipline', role: 'minimum', offset: 0x414D8, vanilla: 0x24680001, form: 0x24680000 }),
    Object.freeze({ modelKey: 'urnOfChaos', role: 'width', offset: 0x41518, vanilla: 0x24030003, form: 0x24030000 }),
    Object.freeze({ modelKey: 'urnOfChaos', role: 'minimum', offset: 0x41554, vanilla: 0x2468FFFD, form: 0x24680000 }),
    Object.freeze({ modelKey: 'gobletOfDestiny', role: 'width', offset: 0x41570, vanilla: 0x24030003, form: 0x24030000 }),
    Object.freeze({ modelKey: 'gobletOfDestiny', role: 'minimum', offset: 0x415AC, vanilla: 0x2468FFFF, form: 0x24680000 })
  ]);

  // Explicit negative guards. Context hashes below additionally cover every
  // immutable byte in each complete neighboring generation/application range.
  var IMMUTABLE_WORD_GUARDS = Object.freeze([
    Object.freeze({ offset: 0x2CBD8, expected: 0x24014E6D, label: 'RNG multiplier' }),
    Object.freeze({ offset: 0x2CBE4, expected: 0x25F93039, label: 'RNG increment' }),
    Object.freeze({ offset: 0x2CBF8, expected: 0x30427FFF, label: 'RNG output mask' }),

    Object.freeze({ offset: 0x41334, expected: 0x94830016, label: 'Cup max-HP load' }),
    Object.freeze({ offset: 0x41340, expected: 0x24840016, label: 'Cup max-HP target pointer' }),
    Object.freeze({ offset: 0x41398, expected: 0x240203E7, label: 'Cup cap compare value' }),
    Object.freeze({ offset: 0x413AC, expected: 0x240303E7, label: 'Cup cap store value' }),
    Object.freeze({ offset: 0x413C0, expected: 0xA4830000, label: 'Cup max-HP store' }),

    Object.freeze({ offset: 0x413CC, expected: 0x9483001C, label: 'Sword/STR target selector' }),
    Object.freeze({ offset: 0x413D0, expected: 0x0805AD4C, label: 'Shared stat apply branch' }),
    Object.freeze({ offset: 0x413D4, expected: 0x2484001C, label: 'Sword/STR target pointer' }),
    Object.freeze({ offset: 0x413D8, expected: 0x00021403, label: 'Shared stat signed delta' }),
    Object.freeze({ offset: 0x413DC, expected: 0x00621821, label: 'Shared stat addition' }),
    Object.freeze({ offset: 0x413E0, expected: 0x240203E7, label: 'Shared stat cap compare value' }),
    Object.freeze({ offset: 0x413E4, expected: 0x0043102A, label: 'Shared stat cap comparison' }),
    Object.freeze({ offset: 0x413E8, expected: 0x5440FFF3, label: 'Shared stat cap branch' }),
    Object.freeze({ offset: 0x413EC, expected: 0x240303E7, label: 'Shared stat cap store value' }),
    Object.freeze({ offset: 0x413F8, expected: 0x9483001E, label: 'Bracer/VIT target selector' }),
    Object.freeze({ offset: 0x41404, expected: 0x94830020, label: 'Crown/INT target selector' }),
    Object.freeze({ offset: 0x41410, expected: 0x94830022, label: 'Mirror/MEN target selector' }),
    Object.freeze({ offset: 0x4141C, expected: 0x94830024, label: 'Stone/AGI target selector' }),
    Object.freeze({ offset: 0x41428, expected: 0x94830026, label: 'Crystal/DEX target selector' }),

    Object.freeze({ offset: 0x41484, expected: 0x9083001B, label: 'Scroll Alignment load' }),
    Object.freeze({ offset: 0x41490, expected: 0x2484001B, label: 'Scroll Alignment target pointer' }),
    Object.freeze({ offset: 0x414E8, expected: 0x24020064, label: 'Alignment cap compare value' }),
    Object.freeze({ offset: 0x414F4, expected: 0x24030064, label: 'Alignment cap store value' }),
    Object.freeze({ offset: 0x41500, expected: 0x9083001B, label: 'Urn Alignment load' }),
    Object.freeze({ offset: 0x4150C, expected: 0x2484001B, label: 'Urn Alignment target pointer' }),

    Object.freeze({ offset: 0x41558, expected: 0x90830028, label: 'Goblet Luck load' }),
    Object.freeze({ offset: 0x41564, expected: 0x24840028, label: 'Goblet Luck target pointer' }),
    Object.freeze({ offset: 0x415B0, expected: 0x00051400, label: 'Goblet signed delta shift left' }),
    Object.freeze({ offset: 0x415B4, expected: 0x00021403, label: 'Goblet signed delta shift right' }),
    Object.freeze({ offset: 0x415B8, expected: 0x00621821, label: 'Goblet Luck addition' }),
    Object.freeze({ offset: 0x415BC, expected: 0x24020064, label: 'Goblet cap compare value' }),
    Object.freeze({ offset: 0x415C0, expected: 0x0043102A, label: 'Goblet cap comparison' }),
    Object.freeze({ offset: 0x415C4, expected: 0x10400003, label: 'Goblet cap branch' }),
    Object.freeze({ offset: 0x415C8, expected: 0x00031027, label: 'Goblet lower clamp inversion' }),
    Object.freeze({ offset: 0x415CC, expected: 0x0805ADB7, label: 'Goblet shared-tail branch' }),
    Object.freeze({ offset: 0x415D0, expected: 0x24030064, label: 'Goblet cap store value' }),
    Object.freeze({ offset: 0x415D4, expected: 0x000217C3, label: 'Shared lower clamp sign extraction' }),
    Object.freeze({ offset: 0x415D8, expected: 0x00621824, label: 'Shared lower clamp mask' }),
    Object.freeze({ offset: 0x415DC, expected: 0x312200FF, label: 'Shared apply-flag mask' }),
    Object.freeze({ offset: 0x415E0, expected: 0x54400001, label: 'Shared apply/store branch' }),
    Object.freeze({ offset: 0x415E4, expected: 0xA0830000, label: 'Final shared byte store' })
  ]);

  var CONTEXT_GUARDS = Object.freeze([
    Object.freeze({
      id: 'cupOfLife', start: 0x41334, end: 0x413CC,
      immutableLength: 144,
      immutableProjectionSha256: 'F10816BFBCC669EDB2E4434D82EF616A8EA3538D64C3EA3E475A27DC85A3FB04',
      sourceFullSha256: '095B382AB1ACAD1DDE481F6BDCAB25BF7A0C676E29B4CF3D88E55AD504ABFA4F',
      mutableOffsets: Object.freeze([0x4134C, 0x41388])
    }),
    Object.freeze({
      id: 'sharedStatBoosters11To16', start: 0x413CC, end: 0x41484,
      immutableLength: 176,
      immutableProjectionSha256: '1F065073C634DD4764E8E1751767FD52ED7AEE0B5B2459CB77B1FFE706839AF9',
      sourceFullSha256: '663355F1E4B6F65DC0BC1EE0F28750C09E08A6A76D22AF216446BE9BEF75E66A',
      mutableOffsets: Object.freeze([0x41444, 0x41480])
    }),
    Object.freeze({
      id: 'scrollOfDiscipline', start: 0x41484, end: 0x41500,
      immutableLength: 116,
      immutableProjectionSha256: '984289E0B3B386CB2A2CB1462BBF03651071CB7957D26395B983BC9EA18B8F70',
      sourceFullSha256: '96D91D556326D1F5769308FA44E5644C9292CC711FD7876B370E0D50BBB63E25',
      mutableOffsets: Object.freeze([0x4149C, 0x414D8])
    }),
    Object.freeze({
      id: 'urnOfChaos', start: 0x41500, end: 0x41558,
      immutableLength: 80,
      immutableProjectionSha256: 'E21D34EE2FB0B2A6AC737B395F8A090856EF768A56DF9EED3B3BB07A5DDEE70B',
      sourceFullSha256: '1E7D65AEFFDFFA837B6421D040F30AD573169C4E4930CDA96E2328A5C7BBDEAB',
      mutableOffsets: Object.freeze([0x41518, 0x41554])
    }),
    Object.freeze({
      id: 'gobletOfDestiny', start: 0x41558, end: 0x415E8,
      immutableLength: 136,
      immutableProjectionSha256: '8F013ED18226A3FB6226A9D4E4F08D583143FAC4A106CCEFEF39B9B5C9C859CB',
      sourceFullSha256: '6004206B92FB90AAB7FC58221202E320456104BE25CDB15FD05314162DC58D4C',
      mutableOffsets: Object.freeze([0x41570, 0x415AC])
    })
  ]);

  var DISPATCH_WORDS = Object.freeze([
    0x8016B1EC, 0x8016B1EC, 0x8016B208, 0x8016B33C, 0x8016B33C,
    0x8016B388, 0x8016B3CC, 0x8016B6EC, 0x8016B408, 0x8016B434,
    0x8016B4CC, 0x8016B4F8, 0x8016B504, 0x8016B510, 0x8016B51C,
    0x8016B528, 0x8016B584, 0x8016B600, 0x8016B658
  ]);

  var TARGET_METADATA_GUARDS = Object.freeze([
    Object.freeze({ id: 10, offset: 0x6464C, expected: 0x01 }),
    Object.freeze({ id: 11, offset: 0x64658, expected: 0x01 }),
    Object.freeze({ id: 12, offset: 0x64664, expected: 0x01 }),
    Object.freeze({ id: 13, offset: 0x64670, expected: 0x01 }),
    Object.freeze({ id: 14, offset: 0x6467C, expected: 0x01 }),
    Object.freeze({ id: 15, offset: 0x64688, expected: 0x01 }),
    Object.freeze({ id: 16, offset: 0x64694, expected: 0x01 }),
    Object.freeze({ id: 17, offset: 0x646A0, expected: 0x01 }),
    Object.freeze({ id: 18, offset: 0x646AC, expected: 0x01 }),
    Object.freeze({ id: 19, offset: 0x646B8, expected: 0x01 })
  ]);

  var GUARD_MANIFEST = Object.freeze({
    sourceSha256: SOURCE_DESCRIPTOR.sha256,
    editableWords: EDITABLE_WORD_GUARDS,
    immutableWords: IMMUTABLE_WORD_GUARDS,
    contextRanges: CONTEXT_GUARDS,
    dispatchTable: Object.freeze({
      start: 0x65D60,
      end: 0x65DAC,
      sha256: '06532D7BCA4FD20FF9409CCEEFAF0B518250005DB000039CFDF66FFB375D1D04',
      words: DISPATCH_WORDS
    }),
    targetMetadata: TARGET_METADATA_GUARDS
  });

  var EXPECTED_LOADED_WORDS = Object.freeze({
    cupOfLife: Object.freeze({ width: '0x8016B44C', minimum: '0x8016B488' }),
    sharedStatBoosters11To16: Object.freeze({ width: '0x8016B544', minimum: '0x8016B580' }),
    scrollOfDiscipline: Object.freeze({ width: '0x8016B59C', minimum: '0x8016B5D8' }),
    urnOfChaos: Object.freeze({ width: '0x8016B618', minimum: '0x8016B654' }),
    gobletOfDestiny: Object.freeze({ width: '0x8016B670', minimum: '0x8016B6AC' }),
    finalStore: '0x8016B6E4',
    dispatchTable: Object.freeze({
      source: '[0x00065D60,0x00065DAC)',
      live: '[0x8018FE60,0x8018FEAC)',
      lastWord: '0x8018FEA8'
    })
  });

  function own(obj, key) {
    return !!obj && Object.prototype.hasOwnProperty.call(obj, key);
  }

  function isPlainObject(value) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
    var proto = Object.getPrototypeOf(value);
    return proto === Object.prototype || proto === null;
  }

  function hex(value, width) {
    var text = (Number(value) >>> 0).toString(16).toUpperCase();
    while (text.length < (width || 8)) text = '0' + text;
    return '0x' + text;
  }

  function bytesToHex(bytes) {
    var out = '';
    for (var i = 0; bytes && i < bytes.length; i++) {
      out += (bytes[i] & 0xFF).toString(16).toUpperCase().padStart(2, '0');
    }
    return out;
  }

  function readU32(z64, offset) {
    return (((z64[offset] << 24) >>> 0) |
      (z64[offset + 1] << 16) |
      (z64[offset + 2] << 8) |
      z64[offset + 3]) >>> 0;
  }

  function writeU32(z64, offset, value) {
    value >>>= 0;
    z64[offset] = (value >>> 24) & 0xFF;
    z64[offset + 1] = (value >>> 16) & 0xFF;
    z64[offset + 2] = (value >>> 8) & 0xFF;
    z64[offset + 3] = value & 0xFF;
  }

  function asBytes(value) {
    if (value instanceof Uint8Array) return value;
    if (value instanceof ArrayBuffer) return new Uint8Array(value);
    if (ArrayBuffer.isView(value)) {
      return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
    }
    if (typeof value === 'string') {
      if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(value);
      var encoded = unescape(encodeURIComponent(value));
      var out = new Uint8Array(encoded.length);
      for (var i = 0; i < encoded.length; i++) out[i] = encoded.charCodeAt(i);
      return out;
    }
    return new Uint8Array(value || []);
  }

  // Small dependency-free SHA-256 implementation used by guard projections and
  // as a fallback when Web Crypto is unavailable under file://.
  function sha256HexSync(input) {
    var bytes = asBytes(input);
    var bitLength = bytes.length * 8;
    var withOne = bytes.length + 1;
    var paddedLength = ((withOne + 8 + 63) >> 6) << 6;
    var data = new Uint8Array(paddedLength);
    data.set(bytes);
    data[bytes.length] = 0x80;
    var high = Math.floor(bitLength / 0x100000000);
    var low = bitLength >>> 0;
    data[paddedLength - 8] = (high >>> 24) & 0xFF;
    data[paddedLength - 7] = (high >>> 16) & 0xFF;
    data[paddedLength - 6] = (high >>> 8) & 0xFF;
    data[paddedLength - 5] = high & 0xFF;
    data[paddedLength - 4] = (low >>> 24) & 0xFF;
    data[paddedLength - 3] = (low >>> 16) & 0xFF;
    data[paddedLength - 2] = (low >>> 8) & 0xFF;
    data[paddedLength - 1] = low & 0xFF;

    var k = [
      0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
      0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
      0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
      0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
      0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
      0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
      0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
      0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
    ];
    var h = [0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19];
    var w = new Uint32Array(64);
    function rotr(x, n) { return (x >>> n) | (x << (32 - n)); }

    for (var base = 0; base < data.length; base += 64) {
      for (var wi = 0; wi < 16; wi++) {
        var o = base + wi * 4;
        w[wi] = ((data[o] << 24) | (data[o + 1] << 16) | (data[o + 2] << 8) | data[o + 3]) >>> 0;
      }
      for (var wx = 16; wx < 64; wx++) {
        var s0 = rotr(w[wx - 15], 7) ^ rotr(w[wx - 15], 18) ^ (w[wx - 15] >>> 3);
        var s1 = rotr(w[wx - 2], 17) ^ rotr(w[wx - 2], 19) ^ (w[wx - 2] >>> 10);
        w[wx] = (w[wx - 16] + s0 + w[wx - 7] + s1) >>> 0;
      }
      var a = h[0], b = h[1], c = h[2], d = h[3];
      var e = h[4], f = h[5], g = h[6], hh = h[7];
      for (var round = 0; round < 64; round++) {
        var sum1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        var choose = (e & f) ^ (~e & g);
        var t1 = (hh + sum1 + choose + k[round] + w[round]) >>> 0;
        var sum0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        var majority = (a & b) ^ (a & c) ^ (b & c);
        var t2 = (sum0 + majority) >>> 0;
        hh = g; g = f; f = e; e = (d + t1) >>> 0;
        d = c; c = b; b = a; a = (t1 + t2) >>> 0;
      }
      h[0] = (h[0] + a) >>> 0;
      h[1] = (h[1] + b) >>> 0;
      h[2] = (h[2] + c) >>> 0;
      h[3] = (h[3] + d) >>> 0;
      h[4] = (h[4] + e) >>> 0;
      h[5] = (h[5] + f) >>> 0;
      h[6] = (h[6] + g) >>> 0;
      h[7] = (h[7] + hh) >>> 0;
    }
    return h.map(function(value) { return value.toString(16).padStart(8, '0'); }).join('').toUpperCase();
  }

  function sha256Hex(input) {
    var bytes = asBytes(input);
    var cryptoObject = typeof globalThis !== 'undefined' ? globalThis.crypto : null;
    if (cryptoObject && cryptoObject.subtle && cryptoObject.subtle.digest) {
      var copy = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      return cryptoObject.subtle.digest('SHA-256', copy).then(function(digest) {
        return bytesToHex(new Uint8Array(digest));
      });
    }
    return Promise.resolve(sha256HexSync(bytes));
  }

  function detectByteOrder(raw) {
    if (!raw || raw.length < 4) return 'unknown';
    var key = [raw[0], raw[1], raw[2], raw[3]].map(function(v) {
      return v.toString(16).padStart(2, '0');
    }).join('').toUpperCase();
    if (key === '37804012') return 'v64';
    if (key === '80371240') return 'z64';
    if (key === '40123780') return 'n64';
    return 'unknown';
  }

  function normalizedHeader(raw, byteOrder) {
    var length = Math.min(0x40, raw.length);
    var out = new Uint8Array(length);
    var i;
    if (byteOrder === 'v64') {
      for (i = 0; i + 1 < length; i += 2) {
        out[i] = raw[i + 1];
        out[i + 1] = raw[i];
      }
      return out;
    }
    if (byteOrder === 'n64') {
      for (i = 0; i + 3 < length; i += 4) {
        out[i] = raw[i + 3];
        out[i + 1] = raw[i + 2];
        out[i + 2] = raw[i + 1];
        out[i + 3] = raw[i];
      }
      return out;
    }
    out.set(raw.subarray(0, length));
    return out;
  }

  function headerText(bytes, offset, length) {
    var out = '';
    for (var i = 0; i < length && offset + i < bytes.length; i++) {
      var value = bytes[offset + i];
      if (!value) break;
      out += String.fromCharCode(value);
    }
    return out;
  }

  function sourceFactsFromRaw(rawInput, filename, hash) {
    var raw = asBytes(rawInput);
    var byteOrder = detectByteOrder(raw);
    var header = normalizedHeader(raw, byteOrder);
    return {
      filename: filename || '',
      size: raw.length,
      sha256: String(hash || '').toUpperCase(),
      byteOrder: byteOrder,
      imageName: headerText(header, 0x20, 20).trim(),
      gameId: headerText(header, 0x3B, 4),
      country: header.length > 0x3E ? header[0x3E] : null,
      version: header.length > 0x3F ? header[0x3F] : null,
      crc1: header.length >= 0x14 ? readU32(header, 0x10) : null,
      crc2: header.length >= 0x18 ? readU32(header, 0x14) : null
    };
  }

  function evaluateSourceIdentity(facts) {
    facts = facts || {};
    var checks = {
      filename: facts.filename === SOURCE_DESCRIPTOR.filename,
      size: facts.size === SOURCE_DESCRIPTOR.size,
      sha256: String(facts.sha256 || '').toUpperCase() === SOURCE_DESCRIPTOR.sha256,
      byteOrder: facts.byteOrder === SOURCE_DESCRIPTOR.byteOrder,
      imageName: facts.imageName === SOURCE_DESCRIPTOR.imageName,
      gameId: facts.gameId === SOURCE_DESCRIPTOR.gameId,
      country: Number(facts.country) === SOURCE_DESCRIPTOR.country,
      version: Number(facts.version) === SOURCE_DESCRIPTOR.version,
      crc1: (Number(facts.crc1) >>> 0) === SOURCE_DESCRIPTOR.crc1,
      crc2: (Number(facts.crc2) >>> 0) === SOURCE_DESCRIPTOR.crc2
    };
    var eligible = Object.keys(checks).every(function(key) { return checks[key]; });
    var reason = '';
    if (!eligible) {
      if (!checks.filename) {
        reason = 'Effect editing requires the exact verified source file named "' +
          SOURCE_DESCRIPTOR.filename + '".';
      } else if (!checks.size) {
        reason = 'Effect editing requires the exact 41,943,040-byte US rev0 source.';
      } else if (!checks.sha256) {
        reason = 'The loaded file is not the verified immutable US rev0 source. Reopened candidates, modified ROMs, and header-only matches cannot enable effect editing.';
      } else if (!checks.byteOrder) {
        reason = 'The verified effect source must be loaded in its original .v64 byte order.';
      } else {
        reason = 'The loaded file does not match the verified US rev0 header identity required for effect editing.';
      }
    }
    return {
      eligible: eligible,
      reason: reason,
      facts: Object.freeze({
        filename: facts.filename || '',
        size: facts.size,
        sha256: String(facts.sha256 || '').toUpperCase(),
        byteOrder: facts.byteOrder || 'unknown',
        imageName: facts.imageName || '',
        gameId: facts.gameId || '',
        country: facts.country,
        version: facts.version,
        crc1: facts.crc1,
        crc2: facts.crc2
      }),
      checks: Object.freeze(checks)
    };
  }

  function inspectSourceIdentity(rawInput, filename) {
    var raw = asBytes(rawInput);
    return sha256Hex(raw).then(function(hash) {
      return evaluateSourceIdentity(sourceFactsFromRaw(raw, filename, hash));
    });
  }

  function cloneModels(models) {
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var source = models && models[key] ? models[key] : MODEL_DEFS[key];
      out[key] = { minimum: Number(source.minimum != null ? source.minimum : source.vanillaMin),
        maximum: Number(source.maximum != null ? source.maximum : source.vanillaMax) };
    }
    return out;
  }

  function vanillaModels() {
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      out[key] = { minimum: MODEL_DEFS[key].vanillaMin, maximum: MODEL_DEFS[key].vanillaMax };
    }
    return out;
  }

  function validateRange(modelKey, minimum, maximum) {
    var def = MODEL_DEFS[modelKey];
    if (!def) throw new Error('Unknown consumable effect model "' + modelKey + '".');
    if (!Number.isInteger(minimum) || !Number.isInteger(maximum)) {
      throw new Error('Minimum and Maximum must be integers.');
    }
    if (minimum > maximum) throw new Error('Minimum cannot be greater than Maximum.');
    if (minimum < def.domainMin || maximum > def.domainMax) {
      throw new Error('Supported range for this effect is ' + def.domainMin + ' through ' + def.domainMax + '.');
    }
    var width = maximum - minimum + 1;
    if (!Number.isInteger(width) || width < 1 || width > 32767) {
      throw new Error('Inclusive width must be an integer from 1 through 32767.');
    }
    return { minimum: minimum, maximum: maximum, width: width };
  }

  function validateAllModels(models) {
    var normalized = {};
    var keys = Object.keys(models || {});
    for (var k = 0; k < keys.length; k++) {
      if (MODEL_ORDER.indexOf(keys[k]) === -1) {
        throw new Error('Unknown canonical consumable effect model "' + keys[k] + '".');
      }
    }
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var value = models && models[key];
      if (!value) throw new Error('Missing canonical consumable effect model "' + key + '".');
      normalized[key] = validateRange(key, value.minimum, value.maximum);
    }
    return normalized;
  }

  function encodeRange(modelKey, range) {
    var checked = validateRange(modelKey, range.minimum, range.maximum);
    return {
      width: checked.width,
      widthWord: (0x24030000 | checked.width) >>> 0,
      minimumWord: (0x24680000 | (checked.minimum & 0xFFFF)) >>> 0
    };
  }

  function wordKey(offset) {
    return String(offset >>> 0);
  }

  function initialLedgerWords() {
    var out = {};
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      out[wordKey(EDITABLE_WORD_GUARDS[i].offset)] = EDITABLE_WORD_GUARDS[i].vanilla >>> 0;
    }
    return out;
  }

  function mutableOffsetSet(manifest) {
    var set = {};
    var entries = (manifest || GUARD_MANIFEST).editableWords || [];
    for (var i = 0; i < entries.length; i++) set[wordKey(entries[i].offset)] = true;
    return set;
  }

  function immutableProjection(z64, range, manifest) {
    var mutable = mutableOffsetSet(manifest);
    var bytes = [];
    for (var offset = range.start; offset < range.end; offset += 4) {
      if (mutable[wordKey(offset)]) continue;
      for (var b = 0; b < 4; b++) bytes.push(z64[offset + b]);
    }
    return new Uint8Array(bytes);
  }

  function validateGuards(z64, session, manifest) {
    manifest = manifest || GUARD_MANIFEST;
    var errors = [];
    var editable = manifest.editableWords || [];
    var ledgerWords = session && session.ledger && session.ledger.currentWords || initialLedgerWords();
    for (var i = 0; i < editable.length; i++) {
      var edit = editable[i];
      if (!z64 || edit.offset + 4 > z64.length) {
        errors.push(edit.modelKey + ' ' + edit.role + ' guard lies outside the loaded image.');
        continue;
      }
      var actual = readU32(z64, edit.offset);
      var expected = own(ledgerWords, wordKey(edit.offset))
        ? ledgerWords[wordKey(edit.offset)] >>> 0
        : edit.vanilla >>> 0;
      if (actual !== expected) {
        errors.push(edit.modelKey + ' ' + edit.role + ' at ' + hex(edit.offset, 6) +
          ' expected ledger word ' + hex(expected) + ' but found ' + hex(actual) + '.');
      }
      if ((actual & 0xFFFF0000) !== edit.form) {
        errors.push(edit.modelKey + ' ' + edit.role + ' at ' + hex(edit.offset, 6) +
          ' no longer has the required opcode/register form ' + hex(edit.form) + '.');
      }
    }

    var immutable = manifest.immutableWords || [];
    for (var g = 0; g < immutable.length; g++) {
      var guard = immutable[g];
      if (!z64 || guard.offset + 4 > z64.length) {
        errors.push(guard.label + ' guard lies outside the loaded image.');
        continue;
      }
      var found = readU32(z64, guard.offset);
      if (found !== (guard.expected >>> 0)) {
        errors.push(guard.label + ' at ' + hex(guard.offset, 6) + ' expected ' +
          hex(guard.expected) + ' but found ' + hex(found) + '.');
      }
    }

    var contexts = manifest.contextRanges || [];
    for (var c = 0; c < contexts.length; c++) {
      var context = contexts[c];
      if (!z64 || context.end > z64.length) {
        errors.push(context.id + ' immutable context lies outside the loaded image.');
        continue;
      }
      var projection = immutableProjection(z64, context, manifest);
      if (context.immutableLength != null && projection.length !== context.immutableLength) {
        errors.push(context.id + ' immutable context length expected ' + context.immutableLength +
          ' but projected ' + projection.length + '.');
      }
      var projectedHash = sha256HexSync(projection);
      if (projectedHash !== context.immutableProjectionSha256) {
        errors.push(context.id + ' immutable context ' + hex(context.start, 6) + '..' +
          hex(context.end, 6) + ' expected SHA-256 ' + context.immutableProjectionSha256 +
          ' but found ' + projectedHash + '.');
      }
    }

    var table = manifest.dispatchTable;
    if (table) {
      if (!z64 || table.end > z64.length) {
        errors.push('The complete 19-word dispatch table lies outside the loaded image.');
      } else {
        for (var d = 0; d < table.words.length; d++) {
          var tableOffset = table.start + d * 4;
          var tableWord = readU32(z64, tableOffset);
          if (tableWord !== (table.words[d] >>> 0)) {
            errors.push('Dispatch table word ' + (d + 1) + ' at ' + hex(tableOffset, 6) +
              ' expected ' + hex(table.words[d]) + ' but found ' + hex(tableWord) + '.');
          }
        }
        var tableHash = sha256HexSync(z64.subarray(table.start, table.end));
        if (tableHash !== table.sha256) {
          errors.push('Complete dispatch table SHA-256 expected ' + table.sha256 + ' but found ' + tableHash + '.');
        }
      }
    }

    var metadata = manifest.targetMetadata || [];
    for (var m = 0; m < metadata.length; m++) {
      var meta = metadata[m];
      if (!z64 || meta.offset >= z64.length) {
        errors.push('Target-mode metadata for ID ' + meta.id + ' lies outside the loaded image.');
      } else if (z64[meta.offset] !== meta.expected) {
        errors.push('Target-mode metadata for ID ' + meta.id + ' at ' + hex(meta.offset, 6) +
          ' expected ' + hex(meta.expected, 2) + ' but found ' + hex(z64[meta.offset], 2) + '.');
      }
    }
    return { ok: errors.length === 0, errors: errors };
  }

  function initializeSession(rom, identity, sourceMetadata) {
    if (!rom || !rom.z64) throw new Error('A parsed ROM is required to initialize consumable effects.');
    var immutableIdentity = identity || evaluateSourceIdentity({});
    var session = {
      identity: immutableIdentity,
      source: {
        filename: sourceMetadata && sourceMetadata.filename || (immutableIdentity.facts && immutableIdentity.facts.filename) || '',
        size: immutableIdentity.facts && immutableIdentity.facts.size,
        sha256: immutableIdentity.facts && immutableIdentity.facts.sha256 || '',
        byteOrder: rom.byteOrder || (immutableIdentity.facts && immutableIdentity.facts.byteOrder) || 'unknown',
        header: {
          imageName: immutableIdentity.facts && immutableIdentity.facts.imageName || '',
          gameId: immutableIdentity.facts && immutableIdentity.facts.gameId || '',
          country: immutableIdentity.facts && immutableIdentity.facts.country,
          version: immutableIdentity.facts && immutableIdentity.facts.version,
          crc1: immutableIdentity.facts && immutableIdentity.facts.crc1,
          crc2: immutableIdentity.facts && immutableIdentity.facts.crc2
        }
      },
      baselineZ64: rom.z64.slice(),
      guardManifest: sourceMetadata && sourceMetadata.guardManifest || GUARD_MANIFEST,
      models: vanillaModels(),
      generation: 0,
      pendingWrites: false,
      lastError: '',
      ledger: {
        sourceSha256: immutableIdentity.facts && immutableIdentity.facts.sha256 || '',
        currentWords: initialLedgerWords(),
        effectOwnedWrites: [],
        priorOwnerRegions: [],
        headerCrcWrites: [],
        lastChangeRanges: [],
        lastCandidate: null,
        exports: []
      }
    };
    if (session.identity.eligible) {
      var guardResult = validateGuards(rom.z64, session, session.guardManifest);
      if (!guardResult.ok) {
        session.identity = {
          eligible: false,
          reason: 'The exact source identity matched, but required consumable-effect guards failed: ' +
            guardResult.errors[0],
          facts: immutableIdentity.facts,
          checks: immutableIdentity.checks,
          guardErrors: guardResult.errors
        };
      }
    }
    rom.consumableEffects = session;
    return session;
  }

  function sessionFor(romOrSession) {
    if (!romOrSession) return null;
    return romOrSession.models && romOrSession.ledger
      ? romOrSession
      : romOrSession.consumableEffects || null;
  }

  function assertEligible(session) {
    if (!session || !session.identity || !session.identity.eligible) {
      throw new Error(session && session.identity && session.identity.reason
        ? session.identity.reason
        : 'Consumable effect editing requires a verified source session.');
    }
  }

  function assertSessionOwnership(session, manifest) {
    manifest = manifest || GUARD_MANIFEST;
    assertEligible(session);
    var identityHash = session.identity && session.identity.facts &&
      String(session.identity.facts.sha256 || '').toUpperCase();
    var sourceHash = session.source && String(session.source.sha256 || '').toUpperCase();
    var ledgerHash = session.ledger && String(session.ledger.sourceSha256 || '').toUpperCase();
    if (identityHash !== SOURCE_DESCRIPTOR.sha256 ||
        sourceHash !== SOURCE_DESCRIPTOR.sha256 ||
        ledgerHash !== SOURCE_DESCRIPTOR.sha256 ||
        String(manifest.sourceSha256 || '').toUpperCase() !== SOURCE_DESCRIPTOR.sha256) {
      throw new Error('Consumable effect source/session ledger ownership does not match the verified immutable source.');
    }
    if (!session.baselineZ64 || session.baselineZ64.length !== SOURCE_DESCRIPTOR.size) {
      throw new Error('Consumable effect immutable normalized baseline is missing or has the wrong size.');
    }
    if (!session.ledger.currentWords || !Array.isArray(session.ledger.effectOwnedWrites) ||
        !Array.isArray(session.ledger.priorOwnerRegions) ||
        !Array.isArray(session.ledger.headerCrcWrites) ||
        !Array.isArray(session.ledger.lastChangeRanges) ||
        !Array.isArray(session.ledger.exports)) {
      throw new Error('Consumable effect session ledger is incomplete.');
    }
    var expectedWordKeys = (manifest.editableWords || []).map(function(entry) {
      return wordKey(entry.offset);
    }).sort();
    var ledgerWordKeys = Object.keys(session.ledger.currentWords).sort();
    if (expectedWordKeys.length !== ledgerWordKeys.length ||
        expectedWordKeys.some(function(key, index) { return key !== ledgerWordKeys[index]; })) {
      throw new Error('Consumable effect session ledger does not own exactly the ten editable words.');
    }
    for (var i = 0; i < expectedWordKeys.length; i++) {
      var word = session.ledger.currentWords[expectedWordKeys[i]];
      if (!Number.isInteger(word) || word < 0 || word > 0xFFFFFFFF) {
        throw new Error('Consumable effect session ledger contains an invalid current word.');
      }
    }
    for (var w = 0; w < session.ledger.effectOwnedWrites.length; w++) {
      var owned = session.ledger.effectOwnedWrites[w];
      var ownedKey = wordKey(owned.offset);
      if (!own(session.ledger.currentWords, ownedKey) ||
          (session.ledger.currentWords[ownedKey] >>> 0) !== (owned.afterWord >>> 0)) {
        throw new Error('Consumable effect session ledger contains an unowned or stale effect write.');
      }
    }
  }

  function assertSharedBinding(session) {
    var sharedKey = 'sharedStatBoosters11To16';
    for (var id = 11; id <= 16; id++) {
      if (ITEM_TO_MODEL[id] !== sharedKey) {
        throw new Error('Shared stat item ID ' + id + ' is not bound to the one canonical shared model.');
      }
    }
    if (!session || !session.models || !session.models[sharedKey]) {
      throw new Error('The canonical shared stat-booster model is missing.');
    }
    if (Object.keys(session.models).length !== MODEL_ORDER.length) {
      throw new Error('Consumable effects must contain exactly five canonical models.');
    }
    return true;
  }

  function desiredWords(session) {
    var checked = validateAllModels(session.models);
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var encoded = encodeRange(key, checked[key]);
      var def = MODEL_DEFS[key];
      out[wordKey(def.widthOffset)] = encoded.widthWord;
      out[wordKey(def.minimumOffset)] = encoded.minimumWord;
    }
    return out;
  }

  function refreshPending(session) {
    var wanted = desiredWords(session);
    var current = session.ledger.currentWords;
    session.pendingWrites = Object.keys(wanted).some(function(key) {
      return (wanted[key] >>> 0) !== (current[key] >>> 0);
    });
    return session.pendingWrites;
  }

  function modelDiffersFromVanilla(modelKey, value) {
    var def = MODEL_DEFS[modelKey];
    return value.minimum !== def.vanillaMin || value.maximum !== def.vanillaMax;
  }

  function hasDesiredEffects(session) {
    if (!session) return false;
    return MODEL_ORDER.some(function(key) {
      return modelDiffersFromVanilla(key, session.models[key]);
    });
  }

  function hasAppliedEffects(session) {
    if (!session || !session.ledger) return false;
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      if ((session.ledger.currentWords[wordKey(entry.offset)] >>> 0) !== (entry.vanilla >>> 0)) return true;
    }
    return false;
  }

  function setModelRange(session, modelKey, minimum, maximum) {
    assertSessionOwnership(session, session.guardManifest || GUARD_MANIFEST);
    var prospective = cloneModels(session.models);
    prospective[modelKey] = { minimum: minimum, maximum: maximum };
    var checked = validateAllModels(prospective);
    session.models = cloneModels(checked);
    session.generation++;
    session.lastError = '';
    refreshPending(session);
    return session.models[modelKey];
  }

  function setItemRange(session, itemId, minimum, maximum) {
    var key = ITEM_TO_MODEL[Number(itemId)];
    if (!key) throw new Error('Item ID ' + itemId + ' has no supported effect range.');
    return setModelRange(session, key, minimum, maximum);
  }

  function resetModel(session, modelKey) {
    var def = MODEL_DEFS[modelKey];
    if (!def) throw new Error('Unknown consumable effect model "' + modelKey + '".');
    return setModelRange(session, modelKey, def.vanillaMin, def.vanillaMax);
  }

  function resetItem(session, itemId) {
    var key = ITEM_TO_MODEL[Number(itemId)];
    if (!key) throw new Error('Item ID ' + itemId + ' has no supported effect range.');
    return resetModel(session, key);
  }

  function collectProjectPayload(session) {
    var out = {};
    if (!session) return out;
    var checked = validateAllModels(session.models);
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      if (!modelDiffersFromVanilla(key, checked[key])) continue;
      out[MODEL_DEFS[key].projectKey] = {
        deltaMin: checked[key].minimum,
        deltaMax: checked[key].maximum
      };
    }
    return out;
  }

  function validateProjectPayload(payload, session, patchVersion) {
    if (payload === null) throw new Error('patches.consumableEffects must be an object, not null.');
    if (payload === undefined) return { entries: {}, modelCount: 0 };
    if (!isPlainObject(payload)) throw new Error('patches.consumableEffects must be an object.');
    if (patchVersion != null && patchVersion < PROJECT_VERSION && Object.keys(payload).length) {
      throw new Error('Consumable effect data requires Project format version 12.');
    }
    var keys = Object.keys(payload);
    var normalized = {};
    for (var i = 0; i < keys.length; i++) {
      var projectKey = keys[i];
      if (!own(PROJECT_TO_MODEL, projectKey)) {
        if (/^(11|12|13|14|15|16)$/.test(projectKey)) {
          throw new Error('IDs 11-16 must use the one shared Project key "11-16".');
        }
        throw new Error('Unsupported consumable effect Project key "' + projectKey + '".');
      }
      var entry = payload[projectKey];
      if (!isPlainObject(entry)) {
        throw new Error('Consumable effect "' + projectKey + '" must be an object.');
      }
      var fields = Object.keys(entry);
      if (fields.length !== 2 || fields.indexOf('deltaMin') === -1 || fields.indexOf('deltaMax') === -1) {
        throw new Error('Consumable effect "' + projectKey + '" must contain only deltaMin and deltaMax.');
      }
      var modelKey = PROJECT_TO_MODEL[projectKey];
      var checked = validateRange(modelKey, entry.deltaMin, entry.deltaMax);
      normalized[modelKey] = { minimum: checked.minimum, maximum: checked.maximum };
    }
    if (keys.length) assertSessionOwnership(session, session && session.guardManifest || GUARD_MANIFEST);
    var prospective = cloneModels(session ? session.models : vanillaModels());
    Object.keys(normalized).forEach(function(key) { prospective[key] = normalized[key]; });
    validateAllModels(prospective);
    return { entries: normalized, modelCount: keys.length };
  }

  function applyProjectPayload(session, validated) {
    if (!validated || !validated.modelCount) return 0;
    assertSessionOwnership(session, session.guardManifest || GUARD_MANIFEST);
    var prospective = cloneModels(session.models);
    Object.keys(validated.entries).forEach(function(key) {
      prospective[key] = validated.entries[key];
    });
    session.models = cloneModels(validateAllModels(prospective));
    session.generation++;
    refreshPending(session);
    return validated.modelCount;
  }

  function normalizeRegion(owner, region) {
    var start = Number(region && (region.start != null ? region.start : region.offset));
    var size = Number(region && (region.size != null ? region.size : region.length));
    if (!region || !Number.isFinite(start) || !Number.isFinite(size) || size <= 0) return null;
    return {
      ownerId: owner.id,
      ownerName: owner.name,
      category: owner.category || owner.id,
      kind: region.kind || 'rom',
      start: start,
      end: start + size,
      size: size,
      label: region.label || 'region'
    };
  }

  function normalizedOwnerRegions(owners) {
    var out = [];
    for (var i = 0; i < (owners || []).length; i++) {
      var owner = owners[i];
      for (var r = 0; r < (owner.regions || []).length; r++) {
        var normalized = normalizeRegion(owner, owner.regions[r]);
        if (normalized) out.push(normalized);
      }
    }
    return out;
  }

  function findRegionConflicts(owners) {
    var regions = normalizedOwnerRegions(owners);
    var out = [];
    for (var i = 0; i < regions.length; i++) {
      for (var j = i + 1; j < regions.length; j++) {
        var a = regions[i], b = regions[j];
        if (a.ownerId === b.ownerId || a.kind !== b.kind) continue;
        if (a.start < b.end && b.start < a.end) out.push({ a: a, b: b });
      }
    }
    return out;
  }

  function conflictMessage(conflicts) {
    return conflicts.map(function(conflict) {
      return conflict.a.ownerName + ' ' + conflict.a.kind + ' ' +
        hex(conflict.a.start, 6) + '..' + hex(conflict.a.end, 6) +
        ' (' + conflict.a.label + ') overlaps ' +
        conflict.b.ownerName + ' ' + conflict.b.kind + ' ' +
        hex(conflict.b.start, 6) + '..' + hex(conflict.b.end, 6) +
        ' (' + conflict.b.label + ')';
    }).join('\n  ');
  }

  function effectCollisionOwner() {
    var regions = [];
    for (var i = 0; i < CONTEXT_GUARDS.length; i++) {
      regions.push({
        kind: 'rom',
        start: CONTEXT_GUARDS[i].start,
        size: CONTEXT_GUARDS[i].end - CONTEXT_GUARDS[i].start,
        label: CONTEXT_GUARDS[i].id + ' guarded path'
      });
    }
    for (var r = 0; r < 3; r++) {
      regions.push({
        kind: 'rom',
        start: IMMUTABLE_WORD_GUARDS[r].offset,
        size: 4,
        label: IMMUTABLE_WORD_GUARDS[r].label
      });
    }
    regions.push({
      kind: 'rom',
      start: GUARD_MANIFEST.dispatchTable.start,
      size: GUARD_MANIFEST.dispatchTable.end - GUARD_MANIFEST.dispatchTable.start,
      label: 'complete 19-word consumable dispatch table'
    });
    for (var m = 0; m < TARGET_METADATA_GUARDS.length; m++) {
      regions.push({
        kind: 'rom',
        start: TARGET_METADATA_GUARDS[m].offset,
        size: 1,
        label: 'ID ' + TARGET_METADATA_GUARDS[m].id + ' target-mode metadata'
      });
    }
    return {
      id: 'consumable-effects-guard-collision',
      name: 'Consumable Effects Guard/Collision Surface',
      category: 'consumableEffects',
      regions: regions
    };
  }

  function effectDeltaOwner() {
    return {
      id: 'consumable-effects',
      name: 'Consumable Effects',
      category: 'consumableEffects',
      regions: EDITABLE_WORD_GUARDS.map(function(entry) {
        return {
          kind: 'rom',
          start: entry.offset,
          size: 4,
          label: entry.modelKey + ' ' + entry.role + ' editable effect word'
        };
      })
    };
  }

  function assertEffectDeltaOwner(owner) {
    if (!owner || owner.id !== 'consumable-effects' ||
        owner.category !== 'consumableEffects' ||
        !owner.regions || owner.regions.length !== EDITABLE_WORD_GUARDS.length) {
      throw new Error('Consumable effect delta ownership must be the ten concrete editable words.');
    }
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var expected = EDITABLE_WORD_GUARDS[i];
      var actual = owner.regions[i];
      if (!actual || actual.kind !== 'rom' || actual.start !== expected.offset ||
          actual.size !== 4) {
        throw new Error('Consumable effect delta ownership is not concrete at ' +
          hex(expected.offset, 6) + '.');
      }
    }
    return owner;
  }

  function prepareTransaction(session, z64, otherOwners) {
    if (!session) return null;
    refreshPending(session);
    var relevant = session.pendingWrites || hasDesiredEffects(session) || hasAppliedEffects(session);
    if (!relevant) return null;
    assertSessionOwnership(session, session.guardManifest || GUARD_MANIFEST);
    var checked = validateAllModels(session.models);
    assertSharedBinding(session);
    var guarded = validateGuards(z64, session, session.guardManifest || GUARD_MANIFEST);
    if (!guarded.ok) {
      throw new Error('Consumable effect preflight failed before writes:\n  ' + guarded.errors.join('\n  '));
    }

    var wanted = desiredWords(session);
    var writes = [];
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      var key = wordKey(entry.offset);
      var before = session.ledger.currentWords[key] >>> 0;
      var after = wanted[key] >>> 0;
      if (before !== after) {
        writes.push({
          modelKey: entry.modelKey,
          role: entry.role,
          offset: entry.offset,
          beforeWord: before,
          afterWord: after
        });
      }
    }

    var collisionOwner = effectCollisionOwner();
    var deltaOwner = assertEffectDeltaOwner(effectDeltaOwner());
    var owners = (otherOwners || []).filter(function(otherOwner) {
      // Same-session prior effect ranges are concrete restoration ownership,
      // not a foreign subsystem collision.
      return otherOwner.id !== deltaOwner.id;
    });
    owners.push(collisionOwner);
    var conflicts = findRegionConflicts(owners).filter(function(conflict) {
      return conflict.a.ownerId === collisionOwner.id ||
        conflict.b.ownerId === collisionOwner.id;
    });
    if (conflicts.length) {
      throw new Error('Patch region collision:\n  ' + conflictMessage(conflicts));
    }

    var seen = {};
    for (var w = 0; w < writes.length; w++) {
      var write = writes[w];
      if (seen[write.offset]) throw new Error('Duplicate effect write at ' + hex(write.offset, 6) + '.');
      seen[write.offset] = true;
      if (write.offset < 0 || write.offset + 4 > z64.length) {
        throw new Error('Effect write lies outside the candidate at ' + hex(write.offset, 6) + '.');
      }
      if (readU32(z64, write.offset) !== write.beforeWord) {
        throw new Error('Effect write preimage changed at ' + hex(write.offset, 6) + '.');
      }
    }

    var models = cloneModels(checked);
    var modelChanges = [];
    for (var modelIndex = 0; modelIndex < MODEL_ORDER.length; modelIndex++) {
      var modelKey = MODEL_ORDER[modelIndex];
      var def = MODEL_DEFS[modelKey];
      var pairWrites = writes.filter(function(writeEntry) { return writeEntry.modelKey === modelKey; });
      var encoded = encodeRange(modelKey, models[modelKey]);
      var previousWidth = session.ledger.currentWords[wordKey(def.widthOffset)] >>> 0;
      var previousMinimum = session.ledger.currentWords[wordKey(def.minimumOffset)] >>> 0;
      if (pairWrites.length || modelDiffersFromVanilla(modelKey, models[modelKey]) ||
          previousWidth !== EDITABLE_WORD_GUARDS.find(function(e) {
            return e.modelKey === modelKey && e.role === 'width';
          }).vanilla) {
        modelChanges.push({
          modelKey: modelKey,
          projectKey: def.projectKey,
          itemIds: def.itemIds.slice(),
          minimum: models[modelKey].minimum,
          maximum: models[modelKey].maximum,
          width: encoded.width,
          previousWords: {
            width: hex(previousWidth),
            minimum: hex(previousMinimum)
          },
          candidateWords: {
            width: hex(encoded.widthWord),
            minimum: hex(encoded.minimumWord)
          }
        });
      }
    }

    return {
      baseGeneration: session.generation,
      sourceWords: Object.assign({}, session.ledger.currentWords),
      desiredWords: wanted,
      writes: writes,
      models: models,
      modelChanges: modelChanges,
      collisionOwner: collisionOwner,
      deltaOwner: deltaOwner,
      otherOwners: (otherOwners || []).slice(),
      applied: false
    };
  }

  function applyTransaction(transaction, candidateZ64, session) {
    if (!transaction) return [];
    if (!session || session.generation !== transaction.baseGeneration) {
      throw new Error('Consumable effect state changed after export preflight.');
    }
    var guarded = validateGuards(candidateZ64, session, session.guardManifest || GUARD_MANIFEST);
    if (!guarded.ok) {
      throw new Error('Candidate guard changed before the atomic effect write:\n  ' + guarded.errors.join('\n  '));
    }
    for (var i = 0; i < transaction.writes.length; i++) {
      var check = transaction.writes[i];
      if (readU32(candidateZ64, check.offset) !== (check.beforeWord >>> 0)) {
        throw new Error('Candidate word at ' + hex(check.offset, 6) +
          ' changed after preflight; no effect word was written.');
      }
    }
    for (var w = 0; w < transaction.writes.length; w++) {
      writeU32(candidateZ64, transaction.writes[w].offset, transaction.writes[w].afterWord);
    }
    transaction.applied = true;
    return transaction.writes.slice();
  }

  function commitTransaction(session, transaction, provenance) {
    if (!transaction) return;
    if (session.generation !== transaction.baseGeneration) {
      throw new Error('Cannot commit a stale consumable effect transaction.');
    }
    if (transaction.writes.length && !transaction.applied) {
      throw new Error('Cannot commit consumable effects before applying the prepared transaction.');
    }
    session.ledger.currentWords = Object.assign({}, transaction.desiredWords);
    session.ledger.effectOwnedWrites = transaction.writes.map(function(write) {
      return {
        offset: write.offset,
        beforeWord: write.beforeWord,
        afterWord: write.afterWord,
        modelKey: write.modelKey,
        role: write.role
      };
    });
    if (provenance && provenance.changeRanges) {
      session.ledger.priorOwnerRegions = provenance.changeRanges.map(function(range) {
        return {
          id: range.ownerId,
          name: range.owner,
          category: range.category,
          regions: [{ kind: 'rom', start: range.start, size: range.length, label: 'prior editor-owned candidate range' }]
        };
      });
      session.ledger.lastChangeRanges = provenance.changeRanges.map(function(range) {
        return Object.assign({}, range);
      });
      session.ledger.headerCrcWrites = (provenance.headerCrcDelta || []).map(function(range) {
        return Object.assign({}, range);
      });
      session.ledger.lastCandidate = provenance.candidate
        ? Object.assign({}, provenance.candidate)
        : null;
    }
    session.ledger.exports.push({
      candidateSha256: provenance && provenance.candidate && provenance.candidate.sha256 || '',
      candidateFilename: provenance && provenance.candidate && provenance.candidate.filename || '',
      effectWrites: transaction.writes.length,
      headerCrcWrites: provenance && provenance.headerCrcDelta
        ? provenance.headerCrcDelta.length
        : 0,
      changeRanges: provenance && provenance.changeRanges
        ? provenance.changeRanges.length
        : 0
    });
    refreshPending(session);
  }

  // Independent CIC-6102 computation. This verifier never calls the editor's
  // result-returning CRC helper and never trusts the header bytes as its input.
  function computeIndependentCrc(z64) {
    if (!z64 || z64.length < 0x101000) throw new Error('Candidate is too small for CIC-6102 verification.');
    var seed = 0xF8CA4DDC >>> 0;
    var carryAccumulator = seed;
    var comparisonAccumulator = seed;
    var xorAccumulator = seed;
    var carryCount = seed;
    var rotateAccumulator = seed;
    var mixedAccumulator = seed;
    for (var offset = 0x1000; offset < 0x101000; offset += 4) {
      var data = readU32(z64, offset) >>> 0;
      var sum = (mixedAccumulator + data) >>> 0;
      if (sum < mixedAccumulator) carryCount = (carryCount + 1) >>> 0;
      mixedAccumulator = sum;
      xorAccumulator = (xorAccumulator ^ data) >>> 0;
      var amount = data & 31;
      var rotated = amount === 0 ? data : ((data << amount) | (data >>> (32 - amount))) >>> 0;
      rotateAccumulator = (rotateAccumulator + rotated) >>> 0;
      comparisonAccumulator = comparisonAccumulator > data
        ? (comparisonAccumulator ^ rotated) >>> 0
        : (comparisonAccumulator ^ mixedAccumulator ^ data) >>> 0;
      carryAccumulator = (carryAccumulator + ((rotateAccumulator ^ data) >>> 0)) >>> 0;
    }
    return {
      crc1: (mixedAccumulator ^ carryCount ^ xorAccumulator) >>> 0,
      crc2: (rotateAccumulator ^ comparisonAccumulator ^ carryAccumulator) >>> 0
    };
  }

  function verifyIndependentCrc(z64) {
    var computed = computeIndependentCrc(z64);
    var header = { crc1: readU32(z64, 0x10), crc2: readU32(z64, 0x14) };
    return {
      ok: computed.crc1 === header.crc1 && computed.crc2 === header.crc2,
      computed: computed,
      header: header
    };
  }

  function ownersAt(offset, ownerRegions) {
    var matches = [];
    for (var i = 0; i < ownerRegions.length; i++) {
      var region = ownerRegions[i];
      if (region.kind === 'rom' && offset >= region.start && offset < region.end) matches.push(region);
    }
    return matches;
  }

  function concreteOwnerAt(offset, ownerRegions) {
    var matches = ownersAt(offset, ownerRegions);
    if (!matches.length) return null;
    var owners = {};
    for (var i = 0; i < matches.length; i++) owners[matches[i].ownerId] = matches[i];
    var ownerIds = Object.keys(owners);
    if (ownerIds.length > 1) {
      throw new Error('Concrete patch owner collision at normalized z64 ' +
        '[' + hex(offset, 8) + ',' + hex(offset + 1, 8) + '): ' +
        ownerIds.map(function(id) {
          var region = owners[id];
          return region.ownerName + ' (' + region.label + ', [' +
            hex(region.start, 8) + ',' + hex(region.end, 8) + '))';
        }).join(' overlaps '));
    }
    matches.sort(function(a, b) {
      return a.size - b.size;
    });
    return matches[0];
  }

  function buildChangeRanges(sourceZ64, candidateZ64, owners) {
    if (!sourceZ64 || !candidateZ64 || sourceZ64.length !== candidateZ64.length) {
      throw new Error('Source and candidate must have equal normalized lengths.');
    }
    var allOwners = (owners || []).slice();
    for (var suppliedOwnerIndex = 0;
        suppliedOwnerIndex < allOwners.length;
        suppliedOwnerIndex++) {
      if (allOwners[suppliedOwnerIndex].id === 'consumable-effects-guard-collision') {
        throw new Error(
          'The broad consumable-effect guard/collision surface cannot own candidate deltas.'
        );
      }
    }
    allOwners.push({
      id: 'header-crc',
      name: 'CIC-6102 Header CRC',
      category: 'crc',
      regions: [{ kind: 'rom', start: 0x10, size: 8, label: 'CRC1/CRC2 header words' }]
    });
    var regions = normalizedOwnerRegions(allOwners);
    var out = [];
    var start = -1;
    var currentOwner = null;
    function close(end) {
      if (start < 0) return;
      var before = sourceZ64.subarray(start, end);
      var after = candidateZ64.subarray(start, end);
      var exact = currentOwner.ownerId === 'consumable-effects' || currentOwner.ownerId === 'header-crc';
      out.push({
        ownerId: currentOwner.ownerId,
        owner: currentOwner.ownerName,
        category: currentOwner.category,
        label: currentOwner.label,
        start: start,
        end: end,
        length: end - start,
        beforeSha256: sha256HexSync(before),
        afterSha256: sha256HexSync(after),
        beforeBytes: exact ? bytesToHex(before) : undefined,
        afterBytes: exact ? bytesToHex(after) : undefined
      });
      start = -1;
      currentOwner = null;
    }
    for (var i = 0; i < sourceZ64.length; i++) {
      if (sourceZ64[i] === candidateZ64[i]) {
        close(i);
        continue;
      }
      var found = concreteOwnerAt(i, regions);
      if (!found) {
        var unmatchedEnd = i + 1;
        while (unmatchedEnd < sourceZ64.length &&
            sourceZ64[unmatchedEnd] !== candidateZ64[unmatchedEnd] &&
            !ownersAt(unmatchedEnd, regions).length) {
          unmatchedEnd++;
        }
        throw new Error('Unexplained candidate delta at normalized z64 [' +
          hex(i, 8) + ',' + hex(unmatchedEnd, 8) +
          '): no concrete subsystem owner covers this changed range.');
      }
      var signature = found.ownerId + '|' + found.category + '|' + found.label;
      var previousSignature = currentOwner
        ? currentOwner.ownerId + '|' + currentOwner.category + '|' + currentOwner.label
        : '';
      if (start >= 0 && signature !== previousSignature) close(i);
      if (start < 0) {
        start = i;
        currentOwner = found;
      }
    }
    close(sourceZ64.length);
    return out;
  }

  function itemName(rom, id) {
    var record = rom && rom.consumables && rom.consumables[id];
    return record && record.name
      ? record.name
      : (OB64.consumableName ? OB64.consumableName(id) : 'Consumable ' + id);
  }

  function modelProvenance(rom, transaction) {
    return transaction.modelChanges.map(function(change) {
      return {
        key: change.projectKey,
        model: change.modelKey,
        affectedItems: change.itemIds.map(function(id) {
          return { id: id, name: itemName(rom, id), target: SHARED_TARGETS[id] || MODEL_DEFS[change.modelKey].target };
        }),
        requested: { minimum: change.minimum, maximum: change.maximum, width: change.width },
        previousWords: change.previousWords,
        candidateWords: change.candidateWords
      };
    });
  }

  function profileForModels(models) {
    var changed = MODEL_ORDER.filter(function(key) { return modelDiffersFromVanilla(key, models[key]); });
    if (!changed.length) return 'V';
    if (changed.length !== 1) return 'custom';
    var key = changed[0], value = models[key];
    if (value.minimum !== value.maximum) return 'custom';
    if (key === 'cupOfLife' && value.minimum === 7) return 'C';
    if (key === 'sharedStatBoosters11To16' && value.minimum === 7) return 'B';
    if (key === 'scrollOfDiscipline' && value.minimum === 7) return 'S';
    if (key === 'urnOfChaos' && value.minimum === -7) return 'U';
    if (key === 'gobletOfDestiny' && value.minimum === -7) return 'G-';
    if (key === 'gobletOfDestiny' && value.minimum === 0) return 'G0';
    if (key === 'gobletOfDestiny' && value.minimum === 7) return 'G+';
    return 'custom';
  }

  function normalizedHeaderIdentity(z64) {
    return {
      imageName: headerText(z64, 0x20, 20).trim(),
      gameId: headerText(z64, 0x3B, 4),
      country: z64[0x3E],
      version: z64[0x3F],
      crc1: hex(readU32(z64, 0x10)),
      crc2: hex(readU32(z64, 0x14))
    };
  }

  function dirtyCategoryList(dirty) {
    return Object.keys(dirty || {}).filter(function(key) { return !!dirty[key]; }).sort();
  }

  function buildProvenance(rom, session, transaction, candidateZ64, candidateBytes,
      candidateFilename, owners, dirtySnapshot) {
    var crc = verifyIndependentCrc(candidateZ64);
    if (!crc.ok) throw new Error('Independent CIC-6102 verification failed before download.');
    var allOwners = (owners || []).slice();
    allOwners.push(assertEffectDeltaOwner(transaction.deltaOwner));
    var ranges = buildChangeRanges(session.baselineZ64, candidateZ64, allOwners);
    var effectDelta = ranges.filter(function(range) { return range.ownerId === 'consumable-effects'; });
    var headerDelta = ranges.filter(function(range) { return range.ownerId === 'header-crc'; });
    return sha256Hex(candidateBytes).then(function(candidateHash) {
      return {
        schema: 'ob64-consumable-effects-provenance',
        version: 1,
        generatedAt: new Date().toISOString(),
        source: {
          filename: session.source.filename,
          size: session.source.size,
          sha256: session.source.sha256,
          byteOrder: session.source.byteOrder,
          normalizedHeader: session.source.header
        },
        candidate: {
          filename: candidateFilename,
          size: candidateBytes.length,
          sha256: candidateHash,
          byteOrder: rom.exportByteOrder || rom.byteOrder || 'v64',
          normalizedHeader: normalizedHeaderIdentity(candidateZ64)
        },
        profile: profileForModels(transaction.models),
        models: modelProvenance(rom, transaction),
        effectOwnedDelta: effectDelta,
        headerCrcDelta: headerDelta,
        changeRanges: ranges,
        dirtyCategories: dirtyCategoryList(dirtySnapshot),
        independentCrc: {
          ok: crc.ok,
          computedCrc1: hex(crc.computed.crc1),
          computedCrc2: hex(crc.computed.crc2),
          headerCrc1: hex(crc.header.crc1),
          headerCrc2: hex(crc.header.crc2)
        },
        expectedLoadedWords: expectedLoadedWordsForCandidate(candidateZ64),
        dispatchTable: {
          normalizedSource: '[0x00065D60,0x00065DAC)',
          requiredLiveInterval: '[0x8018FE60,0x8018FEAC)',
          lastLiveWord: '0x8018FEA8',
          sha256: GUARD_MANIFEST.dispatchTable.sha256
        },
        editorVersion: EDITOR_VERSION,
        projectFormatVersion: PROJECT_VERSION,
        lockedItems: {
          count: 34,
          ids: lockedIds(),
          contributedEffectWrites: false,
          statement: 'Disabled IDs contributed no consumable-effect write.'
        }
      };
    });
  }

  function prepareOrdinaryExport(rom, session, candidateZ64, candidateBytes,
      candidateFilename, owners) {
    assertSessionOwnership(session, session.guardManifest || GUARD_MANIFEST);
    var baseGeneration = session.generation;
    var ranges = buildChangeRanges(session.baselineZ64, candidateZ64, owners || []);
    return sha256Hex(candidateBytes).then(function(candidateHash) {
      if (session.generation !== baseGeneration) {
        throw new Error('Consumable effect state changed while the ordinary candidate was being verified.');
      }
      var candidate = {
        filename: candidateFilename,
        size: candidateBytes.length,
        sha256: candidateHash,
        byteOrder: rom.exportByteOrder || rom.byteOrder || 'v64',
        normalizedHeader: normalizedHeaderIdentity(candidateZ64)
      };
      return {
        baseGeneration: baseGeneration,
        candidate: candidate,
        ranges: ranges
      };
    });
  }

  function commitOrdinaryExport(session, prepared) {
    if (!prepared || session.generation !== prepared.baseGeneration) {
      throw new Error('Cannot commit a stale ordinary export ledger.');
    }
    var ranges = prepared.ranges || [];
    var candidate = prepared.candidate || {};
    session.ledger.priorOwnerRegions = ranges.map(function(range) {
      return {
        id: range.ownerId,
        name: range.owner,
        category: range.category,
        regions: [{
          kind: 'rom',
          start: range.start,
          size: range.length,
          label: 'prior editor-owned candidate range'
        }]
      };
    });
    session.ledger.lastChangeRanges = ranges.map(function(range) {
      return Object.assign({}, range);
    });
    session.ledger.headerCrcWrites = ranges.filter(function(range) {
      return range.ownerId === 'header-crc';
    }).map(function(range) {
      return Object.assign({}, range);
    });
    session.ledger.lastCandidate = Object.assign({}, candidate);
    session.ledger.exports.push({
      candidateSha256: candidate.sha256 || '',
      candidateFilename: candidate.filename || '',
      effectWrites: 0,
      headerCrcWrites: session.ledger.headerCrcWrites.length,
      changeRanges: ranges.length
    });
  }

  function expectedLoadedWordsForCandidate(z64) {
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var def = MODEL_DEFS[key];
      out[key] = {
        widthAddress: EXPECTED_LOADED_WORDS[key].width,
        widthWord: hex(readU32(z64, def.widthOffset)),
        minimumAddress: EXPECTED_LOADED_WORDS[key].minimum,
        minimumWord: hex(readU32(z64, def.minimumOffset))
      };
    }
    out.finalStore = { address: EXPECTED_LOADED_WORDS.finalStore, word: hex(readU32(z64, 0x415E4)) };
    return out;
  }

  function serializeCandidate(rom) {
    var order = rom.exportByteOrder || rom.byteOrder || 'v64';
    if (OB64.serializeRomImage) return OB64.serializeRomImage(rom.z64, order);
    if (order === 'v64') {
      var out = new Uint8Array(rom.z64.length);
      for (var i = 0; i + 1 < rom.z64.length; i += 2) {
        out[i] = rom.z64[i + 1];
        out[i + 1] = rom.z64[i];
      }
      return out;
    }
    return rom.z64.slice();
  }

  function downloadBytes(bytes, filename, mime) {
    var blob = new Blob([bytes], { type: mime || 'application/octet-stream' });
    var url = URL.createObjectURL(blob);
    var anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = filename;
    document.body.appendChild(anchor);
    anchor.click();
    document.body.removeChild(anchor);
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    return filename;
  }

  function downloadRomCandidate(candidateBytes, candidateFilename) {
    if (!(candidateBytes instanceof Uint8Array)) {
      throw new Error('Verified ROM candidate bytes are required for download.');
    }
    if (!candidateFilename) throw new Error('Verified ROM candidate filename is required for download.');
    downloadBytes(candidateBytes, candidateFilename, 'application/octet-stream');
    return {
      candidateFilename: candidateFilename,
      candidateBytes: candidateBytes
    };
  }

  function lockedIds() {
    // Preserve the complete effect-domain nonparticipation ledger even though
    // Joe intentionally removed quest/story IDs 32-44 from the visible tab.
    var out = [];
    for (var id = 1; id <= 44; id++) if (id < 10 || id > 19) out.push(id);
    return out;
  }

  function catalogDisposition(id) {
    if (id === 1) return { effect: 'Restores current HP to one target.', value: '100', reason: 'Healing also reaches an unresolved special HP pool; no isolated amount control is accepted.', category: 'Consumable' };
    if (id === 2) return { effect: 'Restores current HP to one target.', value: '300', reason: 'Healing also reaches an unresolved special HP pool; no isolated amount control is accepted.', category: 'Consumable' };
    if (id === 3) return { effect: 'Restores current HP across the group.', value: '150', reason: 'Group healing and unresolved special-pool behavior are coupled.', category: 'Consumable' };
    if (id === 4) return { effect: 'Reduces group fatigue.', value: '20', reason: 'The complete mixed-group fatigue matrix is not accepted.', category: 'Consumable' };
    if (id === 5) return { effect: 'Reduces group fatigue.', value: '50', reason: 'The complete mixed-group fatigue matrix is not accepted.', category: 'Consumable' };
    if (id === 6) return { effect: 'Clears a status bit.', value: '\u2014', reason: 'Bit-clear behavior has no safe local numeric amount.', category: 'Consumable' };
    if (id === 7) return { effect: 'Restores current HP from the target maximum and clears a status bit.', value: '\u2014', reason: 'Derived maximum-HP behavior has no safe local numeric amount.', category: 'Consumable' };
    if (id === 8) return { effect: 'Menu/system route.', value: '\u2014', reason: 'Quit Gate is a menu/system route with no helper effect.', category: 'Menu/System' };
    if (id === 9) return { effect: 'Generated level-up trigger.', value: '1', reason: 'Champion invokes broadly coupled level-up logic rather than an editable magnitude.', category: 'Consumable' };
    if (id === 10) return { effect: 'Adds to maximum HP; current HP is unchanged.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id >= 11 && id <= 16) return { effect: 'Adds to ' + SHARED_TARGETS[id].split(' ')[0] + '.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable', shared: true };
    if (id === 17) return { effect: 'Raises Alignment.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 18) return { effect: 'Lowers Alignment.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 19) return { effect: 'Adjusts Luck; a no-op can still consume.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 20) return { effect: 'Raises persistent squad Unity.', value: '+1..+5', reason: 'Flag width uses compiled reciprocal arithmetic and Unity is system-coupled.', category: 'Consumable' };
    if (id >= 21 && id <= 31) return { effect: 'Unresolved effect consumer and semantics.', value: '\u2014', reason: 'Unresolved: no safe numeric effect control is established.', category: 'Unresolved' };
    return { effect: 'Quest/story record.', value: '\u2014', reason: 'Quest/story records have no established numeric effect control.', category: 'Quest/Story' };
  }

  function buildCatalog(rom, session) {
    var rows = [];
    var eligible = !!(session && session.identity && session.identity.eligible);
    for (var id = 1; id <= CATALOG_MAX_ID; id++) {
      var disposition = catalogDisposition(id);
      var name = itemName(rom, id);
      var iconUrl = OB64.itemIconURL ? OB64.itemIconURL(name) : null;
      var editable = eligible && !!disposition.modelKey;
      var reason = editable ? '' : (disposition.modelKey
        ? session && session.identity && session.identity.reason ||
          'Load the exact verified US rev0 source to edit this effect.'
        : disposition.reason);
      if (!eligible && !disposition.modelKey) {
        reason += ' Effect editing is also unavailable for this source: ' +
          (session && session.identity && session.identity.reason ||
            'load the exact verified US rev0 source.');
      }
      rows.push({
        id: id,
        name: name,
        iconUrl: iconUrl,
        effect: disposition.effect,
        category: disposition.category,
        modelKey: disposition.modelKey || null,
        shared: !!disposition.shared,
        editable: editable,
        value: disposition.value,
        reason: reason
      });
    }
    return rows;
  }

  function createElement(doc, tag, className, text) {
    var element = doc.createElement(tag);
    if (className) element.className = className;
    if (text != null) element.textContent = text;
    return element;
  }

  function setData(element, key, value) {
    if (element.dataset) element.dataset[key] = String(value);
    else element.setAttribute('data-' + key.replace(/[A-Z]/g, function(letter) {
      return '-' + letter.toLowerCase();
    }), String(value));
  }

  function queryAll(rootElement, selector) {
    return rootElement && rootElement.querySelectorAll
      ? Array.prototype.slice.call(rootElement.querySelectorAll(selector))
      : [];
  }

  function formatSigned(value) {
    return value > 0 ? '+' + value : String(value);
  }

  function syncModelViews(panel, session, modelKey) {
    var model = session.models[modelKey];
    var inputs = queryAll(panel, '[data-effect-model="' + modelKey + '"]');
    for (var i = 0; i < inputs.length; i++) {
      var role = inputs[i].getAttribute('data-effect-role');
      inputs[i].value = role === 'minimum' ? String(model.minimum) : String(model.maximum);
      inputs[i].setAttribute('aria-invalid', 'false');
    }
    var derived = queryAll(panel, '[data-effect-derived="' + modelKey + '"]');
    var width = model.maximum - model.minimum + 1;
    for (var d = 0; d < derived.length; d++) {
      derived[d].textContent = 'Width ' + width + ' \u00b7 reachable ' +
        formatSigned(model.minimum) + '..' + formatSigned(model.maximum) + ' inclusive';
    }
    var errors = queryAll(panel, '[data-effect-error="' + modelKey + '"]');
    for (var e = 0; e < errors.length; e++) {
      errors[e].textContent = '';
      errors[e].hidden = true;
    }
  }

  function announceShared(panel, message, moveFocus) {
    var notice = panel.querySelector && panel.querySelector('[data-shared-effect-notice]');
    if (!notice) return;
    notice.textContent = message;
    if (moveFocus && typeof notice.focus === 'function') {
      try { notice.focus({ preventScroll: true }); } catch (err) { notice.focus(); }
    }
  }

  function renderEditableValue(doc, panel, row, session, options) {
    var def = MODEL_DEFS[row.modelKey];
    var valueCell = createElement(doc, 'div', 'consumable-effect-value');
    var controls = createElement(doc, 'div', 'consumable-effect-range');
    var suffixes = ['minimum', 'maximum'];
    for (var i = 0; i < suffixes.length; i++) {
      var role = suffixes[i];
      var label = createElement(doc, 'label', 'consumable-effect-number');
      var inputId = 'consumable-effect-' + row.id + '-' + role;
      label.setAttribute('for', inputId);
      label.appendChild(createElement(doc, 'span', '', role === 'minimum' ? 'Minimum' : 'Maximum'));
      var input = createElement(doc, 'input');
      input.type = 'number';
      input.step = '1';
      input.id = inputId;
      input.min = String(def.domainMin);
      input.max = String(def.domainMax);
      input.value = String(role === 'minimum' ? session.models[row.modelKey].minimum : session.models[row.modelKey].maximum);
      input.setAttribute('inputmode', 'numeric');
      input.setAttribute('data-effect-model', row.modelKey);
      input.setAttribute('data-effect-role', role);
      input.setAttribute('aria-describedby', 'consumable-effect-help-' + row.id + ' consumable-effect-error-' + row.id);
      label.appendChild(input);
      controls.appendChild(label);
    }
    valueCell.appendChild(controls);
    var derived = createElement(doc, 'div', 'consumable-effect-derived');
    derived.setAttribute('data-effect-derived', row.modelKey);
    derived.id = 'consumable-effect-help-' + row.id;
    valueCell.appendChild(derived);
    var retail = createElement(doc, 'div', 'consumable-effect-retail', 'Retail: ' + def.retail);
    valueCell.appendChild(retail);
    var error = createElement(doc, 'div', 'consumable-effect-error');
    error.id = 'consumable-effect-error-' + row.id;
    error.setAttribute('data-effect-error', row.modelKey);
    error.setAttribute('role', 'alert');
    error.hidden = true;
    valueCell.appendChild(error);
    var reset = createElement(doc, 'button', 'consumable-effect-reset', 'Reset to retail');
    reset.type = 'button';
    reset.setAttribute('data-effect-reset', String(row.id));
    reset.addEventListener('click', function() {
      resetItem(session, row.id);
      syncModelViews(panel, session, row.modelKey);
      if (row.shared) {
        announceShared(panel, 'Shared stat booster range reset from ID ' + row.id +
          '. All six rows now use retail ' + def.retail + '.', true);
      }
      if (options && options.onChange) options.onChange(session.pendingWrites, session);
    });
    valueCell.appendChild(reset);

    var rangeInputs = queryAll(valueCell, '[data-effect-model="' + row.modelKey + '"]');
    function applyInput(event) {
      var minimumInput = valueCell.querySelector('[data-effect-role="minimum"]');
      var maximumInput = valueCell.querySelector('[data-effect-role="maximum"]');
      var minimum = Number(minimumInput.value);
      var maximum = Number(maximumInput.value);
      try {
        if (minimumInput.value.trim() === '' || maximumInput.value.trim() === '') {
          throw new Error('Minimum and Maximum are required integers.');
        }
        setItemRange(session, row.id, minimum, maximum);
        syncModelViews(panel, session, row.modelKey);
        if (row.shared) {
          announceShared(panel, 'Shared stat booster range updated from ID ' + row.id +
            '. IDs 11 through 16 now use ' + formatSigned(minimum) + '..' +
            formatSigned(maximum) + '.', event && event.type === 'change');
        }
        if (options && options.onChange) options.onChange(session.pendingWrites, session);
      } catch (err) {
        minimumInput.setAttribute('aria-invalid', 'true');
        maximumInput.setAttribute('aria-invalid', 'true');
        error.textContent = err.message;
        error.hidden = false;
      }
    }
    for (var ri = 0; ri < rangeInputs.length; ri++) {
      rangeInputs[ri].addEventListener('input', applyInput);
      rangeInputs[ri].addEventListener('change', applyInput);
    }
    return valueCell;
  }

  function renderLockedValue(doc, row) {
    var valueCell = createElement(doc, 'div', 'consumable-effect-value consumable-effect-value-locked');
    var input = createElement(doc, 'input', 'consumable-effect-locked-input');
    input.type = 'text';
    input.value = row.value == null ? '\u2014' : row.value;
    input.disabled = true;
    input.setAttribute('aria-label', 'Effect Value unavailable for ' + row.name);
    input.setAttribute('aria-describedby', 'consumable-effect-reason-' + row.id);
    valueCell.appendChild(input);
    return valueCell;
  }

  function applyFilter(panel) {
    var search = panel.querySelector('[data-consumable-filter]');
    var status = panel.querySelector('[data-consumable-status-filter]');
    var query = search ? String(search.value || '').trim().toLowerCase() : '';
    var statusValue = status ? status.value : 'all';
    var rows = queryAll(panel, '[data-consumable-row]');
    for (var i = 0; i < rows.length; i++) {
      var haystack = (rows[i].getAttribute('data-search') || '').toLowerCase();
      var state = rows[i].getAttribute('data-availability');
      rows[i].hidden = !!(query && haystack.indexOf(query) === -1) ||
        (statusValue !== 'all' && state !== statusValue);
    }
  }

  function render(panel, rom, options) {
    if (!panel || !rom) return;
    var session = sessionFor(rom);
    var doc = panel.ownerDocument || (typeof document !== 'undefined' ? document : null);
    if (!doc) throw new Error('Consumables tab rendering requires a document.');
    panel.innerHTML = '';
    panel.classList.add('consumables-panel');

    var shell = createElement(doc, 'div', 'consumables-shell');
    var heading = createElement(doc, 'div', 'consumables-heading');
    var copy = createElement(doc, 'div');
    copy.appendChild(createElement(doc, 'h2', '', 'Consumables'));
    copy.appendChild(createElement(doc, 'p', '',
      'Consumable records 1\u201331. Supported effect ranges are staged for the next combined ROM export.'));
    heading.appendChild(copy);
    shell.appendChild(heading);

    var compatibility = createElement(doc, 'div',
      'consumable-compatibility ' + (session && session.identity && session.identity.eligible ? 'is-supported' : 'is-unavailable'));
    compatibility.setAttribute('role', 'status');
    compatibility.setAttribute('aria-live', 'polite');
    compatibility.textContent = session && session.identity && session.identity.eligible
      ? 'Effect editing available: exact verified US rev0 source and all guards match.'
      : 'Effect editing unavailable: ' + (session && session.identity && session.identity.reason ||
        'load the exact verified US rev0 source.');
    shell.appendChild(compatibility);

    var sharedNotice = createElement(doc, 'div', 'consumable-shared-notice',
      'Shared stat boosters: every edit to IDs 11\u201316 changes all six rows.');
    sharedNotice.setAttribute('data-shared-effect-notice', '');
    sharedNotice.setAttribute('role', 'status');
    sharedNotice.setAttribute('aria-live', 'polite');
    sharedNotice.setAttribute('tabindex', '-1');
    shell.appendChild(sharedNotice);

    var filters = createElement(doc, 'div', 'consumable-filters');
    var searchLabel = createElement(doc, 'label');
    searchLabel.setAttribute('for', 'consumable-filter');
    searchLabel.appendChild(createElement(doc, 'span', '', 'Filter by name or ID'));
    var search = createElement(doc, 'input');
    search.type = 'search';
    search.id = 'consumable-filter';
    search.placeholder = 'e.g. Scroll or 17';
    search.setAttribute('data-consumable-filter', '');
    searchLabel.appendChild(search);
    filters.appendChild(searchLabel);
    var statusLabel = createElement(doc, 'label');
    statusLabel.setAttribute('for', 'consumable-status-filter');
    statusLabel.appendChild(createElement(doc, 'span', '', 'Availability'));
    var status = createElement(doc, 'select');
    status.id = 'consumable-status-filter';
    status.setAttribute('data-consumable-status-filter', '');
    [['all', 'All 31 items'], ['editable', 'Editable'], ['unavailable', 'Unavailable']].forEach(function(pair) {
      var option = createElement(doc, 'option', '', pair[1]);
      option.value = pair[0];
      status.appendChild(option);
    });
    statusLabel.appendChild(status);
    filters.appendChild(statusLabel);
    shell.appendChild(filters);

    var list = createElement(doc, 'div', 'consumable-list');
    list.setAttribute('role', 'list');
    var catalog = buildCatalog(rom, session);
    for (var i = 0; i < catalog.length; i++) {
      (function(row) {
        var card = createElement(doc, 'article', 'consumable-row' + (row.editable ? ' is-editable' : ' is-locked'));
        card.setAttribute('role', 'listitem');
        card.setAttribute('data-consumable-row', '');
        card.setAttribute('data-item-id', String(row.id));
        card.setAttribute('data-availability', row.editable ? 'editable' : 'unavailable');
        card.setAttribute('data-search', row.id + ' #' + row.id + ' ' + row.name + ' ' + row.category);

        var identity = createElement(doc, 'div', 'consumable-identity');
        var iconWrap = createElement(doc, 'div', 'consumable-icon-wrap');
        var image = createElement(doc, 'img', 'consumable-icon');
        image.alt = '';
        image.loading = 'lazy';
        if (row.iconUrl) image.src = row.iconUrl;
        var fallback = createElement(doc, 'span', 'consumable-icon-fallback', '\u25c7');
        fallback.hidden = true;
        fallback.setAttribute('aria-hidden', 'true');
        image.addEventListener('error', function() {
          image.hidden = true;
          fallback.hidden = false;
          card.classList.add('icon-missing');
        });
        iconWrap.appendChild(image);
        iconWrap.appendChild(fallback);
        identity.appendChild(iconWrap);
        var nameBlock = createElement(doc, 'div');
        nameBlock.appendChild(createElement(doc, 'div', 'consumable-id', 'ID ' + row.id));
        nameBlock.appendChild(createElement(doc, 'h3', 'consumable-name', row.name));
        nameBlock.appendChild(createElement(doc, 'span', 'consumable-category', row.category));
        identity.appendChild(nameBlock);
        card.appendChild(identity);

        var effect = createElement(doc, 'div', 'consumable-effect-summary', row.effect);
        if (row.shared) {
          effect.appendChild(createElement(doc, 'div', 'consumable-shared-badge',
            'Shared: changes IDs 11\u201316'));
        }
        card.appendChild(effect);

        card.appendChild(row.editable
          ? renderEditableValue(doc, panel, row, session, options)
          : renderLockedValue(doc, row));

        var availability = createElement(doc, 'div', 'consumable-availability');
        var badge = createElement(doc, 'span', 'consumable-availability-badge',
          row.editable ? 'Editable' : 'Unavailable');
        availability.appendChild(badge);
        if (!row.editable) {
          var reason = createElement(doc, 'p', 'consumable-lock-reason', row.reason);
          reason.id = 'consumable-effect-reason-' + row.id;
          availability.appendChild(reason);
        } else {
          availability.appendChild(createElement(doc, 'p', 'consumable-ready-reason',
            row.shared ? 'One canonical shared model and byte pair.' : 'Verified local range model.'));
        }
        card.appendChild(availability);
        list.appendChild(card);
      })(catalog[i]);
    }
    shell.appendChild(list);
    panel.appendChild(shell);

    for (var modelIndex = 0; modelIndex < MODEL_ORDER.length; modelIndex++) {
      syncModelViews(panel, session, MODEL_ORDER[modelIndex]);
    }
    search.addEventListener('input', function() { applyFilter(panel); });
    status.addEventListener('change', function() { applyFilter(panel); });
    return catalog;
  }

  function consumableChangedByteRegions(rom) {
    var regions = [];
    if (!rom || !rom.consumables || !rom.original || !rom.original.consumables) return regions;
    for (var i = 0; i < rom.consumables.length; i++) {
      var current = rom.consumables[i];
      var original = rom.original.consumables[i];
      if (!current || !original) continue;
      var base = current.romOffset != null ? current.romOffset :
        ((OB64.CONSUMABLE_TABLE_OFFSET || 0x645CC) + i * 12);
      if (current.flagHi !== original.flagHi) regions.push({ kind: 'rom', start: base + 4, size: 2, label: 'ID ' + i + ' flagHi' });
      if (current.price !== original.price) regions.push({ kind: 'rom', start: base + 6, size: 2, label: 'ID ' + i + ' price' });
      var currentFlags = current.flagLo || [];
      var originalFlags = original.flagLo || [];
      for (var b = 0; b < 4; b++) {
        if (currentFlags[b] !== originalFlags[b]) {
          regions.push({ kind: 'rom', start: base + 8 + b, size: 1, label: 'ID ' + i + ' behavior byte ' + b });
        }
      }
    }
    return regions;
  }

  function byteLength(value) {
    if (typeof value === 'string') return Math.floor(value.replace(/\s+/g, '').length / 2);
    return value && typeof value.length === 'number' ? value.length : 0;
  }

  function toolWriteRegions(feature, currentState) {
    var regions = OB64.tools.featureRegions(feature).map(function(region) {
      return { kind: region.kind, start: region.start, size: region.size, label: region.label };
    });
    if (currentState === 'outdated') {
      var superseded = feature.superseded || [];
      for (var s = 0; s < superseded.length; s++) {
        var writes = superseded[s].writes || [];
        for (var w = 0; w < writes.length; w++) {
          var size = Math.max(byteLength(writes[w].patched), byteLength(writes[w].original));
          if (size > 0) {
            regions.push({
              kind: 'rom',
              start: writes[w].offset,
              size: size,
              label: (writes[w].label || ('superseded write ' + w)) + ' restoration'
            });
          }
        }
      }
    }
    return regions;
  }

  function scenarioPatchOwners(rom) {
    if (!rom || !rom.scenarioEditor) return [];
    var state = rom.scenarioEditor;
    var archiveIds = {};
    function claimArchive(value) {
      var archive = Number(value);
      if (Number.isInteger(archive) && archive >= 0) archiveIds[archive] = true;
    }
    Object.keys(state.slotOwnedArchives || {}).forEach(claimArchive);
    Object.keys(state.modifiedKeys || {}).forEach(function(key) {
      if (!state.modifiedKeys[key]) return;
      var meta = (state.metadata || {})[key];
      if (meta && meta.archive != null) claimArchive(meta.archive);
    });
    Object.keys(state.modifiedTreasureArchives || {}).forEach(function(key) {
      if (state.modifiedTreasureArchives[key]) claimArchive(key);
    });
    Object.keys(state.siteAllegiances || {}).forEach(function(key) {
      var intents = state.siteAllegiances[key] || {};
      if (!Object.keys(intents).length) return;
      (state.sites && state.sites[key] || []).forEach(function(site) {
        if (!Object.prototype.hasOwnProperty.call(intents, String(site.selector))) return;
        var descriptor = site && site.siteDescriptor;
        if (descriptor && descriptor.scincsvArchive != null) claimArchive(descriptor.scincsvArchive);
      });
    });
    if (Object.keys(state.strongholdFields || {}).length ||
        (state.slotOwnedArchives && state.slotOwnedArchives[691])) {
      claimArchive(691);
    }
    var archiveRegions = Object.keys(archiveIds).sort(function(a, b) {
      return Number(a) - Number(b);
    }).map(function(key) {
      var archive = rom.archives && rom.archives[Number(key)];
      if (!archive) return null;
      return {
        kind: 'rom',
        start: archive.offset,
        size: (archive.totalHeaderSize || 0) + (archive.compSize || 0),
        label: 'scenario archive #' + key + ' fixed slot'
      };
    }).filter(Boolean);
    var owners = [];
    if (archiveRegions.length) {
      owners.push({
        id: 'scenario-archives',
        name: 'Scenario Archives',
        category: 'scenario',
        regions: archiveRegions
      });
    }
    var relocationRegions = [];
    var existingRelocations = rom.scenarioRelocations || [];
    var ownedWindows = state.relocationOwnedWindows || [];
    if ((existingRelocations.length || ownedWindows.length) &&
        OB64.scenario && OB64.scenario.patchRegions) {
      relocationRegions = OB64.scenario.patchRegions(rom.scenarioRelocations || []);
    }
    ownedWindows.forEach(function(window, index) {
      relocationRegions.push({
        kind: 'rom',
        start: window.tailDmaStart,
        size: window.windowSize,
        label: 'prior scenario relocation tail restoration ' + (index + 1)
      });
    });
    if (relocationRegions.length) {
      owners.push({
        id: 'scenario-eset-relocation',
        name: 'Scenario ESET Relocation',
        category: 'scenario',
        regions: relocationRegions
      });
    }
    return owners;
  }

  function standardPatchOwners(rom, dirty) {
    var owners = [];
    function add(id, name, category, regions) {
      if (regions && regions.length) owners.push({ id: id, name: name, category: category, regions: regions });
    }
    if (dirty && dirty.enemies && rom.archives && rom.archives[647]) {
      var enemyArchive = rom.archives[647];
      add('enemy-squads', 'Enemy Squads', 'enemies', [{
        kind: 'rom', start: enemyArchive.offset,
        size: enemyArchive.totalHeaderSize + enemyArchive.compSize,
        label: 'enemydat archive'
      }]);
    }
    if (dirty && dirty.items) add('items', 'Items', 'items', [{
      kind: 'rom', start: OB64.ITEM_STAT_OFFSET || 0x62310,
      size: (OB64.ITEM_STAT_COUNT || 278) * (OB64.ITEM_STAT_SIZE || 32),
      label: 'item records'
    }]);
    if (dirty && dirty.classDefs) add('classes', 'Classes', 'classDefs', [{
      kind: 'rom', start: OB64.CLASS_DEF_OFFSET || 0x5DAD8,
      size: (OB64.CLASS_DEF_TOTAL || 166) * (OB64.CLASS_DEF_RECORD_SIZE || 72),
      label: 'class definition records'
    }]);
    if (dirty && dirty.encounters) add('encounters', 'Neutral Encounters', 'encounters', [
      { kind: 'rom', start: OB64.NEUTRAL_TERRAIN_RATE_OFFSET || 0x141E80, size: 0x40, label: 'terrain encounter tables' },
      { kind: 'rom', start: OB64.NEUTRAL_ENCOUNTER_OFFSET || 0x141ED0, size: 0x330, label: 'neutral encounter records' },
      { kind: 'rom', start: OB64.NEUTRAL_GLOBAL_DIV_HI_OFFSET || 0x13C1E8, size: 0x44, label: 'global encounter roll code' }
    ]);
    if (dirty && dirty.creatureDrops) add('creature-drops', 'Creature Drops', 'creatureDrops', [{
      kind: 'rom', start: OB64.CREATURE_DROP_OFFSET || 0x142258,
      size: (OB64.CREATURE_DROP_COUNT || 36) * (OB64.CREATURE_DROP_STRIDE || 8),
      label: 'creature drop records'
    }]);
    if (dirty && dirty.consumables) add('consumable-metadata', 'Consumable Metadata', 'consumables',
      consumableChangedByteRegions(rom));
    if (dirty && dirty.statGates && rom.statGates && rom.statGates.meta) {
      var meta = rom.statGates.meta;
      add('stat-gates', 'Class-change Stat Gates', 'statGates', [{
        kind: 'rom', start: meta.compDataOff - 8, size: meta.compDataSize + 8, label: 'stat-gate LZSS slot'
      }]);
    }
    if (dirty && dirty.tools && rom && rom.tools && OB64.tools && OB64.tools.features) {
      var features = OB64.tools.features();
      for (var f = 0; f < features.length; f++) {
        var currentState = OB64.tools.featureState
          ? OB64.tools.featureState(rom.z64, features[f])
          : rom.tools.initial[features[f].id];
        if (currentState === 'foreign' || currentState === 'unsupported') continue;
        var desired = !!rom.tools.desired[features[f].id];
        var pending = currentState === 'outdated' || desired !== (currentState === 'applied');
        if (!pending) continue;
        var featureRegions = toolWriteRegions(features[f], currentState);
        add('tool-' + features[f].id, features[f].name, 'tools', featureRegions);
      }
    }
    if (dirty && dirty.scenario) owners = owners.concat(scenarioPatchOwners(rom));
    var session = sessionFor(rom);
    if (session && session.ledger && session.ledger.priorOwnerRegions) {
      owners = owners.concat(session.ledger.priorOwnerRegions);
    }
    return owners;
  }

  return {
    PROJECT_VERSION: PROJECT_VERSION,
    EDITOR_VERSION: EDITOR_VERSION,
    CATALOG_MAX_ID: CATALOG_MAX_ID,
    SOURCE_DESCRIPTOR: SOURCE_DESCRIPTOR,
    MODEL_ORDER: MODEL_ORDER.slice(),
    MODEL_DEFS: MODEL_DEFS,
    ITEM_TO_MODEL: Object.freeze(Object.assign({}, ITEM_TO_MODEL)),
    SHARED_TARGETS: SHARED_TARGETS,
    GUARD_MANIFEST: GUARD_MANIFEST,
    EXPECTED_LOADED_WORDS: EXPECTED_LOADED_WORDS,
    sha256Hex: sha256Hex,
    sha256HexSync: sha256HexSync,
    inspectSourceIdentity: inspectSourceIdentity,
    evaluateSourceIdentity: evaluateSourceIdentity,
    sourceFactsFromRaw: sourceFactsFromRaw,
    vanillaModels: vanillaModels,
    validateRange: validateRange,
    validateAllModels: validateAllModels,
    encodeRange: encodeRange,
    validateGuards: validateGuards,
    immutableProjection: immutableProjection,
    initializeSession: initializeSession,
    sessionFor: sessionFor,
    assertSessionOwnership: assertSessionOwnership,
    assertSharedBinding: assertSharedBinding,
    refreshPending: refreshPending,
    hasDesiredEffects: hasDesiredEffects,
    hasAppliedEffects: hasAppliedEffects,
    setModelRange: setModelRange,
    setItemRange: setItemRange,
    resetModel: resetModel,
    resetItem: resetItem,
    collectProjectPayload: collectProjectPayload,
    validateProjectPayload: validateProjectPayload,
    applyProjectPayload: applyProjectPayload,
    effectCollisionOwner: effectCollisionOwner,
    effectDeltaOwner: effectDeltaOwner,
    standardPatchOwners: standardPatchOwners,
    scenarioPatchOwners: scenarioPatchOwners,
    findRegionConflicts: findRegionConflicts,
    prepareTransaction: prepareTransaction,
    applyTransaction: applyTransaction,
    commitTransaction: commitTransaction,
    computeIndependentCrc: computeIndependentCrc,
    verifyIndependentCrc: verifyIndependentCrc,
    buildChangeRanges: buildChangeRanges,
    buildProvenance: buildProvenance,
    prepareOrdinaryExport: prepareOrdinaryExport,
    commitOrdinaryExport: commitOrdinaryExport,
    serializeCandidate: serializeCandidate,
    downloadRomCandidate: downloadRomCandidate,
    profileForModels: profileForModels,
    buildCatalog: buildCatalog,
    catalogDisposition: catalogDisposition,
    lockedIds: lockedIds,
    render: render
  };
});
