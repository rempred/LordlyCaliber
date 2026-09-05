'use strict';

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
const NATIVE_BUNDLE = path.resolve(process.env.OB64_NATIVE_BUNDLE ||
  path.join(EDITOR, 'native-rebuild-data.js'));

global.window = global;
global.module = undefined;
global.localStorage = {
  getItem() { return null; },
  setItem() {},
  removeItem() {},
};

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js',
  'rom-names-data.js',
  'portraits.js',
  'parsers.js',
  'repack.js',
  'description-codec.js',
  'consumable-effects.js',
  'patch.js',
  'tools-data.js',
  'tools.js',
  'native-rebuild-data.js',
  'native-rebuild.js',
  'rom-export-validator.js',
]) {
  const fullPath = filename === 'native-rebuild-data.js' ? NATIVE_BUNDLE :
    path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const masterContainer = new Uint8Array(fs.readFileSync(MASTER));
const retailZ64 = OB64.loadROM(masterContainer.slice().buffer).z64.slice();
const data = OB64.nativeRebuild.verifyData();
const indexSource = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8')
  .replace(/\r\n/g, '\n');
const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8')
  .replace(/\r\n/g, '\n');
const patchSource = fs.readFileSync(path.join(EDITOR, 'patch.js'), 'utf8')
  .replace(/\r\n/g, '\n');
const validatorSource = fs.readFileSync(
  path.join(EDITOR, 'rom-export-validator.js'), 'utf8').replace(/\r\n/g, '\n');
let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function expectThrow(name, action, pattern) {
  try {
    action();
    check(name, false, 'no error was thrown');
  } catch (error) {
    check(name, pattern.test(error.message), error.message);
  }
}

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function equalBytes(left, right) {
  return Buffer.from(left).equals(Buffer.from(right));
}

function hexBytes(hex) {
  return Uint8Array.from(Buffer.from(hex, 'hex'));
}

function freshRom(input) {
  const bytes = input ? input.slice() : masterContainer.slice();
  const rom = OB64.loadROM(bytes.buffer.slice(bytes.byteOffset,
    bytes.byteOffset + bytes.byteLength));
  OB64.nativeRebuild.initializeSource(rom);
  const identity = OB64.consumableEffects.evaluateSourceIdentity({
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
  });
  OB64.consumableEffects.initializeSession(rom, identity, { filename: MASTER_NAME });
  OB64.patch.snapshotOriginal(rom);
  OB64.tools.initState(rom);
  OB64.nativeRebuild.initializeToolState(rom);
  return rom;
}

function baselineRecords(rom) {
  return Array.from({ length: 36 }, (_, index) => ({
    index,
    equipment: (rom.shops[index].items || []).slice(),
    expendables: (rom.shops[index].consumables || []).slice(),
  }));
}

function setRecords(rom, records) {
  records.forEach(record => {
    rom.shops[record.index].items = record.equipment.slice();
    rom.shops[record.index].consumables = record.expendables.slice();
  });
}

function embeddedAcceptedShopRecords() {
  const component = data.shops.components.find(row =>
    row.name === 'source-linked-shop-module-and-membership-contract');
  const bytes = hexBytes(component.dataHex);
  const start = data.shops.module.contractOffset;
  return OB64.nativeRebuild.decodeShopContract(
    bytes.slice(start, start + data.shops.contract.totalBytes));
}

function selectMigrated(rom, mask, experienceSelected, shopSelected) {
  OB64.tools.setDesiredPercent(rom, 'experience-size-scale',
    experienceSelected ? 200 : 100);
  rom.tools.desired['character-card-luck'] = !!(mask & 1);
  rom.tools.desired['squad-menu-alignment'] = !!(mask & 2);
  rom.tools.desired['cf-army-counter'] = !!(mask & 4);
  if (shopSelected) setRecords(rom, embeddedAcceptedShopRecords());
}

function executeNativePlan(rom, shopsDirty) {
  const plan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: !!shopsDirty });
  plan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(
    plan.owners.concat(plan.restorationOwners));
  const candidate = Object.assign({}, rom, { z64: plan.baseZ64.slice() });
  const result = OB64.nativeRebuild.applyPlan(plan, candidate, null);
  if (result.crc) OB64.recalcN64CRC(candidate.z64);
  const validation = OB64.nativeRebuild.validateApplied(candidate, plan, null);
  return { candidate, plan, result, validation };
}

function emptyDirty() {
  return {
    shops: false,
    items: false,
    itemDescriptions: false,
    classDefs: false,
    classDescriptions: false,
    actionDescriptions: false,
    encounters: false,
    creatureDrops: false,
    neutralRuntime: false,
    consumables: false,
    consumableDescriptions: false,
    consumableEffects: false,
    combatAnimationOverrides: false,
    statGates: false,
    tools: false,
    squadOverrides: false,
    scenario: false,
    art: false,
    cutscenes: false,
  };
}

check('authenticated bundle names the accepted decomp commit and tree',
  data.acceptedSource.commit === 'e50fe500839288c94c538969acc7a5b966075626' &&
  data.acceptedSource.tree === 'ef58ea271b2298be71b09be9318a359805c266c0');
check('authenticated bundle names the versioned native input contract',
  data.inputContract.id === 'lordly-native-rebuild.phase5a.editor-inputs.v1' &&
  data.inputContract.sha256 ===
    'A7BE40F1F386239BCA42AAACBF60CCAF0B673051ACD74DFE90D50BA53C11223E');
check('authenticated bundle names the accepted Shop contract and composition manifest',
  data.shops.contractSha256 ===
    'C6D71952A0AE7A813C3F79D8EEAAA4CCAADF53168E2AA627BE577726F64FD200' &&
  data.composition.manifestSha256 ===
    'DBCA8D456D8D786DE9A69130CBABF9CCF6FB27B8E746DEC0E0616511B4D420A3');
check('authenticated bundle records build reports, layouts, relocations, and object manifests',
  data.acceptedSource.inputs.length === 52 &&
  data.acceptedSource.inputs.some(input => input.path.endsWith('objects/manifest.json')) &&
  data.acceptedSource.inputs.filter(input =>
    input.path.endsWith('relocation-ledger.json')).length === 3 &&
  data.composition.phase4Extension.ownershipTransition.nativePhysicalOwner ===
    'runtime.shop-overrides');
check('Rebuild ROM declares the accepted 8 MiB RDRAM product',
  data.acceptedSource.runtimeRdramBytes === 8 * 1024 * 1024);
check('the normalized read-only master is exact accepted US Rev 0',
  sha256(retailZ64) === data.acceptedSource.retailSha256);
check('the two public build-mode labels are exact',
  OB64.buildModes.info('rebuild').label === 'Rebuild ROM' &&
  OB64.buildModes.info('patch').label === 'Patch ROM');
check('the export workflow exposes both exact visible option labels',
  indexSource.includes('<option value="rebuild">Rebuild ROM</option>') &&
  indexSource.includes('<option value="patch" selected>Patch ROM</option>') &&
  indexSource.includes('id="build-mode-help"'));
check('authenticated native data loads before the rebuild module and export controller',
  indexSource.indexOf('native-rebuild-data.js') <
    indexSource.indexOf('native-rebuild.js') &&
  indexSource.indexOf('native-rebuild.js') < indexSource.indexOf('app.js'));
const modeHandler = appSource.slice(
  appSource.indexOf("buildModeSelect.addEventListener('change'"),
  appSource.indexOf('// ============================================================\n  // ROM Loading'));
check('mode selection reaches durable model state before rerender',
  modeHandler.indexOf('OB64.buildModes.set(rom, buildModeSelect.value)') >= 0 &&
  modeHandler.indexOf('OB64.buildModes.set(rom, buildModeSelect.value)') <
    modeHandler.indexOf('renderTab(activeTab)'));
const renderDispatcher = appSource.slice(
  appSource.indexOf('function renderTab(tab)'),
  appSource.indexOf('function renderSaveGame'));
check('every tab rerender restores the durable build-mode selection',
  renderDispatcher.indexOf('syncBuildModeControl()') >= 0);
check('each export mode has a distinct producer filename and validation record',
  appSource.includes("buildMode === 'rebuild' ? 'ob64_rebuilt' : 'ob64_modified'") &&
  validatorSource.includes('buildModeLabel: buildModeLabel') &&
  validatorSource.includes('nativeProvenance: options && options.nativePlan'));
check('failed Rebuild ROM validation cannot expose the legacy download bypass',
  appSource.includes("if (buildMode === 'patch') {\n          validationActions = {") &&
  appSource.includes('showRomValidationModal(validationReport, validationActions);'));
check('Project schema explicitly serializes build mode and defaults older files to Patch ROM',
  patchSource.includes('build_mode: OB64.buildModes') &&
  patchSource.includes("? patch.build_mode : 'patch'") &&
  patchSource.includes('Project schema 35 requires an explicit build_mode'));
check('the active Shop capacities are mode-specific',
  OB64.nativeRebuild.capacities('patch').equipment === 50 &&
  OB64.nativeRebuild.capacities('patch').expendables === 15 &&
  OB64.nativeRebuild.capacities('rebuild').equipment === 50 &&
  OB64.nativeRebuild.capacities('rebuild').expendables === 30);
check('Patch ROM retains the legacy Shop ordering path while Rebuild ROM preserves authored order',
  appSource.includes("if (currentBuildMode() === 'patch') {\n      var legacyKept") &&
  appSource.includes('shop.consumables = currentBuildMode() === \'patch\'') &&
  appSource.includes(': selectedOrder.slice();'));

const allOff = freshRom();
OB64.buildModes.set(allOff, 'rebuild');
const allOffBuild = executeNativePlan(allOff, false);
check('all migrated owners off produces exact US Rev 0',
  sha256(allOffBuild.candidate.z64) === data.acceptedSource.retailSha256);
check('all-off build retains the two mandatory native consumable owners',
  allOffBuild.plan.owners.map(owner => owner.id).sort().join('|') ===
    'effects.consumable-code|text.consumable-descriptions' &&
  allOffBuild.plan.writes.length === 2);

for (const matrixEntry of data.phase3.matrix) {
  const rom = freshRom();
  OB64.buildModes.set(rom, 'rebuild');
  selectMigrated(rom, matrixEntry.mask, false, false);
  const built = executeNativePlan(rom, false);
  check('Phase 3 native matrix ' + matrixEntry.id + ' reproduces the accepted product',
    sha256(built.candidate.z64) === matrixEntry.sha256,
    sha256(built.candidate.z64));
}

const experienceRom = freshRom();
OB64.buildModes.set(experienceRom, 'rebuild');
selectMigrated(experienceRom, 0, true, false);
const experienceBuild = executeNativePlan(experienceRom, false);
const experienceRegion = experienceBuild.candidate.z64.slice(
  data.experience.romStart, data.experience.romStart + data.experience.bytes);
check('Experience Scale 200 uses the accepted source-generated owner',
  sha256(experienceRegion) === data.experience.proof200Sha256);
check('Rebuild ROM records only one Experience Scale physical owner',
  experienceBuild.plan.owners.filter(owner =>
    owner.id === 'tool.experience-size-scale').length === 1);
check('every displayed Experience Scale value has authenticated source coverage',
  data.experience.coverageSha256.every((expected, percent) =>
    sha256(OB64.nativeRebuild._test.experienceBytes(percent)) === expected));

const acceptedShopRecords = embeddedAcceptedShopRecords();
const shopRom = freshRom();
OB64.buildModes.set(shopRom, 'rebuild');
setRecords(shopRom, acceptedShopRecords);
const shopBuild = executeNativePlan(shopRom, true);
check('one native Shop edit reproduces the accepted Phase 4 product',
  sha256(shopBuild.candidate.z64) === data.shops.products[0].sha256,
  sha256(shopBuild.candidate.z64));
const shopValidationReport = OB64.romExportValidator.validate({
  buildMode: 'rebuild',
  buildModeLabel: 'Rebuild ROM',
  sourceRom: shopRom,
  candidateRom: shopBuild.candidate,
  dirty: Object.assign(emptyDirty(), { shops: true }),
  touched: ['native owners: Shop memberships'],
  owners: shopBuild.plan.owners,
  nativePlan: shopBuild.plan,
  nativeResult: shopBuild.result,
});
check('the complete export validator authenticates and reloads the Phase 4 product',
  shopValidationReport.ok &&
  shopValidationReport.buildMode === 'rebuild' &&
  shopValidationReport.nativeProvenance.acceptedSourceCommit ===
    data.acceptedSource.commit,
  JSON.stringify(shopValidationReport.errors));
check('reloaded Rebuild ROM restores all Shop memberships through the contract',
  (() => {
    const reloaded = freshRom(shopBuild.candidate.z64);
    return reloaded.buildMode === 'rebuild' &&
      acceptedShopRecords.every(record =>
        equalBytes(reloaded.shops[record.index].items, record.equipment) &&
        equalBytes(reloaded.shops[record.index].consumables, record.expendables));
  })());

const emptyRecords = baselineRecords(freshRom()).map(record => ({
  index: record.index,
  equipment: [],
  expendables: [],
}));
const emptyContract = OB64.nativeRebuild.encodeShopContract(emptyRecords);
check('empty native Shop lists round-trip exactly',
  OB64.nativeRebuild.decodeShopContract(emptyContract).every(record =>
    record.equipment.length === 0 && record.expendables.length === 0));
check('record zero remains the empty sentinel',
  emptyContract[0x40] === 0 && emptyContract[0x41] === 1 &&
  emptyContract[0x42] === 0 && emptyContract[0x43] === 0);
check('empty records zero every encoded slot',
  Array.from({ length: 36 }, (_, index) => {
    const base = 0x40 + index * 0xA8;
    return emptyContract.slice(base + 4, base + 0xA8)
      .every(value => value === 0);
  }).every(Boolean));

const capacityRecords = emptyRecords.map(record => ({
  index: record.index,
  equipment: record.equipment.slice(),
  expendables: record.expendables.slice(),
}));
capacityRecords[1].equipment = Array.from({ length: 50 }, (_, index) => index + 1);
capacityRecords[1].expendables = Array.from({ length: 30 }, (_, index) => index + 1);
const capacityContract = OB64.nativeRebuild.encodeShopContract(capacityRecords);
const capacityRoundTrip = OB64.nativeRebuild.decodeShopContract(capacityContract)[1];
check('exactly 50 equipment and 30 Expendables preserve authored order',
  equalBytes(capacityRoundTrip.equipment, capacityRecords[1].equipment) &&
  equalBytes(capacityRoundTrip.expendables, capacityRecords[1].expendables));
check('first unused capacity slots are explicit zero terminators',
  capacityContract[0x40 + 0x04 + 50 * 2] === 0 &&
  capacityContract[0x40 + 0x04 + 50 * 2 + 1] === 0 &&
  capacityContract[0x40 + 0x6A + 30 * 2] === 0 &&
  capacityContract[0x40 + 0x6A + 30 * 2 + 1] === 0);

function invalidRecords(mutator) {
  const records = emptyRecords.map(record => ({
    index: record.index,
    equipment: record.equipment.slice(),
    expendables: record.expendables.slice(),
  }));
  mutator(records);
  return records;
}

expectThrow('the 51st equipment entry fails closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].equipment = Array.from({ length: 51 }, (_, index) => index + 1);
  })), /capacity is 50/);
expectThrow('the 31st Expendable entry fails closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].expendables = Array.from({ length: 31 }, (_, index) => index + 1);
  })), /capacity is 30/);
expectThrow('equipment identifier zero fails closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].equipment = [0];
  })), /reserved identifier zero/);
expectThrow('duplicate identifiers fail closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].equipment = [1, 1];
  })), /duplicate identifier 1/);
expectThrow('out-of-range equipment fails closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].equipment = [278];
  })), /outside 1\.\.277/);
expectThrow('out-of-range Expendables fail closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].expendables = [45];
  })), /outside 1\.\.44/);
expectThrow('numeric strings fail closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].equipment = ['1'];
  })), /not an integer/);
expectThrow('unknown Shop record fields fail closed', () =>
  OB64.nativeRebuild.encodeShopContract(invalidRecords(records => {
    records[1].unknownFlag = true;
  })), /unknown field or flag/);
const unknownFlagsContract = emptyContract.slice();
unknownFlagsContract[0x41] = 3;
expectThrow('unknown encoded record flags fail closed', () =>
  OB64.nativeRebuild.decodeShopContract(unknownFlagsContract),
  /invalid index or unknown flags/);
const nonzeroTailContract = emptyContract.slice();
nonzeroTailContract[0x44] = 1;
expectThrow('nonzero bytes after an empty list fail closed', () =>
  OB64.nativeRebuild.decodeShopContract(nonzeroTailContract),
  /equipment tail is nonzero/);

const oneShopRom = freshRom();
OB64.buildModes.set(oneShopRom, 'rebuild');
oneShopRom.shops[1].items = [7, 3, 9];
oneShopRom.shops[1].consumables = [4, 2];
const oneShopBuild = executeNativePlan(oneShopRom, true);
const oneShopRecords = OB64.nativeRebuild.classifySource(
  oneShopBuild.candidate.z64, oneShopBuild.candidate).owners.shops.records;
check('one edited Shop preserves its exact authored membership order',
  equalBytes(oneShopRecords[1].equipment, [7, 3, 9]) &&
  equalBytes(oneShopRecords[1].expendables, [4, 2]));
const multipleShopRom = freshRom();
OB64.buildModes.set(multipleShopRom, 'rebuild');
multipleShopRom.shops[1].items = [9, 4];
multipleShopRom.shops[2].items = [17, 2, 8];
multipleShopRom.shops[1].consumables = [];
multipleShopRom.shops[2].consumables = [5, 1];
const multipleShopBuild = executeNativePlan(multipleShopRom, true);
const multipleShopRecords = OB64.nativeRebuild.classifySource(
  multipleShopBuild.candidate.z64, multipleShopBuild.candidate).owners.shops.records;
check('distinct edits in multiple Shops remain distinct after rebuild',
  equalBytes(multipleShopRecords[1].equipment, [9, 4]) &&
  equalBytes(multipleShopRecords[2].equipment, [17, 2, 8]) &&
  multipleShopRecords[1].expendables.length === 0 &&
  equalBytes(multipleShopRecords[2].expendables, [5, 1]));

let combinationCount = 0;
for (let experienceSelected = 0; experienceSelected <= 1; experienceSelected++) {
  for (let shopSelected = 0; shopSelected <= 1; shopSelected++) {
    for (let mask = 0; mask < 8; mask++) {
      const rom = freshRom();
      OB64.buildModes.set(rom, 'rebuild');
      selectMigrated(rom, mask, !!experienceSelected, !!shopSelected);
      const built = executeNativePlan(rom, !!shopSelected);
      const ownerIds = built.plan.owners.map(owner => owner.id);
      const uniqueOwnerIds = new Set(ownerIds);
      check('migrated-owner combination ' + experienceSelected + shopSelected + '-' + mask +
        ' has one physical owner per contributor',
      uniqueOwnerIds.size === ownerIds.length &&
        built.validation.details.migratedFeatureMask === mask);
      combinationCount++;
    }
  }
}
check('all 32 migrated-owner on/off combinations were exercised', combinationCount === 32);

const passthroughRom = freshRom();
passthroughRom.tools.desired['high-attack-streamsplit'] = true;
const passthroughOwners = OB64.nativeRebuild.passthroughToolOwners(passthroughRom);
const passthroughAuthorization = OB64.nativeRebuild.authorizeComposition(passthroughOwners);
check('the non-migrated High Attack tool is an authorized passthrough owner',
  passthroughAuthorization.owners.length === 1 &&
  passthroughAuthorization.owners[0].id === 'tool.high-attack-streamsplit');
const passthroughResult = OB64.tools.applyDesired(passthroughRom, {
  excludeIds: OB64.nativeRebuild.migratedToolIds,
});
check('passthrough Tool application suppresses every migrated legacy writer',
  passthroughResult.applied.length === 1 &&
  passthroughResult.applied[0] ===
    OB64.tools.getFeature('high-attack-streamsplit').name);
expectThrow('the composition authority rejects a migrated legacy Tool producer', () =>
  OB64.nativeRebuild.authorizeComposition([{
    id: 'tool-cf-army-counter',
    name: 'Legacy Chaos Frame Counter',
    category: 'tools',
    regions: OB64.tools.featureRegions(OB64.tools.getFeature('cf-army-counter')),
  }]), /cannot own migrated contributor/);

const fullNativeOwnerRom = freshRom();
selectMigrated(fullNativeOwnerRom, 7, true, true);
const fullNativeOwnerPlan = OB64.nativeRebuild.prepareExport(
  fullNativeOwnerRom, { shopsDirty: true });
let passthroughCoverage = 0;
data.composition.owners.forEach(ownerDefinition => {
  if (data.composition.migratedOwners.includes(ownerDefinition.id) ||
      data.composition.retiredOwners.includes(ownerDefinition.id)) return;
  const kind = ownerDefinition.rom.length ? 'rom' : 'ram';
  const bound = ownerDefinition[kind][0];
  const representative = {
    id: ownerDefinition.id,
    name: ownerDefinition.id,
    category: 'manifest-passthrough-test',
    regions: [{
      kind,
      start: bound.start,
      size: 1,
      label: 'representative accepted bound',
    }],
  };
  const combined = fullNativeOwnerPlan.owners.concat([representative]);
  const conflicts = OB64.consumableEffects.findRegionConflicts(combined);
  const authorization = OB64.nativeRebuild.authorizeComposition(combined);
  check('passthrough owner ' + ownerDefinition.id +
    ' is authorized beside all migrated native owners',
  conflicts.length === 0 && authorization.owners.some(entry =>
    entry.id === ownerDefinition.id));
  passthroughCoverage++;
});
check('every non-migrated Phase 2 owner has coexistence coverage',
  passthroughCoverage === data.composition.owners.filter(owner =>
    !data.composition.migratedOwners.includes(owner.id) &&
    !data.composition.retiredOwners.includes(owner.id)).length);
expectThrow('unknown composition producers fail closed', () =>
  OB64.nativeRebuild.authorizeComposition([{
    id: 'unknown-producer', name: 'Unknown', regions: [],
  }]), /does not authorize producer/);
expectThrow('a passthrough producer cannot claim two physical owner records', () =>
  OB64.nativeRebuild.authorizeComposition([
    passthroughOwners[0], passthroughOwners[0],
  ]), /appears more than once/);
expectThrow('a migrated native owner with an altered layout fails closed', () =>
  OB64.nativeRebuild.authorizeComposition([{
    id: experienceBuild.plan.owners[0].id,
    name: experienceBuild.plan.owners[0].name,
    category: 'nativeRebuild',
    regions: experienceBuild.plan.owners[0].regions.map((region, index) =>
      Object.assign({}, region, index === 0 ? { size: region.size + 1 } : {})),
  }]), /does not match its authenticated layout/);
expectThrow('two physical native owners for one contributor fail closed', () =>
  OB64.nativeRebuild.authorizeComposition([
    experienceBuild.plan.owners[0], experienceBuild.plan.owners[0],
  ]), /more than one physical owner/);

const mutatedPlanRom = freshRom();
OB64.buildModes.set(mutatedPlanRom, 'rebuild');
selectMigrated(mutatedPlanRom, 1, false, false);
const mutatedPlan = OB64.nativeRebuild.prepareExport(mutatedPlanRom, {
  shopsDirty: false,
});
mutatedPlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(
  mutatedPlan.owners);
mutatedPlan.writes[0].bytes[0] ^= 0x01;
const mutationCandidate = Object.assign({}, mutatedPlanRom, {
  z64: mutatedPlan.baseZ64.slice(),
});
const mutationCandidateHash = sha256(mutationCandidate.z64);
expectThrow('a native plan changed after preflight fails before candidate mutation', () =>
  OB64.nativeRebuild.applyPlan(mutatedPlan, mutationCandidate, null),
  /plan changed after authenticated preflight/);
check('rejected plan mutation leaves the detached candidate unchanged',
  sha256(mutationCandidate.z64) === mutationCandidateHash);

const mixedBytes = shopBuild.candidate.z64.slice();
mixedBytes.set(hexBytes(data.shops.legacy.retailLaneHex).slice(0, 4),
  data.shops.components[0].romStart);
const mixedRom = freshRom(mixedBytes);
OB64.buildModes.set(mixedRom, 'rebuild');
expectThrow('mixed native and non-native ownership fails before export', () =>
  OB64.nativeRebuild.prepareExport(mixedRom, { shopsDirty: false }),
  /partial, mixed, or unknown migrated ownership/);
expectThrow('Patch ROM rejects an authenticated rebuilt source', () =>
  OB64.nativeRebuild.patchSourceCheck(freshRom(shopBuild.candidate.z64)),
  /cannot export from a Rebuild ROM/);
expectThrow('Patch ROM rejects a partially installed native owner', () =>
  OB64.nativeRebuild.patchSourceCheck(mixedRom),
  /cannot export from a Rebuild ROM/);

const legacyExperienceRom = freshRom();
OB64.tools.setDesiredPercent(legacyExperienceRom, 'experience-size-scale', 85);
legacyExperienceRom.tools.desired['cf-army-counter'] = true;
const legacyExperienceApply = OB64.tools.applyDesired(legacyExperienceRom);
if (legacyExperienceApply.crc) OB64.recalcN64CRC(legacyExperienceRom.z64);
const reloadedLegacyExperienceRom = freshRom(legacyExperienceRom.z64);
const legacyExperienceClassification =
  reloadedLegacyExperienceRom.nativeRebuild.sourceClassification;
check('byte-identical legacy Experience Scale ownership is recorded as equivalent',
  legacyExperienceClassification.owners['experience-size-scale'].state === 'native' &&
  legacyExperienceClassification.owners['experience-size-scale'].percent === 85 &&
  legacyExperienceClassification.owners['experience-size-scale'].legacyEquivalent === true);
check('legacy migrated ownership keeps the equivalent Experience product in Patch ROM mode',
  legacyExperienceClassification.hasLegacy === true &&
  legacyExperienceClassification.hasExclusiveNative === false &&
  reloadedLegacyExperienceRom.buildMode === 'patch');
let equivalentPatchSourceError = null;
try {
  OB64.nativeRebuild.patchSourceCheck(reloadedLegacyExperienceRom);
} catch (error) {
  equivalentPatchSourceError = error;
}
check('Patch ROM accepts its byte-identical legacy Experience Scale output on reload',
  equivalentPatchSourceError === null,
  equivalentPatchSourceError && equivalentPatchSourceError.message);

const changedSnapshot = freshRom();
changedSnapshot.nativeRebuild.sourceZ64[0x2000] ^= 0x01;
expectThrow('Rebuild ROM authenticates its immutable loaded-source snapshot', () =>
  OB64.nativeRebuild.prepareExport(changedSnapshot, { shopsDirty: false }),
  /snapshot failed its recorded SHA-256 identity/);

const fakeRev1 = freshRom();
fakeRev1.layout = { id: 'us-rev1', name: 'US Rev 1' };
OB64.buildModes.set(fakeRev1, 'rebuild');
const rev1SourceHash = sha256(fakeRev1.nativeRebuild.sourceZ64);
expectThrow('US Rev 1 Rebuild ROM fails before writing', () =>
  OB64.nativeRebuild.prepareExport(fakeRev1, { shopsDirty: false }),
  /US Rev 1 has no accepted rebuilt placement/);
check('failed US Rev 1 rebuild leaves its immutable source unchanged',
  sha256(fakeRev1.nativeRebuild.sourceZ64) === rev1SourceHash);

const rejectedCapacityRom = freshRom();
OB64.buildModes.set(rejectedCapacityRom, 'rebuild');
rejectedCapacityRom.shops[1].items = Array.from({ length: 51 }, (_, index) => index + 1);
const rejectedCapacityModel = JSON.stringify(rejectedCapacityRom.shops[1].items);
const rejectedCapacitySource = sha256(rejectedCapacityRom.z64);
const rejectedCapacitySnapshot = sha256(rejectedCapacityRom.nativeRebuild.sourceZ64);
expectThrow('an over-capacity Rebuild ROM fails during preflight', () =>
  OB64.nativeRebuild.prepareExport(rejectedCapacityRom, { shopsDirty: true }),
  /contains 51 entries; capacity is 50/);
check('failed Rebuild ROM preflight leaves the model and source images unchanged',
  JSON.stringify(rejectedCapacityRom.shops[1].items) === rejectedCapacityModel &&
  sha256(rejectedCapacityRom.z64) === rejectedCapacitySource &&
  sha256(rejectedCapacityRom.nativeRebuild.sourceZ64) === rejectedCapacitySnapshot);

const projectSource = freshRom();
OB64.buildModes.set(projectSource, 'rebuild');
projectSource.shops[1].items = [7, 3, 9];
projectSource.shops[1].consumables = [4, 2];
OB64.tools.setDesiredPercent(projectSource, 'experience-size-scale', 137);
projectSource.tools.desired['cf-army-counter'] = true;
projectSource.tools.desired['character-card-luck'] = false;
projectSource.tools.desired['squad-menu-alignment'] = true;
const project = OB64.patch.collectPatch(projectSource);
check('Project schema 35 records Rebuild ROM explicitly',
  project.version === 35 && project.build_mode === 'rebuild' &&
  project.summary.build_mode === 'Rebuild ROM' &&
  project.rebuild_owners.experience_scale_percent === 137 &&
  project.rebuild_owners.chaos_frame_counter === true &&
  project.rebuild_owners.character_card_luck === false &&
  project.rebuild_owners.squad_menu_alignment === true &&
  project.rebuild_owners.shop_memberships === true);
check('Rebuild ROM Project stores all 35 real Shops and no legacy migrated tool records',
  Object.keys(project.patches.shops).length === 35 &&
  ['1', '35'].every(key => Object.prototype.hasOwnProperty.call(
    project.patches.shops, key)) &&
  OB64.nativeRebuild.migratedToolIds.every(id =>
    !Object.prototype.hasOwnProperty.call(project.patches.tools, id)));
const projectTarget = freshRom();
const projectResult = OB64.patch.applyPatch(projectTarget, project, emptyDirty());
check('Project load restores mode, every native tool choice, and Shop membership order',
  projectResult.buildMode === 'rebuild' && projectTarget.buildMode === 'rebuild' &&
  OB64.tools.desiredPercent(projectTarget,
    OB64.tools.getFeature('experience-size-scale')) === 137 &&
  projectTarget.tools.desired['cf-army-counter'] === true &&
  projectTarget.tools.desired['character-card-luck'] === false &&
  projectTarget.tools.desired['squad-menu-alignment'] === true &&
  OB64.nativeRebuild.shopOwnerSelected(projectTarget) &&
  equalBytes(projectTarget.shops[1].items, [7, 3, 9]) &&
  equalBytes(projectTarget.shops[1].consumables, [4, 2]));
const projectSourceBuild = executeNativePlan(projectSource, true);
const projectTargetBuild = executeNativePlan(projectTarget, false);
check('Project round trip reproduces the same rebuilt native product',
  equalBytes(projectSourceBuild.candidate.z64, projectTargetBuild.candidate.z64));

const reloadedNativeProjectSource = freshRom(shopBuild.candidate.z64);
const reloadedNativeProject = OB64.patch.collectPatch(reloadedNativeProjectSource);
const reloadedNativeTarget = freshRom();
OB64.patch.applyPatch(reloadedNativeTarget, reloadedNativeProject, emptyDirty());
const reloadedNativeTargetBuild = executeNativePlan(reloadedNativeTarget, false);
check('Project saved from a rebuilt ROM preserves all decoded native Shop data',
  reloadedNativeProject.rebuild_owners.shop_memberships === true &&
  Object.keys(reloadedNativeProject.patches.shops).length === 35 &&
  equalBytes(reloadedNativeTargetBuild.candidate.z64, shopBuild.candidate.z64));

const allOffProjectSource = freshRom();
OB64.buildModes.set(allOffProjectSource, 'rebuild');
const allOffProject = OB64.patch.collectPatch(allOffProjectSource);
const nativeOwnerDisableTarget = freshRom(shopBuild.candidate.z64);
OB64.patch.applyPatch(nativeOwnerDisableTarget, allOffProject, emptyDirty());
const disabledNativeBuild = executeNativePlan(nativeOwnerDisableTarget, false);
check('an explicit all-off Rebuild Project restores a loaded native owner to retail',
  disabledNativeBuild.plan.owners.length === 2 &&
  disabledNativeBuild.plan.restorationOwners.length === 1 &&
  equalBytes(disabledNativeBuild.candidate.z64, retailZ64));
const resavedAllOffProject = OB64.patch.collectPatch(nativeOwnerDisableTarget);
check('a restored native Shop owner remains unselected when the Project is saved again',
  resavedAllOffProject.rebuild_owners.shop_memberships === false &&
  Object.keys(resavedAllOffProject.patches.shops).length === 0);
const disabledNativeReport = OB64.romExportValidator.validate({
  buildMode: 'rebuild',
  buildModeLabel: 'Rebuild ROM',
  sourceRom: nativeOwnerDisableTarget,
  candidateRom: disabledNativeBuild.candidate,
  dirty: Object.assign(emptyDirty(), { shops: true }),
  touched: ['retail Shop ownership restoration'],
  owners: disabledNativeBuild.plan.owners.concat(
    disabledNativeBuild.plan.restorationOwners),
  nativePlan: disabledNativeBuild.plan,
  nativeResult: disabledNativeBuild.result,
});
check('retail restoration has complete ownership and unrelated-byte coverage',
  disabledNativeReport.ok, JSON.stringify(disabledNativeReport.errors));

const uncoveredPlanRom = freshRom();
selectMigrated(uncoveredPlanRom, 1, false, false);
const uncoveredPlan = OB64.nativeRebuild.prepareExport(uncoveredPlanRom, {});
uncoveredPlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition([]);
expectThrow('a composition authorization cannot omit an active migrated owner', () =>
  OB64.nativeRebuild.applyPlan(uncoveredPlan,
    Object.assign({}, uncoveredPlanRom, { z64: uncoveredPlan.baseZ64.slice() }), null),
  /does not cover the exact migrated-owner plan/);

const alteredBasePlanRom = freshRom();
selectMigrated(alteredBasePlanRom, 1, false, false);
const alteredBasePlan = OB64.nativeRebuild.prepareExport(alteredBasePlanRom, {});
alteredBasePlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(
  alteredBasePlan.owners);
alteredBasePlan.baseZ64[0x200000] ^= 0xFF;
expectThrow('the authenticated plan rejects a changed native base image', () =>
  OB64.nativeRebuild.applyPlan(alteredBasePlan,
    Object.assign({}, alteredBasePlanRom, { z64: alteredBasePlan.baseZ64.slice() }), null),
  /changed after authenticated preflight/);

const incompleteOwnerProject = JSON.parse(JSON.stringify(project));
delete incompleteOwnerProject.rebuild_owners.character_card_luck;
expectThrow('incomplete rebuilt owner state fails closed before Project mutation', () =>
  OB64.patch.applyPatch(freshRom(), incompleteOwnerProject, emptyDirty()),
  /fields are incomplete or unknown/);
const incompleteShopProject = JSON.parse(JSON.stringify(project));
delete incompleteShopProject.patches.shops['35'];
expectThrow('partial native Shop Project data fails closed', () =>
  OB64.patch.applyPatch(freshRom(), incompleteShopProject, emptyDirty()),
  /requires complete records 1 through 35/);
const ambiguousPatchProject = OB64.patch.collectPatch(freshRom());
ambiguousPatchProject.rebuild_owners = project.rebuild_owners;
expectThrow('Patch ROM Project cannot carry ambiguous native owner state', () =>
  OB64.patch.applyPatch(freshRom(), ambiguousPatchProject, emptyDirty()),
  /rebuild_owners must be null/);
const oldProject = OB64.patch.collectPatch(freshRom());
oldProject.version = 34;
delete oldProject.build_mode;
delete oldProject.rebuild_owners;
delete oldProject.summary.build_mode;
delete oldProject.summary.native_owners_selected;
const oldTarget = freshRom();
const oldResult = OB64.patch.applyPatch(oldTarget, oldProject, emptyDirty());
check('older Projects without build_mode default to Patch ROM',
  oldResult.buildMode === 'patch' && oldTarget.buildMode === 'patch');
const badProject = JSON.parse(JSON.stringify(project));
badProject.build_mode = 'hybrid';
expectThrow('unknown Project build modes fail closed', () =>
  OB64.patch.applyPatch(freshRom(), badProject, emptyDirty()),
  /build_mode must be/);

const scenarioOwnedKey = '41:200';
const scenarioOwnedRecord = Uint8Array.from(
  { length: 35 }, (_, index) => (index * 7 + 3) & 0xFF);
const scenarioOwnedProject = OB64.patch.collectPatch(freshRom());
scenarioOwnedProject.patches.squadOverrides = {
  [scenarioOwnedKey]: {
    scenario_id: 41,
    edat_id: 200,
    record: Buffer.from(scenarioOwnedRecord).toString('hex').toUpperCase(),
  },
};
scenarioOwnedProject.patches.scenario = { squadLevelCopies: [{}] };
const originalScenarioApi = OB64.scenario;
let preparedScenarioRecord = scenarioOwnedRecord.slice();
OB64.scenario = {
  prepareProject(rom) {
    return {
      targetRom: rom,
      scenarioEditor: {},
      squadOverrides: rom.squadOverrides || {},
      scenarioOverrides: { [scenarioOwnedKey]: preparedScenarioRecord.slice() },
      removedOverrideKeys: [],
    };
  },
  applyPreparedProject(rom, prepared) {
    rom.squadOverrides = rom.squadOverrides || {};
    Object.keys(prepared.scenarioOverrides).forEach(key => {
      rom.squadOverrides[key] = prepared.scenarioOverrides[key].slice();
    });
  },
  loadProject() {},
};
try {
  const scenarioOwnedTarget = freshRom();
  let scenarioOwnedResult = null;
  let scenarioOwnedError = null;
  try {
    scenarioOwnedResult = OB64.patch.applyPatch(
      scenarioOwnedTarget, scenarioOwnedProject, emptyDirty());
  } catch (error) {
    scenarioOwnedError = error;
  }
  check('Scenario-owned squad aliases load without false runtime-key warnings',
    scenarioOwnedError === null &&
    scenarioOwnedResult.applied.squadOverrides === 1 &&
    !scenarioOwnedResult.warnings.some(warning =>
      warning.includes('does not match a known runtime-key EDAT row')) &&
    equalBytes(scenarioOwnedTarget.squadOverrides[scenarioOwnedKey], scenarioOwnedRecord),
    scenarioOwnedError && scenarioOwnedError.message);

  preparedScenarioRecord = scenarioOwnedRecord.slice();
  preparedScenarioRecord[0] ^= 0xFF;
  expectThrow('conflicting duplicate Scenario and squad override owners fail closed', () =>
    OB64.patch.applyPatch(freshRom(), scenarioOwnedProject, emptyDirty()),
    /conflicts with the prepared Scenario owner/);
} finally {
  if (originalScenarioApi === undefined) delete OB64.scenario;
  else OB64.scenario = originalScenarioApi;
}

const modeModel = freshRom();
modeModel.shops[2].items = [11, 5];
modeModel.tools.desired['cf-army-counter'] = true;
const modelBefore = JSON.stringify({
  shops: modeModel.shops.map(shop => shop.items),
  tools: modeModel.tools.desired,
});
OB64.buildModes.set(modeModel, 'rebuild');
OB64.buildModes.set(modeModel, 'patch');
check('switching modes does not mutate the in-memory edit model',
  JSON.stringify({
    shops: modeModel.shops.map(shop => shop.items),
    tools: modeModel.tools.desired,
  }) === modelBefore);

const legacyNoOp = freshRom();
OB64.nativeRebuild.patchSourceCheck(legacyNoOp);
check('Patch ROM no-op source stays byte-identical',
  equalBytes(legacyNoOp.z64, retailZ64));

if (failures) {
  console.error('\n' + failures + ' native dual-build test(s) failed.');
  process.exit(1);
}
console.log('\nAll native dual-build tests passed.');
