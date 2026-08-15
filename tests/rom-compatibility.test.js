'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const REV0 = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const REV1 = path.join(ROOT, 'Rev 1.1',
  'Ogre Battle 64 - Person of Lordly Caliber (USA) (Rev 1).z64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
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
  'art.js',
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
  'squads-data.js',
  'squads.js',
  'scenario-map-calibration.js',
  'scenario-eset-data.js',
  'scenario-treasure-data.js',
  'scenario-eset-codec.js',
  'scenario.js',
  'rom-export-validator.js',
  'rom-compatibility.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function component(report, id) {
  return report.components.find(row => row.id === id);
}

async function initializeReadableFeatures(rom, sourceIdentity, filename) {
  const run = (spec, action) => OB64.romCompatibility.runInitializer(rom, spec, action);
  await run({ id: 'native-art', label: 'Avatars and item icons', affectsTabs: ['art'] },
    () => OB64.art.initialize(rom));
  await run({
    id: 'consumable-effects', label: 'Consumable effect code and text',
    affectsTabs: ['consumables'], requiredForExport: true,
  }, () => OB64.consumableEffects.initializeSession(rom, sourceIdentity, { filename }));
  await run({
    id: 'runtime-shop-readback', label: 'Runtime shop override readback',
    affectsTabs: ['shops'],
  }, () => OB64.runtimeOverrides.applyParsedShopOverrides(rom));
  await run({
    id: 'combat-animation-overrides', label: 'Combat animation override lane',
    affectsTabs: ['classes'],
  }, () => OB64.combatAnimationOverrides.initialize(rom));
  await run({
    id: 'project-baseline', label: 'Project and export baseline', requiredForExport: true,
  }, () => OB64.patch.snapshotOriginal(rom));
  await run({ id: 'tools-patches', label: 'Tools patch regions', affectsTabs: ['tools'] },
    () => OB64.tools.initState(rom));
  rom.squadOverrides = {};
  await run({ id: 'scenario-models', label: 'Scenario authoring models', affectsTabs: ['scenario'] },
    () => OB64.scenario.ensureState(rom));
}

async function loadAndAssess(bytes, filename) {
  const source = bytes.slice();
  const sourceIdentity = await OB64.consumableEffects.inspectSourceIdentity(source, filename);
  const rom = OB64.loadROM(source.buffer);
  await OB64.romCompatibility.identify(rom, sourceIdentity);
  if (rom.compatibility.canEdit) {
    await initializeReadableFeatures(rom, sourceIdentity, filename);
  }
  OB64.romCompatibility.assessFeatures(rom);
  return rom;
}

async function run() {
  const rev0Bytes = new Uint8Array(fs.readFileSync(REV0));
  const retail0 = await loadAndAssess(rev0Bytes, path.basename(REV0));
  check('exact Rev0 is identified',
    retail0.compatibility.classification === 'us-rev0-vanilla');
  check('exact Rev0 remains fully editable and exportable',
    retail0.compatibility.canEdit && retail0.compatibility.canExport);
  check('exact Rev0 has no compatibility warning',
    retail0.compatibility.overall === 'verified', retail0.compatibility.overall);
  check('exact Rev0 native art is readable',
    component(retail0.compatibility, 'native-art').status === 'readable');

  const harmless = rev0Bytes.slice();
  harmless[harmless.length - 2] ^= 1;
  const modified0 = await loadAndAssess(harmless, 'harmless-modified-rev0.v64');
  check('modified Rev0 is loaded instead of rejected',
    modified0.compatibility.classification === 'us-rev0-modified');
  check('harmless modified Rev0 keeps readable systems enabled',
    modified0.compatibility.canEdit && modified0.compatibility.canExport);
  check('harmless modified Rev0 reports its non-vanilla identity',
    modified0.compatibility.overall === 'warning');
  check('native art uses structural checks on a modified Rev0',
    modified0.art && modified0.art.supported && !modified0.art.exactRetail);

  const invalidChecksumBytes = retail0.z64.slice();
  invalidChecksumBytes[OB64.ITEM_STAT_OFFSET] ^= 1;
  const invalidChecksum = await loadAndAssess(invalidChecksumBytes, 'invalid-checksum.z64');
  check('invalid source checksum is reported as a compatibility conflict',
    component(invalidChecksum.compatibility, 'rom-checksum').status === 'conflict');
  check('invalid source checksum explains the runtime risk',
    /Hardware or emulators/.test(component(invalidChecksum.compatibility, 'rom-checksum').reason));
  check('invalid source checksum blocks export but not readable inspection',
    invalidChecksum.compatibility.canEdit && !invalidChecksum.compatibility.canExport);
  check('invalid source checksum is named by the export-safety block',
    component(invalidChecksum.compatibility, 'export-safety').status === 'blocked' &&
      /N64 boot checksum/.test(component(invalidChecksum.compatibility, 'export-safety').reason));

  const unknownBytes = retail0.z64.slice();
  unknownBytes[0x3F] = 2;
  const unknown = await loadAndAssess(unknownBytes, 'unknown-header-revision.z64');
  check('unknown header revision loads diagnostic-only',
    unknown.layout === null && unknown.compatibility.classification === 'unknown-layout');
  check('unknown layout disables fixed-offset editing and export',
    !unknown.compatibility.canEdit && !unknown.compatibility.canExport);
  check('unknown layout explains why parsers did not run',
    /fixed-offset parsers were not run/.test(component(unknown.compatibility, 'layout').reason));
  check('unassessed stat-gate ownership is not reported as readable',
    component(unknown.compatibility, 'stat-gate-writeback').status === 'blocked');

  const archiveConflictBytes = retail0.z64.slice();
  archiveConflictBytes[retail0.archives[647].offset + 4] ^= 1;
  const archiveConflict = await loadAndAssess(archiveConflictBytes, 'shifted-archive-catalog.z64');
  check('damaged archive catalog is reported without aborting the load',
    component(archiveConflict.compatibility, 'archive-catalog').status === 'blocked');
  check('independent fixed-offset item parser remains readable',
    component(archiveConflict.compatibility, 'items').status === 'readable');
  check('readable tabs remain editable after an unrelated parser failure',
    archiveConflict.compatibility.canEdit &&
      OB64.romCompatibility.tabState(archiveConflict.compatibility, 'items').status !== 'blocked');
  check('a required parser failure blocks complete ROM export',
    !archiveConflict.compatibility.canExport);
  check('scenario tab names the failed dependency',
    OB64.romCompatibility.tabState(archiveConflict.compatibility, 'scenario')
      .reasons.some(reason => /LHA archive catalog/.test(reason)));

  const artConflictBytes = retail0.z64.slice();
  artConflictBytes[OB64.art.constants.AVATAR_DESCRIPTOR_SITES[0].lui] ^= 4;
  const artConflict = await loadAndAssess(artConflictBytes, 'foreign-art-pointer.z64');
  check('foreign avatar pointer blocks only native art',
    component(artConflict.compatibility, 'native-art').status === 'blocked' &&
      OB64.romCompatibility.tabState(artConflict.compatibility, 'art').status === 'blocked');
  check('foreign avatar pointer reason identifies its guarded owner',
    /key owner|Native art is unavailable/.test(component(artConflict.compatibility, 'native-art').reason));
  check('other readable systems remain available after an art conflict',
    artConflict.compatibility.canEdit &&
      OB64.romCompatibility.tabState(artConflict.compatibility, 'items').status !== 'blocked');

  const toolsConflictBytes = retail0.z64.slice();
  const highAttackFeature = OB64.tools.getFeature('high-attack-streamsplit');
  toolsConflictBytes[highAttackFeature.writes[0].offset] ^= 1;
  const toolsConflict = await loadAndAssess(toolsConflictBytes, 'foreign-tools-region.z64');
  check('foreign Tools bytes are identified without rejecting the ROM',
    component(toolsConflict.compatibility, 'tools-patches').status === 'conflict' &&
      toolsConflict.compatibility.canEdit);
  check('foreign Tools conflict names the affected feature',
    /High Attack Battle Stream Fix/.test(
      component(toolsConflict.compatibility, 'tools-patches').reason));
  check('a feature-local Tools conflict warns its tab without blocking unrelated export',
    OB64.romCompatibility.tabState(toolsConflict.compatibility, 'tools').status === 'warning' &&
      toolsConflict.compatibility.canExport);

  const rev1Bytes = new Uint8Array(fs.readFileSync(REV1));
  const retail1 = await loadAndAssess(rev1Bytes, path.basename(REV1));
  check('exact Rev1 is identified',
    retail1.compatibility.classification === 'us-rev1-vanilla');
  check('exact Rev1 remains editable and exportable',
    retail1.compatibility.canEdit && retail1.compatibility.canExport);
  check('Rev1 reports known unavailable optional features',
    retail1.compatibility.overall === 'warning' &&
      component(retail1.compatibility, 'native-art').status === 'blocked');
  check('Rev1 class editor remains available with a feature warning',
    OB64.romCompatibility.tabState(retail1.compatibility, 'classes').status === 'warning');

  const line = OB64.romCompatibility.logLine(archiveConflict.compatibility);
  check('detailed log line includes identity and parser counts',
    /classification=us-rev0-modified/.test(line) && /blocked=/.test(line), line);

  if (failures) {
    console.error('\n' + failures + ' ROM compatibility test(s) failed.');
    process.exit(1);
  }
  console.log('\nAll ROM compatibility tests passed.');
}

run().catch(error => {
  console.error(error && error.stack || error);
  process.exit(1);
});
