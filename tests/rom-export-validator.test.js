'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const MASTER = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
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
  'squads-data.js',
  'squads.js',
  'scenario-map-calibration.js',
  'scenario-eset-data.js',
  'scenario-treasure-data.js',
  'scenario-eset-codec.js',
  'scenario.js',
  'rom-export-validator.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const masterBytes = new Uint8Array(fs.readFileSync(MASTER));
let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function equalBytes(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let index = 0; index < a.length; index++) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

function loadRom() {
  return OB64.loadROM(masterBytes.slice().buffer);
}

function cloneScenarioState(state) {
  const cloned = Object.assign({}, state);
  cloned.archiveOriginalSlots = Object.assign({}, state.archiveOriginalSlots || {});
  cloned.slotOwnedArchives = Object.assign({}, state.slotOwnedArchives || {});
  cloned.relocationOwnedWindows = (state.relocationOwnedWindows || []).map(window =>
    Object.assign({}, window));
  return cloned;
}

function candidateFor(source) {
  const candidate = Object.assign({}, source);
  candidate.z64 = source.z64.slice();
  if (source.scenarioEditor) {
    candidate.scenarioEditor = cloneScenarioState(source.scenarioEditor);
  }
  candidate.scenarioRelocations = (source.scenarioRelocations || []).map(entry =>
    Object.assign({}, entry));
  return candidate;
}

function applyScenarioRedirect(source, candidate, scenarioResult) {
  const currentRequests = OB64.sourceRedirect.scenarioRequests(
    source.scenarioRelocations || []
  );
  const plan = OB64.sourceRedirect.prepare(
    candidate.z64,
    scenarioResult.redirectRequests || [],
    { knownCurrentRequests: currentRequests }
  );
  const result = OB64.sourceRedirect.apply(candidate.z64, plan);
  if (result.crc) OB64.recalcN64CRC(candidate.z64);
  scenarioResult.crc = result.crc;
  return plan;
}

function statModelFromBytes(base, decoded) {
  const fields = ['str', 'vit', 'int', 'men', 'agi', 'dex', 'alnMin', 'alnMax'];
  const byClass = {};
  for (let classId = 0; classId < 81; classId++) {
    const offset = classId * 8;
    const row = { classId, offset };
    fields.forEach((field, index) => { row[field] = decoded[offset + index]; });
    byClass[classId] = row;
  }
  return { raw: decoded.slice(), byClass, meta: base.meta };
}

function maximumStatWitness() {
  const a = new Array(9 * 3 + 1).fill(0);
  const sequence = [];
  function db(t, p) {
    if (t > 3) {
      if (3 % p === 0) {
        for (let index = 1; index <= p; index++) sequence.push(a[index]);
      }
      return;
    }
    a[t] = a[t - p];
    db(t + 1, p);
    for (let value = a[t - p] + 1; value < 9; value++) {
      a[t] = value;
      db(t + 1, t);
    }
  }
  db(1, 1);
  return Uint8Array.from(sequence.slice(0, 648), value => value + 1);
}

function dirtyFlags(overrides) {
  return Object.assign({
    shops: false,
    items: false,
    classDefs: false,
    encounters: false,
    creatureDrops: false,
    consumables: false,
    consumableEffects: false,
    combatAnimationOverrides: false,
    statGates: false,
    tools: false,
    squadOverrides: false,
    scenario: false,
  }, overrides || {});
}

function reportCodes(report) {
  return report.errors.map(error => error.code);
}

function commonHeaderCrcOffset(z64, memberOffset) {
  const headerSize = OB64.readU16LE(z64, memberOffset);
  let cursor = memberOffset + 24;
  const end = memberOffset + headerSize;
  while (cursor + 2 <= end) {
    const size = OB64.readU16LE(z64, cursor);
    if (!size) return -1;
    if (size < 3 || cursor + size > end) return -1;
    if (z64[cursor + 2] === 0 && size >= 5) return cursor + 3;
    cursor += size;
  }
  return -1;
}

function rewriteHeaderCrc(z64, memberOffset) {
  const headerSize = OB64.readU16LE(z64, memberOffset);
  const crcOffset = commonHeaderCrcOffset(z64, memberOffset);
  if (crcOffset < 0) throw new Error('test archive has no common-header CRC');
  z64[crcOffset] = 0;
  z64[crcOffset + 1] = 0;
  const crc = OB64.crc16(z64.slice(memberOffset, memberOffset + headerSize));
  z64[crcOffset] = crc & 0xFF;
  z64[crcOffset + 1] = (crc >>> 8) & 0xFF;
}

function runNoOpValidation() {
  const source = loadRom();
  const candidate = candidateFor(source);
  const report = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirtyFlags(),
    touched: [],
    owners: [],
    shopOverrides: [],
  });
  check('no-op ROM passes the complete export validator', report.ok,
    JSON.stringify(report.errors));
  check('no-op audit reports zero changed ranges', report.changeRanges.length === 0,
    String(report.changeRanges.length));
}

function buildEditedScenarioCandidate() {
  const source = loadRom();
  const state = OB64.scenario.ensureState(source);
  const model = state.models[1];
  model.section1[0].bytes[3] = (model.section1[0].bytes[3] + 1) & 0xFF;
  OB64.scenarioCodec.refreshDecodedRows(model);

  const candidate = candidateFor(source);
  const scenarioResult = OB64.scenario.exportScenarioArchives(candidate, {
    deferRedirect: true,
  });
  if (scenarioResult.blocked.length) {
    throw new Error(scenarioResult.blocked.join('\n'));
  }
  const sourceRedirectPlan = applyScenarioRedirect(
    source,
    candidate,
    scenarioResult
  );
  const dirty = dirtyFlags({ scenario: true });
  const owners = OB64.consumableEffects.standardPatchOwners(source, dirty)
    .concat(OB64.consumableEffects.scenarioPatchOwners(candidate));
  if (sourceRedirectPlan.changed) owners.push(OB64.sourceRedirect.patchOwner());
  return { source, candidate, dirty, owners, scenarioResult, sourceRedirectPlan };
}

function buildRelocatedScenarioCandidate() {
  const source = loadRom();
  const state = OB64.scenario.ensureState(source);
  const model = state.models[1];
  const metadata = state.metadata[1];
  const archive = source.archives[metadata.archive];
  const slotSize = archive.totalHeaderSize + archive.compSize;
  let sequence = 0;
  while (model.section3.length < 16) {
    OB64.scenario._modelTest.allocExtra(
      model,
      1,
      [sequence & 0xFF, 1, 2, 3, 4, 5, 6, 7]
    );
    const raw = OB64.scenarioCodec.serializeEset(model);
    const rebuilt = OB64.buildLHAArchive(
      OB64.lh5Compress(raw),
      raw,
      metadata.filename || 'eset_key_1.bin'
    );
    if (rebuilt.length > slotSize) break;
    sequence++;
  }

  const candidate = candidateFor(source);
  const scenarioResult = OB64.scenario.exportScenarioArchives(candidate, {
    deferRedirect: true,
  });
  if (scenarioResult.blocked.length) {
    throw new Error(scenarioResult.blocked.join('\n'));
  }
  if (!scenarioResult.relocations.length) {
    throw new Error('test scenario did not grow enough to require relocation');
  }
  const sourceRedirectPlan = applyScenarioRedirect(
    source,
    candidate,
    scenarioResult
  );
  const dirty = dirtyFlags({ scenario: true });
  const owners = OB64.consumableEffects.standardPatchOwners(source, dirty)
    .concat(OB64.consumableEffects.scenarioPatchOwners(candidate));
  owners.push(OB64.sourceRedirect.patchOwner());
  return { source, candidate, dirty, owners, scenarioResult, sourceRedirectPlan };
}

function runEditedScenarioValidation() {
  const built = buildEditedScenarioCandidate();
  const report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: built.candidate,
    dirty: built.dirty,
    touched: built.scenarioResult.touched,
    owners: built.owners,
    scenarioResult: built.scenarioResult,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('edited scenario ROM passes complete validation', report.ok,
    JSON.stringify(report.errors));
  check('scenario export returns an extract-back validation target',
    built.scenarioResult.validationTargets.length === 1,
    String(built.scenarioResult.validationTargets.length));
  check('machine report does not embed the raw intended archive payload',
    JSON.stringify(report).indexOf('expectedRaw') === -1);
  const redirectCheck = report.checks.find(entry =>
    entry.id === 'scenario-relocation-integrity');
  check('scenario redirect integrity is checked after scenario export',
    redirectCheck && redirectCheck.status === 'passed',
    redirectCheck && redirectCheck.summary);
}

function runRelocationFailureClassification() {
  const built = buildRelocatedScenarioCandidate();
  let report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: built.candidate,
    dirty: built.dirty,
    touched: built.scenarioResult.touched,
    owners: built.owners,
    scenarioResult: built.scenarioResult,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('installed scenario redirect passes the complete export validator',
    report.ok, JSON.stringify(report.errors));

  const cave = OB64.sourceRedirect.constants.CAVE_ROM;
  built.candidate.z64[cave] ^= 1;
  OB64.recalcN64CRC(built.candidate.z64);
  report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: built.candidate,
    dirty: built.dirty,
    touched: built.scenarioResult.touched,
    owners: built.owners,
    scenarioResult: built.scenarioResult,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('damaged scenario redirect has a stable machine error code',
    reportCodes(report).includes('SCENARIO_RELOCATION_INTEGRITY'),
    reportCodes(report).join(','));
}

function runArchiveFailureClassification() {
  const built = buildEditedScenarioCandidate();
  const target = built.scenarioResult.validationTargets[0];
  const crcOffset = commonHeaderCrcOffset(built.candidate.z64, target.offset);

  const badHeader = built.candidate.z64.slice();
  badHeader[crcOffset] ^= 1;
  let code = '';
  try {
    OB64.romExportValidator._test.validateArchiveTarget(badHeader, target);
  } catch (error) {
    code = error.validationIssue && error.validationIssue.code;
  }
  check('bad archive header checksum has a stable machine error code',
    code === 'ARCHIVE_HEADER_CRC', code);

  const badBoundary = built.candidate.z64.slice();
  const declared = OB64.readU32LE(badBoundary, target.offset + 7) - 1;
  badBoundary[target.offset + 7] = declared & 0xFF;
  badBoundary[target.offset + 8] = (declared >>> 8) & 0xFF;
  badBoundary[target.offset + 9] = (declared >>> 16) & 0xFF;
  badBoundary[target.offset + 10] = (declared >>> 24) & 0xFF;
  rewriteHeaderCrc(badBoundary, target.offset);
  code = '';
  try {
    OB64.romExportValidator._test.validateArchiveTarget(badBoundary, target);
  } catch (error) {
    code = error.validationIssue && error.validationIssue.code;
  }
  check('wrong archive end has a stable machine error code',
    code === 'ARCHIVE_BOUNDARY', code);

  const wrongExpectation = Object.assign({}, target, {
    expectedRaw: target.expectedRaw.slice(),
  });
  wrongExpectation.expectedRaw[0] ^= 1;
  code = '';
  try {
    OB64.romExportValidator._test.validateArchiveTarget(
      built.candidate.z64,
      wrongExpectation
    );
  } catch (error) {
    code = error.validationIssue && error.validationIssue.code;
  }
  check('extract-back mismatch has a stable machine error code',
    code === 'ARCHIVE_EXTRACT_MISMATCH', code);
}

function runCombinedFailureReport() {
  const source = loadRom();
  source.itemStats[1].price = (source.itemStats[1].price + 1) & 0xFFFF;
  const candidate = candidateFor(source);
  candidate.z64[0x10] ^= 1; // Invalid CIC header checksum, but still an owned header range.
  candidate.z64[0x40] ^= 1; // Outside every export owner.
  const dirty = dirtyFlags({ items: true });
  const report = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirty,
    touched: ['items'],
    owners: OB64.consumableEffects.standardPatchOwners(source, dirty),
    shopOverrides: [],
  });
  const codes = reportCodes(report);
  check('failed export report blocks the ROM', !report.ok && report.result === 'failed');
  check('failed export report identifies invalid CIC checksum',
    codes.includes('ROM_CHECKSUM_INVALID'), codes.join(','));
  check('failed export report identifies an unplanned ROM write',
    codes.includes('UNPLANNED_ROM_WRITE'), codes.join(','));
  check('failed export report identifies semantic readback mismatch',
    codes.includes('SEMANTIC_READBACK_MISMATCH'), codes.join(','));
  check('machine errors include plain-language UI fields', report.errors.every(error =>
    error.title && error.message && error.suggestion), JSON.stringify(report.errors));
}

function runStoredArchiveHeaderTest() {
  const payload = new Uint8Array([1, 2, 3, 4, 5, 6]);
  const built = OB64.buildLHAArchiveUncompressed(payload, 'small.bin', 80);
  const member = OB64.romExportValidator._test.parseArchiveAt(built, 0);
  let passed = true;
  try {
    OB64.romExportValidator._test.validateCommonHeaderCrc(built, member);
  } catch (error) {
    passed = false;
  }
  check('stored -lh0- builder emits a valid common-header checksum', passed);
  check('stored -lh0- member retains the requested total header size',
    member.totalHeaderSize === 80, String(member.totalHeaderSize));
}

function runValidDirectTableMatrix() {
  const source = loadRom();
  OB64.patch.snapshotOriginal(source);
  source.itemStats[1].price = (source.itemStats[1].price + 1) & 0xFFFF;
  source.classDefs[2].frontAtks = (source.classDefs[2].frontAtks + 1) & 0xFF;
  source.neutralEncounters.records[0].slots[2].classA ^= 1;
  source.creatureDrops.records[0].classId ^= 1;
  source.consumables[1].price = (source.consumables[1].price + 1) & 0xFFFF;
  source.statGates.byClass[1].str = (source.statGates.byClass[1].str + 1) & 0xFF;

  const candidate = candidateFor(source);
  OB64.serializeItemStats(source.itemStats, candidate.z64);
  OB64.serializeClassDefs(source.classDefs, candidate.z64);
  OB64.serializeNeutralEncounters(source.neutralEncounters, candidate.z64);
  OB64.serializeCreatureDrops(source.creatureDrops, candidate.z64);
  OB64.serializeConsumables(source.consumables, candidate.z64);
  OB64.serializeStatGates(source.statGates, candidate.z64);
  OB64.recalcN64CRC(candidate.z64);

  const dirty = dirtyFlags({
    items: true,
    classDefs: true,
    encounters: true,
    creatureDrops: true,
    consumables: true,
    statGates: true,
  });
  const report = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirty,
    touched: ['items', 'classes', 'encounters', 'creature drops', 'consumables', 'stat gates'],
    owners: OB64.consumableEffects.standardPatchOwners(source, dirty),
    shopOverrides: [],
  });
  check('valid direct-table export matrix has no validation false positives',
    report.ok, JSON.stringify(report.errors));
  const semanticChecks = report.checks.filter(entry =>
    entry.id.indexOf('semantic-readback-') === 0);
  check('every changed direct table passes semantic readback',
    semanticChecks.length === 6 && semanticChecks.every(entry => entry.status === 'passed'),
    JSON.stringify(semanticChecks));
}

function buildMaximumStatCandidate() {
  const source = loadRom();
  const witness = maximumStatWitness();
  source.statGates = statModelFromBytes(source.statGates, witness);
  const candidate = candidateFor(source);
  const statGatePlan = OB64.statGateRelocation.prepare(
    source.statGates,
    candidate.z64,
    source.layout
  );
  OB64.statGateRelocation.apply(statGatePlan, candidate.z64);
  const sourceRedirectPlan = OB64.sourceRedirect.prepare(
    candidate.z64,
    statGatePlan.redirectRequests,
    { knownCurrentRequests: [] }
  );
  const redirectResult = OB64.sourceRedirect.apply(
    candidate.z64,
    sourceRedirectPlan
  );
  if (redirectResult.crc) OB64.recalcN64CRC(candidate.z64);
  const dirty = dirtyFlags({ statGates: true });
  const owners = OB64.consumableEffects.standardPatchOwners(source, dirty)
    .concat([OB64.sourceRedirect.patchOwner()]);
  return {
    source,
    candidate,
    dirty,
    owners,
    witness,
    statGatePlan,
    sourceRedirectPlan,
  };
}

function runMaximumStatValidation() {
  const built = buildMaximumStatCandidate();
  let report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: built.candidate,
    dirty: built.dirty,
    touched: ['stat gates relocated (659 logical bytes)'],
    owners: built.owners,
    statGatePlan: built.statGatePlan,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('maximum 659-byte stat container passes complete export validation',
    report.ok, JSON.stringify(report.errors));
  const statCheck = report.checks.find(entry =>
    entry.id === 'stat-gate-relocation-integrity');
  check('complete validator reports exact maximum container sizes',
    statCheck && statCheck.status === 'passed' &&
      statCheck.details.logicalStreamBytes === 659 &&
      statCheck.details.storedStreamBytes === 660 &&
      statCheck.details.payloadBytes === 664 &&
      statCheck.details.containerBytes === 668,
    statCheck && JSON.stringify(statCheck.details));
  const redirectCheck = report.checks.find(entry =>
    entry.id === 'source-redirect-integrity');
  check('complete validator checks all three maximum stat redirects',
    redirectCheck && redirectCheck.status === 'passed' &&
      redirectCheck.details.entryCount === 3,
    redirectCheck && JSON.stringify(redirectCheck.details));

  const badDescriptor = candidateFor(built.candidate);
  badDescriptor.z64[
    OB64.statGateRelocation.constants.DESCRIPTOR_OFFSET + 28
  ] ^= 1;
  report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: badDescriptor,
    dirty: built.dirty,
    touched: ['damaged stat descriptor fixture'],
    owners: built.owners,
    statGatePlan: built.statGatePlan,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('damaged OBSG descriptor has a stable validator error code',
    reportCodes(report).includes('STAT_GATE_RELOCATION_INTEGRITY'),
    reportCodes(report).join(','));

  const badRedirect = candidateFor(built.candidate);
  badRedirect.z64[OB64.sourceRedirect.constants.CAVE_ROM] ^= 1;
  OB64.recalcN64CRC(badRedirect.z64);
  report = OB64.romExportValidator.validate({
    sourceRom: built.source,
    candidateRom: badRedirect,
    dirty: built.dirty,
    touched: ['damaged shared redirect fixture'],
    owners: built.owners,
    statGatePlan: built.statGatePlan,
    sourceRedirectPlan: built.sourceRedirectPlan,
    shopOverrides: [],
  });
  check('damaged shared redirect has a stable validator error code',
    reportCodes(report).includes('SOURCE_REDIRECT_INTEGRITY'),
    reportCodes(report).join(','));

  const clean = built.source.z64.slice();
  let conflictMessage = '';
  try {
    OB64.sourceRedirect.prepare(clean, [
      {
        owner: 'stat-gates',
        sourceRomOffset: 0x200000,
        destinationRomOffset: 0x210000,
      },
      {
        owner: 'scenario',
        sourceRomOffset: 0x200000,
        destinationRomOffset: 0x220000,
      },
    ], { knownCurrentRequests: [] });
  } catch (error) {
    conflictMessage = error.message;
  }
  check('conflicting redirect ownership is rejected before candidate mutation',
    conflictMessage.includes('two features requested different destinations') &&
      equalBytes(clean, built.source.z64), conflictMessage);
}

function runValidRuntimeShopExport() {
  const source = loadRom();
  OB64.patch.snapshotOriginal(source);
  const shopIndex = source.shops.findIndex(shop => shop.items && shop.items.length > 1);
  if (shopIndex < 0) throw new Error('test ROM has no populated shop');
  source.shops[shopIndex].items = source.shops[shopIndex].items.slice().reverse();
  OB64.runtimeOverrides.refreshShopOverrideState(source, shopIndex);
  const shopOverrides = OB64.runtimeOverrides.collectShopOverrides(source);
  const candidate = candidateFor(source);
  const runtimeWritePlan = OB64.runtimeOverrides.buildRuntimeOverrideWrites(
    [],
    shopOverrides,
    source.shops.length,
    candidate
  );
  runtimeWritePlan.writes.forEach(write =>
    candidate.z64.set(write.bytes, write.offset));
  if (runtimeWritePlan.crcWindow) OB64.recalcN64CRC(candidate.z64);
  const dirty = dirtyFlags({ shops: true });
  const runtimeOwner = {
    id: 'runtime-overrides',
    name: 'Shared Runtime Overrides',
    regions: OB64.runtimeOverrides.patchRegions(source),
  };
  const report = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirty,
    touched: ['runtime shops (1)'],
    owners: OB64.consumableEffects.standardPatchOwners(source, dirty)
      .concat([runtimeOwner]),
    runtimeWritePlan: runtimeWritePlan,
    runtimeMode: 'write',
    shopOverrides: shopOverrides,
  });
  check('valid runtime-shop export has no validation false positives',
    report.ok, JSON.stringify(report.errors));
}

function runValidCombatOverrideExport() {
  const source = loadRom();
  OB64.combatAnimationOverrides.initialize(source);
  OB64.combatAnimationOverrides.setEntry(source.combatAnimationOverrides, {
    classId: 1,
    actionId: 1,
    normalSelector: 40,
    blockedSelector: 41,
  });
  const selectorPlan = OB64.combatAnimationOverrides.prepareExport(source);
  const candidate = candidateFor(source);
  const selectorResult = OB64.combatAnimationOverrides.applyPlan(candidate, selectorPlan);
  if (selectorResult.crc) OB64.recalcN64CRC(candidate.z64);
  const dirty = dirtyFlags({ combatAnimationOverrides: true });
  const selectorOwner = OB64.combatAnimationOverrides.collisionOwner(source);
  const report = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirty,
    touched: ['attack animation overrides (1)'],
    owners: OB64.consumableEffects.standardPatchOwners(source, dirty)
      .concat(selectorOwner ? [selectorOwner] : []),
    selectorPlan: selectorPlan,
    shopOverrides: [],
  });
  check('valid combat-animation export has no validation false positives',
    report.ok, JSON.stringify(report.errors));
}

async function runArchiveMethodTransitionRegression() {
  const source = loadRom();
  const candidate = candidateFor(source);
  const archiveIndex = 694;
  const archive = source.archives[archiveIndex];
  const slotSize = archive.totalHeaderSize + archive.compSize;
  const raw = OB64.extractArchive(source.z64, archive);
  const rebuilt = OB64.buildLHAArchiveUncompressed(
    raw,
    'scincsv-regression.bin',
    slotSize - raw.length
  );
  if (rebuilt.length !== slotSize) {
    throw new Error('scincsv regression archive did not retain its fixed slot');
  }
  candidate.z64.set(rebuilt, archive.offset);
  const target = {
    kind: 'fixed',
    archive: archiveIndex,
    label: 'town allegiance (scincsv 694)',
    contentKind: 'scincsv',
    offset: archive.offset,
    slotSize: slotSize,
    slotEnd: archive.offset + slotSize,
    expectedRaw: raw,
  };
  const scenarioResult = {
    validationTargets: [target],
    relocations: [],
    crc: false,
  };
  const owner = {
    id: 'scincsv-regression',
    name: 'scincsv regression rebuild',
    regions: [{ kind: 'rom', start: archive.offset, size: slotSize }],
  };
  const progress = [];
  const report = await OB64.romExportValidator.validateAsync({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirtyFlags({ scenario: true }),
    touched: ['town allegiance (scincsv 694)'],
    owners: [owner],
    scenarioResult: scenarioResult,
    shopOverrides: [],
  }, event => progress.push(event));
  check('authorized rebuilt archive may change from -lh5- to -lh0-',
    report.ok, JSON.stringify(report.errors));
  const catalog = report.checks.find(entry => entry.id === 'archive-catalog');
  check('catalog report records the authorized compression change',
    catalog && catalog.details.authorizedMethodChanges.length === 1,
    catalog && JSON.stringify(catalog.details));
  check('asynchronous validation reports every active test stage',
    progress.filter(event => !event.complete).length === report.checks.length &&
      progress.some(event => event.name === 'ROM archive boundaries'),
    JSON.stringify(progress));
  check('asynchronous validation reports completion',
    progress.length && progress[progress.length - 1].complete === true);

  const untrusted = OB64.romExportValidator.validate({
    sourceRom: source,
    candidateRom: candidate,
    dirty: dirtyFlags({ scenario: true }),
    touched: ['unclaimed method transition'],
    owners: [owner],
    shopOverrides: [],
  });
  check('compression changes outside rebuilt targets still fail',
    reportCodes(untrusted).includes('ARCHIVE_CATALOG_CHANGED'),
    reportCodes(untrusted).join(','));
}

(async function main() {
  runNoOpValidation();
  runEditedScenarioValidation();
  runRelocationFailureClassification();
  runArchiveFailureClassification();
  runCombinedFailureReport();
  runStoredArchiveHeaderTest();
  runValidDirectTableMatrix();
  runMaximumStatValidation();
  runValidRuntimeShopExport();
  runValidCombatOverrideExport();
  await runArchiveMethodTransitionRegression();

  if (failures) {
    console.error('\n' + failures + ' ROM export validator test(s) failed.');
    process.exit(1);
  }
  console.log('\nAll ROM export validator tests passed.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exit(1);
});
