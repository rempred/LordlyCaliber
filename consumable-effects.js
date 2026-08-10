// OB64 Mod Editor - evidence-backed consumable effect ranges.
//
// The explicit patch profiles below cover verified US retail Rev 0 and Rev 1.
// Compatibility is decided from normalized ROM structure and feature-local
// guarded paths—not from a filename, whole-file hash, or container byte order.
// All ROM offsets are normalized z64 offsets.
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

  var PROJECT_VERSION = 13;
  var EDITOR_VERSION = '2026-07-24';
  var CATALOG_MAX_ID = 31;
  var FINAL_AFTER_IMAGE_PROOFS = new WeakMap();
  var VERIFIED_CANDIDATE_PACKAGES = new WeakMap();
  var VERIFIED_PROVENANCES = new WeakMap();
  var VERIFIED_DOWNLOAD_RECEIPTS = new WeakMap();
  var VERIFIED_ADOPTIONS = new WeakMap();
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
  var SUPPORTED_BYTE_ORDERS = Object.freeze(['v64', 'z64', 'n64']);
  var SUPPORTED_HEADER_VERSIONS = Object.freeze([0x00, 0x01]);

  var RANGE_MODEL_ORDER = Object.freeze([
    'cupOfLife',
    'sharedStatBoosters11To16',
    'scrollOfDiscipline',
    'urnOfChaos',
    'gobletOfDestiny'
  ]);
  var MAGNITUDE_MODEL_ORDER = Object.freeze([
    'healLeaf',
    'healSeed',
    'healPack',
    'powerFruit',
    'angelFruit'
  ]);
  var MODEL_ORDER = MAGNITUDE_MODEL_ORDER.concat(RANGE_MODEL_ORDER);

  var MODEL_DEFS = Object.freeze({
    healLeaf: Object.freeze({
      kind: 'magnitude',
      projectKey: '1',
      itemIds: Object.freeze([1]),
      magnitudeOffsets: Object.freeze([0x410F8]),
      magnitudeForms: Object.freeze([0x240A0000]),
      magnitudeRoles: Object.freeze(['magnitude / ID 2 delay-slot seed']),
      vanillaMagnitude: 100,
      domainMin: 0,
      domainMax: 999,
      normalMin: 100,
      normalMax: 999,
      target: 'Current HP for one target; bounded special-pool path',
      retail: '100',
      healingDescription: true
    }),
    healSeed: Object.freeze({
      kind: 'magnitude',
      projectKey: '2',
      itemIds: Object.freeze([2]),
      magnitudeOffsets: Object.freeze([0x410FC]),
      magnitudeForms: Object.freeze([0x240A0000]),
      magnitudeRoles: Object.freeze(['magnitude after ID 1 delay-slot seed']),
      vanillaMagnitude: 300,
      domainMin: 0,
      domainMax: 999,
      normalMin: 100,
      normalMax: 999,
      target: 'Current HP for one target; bounded special-pool path',
      retail: '300',
      healingDescription: true
    }),
    healPack: Object.freeze({
      kind: 'magnitude',
      projectKey: '3',
      itemIds: Object.freeze([3]),
      magnitudeOffsets: Object.freeze([0x41108, 0x4110C]),
      magnitudeForms: Object.freeze([0x240A0000, 0x24080000]),
      magnitudeRoles: Object.freeze(['special-pool magnitude', 'ordinary/cache magnitude']),
      vanillaMagnitude: 150,
      domainMin: 0,
      domainMax: 999,
      normalMin: 100,
      normalMax: 999,
      target: 'Current HP across the group; bounded special-pool path',
      retail: '150',
      healingDescription: true,
      pairAtomic: true
    }),
    powerFruit: Object.freeze({
      kind: 'magnitude',
      projectKey: '4',
      itemIds: Object.freeze([4]),
      magnitudeOffsets: Object.freeze([0x41248]),
      magnitudeForms: Object.freeze([0x240A0000]),
      magnitudeRoles: Object.freeze(['magnitude / ID 5 delay-slot seed']),
      vanillaMagnitude: 20,
      domainMin: 0,
      domainMax: 65535,
      normalMin: 1,
      normalMax: 255,
      target: 'Ordinary-target Fatigue byte (C+0x32); special targets are unchanged',
      retail: '20',
      healingDescription: false
    }),
    angelFruit: Object.freeze({
      kind: 'magnitude',
      projectKey: '5',
      itemIds: Object.freeze([5]),
      magnitudeOffsets: Object.freeze([0x4124C]),
      magnitudeForms: Object.freeze([0x240A0000]),
      magnitudeRoles: Object.freeze(['magnitude after ID 4 delay-slot seed']),
      vanillaMagnitude: 50,
      domainMin: 0,
      domainMax: 65535,
      normalMin: 1,
      normalMax: 255,
      target: 'Ordinary-target Fatigue byte (C+0x32); special targets are unchanged',
      retail: '50',
      healingDescription: false
    }),
    cupOfLife: Object.freeze({
      kind: 'range',
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
      kind: 'range',
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
      kind: 'range',
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
      kind: 'range',
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
      kind: 'range',
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
    '1': 'healLeaf',
    '2': 'healSeed',
    '3': 'healPack',
    '4': 'powerFruit',
    '5': 'angelFruit',
    '10': 'cupOfLife',
    '11-16': 'sharedStatBoosters11To16',
    '17': 'scrollOfDiscipline',
    '18': 'urnOfChaos',
    '19': 'gobletOfDestiny'
  };

  var ITEM_TO_MODEL = {
    1: 'healLeaf',
    2: 'healSeed',
    3: 'healPack',
    4: 'powerFruit',
    5: 'angelFruit',
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

  var MAGNITUDE_WORD_GUARDS = Object.freeze([
    Object.freeze({ modelKey: 'healLeaf', role: 'magnitude', offset: 0x410F8, vanilla: 0x240A0064, form: 0x240A0000 }),
    Object.freeze({ modelKey: 'healSeed', role: 'magnitude', offset: 0x410FC, vanilla: 0x240A012C, form: 0x240A0000 }),
    Object.freeze({ modelKey: 'healPack', role: 'specialPoolMagnitude', offset: 0x41108, vanilla: 0x240A0096, form: 0x240A0000 }),
    Object.freeze({ modelKey: 'healPack', role: 'ordinaryCacheMagnitude', offset: 0x4110C, vanilla: 0x24080096, form: 0x24080000 }),
    Object.freeze({ modelKey: 'powerFruit', role: 'magnitude', offset: 0x41248, vanilla: 0x240A0014, form: 0x240A0000 }),
    Object.freeze({ modelKey: 'angelFruit', role: 'magnitude', offset: 0x4124C, vanilla: 0x240A0032, form: 0x240A0000 })
  ]);

  var RANGE_EDITABLE_WORD_GUARDS = Object.freeze([
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
  var EDITABLE_WORD_GUARDS = Object.freeze(
    MAGNITUDE_WORD_GUARDS.concat(RANGE_EDITABLE_WORD_GUARDS)
  );

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

  var TARGET_METADATA_GUARDS_REV0 = Object.freeze([
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
  var TARGET_METADATA_GUARDS_REV1 = Object.freeze(
    TARGET_METADATA_GUARDS_REV0.map(function(entry) {
      return Object.freeze({
        id: entry.id,
        offset: entry.offset + 0x20,
        expected: entry.expected
      });
    })
  );

  var HELPER_OPERANDS_REV0 = Object.freeze([
    Object.freeze({ offset: 0x410B8, expected: 0x24633BC0 }),
    Object.freeze({ offset: 0x410E0, expected: 0x8C22FE60 }),
    Object.freeze({ offset: 0x4112C, expected: 0x94420F82 }),
    Object.freeze({ offset: 0x41140, expected: 0x24420F82 }),
    Object.freeze({ offset: 0x411C4, expected: 0x94A50F82 }),
    Object.freeze({ offset: 0x411D8, expected: 0x24630EBC }),
    Object.freeze({ offset: 0x412A8, expected: 0x90421010 }),
    Object.freeze({ offset: 0x412BC, expected: 0xA0221010 }),
    Object.freeze({ offset: 0x41324, expected: 0x0C05BAE9 })
  ]);
  var HELPER_OPERANDS_REV1 = Object.freeze([
    Object.freeze({ offset: 0x410B8, expected: 0x24633BE0 }),
    Object.freeze({ offset: 0x410E0, expected: 0x8C22FE80 }),
    Object.freeze({ offset: 0x4112C, expected: 0x94420FA2 }),
    Object.freeze({ offset: 0x41140, expected: 0x24420FA2 }),
    Object.freeze({ offset: 0x411C4, expected: 0x94A50FA2 }),
    Object.freeze({ offset: 0x411D8, expected: 0x24630EDC }),
    Object.freeze({ offset: 0x412A8, expected: 0x90421030 }),
    Object.freeze({ offset: 0x412BC, expected: 0xA0221030 }),
    Object.freeze({ offset: 0x41324, expected: 0x0C05BAF1 })
  ]);

  var HEALING_DESCRIPTION_CONSTANTS = Object.freeze({
    headerHex: '0000080B00000CA2',
    payloadSize: 0x80B,
    streamCapacity: 2055,
    decodedSize: 0xCA2,
    ownerSize: 2064,
    alignment: 0x00,
    nextHeaderHex: '00000BF500002000',
    digitOffsets: Object.freeze([0x41, 0x42, 0x43, 0x7F, 0x80, 0x81, 0xB2, 0xB3, 0xB4]),
    tripleOffsets: Object.freeze([0x41, 0x7F, 0xB2]),
    immutableProjectionSha256: '52C88BA084034554DC8237384D373A0276934DEBCABEF8370A3CC70ED80E9397',
    zeroedDecodedSha256: 'A1A8C86DB565797BE0B24F448A5A389CC09C82CDC32FD2ACF8615BE0D4DB3038',
    canonicalConsumed: 2015,
    canonicalPadding: 40,
    canonicalPaddingByte: 0x03,
    planSha256: '39967179B4A89988612AE42ED486472F5FBFCFB1874F9148D6A607367B55581A',
    planTokens: 593,
    planLiterals: 173,
    planShortReferences: 412,
    planMediumReferences: 7,
    planLongReferences: 1,
    planMaximumDistance: 2274,
    variableLiteralTokens: Object.freeze([
      Object.freeze([0x3F, 7]),
      Object.freeze([0x7F, 3]),
      Object.freeze([0xB1, 4])
    ]),
    slotVectors: Object.freeze({
      '100/300/150': '0978C582BF881033B2F475B63D099BEB1D78395643AC70E84F5134CAE2449BFC',
      '111/222/333': '804594D42CA773E869C1D08160E3065C55A3412073A567657C1BBA4654F6CDF8',
      '777/888/999': '5C3FE8E2E89D07DFD3560C08B66DAB81B119357E2C044F3DC37C0BC6C6265F35',
      '000/000/000': '5E7EDCC4C6BAEA860D69201BB41FB75CA31A0726D50C71D2CD6B0B409CBBC8EC',
      '001/042/099': 'AAFCCA534815B44BAB6B547C0E2FF23CDD54ACA362BF48ED2725FDB5853EBFBE',
      '999/999/999': '1F9198660AD5EF8724E1A4651D24497067AD044048264FA151E50612936EFBF5'
    })
  });

  function makeRangeProfile(revision, dispatchStart, targetMetadata) {
    return Object.freeze({
      id: revision + '-range-models',
      revision: revision,
      editableWords: RANGE_EDITABLE_WORD_GUARDS,
      immutableWords: IMMUTABLE_WORD_GUARDS,
      contextRanges: CONTEXT_GUARDS,
      dispatchTable: Object.freeze({
        start: dispatchStart,
        end: dispatchStart + 0x4C,
        sha256: '06532D7BCA4FD20FF9409CCEEFAF0B518250005DB000039CFDF66FFB375D1D04',
        words: DISPATCH_WORDS
      }),
      targetMetadata: targetMetadata
    });
  }

  function hashGuard(id, label, start, end, sha256, omittedOffsets) {
    return Object.freeze({
      id: id,
      label: label,
      start: start,
      end: end,
      sha256: sha256,
      omittedOffsets: Object.freeze((omittedOffsets || []).slice())
    });
  }

  function makeCommonProfile(revision, values) {
    return Object.freeze({
      id: revision + '-ids-1-5-common',
      revision: revision,
      helperOperands: values.helperOperands,
      hashGuards: Object.freeze([
        hashGuard('helper', 'helper/prologue', 0x41098, 0x410EC, values.helperHash),
        hashGuard('healing-body', 'IDs 1-3 body excluding magnitude words',
          0x410EC, 0x4123C, values.healingBodyHash,
          [0x410F8, 0x410FC, 0x41108, 0x4110C]),
        hashGuard('fruit-body', 'IDs 4-5 body excluding magnitude words',
          0x4123C, 0x41288,
          'C10D83B89C67AC2CCF913A32A110D4DF940CA9A565384C315F1D2698C74DA0B9',
          [0x41248, 0x4124C]),
        hashGuard('shared-epilogue', 'shared helper epilogue',
          0x415EC, 0x415FC,
          '9EFC10AECF13FD364EF2DA4CAF48B2281F0719293EA911366DF044A19FBA85BE'),
        hashGuard('caller', 'state-correlated consumable caller',
          values.callerStart, values.callerEnd, values.callerHash),
        hashGuard('validator', 'consumable validator',
          values.validatorStart, values.validatorEnd, values.validatorHash),
        hashGuard('metadata-helper', 'target-scope metadata helper',
          values.metadataStart, values.metadataEnd, values.metadataHash),
        hashGuard('records-1-5', 'consumable records 1-5',
          values.recordsStart, values.recordsEnd, values.recordsHash),
        hashGuard('dispatch', 'complete 19-word consumable dispatch table',
          values.dispatchStart, values.dispatchStart + 0x4C,
          '06532D7BCA4FD20FF9409CCEEFAF0B518250005DB000039CFDF66FFB375D1D04')
      ]),
      scopeBytes: Object.freeze(values.scopeOffsets.map(function(offset, index) {
        return Object.freeze({ id: index + 1, offset: offset, expected: [1, 1, 2, 2, 2][index] });
      }))
    });
  }

  var REV0_RANGE_PROFILE = makeRangeProfile('rev0', 0x65D60, TARGET_METADATA_GUARDS_REV0);
  var REV1_RANGE_PROFILE = makeRangeProfile('rev1', 0x65D80, TARGET_METADATA_GUARDS_REV1);
  var REV0_COMMON_PROFILE = makeCommonProfile('rev0', {
    helperOperands: HELPER_OPERANDS_REV0,
    helperHash: 'FB718FF5CA9A88D0E06BF75F7C510CA843EE3002299E04DDED6302590E73987F',
    healingBodyHash: '5C8B9C8AF8C01C19957BC5A9356773CD53BC1644F306E2DAFFE97B482AC0E044',
    callerStart: 0xC9850, callerEnd: 0xCA7E8,
    callerHash: '83D4FBB1EE7EF0D53F51163A69C4D152B81302D959FD7C4A776B2C3C2B7BA1DD',
    validatorStart: 0xC4650, validatorEnd: 0xC4A40,
    validatorHash: 'F71887B48AEB553008FBD2A19F93921D66392DDD6BB52F44C7BE092A721704C2',
    metadataStart: 0x45440, metadataEnd: 0x45460,
    metadataHash: '8870BFE8DA76416B82E9AC2A804A4D5EB6F8670D98CA3B3A44E7DDEA47021861',
    recordsStart: 0x645D8, recordsEnd: 0x64614,
    recordsHash: 'A0425C2E811BA9983BC13914043EA8474D79C1B6B407A71C9E20DA52C81590D4',
    dispatchStart: 0x65D60,
    scopeOffsets: [0x645E0, 0x645EC, 0x645F8, 0x64604, 0x64610]
  });
  var REV1_COMMON_PROFILE = makeCommonProfile('rev1', {
    helperOperands: HELPER_OPERANDS_REV1,
    helperHash: 'B8D1A39107C0D563CD9F0DD9823A9221FA18CC48A6BA8D724435A0E3769098D9',
    healingBodyHash: 'E9F3D564A69AB330F26C6E1722FB5F4F0C1B994A2B0ED8EC960FD94BDF0E7B16',
    callerStart: 0xC9870, callerEnd: 0xCA808,
    callerHash: '8CC4ACC0DAB77FCE6E043774657433FB7D30CE42975FAE561F6FAD75D00847A8',
    validatorStart: 0xC4670, validatorEnd: 0xC4A60,
    validatorHash: '2A596736ABC9271CEE89065FCD706D9C3A4B78C4CF7E4068EBF6AEF278B637F7',
    metadataStart: 0x45460, metadataEnd: 0x45480,
    metadataHash: 'F2EA69E20E8C8DF28B27B835B06C1B8E384859DFCF7C57F0F07C172D12F122BF',
    recordsStart: 0x645F8, recordsEnd: 0x64634,
    recordsHash: '0CD281C9187D870E2249C96FED09197C49404EB2A2E3FA0A18B779080C628F1A',
    dispatchStart: 0x65D80,
    scopeOffsets: [0x64600, 0x6460C, 0x64618, 0x64624, 0x64630]
  });

  function makeRevisionManifest(id, headerVersion, rangeProfile, commonProfile,
      descriptionStart, loadedDispatchStart) {
    return Object.freeze({
      id: id,
      label: id === 'rev0' ? 'Rev 0' : 'Rev 1',
      headerVersion: headerVersion,
      rangeProfile: rangeProfile,
      commonProfile: commonProfile,
      magnitudeWords: MAGNITUDE_WORD_GUARDS,
      description: Object.freeze({
        start: descriptionStart,
        end: descriptionStart + HEALING_DESCRIPTION_CONSTANTS.ownerSize,
        nextHeaderOffset: descriptionStart + HEALING_DESCRIPTION_CONSTANTS.ownerSize
      }),
      loadedDispatch: Object.freeze({
        start: loadedDispatchStart,
        end: loadedDispatchStart + 0x4C
      })
    });
  }

  var REVISION_MANIFESTS = Object.freeze({
    rev0: makeRevisionManifest('rev0', 0x00, REV0_RANGE_PROFILE,
      REV0_COMMON_PROFILE, 0x237744A, 0x8018FE60),
    rev1: makeRevisionManifest('rev1', 0x01, REV1_RANGE_PROFILE,
      REV1_COMMON_PROFILE, 0x237709E, 0x8018FE80)
  });

  // Legacy public alias: the accepted Rev 0 IDs 10-19 range profile.
  var GUARD_MANIFEST = REV0_RANGE_PROFILE;

  var EXPECTED_LOADED_WORDS = Object.freeze({
    healLeaf: Object.freeze({ magnitude: '0x8016B1F8' }),
    healSeed: Object.freeze({ magnitude: '0x8016B1FC' }),
    healPack: Object.freeze({
      specialPoolMagnitude: '0x8016B208',
      ordinaryCacheMagnitude: '0x8016B20C'
    }),
    powerFruit: Object.freeze({ magnitude: '0x8016B348' }),
    angelFruit: Object.freeze({ magnitude: '0x8016B34C' }),
    cupOfLife: Object.freeze({ width: '0x8016B44C', minimum: '0x8016B488' }),
    sharedStatBoosters11To16: Object.freeze({ width: '0x8016B544', minimum: '0x8016B580' }),
    scrollOfDiscipline: Object.freeze({ width: '0x8016B59C', minimum: '0x8016B5D8' }),
    urnOfChaos: Object.freeze({ width: '0x8016B618', minimum: '0x8016B654' }),
    gobletOfDestiny: Object.freeze({ width: '0x8016B670', minimum: '0x8016B6AC' }),
    finalStore: '0x8016B6E4',
    dispatchTableByRevision: Object.freeze({
      rev0: Object.freeze({
        source: '[0x00065D60,0x00065DAC)',
        live: '[0x8018FE60,0x8018FEAC)',
        lastWord: '0x8018FEA8'
      }),
      rev1: Object.freeze({
        source: '[0x00065D80,0x00065DCC)',
        live: '[0x8018FE80,0x8018FECC)',
        lastWord: '0x8018FEC8'
      })
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
      size: facts.size === SOURCE_DESCRIPTOR.size,
      byteOrder: SUPPORTED_BYTE_ORDERS.indexOf(facts.byteOrder) !== -1,
      imageName: facts.imageName === SOURCE_DESCRIPTOR.imageName,
      gameId: facts.gameId === SOURCE_DESCRIPTOR.gameId,
      country: Number(facts.country) === SOURCE_DESCRIPTOR.country,
      version: SUPPORTED_HEADER_VERSIONS.indexOf(Number(facts.version)) !== -1
    };
    var referenceChecks = {
      filename: facts.filename === SOURCE_DESCRIPTOR.filename,
      sha256: String(facts.sha256 || '').toUpperCase() === SOURCE_DESCRIPTOR.sha256,
      byteOrder: facts.byteOrder === SOURCE_DESCRIPTOR.byteOrder,
      version: Number(facts.version) === SOURCE_DESCRIPTOR.version,
      crc1: (Number(facts.crc1) >>> 0) === SOURCE_DESCRIPTOR.crc1,
      crc2: (Number(facts.crc2) >>> 0) === SOURCE_DESCRIPTOR.crc2
    };
    var eligible = Object.keys(checks).every(function(key) { return checks[key]; });
    var reason = '';
    if (!eligible) {
      if (!checks.size) {
        reason = 'Effect editing requires the 41,943,040-byte US retail ROM layout.';
      } else if (!checks.byteOrder) {
        reason = 'Effect editing requires a recognized .v64, .z64, or .n64 ROM image.';
      } else {
        reason = 'Effect editing requires a supported US retail rev0 or rev1 ROM header.';
      }
    }
    var revision = Number(facts.version) === 0x00
      ? 'rev0'
      : (Number(facts.version) === 0x01 ? 'rev1' : null);
    return {
      eligible: eligible,
      reason: reason,
      revision: revision,
      revisionLabel: revision && REVISION_MANIFESTS[revision]
        ? REVISION_MANIFESTS[revision].label
        : 'Unsupported revision',
      referenceMatch: Object.keys(referenceChecks).every(function(key) {
        return referenceChecks[key];
      }),
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
      checks: Object.freeze(checks),
      referenceChecks: Object.freeze(referenceChecks)
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
      if (MODEL_DEFS[key].kind === 'magnitude') {
        out[key] = {
          magnitude: Number(source.magnitude != null
            ? source.magnitude
            : source.vanillaMagnitude)
        };
      } else {
        out[key] = {
          minimum: Number(source.minimum != null ? source.minimum : source.vanillaMin),
          maximum: Number(source.maximum != null ? source.maximum : source.vanillaMax)
        };
      }
    }
    return out;
  }

  function vanillaModels() {
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      if (MODEL_DEFS[key].kind === 'magnitude') {
        out[key] = { magnitude: MODEL_DEFS[key].vanillaMagnitude };
      } else {
        out[key] = {
          minimum: MODEL_DEFS[key].vanillaMin,
          maximum: MODEL_DEFS[key].vanillaMax
        };
      }
    }
    return out;
  }

  function magnitudeStatus(modelKey, magnitude) {
    var def = MODEL_DEFS[modelKey];
    if (!def || def.kind !== 'magnitude') {
      throw new Error('Unknown consumable magnitude model "' + modelKey + '".');
    }
    if (!Number.isInteger(magnitude) ||
        magnitude < def.domainMin || magnitude > def.domainMax) {
      throw new Error('Supported magnitude for this effect is ' +
        def.domainMin + ' through ' + def.domainMax + '.');
    }
    if (def.healingDescription) {
      if (magnitude === 0) {
        return {
          tier: 'warning-no-op',
          label: 'Warning: consuming no-op',
          message: '000 is supported, but the item can be consumed without changing the target value.',
          padded: '000'
        };
      }
      if (magnitude < 100) {
        return {
          tier: 'warning-experimental',
          label: 'Warning: experimental value tier',
          message: String(magnitude).padStart(3, '0') +
            ' is synchronized into the in-game description as a zero-padded experimental value.',
          padded: String(magnitude).padStart(3, '0')
        };
      }
      return {
        tier: 'normal',
        label: 'Normal value tier',
        message: 'The in-game numeric description will be synchronized as ' +
          String(magnitude).padStart(3, '0') + '.',
        padded: String(magnitude).padStart(3, '0')
      };
    }
    if (magnitude === 0) {
      return {
        tier: 'warning-no-op',
        label: 'Warning: consuming no-op',
        message: '0 is supported, but an accepted use can consume the item without reducing Fatigue.',
        padded: null
      };
    }
    if (magnitude > 255) {
      return {
        tier: 'warning-redundant',
        label: 'Warning: redundant u8 result tier',
        message: magnitude + ' is supported, but it has the same ordinary-target result as 255 because Fatigue is a u8 value.',
        padded: null
      };
    }
    return {
      tier: 'normal',
      label: 'Normal value tier',
      message: 'Ordinary-target Fatigue is reduced by this u16 magnitude and floored at zero.',
      padded: null
    };
  }

  function validateMagnitude(modelKey, magnitude) {
    var def = MODEL_DEFS[modelKey];
    if (!def || def.kind !== 'magnitude') {
      throw new Error('Unknown consumable magnitude model "' + modelKey + '".');
    }
    if (!Number.isInteger(magnitude)) {
      throw new Error('Magnitude must be an integer.');
    }
    if (magnitude < def.domainMin || magnitude > def.domainMax) {
      throw new Error('Supported magnitude for this effect is ' +
        def.domainMin + ' through ' + def.domainMax + '.');
    }
    return {
      magnitude: magnitude,
      status: magnitudeStatus(modelKey, magnitude)
    };
  }

  function validateRange(modelKey, minimum, maximum) {
    var def = MODEL_DEFS[modelKey];
    if (!def || def.kind !== 'range') {
      throw new Error('Unknown consumable range model "' + modelKey + '".');
    }
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
      if (MODEL_DEFS[key].kind === 'magnitude') {
        normalized[key] = {
          magnitude: validateMagnitude(key, value.magnitude).magnitude
        };
      } else {
        normalized[key] = validateRange(key, value.minimum, value.maximum);
      }
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

  function encodeMagnitude(modelKey, value) {
    var checked = validateMagnitude(modelKey, value.magnitude);
    var def = MODEL_DEFS[modelKey];
    return {
      magnitude: checked.magnitude,
      status: checked.status,
      words: def.magnitudeOffsets.map(function(offset, index) {
        return {
          offset: offset,
          role: def.magnitudeRoles[index],
          word: (def.magnitudeForms[index] | checked.magnitude) >>> 0
        };
      })
    };
  }

  function hexToBytes(value) {
    var clean = String(value || '').replace(/\s+/g, '');
    if (!clean.length || clean.length % 2) throw new Error('Expected an even-length hexadecimal byte string.');
    var out = new Uint8Array(clean.length / 2);
    for (var i = 0; i < out.length; i++) {
      var parsed = parseInt(clean.slice(i * 2, i * 2 + 2), 16);
      if (!Number.isFinite(parsed)) throw new Error('Invalid hexadecimal byte string.');
      out[i] = parsed;
    }
    return out;
  }

  function equalBytes(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function decodeLzssStrict(input, expectedOutputSize) {
    var data = asBytes(input);
    if (!Number.isInteger(expectedOutputSize) || expectedOutputSize < 0) {
      throw new Error('Strict LZSS decode requires a nonnegative integer output size.');
    }
    var output = new Uint8Array(expectedOutputSize);
    var tokens = [];
    var ip = 0;
    var outPos = 0;
    function need(count, label) {
      if (ip + count > data.length) {
        throw new Error('Strict LZSS ' + label + ' token overruns the input at byte ' + ip + '.');
      }
    }
    function room(length, label) {
      if (outPos + length > expectedOutputSize) {
        throw new Error('Strict LZSS ' + label + ' token overruns the declared output at byte ' + outPos + '.');
      }
    }
    function copyReference(length, distance, label, inputStart, inputLength) {
      room(length, label);
      if (distance < 1 || distance > outPos) {
        throw new Error('Strict LZSS ' + label + ' reference has invalid source distance ' +
          distance + ' at output byte ' + outPos + '.');
      }
      var tokenStart = outPos;
      for (var index = 0; index < length; index++) {
        output[outPos] = output[outPos - distance];
        outPos++;
      }
      tokens.push({
        type: label,
        position: tokenStart,
        length: length,
        distance: distance,
        inputStart: inputStart,
        inputLength: inputLength
      });
    }
    while (outPos < expectedOutputSize) {
      need(1, 'opcode');
      var inputStart = ip;
      var opcode = data[ip++];
      var length;
      var distance;
      if (opcode >= 0x80) {
        need(1, 'short-reference');
        length = ((opcode >>> 3) & 0x0F) + 3;
        distance = (((opcode & 0x07) << 8) | data[ip++]) + 1;
        copyReference(length, distance, 'short-reference', inputStart, 2);
      } else if (opcode >= 0x40) {
        length = (opcode & 0x3F) + 1;
        need(length, 'literal');
        room(length, 'literal');
        var literalStart = outPos;
        output.set(data.subarray(ip, ip + length), outPos);
        ip += length;
        outPos += length;
        tokens.push({
          type: 'literal',
          position: literalStart,
          length: length,
          distance: 0,
          inputStart: inputStart,
          inputLength: length + 1
        });
      } else if (opcode >= 0x20) {
        length = (opcode & 0x1F) + 2;
        room(length, 'short-zero-fill');
        tokens.push({
          type: 'short-zero-fill',
          position: outPos,
          length: length,
          distance: 0,
          inputStart: inputStart,
          inputLength: 1
        });
        outPos += length;
      } else if (opcode >= 0x10) {
        need(2, 'medium-reference');
        var mediumByte = data[ip++];
        length = ((opcode & 0x0F) | ((mediumByte & 0xC0) >>> 2)) + 4;
        distance = (((mediumByte & 0x3F) << 8) | data[ip++]) + 1;
        copyReference(length, distance, 'medium-reference', inputStart, 3);
      } else if (opcode === 0x00) {
        need(3, 'long-reference');
        length = data[ip++] + 5;
        distance = ((data[ip++] << 8) | data[ip++]) + 1;
        copyReference(length, distance, 'long-reference', inputStart, 4);
      } else if (opcode === 0x01 || opcode === 0x02) {
        need(1, opcode === 0x01 ? 'FF-fill' : 'long-zero-fill');
        length = data[ip++] + 3;
        room(length, opcode === 0x01 ? 'FF-fill' : 'long-zero-fill');
        var fillStart = outPos;
        if (opcode === 0x01) output.fill(0xFF, outPos, outPos + length);
        outPos += length;
        tokens.push({
          type: opcode === 0x01 ? 'ff-fill' : 'long-zero-fill',
          position: fillStart,
          length: length,
          distance: 0,
          inputStart: inputStart,
          inputLength: 2
        });
      } else {
        tokens.push({
          type: 'no-output',
          position: outPos,
          length: 0,
          distance: 0,
          inputStart: inputStart,
          inputLength: 1
        });
      }
    }
    return {
      output: output,
      bytesConsumed: ip,
      tokens: tokens
    };
  }

  // Deliberately separate parser/control flow from decodeLzssStrict. Export
  // verification requires both implementations to agree byte-for-byte.
  function decodeLzssStrictAlternate(input, expectedOutputSize) {
    var source = asBytes(input);
    var destination = new Uint8Array(expectedOutputSize);
    var cursor = 0;
    var produced = 0;
    var tokenCount = 0;
    while (produced !== expectedOutputSize) {
      if (cursor >= source.length) {
        throw new Error('Alternate strict LZSS decode ended before the declared output was complete.');
      }
      var start = cursor;
      var control = source[cursor++];
      var kind = '';
      var count = 0;
      var distance = 0;
      var payloadStart = cursor;
      if (control >= 0x80) {
        kind = 'reference';
        if (cursor >= source.length) throw new Error('Alternate strict short reference is truncated.');
        count = ((control >>> 3) & 15) + 3;
        distance = (((control & 7) << 8) | source[cursor++]) + 1;
      } else if (control >= 0x40) {
        kind = 'literal';
        count = (control & 63) + 1;
        payloadStart = cursor;
        if (cursor + count > source.length) throw new Error('Alternate strict literal is truncated.');
        cursor += count;
      } else if (control >= 0x20) {
        kind = 'zero';
        count = (control & 31) + 2;
      } else if (control >= 0x10) {
        kind = 'reference';
        if (cursor + 2 > source.length) throw new Error('Alternate strict medium reference is truncated.');
        var packed = source[cursor++];
        count = ((control & 15) | ((packed & 0xC0) >>> 2)) + 4;
        distance = (((packed & 0x3F) << 8) | source[cursor++]) + 1;
      } else if (control === 0x00) {
        kind = 'reference';
        if (cursor + 3 > source.length) throw new Error('Alternate strict long reference is truncated.');
        count = source[cursor++] + 5;
        distance = ((source[cursor++] << 8) | source[cursor++]) + 1;
      } else if (control === 0x01 || control === 0x02) {
        kind = control === 0x01 ? 'ff' : 'zero';
        if (cursor >= source.length) throw new Error('Alternate strict fill token is truncated.');
        count = source[cursor++] + 3;
      } else {
        kind = 'nop';
      }
      if (produced + count > expectedOutputSize) {
        throw new Error('Alternate strict LZSS token at input byte ' + start +
          ' overruns the declared output.');
      }
      if (kind === 'literal') {
        destination.set(source.subarray(payloadStart, payloadStart + count), produced);
        produced += count;
      } else if (kind === 'reference') {
        if (distance < 1 || distance > produced) {
          throw new Error('Alternate strict LZSS reference at input byte ' + start +
            ' reads before output start.');
        }
        for (var copyIndex = 0; copyIndex < count; copyIndex++) {
          destination[produced] = destination[produced - distance];
          produced++;
        }
      } else if (kind === 'zero' || kind === 'ff') {
        destination.fill(kind === 'ff' ? 0xFF : 0x00, produced, produced + count);
        produced += count;
      }
      tokenCount++;
    }
    return {
      output: destination,
      bytesConsumed: cursor,
      tokenCount: tokenCount
    };
  }

  function healingImmutableProjection(decoded) {
    var source = asBytes(decoded);
    var variable = {};
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      variable[offset] = true;
    });
    var out = new Uint8Array(source.length - HEALING_DESCRIPTION_CONSTANTS.digitOffsets.length);
    var cursor = 0;
    for (var i = 0; i < source.length; i++) {
      if (!variable[i]) out[cursor++] = source[i];
    }
    return out;
  }

  function zeroHealingDigits(decoded) {
    var out = asBytes(decoded).slice();
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      out[offset] = 0;
    });
    return out;
  }

  function healingReferenceIsVariableSafe(decoded, position, length, distance) {
    var source = asBytes(decoded);
    var variable = {};
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      variable[offset] = true;
    });
    if (!Number.isInteger(position) || !Number.isInteger(length) ||
        !Number.isInteger(distance) || length < 1 || distance < 1 ||
        position < distance || position + length > source.length) {
      return false;
    }
    for (var index = 0; index < length; index++) {
      var target = position + index;
      var sourceCoordinate = position - distance + index;
      if (variable[target] || variable[sourceCoordinate]) return false;
      if (sourceCoordinate < 0 || source[sourceCoordinate] !== source[target]) return false;
    }
    return true;
  }

  var HEALING_TOKEN_RANK = Object.freeze({
    longReference: 0,
    mediumReference: 1,
    shortReference: 2,
    longZeroFill: 3,
    shortZeroFill: 4,
    ffFill: 5,
    literal: 6
  });
  var cachedHealingPlan = null;

  function buildHealingPlan(decodedInput) {
    var decoded = asBytes(decodedInput);
    var outputSize = HEALING_DESCRIPTION_CONSTANTS.decodedSize;
    if (decoded.length !== outputSize) {
      throw new Error('Healing-description plan requires exactly ' + outputSize + ' decoded bytes.');
    }
    var variable = new Uint8Array(outputSize);
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      variable[offset] = 1;
    });
    var nextVariableDistance = new Uint16Array(outputSize + 1);
    var nextVariable = outputSize;
    for (var nv = outputSize - 1; nv >= 0; nv--) {
      if (variable[nv]) nextVariable = nv;
      nextVariableDistance[nv] = nextVariable - nv;
    }

    // A complete suffix LCP table makes reference enumeration exact while
    // keeping overlap simulation O(1) per candidate distance.
    var side = outputSize + 1;
    var lcp = new Uint16Array(side * side);
    for (var left = outputSize - 1; left >= 0; left--) {
      var row = left * side;
      var nextRow = (left + 1) * side;
      for (var right = outputSize - 1; right >= 0; right--) {
        if (decoded[left] === decoded[right]) {
          lcp[row + right] = 1 + lcp[nextRow + right + 1];
        }
      }
    }

    var unreachable = 0xFFFFFFFF;
    var dp = new Uint32Array(outputSize + 1);
    dp.fill(unreachable);
    dp[outputSize] = 0;
    var choices = new Array(outputSize);
    function isBetter(candidate, current) {
      if (!current) return true;
      if (candidate.cost !== current.cost) return candidate.cost < current.cost;
      if (candidate.rank !== current.rank) return candidate.rank < current.rank;
      if (candidate.length !== current.length) return candidate.length > current.length;
      return candidate.distance < current.distance;
    }

    for (var position = outputSize - 1; position >= 0; position--) {
      var best = null;
      function consider(type, length, distance, tokenBytes) {
        if (position + length > outputSize || dp[position + length] === unreachable) return;
        var candidate = {
          type: type,
          position: position,
          length: length,
          distance: distance || 0,
          rank: HEALING_TOKEN_RANK[type],
          tokenBytes: tokenBytes,
          cost: tokenBytes + dp[position + length]
        };
        if (isBetter(candidate, best)) best = candidate;
      }

      for (var literalLength = 1;
          literalLength <= Math.min(64, outputSize - position);
          literalLength++) {
        consider('literal', literalLength, 0, literalLength + 1);
      }

      var zeroRun = 0;
      while (zeroRun < Math.min(258, outputSize - position) &&
          decoded[position + zeroRun] === 0 &&
          !variable[position + zeroRun]) {
        zeroRun++;
      }
      for (var shortZero = 2; shortZero <= Math.min(33, zeroRun); shortZero++) {
        consider('shortZeroFill', shortZero, 0, 1);
      }
      for (var longZero = 3; longZero <= zeroRun; longZero++) {
        consider('longZeroFill', longZero, 0, 2);
      }

      var ffRun = 0;
      while (ffRun < Math.min(258, outputSize - position) &&
          decoded[position + ffRun] === 0xFF &&
          !variable[position + ffRun]) {
        ffRun++;
      }
      for (var ffLength = 3; ffLength <= ffRun; ffLength++) {
        consider('ffFill', ffLength, 0, 2);
      }

      var shortestShortDistance = new Uint16Array(19);
      var shortestMediumDistance = new Uint16Array(68);
      var shortestLongDistance = new Uint16Array(261);
      var targetSafeLength = nextVariableDistance[position];
      for (var distance = 1; distance <= position; distance++) {
        var sourceStart = position - distance;
        var maximum = Math.min(
          lcp[sourceStart * side + position],
          nextVariableDistance[sourceStart],
          targetSafeLength,
          260,
          outputSize - position
        );
        if (maximum >= 3 && distance <= 2048) {
          for (var shortLength = 3; shortLength <= Math.min(18, maximum); shortLength++) {
            if (!shortestShortDistance[shortLength]) {
              shortestShortDistance[shortLength] = distance;
            }
          }
        }
        if (maximum >= 4 && distance <= 16384) {
          for (var mediumLength = 4; mediumLength <= Math.min(67, maximum); mediumLength++) {
            if (!shortestMediumDistance[mediumLength]) {
              shortestMediumDistance[mediumLength] = distance;
            }
          }
        }
        if (maximum >= 5 && distance <= 65536) {
          for (var longLength = 5; longLength <= Math.min(260, maximum); longLength++) {
            if (!shortestLongDistance[longLength]) {
              shortestLongDistance[longLength] = distance;
            }
          }
        }
      }
      for (var srLength = 3; srLength <= 18; srLength++) {
        if (shortestShortDistance[srLength]) {
          consider('shortReference', srLength, shortestShortDistance[srLength], 2);
        }
      }
      for (var mrLength = 4; mrLength <= 67; mrLength++) {
        if (shortestMediumDistance[mrLength]) {
          consider('mediumReference', mrLength, shortestMediumDistance[mrLength], 3);
        }
      }
      for (var lrLength = 5; lrLength <= 260; lrLength++) {
        if (shortestLongDistance[lrLength]) {
          consider('longReference', lrLength, shortestLongDistance[lrLength], 4);
        }
      }
      if (!best) throw new Error('No deterministic healing-description token at decoded byte ' + position + '.');
      dp[position] = best.cost;
      choices[position] = best;
    }

    var plan = [];
    for (var cursor = 0; cursor < outputSize;) {
      var token = choices[cursor];
      if (!token || token.position !== cursor || token.length < 1) {
        throw new Error('Healing-description plan reconstruction failed at decoded byte ' + cursor + '.');
      }
      plan.push(token);
      cursor += token.length;
    }
    plan.byteLength = dp[0];
    return plan;
  }

  function serializeHealingPlan(plan) {
    var out = new Uint8Array((plan || []).length * 7);
    for (var i = 0; i < (plan || []).length; i++) {
      var token = plan[i];
      var offset = i * 7;
      out[offset] = (token.position >>> 8) & 0xFF;
      out[offset + 1] = token.position & 0xFF;
      out[offset + 2] = (token.length >>> 8) & 0xFF;
      out[offset + 3] = token.length & 0xFF;
      out[offset + 4] = (token.distance >>> 8) & 0xFF;
      out[offset + 5] = token.distance & 0xFF;
      out[offset + 6] = token.rank & 0xFF;
    }
    return out;
  }

  function assertHealingPlanVariableIsolation(plan) {
    var size = HEALING_DESCRIPTION_CONSTANTS.decodedSize;
    var digit = new Uint8Array(size);
    var tainted = new Uint8Array(size);
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      digit[offset] = 1;
    });
    var cursor = 0;
    for (var tokenIndex = 0; tokenIndex < (plan || []).length; tokenIndex++) {
      var token = plan[tokenIndex];
      if (!token || token.position !== cursor || !Number.isInteger(token.length) ||
          token.length < 1 || cursor + token.length > size) {
        throw new Error('Healing-description variable-isolation plan is discontinuous at decoded byte ' +
          cursor + '.');
      }
      for (var index = 0; index < token.length; index++) {
        var target = token.position + index;
        if (token.type === 'literal') {
          tainted[target] = digit[target];
        } else if (token.type === 'shortReference' ||
            token.type === 'mediumReference' ||
            token.type === 'longReference') {
          var source = target - token.distance;
          if (source < 0 || source >= target) {
            throw new Error('Healing-description plan reference has an invalid source at decoded byte ' +
              target + '.');
          }
          tainted[target] = tainted[source];
        } else {
          tainted[target] = 0;
        }
        if (digit[target] && token.type !== 'literal') {
          throw new Error('Healing-description plan directly references variable digit byte ' +
            hex(target, 4) + '.');
        }
        if (!digit[target] && tainted[target]) {
          throw new Error('Healing-description plan transitively contaminates immutable decoded byte ' +
            hex(target, 4) + ' from a variable digit.');
        }
      }
      cursor += token.length;
    }
    if (cursor !== size) {
      throw new Error('Healing-description variable-isolation plan ends at ' +
        cursor + ' rather than ' + size + '.');
    }
    return true;
  }

  function healingPlanStats(plan) {
    assertHealingPlanVariableIsolation(plan);
    var stats = {
      byteLength: 0,
      tokens: (plan || []).length,
      literals: 0,
      shortReferences: 0,
      mediumReferences: 0,
      longReferences: 0,
      maximumDistance: 0,
      variableLiteralTokens: []
    };
    var variable = {};
    HEALING_DESCRIPTION_CONSTANTS.digitOffsets.forEach(function(offset) {
      variable[offset] = true;
    });
    for (var i = 0; i < (plan || []).length; i++) {
      var token = plan[i];
      stats.byteLength += token.tokenBytes;
      if (token.type === 'literal') stats.literals++;
      else if (token.type === 'shortReference') stats.shortReferences++;
      else if (token.type === 'mediumReference') stats.mediumReferences++;
      else if (token.type === 'longReference') stats.longReferences++;
      if (token.distance > stats.maximumDistance) stats.maximumDistance = token.distance;
      var carriesVariable = false;
      for (var p = token.position; p < token.position + token.length; p++) {
        if (variable[p]) carriesVariable = true;
      }
      if (carriesVariable) {
        stats.variableLiteralTokens.push([token.position, token.length]);
        if (token.type !== 'literal') {
          throw new Error('Healing-description plan admitted a non-literal variable token.');
        }
      }
    }
    return stats;
  }

  function assertHealingPlanIdentity(plan) {
    var constants = HEALING_DESCRIPTION_CONSTANTS;
    var stats = healingPlanStats(plan);
    var planHash = sha256HexSync(serializeHealingPlan(plan));
    if (stats.byteLength !== constants.canonicalConsumed ||
        stats.tokens !== constants.planTokens ||
        stats.literals !== constants.planLiterals ||
        stats.shortReferences !== constants.planShortReferences ||
        stats.mediumReferences !== constants.planMediumReferences ||
        stats.longReferences !== constants.planLongReferences ||
        stats.maximumDistance !== constants.planMaximumDistance ||
        JSON.stringify(stats.variableLiteralTokens) !== JSON.stringify(constants.variableLiteralTokens) ||
        planHash !== constants.planSha256) {
      throw new Error('Deterministic healing-description plan identity does not match the reviewed policy.');
    }
    return {
      sha256: planHash,
      stats: stats
    };
  }

  function canonicalHealingPlan(decoded) {
    if (cachedHealingPlan) return cachedHealingPlan;
    var plan = buildHealingPlan(decoded);
    assertHealingPlanIdentity(plan);
    cachedHealingPlan = plan;
    return cachedHealingPlan;
  }

  function encodeHealingPlan(decodedInput, plan) {
    var decoded = asBytes(decodedInput);
    var bytes = [];
    for (var i = 0; i < plan.length; i++) {
      var token = plan[i];
      var length = token.length;
      var packedDistance = token.distance - 1;
      if (token.type === 'literal') {
        bytes.push(0x40 | (length - 1));
        for (var literalIndex = 0; literalIndex < length; literalIndex++) {
          bytes.push(decoded[token.position + literalIndex]);
        }
      } else if (token.type === 'shortReference') {
        bytes.push(
          0x80 | ((length - 3) << 3) | ((packedDistance >>> 8) & 0x07),
          packedDistance & 0xFF
        );
      } else if (token.type === 'mediumReference') {
        var packedLength = length - 4;
        bytes.push(
          0x10 | (packedLength & 0x0F),
          ((packedLength & 0x30) << 2) | ((packedDistance >>> 8) & 0x3F),
          packedDistance & 0xFF
        );
      } else if (token.type === 'longReference') {
        bytes.push(
          0x00,
          length - 5,
          (packedDistance >>> 8) & 0xFF,
          packedDistance & 0xFF
        );
      } else if (token.type === 'shortZeroFill') {
        bytes.push(0x20 | (length - 2));
      } else if (token.type === 'longZeroFill') {
        bytes.push(0x02, length - 3);
      } else if (token.type === 'ffFill') {
        bytes.push(0x01, length - 3);
      } else {
        throw new Error('Unknown healing-description plan token "' + token.type + '".');
      }
    }
    return new Uint8Array(bytes);
  }

  function setHealingTriples(decodedInput, values) {
    var decoded = asBytes(decodedInput).slice();
    if (!Array.isArray(values) || values.length !== 3) {
      throw new Error('Exactly three healing magnitudes are required.');
    }
    for (var i = 0; i < values.length; i++) {
      var checked = validateMagnitude(MAGNITUDE_MODEL_ORDER[i], values[i]);
      var text = String(checked.magnitude).padStart(3, '0');
      var offset = HEALING_DESCRIPTION_CONSTANTS.tripleOffsets[i];
      for (var digit = 0; digit < 3; digit++) decoded[offset + digit] = text.charCodeAt(digit);
    }
    return decoded;
  }

  function buildHealingDescriptionSlot(decodedInput, values, importedSlot) {
    var constants = HEALING_DESCRIPTION_CONSTANTS;
    var decoded = setHealingTriples(decodedInput, values);
    if (sha256HexSync(healingImmutableProjection(decoded)) !== constants.immutableProjectionSha256 ||
        sha256HexSync(zeroHealingDigits(decoded)) !== constants.zeroedDecodedSha256) {
      throw new Error('Healing-description immutable decoded projection changed before encoding.');
    }
    var plan = canonicalHealingPlan(decoded);
    var planIdentity = assertHealingPlanIdentity(plan);
    var stream = encodeHealingPlan(decoded, plan);
    if (stream.length !== constants.canonicalConsumed) {
      throw new Error('Healing-description stream consumed ' + stream.length +
        ' bytes; reviewed policy requires ' + constants.canonicalConsumed + '.');
    }
    var template = importedSlot ? asBytes(importedSlot) : new Uint8Array(constants.ownerSize);
    if (template.length !== constants.ownerSize) {
      throw new Error('Healing-description physical owner must be exactly ' + constants.ownerSize + ' bytes.');
    }
    var slot = template.slice();
    slot.set(hexToBytes(constants.headerHex), 0);
    slot.set(stream, 8);
    slot.fill(constants.canonicalPaddingByte, 8 + stream.length, 8 + constants.streamCapacity);
    slot[slot.length - 1] = constants.alignment;
    var primary = decodeLzssStrict(
      slot.subarray(8, 8 + constants.streamCapacity),
      constants.decodedSize
    );
    var alternate = decodeLzssStrictAlternate(
      slot.subarray(8, 8 + constants.streamCapacity),
      constants.decodedSize
    );
    if (primary.bytesConsumed !== constants.canonicalConsumed ||
        alternate.bytesConsumed !== constants.canonicalConsumed ||
        !equalBytes(primary.output, alternate.output) ||
        !equalBytes(primary.output, decoded)) {
      throw new Error('Healing-description dual strict decoders rejected the canonical stream.');
    }
    for (var padding = 8 + constants.canonicalConsumed;
        padding < 8 + constants.streamCapacity;
        padding++) {
      if (slot[padding] !== constants.canonicalPaddingByte) {
        throw new Error('Healing-description canonical padding is not 0x03.');
      }
    }
    return {
      slot: slot,
      decoded: decoded,
      stream: stream,
      values: values.slice(),
      bytesConsumed: primary.bytesConsumed,
      planSha256: planIdentity.sha256,
      planStats: planIdentity.stats,
      slotSha256: sha256HexSync(slot)
    };
  }

  function inspectHealingDescription(z64, revisionManifest) {
    var constants = HEALING_DESCRIPTION_CONSTANTS;
    var description = revisionManifest && revisionManifest.description;
    var revisionLabel = revisionManifest ? revisionManifest.label : 'Unknown revision';
    function fail(message, details) {
      return {
        ok: false,
        facet: 'healing descriptions',
        revision: revisionLabel,
        reason: revisionLabel + ' healing descriptions: ' + message,
        errors: [revisionLabel + ' healing descriptions: ' + message],
        details: details || null
      };
    }
    if (!description || !z64 || description.nextHeaderOffset + 8 > z64.length) {
      return fail('physical slot lies outside the normalized image.');
    }
    var slot = z64.slice(description.start, description.end);
    var expectedHeader = hexToBytes(constants.headerHex);
    if (!equalBytes(slot.subarray(0, 8), expectedHeader)) {
      return fail('header at ' + hex(description.start, 8) + ' expected ' +
        constants.headerHex + ' but found ' + bytesToHex(slot.subarray(0, 8)) + '.');
    }
    if (readU32(slot, 0) !== constants.payloadSize ||
        readU32(slot, 4) !== constants.decodedSize) {
      return fail('payload/output size fields do not match 0x80B/0xCA2.');
    }
    if (slot[slot.length - 1] !== constants.alignment) {
      return fail('alignment byte at ' + hex(description.end - 1, 8) +
        ' expected 0x00 but found ' + hex(slot[slot.length - 1], 2) + '.');
    }
    var nextHeader = z64.subarray(description.nextHeaderOffset, description.nextHeaderOffset + 8);
    if (bytesToHex(nextHeader) !== constants.nextHeaderHex) {
      return fail('next header at ' + hex(description.nextHeaderOffset, 8) +
        ' expected ' + constants.nextHeaderHex + ' but found ' + bytesToHex(nextHeader) + '.');
    }
    var primary;
    var alternate;
    try {
      var stream = slot.subarray(8, 8 + constants.streamCapacity);
      primary = decodeLzssStrict(stream, constants.decodedSize);
      alternate = decodeLzssStrictAlternate(stream, constants.decodedSize);
    } catch (error) {
      return fail(error.message);
    }
    if (!equalBytes(primary.output, alternate.output)) {
      return fail('the two strict decoders disagree over the 3,234-byte output.');
    }
    if (primary.bytesConsumed !== alternate.bytesConsumed) {
      return fail('the two strict decoders disagree over compressed bytes consumed (' +
        primary.bytesConsumed + ' versus ' + alternate.bytesConsumed + ').');
    }
    var projectionHash = sha256HexSync(healingImmutableProjection(primary.output));
    if (projectionHash !== constants.immutableProjectionSha256) {
      return fail('immutable decoded projection expected SHA-256 ' +
        constants.immutableProjectionSha256 + ' but found ' + projectionHash + '.');
    }
    var zeroedHash = sha256HexSync(zeroHealingDigits(primary.output));
    if (zeroedHash !== constants.zeroedDecodedSha256) {
      return fail('zeroed decoded block expected SHA-256 ' +
        constants.zeroedDecodedSha256 + ' but found ' + zeroedHash + '.');
    }
    var values = [];
    for (var tripleIndex = 0;
        tripleIndex < constants.tripleOffsets.length;
        tripleIndex++) {
      var tripleOffset = constants.tripleOffsets[tripleIndex];
      var text = '';
      for (var digitIndex = 0; digitIndex < 3; digitIndex++) {
        var digitValue = primary.output[tripleOffset + digitIndex];
        if (digitValue < 0x30 || digitValue > 0x39) {
          return fail('decoded digit at ' + hex(tripleOffset + digitIndex, 4) +
            ' is not ASCII 0-9.');
        }
        text += String.fromCharCode(digitValue);
      }
      values.push(Number(text));
    }
    return {
      ok: true,
      facet: 'healing descriptions',
      revision: revisionLabel,
      reason: '',
      errors: [],
      slot: slot,
      slotSha256: sha256HexSync(slot),
      decoded: primary.output,
      values: values,
      paddedValues: values.map(function(value) { return String(value).padStart(3, '0'); }),
      bytesConsumed: primary.bytesConsumed,
      alternateBytesConsumed: alternate.bytesConsumed,
      nextHeader: nextHeader.slice(),
      immutableProjectionSha256: projectionHash
    };
  }

  function wordKey(offset) {
    return String(offset >>> 0);
  }

  function initialLedgerWords(z64) {
    var out = {};
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      out[wordKey(entry.offset)] = z64 && entry.offset + 4 <= z64.length
        ? readU32(z64, entry.offset)
        : entry.vanilla >>> 0;
    }
    return out;
  }

  function freezeCompleteWordImage(words, phase) {
    var label = phase || 'word-image';
    var keys = Object.keys(words || {}).sort();
    var expectedKeys = EDITABLE_WORD_GUARDS.map(function(entry) {
      return wordKey(entry.offset);
    }).sort();
    if (keys.length !== expectedKeys.length ||
        expectedKeys.some(function(key, index) { return key !== keys[index]; })) {
      throw new Error('Consumable effect ' + label +
        ' expectation must contain exactly all 16 editable words.');
    }
    var frozen = {};
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      var key = wordKey(entry.offset);
      var value = words[key];
      if (!Number.isInteger(value) || value < 0 || value > 0xFFFFFFFF) {
        throw new Error('Consumable effect ' + label + ' expectation at normalized z64 ' +
          hex(entry.offset, 8) + ' is not a valid 32-bit word.');
      }
      frozen[key] = value >>> 0;
    }
    return Object.freeze(frozen);
  }

  function readCompleteWordImage(z64, phase) {
    var label = phase || 'word-image';
    if (!z64) {
      throw new Error('Consumable effect ' + label +
        ' validation has no normalized candidate.');
    }
    var words = {};
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      if (entry.offset < 0 || entry.offset + 4 > z64.length) {
        throw new Error('Consumable effect ' + label + ' word at normalized z64 ' +
          hex(entry.offset, 8) + ' lies outside the image.');
      }
      words[wordKey(entry.offset)] = readU32(z64, entry.offset);
    }
    return Object.freeze(words);
  }

  function assertCompleteWordImage(z64, expectedWords, phase) {
    var label = phase || 'word-image';
    var expected = freezeCompleteWordImage(expectedWords, label);
    var actual = readCompleteWordImage(z64, label);
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      var key = wordKey(entry.offset);
      if ((actual[key] >>> 0) !== (expected[key] >>> 0)) {
        throw new Error('Consumable effect ' + label +
          ' mismatch at normalized z64 ' + hex(entry.offset, 8) +
          ': expected ' + hex(expected[key]) + ' but found ' +
          hex(actual[key]) + '.');
      }
    }
    return actual;
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
    manifest = manifest && manifest.rangeProfile
      ? manifest.rangeProfile
      : (manifest || GUARD_MANIFEST);
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

  function revisionManifestFor(identity, sourceMetadata) {
    if (sourceMetadata && sourceMetadata.revisionManifest) {
      return sourceMetadata.revisionManifest;
    }
    var revision = identity && identity.revision;
    var base = revision && REVISION_MANIFESTS[revision];
    if (!base) return null;
    if (sourceMetadata && sourceMetadata.guardManifest) {
      return Object.assign({}, base, {
        rangeProfile: sourceMetadata.guardManifest
      });
    }
    return base;
  }

  function hashGuardProjection(z64, guard) {
    var omitted = {};
    (guard.omittedOffsets || []).forEach(function(offset) {
      for (var byteIndex = 0; byteIndex < 4; byteIndex++) {
        omitted[offset + byteIndex] = true;
      }
    });
    var bytes = new Uint8Array(
      (guard.end - guard.start) - Object.keys(omitted).filter(function(key) {
        var offset = Number(key);
        return offset >= guard.start && offset < guard.end;
      }).length
    );
    var cursor = 0;
    for (var offset = guard.start; offset < guard.end; offset++) {
      if (!omitted[offset]) bytes[cursor++] = z64[offset];
    }
    return bytes;
  }

  function validateCommonProfile(z64, revisionManifest) {
    var profile = revisionManifest && revisionManifest.commonProfile;
    var revisionLabel = revisionManifest ? revisionManifest.label : 'Unknown revision';
    function failure(facet, message, details) {
      var full = revisionLabel + ' ' + facet + ': ' + message;
      return {
        ok: false,
        facet: facet,
        revision: revisionLabel,
        reason: full,
        errors: [full],
        details: details || null
      };
    }
    if (!profile) return failure('IDs 1-5 common profile', 'no explicit revision manifest is selected.');
    for (var operandIndex = 0;
        operandIndex < profile.helperOperands.length;
        operandIndex++) {
      var operand = profile.helperOperands[operandIndex];
      if (!z64 || operand.offset + 4 > z64.length) {
        return failure('IDs 1-5 common profile',
          'helper operand at ' + hex(operand.offset, 8) + ' lies outside the image.');
      }
      var actualWord = readU32(z64, operand.offset);
      if (actualWord !== (operand.expected >>> 0)) {
        return failure('IDs 1-5 common profile',
          'helper operand at ' + hex(operand.offset, 8) + ' expected ' +
          hex(operand.expected) + ' but found ' + hex(actualWord) + '.',
          { offset: operand.offset, expected: operand.expected >>> 0, actual: actualWord });
      }
    }
    for (var guardIndex = 0; guardIndex < profile.hashGuards.length; guardIndex++) {
      var guard = profile.hashGuards[guardIndex];
      if (!z64 || guard.end > z64.length) {
        return failure(guard.label,
          'range [' + hex(guard.start, 8) + ',' + hex(guard.end, 8) + ') lies outside the image.');
      }
      var actualHash = sha256HexSync(hashGuardProjection(z64, guard));
      if (actualHash !== guard.sha256) {
        return failure(guard.label,
          'range [' + hex(guard.start, 8) + ',' + hex(guard.end, 8) +
          ') expected SHA-256 ' + guard.sha256 + ' but found ' + actualHash + '.',
          { start: guard.start, end: guard.end, expected: guard.sha256, actual: actualHash });
      }
    }
    for (var scopeIndex = 0; scopeIndex < profile.scopeBytes.length; scopeIndex++) {
      var scope = profile.scopeBytes[scopeIndex];
      if (!z64 || scope.offset >= z64.length) {
        return failure('IDs 1-5 target scopes',
          'ID ' + scope.id + ' scope at ' + hex(scope.offset, 8) + ' lies outside the image.');
      }
      if (z64[scope.offset] !== scope.expected) {
        return failure('IDs 1-5 target scopes',
          'ID ' + scope.id + ' at ' + hex(scope.offset, 8) + ' expected ' +
          hex(scope.expected, 2) + ' but found ' + hex(z64[scope.offset], 2) + '.',
          { offset: scope.offset, expected: scope.expected, actual: z64[scope.offset] });
      }
    }
    return {
      ok: true,
      facet: 'IDs 1-5 common profile',
      revision: revisionLabel,
      reason: '',
      errors: []
    };
  }

  function decodeRangeModel(z64, modelKey) {
    var def = MODEL_DEFS[modelKey];
    var widthWord = readU32(z64, def.widthOffset);
    var minimumWord = readU32(z64, def.minimumOffset);
    var width = widthWord & 0xFFFF;
    var minimumRaw = minimumWord & 0xFFFF;
    var minimum = minimumRaw & 0x8000 ? minimumRaw - 0x10000 : minimumRaw;
    var maximum = minimum + width - 1;
    return validateRange(modelKey, minimum, maximum);
  }

  function validateRangeProfileAndImport(z64, session, revisionManifest) {
    var revisionLabel = revisionManifest ? revisionManifest.label : 'Unknown revision';
    var rangeProfile = revisionManifest && revisionManifest.rangeProfile;
    if (!rangeProfile) {
      return {
        ok: false,
        facet: 'IDs 10-19 range profile',
        revision: revisionLabel,
        reason: revisionLabel + ' IDs 10-19 range profile: no explicit profile is selected.',
        errors: ['No range profile selected.'],
        models: {}
      };
    }
    var guards = validateGuards(z64, session, rangeProfile);
    if (!guards.ok) {
      return {
        ok: false,
        facet: 'IDs 10-19 range profile',
        revision: revisionLabel,
        reason: revisionLabel + ' IDs 10-19 range profile: ' + guards.errors[0],
        errors: guards.errors,
        models: {}
      };
    }
    var models = {};
    try {
      for (var i = 0; i < RANGE_MODEL_ORDER.length; i++) {
        var key = RANGE_MODEL_ORDER[i];
        var decoded = decodeRangeModel(z64, key);
        models[key] = { minimum: decoded.minimum, maximum: decoded.maximum };
      }
    } catch (error) {
      return {
        ok: false,
        facet: 'IDs 10-19 range profile',
        revision: revisionLabel,
        reason: revisionLabel + ' IDs 10-19 range profile: ' + error.message,
        errors: [error.message],
        models: {}
      };
    }
    return {
      ok: true,
      facet: 'IDs 10-19 range profile',
      revision: revisionLabel,
      reason: '',
      errors: [],
      models: models
    };
  }

  function validateMagnitudeFacet(z64, session, revisionManifest, modelKey) {
    var def = MODEL_DEFS[modelKey];
    var revisionLabel = revisionManifest ? revisionManifest.label : 'Unknown revision';
    function fail(message, details) {
      return {
        ok: false,
        facet: def ? ('ID ' + def.itemIds[0] + ' magnitude') : 'unknown magnitude',
        revision: revisionLabel,
        reason: revisionLabel + ' ID ' + (def ? def.itemIds[0] : '?') +
          ' magnitude: ' + message,
        errors: [message],
        details: details || null
      };
    }
    if (!def || def.kind !== 'magnitude') return fail('unknown magnitude model.');
    var values = [];
    var words = [];
    for (var i = 0; i < def.magnitudeOffsets.length; i++) {
      var offset = def.magnitudeOffsets[i];
      if (!z64 || offset + 4 > z64.length) {
        return fail('word at ' + hex(offset, 8) + ' lies outside the image.');
      }
      var word = readU32(z64, offset);
      var expectedPreimage = session && session.ledger && session.ledger.currentWords &&
        session.ledger.currentWords[wordKey(offset)];
      if (expectedPreimage != null && word !== (expectedPreimage >>> 0)) {
        return fail('word at ' + hex(offset, 8) + ' expected ledger preimage ' +
          hex(expectedPreimage) + ' but found ' + hex(word) + '.',
          { offset: offset, expected: expectedPreimage >>> 0, actual: word });
      }
      if ((word & 0xFFFF0000) !== def.magnitudeForms[i]) {
        return fail('word at ' + hex(offset, 8) + ' expected opcode/register form ' +
          hex(def.magnitudeForms[i]) + ' but found ' + hex(word) + '.',
          { offset: offset, expectedForm: def.magnitudeForms[i], actual: word });
      }
      values.push(word & 0xFFFF);
      words.push(word);
    }
    if (def.pairAtomic && values.some(function(value) { return value !== values[0]; })) {
      return fail('the two required storage words are unequal (' +
        values.map(String).join(' versus ') + ').');
    }
    try {
      validateMagnitude(modelKey, values[0]);
    } catch (error) {
      return fail(error.message);
    }
    return {
      ok: true,
      facet: 'ID ' + def.itemIds[0] + ' magnitude',
      revision: revisionLabel,
      reason: '',
      errors: [],
      modelKey: modelKey,
      magnitude: values[0],
      words: words
    };
  }

  function reconcileHealingDescription(descriptionFacet, magnitudeFacets, revisionManifest) {
    var revisionLabel = revisionManifest ? revisionManifest.label : 'Unknown revision';
    if (!descriptionFacet || !descriptionFacet.ok) return descriptionFacet;
    var codeValues = MAGNITUDE_MODEL_ORDER.slice(0, 3).map(function(modelKey) {
      var facet = magnitudeFacets[modelKey];
      return facet && facet.ok ? facet.magnitude : null;
    });
    if (codeValues.some(function(value) { return value == null; })) {
      return {
        ok: false,
        facet: 'healing description reconciliation',
        revision: revisionLabel,
        reason: revisionLabel +
          ' healing description reconciliation: one or more healing code magnitudes are unavailable.',
        errors: ['One or more healing code magnitudes are unavailable.'],
        codeValues: codeValues,
        textValues: descriptionFacet.values.slice()
      };
    }
    for (var i = 0; i < codeValues.length; i++) {
      if (codeValues[i] !== descriptionFacet.values[i]) {
        var offset = HEALING_DESCRIPTION_CONSTANTS.tripleOffsets[i];
        return {
          ok: false,
          facet: 'healing description reconciliation',
          revision: revisionLabel,
          reason: revisionLabel + ' healing description reconciliation: ID ' +
            (i + 1) + ' code magnitude ' + codeValues[i] +
            ' disagrees with prose ' + descriptionFacet.paddedValues[i] +
            ' at decoded ' + hex(offset, 4) + '.',
          errors: ['ID ' + (i + 1) + ' code/text disagreement.'],
          codeValues: codeValues,
          textValues: descriptionFacet.values.slice(),
          details: { itemId: i + 1, decodedOffset: offset }
        };
      }
    }
    return Object.assign({}, descriptionFacet, {
      ok: true,
      facet: 'healing description reconciliation',
      codeValues: codeValues,
      textValues: descriptionFacet.values.slice()
    });
  }

  function modelAvailability(session, modelKey) {
    if (!session || !session.identity || !session.identity.eligible) {
      return {
        ok: false,
        reason: session && session.identity && session.identity.reason ||
          'Load a compatible normalized US ROM.'
      };
    }
    var def = MODEL_DEFS[modelKey];
    if (!def) return { ok: false, reason: 'Unknown consumable effect model.' };
    if (!session.availability || !session.availability.magnitudes) {
      return {
        ok: false,
        reason: 'Consumable-effect feature availability was not initialized.'
      };
    }
    if (def.kind === 'range') return session.availability.range;
    var common = session.availability.common;
    var item = session.availability.magnitudes[modelKey];
    if (!common.ok) return common;
    if (!item || !item.ok) return item || { ok: false, reason: 'Magnitude facet is unavailable.' };
    if (def.healingDescription && !session.availability.descriptions.ok) {
      return session.availability.descriptions;
    }
    return { ok: true, reason: '', revision: session.revisionManifest.label };
  }

  function initializeSession(rom, identity, sourceMetadata) {
    if (!rom || !rom.z64) throw new Error('A parsed ROM is required to initialize consumable effects.');
    var immutableIdentity = identity || evaluateSourceIdentity({});
    var revisionManifest = revisionManifestFor(immutableIdentity, sourceMetadata);
    var initialModels = vanillaModels();
    var session = {
      identity: immutableIdentity,
      revisionManifest: revisionManifest,
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
      // Retain the old public field for callers that only know the accepted
      // IDs 10-19 range profile. New code uses revisionManifest explicitly.
      guardManifest: revisionManifest && revisionManifest.rangeProfile ||
        sourceMetadata && sourceMetadata.guardManifest || GUARD_MANIFEST,
      models: initialModels,
      // Immutable Project-diff baseline from the source ROM loaded by the user.
      // Verified export adoption advances the ledger, not this baseline.
      importedModels: cloneModels(initialModels),
      availability: {
        range: { ok: false, reason: 'No supported revision profile is selected.' },
        common: { ok: false, reason: 'No supported revision profile is selected.' },
        magnitudes: {},
        descriptions: { ok: false, reason: 'No supported revision profile is selected.' }
      },
      generation: 0,
      pendingWrites: false,
      lastError: '',
      ledger: {
        sourceSha256: immutableIdentity.facts && immutableIdentity.facts.sha256 || '',
        currentWords: initialLedgerWords(rom.z64),
        currentDescriptionSlot: null,
        currentDescriptionDecoded: null,
        currentDescriptionValues: null,
        initialDescriptionSlot: null,
        effectOwnedWrites: [],
        descriptionOwnedWrite: null,
        priorOwnerRegions: [],
        headerCrcWrites: [],
        lastChangeRanges: [],
        lastCandidate: null,
        exports: []
      }
    };

    if (session.identity.eligible && revisionManifest) {
      var rangeFacet = validateRangeProfileAndImport(rom.z64, session, revisionManifest);
      var commonFacet = validateCommonProfile(rom.z64, revisionManifest);
      var magnitudeFacets = {};
      for (var magnitudeIndex = 0;
          magnitudeIndex < MAGNITUDE_MODEL_ORDER.length;
          magnitudeIndex++) {
        var magnitudeKey = MAGNITUDE_MODEL_ORDER[magnitudeIndex];
        magnitudeFacets[magnitudeKey] = validateMagnitudeFacet(
          rom.z64,
          session,
          revisionManifest,
          magnitudeKey
        );
        if (magnitudeFacets[magnitudeKey].ok) {
          initialModels[magnitudeKey] = {
            magnitude: magnitudeFacets[magnitudeKey].magnitude
          };
        }
      }
      if (rangeFacet.ok) {
        for (var rangeIndex = 0; rangeIndex < RANGE_MODEL_ORDER.length; rangeIndex++) {
          var rangeKey = RANGE_MODEL_ORDER[rangeIndex];
          initialModels[rangeKey] = {
            minimum: rangeFacet.models[rangeKey].minimum,
            maximum: rangeFacet.models[rangeKey].maximum
          };
        }
      }
      var descriptionFacet = inspectHealingDescription(rom.z64, revisionManifest);
      var reconciledDescriptions = reconcileHealingDescription(
        descriptionFacet,
        magnitudeFacets,
        revisionManifest
      );
      session.availability = {
        range: rangeFacet,
        common: commonFacet,
        magnitudes: magnitudeFacets,
        descriptions: reconciledDescriptions
      };
      session.models = cloneModels(initialModels);
      session.importedModels = cloneModels(initialModels);
      if (descriptionFacet.slot) {
        session.ledger.currentDescriptionSlot = descriptionFacet.slot.slice();
        session.ledger.initialDescriptionSlot = descriptionFacet.slot.slice();
      } else if (revisionManifest.description &&
          revisionManifest.description.end <= rom.z64.length) {
        session.ledger.currentDescriptionSlot = rom.z64.slice(
          revisionManifest.description.start,
          revisionManifest.description.end
        );
        session.ledger.initialDescriptionSlot =
          session.ledger.currentDescriptionSlot.slice();
      }
      if (descriptionFacet.ok) {
        session.ledger.currentDescriptionDecoded = descriptionFacet.decoded.slice();
        session.ledger.currentDescriptionValues = descriptionFacet.values.slice();
      }
    } else if (session.identity.eligible && !revisionManifest) {
      session.identity = Object.assign({}, immutableIdentity, {
        eligible: false,
        reason: 'Effect editing requires an explicit supported Rev 0 or Rev 1 manifest.'
      });
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
        : 'Consumable effect editing requires a compatible guarded ROM session.');
    }
  }

  function assertSessionOwnership(session, manifest) {
    assertEligible(session);
    var revisionManifest = session.revisionManifest ||
      (manifest && manifest.rangeProfile ? manifest : null);
    var identityHash = session.identity && session.identity.facts &&
      String(session.identity.facts.sha256 || '').toUpperCase();
    var sourceHash = session.source && String(session.source.sha256 || '').toUpperCase();
    var ledgerHash = session.ledger && String(session.ledger.sourceSha256 || '').toUpperCase();
    if (!identityHash || identityHash !== sourceHash || identityHash !== ledgerHash) {
      throw new Error('Consumable effect source/session ledger ownership does not match the loaded source.');
    }
    if (!revisionManifest || !revisionManifest.rangeProfile ||
        !revisionManifest.commonProfile || !revisionManifest.description) {
      throw new Error('Consumable effect session has no complete selected-revision manifest.');
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
    if (!session.models || !session.importedModels || !session.availability ||
        !session.availability.magnitudes) {
      throw new Error('Consumable effect semantic baseline or availability ledger is incomplete.');
    }
    var expectedWordKeys = EDITABLE_WORD_GUARDS.map(function(entry) {
      return wordKey(entry.offset);
    }).sort();
    var ledgerWordKeys = Object.keys(session.ledger.currentWords).sort();
    if (expectedWordKeys.length !== ledgerWordKeys.length ||
        expectedWordKeys.some(function(key, index) { return key !== ledgerWordKeys[index]; })) {
      throw new Error('Consumable effect session ledger does not own exactly the 16 editable words.');
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
      throw new Error('Consumable effects must contain exactly ten canonical models.');
    }
    return true;
  }

  function desiredWords(session) {
    var checked = validateAllModels(session.models);
    var out = Object.assign({}, session.ledger.currentWords);
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var def = MODEL_DEFS[key];
      var availability = modelAvailability(session, key);
      if (!availability.ok) continue;
      if (def.kind === 'magnitude') {
        var encodedMagnitudeValue = encodeMagnitude(key, checked[key]);
        encodedMagnitudeValue.words.forEach(function(entry) {
          out[wordKey(entry.offset)] = entry.word;
        });
      } else {
        var encoded = encodeRange(key, checked[key]);
        out[wordKey(def.widthOffset)] = encoded.widthWord;
        out[wordKey(def.minimumOffset)] = encoded.minimumWord;
      }
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
    if (def.kind === 'magnitude') {
      return value.magnitude !== def.vanillaMagnitude;
    }
    return value.minimum !== def.vanillaMin || value.maximum !== def.vanillaMax;
  }

  function modelDiffersFromBaseline(session, modelKey, value) {
    var baseline = session && session.importedModels && session.importedModels[modelKey];
    var def = MODEL_DEFS[modelKey];
    if (!baseline || !def) return true;
    return def.kind === 'magnitude'
      ? value.magnitude !== baseline.magnitude
      : value.minimum !== baseline.minimum || value.maximum !== baseline.maximum;
  }

  function hasDesiredEffects(session) {
    if (!session) return false;
    return MODEL_ORDER.some(function(key) {
      return modelDiffersFromBaseline(session, key, session.models[key]);
    });
  }

  function hasAppliedEffects(session) {
    if (!session || !session.ledger) return false;
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      if (!modelAvailability(session, entry.modelKey).ok) continue;
      if ((session.ledger.currentWords[wordKey(entry.offset)] >>> 0) !== (entry.vanilla >>> 0)) return true;
    }
    return false;
  }

  function setModelRange(session, modelKey, minimum, maximum) {
    assertSessionOwnership(session);
    var def = MODEL_DEFS[modelKey];
    if (!def || def.kind !== 'range') {
      throw new Error('Unknown consumable range model "' + modelKey + '".');
    }
    var availability = modelAvailability(session, modelKey);
    if (!availability.ok) throw new Error(availability.reason);
    var prospective = cloneModels(session.models);
    prospective[modelKey] = { minimum: minimum, maximum: maximum };
    var checked = validateAllModels(prospective);
    session.models = cloneModels(checked);
    session.generation++;
    session.lastError = '';
    refreshPending(session);
    return session.models[modelKey];
  }

  function setModelMagnitude(session, modelKey, magnitude) {
    assertSessionOwnership(session);
    var def = MODEL_DEFS[modelKey];
    if (!def || def.kind !== 'magnitude') {
      throw new Error('Unknown consumable magnitude model "' + modelKey + '".');
    }
    var availability = modelAvailability(session, modelKey);
    if (!availability.ok) throw new Error(availability.reason);
    var checkedMagnitude = validateMagnitude(modelKey, magnitude);
    var prospective = cloneModels(session.models);
    prospective[modelKey] = { magnitude: checkedMagnitude.magnitude };
    session.models = cloneModels(validateAllModels(prospective));
    session.generation++;
    session.lastError = '';
    refreshPending(session);
    return {
      magnitude: session.models[modelKey].magnitude,
      status: magnitudeStatus(modelKey, session.models[modelKey].magnitude)
    };
  }

  function setItemRange(session, itemId, minimum, maximum) {
    var key = ITEM_TO_MODEL[Number(itemId)];
    if (!key || MODEL_DEFS[key].kind !== 'range') {
      throw new Error('Item ID ' + itemId + ' has no supported effect range.');
    }
    return setModelRange(session, key, minimum, maximum);
  }

  function setItemMagnitude(session, itemId, magnitude) {
    var key = ITEM_TO_MODEL[Number(itemId)];
    if (!key || MODEL_DEFS[key].kind !== 'magnitude') {
      throw new Error('Item ID ' + itemId + ' has no supported effect magnitude.');
    }
    return setModelMagnitude(session, key, magnitude);
  }

  function resetModel(session, modelKey) {
    var def = MODEL_DEFS[modelKey];
    if (!def) throw new Error('Unknown consumable effect model "' + modelKey + '".');
    if (def.kind === 'magnitude') {
      return setModelMagnitude(session, modelKey, def.vanillaMagnitude);
    }
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
      var def = MODEL_DEFS[key];
      if (!modelDiffersFromBaseline(session, key, checked[key])) continue;
      out[def.projectKey] = def.kind === 'magnitude'
        ? { magnitude: checked[key].magnitude }
        : {
            deltaMin: checked[key].minimum,
            deltaMax: checked[key].maximum
          };
    }
    return out;
  }

  function validateProjectPayload(payload, session, patchVersion, z64) {
    if (payload === null) throw new Error('patches.consumableEffects must be an object, not null.');
    if (payload === undefined) return { entries: {}, modelCount: 0 };
    if (!isPlainObject(payload)) throw new Error('patches.consumableEffects must be an object.');
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
      var modelKey = PROJECT_TO_MODEL[projectKey];
      var def = MODEL_DEFS[modelKey];
      if (def.kind === 'magnitude') {
        if (patchVersion != null && patchVersion < 13) {
          throw new Error('Consumable effect "' + projectKey +
            '" requires Project format version 13.');
        }
        if (fields.length !== 1 || fields[0] !== 'magnitude') {
          throw new Error('Consumable effect "' + projectKey +
            '" must contain only magnitude.');
        }
        var checkedMagnitude = validateMagnitude(modelKey, entry.magnitude);
        normalized[modelKey] = { magnitude: checkedMagnitude.magnitude };
      } else {
        if (patchVersion != null && patchVersion < 12) {
          throw new Error('Consumable effect "' + projectKey +
            '" requires Project format version 12.');
        }
        if (fields.length !== 2 || fields.indexOf('deltaMin') === -1 ||
            fields.indexOf('deltaMax') === -1) {
          throw new Error('Consumable effect "' + projectKey +
            '" must contain only deltaMin and deltaMax.');
        }
        var checked = validateRange(modelKey, entry.deltaMin, entry.deltaMax);
        normalized[modelKey] = {
          minimum: checked.minimum,
          maximum: checked.maximum
        };
      }
      if (session) {
        var availability = modelAvailability(session, modelKey);
        if (!availability.ok) {
          throw new Error('Consumable effect "' + projectKey +
            '" is unavailable for this source: ' + availability.reason);
        }
      }
    }
    if (keys.length) assertSessionOwnership(session);
    var prospective = cloneModels(session ? session.models : vanillaModels());
    Object.keys(normalized).forEach(function(key) { prospective[key] = normalized[key]; });
    validateAllModels(prospective);
    if (keys.length && z64) {
      var normalizedKeys = Object.keys(normalized);
      validatePreparedFacets(
        session,
        z64,
        normalizedKeys,
        normalizedKeys.some(function(key) {
          return !!MODEL_DEFS[key].healingDescription;
        })
      );
    }
    return { entries: normalized, modelCount: keys.length };
  }

  function applyProjectPayload(session, validated) {
    if (!validated || !validated.modelCount) return 0;
    assertSessionOwnership(session);
    var prospective = cloneModels(session.models);
    Object.keys(validated.entries).forEach(function(key) {
      var availability = modelAvailability(session, key);
      if (!availability.ok) {
        throw new Error('Consumable effect "' + MODEL_DEFS[key].projectKey +
          '" is unavailable for this source: ' + availability.reason);
      }
      prospective[key] = validated.entries[key];
    });
    var checked = cloneModels(validateAllModels(prospective));
    session.models = checked;
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

  function effectCollisionOwner(sessionOrManifest) {
    var manifest = sessionOrManifest && sessionOrManifest.revisionManifest
      ? sessionOrManifest.revisionManifest
      : (sessionOrManifest && sessionOrManifest.rangeProfile
        ? sessionOrManifest
        : REVISION_MANIFESTS.rev0);
    var rangeProfile = manifest.rangeProfile;
    var commonProfile = manifest.commonProfile;
    var regions = [];
    for (var i = 0; i < rangeProfile.contextRanges.length; i++) {
      regions.push({
        kind: 'rom',
        start: rangeProfile.contextRanges[i].start,
        size: rangeProfile.contextRanges[i].end - rangeProfile.contextRanges[i].start,
        label: rangeProfile.contextRanges[i].id + ' guarded path'
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
      start: rangeProfile.dispatchTable.start,
      size: rangeProfile.dispatchTable.end - rangeProfile.dispatchTable.start,
      label: 'complete 19-word consumable dispatch table'
    });
    for (var m = 0; m < rangeProfile.targetMetadata.length; m++) {
      regions.push({
        kind: 'rom',
        start: rangeProfile.targetMetadata[m].offset,
        size: 1,
        label: 'ID ' + rangeProfile.targetMetadata[m].id + ' target-mode metadata'
      });
    }
    for (var h = 0; h < commonProfile.hashGuards.length; h++) {
      var guard = commonProfile.hashGuards[h];
      regions.push({
        kind: 'rom',
        start: guard.start,
        size: guard.end - guard.start,
        label: manifest.label + ' ' + guard.label + ' compatibility surface'
      });
    }
    regions.push({
      kind: 'rom',
      start: manifest.description.start,
      size: manifest.description.end - manifest.description.start,
      label: manifest.label + ' healing-description guarded physical slot'
    });
    regions.push({
      kind: 'rom',
      start: manifest.description.nextHeaderOffset,
      size: 8,
      label: manifest.label + ' healing-description next-header guard'
    });
    return {
      id: 'consumable-effects-guard-collision',
      name: 'Consumable Effects Guard/Collision Surface (' + manifest.label + ')',
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
      throw new Error('Consumable effect delta ownership must be the 16 concrete editable words.');
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

  function healingDescriptionOwner(revisionManifest) {
    if (!revisionManifest || !revisionManifest.description) {
      throw new Error('A selected revision manifest is required for healing-description ownership.');
    }
    return {
      id: 'consumable-healing-descriptions',
      name: 'Consumable Healing Descriptions',
      category: 'consumableEffects',
      regions: [{
        kind: 'rom',
        start: revisionManifest.description.start,
        size: revisionManifest.description.end - revisionManifest.description.start,
        label: revisionManifest.label + ' complete healing-description physical slot'
      }]
    };
  }

  function assertHealingDescriptionOwner(owner, revisionManifest) {
    var expected = revisionManifest && revisionManifest.description;
    var region = owner && owner.regions && owner.regions[0];
    if (!owner || owner.id !== 'consumable-healing-descriptions' ||
        owner.category !== 'consumableEffects' ||
        !expected || owner.regions.length !== 1 || !region ||
        region.kind !== 'rom' || region.start !== expected.start ||
        region.size !== expected.end - expected.start) {
      throw new Error('Healing-description delta ownership must be the complete selected-revision physical slot.');
    }
    return owner;
  }

  function modelHasNonRetailLedgerWords(session, modelKey) {
    return EDITABLE_WORD_GUARDS.some(function(entry) {
      return entry.modelKey === modelKey &&
        (session.ledger.currentWords[wordKey(entry.offset)] >>> 0) !==
          (entry.vanilla >>> 0);
    });
  }

  function validatePreparedFacets(session, z64, changedModels, requireHealingText) {
    var revisionManifest = session.revisionManifest;
    var rangeNeeded = changedModels.some(function(key) {
      return MODEL_DEFS[key].kind === 'range';
    });
    var magnitudeKeys = changedModels.filter(function(key) {
      return MODEL_DEFS[key].kind === 'magnitude';
    });
    if (rangeNeeded) {
      var rangeFacet = validateRangeProfileAndImport(z64, session, revisionManifest);
      if (!rangeFacet.ok) throw new Error(rangeFacet.reason);
    }
    var commonFacet = null;
    var liveMagnitudeFacets = {};
    if (magnitudeKeys.length || requireHealingText) {
      commonFacet = validateCommonProfile(z64, revisionManifest);
      if (!commonFacet.ok) throw new Error(commonFacet.reason);
      var keysToValidate = {};
      magnitudeKeys.forEach(function(key) { keysToValidate[key] = true; });
      if (requireHealingText) {
        MAGNITUDE_MODEL_ORDER.slice(0, 3).forEach(function(key) {
          keysToValidate[key] = true;
        });
      }
      Object.keys(keysToValidate).forEach(function(key) {
        var facet = validateMagnitudeFacet(z64, session, revisionManifest, key);
        if (!facet.ok) throw new Error(facet.reason);
        liveMagnitudeFacets[key] = facet;
      });
    }
    var descriptionFacet = null;
    if (requireHealingText) {
      descriptionFacet = inspectHealingDescription(z64, revisionManifest);
      if (!descriptionFacet.ok) throw new Error(descriptionFacet.reason);
      if (!session.ledger.currentDescriptionSlot ||
          !equalBytes(
            descriptionFacet.slot,
            session.ledger.currentDescriptionSlot
          )) {
        throw new Error(revisionManifest.label +
          ' healing descriptions: physical-slot ledger preimage changed.');
      }
      var reconciliation = reconcileHealingDescription(
        descriptionFacet,
        liveMagnitudeFacets,
        revisionManifest
      );
      if (!reconciliation.ok) throw new Error(reconciliation.reason);
      descriptionFacet = reconciliation;
    }
    return {
      common: commonFacet,
      magnitudes: liveMagnitudeFacets,
      descriptions: descriptionFacet
    };
  }

  function deriveEffectWrites(currentWords, wantedWords) {
    var writes = [];
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var entry = EDITABLE_WORD_GUARDS[i];
      var key = wordKey(entry.offset);
      var before = currentWords[key] >>> 0;
      var after = wantedWords[key] >>> 0;
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
    return writes;
  }

  function assertPreparedTransactionIntegrity(transaction, session) {
    if (!transaction || !session) {
      throw new Error('Consumable effect transaction/session is missing.');
    }
    if (!Object.isFrozen(transaction.sourceWords) ||
        !Object.isFrozen(transaction.desiredWords)) {
      throw new Error(
        'Consumable effect transaction must freeze its complete source and desired word images.'
      );
    }
    var ledgerKeys = Object.keys(session.ledger.currentWords).sort();
    var sourceKeys = Object.keys(transaction.sourceWords || {}).sort();
    if (ledgerKeys.length !== sourceKeys.length ||
        ledgerKeys.some(function(key, index) {
          return key !== sourceKeys[index] ||
            (session.ledger.currentWords[key] >>> 0) !==
              (transaction.sourceWords[key] >>> 0);
        })) {
      throw new Error('Consumable effect transaction source-word ledger changed after preflight.');
    }
    var expectedDesired = desiredWords(session);
    var desiredKeys = Object.keys(expectedDesired).sort();
    var transactionDesiredKeys = Object.keys(transaction.desiredWords || {}).sort();
    if (desiredKeys.length !== transactionDesiredKeys.length ||
        desiredKeys.some(function(key, index) {
          return key !== transactionDesiredKeys[index] ||
            (expectedDesired[key] >>> 0) !==
              (transaction.desiredWords[key] >>> 0);
        })) {
      throw new Error('Consumable effect transaction desired-word set changed after preflight.');
    }
    var expectedWrites = deriveEffectWrites(
      transaction.sourceWords,
      expectedDesired
    );
    if (!Array.isArray(transaction.writes) ||
        transaction.writes.length !== expectedWrites.length) {
      throw new Error('Consumable effect transaction no longer contains the complete atomic write set.');
    }
    for (var writeIndex = 0; writeIndex < expectedWrites.length; writeIndex++) {
      var expected = expectedWrites[writeIndex];
      var actual = transaction.writes[writeIndex];
      if (!actual || actual.modelKey !== expected.modelKey ||
          actual.role !== expected.role || actual.offset !== expected.offset ||
          (actual.beforeWord >>> 0) !== expected.beforeWord ||
          (actual.afterWord >>> 0) !== expected.afterWord) {
        throw new Error('Consumable effect transaction write set changed at index ' +
          writeIndex + '.');
      }
    }
    if (JSON.stringify(validateAllModels(transaction.models)) !==
        JSON.stringify(validateAllModels(session.models))) {
      throw new Error('Consumable effect transaction semantic model set changed after preflight.');
    }
    var healingChanged = expectedWrites.some(function(write) {
      return !!MODEL_DEFS[write.modelKey].healingDescription;
    });
    if (healingChanged) {
      if (!transaction.descriptionWrite || !transaction.textOwner) {
        throw new Error('Healing transaction is missing its atomic complete-slot write.');
      }
      assertHealingDescriptionOwner(transaction.textOwner, session.revisionManifest);
      var expectedValues = MAGNITUDE_MODEL_ORDER.slice(0, 3).map(function(key) {
        return session.models[key].magnitude;
      });
      if (transaction.descriptionWrite.offset !==
          session.revisionManifest.description.start ||
          transaction.descriptionWrite.beforeBytes.length !==
            HEALING_DESCRIPTION_CONSTANTS.ownerSize ||
          transaction.descriptionWrite.afterBytes.length !==
            HEALING_DESCRIPTION_CONSTANTS.ownerSize ||
          JSON.stringify(transaction.descriptionWrite.values) !==
            JSON.stringify(expectedValues)) {
        throw new Error('Healing transaction complete-slot write changed after preflight.');
      }
      var rebuiltDescription = buildHealingDescriptionSlot(
        transaction.descriptionWrite.decoded,
        expectedValues,
        transaction.descriptionWrite.beforeBytes
      );
      if (!equalBytes(
            rebuiltDescription.decoded,
            transaction.descriptionWrite.decoded
          ) ||
          !equalBytes(
            rebuiltDescription.slot,
            transaction.descriptionWrite.afterBytes
          ) ||
          transaction.descriptionWrite.beforeSha256 !==
            sha256HexSync(transaction.descriptionWrite.beforeBytes) ||
          transaction.descriptionWrite.afterSha256 !==
            rebuiltDescription.slotSha256 ||
          transaction.descriptionWrite.bytesConsumed !==
            rebuiltDescription.bytesConsumed ||
          transaction.descriptionWrite.planSha256 !==
            rebuiltDescription.planSha256) {
        throw new Error(
          'Healing transaction complete-slot deterministic integrity changed after preflight.'
        );
      }
    } else if (transaction.descriptionWrite || transaction.textOwner) {
      throw new Error('A non-healing transaction unexpectedly owns the healing-description slot.');
    }
    assertEffectDeltaOwner(transaction.deltaOwner);
    return true;
  }

  function prepareTransaction(session, z64, otherOwners) {
    if (!session) return null;
    refreshPending(session);
    var relevant = session.pendingWrites || hasDesiredEffects(session) || hasAppliedEffects(session);
    if (!relevant) return null;
    assertSessionOwnership(session);
    var sourceWords = assertCompleteWordImage(
      z64,
      freezeCompleteWordImage(session.ledger.currentWords, 'prepare/source'),
      'prepare/source'
    );
    var checked = validateAllModels(session.models);
    assertSharedBinding(session);

    var wanted = freezeCompleteWordImage(desiredWords(session), 'prepare/desired');
    var writes = deriveEffectWrites(sourceWords, wanted);

    var changedModelSet = {};
    writes.forEach(function(write) { changedModelSet[write.modelKey] = true; });
    var changedModels = Object.keys(changedModelSet);
    var healingChanged = changedModels.some(function(key) {
      return MODEL_DEFS[key].healingDescription;
    });
    validatePreparedFacets(session, z64, changedModels, healingChanged);

    var descriptionWrite = null;
    var textOwner = null;
    if (healingChanged) {
      var currentDescription = inspectHealingDescription(z64, session.revisionManifest);
      if (!currentDescription.ok) throw new Error(currentDescription.reason);
      var liveFacets = {};
      MAGNITUDE_MODEL_ORDER.slice(0, 3).forEach(function(key) {
        liveFacets[key] = validateMagnitudeFacet(
          z64,
          session,
          session.revisionManifest,
          key
        );
      });
      var currentReconciliation = reconcileHealingDescription(
        currentDescription,
        liveFacets,
        session.revisionManifest
      );
      if (!currentReconciliation.ok) throw new Error(currentReconciliation.reason);
      var healingValues = MAGNITUDE_MODEL_ORDER.slice(0, 3).map(function(key) {
        return checked[key].magnitude;
      });
      var encodedDescription = buildHealingDescriptionSlot(
        currentDescription.decoded,
        healingValues,
        currentDescription.slot
      );
      if (equalBytes(encodedDescription.slot, currentDescription.slot)) {
        throw new Error('A pending healing magnitude change produced no physical description-slot delta.');
      }
      descriptionWrite = {
        offset: session.revisionManifest.description.start,
        beforeBytes: currentDescription.slot.slice(),
        afterBytes: encodedDescription.slot.slice(),
        beforeSha256: currentDescription.slotSha256,
        afterSha256: encodedDescription.slotSha256,
        values: healingValues,
        decoded: encodedDescription.decoded.slice(),
        bytesConsumed: encodedDescription.bytesConsumed,
        planSha256: encodedDescription.planSha256
      };
      textOwner = assertHealingDescriptionOwner(
        healingDescriptionOwner(session.revisionManifest),
        session.revisionManifest
      );
    }

    var collisionOwner = effectCollisionOwner(session);
    var deltaOwner = assertEffectDeltaOwner(effectDeltaOwner());
    var owners = (otherOwners || []).filter(function(otherOwner) {
      // Same-session prior effect ranges are concrete restoration ownership,
      // not a foreign subsystem collision.
      return otherOwner.id !== deltaOwner.id &&
        otherOwner.id !== 'consumable-healing-descriptions' &&
        otherOwner.id !== 'consumable-descriptions-adopted';
    });
    owners.push(collisionOwner);
    var conflicts = findRegionConflicts(owners).filter(function(conflict) {
      return conflict.a.ownerId === collisionOwner.id ||
        conflict.b.ownerId === collisionOwner.id;
    });
    if (conflicts.length) {
      throw new Error('Patch region collision:\n  ' + conflictMessage(conflicts));
    }
    var exactOwners = owners.filter(function(owner) {
      return owner.id !== collisionOwner.id;
    });
    exactOwners.push(deltaOwner);
    if (textOwner) exactOwners.push(textOwner);
    var exactConflicts = findRegionConflicts(exactOwners).filter(function(conflict) {
      return conflict.a.ownerId === deltaOwner.id ||
        conflict.b.ownerId === deltaOwner.id ||
        conflict.a.ownerId === 'consumable-healing-descriptions' ||
        conflict.b.ownerId === 'consumable-healing-descriptions';
    });
    if (exactConflicts.length) {
      throw new Error('Patch region collision:\n  ' + conflictMessage(exactConflicts));
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
    if (descriptionWrite) {
      if (descriptionWrite.offset < 0 ||
          descriptionWrite.offset + descriptionWrite.beforeBytes.length > z64.length) {
        throw new Error('Healing-description write lies outside the candidate.');
      }
      if (!equalBytes(
        z64.subarray(
          descriptionWrite.offset,
          descriptionWrite.offset + descriptionWrite.beforeBytes.length
        ),
        descriptionWrite.beforeBytes
      )) {
        throw new Error('Healing-description physical-slot preimage changed before export.');
      }
    }

    var models = cloneModels(checked);
    var modelChanges = [];
    for (var modelIndex = 0; modelIndex < MODEL_ORDER.length; modelIndex++) {
      var modelKey = MODEL_ORDER[modelIndex];
      var def = MODEL_DEFS[modelKey];
      var modelWrites = writes.filter(function(writeEntry) {
        return writeEntry.modelKey === modelKey;
      });
      if (!modelWrites.length && !modelDiffersFromBaseline(session, modelKey, models[modelKey])) {
        continue;
      }
      if (def.kind === 'magnitude') {
        var encodedMagnitudeValue = encodeMagnitude(modelKey, models[modelKey]);
        modelChanges.push({
          kind: 'magnitude',
          modelKey: modelKey,
          projectKey: def.projectKey,
          itemIds: def.itemIds.slice(),
          baselineMagnitude: session.importedModels[modelKey].magnitude,
          magnitude: models[modelKey].magnitude,
          retailMagnitude: def.vanillaMagnitude,
          status: encodedMagnitudeValue.status,
          target: def.target,
          paddedDescription: def.healingDescription
            ? encodedMagnitudeValue.status.padded
            : null,
          pairAtomic: !!def.pairAtomic,
          previousWords: modelWrites.map(function(write) {
            return { role: write.role, offset: hex(write.offset, 6), word: hex(write.beforeWord) };
          }),
          candidateWords: encodedMagnitudeValue.words.map(function(entry) {
            return { role: entry.role, offset: hex(entry.offset, 6), word: hex(entry.word) };
          })
        });
      } else {
        var encoded = encodeRange(modelKey, models[modelKey]);
        var previousWidth = session.ledger.currentWords[wordKey(def.widthOffset)] >>> 0;
        var previousMinimum = session.ledger.currentWords[wordKey(def.minimumOffset)] >>> 0;
        modelChanges.push({
          kind: 'range',
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

    var transaction = {
      baseGeneration: session.generation,
      sourceWords: sourceWords,
      desiredWords: wanted,
      writes: writes,
      descriptionWrite: descriptionWrite,
      models: models,
      modelChanges: modelChanges,
      collisionOwner: collisionOwner,
      deltaOwner: deltaOwner,
      textOwner: textOwner,
      otherOwners: (otherOwners || []).slice(),
      applied: false
    };
    assertPreparedTransactionIntegrity(transaction, session);
    return transaction;
  }

  function applyTransaction(transaction, candidateZ64, session) {
    if (!transaction) return [];
    FINAL_AFTER_IMAGE_PROOFS.delete(transaction);
    if (!session || session.generation !== transaction.baseGeneration) {
      throw new Error('Consumable effect state changed after export preflight.');
    }
    assertPreparedTransactionIntegrity(transaction, session);
    assertCompleteWordImage(
      candidateZ64,
      transaction.sourceWords,
      'pre-apply candidate'
    );
    var changedModels = [];
    transaction.writes.forEach(function(write) {
      if (changedModels.indexOf(write.modelKey) === -1) changedModels.push(write.modelKey);
    });
    validatePreparedFacets(
      session,
      candidateZ64,
      changedModels,
      !!transaction.descriptionWrite
    );
    for (var i = 0; i < transaction.writes.length; i++) {
      var check = transaction.writes[i];
      if (readU32(candidateZ64, check.offset) !== (check.beforeWord >>> 0)) {
        throw new Error('Candidate word at ' + hex(check.offset, 6) +
          ' changed after preflight; no effect word was written.');
      }
    }
    if (transaction.descriptionWrite) {
      var description = transaction.descriptionWrite;
      if (!equalBytes(
        candidateZ64.subarray(
          description.offset,
          description.offset + description.beforeBytes.length
        ),
        description.beforeBytes
      )) {
        throw new Error('Candidate healing-description slot changed after preflight; no effect byte was written.');
      }
      var rebuilt = buildHealingDescriptionSlot(
        inspectHealingDescription(candidateZ64, session.revisionManifest).decoded,
        description.values,
        description.beforeBytes
      );
      if (!equalBytes(rebuilt.slot, description.afterBytes) ||
          rebuilt.planSha256 !== HEALING_DESCRIPTION_CONSTANTS.planSha256) {
        throw new Error('Healing-description deterministic result changed after preflight; no effect byte was written.');
      }
    }
    for (var w = 0; w < transaction.writes.length; w++) {
      writeU32(candidateZ64, transaction.writes[w].offset, transaction.writes[w].afterWord);
    }
    if (transaction.descriptionWrite) {
      candidateZ64.set(
        transaction.descriptionWrite.afterBytes,
        transaction.descriptionWrite.offset
      );
    }
    transaction.applied = true;
    return transaction.writes.concat(transaction.descriptionWrite
      ? [{
          ownerId: 'consumable-healing-descriptions',
          offset: transaction.descriptionWrite.offset,
          length: transaction.descriptionWrite.afterBytes.length,
          beforeSha256: transaction.descriptionWrite.beforeSha256,
          afterSha256: transaction.descriptionWrite.afterSha256
        }]
      : []);
  }

  function firstByteMismatch(actual, expected) {
    var length = Math.min(actual ? actual.length : 0, expected ? expected.length : 0);
    for (var i = 0; i < length; i++) {
      if (actual[i] !== expected[i]) return i;
    }
    return actual && expected && actual.length === expected.length ? -1 : length;
  }

  function finalDescriptionError(description, relativeOffset, message) {
    var offset = description.offset + Math.max(0, relativeOffset || 0);
    throw new Error(
      'Consumable effect final after-image/healing description mismatch at normalized z64 ' +
      hex(offset, 8) + ': ' + message
    );
  }

  function assertFinalHealingDescriptionAfterImage(
      transaction, candidateZ64, session) {
    var description = transaction.descriptionWrite;
    var manifest = session.revisionManifest;
    var manifestDescription = manifest && manifest.description;
    var constants = HEALING_DESCRIPTION_CONSTANTS;
    if (!manifestDescription ||
        description.offset !== manifestDescription.start ||
        description.offset + constants.ownerSize !== manifestDescription.end ||
        manifestDescription.nextHeaderOffset !== manifestDescription.end) {
      finalDescriptionError(
        description,
        0,
        'the prepared complete-slot boundary no longer matches the selected revision.'
      );
    }
    if (!candidateZ64 ||
        manifestDescription.nextHeaderOffset + 8 > candidateZ64.length) {
      finalDescriptionError(description, 0, 'the complete slot or following header is outside the image.');
    }
    var finalSlot = candidateZ64.slice(
      description.offset,
      description.offset + constants.ownerSize
    );
    var mismatch = firstByteMismatch(finalSlot, description.afterBytes);
    if (mismatch !== -1) {
      finalDescriptionError(
        description,
        mismatch,
        'expected byte ' + hex(description.afterBytes[mismatch], 2) +
          ' but found ' + hex(finalSlot[mismatch], 2) + '.'
      );
    }
    var finalSlotSha256 = sha256HexSync(finalSlot);
    if (finalSlotSha256 !== description.afterSha256) {
      finalDescriptionError(
        description,
        0,
        'complete-slot SHA-256 expected ' + description.afterSha256 +
          ' but found ' + finalSlotSha256 + '.'
      );
    }

    var inspected = inspectHealingDescription(candidateZ64, manifest);
    if (!inspected.ok) {
      finalDescriptionError(description, 0, inspected.reason);
    }
    if (inspected.bytesConsumed !== constants.canonicalConsumed ||
        inspected.alternateBytesConsumed !== constants.canonicalConsumed) {
      finalDescriptionError(
        description,
        8,
        'strict decoders must both consume exactly ' +
          constants.canonicalConsumed + ' bytes.'
      );
    }
    if (!equalBytes(inspected.decoded, description.decoded)) {
      finalDescriptionError(
        description,
        8,
        'strictly decoded output differs from the prepared 3,234-byte after-image.'
      );
    }
    if (inspected.immutableProjectionSha256 !==
        constants.immutableProjectionSha256) {
      finalDescriptionError(
        description,
        8,
        'immutable decoded projection does not match the reviewed hash.'
      );
    }
    var expectedPaddedValues = description.values.map(function(value) {
      return String(value).padStart(3, '0');
    });
    if (JSON.stringify(inspected.values) !== JSON.stringify(description.values) ||
        JSON.stringify(inspected.paddedValues) !==
          JSON.stringify(expectedPaddedValues)) {
      finalDescriptionError(
        description,
        8,
        'decoded digit triples do not equal the prepared zero-padded values.'
      );
    }
    for (var padding = 8 + constants.canonicalConsumed;
        padding < 8 + constants.streamCapacity;
        padding++) {
      if (finalSlot[padding] !== constants.canonicalPaddingByte) {
        finalDescriptionError(
          description,
          padding,
          'canonical padding expected 0x03 but found ' +
            hex(finalSlot[padding], 2) + '.'
        );
      }
    }
    if (finalSlot[finalSlot.length - 1] !== constants.alignment) {
      finalDescriptionError(
        description,
        finalSlot.length - 1,
        'alignment byte does not match the prepared boundary.'
      );
    }
    if (bytesToHex(candidateZ64.subarray(
          manifestDescription.nextHeaderOffset,
          manifestDescription.nextHeaderOffset + 8
        )) !== constants.nextHeaderHex) {
      finalDescriptionError(
        description,
        constants.ownerSize,
        'the following header changed, indicating a boundary spill.'
      );
    }

    var rebuilt = buildHealingDescriptionSlot(
      description.decoded,
      description.values,
      description.beforeBytes
    );
    if (!equalBytes(rebuilt.slot, finalSlot) ||
        rebuilt.slotSha256 !== description.afterSha256 ||
        rebuilt.planSha256 !== description.planSha256 ||
        rebuilt.planSha256 !== constants.planSha256 ||
        rebuilt.bytesConsumed !== description.bytesConsumed ||
        rebuilt.bytesConsumed !== constants.canonicalConsumed) {
      finalDescriptionError(
        description,
        0,
        'deterministic plan, hash, consumed length, or complete-slot rebuild changed.'
      );
    }
    return {
      slot: finalSlot,
      slotSha256: finalSlotSha256,
      decoded: inspected.decoded.slice(),
      values: inspected.values.slice(),
      paddedValues: inspected.paddedValues.slice(),
      bytesConsumed: inspected.bytesConsumed,
      planSha256: rebuilt.planSha256
    };
  }

  function validateFinalAfterImage(transaction, candidateZ64, session) {
    if (!transaction) return null;
    FINAL_AFTER_IMAGE_PROOFS.delete(transaction);
    if (!session || session.generation !== transaction.baseGeneration) {
      throw new Error('Consumable effect final after-image belongs to a stale transaction.');
    }
    assertPreparedTransactionIntegrity(transaction, session);
    if ((transaction.writes.length || transaction.descriptionWrite) &&
        !transaction.applied) {
      throw new Error(
        'Consumable effect final after-image cannot be validated before application.'
      );
    }
    var finalWords = assertCompleteWordImage(
      candidateZ64,
      transaction.desiredWords,
      'final after-image'
    );
    var finalDescription = transaction.descriptionWrite
      ? assertFinalHealingDescriptionAfterImage(transaction, candidateZ64, session)
      : null;
    var proof = Object.freeze({
      baseGeneration: transaction.baseGeneration,
      candidateZ64: candidateZ64,
      words: finalWords,
      description: finalDescription
    });
    FINAL_AFTER_IMAGE_PROOFS.set(transaction, proof);
    return {
      words: Object.assign({}, finalWords),
      description: finalDescription
        ? {
            slotSha256: finalDescription.slotSha256,
            values: finalDescription.values.slice(),
            bytesConsumed: finalDescription.bytesConsumed,
            planSha256: finalDescription.planSha256
          }
        : null
    };
  }

  function buildCommittedSessionTransition(session, transaction, liveWords,
      liveDescription, provenance) {
    var nextLedger = Object.assign({}, session.ledger);
    nextLedger.currentWords = Object.assign({}, liveWords);
    nextLedger.effectOwnedWrites = transaction.writes.map(function(write) {
      return {
        offset: write.offset,
        beforeWord: write.beforeWord,
        afterWord: write.afterWord,
        modelKey: write.modelKey,
        role: write.role
      };
    });
    if (transaction.descriptionWrite) {
      nextLedger.currentDescriptionSlot = liveDescription.slot.slice();
      nextLedger.currentDescriptionValues = liveDescription.values.slice();
      nextLedger.currentDescriptionDecoded = liveDescription.decoded.slice();
      nextLedger.descriptionOwnedWrite = {
        offset: transaction.descriptionWrite.offset,
        beforeSha256: transaction.descriptionWrite.beforeSha256,
        afterSha256: liveDescription.slotSha256,
        length: liveDescription.slot.length
      };
    } else {
      nextLedger.descriptionOwnedWrite = null;
    }

    // Project data remains relative to the ROM that the user loaded. Export
    // adoption advances the live word ledger, but it must not redefine that
    // immutable source baseline or a later Save Project will omit the edits.
    var nextImportedModels = cloneModels(session.importedModels);
    var nextAvailability = Object.assign({}, session.availability);
    nextAvailability.magnitudes = Object.assign(
      {},
      session.availability.magnitudes
    );
    MAGNITUDE_MODEL_ORDER.forEach(function(modelKey) {
      var facet = session.availability.magnitudes[modelKey];
      if (facet && facet.ok) {
        nextAvailability.magnitudes[modelKey] = Object.assign({}, facet, {
          magnitude: transaction.models[modelKey].magnitude,
          words: MODEL_DEFS[modelKey].magnitudeOffsets.map(function(offset) {
            return nextLedger.currentWords[wordKey(offset)] >>> 0;
          })
        });
      }
    });
    if (session.availability.range && session.availability.range.ok) {
      var committedRanges = {};
      RANGE_MODEL_ORDER.forEach(function(modelKey) {
        committedRanges[modelKey] = {
          minimum: transaction.models[modelKey].minimum,
          maximum: transaction.models[modelKey].maximum
        };
      });
      nextAvailability.range = Object.assign({}, session.availability.range, {
        models: committedRanges
      });
    }
    if (transaction.descriptionWrite && session.availability.descriptions.ok) {
      nextAvailability.descriptions = Object.assign(
        {},
        session.availability.descriptions,
        {
          slot: liveDescription.slot.slice(),
          slotSha256: liveDescription.slotSha256,
          decoded: liveDescription.decoded.slice(),
          values: liveDescription.values.slice(),
          paddedValues: liveDescription.paddedValues.slice(),
          codeValues: liveDescription.values.slice(),
          textValues: liveDescription.values.slice()
        }
      );
    }

    if (provenance && provenance.changeRanges) {
      nextLedger.priorOwnerRegions = provenance.changeRanges.map(function(range) {
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
      nextLedger.lastChangeRanges = provenance.changeRanges.map(function(range) {
        return Object.assign({}, range);
      });
      nextLedger.headerCrcWrites =
        (provenance.headerCrcDelta || []).map(function(range) {
          return Object.assign({}, range);
        });
      nextLedger.lastCandidate = provenance.candidate
        ? Object.assign({}, provenance.candidate)
        : null;
    }
    nextLedger.exports = session.ledger.exports.slice();
    nextLedger.exports.push({
      candidateSha256:
        provenance && provenance.candidate && provenance.candidate.sha256 || '',
      candidateFilename:
        provenance && provenance.candidate && provenance.candidate.filename || '',
      effectWrites: transaction.writes.length,
      descriptionWrites: transaction.descriptionWrite ? 1 : 0,
      headerCrcWrites: provenance && provenance.headerCrcDelta
        ? provenance.headerCrcDelta.length
        : 0,
      changeRanges: provenance && provenance.changeRanges
        ? provenance.changeRanges.length
        : 0
    });

    var pendingProbe = Object.assign({}, session, {
      ledger: nextLedger,
      importedModels: nextImportedModels,
      availability: nextAvailability
    });
    var wanted = desiredWords(pendingProbe);
    var pendingWrites = Object.keys(wanted).some(function(key) {
      return (wanted[key] >>> 0) !== (nextLedger.currentWords[key] >>> 0);
    });
    if (pendingWrites) {
      throw new Error(
        'Verified candidate commit would leave consumable effects pending.'
      );
    }
    return {
      ledger: nextLedger,
      importedModels: nextImportedModels,
      availability: nextAvailability,
      pendingWrites: false
    };
  }

  // Validate and precompute the complete semantic transition, but do not mutate
  // either the active ROM or its session. The opaque adoption is the only
  // operation that may install this prepared state.
  function commitTransaction(session, transaction, packageToken,
      provenanceToken, receiptToken) {
    if (!transaction) return null;
    var state = candidatePackageState(packageToken);
    var provenance;
    var liveWords;
    var liveDescription;
    var transition;
    var normalizedBytes;
    try {
      if (state.status !== 'downloaded') {
        throw new Error('Verified candidate package lifecycle is ' +
          state.status + ', not downloaded.');
      }
      if (state.session !== session || state.transaction !== transaction ||
          session.generation !== transaction.baseGeneration ||
          state.baseGeneration !== transaction.baseGeneration) {
        throw new Error('Cannot commit a stale or mismatched consumable effect package.');
      }
      provenanceStateForPackage(provenanceToken, packageToken, state);
      var receiptState = receiptToken &&
        VERIFIED_DOWNLOAD_RECEIPTS.get(receiptToken);
      if (!receiptState ||
          receiptState.packageToken !== packageToken ||
          receiptState.packageState !== state ||
          receiptState.provenanceToken !== provenanceToken ||
          receiptState.transaction !== transaction ||
          receiptState.baseGeneration !== transaction.baseGeneration ||
          state.receiptToken !== receiptToken) {
        throw new Error(
          'Cannot commit without the successful download receipt for this exact package.'
        );
      }
      if (receiptToken.candidateFilename !== state.candidateFilename ||
          receiptToken.byteOrder !== state.byteOrder ||
          receiptToken.candidateSha256 !== state.rawSha256) {
        throw new Error('Candidate download receipt identity changed before commit.');
      }
      assertPreparedTransactionIntegrity(transaction, session);
      if ((transaction.writes.length || transaction.descriptionWrite) &&
          !transaction.applied) {
        throw new Error(
          'Cannot commit consumable effects before applying the prepared transaction.'
        );
      }
      liveWords = assertCompleteWordImage(
        state.normalizedRef,
        transaction.desiredWords,
        'commit/live word'
      );
      liveDescription = transaction.descriptionWrite
        ? assertFinalHealingDescriptionAfterImage(
            transaction,
            state.normalizedRef,
            session
          )
        : null;
      var liveProof = {
        baseGeneration: transaction.baseGeneration,
        candidateZ64: state.normalizedRef,
        words: liveWords,
        description: liveDescription
      };
      assertProofIdentity(
        liveProof,
        state.finalProof,
        transaction,
        'commit/live physical proof'
      );
      assertCandidatePackageIntegrity(state, 'commit/live package binding');
      normalizedBytes = state.normalizedSnapshot.slice();
      assertNormalizedMagic(normalizedBytes, 'commit/private adoption image');
      assertIndependentCrcBound(normalizedBytes, 'commit/private adoption image');
      provenance = provenanceToken;
      transition = buildCommittedSessionTransition(
        session,
        transaction,
        liveWords,
        liveDescription,
        provenance
      );
    } catch (error) {
      invalidateCandidatePackage(packageToken, state);
      throw error;
    }
    var adoptionToken = Object.freeze({
      kind: 'ob64-verified-candidate-adoption',
      candidateFilename: state.candidateFilename,
      byteOrder: state.byteOrder,
      candidateSha256: state.rawSha256
    });
    VERIFIED_ADOPTIONS.set(adoptionToken, {
      sourceRom: state.sourceRom,
      sourceZ64Ref: state.sourceZ64Ref,
      candidateRom: state.candidateRom,
      candidateZ64Ref: state.normalizedRef,
      session: session,
      sourceLedgerRef: session.ledger,
      sourceImportedModelsRef: session.importedModels,
      sourceAvailabilityRef: session.availability,
      sourcePendingWrites: session.pendingWrites,
      transaction: transaction,
      baseGeneration: transaction.baseGeneration,
      normalizedBytes: normalizedBytes,
      transition: transition,
      candidateFilename: state.candidateFilename,
      byteOrder: state.byteOrder,
      candidateSha256: state.rawSha256
    });
    state.status = 'adoption-ready';
    FINAL_AFTER_IMAGE_PROOFS.delete(transaction);
    VERIFIED_DOWNLOAD_RECEIPTS.delete(receiptToken);
    VERIFIED_PROVENANCES.delete(provenanceToken);
    VERIFIED_CANDIDATE_PACKAGES.delete(packageToken);
    return adoptionToken;
  }

  function adoptVerifiedCandidate(adoptionToken, targetRom, candidateRom) {
    var adoption = adoptionToken && VERIFIED_ADOPTIONS.get(adoptionToken);
    if (!adoption) {
      throw new Error('A live opaque verified candidate adoption is required.');
    }
    try {
      if (targetRom !== adoption.sourceRom) {
        throw new Error(
          'Verified candidate adoption requires the exact source ROM object.'
        );
      }
      if (candidateRom !== adoption.candidateRom) {
        throw new Error(
          'Verified candidate adoption requires the exact detached candidate ROM object.'
        );
      }
      if (targetRom.consumableEffects !== adoption.session ||
          targetRom.z64 !== adoption.sourceZ64Ref) {
        throw new Error(
          'Verified candidate adoption source state changed before installation.'
        );
      }
      if (adoption.session.generation !== adoption.baseGeneration ||
          adoption.session.ledger !== adoption.sourceLedgerRef ||
          adoption.session.importedModels !== adoption.sourceImportedModelsRef ||
          adoption.session.availability !== adoption.sourceAvailabilityRef ||
          adoption.session.pendingWrites !== adoption.sourcePendingWrites) {
        throw new Error(
          'Verified candidate adoption session state changed before installation.'
        );
      }
      assertPreparedTransactionIntegrity(
        adoption.transaction,
        adoption.session
      );
      var installedBytes = adoption.normalizedBytes.slice();
      assertNormalizedMagic(installedBytes, 'adoption/private image');
      assertIndependentCrcBound(installedBytes, 'adoption/private image');
      assertCompleteWordImage(
        installedBytes,
        adoption.transition.ledger.currentWords,
        'adoption/private word'
      );
      if (adoption.transaction.descriptionWrite) {
        var description = adoption.session.revisionManifest.description;
        assertExactBytes(
          installedBytes.subarray(description.start, description.end),
          adoption.transition.ledger.currentDescriptionSlot,
          'adoption/private description binding',
          'normalized z64 description-slot byte'
        );
      }
      var adoptionResult = {
        candidateFilename: adoption.candidateFilename,
        byteOrder: adoption.byteOrder,
        candidateSha256: adoption.candidateSha256
      };
      VERIFIED_ADOPTIONS.delete(adoptionToken);

      // All fallible validation and allocation is complete. These direct
      // assignments form one synchronous, non-yielding ROM/session transition;
      // there is no ledger-new/ROM-old observation point.
      targetRom.z64 = installedBytes;
      adoption.session.ledger = adoption.transition.ledger;
      adoption.session.importedModels = adoption.transition.importedModels;
      adoption.session.availability = adoption.transition.availability;
      adoption.session.pendingWrites = adoption.transition.pendingWrites;
    } catch (error) {
      VERIFIED_ADOPTIONS.delete(adoptionToken);
      throw error;
    }
    return adoptionResult;
  }

  function commitAndAdoptTransaction(session, transaction, packageToken,
      provenanceToken, receiptToken, targetRom, candidateRom) {
    if (!transaction) return null;
    var adoptionToken = commitTransaction(
      session,
      transaction,
      packageToken,
      provenanceToken,
      receiptToken
    );
    return adoptVerifiedCandidate(adoptionToken, targetRom, candidateRom);
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

  function assertIndependentCrcBound(z64, phase) {
    var crc = verifyIndependentCrc(z64);
    if (!crc.ok) {
      throw new Error('Independent CIC-6102 verification failed for ' +
        (phase || 'verified candidate package') + '.');
    }
    return Object.freeze({
      ok: true,
      computedCrc1: crc.computed.crc1 >>> 0,
      computedCrc2: crc.computed.crc2 >>> 0,
      headerCrc1: crc.header.crc1 >>> 0,
      headerCrc2: crc.header.crc2 >>> 0
    });
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
      var exact = currentOwner.ownerId === 'consumable-effects' ||
        currentOwner.ownerId === 'consumable-healing-descriptions' ||
        currentOwner.ownerId === 'header-crc';
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
      if (change.kind === 'magnitude') {
        return {
          key: change.projectKey,
          model: change.modelKey,
          affectedItems: change.itemIds.map(function(id) {
            return {
              id: id,
              name: itemName(rom, id),
              target: change.target
            };
          }),
          loadedBaseline: change.baselineMagnitude,
          requestedMagnitude: change.magnitude,
          retailMagnitude: change.retailMagnitude,
          warningTier: change.status.tier,
          warningLabel: change.status.label,
          paddedHealingDescription: change.paddedDescription,
          pairAtomic: change.pairAtomic,
          previousWords: change.previousWords,
          candidateWords: change.candidateWords
        };
      }
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

  function cloneAuditValue(value) {
    if (Array.isArray(value)) return value.map(cloneAuditValue);
    if (isPlainObject(value)) {
      var out = {};
      Object.keys(value).forEach(function(key) {
        out[key] = cloneAuditValue(value[key]);
      });
      return out;
    }
    return value;
  }

  function deepFreezeAudit(value) {
    if (!value || typeof value !== 'object' || ArrayBuffer.isView(value) ||
        value instanceof ArrayBuffer || Object.isFrozen(value)) {
      return value;
    }
    Object.keys(value).forEach(function(key) {
      deepFreezeAudit(value[key]);
    });
    return Object.freeze(value);
  }

  function outputOrderForRom(rom) {
    var order = rom && (rom.exportByteOrder || rom.byteOrder) || 'v64';
    if (SUPPORTED_BYTE_ORDERS.indexOf(order) === -1) {
      throw new Error('Verified candidate package has unsupported output byte order "' +
        order + '".');
    }
    return order;
  }

  function outputExtension(order) {
    if (order === 'v64') return 'v64';
    if (order === 'n64') return 'n64';
    return 'z64';
  }

  function assertCandidateFilename(candidateFilename, order) {
    if (typeof candidateFilename !== 'string' || !candidateFilename ||
        !candidateFilename.toLowerCase().endsWith('.' + outputExtension(order))) {
      throw new Error('Verified candidate filename extension does not match ' +
        order + ' output byte order.');
    }
  }

  function serializeNormalizedBytes(z64, order) {
    if (!(z64 instanceof Uint8Array)) {
      throw new Error('Verified candidate normalized image must be a Uint8Array.');
    }
    var out = new Uint8Array(z64.length);
    var i;
    if (order === 'z64') {
      out.set(z64);
      return out;
    }
    if (order === 'v64') {
      if (z64.length % 2) {
        throw new Error('v64 candidate length must be divisible by two.');
      }
      for (i = 0; i < z64.length; i += 2) {
        out[i] = z64[i + 1];
        out[i + 1] = z64[i];
      }
      return out;
    }
    if (order === 'n64') {
      if (z64.length % 4) {
        throw new Error('n64 candidate length must be divisible by four.');
      }
      for (i = 0; i < z64.length; i += 4) {
        out[i] = z64[i + 3];
        out[i + 1] = z64[i + 2];
        out[i + 2] = z64[i + 1];
        out[i + 3] = z64[i];
      }
      return out;
    }
    throw new Error('Cannot serialize unsupported candidate byte order "' + order + '".');
  }

  function normalizeSerializedBytes(raw, order) {
    if (!(raw instanceof Uint8Array)) {
      throw new Error('Verified candidate raw image must be a Uint8Array.');
    }
    return serializeNormalizedBytes(raw, order);
  }

  function assertExactBytes(actual, expected, phase, coordinate) {
    var label = phase || 'candidate identity';
    var space = coordinate || 'bytes';
    if (!(actual instanceof Uint8Array) || !(expected instanceof Uint8Array)) {
      throw new Error('Verified candidate ' + label + ' lacks comparable ' + space + '.');
    }
    if (actual.length !== expected.length) {
      throw new Error('Verified candidate ' + label + ' ' + space +
        ' length mismatch: expected ' + expected.length + ' but found ' +
        actual.length + '.');
    }
    for (var i = 0; i < expected.length; i++) {
      if (actual[i] !== expected[i]) {
        throw new Error('Verified candidate ' + label + ' mismatch at ' + space +
          ' ' + hex(i, 8) + ': expected ' + hex(expected[i], 2) +
          ' but found ' + hex(actual[i], 2) + '.');
      }
    }
  }

  function assertNormalizedMagic(z64, phase) {
    if (!z64 || z64.length < 4 ||
        z64[0] !== 0x80 || z64[1] !== 0x37 ||
        z64[2] !== 0x12 || z64[3] !== 0x40) {
      throw new Error('Verified candidate ' + (phase || 'package') +
        ' has invalid normalized z64 magic.');
    }
  }

  function assertProofIdentity(actual, expected, transaction, phase) {
    var label = phase || 'candidate proof';
    if (!actual || !expected ||
        actual.baseGeneration !== expected.baseGeneration ||
        actual.baseGeneration !== transaction.baseGeneration ||
        actual.candidateZ64 !== expected.candidateZ64) {
      throw new Error('Verified candidate ' + label +
        ' does not identify one physical transaction image.');
    }
    for (var i = 0; i < EDITABLE_WORD_GUARDS.length; i++) {
      var key = wordKey(EDITABLE_WORD_GUARDS[i].offset);
      if ((actual.words[key] >>> 0) !== (expected.words[key] >>> 0)) {
        throw new Error('Verified candidate ' + label +
          ' complete-word snapshot changed at normalized z64 ' +
          hex(EDITABLE_WORD_GUARDS[i].offset, 8) + '.');
      }
    }
    if (!!actual.description !== !!expected.description) {
      throw new Error('Verified candidate ' + label +
        ' conditional complete-slot snapshot changed.');
    }
    if (actual.description) {
      assertExactBytes(
        actual.description.slot,
        expected.description.slot,
        label,
        'normalized z64 description-slot byte'
      );
      if (actual.description.slotSha256 !== expected.description.slotSha256 ||
          actual.description.planSha256 !== expected.description.planSha256 ||
          actual.description.bytesConsumed !== expected.description.bytesConsumed ||
          JSON.stringify(actual.description.values) !==
            JSON.stringify(expected.description.values) ||
          JSON.stringify(actual.description.decoded) !==
            JSON.stringify(expected.description.decoded) ||
          JSON.stringify(actual.description.paddedValues) !==
            JSON.stringify(expected.description.paddedValues)) {
        throw new Error('Verified candidate ' + label +
          ' conditional complete-slot semantic proof changed.');
      }
    }
  }

  function invalidateCandidatePackage(packageToken, state) {
    if (!state) return;
    state.status = 'invalid';
    FINAL_AFTER_IMAGE_PROOFS.delete(state.transaction);
    if (state.provenanceToken) VERIFIED_PROVENANCES.delete(state.provenanceToken);
    if (state.receiptToken) VERIFIED_DOWNLOAD_RECEIPTS.delete(state.receiptToken);
    VERIFIED_CANDIDATE_PACKAGES.delete(packageToken);
  }

  function candidatePackageState(packageToken, allowedStatuses) {
    var state = packageToken && VERIFIED_CANDIDATE_PACKAGES.get(packageToken);
    if (!state) {
      throw new Error('A live opaque verified candidate package is required.');
    }
    if (allowedStatuses && allowedStatuses.indexOf(state.status) === -1) {
      throw new Error('Verified candidate package lifecycle is ' + state.status +
        ', not ' + allowedStatuses.join(' or ') + '.');
    }
    return state;
  }

  function provenanceStateForPackage(provenanceToken, packageToken, state) {
    var provenanceState = provenanceToken && VERIFIED_PROVENANCES.get(provenanceToken);
    if (!provenanceState ||
        provenanceState.packageToken !== packageToken ||
        provenanceState.packageState !== state ||
        state.provenanceToken !== provenanceToken) {
      throw new Error('Verified provenance does not belong to this candidate package.');
    }
    if (!provenanceToken.candidate ||
        provenanceToken.candidate.sha256 !== state.rawSha256 ||
        provenanceToken.candidate.filename !== state.candidateFilename ||
        provenanceToken.candidate.byteOrder !== state.byteOrder ||
        provenanceToken.candidate.size !== state.rawBytes.length) {
      throw new Error('Verified provenance candidate identity changed.');
    }
    return provenanceState;
  }

  function assertCandidatePackageIntegrity(state, phase) {
    var label = phase || 'package integrity';
    if (!state.session ||
        state.session.generation !== state.baseGeneration ||
        state.transaction.baseGeneration !== state.baseGeneration) {
      throw new Error('Verified candidate ' + label +
        ' belongs to a stale session generation.');
    }
    if (!state.sourceRom || state.sourceRom.consumableEffects !== state.session ||
        !state.candidateRom || state.candidateRom === state.sourceRom ||
        state.candidateRom.z64 !== state.normalizedRef ||
        state.normalizedRef === state.sourceZ64Ref) {
      throw new Error('Verified candidate ' + label +
        ' no longer identifies its source and detached candidate objects.');
    }
    assertPreparedTransactionIntegrity(state.transaction, state.session);
    var liveOrder = outputOrderForRom(state.candidateRom);
    if (liveOrder !== state.byteOrder) {
      throw new Error('Verified candidate ' + label + ' output byte order changed.');
    }
    assertCandidateFilename(state.candidateFilename, state.byteOrder);
    if (state.normalizedRef.length !== state.session.baselineZ64.length ||
        state.rawBytes.length !== state.normalizedSnapshot.length) {
      throw new Error('Verified candidate ' + label + ' image length changed.');
    }
    assertNormalizedMagic(state.normalizedRef, label);
    assertExactBytes(
      state.normalizedRef,
      state.normalizedSnapshot,
      label,
      'normalized z64 byte'
    );
    var deterministicRaw = serializeNormalizedBytes(
      state.normalizedSnapshot,
      state.byteOrder
    );
    assertExactBytes(
      state.rawBytes,
      deterministicRaw,
      label,
      state.byteOrder + ' raw byte'
    );
    if (detectByteOrder(state.rawBytes) !== state.byteOrder) {
      throw new Error('Verified candidate ' + label +
        ' serialized magic does not match its output byte order.');
    }
    var normalizedRaw = normalizeSerializedBytes(state.rawBytes, state.byteOrder);
    assertExactBytes(
      normalizedRaw,
      state.normalizedSnapshot,
      label,
      'normalized raw byte'
    );
    var crc = assertIndependentCrcBound(state.normalizedRef, label);
    if (state.crc &&
        (crc.computedCrc1 !== state.crc.computedCrc1 ||
          crc.computedCrc2 !== state.crc.computedCrc2 ||
          crc.headerCrc1 !== state.crc.headerCrc1 ||
          crc.headerCrc2 !== state.crc.headerCrc2)) {
      throw new Error('Verified candidate ' + label +
        ' independent CRC identity changed.');
    }
    return crc;
  }

  function refreshCandidatePackageProof(state, phase) {
    var previous = state.finalProof;
    validateFinalAfterImage(
      state.transaction,
      state.normalizedRef,
      state.session
    );
    var current = FINAL_AFTER_IMAGE_PROOFS.get(state.transaction);
    if (!current) {
      throw new Error('Verified candidate ' + (phase || 'package') +
        ' did not retain its final physical proof.');
    }
    if (previous) {
      assertProofIdentity(current, previous, state.transaction, phase);
    }
    state.finalProof = current;
    return current;
  }

  function createVerifiedCandidatePackage(sourceRom, candidateRom, session,
      transaction, candidateFilename) {
    if (!sourceRom || !candidateRom || !session || !transaction) {
      throw new Error('Source, detached candidate, session, and transaction are required.');
    }
    if (sourceRom === candidateRom || sourceRom.z64 === candidateRom.z64) {
      throw new Error('Verified candidate package requires a detached candidate object and image.');
    }
    if (sourceRom.consumableEffects !== session) {
      throw new Error('Verified candidate package source does not own the supplied session.');
    }
    var byteOrder = outputOrderForRom(candidateRom);
    assertCandidateFilename(candidateFilename, byteOrder);
    validateFinalAfterImage(transaction, candidateRom.z64, session);
    var proof = FINAL_AFTER_IMAGE_PROOFS.get(transaction);
    var normalizedSnapshot = candidateRom.z64.slice();
    var rawBytes = serializeNormalizedBytes(normalizedSnapshot, byteOrder);
    if (detectByteOrder(rawBytes) !== byteOrder) {
      throw new Error('Verified candidate serialized magic does not match ' +
        byteOrder + ' output byte order.');
    }
    var state = {
      status: 'created',
      sourceRom: sourceRom,
      sourceZ64Ref: sourceRom.z64,
      candidateRom: candidateRom,
      normalizedRef: candidateRom.z64,
      normalizedSnapshot: normalizedSnapshot,
      rawBytes: rawBytes,
      rawSha256: '',
      byteOrder: byteOrder,
      candidateFilename: candidateFilename,
      session: session,
      transaction: transaction,
      baseGeneration: transaction.baseGeneration,
      finalProof: proof,
      crc: null,
      provenanceToken: null,
      receiptToken: null
    };
    var packageToken = Object.freeze({
      kind: 'ob64-verified-candidate-package',
      byteOrder: byteOrder,
      candidateFilename: candidateFilename
    });
    VERIFIED_CANDIDATE_PACKAGES.set(packageToken, state);
    try {
      state.crc = assertCandidatePackageIntegrity(state, 'package creation');
      refreshCandidatePackageProof(state, 'package creation/final proof');
      return packageToken;
    } catch (error) {
      invalidateCandidatePackage(packageToken, state);
      throw error;
    }
  }

  function buildProvenance(packageToken, owners, dirtySnapshot) {
    var state = candidatePackageState(packageToken, ['created']);
    var ownerSnapshot = cloneAuditValue(owners || []);
    var dirtyCopy = cloneAuditValue(dirtySnapshot || {});
    return sha256Hex(state.rawBytes).then(function(candidateHash) {
      try {
        candidatePackageState(packageToken, ['created']);
        refreshCandidatePackageProof(state, 'post-provenance final proof');
        assertCandidatePackageIntegrity(state, 'post-provenance boundary');
        state.rawSha256 = candidateHash;
        var allOwners = ownerSnapshot.slice();
        allOwners.push(assertEffectDeltaOwner(state.transaction.deltaOwner));
        if (state.transaction.textOwner) {
          allOwners.push(assertHealingDescriptionOwner(
            state.transaction.textOwner,
            state.session.revisionManifest
          ));
        }
        var ranges = buildChangeRanges(
          state.session.baselineZ64,
          state.normalizedSnapshot,
          allOwners
        );
        var effectDelta = ranges.filter(function(range) {
          return range.ownerId === 'consumable-effects';
        });
        var descriptionDelta = ranges.filter(function(range) {
          return range.ownerId === 'consumable-healing-descriptions';
        });
        var headerDelta = ranges.filter(function(range) {
          return range.ownerId === 'header-crc';
        });
        var record = {
          schema: 'ob64-consumable-effects-provenance',
          version: 1,
          generatedAt: new Date().toISOString(),
          source: {
            filename: state.session.source.filename,
            size: state.session.source.size,
            sha256: state.session.source.sha256,
            byteOrder: state.session.source.byteOrder,
            normalizedHeader: cloneAuditValue(state.session.source.header)
          },
          candidate: {
            filename: state.candidateFilename,
            size: state.rawBytes.length,
            sha256: state.rawSha256,
            byteOrder: state.byteOrder,
            normalizedHeader: normalizedHeaderIdentity(state.normalizedSnapshot)
          },
          profile: profileForModels(state.transaction.models),
          models: modelProvenance(state.candidateRom, state.transaction),
          effectOwnedDelta: effectDelta,
          healingDescriptionOwnedDelta: descriptionDelta,
          headerCrcDelta: headerDelta,
          changeRanges: ranges,
          dirtyCategories: dirtyCategoryList(dirtyCopy),
          independentCrc: {
            ok: true,
            computedCrc1: hex(state.crc.computedCrc1),
            computedCrc2: hex(state.crc.computedCrc2),
            headerCrc1: hex(state.crc.headerCrc1),
            headerCrc2: hex(state.crc.headerCrc2)
          },
          expectedLoadedWords: expectedLoadedWordsForCandidate(
            state.normalizedSnapshot
          ),
          dispatchTable: {
            revision: state.session.revisionManifest.id,
            normalizedSource: EXPECTED_LOADED_WORDS.dispatchTableByRevision[
              state.session.revisionManifest.id
            ].source,
            requiredLiveInterval: EXPECTED_LOADED_WORDS.dispatchTableByRevision[
              state.session.revisionManifest.id
            ].live,
            lastLiveWord: EXPECTED_LOADED_WORDS.dispatchTableByRevision[
              state.session.revisionManifest.id
            ].lastWord,
            sha256: state.session.revisionManifest.rangeProfile.dispatchTable.sha256
          },
          editorVersion: EDITOR_VERSION,
          projectFormatVersion: PROJECT_VERSION,
          lockedItems: {
            count: lockedIds().length,
            ids: lockedIds(),
            contributedEffectWrites: false,
            statement: 'Disabled IDs contributed no consumable-effect write.'
          }
        };
        var provenanceToken = deepFreezeAudit(cloneAuditValue(record));
        VERIFIED_PROVENANCES.set(provenanceToken, {
          packageToken: packageToken,
          packageState: state
        });
        state.provenanceToken = provenanceToken;
        state.status = 'provenance';
        return provenanceToken;
      } catch (error) {
        invalidateCandidatePackage(packageToken, state);
        throw error;
      }
    }, function(error) {
      invalidateCandidatePackage(packageToken, state);
      throw error;
    });
  }

  function finalizeVerifiedCandidatePackage(packageToken, provenanceToken) {
    var state = candidatePackageState(packageToken, ['provenance']);
    try {
      provenanceStateForPackage(provenanceToken, packageToken, state);
      refreshCandidatePackageProof(state, 'final post-await physical proof');
      assertCandidatePackageIntegrity(state, 'final post-await package gate');
      state.status = 'ready';
      return packageToken;
    } catch (error) {
      invalidateCandidatePackage(packageToken, state);
      throw error;
    }
  }

  function assertCandidateDownloadBytes(state, sinkBytes) {
    assertExactBytes(
      sinkBytes,
      state.rawBytes,
      'download package identity',
      state.byteOrder + ' raw byte'
    );
    if (sinkBytes.length !== state.normalizedSnapshot.length ||
        detectByteOrder(sinkBytes) !== state.byteOrder) {
      throw new Error('Verified candidate download byte order, magic, or length changed.');
    }
    var normalizedSink = normalizeSerializedBytes(sinkBytes, state.byteOrder);
    assertExactBytes(
      normalizedSink,
      state.normalizedSnapshot,
      'download normalization identity',
      'normalized z64 byte'
    );
    assertIndependentCrcBound(normalizedSink, 'exact Blob body');
    if (!state.rawSha256 || !state.provenanceToken ||
        state.provenanceToken.candidate.sha256 !== state.rawSha256) {
      throw new Error('Verified candidate download digest is not bound to provenance.');
    }
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

  function commitOrdinaryExport(session, prepared, adoptedZ64) {
    if (!prepared || session.generation !== prepared.baseGeneration) {
      throw new Error('Cannot commit a stale ordinary export ledger.');
    }
    var ranges = prepared.ranges || [];
    var candidate = prepared.candidate || {};
    session.ledger.priorOwnerRegions = ranges.map(function(range) {
      var adoptedConsumableDescriptions =
        range.ownerId === 'consumable-descriptions-pending';
      return {
        id: adoptedConsumableDescriptions
          ? 'consumable-descriptions-adopted' : range.ownerId,
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
    if (adoptedZ64 && session.revisionManifest &&
        session.revisionManifest.description) {
      var description = session.revisionManifest.description;
      session.ledger.currentDescriptionSlot = adoptedZ64.slice(
        description.start,
        description.end
      );
      var descriptionFacet = inspectHealingDescription(
        adoptedZ64,
        session.revisionManifest
      );
      session.availability.descriptions = descriptionFacet;
      if (descriptionFacet.ok) {
        session.ledger.currentDescriptionDecoded = descriptionFacet.decoded.slice();
        session.ledger.currentDescriptionValues = descriptionFacet.values.slice();
      } else {
        session.ledger.currentDescriptionDecoded = null;
        session.ledger.currentDescriptionValues = null;
      }
    }
  }

  function expectedLoadedWordsForCandidate(z64) {
    var out = {};
    for (var i = 0; i < MODEL_ORDER.length; i++) {
      var key = MODEL_ORDER[i];
      var def = MODEL_DEFS[key];
      if (def.kind === 'magnitude') {
        out[key] = {};
        var loadedKeys = Object.keys(EXPECTED_LOADED_WORDS[key]);
        for (var magnitudeIndex = 0;
            magnitudeIndex < def.magnitudeOffsets.length;
            magnitudeIndex++) {
          var addressKey = loadedKeys[magnitudeIndex];
          out[key][addressKey + 'Address'] =
            EXPECTED_LOADED_WORDS[key][addressKey];
          out[key][addressKey + 'Word'] =
            hex(readU32(z64, def.magnitudeOffsets[magnitudeIndex]));
        }
      } else {
        out[key] = {
          widthAddress: EXPECTED_LOADED_WORDS[key].width,
          widthWord: hex(readU32(z64, def.widthOffset)),
          minimumAddress: EXPECTED_LOADED_WORDS[key].minimum,
          minimumWord: hex(readU32(z64, def.minimumOffset))
        };
      }
    }
    out.finalStore = { address: EXPECTED_LOADED_WORDS.finalStore, word: hex(readU32(z64, 0x415E4)) };
    return out;
  }

  function serializeCandidate(rom) {
    return serializeNormalizedBytes(rom.z64, outputOrderForRom(rom));
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

  function downloadRomCandidate(packageToken, provenanceToken) {
    var state = candidatePackageState(packageToken, ['ready']);
    try {
      provenanceStateForPackage(provenanceToken, packageToken, state);
      refreshCandidatePackageProof(state, 'pre-download physical proof');
      assertCandidatePackageIntegrity(state, 'pre-download package gate');
      var sinkBytes = state.rawBytes.slice();
      assertCandidateDownloadBytes(state, sinkBytes);
      downloadBytes(
        sinkBytes,
        state.candidateFilename,
        'application/octet-stream'
      );
      assertCandidateDownloadBytes(state, sinkBytes);
      refreshCandidatePackageProof(state, 'post-download physical proof');
      assertCandidatePackageIntegrity(state, 'post-download receipt gate');
      var receiptToken = Object.freeze({
        kind: 'ob64-verified-download-receipt',
        candidateFilename: state.candidateFilename,
        byteOrder: state.byteOrder,
        candidateSha256: state.rawSha256
      });
      VERIFIED_DOWNLOAD_RECEIPTS.set(receiptToken, {
        packageToken: packageToken,
        packageState: state,
        provenanceToken: provenanceToken,
        transaction: state.transaction,
        baseGeneration: state.baseGeneration
      });
      state.receiptToken = receiptToken;
      state.status = 'downloaded';
      return receiptToken;
    } catch (error) {
      invalidateCandidatePackage(packageToken, state);
      throw error;
    }
  }

  function lockedIds() {
    // Preserve the complete effect-domain nonparticipation ledger even though
    // Joe intentionally removed quest/story IDs 32-44 from the visible tab.
    var out = [];
    for (var id = 1; id <= 44; id++) if (!ITEM_TO_MODEL[id]) out.push(id);
    return out;
  }

  function catalogDisposition(id) {
    if (id === 1) return { effect: 'Restores current HP to one target; the bounded special-pool path uses the same amount.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 2) return { effect: 'Restores current HP to one target; the bounded special-pool path uses the same amount.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 3) return { effect: 'Restores current HP across the group; one amount is stored in two required equal code words.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 4) return { effect: 'Reduces ordinary-target Fatigue (u8) and floors at zero; the accepted special-target path is a no-op.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
    if (id === 5) return { effect: 'Reduces ordinary-target Fatigue (u8) and floors at zero; the accepted special-target path is a no-op.', modelKey: ITEM_TO_MODEL[id], category: 'Consumable' };
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
      var facet = disposition.modelKey
        ? modelAvailability(session, disposition.modelKey)
        : null;
      var editable = eligible && !!disposition.modelKey && facet.ok;
      var reason = editable ? '' : (disposition.modelKey
        ? facet && facet.reason ||
          'Load a compatible US ROM whose local consumable-effect facet matches the selected revision.'
        : disposition.reason);
      if (!eligible && !disposition.modelKey) {
        reason += ' Effect editing is also unavailable for this source: ' +
          (session && session.identity && session.identity.reason ||
            'load a compatible guarded US ROM.');
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
    var def = MODEL_DEFS[modelKey];
    var inputs = queryAll(panel, '[data-effect-model="' + modelKey + '"]');
    for (var i = 0; i < inputs.length; i++) {
      var role = inputs[i].getAttribute('data-effect-role');
      inputs[i].value = def.kind === 'magnitude'
        ? String(model.magnitude)
        : (role === 'minimum' ? String(model.minimum) : String(model.maximum));
      inputs[i].setAttribute('aria-invalid', 'false');
    }
    if (def.kind === 'magnitude') {
      var status = magnitudeStatus(modelKey, model.magnitude);
      var warnings = queryAll(panel, '[data-effect-warning="' + modelKey + '"]');
      for (var warningIndex = 0; warningIndex < warnings.length; warningIndex++) {
        warnings[warningIndex].className =
          'consumable-effect-warning is-' + status.tier;
        warnings[warningIndex].setAttribute('data-warning-tier', status.tier);
        warnings[warningIndex].textContent = status.label + ': ' + status.message;
      }
    } else {
      var derived = queryAll(panel, '[data-effect-derived="' + modelKey + '"]');
      var width = model.maximum - model.minimum + 1;
      for (var d = 0; d < derived.length; d++) {
        derived[d].textContent = 'Width ' + width + ' \u00b7 reachable ' +
          formatSigned(model.minimum) + '..' + formatSigned(model.maximum) + ' inclusive';
      }
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
    if (def.kind === 'magnitude') {
      return renderEditableMagnitude(doc, panel, row, session, options);
    }
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

  function renderEditableMagnitude(doc, panel, row, session, options) {
    var def = MODEL_DEFS[row.modelKey];
    var valueCell = createElement(doc, 'div',
      'consumable-effect-value consumable-effect-value-magnitude');
    var label = createElement(doc, 'label', 'consumable-effect-number');
    var inputId = 'consumable-effect-' + row.id + '-magnitude';
    var helpId = 'consumable-effect-help-' + row.id;
    var warningId = 'consumable-effect-warning-' + row.id;
    var errorId = 'consumable-effect-error-' + row.id;
    label.setAttribute('for', inputId);
    label.appendChild(createElement(doc, 'span', '', 'Magnitude'));
    var input = createElement(doc, 'input');
    input.type = 'number';
    input.step = '1';
    input.id = inputId;
    input.min = String(def.domainMin);
    input.max = String(def.domainMax);
    input.value = String(session.models[row.modelKey].magnitude);
    input.setAttribute('inputmode', 'numeric');
    input.setAttribute('data-effect-model', row.modelKey);
    input.setAttribute('data-effect-role', 'magnitude');
    input.setAttribute('aria-describedby', helpId + ' ' + warningId + ' ' + errorId);
    label.appendChild(input);
    valueCell.appendChild(label);

    var helpText = def.healingDescription
      ? 'The three-digit numeric in-game description is synchronized on export.'
      : 'The configured u16 amount applies to ordinary-target Fatigue; the accepted special-target path remains a no-op.';
    if (def.pairAtomic) {
      helpText += ' This one configured amount is stored in two required equal code words.';
    }
    var help = createElement(doc, 'div', 'consumable-effect-derived', helpText);
    help.id = helpId;
    valueCell.appendChild(help);

    var retail = createElement(
      doc,
      'div',
      'consumable-effect-retail',
      'Retail: ' + def.vanillaMagnitude
    );
    valueCell.appendChild(retail);

    var warning = createElement(doc, 'div', 'consumable-effect-warning');
    warning.id = warningId;
    warning.setAttribute('data-effect-warning', row.modelKey);
    warning.setAttribute('role', 'status');
    warning.setAttribute('aria-live', 'polite');
    valueCell.appendChild(warning);

    var error = createElement(doc, 'div', 'consumable-effect-error');
    error.id = errorId;
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
      if (options && options.onChange) options.onChange(session.pendingWrites, session);
    });
    valueCell.appendChild(reset);

    function applyInput() {
      try {
        if (String(input.value).trim() === '') {
          throw new Error('Magnitude is required.');
        }
        var magnitude = Number(input.value);
        setItemMagnitude(session, row.id, magnitude);
        syncModelViews(panel, session, row.modelKey);
        if (options && options.onChange) options.onChange(session.pendingWrites, session);
      } catch (err) {
        input.setAttribute('aria-invalid', 'true');
        error.textContent = err.message;
        error.hidden = false;
      }
    }
    input.addEventListener('input', applyInput);
    input.addEventListener('change', applyInput);
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
      'Consumable records 1\u201331. Supported effect values and ranges are staged for the next combined ROM export.'));
    heading.appendChild(copy);
    shell.appendChild(heading);

    var catalog = buildCatalog(rom, session);
    var editableCount = catalog.filter(function(row) { return row.editable; }).length;
    var layoutEligible = !!(session && session.identity && session.identity.eligible);
    var compatibilityClass = !layoutEligible
      ? 'is-unavailable'
      : (editableCount === 15 ? 'is-supported' : 'is-partial');
    var compatibility = createElement(doc, 'div',
      'consumable-compatibility ' + compatibilityClass);
    compatibility.setAttribute('role', 'status');
    compatibility.setAttribute('aria-live', 'polite');
    compatibility.textContent = !layoutEligible
      ? 'Effect editing unavailable: ' + (session && session.identity && session.identity.reason ||
        'load a compatible guarded US ROM.')
      : (editableCount === 15
        ? 'Effect editing available for all 15 supported rows (' +
          session.revisionManifest.label + '); 16 rows remain intentionally unavailable.'
        : 'Partial effect editing (' + session.revisionManifest.label + '): ' +
          editableCount + ' of 15 supported rows are available. Each locked row shows its exact local facet mismatch.');
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
    for (var i = 0; i < catalog.length; i++) {
      (function(row) {
        var card = createElement(doc, 'article', 'consumable-card consumable-row' +
          (row.editable ? ' is-editable' : ' is-locked'));
        card.setAttribute('role', 'listitem');
        card.setAttribute('data-consumable-row', '');
        card.setAttribute('data-item-id', String(row.id));
        card.setAttribute('data-availability', row.editable ? 'editable' : 'unavailable');
        card.setAttribute('data-search', row.id + ' #' + row.id + ' ' + row.name + ' ' + row.category);

        var identity = createElement(doc, 'div', 'consumable-identity');
        var iconColumn = createElement(doc, 'div', 'consumable-icon-column');
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
        iconColumn.appendChild(iconWrap);
        var editDescription = createElement(
          doc,
          'button',
          'consumable-description-button',
          'Edit description'
        );
        editDescription.type = 'button';
        editDescription.addEventListener('click', function() {
          if (options && options.onEditDescription) {
            options.onEditDescription(row.id);
          }
        });
        iconColumn.appendChild(editDescription);
        identity.appendChild(iconColumn);
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
          var def = MODEL_DEFS[row.modelKey];
          var readyReason = def.kind === 'magnitude'
            ? (def.healingDescription
              ? 'Verified code magnitude and synchronized description model.'
              : 'Verified local magnitude model and bounded target behavior.')
            : (row.shared
              ? 'One canonical shared model and byte pair.'
              : 'Verified local range model.');
          availability.appendChild(createElement(
            doc,
            'p',
            'consumable-ready-reason',
            readyReason
          ));
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
    if (dirty && dirty.items) add('items', 'Items', 'items', [{
      // The first logical item name pointer is stored four bytes before the
      // stat-framed table. The serializer can write that pointer, so the audit
      // owner must cover it as well as every 32-byte stat frame.
      kind: 'rom', start: (OB64.ITEM_STAT_OFFSET || 0x62310) - 4,
      size: (OB64.ITEM_STAT_COUNT || 278) * (OB64.ITEM_STAT_SIZE || 32) + 4,
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
    if (dirty && dirty.itemDescriptions && rom.itemDescriptions && OB64.descriptionCodec) {
      add('item-descriptions', 'Item Descriptions', 'itemDescriptions', [
        OB64.descriptionCodec.ownerRegion(rom.itemDescriptions)
      ]);
    }
    if (dirty && dirty.consumableDescriptions && rom.consumableDescriptions &&
        OB64.descriptionCodec) {
      add('consumable-descriptions-pending', 'Consumable Descriptions',
        'consumableDescriptions', [
          OB64.descriptionCodec.ownerRegion(rom.consumableDescriptions)
        ]);
    }
    if (dirty && dirty.classDescriptions && rom.classDescriptions && OB64.descriptionCodec) {
      add('class-descriptions', 'Class Descriptions', 'classDescriptions', [
        OB64.descriptionCodec.ownerRegion(rom.classDescriptions)
      ]);
    }
    if (dirty && dirty.actionDescriptions && rom.actionDescriptions && OB64.descriptionCodec) {
      add('action-descriptions', 'Action Descriptions', 'actionDescriptions', [
        OB64.descriptionCodec.ownerRegion(rom.actionDescriptions)
      ]);
    }
    if (dirty && dirty.statGates && rom.statGates && rom.statGates.meta) {
      if (OB64.statGateRelocation && OB64.statGateRelocation.patchRegions) {
        add('stat-gates', 'Class-change Stat Gates', 'statGates',
          OB64.statGateRelocation.patchRegions(rom.statGates));
      } else {
        var meta = rom.statGates.meta;
        add('stat-gates', 'Class-change Stat Gates', 'statGates', [{
          kind: 'rom', start: meta.compDataOff - 8,
          size: meta.compDataSize + 8, label: 'stat-gate LZSS slot'
        }]);
      }
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
      var priorOwners = session.ledger.priorOwnerRegions;
      if (dirty && dirty.consumableDescriptions) {
        priorOwners = priorOwners.filter(function(owner) {
          return owner.id !== 'consumable-descriptions-adopted';
        });
      }
      owners = owners.concat(priorOwners);
    }
    return owners;
  }

  function toolCompatibilityOwners(plannedOwners, effectTransaction) {
    var collisionOwner = effectTransaction && effectTransaction.collisionOwner;
    var owners = (plannedOwners || []).filter(function(owner) {
      if (!owner || owner.category === 'tools') return false;
      if (!collisionOwner) return true;
      // The broad guard replaces these internal effect owners when comparing
      // against selected Tools features. Keeping both makes the subsystem
      // collide with its own previously adopted byte ranges.
      return owner.id !== 'consumable-effects' &&
        owner.id !== 'consumable-healing-descriptions' &&
        owner.id !== 'consumable-descriptions-adopted' &&
        owner.id !== collisionOwner.id;
    });
    if (collisionOwner) owners.push(collisionOwner);
    return owners;
  }

  return {
    PROJECT_VERSION: PROJECT_VERSION,
    EDITOR_VERSION: EDITOR_VERSION,
    CATALOG_MAX_ID: CATALOG_MAX_ID,
    SOURCE_DESCRIPTOR: SOURCE_DESCRIPTOR,
    MODEL_ORDER: MODEL_ORDER.slice(),
    MAGNITUDE_MODEL_ORDER: MAGNITUDE_MODEL_ORDER.slice(),
    RANGE_MODEL_ORDER: RANGE_MODEL_ORDER.slice(),
    MODEL_DEFS: MODEL_DEFS,
    ITEM_TO_MODEL: Object.freeze(Object.assign({}, ITEM_TO_MODEL)),
    SHARED_TARGETS: SHARED_TARGETS,
    GUARD_MANIFEST: GUARD_MANIFEST,
    REVISION_MANIFESTS: REVISION_MANIFESTS,
    EDITABLE_WORD_GUARDS: EDITABLE_WORD_GUARDS,
    HEALING_DESCRIPTION_CONSTANTS: HEALING_DESCRIPTION_CONSTANTS,
    EXPECTED_LOADED_WORDS: EXPECTED_LOADED_WORDS,
    sha256Hex: sha256Hex,
    sha256HexSync: sha256HexSync,
    inspectSourceIdentity: inspectSourceIdentity,
    evaluateSourceIdentity: evaluateSourceIdentity,
    sourceFactsFromRaw: sourceFactsFromRaw,
    vanillaModels: vanillaModels,
    magnitudeStatus: magnitudeStatus,
    validateMagnitude: validateMagnitude,
    validateRange: validateRange,
    validateAllModels: validateAllModels,
    encodeMagnitude: encodeMagnitude,
    encodeRange: encodeRange,
    validateGuards: validateGuards,
    immutableProjection: immutableProjection,
    decodeLzssStrict: decodeLzssStrict,
    decodeLzssStrictAlternate: decodeLzssStrictAlternate,
    healingImmutableProjection: healingImmutableProjection,
    zeroHealingDigits: zeroHealingDigits,
    healingReferenceIsVariableSafe: healingReferenceIsVariableSafe,
    buildHealingPlan: buildHealingPlan,
    serializeHealingPlan: serializeHealingPlan,
    assertHealingPlanVariableIsolation: assertHealingPlanVariableIsolation,
    healingPlanStats: healingPlanStats,
    assertHealingPlanIdentity: assertHealingPlanIdentity,
    encodeHealingPlan: encodeHealingPlan,
    buildHealingDescriptionSlot: buildHealingDescriptionSlot,
    inspectHealingDescription: inspectHealingDescription,
    validateCommonProfile: validateCommonProfile,
    validateMagnitudeFacet: validateMagnitudeFacet,
    reconcileHealingDescription: reconcileHealingDescription,
    modelAvailability: modelAvailability,
    initializeSession: initializeSession,
    sessionFor: sessionFor,
    assertSessionOwnership: assertSessionOwnership,
    assertSharedBinding: assertSharedBinding,
    refreshPending: refreshPending,
    hasDesiredEffects: hasDesiredEffects,
    hasAppliedEffects: hasAppliedEffects,
    setModelMagnitude: setModelMagnitude,
    setItemMagnitude: setItemMagnitude,
    setModelRange: setModelRange,
    setItemRange: setItemRange,
    resetModel: resetModel,
    resetItem: resetItem,
    collectProjectPayload: collectProjectPayload,
    validateProjectPayload: validateProjectPayload,
    applyProjectPayload: applyProjectPayload,
    effectCollisionOwner: effectCollisionOwner,
    effectDeltaOwner: effectDeltaOwner,
    healingDescriptionOwner: healingDescriptionOwner,
    standardPatchOwners: standardPatchOwners,
    toolCompatibilityOwners: toolCompatibilityOwners,
    scenarioPatchOwners: scenarioPatchOwners,
    findRegionConflicts: findRegionConflicts,
    assertPreparedTransactionIntegrity: assertPreparedTransactionIntegrity,
    assertCompleteWordImage: assertCompleteWordImage,
    prepareTransaction: prepareTransaction,
    applyTransaction: applyTransaction,
    validateFinalAfterImage: validateFinalAfterImage,
    commitTransaction: commitTransaction,
    commitAndAdoptTransaction: commitAndAdoptTransaction,
    computeIndependentCrc: computeIndependentCrc,
    verifyIndependentCrc: verifyIndependentCrc,
    expectedLoadedWordsForCandidate: expectedLoadedWordsForCandidate,
    buildChangeRanges: buildChangeRanges,
    createVerifiedCandidatePackage: createVerifiedCandidatePackage,
    buildProvenance: buildProvenance,
    finalizeVerifiedCandidatePackage: finalizeVerifiedCandidatePackage,
    prepareOrdinaryExport: prepareOrdinaryExport,
    commitOrdinaryExport: commitOrdinaryExport,
    serializeCandidate: serializeCandidate,
    downloadRomCandidate: downloadRomCandidate,
    adoptVerifiedCandidate: adoptVerifiedCandidate,
    profileForModels: profileForModels,
    buildCatalog: buildCatalog,
    catalogDisposition: catalogDisposition,
    lockedIds: lockedIds,
    render: render
  };
});
