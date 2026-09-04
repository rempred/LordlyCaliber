'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const MASTER_NAME = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const MASTER = [
  process.env.OB64_MASTER_ROM,
  path.resolve(EDITOR, '..', MASTER_NAME),
  path.resolve(EDITOR, '..', '..', 'OgreBattlel64', MASTER_NAME),
].filter(Boolean).find(candidate => fs.existsSync(candidate));

if (!MASTER) throw new Error('Set OB64_MASTER_ROM to the read-only US Rev 0 master ROM.');

global.window = global;
global.module = undefined;
global.localStorage = {
  getItem() { return null; },
  setItem() {},
  removeItem() {},
};
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js',
  'rom-names-data.js',
  'portraits.js',
  'parsers.js',
  'repack.js',
  'description-codec.js',
  'source-redirect.js',
  'stat-gate-relocation.js',
  'combat-animation-overrides-data.js',
  'combat-animation-overrides.js',
  'consumable-effects.js',
  'patch.js',
  'tools-data.js',
  'tools.js',
  'squadblob.js',
  'runtimeblob.js',
  'native-rebuild-data.js',
  'native-rebuild.js',
  'squads-data.js',
  'squads.js',
  'scenario-map-calibration.js',
  'scenario-eset-data.js',
  'scenario-treasure-data.js',
  'scenario-eset-codec.js',
  'scenario.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const masterBytes = new Uint8Array(fs.readFileSync(MASTER));
const data = OB64.nativeRebuild.verifyData();

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function freshRom() {
  const rom = OB64.loadROM(masterBytes.slice().buffer);
  OB64.nativeRebuild.initializeSource(rom);
  OB64.consumableEffects.initializeSession(rom,
    OB64.consumableEffects.evaluateSourceIdentity({
      filename: MASTER_NAME,
      size: rom.z64.length,
      sha256: sha256(rom.z64),
      byteOrder: rom.byteOrder,
      imageName: 'OgreBattle64',
      gameId: 'NOBE',
      country: rom.z64[0x3E],
      version: rom.z64[0x3F],
      crc1: OB64.readU32BE(rom.z64, 0x10),
      crc2: OB64.readU32BE(rom.z64, 0x14),
    }), { filename: MASTER_NAME });
  OB64.patch.snapshotOriginal(rom);
  OB64.tools.initState(rom);
  OB64.nativeRebuild.initializeToolState(rom);
  rom.squadOverrides = {};
  OB64.scenario.ensureState(rom);
  return rom;
}

function acceptedShopRecords() {
  const component = data.shops.components.find(entry =>
    entry.name === 'source-linked-shop-module-and-membership-contract');
  const moduleBytes = Uint8Array.from(Buffer.from(component.dataHex, 'hex'));
  const start = data.shops.module.contractOffset;
  return OB64.nativeRebuild.decodeShopContract(
    moduleBytes.slice(start, start + data.shops.contract.totalBytes));
}

function stageFixtures(rom) {
  const scenarioRow = rom.neutralEncounters.scenarioRates.find(row =>
    Number.isInteger(row.slice));
  assert(scenarioRow, 'retail ROM has no mapped scenario-rate fixture');
  scenarioRow.rateOverride = {
    normal: { mode: 'override', passCount: 102, divisor: 72000 },
    alternate: { mode: 'disabled', passCount: null, divisor: null },
  };

  const weightedDrop = rom.creatureDrops.records.find(record =>
    record && !record.isSentinel && record.isRuntimeActive && record.classId &&
    record.slots && record.slots.length >= 3);
  assert(weightedDrop, 'retail ROM has no weighted-drop fixture');
  weightedDrop.slots[0].weight = 1;
  weightedDrop.slots[1].weight = 2;
  weightedDrop.slots[2].weight = 3;

  rom.neutralEncounters.customSquads = [{
    profileId: 1,
    runtimeKey: scenarioRow.runtimeKey,
    slice: scenarioRow.slice,
    terrainSlot: 2,
    members: [
      { classId: 0x1E, levelOffsetRaw: 0, cell: 0, cohort: 'A' },
      { classId: 0x0F, levelOffsetRaw: 0, cell: 8, cohort: 'B' },
    ],
    label: 'Composition Raiders!',
    persuasion: { mode: 'fixed', chance: 37 },
    retreat: { hpThreshold: 25 },
  }];

  const scenario = OB64.SQUAD_DATA.scenarios.find(row => row.id === 1);
  const squad = scenario && scenario.squads && scenario.squads[0];
  assert(squad, 'retail ROM has no squad fixture');
  const original = OB64._squadTest.vanillaRec(scenario, squad.e);
  const replacement = original.slice();
  replacement[1] = replacement[1] === 1 ? 2 : 1;
  rom.squadOverrides['1:' + squad.e] = replacement;

  return {
    squads: OB64.collectSquadOverrides(rom),
    scenarioRates: OB64.runtimeOverrides.collectScenarioRateOverrides(rom),
    weightedDrops: OB64.runtimeOverrides.collectWeightedDropOverrides(rom),
    customNeutral: OB64.runtimeOverrides.collectCustomNeutralSquads(rom),
  };
}

function applyWrites(candidate, plan) {
  (plan && plan.writes || []).forEach(write => {
    candidate.z64.set(write.bytes, write.offset);
  });
}

function buildState(expected) {
  const rom = freshRom();
  const fixtures = stageFixtures(rom);
  if (expected.allMigratedTools) {
    OB64.tools.setDesiredPercent(rom, 'experience-size-scale', 200);
    rom.tools.desired['cf-army-counter'] = true;
    rom.tools.desired['character-card-luck'] = true;
    rom.tools.desired['squad-menu-alignment'] = true;
  }
  if (expected.nativeShops) {
    acceptedShopRecords().forEach(record => {
      rom.shops[record.index].items = record.equipment.slice();
      rom.shops[record.index].consumables = record.expendables.slice();
    });
  }
  OB64.buildModes.set(rom, 'rebuild');
  const nativePlan = OB64.nativeRebuild.prepareExport(rom, {
    shopsDirty: expected.nativeShops,
  });
  const candidate = Object.assign({}, rom, { z64: nativePlan.baseZ64.slice() });

  const selected = {
    squads: expected.remainingMask & 1 ? fixtures.squads : [],
    scenarioRates: expected.remainingMask & 2 ? fixtures.scenarioRates : [],
    weightedDrops: expected.remainingMask & 4 ? fixtures.weightedDrops : [],
    customNeutral: expected.remainingMask & 8 ? fixtures.customNeutral : [],
  };
  const hasNeutral = selected.scenarioRates.length || selected.weightedDrops.length ||
    selected.customNeutral.length;
  let runtimePlan = null;
  if (hasNeutral) {
    runtimePlan = OB64.runtimeOverrides.buildRuntimeOverrideWrites(
      selected.squads,
      [],
      data.shops.contract.recordCount,
      candidate,
      {
        scenarioRateOverrides: selected.scenarioRates,
        weightedDropOverrides: selected.weightedDrops,
        customNeutralSquads: selected.customNeutral,
      });
  } else if (selected.squads.length) {
    runtimePlan = OB64.squad.buildSquadOverrideWrites(selected.squads, candidate);
  }
  // Rebuild ROM always removes the migrated legacy Shop contributor. The
  // accepted Phase 4 source matrix retained it in native-off proof fixtures,
  // but this integration's controlling product decision requires retail Shop
  // ownership whenever the native owner is unselected.
  if (runtimePlan) {
    OB64.nativeRebuild.detachRuntimePlan(runtimePlan);
  }
  applyWrites(candidate, runtimePlan);

  const runtimeOwner = runtimePlan ? [{
    id: 'runtime-overrides',
    name: 'Shared Runtime Overrides',
    category: 'runtimeOverrides',
    regions: OB64.nativeRebuild.remainingRuntimeRegions(
      rom,
      hasNeutral
        ? OB64.runtimeOverrides.patchRegions(rom)
        : OB64.squad.patchRegions(rom)
    ),
  }] : [];
  const owners = runtimeOwner.concat(nativePlan.owners);
  const conflicts = OB64.consumableEffects.findRegionConflicts(owners);
  assert.strictEqual(conflicts.length, 0,
    expected.id + ' has a runtime/native owner collision');
  nativePlan.compositionAuthorization =
    OB64.nativeRebuild.authorizeComposition(owners);
  const nativeResult = OB64.nativeRebuild.applyPlan(nativePlan, candidate, runtimePlan);
  if ((runtimePlan && runtimePlan.crcWindow) || nativeResult.crc) {
    OB64.recalcN64CRC(candidate.z64);
  }
  OB64.nativeRebuild.validateApplied(candidate, nativePlan, runtimePlan);
  return { candidate, nativePlan, nativeResult, runtimePlan };
}

assert.strictEqual(data.composition.phase4Matrix.stateCount, 32);
for (const expected of data.composition.phase4Matrix.states) {
  const built = buildState(expected);
  const actualSha256 = sha256(built.candidate.z64);
  if (expected.nativeShops || expected.remainingMask === 0) {
    assert.strictEqual(actualSha256, expected.sha256,
      expected.id + ' does not reproduce the accepted composition product');
  } else {
    const repeated = buildState(expected);
    assert.strictEqual(actualSha256, sha256(repeated.candidate.z64),
      expected.id + ' is not deterministic after legacy Shop detachment');
  }
  assert.strictEqual(OB64.consumableEffects.verifyIndependentCrc(
    built.candidate.z64).ok, true, expected.id + ' has an invalid CIC-6102 CRC');
  const classification = OB64.nativeRebuild.classifySource(
    built.candidate.z64, built.candidate);
  assert.strictEqual(classification.owners.shops.state,
    expected.nativeShops ? 'native' : 'retail',
    expected.id + ' has the wrong Shop physical owner');
  assert.deepStrictEqual(
    OB64.nativeRebuild._test.legacyShopFindings(
      built.candidate.z64, built.candidate),
    [],
    expected.id + ' retains a legacy Shop implementation');
}

for (let remainingMask = 0; remainingMask < 16; remainingMask++) {
  const expected = {
    id: 'all-migrated-with-remaining-' + remainingMask.toString(2).padStart(4, '0'),
    nativeShops: true,
    remainingMask,
    allMigratedTools: true,
  };
  const built = buildState(expected);
  const classification = OB64.nativeRebuild.classifySource(
    built.candidate.z64, built.candidate);
  assert.strictEqual(classification.owners['experience-size-scale'].state, 'native');
  assert.strictEqual(classification.owners['experience-size-scale'].percent, 200);
  assert.strictEqual(classification.owners['cf-army-counter'].state, 'native');
  assert.strictEqual(classification.owners['character-card-luck'].state, 'native');
  assert.strictEqual(classification.owners['squad-menu-alignment'].state, 'native');
  assert.strictEqual(classification.owners.shops.state, 'native');
}

const sharedModelRom = freshRom();
const sharedShopIndex = sharedModelRom.shops.findIndex((shop, index) =>
  index > 0 && index < 36 && shop.items && shop.items.length > 1);
assert(sharedShopIndex > 0, 'retail ROM has no cross-mode Shop fixture');
sharedModelRom.shops[sharedShopIndex].items =
  sharedModelRom.shops[sharedShopIndex].items.slice().reverse();
OB64.runtimeOverrides.refreshShopOverrideState(sharedModelRom, sharedShopIndex);
const sharedModelIdentity = JSON.stringify({
  items: sharedModelRom.shops[sharedShopIndex].items,
  consumables: sharedModelRom.shops[sharedShopIndex].consumables,
});

OB64.buildModes.set(sharedModelRom, 'patch');
const legacyOverrides = OB64.runtimeOverrides.collectShopOverrides(sharedModelRom);
const patchCandidate = Object.assign({}, sharedModelRom, {
  z64: sharedModelRom.z64.slice(),
});
const patchPlan = OB64.runtimeOverrides.buildRuntimeOverrideWrites(
  [], legacyOverrides, sharedModelRom.shops.length, patchCandidate, {
    scenarioRateOverrides: [],
    weightedDropOverrides: [],
    customNeutralSquads: [],
  });
applyWrites(patchCandidate, patchPlan);
if (patchPlan.crcWindow) OB64.recalcN64CRC(patchCandidate.z64);

OB64.buildModes.set(sharedModelRom, 'rebuild');
const rebuildPlan = OB64.nativeRebuild.prepareExport(sharedModelRom, {
  shopsDirty: true,
});
rebuildPlan.compositionAuthorization =
  OB64.nativeRebuild.authorizeComposition(rebuildPlan.owners);
const rebuildCandidate = Object.assign({}, sharedModelRom, {
  z64: rebuildPlan.baseZ64.slice(),
});
const rebuildResult = OB64.nativeRebuild.applyPlan(
  rebuildPlan, rebuildCandidate, null);
if (rebuildResult.crc) OB64.recalcN64CRC(rebuildCandidate.z64);
OB64.nativeRebuild.validateApplied(rebuildCandidate, rebuildPlan, null);

assert.strictEqual(JSON.stringify({
  items: sharedModelRom.shops[sharedShopIndex].items,
  consumables: sharedModelRom.shops[sharedShopIndex].consumables,
}), sharedModelIdentity, 'switching export paths mutated the shared edit model');
assert.strictEqual(
  OB64.nativeRebuild.classifySource(patchCandidate.z64, patchCandidate).owners.shops.state,
  'legacy', 'Patch ROM did not keep the legacy Shop producer');
assert.strictEqual(
  OB64.nativeRebuild.classifySource(rebuildCandidate.z64, rebuildCandidate).owners.shops.state,
  'native', 'Rebuild ROM did not use the native Shop producer');
assert.notStrictEqual(sha256(patchCandidate.z64), sha256(rebuildCandidate.z64),
  'the two modes reused one physical product');
assert.throws(() => OB64.nativeRebuild.applyPlan(
  rebuildPlan,
  Object.assign({}, sharedModelRom, { z64: rebuildPlan.baseZ64.slice() }),
  null), /stale Rebuild ROM plan cannot be reused/);
assert.throws(() => OB64.nativeRebuild.validateApplied(
  patchCandidate, rebuildPlan, null), /failed exact readback|read back as|native Shop/);

console.log('PASS 32-state Phase 4 runtime composition with rebuilt retail/native Shop isolation');
console.log('PASS all 16 retained-runtime contributor combinations coexist with every migrated native owner');
console.log('PASS one Shop edit follows two isolated producers without model mutation or stale-plan reuse');
