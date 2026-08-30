'use strict';

// Regression coverage for complete Item and Class record editing. UI checks are
// structural because the editor has no DOM test dependency. Serializer checks
// prove that every logical record byte reaches its physical ROM location.

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function equalBytes(actual, expected) {
  if (!actual || actual.length !== expected.length) return false;
  for (let i = 0; i < expected.length; i++) {
    if (actual[i] !== expected[i]) return false;
  }
  return true;
}

function mismatchDetail(actual, expected) {
  for (let i = 0; i < expected.length; i++) {
    if (actual[i] !== expected[i]) {
      return 'logical B' + i + ': expected ' + expected[i] + ', got ' + actual[i];
    }
  }
  return actual.length === expected.length ? '' :
    'expected ' + expected.length + ' bytes, got ' + actual.length;
}

function readU16(bytes, offset) {
  return (bytes[offset] << 8) | bytes[offset + 1];
}

function signedByte(value) {
  return value > 127 ? value - 256 : value;
}

const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const cssSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
const rawRecordCondition =
  'if (rawClassDataUnlocked && rawDef && (rawDef.isTerm || rawDef.isSentinel))';

check('the raw-record warning gate applies to both Class table and card views',
  appSource.split(rawRecordCondition).length - 1 === 2 &&
  appSource.includes('Table and Card views'));
check('the raw-record gate remains visible without forcing Card View',
  !appSource.includes('rawDataGate.style.display') &&
  !appSource.includes("localStorage.setItem('ob64_classes_view', 'cards')"));
check('the Class table resolves gated terminator and sentinel records',
  appSource.includes("tr.classList.add('class-raw-record')") &&
  /def = rawDef;\s+showingRawClassRecord = true;/.test(appSource));
check('locked Class placeholders are read-only instead of falsely editable',
  appSource.includes("if (!def) return addReadOnlyCell(tr, '\\u2014', 'Enable raw class records to edit this record.'") &&
  appSource.includes("if (!gate) return addReadOnlyCell(tr, '\\u2014', title, 'col-gate')"));
check('zero Item stats and resistances retain the editable affordance',
  appSource.includes("tdStat.className = 'num dim editable'") &&
  appSource.includes("tdR.className = 'num dim editable'"));
check('Item and Class click editors support keyboard activation',
  appSource.includes('function makeClickEditorsKeyboardAccessible(') &&
  appSource.includes("makeClickEditorsKeyboardAccessible(table, 'Item')") &&
  appSource.includes("makeClickEditorsKeyboardAccessible(table, 'Class')") &&
  appSource.includes("makeClickEditorsKeyboardAccessible(cardsContainer, 'Class')") &&
  appSource.includes("event.key === 'Enter' || event.key === ' '") &&
  appSource.includes("editor.setAttribute('role', 'button')") &&
  cssSource.includes('.class-card .editable:focus-visible'));
check('both Class stat-base editors refresh the record classification',
  (appSource.match(/OB64\.refreshClassDefClassification\(def\);/g) || [])
    .length === 2);

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['parsers.js', 'repack.js', 'patch.js']) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

OB64.descriptionCodec = {
  prepareProjectChanges() { return null; },
};

const giantProjectRecord = {
  isTerm: true,
  isSentinel: false,
  stats: [{ base: 0xFFFF, g1: 0xFF, g2: 0 }],
  resistances: [],
  defaultEquip: [],
};
const giantProjectClassDefs = new Array(0x4E + 1);
giantProjectClassDefs[0x4E] = giantProjectRecord;
const giantProjectDirty = {};
const giantProjectResult = OB64.patch.applyPatch({
  archives: [], shops: [], itemStats: [], classDefs: giantProjectClassDefs,
}, {
  format: 'ob64-patch',
  version: 33,
  patches: {
    classDefs: {
      77: {
        record_index: 0x4E,
        bytes: {
          0: 0x00, 1: 0x5A,
          43: 0x0B, 44: 2, 45: 0x0B, 46: 2, 47: 0x29, 48: 2,
        },
      },
    },
  },
}, giantProjectDirty);
check('Project class bytes refresh a converted terminator record',
  giantProjectResult.applied.classDefs === 1 && giantProjectDirty.classDefs &&
  giantProjectRecord.stats[0].base === 0x005A &&
  giantProjectRecord.isTerm === false && giantProjectRecord.isSentinel === false &&
  giantProjectRecord.b43Raw === 0x0B && giantProjectRecord.b45Raw === 0x0B &&
  giantProjectRecord.b47Raw === 0x29);

const sentinelRecord = { stats: [{ base: 0x8000 }] };
OB64.refreshClassDefClassification(sentinelRecord);
check('class-record classification retains the existing 0x80 sentinel rule',
  sentinelRecord.isTerm === false && sentinelRecord.isSentinel === true);

// Item records are stat-framed. Logical B28-B31 precede physical B0-B27.
const expectedItem = Uint8Array.from({ length: 32 }, (_, i) => (i * 7 + 3) & 0xFF);
expectedItem[6] = 0xF1;
expectedItem[12] = 0x88;
const item = {
  equipType: expectedItem[0],
  element: expectedItem[1],
  grade: expectedItem[2],
  b3Raw: expectedItem[3],
  price: readU16(expectedItem, 4),
  strRaw: expectedItem[6],
  intRaw: expectedItem[7],
  agiRaw: expectedItem[8],
  dexRaw: expectedItem[9],
  vitRaw: expectedItem[10],
  menRaw: expectedItem[11],
  b12Raw: expectedItem[12],
  resPhys: signedByte(expectedItem[13]),
  resWind: signedByte(expectedItem[14]),
  resFire: signedByte(expectedItem[15]),
  resEarth: signedByte(expectedItem[16]),
  resWater: signedByte(expectedItem[17]),
  resVirtue: signedByte(expectedItem[18]),
  resBane: signedByte(expectedItem[19]),
  growthHpStr: (expectedItem[20] >>> 6) & 3,
  growthUnknown: (expectedItem[20] >>> 4) & 3,
  growthInt: (expectedItem[20] >>> 2) & 3,
  growthAgi: expectedItem[20] & 3,
  growthDex: (expectedItem[21] >>> 6) & 3,
  growthVit: (expectedItem[21] >>> 4) & 3,
  growthMen: (expectedItem[21] >>> 2) & 3,
  growthLck: expectedItem[21] & 3,
};
for (let i = 22; i <= 31; i++) item['b' + i + 'Raw'] = expectedItem[i];

const itemImage = new Uint8Array(OB64.ITEM_STAT_OFFSET + OB64.ITEM_STAT_SIZE * 2 + 16);
OB64.serializeItemStats([null, item], itemImage);
const itemOffset = OB64.ITEM_STAT_OFFSET + OB64.ITEM_STAT_SIZE;
const actualItem = new Uint8Array(32);
actualItem.set(itemImage.subarray(itemOffset, itemOffset + 28), 0);
actualItem.set(itemImage.subarray(itemOffset - 4, itemOffset), 28);
check('Item serialization writes every logical B0-B31 byte',
  equalBytes(actualItem, expectedItem), mismatchDetail(actualItem, expectedItem));

// Class records are also stat-framed. Logical B60-B71 precede physical B0-B59.
const expectedClass = Uint8Array.from({ length: 72 }, (_, i) => (i * 3 + 17) & 0xFF);
expectedClass[0] = 0xFF;
expectedClass[1] = 0xFF;
const classRecord = {
  isTerm: true,
  stats: [],
  lck: expectedClass[23],
  alignment: expectedClass[24],
  resistances: Array.from(expectedClass.subarray(25, 32)),
  moveType: expectedClass[32],
  b33Raw: expectedClass[33],
  defaultEquip: [],
  b42Raw: expectedClass[42],
  b43Raw: expectedClass[43],
  frontAtks: expectedClass[44],
  b45Raw: expectedClass[45],
  midAtks: expectedClass[46],
  b47Raw: expectedClass[47],
  rearAtks: expectedClass[48],
  physAtk: expectedClass[49],
  magAtk: expectedClass[50],
  physDef: expectedClass[51],
  magDef: expectedClass[52],
  baseClass: expectedClass[53],
  baseTransitionLevel: expectedClass[54],
  intermediateClass: expectedClass[55],
  finalTransitionLevel: expectedClass[56],
  classCopyMatch: expectedClass[57],
  dragonElement: expectedClass[58],
  itemCapacity: expectedClass[59],
  namePtr0Raw: expectedClass[60],
  namePtr1Raw: expectedClass[61],
  namePtr2Raw: expectedClass[62],
  namePtr3Raw: expectedClass[63],
  unitSize: expectedClass[64],
  sexOrVoice: expectedClass[65],
  leadership: expectedClass[66],
  headerPad: expectedClass[67],
  baseHp: readU16(expectedClass, 68),
  hpGrowth: expectedClass[70],
  headerTailRaw: expectedClass[71],
};
for (let i = 0; i < 6; i++) {
  const offset = i * 4;
  classRecord.stats.push({
    base: readU16(expectedClass, offset),
    g1: expectedClass[offset + 2],
    g2: expectedClass[offset + 3],
  });
}
classRecord.b3Raw = expectedClass[3];
classRecord.b7Raw = expectedClass[7];
classRecord.b11Raw = expectedClass[11];
classRecord.b15Raw = expectedClass[15];
classRecord.b19Raw = expectedClass[19];
for (let i = 0; i < 4; i++) {
  classRecord.defaultEquip.push(readU16(expectedClass, 34 + i * 2));
}

const classImage = new Uint8Array(
  OB64.CLASS_DEF_OFFSET + OB64.CLASS_DEF_RECORD_SIZE * 3 + 16);
OB64.serializeClassDefs([null, null, classRecord], classImage);
const classOffset = OB64.CLASS_DEF_OFFSET + OB64.CLASS_DEF_RECORD_SIZE * 2;
const actualClass = new Uint8Array(72);
actualClass.set(classImage.subarray(classOffset, classOffset + 60), 0);
actualClass.set(classImage.subarray(classOffset - 12, classOffset), 60);
check('Class serialization writes every logical B0-B71 byte, including a terminator record',
  equalBytes(actualClass, expectedClass), mismatchDetail(actualClass, expectedClass));

console.log(failures ? '\n' + failures + ' FAILURE(S)' : '\nALL TESTS PASSED');
process.exit(failures ? 1 : 0);
