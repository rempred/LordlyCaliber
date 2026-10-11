'use strict';

// Rebuild ROM writes neutral encounter features through the source-built
// Neutral module: module image and tables, boot load stub, and rebuilt retail
// sites. The legacy shared runtime (hooks, bootstrap, tail) stays retail.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const nativeHarness = fs.readFileSync(path.join(__dirname, 'native-rebuild-runtime-composition.test.js'), 'utf8')
  .split('assert.strictEqual(data.composition.phase4Matrix.stateCount')[0];
const { freshRom } = new Function('require', '__dirname', nativeHarness + '\nreturn { freshRom };')(require, __dirname);
for (const file of ['rom-export-candidate.js', 'native-neutral-data.js', 'native-neutral.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), { filename: file });
}

const D = OB64.NATIVE_NEUTRAL_DATA;
const L = D.layout;

function u32(bytes, offset) {
  return ((bytes[offset] << 24) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
}
function u16(bytes, offset) {
  return (bytes[offset] << 8) | bytes[offset + 1];
}
function same(a, b, offset, size) {
  for (let i = 0; i < size; i++) if (a[offset + i] !== b[offset + i]) return false;
  return true;
}
function jal(target) {
  return (0x0C000000 | ((target & 0x0FFFFFFF) >>> 2)) >>> 0;
}

// No neutral edits: the owner stays inactive, so Rebuild output keeps retail.
assert.strictEqual(OB64.nativeNeutral.isActive(freshRom()), false);

const rom = freshRom();
const retail = rom.z64.slice();
const rateRow = rom.neutralEncounters.scenarioRates.find(row => row.runtimeKey === 1 && Number.isInteger(row.slice));
assert(rateRow, 'scenario key 1 has a rate row');
rateRow.rateOverride = {
  normal: { mode: 'override', passCount: 720, divisor: 72000 },
  alternate: { mode: 'disabled' },
};
const dropRecord = rom.creatureDrops.records.find(rec => rec && !rec.isSentinel && rec.isRuntimeActive && rec.classId && rec.slots);
assert(dropRecord, 'an active creature reward record exists');
dropRecord.slots[0].weight = 0;
dropRecord.slots[1].weight = 5;
dropRecord.slots[2].weight = 1;
assert.strictEqual(OB64.nativeNeutral.isActive(rom), true);

OB64.buildModes.set(rom, 'rebuild');
const nativePlan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: false });
const candidate = OB64.romExportCandidate.create(rom);
candidate.z64 = nativePlan.baseZ64.slice();
const plan = OB64.nativeNeutral.prepare(rom, candidate.z64);
assert.deepStrictEqual(plan.counts, { rates: 1, drops: 1, squads: 0 });

const owners = nativePlan.owners.concat([plan.owner]);
assert.deepStrictEqual(OB64.consumableEffects.findRegionConflicts(owners), []);
nativePlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(owners);
OB64.nativeNeutral.apply(candidate, plan);
const nativeResult = OB64.nativeRebuild.applyPlan(nativePlan, candidate, null);
OB64.recalcN64CRC(candidate.z64);
OB64.nativeNeutral.validateApplied(candidate, plan);
OB64.nativeRebuild.validateApplied(candidate, nativePlan, null);
assert(OB64.consumableEffects.verifyIndependentCrc(candidate.z64).ok, 'the CIC-6102 checksum is valid');
assert(nativeResult);

// Module image, boot load, and sites.
const z = candidate.z64;
assert.strictEqual(u32(z, L.romBase), 0x4C4E4555, 'module header magic');
assert.strictEqual(u32(z, 0x25A8), jal(L.bootStubRam), 'boot site calls the native loader');
for (const site of D.sites) {
  assert.strictEqual(u32(z, site.rom), jal(D.entries[site.entry]), site.id + ' site calls its entry');
}
assert(same(z, Buffer.from(D.bootStub.hex, 'hex'), L.bootStubRom, 0) || true);
assert.strictEqual(Buffer.from(z.subarray(L.bootStubRom, L.bootStubRom + D.bootStub.bytes)).toString('hex').toUpperCase(),
  D.bootStub.hex, 'boot stub bytes');

// Tables in the module image use the Editor's contracts at the native base.
const rate = L.romBase + L.rateOffset;
assert.strictEqual(u32(z, rate), 0x4F424E52, 'OBNR rate table');
const row1 = rate + 8 + 1 * 16;
assert.deepStrictEqual([z[row1], u16(z, row1 + 2), u32(z, row1 + 4)], [1, 719, 72000], 'normal branch override');
assert.strictEqual(z[row1 + 8], 2, 'alternate branch disabled');
const drop = L.romBase + L.dropOffset;
assert.strictEqual(u32(z, drop), 0x4F424457, 'OBDW drop table');
const dropRow = drop + 8 + dropRecord.classId * 8;
assert.deepStrictEqual([z[dropRow], u16(z, dropRow + 2), u16(z, dropRow + 4), u16(z, dropRow + 6)], [1, 0, 5, 1]);
for (let i = L.romBase + L.scratchOffset; i < L.romBase + L.imageBytes; i++) {
  assert.strictEqual(z[i], 0, 'scratch state is zero in the image');
}

// The legacy shared runtime is not installed.
for (const [start, size, label] of [
  [0x00195584, 8, 'squad record-builder hook'],
  [0x000283C4, 0x6C, 'legacy bootstrap cave'],
  [0x02780000, 0x10000, 'legacy runtime tail'],
]) {
  assert(same(z, retail, start, size), label + ' stays retail');
}
// Rebuilt sites reuse some legacy hook offsets, but every one calls the
// native module or boot stub, never the legacy bootstrap or runtime module.
for (const site of D.sites) {
  const target = ((u32(z, site.rom) & 0x03FFFFFF) << 2 | 0x80000000) >>> 0;
  assert(target !== 0x80097FC4 && (target < 0x80400000 || target >= 0x80410000),
    site.id + ' does not call the legacy shared runtime');
  assert(target === L.bootStubRam || (target >= L.ramBase && target < L.ramBase + L.codeLimit),
    site.id + ' calls native code');
}

// Every changed byte belongs to a declared owner.
OB64.consumableEffects.buildChangeRanges(retail, z,
  owners.concat(OB64.consumableEffects.standardPatchOwners(candidate, {})));

console.log('native neutral rebuild: all checks passed');
