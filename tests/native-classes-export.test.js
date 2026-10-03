'use strict';

// Execute the actual export click handler with real ROM/build/validation modules.
// Only presentation, progress painting, and the download sink are replaced.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');
const { EDITOR, PARENT, MASTER_NAME } = require('./project-paths.cjs');
global.window = global;
global.module = undefined;
global.localStorage = { getItem() { return null; }, setItem() {}, removeItem() {} };
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const name of [
  'data.js', 'rom-names-data.js', 'portraits.js', 'parsers.js', 'repack.js',
  'description-codec.js', 'source-redirect.js', 'stat-gate-relocation.js',
  'combat-animation-overrides-data.js', 'combat-animation-overrides.js',
  'consumable-effects.js', 'patch.js', 'tools-data.js', 'tools.js',
  'squadblob.js', 'runtimeblob.js', 'native-rebuild-data.js', 'native-rebuild.js',
  'native-classes-data.js', 'native-classes.js', 'class-families.js', 'native-neutral-data.js', 'native-neutral.js', 'squads-data.js', 'squads.js',
  'scenario-map-calibration.js', 'scenario-eset-data.js', 'scenario-treasure-data.js',
  'scenario-eset-codec.js', 'scenario.js', 'rom-export-candidate.js', 'rom-export-validator.js'
]) vm.runInThisContext(fs.readFileSync(path.join(EDITOR, name), 'utf8'), { filename: name });

const master = new Uint8Array(fs.readFileSync(path.join(PARENT, MASTER_NAME)));
const app = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const start = app.indexOf("  btnExport.addEventListener('click', async function() {");
assert(start >= 0);
const end = app.indexOf('\n  });', start) + '\n  });'.length;
assert(end > start);
const handlerSource = app.slice(start, end);
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
const equal = (a, b) => assert.deepStrictEqual(Buffer.from(a), Buffer.from(b));

function fresh(bytes = master) {
  const rom = OB64.loadROM(bytes.slice().buffer);
  OB64.nativeRebuild.initializeSource(rom);
  OB64.consumableEffects.initializeSession(rom,
    OB64.consumableEffects.evaluateSourceIdentity({ filename: MASTER_NAME,
      size: rom.z64.length, sha256: sha(rom.z64), byteOrder: rom.byteOrder,
      imageName: 'OgreBattle64', gameId: 'NOBE', country: rom.z64[0x3E],
      version: rom.z64[0x3F], crc1: OB64.readU32BE(rom.z64, 0x10),
      crc2: OB64.readU32BE(rom.z64, 0x14) }), { filename: MASTER_NAME });
  OB64.patch.snapshotOriginal(rom);
  OB64.tools.initState(rom);
  OB64.nativeRebuild.initializeToolState(rom);
  OB64.combatAnimationOverrides.initialize(rom);
  OB64.scenario.ensureState(rom);
  rom.squadOverrides = {};
  OB64.buildModes.set(rom, 'rebuild');
  // Compatibility is evaluated by the finished-ROM validator below.
  rom.compatibility = { canEdit: true, canExport: true };
  return rom;
}

function entropy(rom) {
  let seed = 0x12345678;
  for (const row of Object.values(rom.statGates.byClass)) {
    for (const key of ['str', 'vit', 'int', 'men', 'agi', 'dex', 'alnMin', 'alnMax']) {
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      row[key] = seed >>> 24;
    }
  }
}

async function runExport(rom, dirty) {
  let handler, downloaded, validation, validationOptions;
  const noop = () => {};
  const element = () => ({ setAttribute: noop, disabled: false });
  const statusBar = {};
  const context = { OB64, console, rom, dirty, romLoadGeneration: 1,
    changes: 1, activeTab: 'classes', statusBar, content: element(),
    currentBuildMode: () => 'rebuild', currentBuildModeInfo: () => ({ label: 'Rebuild ROM' }),
    btnExport: { addEventListener(event, fn) { handler = fn; } },
    fileInput: element(), patchFileInput: element(), btnSavePatch: element(),
    btnChangelog: element(), btnLoadPatchLabel: element(),
    showRomExportProgressModal: () => ({ update: noop, close: noop }),
    paintRomExportProgress: async () => {}, renderTab: noop,
    setRomMutationControlsEnabled: noop, setRomEditingContentAvailable: noop,
    showErrorModal(title, message) { throw new Error(title + ': ' + message); },
    showRomValidationModal(report) { validation = report; },
    showRomCompatibilityModal() { throw new Error('Unexpected compatibility block'); }
  };
  vm.runInNewContext(handlerSource, context, { filename: 'app-export-handler.js' });
  const originalDownload = OB64.exportROM;
  const originalValidate = OB64.romExportValidator.validateAsync;
  OB64.exportROM = candidate => { downloaded = candidate; return 'test.v64'; };
  OB64.romExportValidator.validateAsync = async options => {
    validationOptions = options;
    validation = OB64.romExportValidator.validate(options);
    return validation;
  };
  const before = sha(rom.z64);
  try { await handler(); } finally {
    OB64.exportROM = originalDownload;
    OB64.romExportValidator.validateAsync = originalValidate;
  }
  assert(downloaded, statusBar.textContent + '\n' + JSON.stringify(validation && validation.errors));
  assert(validation.ok, JSON.stringify(validation.errors));
  assert.strictEqual(sha(rom.z64), before, 'export mutated loaded ROM');
  assert(OB64.consumableEffects.verifyIndependentCrc(downloaded.z64).ok);
  return { candidate: downloaded, report: validation, options: validationOptions };
}

(async () => {
  const noEdit = fresh();
  const untouched = await runExport(noEdit, {});
  equal(untouched.candidate.z64, noEdit.z64);
  console.log('PASS actual no-edit Rebuild export is byte-identical');

  const edited = fresh();
  edited.classDefs[3].baseHp += 17;
  edited.classDefs[3].itemCapacity = 7;
  edited.classDescriptions = OB64.descriptionCodec.withText(edited.classDescriptions, 3, 'A rebuilt class.');
  edited.actionDescriptions = OB64.descriptionCodec.withText(edited.actionDescriptions, 3, 'Type: Physical.\nA rebuilt action.');
  entropy(edited);
  const dirty = { classDefs: true, classDescriptions: true, actionDescriptions: true, statGates: true };
  // The handler must never call any legacy Classes producer in Rebuild mode.
  const saved = {};
  for (const key of ['serializeClassDefs', 'serializeClassDescriptions', 'serializeActionDescriptions', 'serializeStatGates']) {
    saved[key] = OB64[key]; OB64[key] = () => { throw new Error('Legacy producer invoked: ' + key); };
  }
  let rebuilt;
  try { rebuilt = await runExport(edited, dirty); } finally { Object.assign(OB64, saved); }
  const reopened = fresh(rebuilt.candidate.z64);
  assert.strictEqual(reopened.classDefs[3].baseHp, edited.classDefs[3].baseHp);
  assert.strictEqual(reopened.classDefs[3].itemCapacity, 7);
  assert.strictEqual(reopened.classDescriptions.records[3].editableText, 'A rebuilt class.');
  equal(reopened.statGates.raw, rebuilt.options.classPlan.promotion.decoded);
  assert.strictEqual(OB64.sourceRedirect.classify(rebuilt.candidate.z64).entries.length, 0);
  console.log('PASS actual Classes export, checksum, owner ledger, and reopen without legacy producers');

  const second = await runExport(reopened, {});
  equal(second.candidate.z64, rebuilt.candidate.z64);
  console.log('PASS no-edit export of reopened native Classes ROM preserves every byte');

  const missing = OB64.romExportValidator.validate(Object.assign({}, rebuilt.options, { classPlan: null }));
  assert(!missing.ok && missing.errors.some(e => e.code === 'CLASSES_REBUILD_INTEGRITY'));
  console.log('PASS finished-ROM validation rejects missing Classes ownership');

  const legacy = fresh();
  entropy(legacy);
  const legacyPlan = OB64.statGateRelocation.prepare(legacy.statGates, legacy.z64, legacy.layout);
  OB64.statGateRelocation.apply(legacyPlan, legacy.z64);
  OB64.sourceRedirect.apply(legacy.z64, OB64.sourceRedirect.prepare(legacy.z64,
    legacyPlan.redirectRequests, { knownCurrentRequests: [] }));
  OB64.recalcN64CRC(legacy.z64);
  const imported = fresh(legacy.z64);
  const migrated = await runExport(imported, {});
  equal(OB64.loadROM(migrated.candidate.z64.slice().buffer).statGates.raw, legacyPlan.decoded);
  assert.strictEqual(OB64.sourceRedirect.classify(migrated.candidate.z64).entries.length, 0);
  assert.strictEqual(OB64.nativeClasses.promotionLoaderState(migrated.candidate.z64), 'relocated');
  console.log('PASS legacy promotion overflow migrates to native loaders and removes its redirect');

  const combined = fresh(legacy.z64);
  OB64.tools.setDesiredPercent(combined, 'experience-size-scale', 200);
  for (const key of ['cf-army-counter', 'character-card-luck', 'squad-menu-alignment']) combined.tools.desired[key] = true;
  const rate = combined.neutralEncounters.scenarioRates.find(row => row.runtimeKey === 1 && Number.isInteger(row.slice));
  rate.rateOverride = { normal: { mode: 'override', passCount: 720, divisor: 72000 }, alternate: { mode: 'disabled' } };
  const state = OB64.scenario.ensureState(combined), model = state.models[1];
  const meta = state.metadata[1], slot = combined.archives[meta.archive];
  let grew = false;
  for (let i = 0; model.section3.length < 16; i++) {
    OB64.scenario._modelTest.allocExtra(model, 1, [i, 1, 2, 3, 4, 5, 6, 7]);
    const raw = OB64.scenarioCodec.serializeEset(model);
    if (OB64.buildLHAArchive(OB64.lh5Compress(raw), raw, meta.filename).length > slot.totalHeaderSize + slot.compSize) {
      grew = true; break;
    }
  }
  assert(grew, 'Scenario fixture must exceed its original slot');
  const composed = await runExport(combined, { statGates: true, scenario: true, tools: true, neutralRuntime: true });
  assert.strictEqual(composed.options.scenarioResult.relocations.length, 1);
  const entries = OB64.sourceRedirect.classify(composed.candidate.z64).entries;
  assert.strictEqual(entries.length, composed.options.scenarioResult.redirectRequests.length);
  assert(entries.length > 0, 'Scenario still needs its redirect');
  assert.strictEqual(OB64.nativeClasses.promotionLoaderState(composed.candidate.z64), 'relocated');
  equal(OB64.loadROM(composed.candidate.z64.slice().buffer).statGates.raw, legacyPlan.decoded);
  console.log('PASS native promotion, Scenario overflow, Neutral, and native Tools coexist in the actual export');
})().catch(error => { console.error(error); process.exitCode = 1; });
