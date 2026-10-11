'use strict';

// Rebuild ROM writes custom squads as real enemydat records: a grown library in
// the native resource arena, one repointed link word, and ESET rows that name
// the new records directly. The squad override hook, bootstrap, and tail stay retail.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const nativeHarness = fs.readFileSync(path.join(__dirname, 'native-rebuild-runtime-composition.test.js'), 'utf8')
  .split('assert.strictEqual(data.composition.phase4Matrix.stateCount')[0];
const { freshRom } = new Function('require', '__dirname', nativeHarness + '\nreturn { freshRom };')(require, __dirname);
for (const file of ['rom-export-candidate.js', 'rom-export-validator.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), { filename: file });
}

const S = OB64.scenario;
const LINK_WORD = 0x027429D0;
const KEY_BASE = 0x00594280;
const RETAIL_RECORDS = 556;

function readU32(bytes, offset) {
  return ((bytes[offset] << 24) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
}

function decodeMember(z64, offset) {
  const headerSize = z64[offset] | (z64[offset + 1] << 8);
  const compSize = (z64[offset + 7] | (z64[offset + 8] << 8) | (z64[offset + 9] << 16) | (z64[offset + 10] << 24)) >>> 0;
  const uncompSize = (z64[offset + 11] | (z64[offset + 12] << 8) | (z64[offset + 13] << 16) | (z64[offset + 14] << 24)) >>> 0;
  return OB64.lh5Decompress(z64.subarray(offset + headerSize, offset + headerSize + compSize), uncompSize);
}

function equal(a, b) {
  return a.length === b.length && a.every((value, index) => value === b[index]);
}

function editLevel(rom, key, rowIndex) {
  const info = S.squadLevelInfo(rom, key, rowIndex);
  if (!info || info.excluded || info.added || !info.selectors.A.materialized) return false;
  const next = info.selectors.A.effectiveLevel + 1;
  if (next < 1 || next > 99) return false;
  try {
    S.setSquadLevelOffsetRaw(rom, key, rowIndex, 'A', (info.selectors.A.raw + 1) & 0xFF);
    return true;
  } catch (error) {
    return false;
  }
}

function firstEditable(rom, keys, predicate) {
  const state = S.ensureState(rom);
  for (const key of keys) {
    const model = state.models[key];
    if (!model) continue;
    for (let row = 0; row < model.section1.length; row++) {
      const info = S.squadLevelInfo(rom, key, row);
      if (info && (!predicate || predicate(info)) && editLevel(rom, key, row)) return { key, row, info };
    }
  }
  return null;
}

function exportCandidate(rom, plan) {
  const candidate = OB64.romExportCandidate.create(rom);
  const result = S.exportScenarioArchives(candidate, { deferRedirect: true, nativeSquads: plan });
  assert.deepStrictEqual(result.blocked, []);
  return { candidate, result };
}

function exportedRow(candidate, result, key, row) {
  const target = result.validationTargets.find(entry => entry.runtimeKey === key);
  assert(target, 'the edited scenario resource was written');
  const eset = OB64.scenarioCodec.parseEset(decodeMember(candidate.z64, target.offset), {});
  return eset.section1[row].bytes;
}

// No edits: nothing is planned, so Rebuild output stays exact retail.
{
  const plan = S.planNativeSquadLibrary(freshRom());
  assert.strictEqual(plan.records.length, 0);
  assert.strictEqual(plan.remappedRows, 0);
  assert.deepStrictEqual(plan.blocked, []);
}

// A custom squad and a flagged retail deployment.
{
  const rom = freshRom();
  const retail = rom.z64.slice();
  const shared = new Set([10, 22, 35, 62, 52, 53, 54]);
  const keys = Object.keys(S.ensureState(rom).models).map(Number).filter(key => !shared.has(key));
  const plain = firstEditable(rom, keys.filter(key => ![4, 21, 27, 42].includes(key)));
  assert(plain, 'an ordinary squad accepts a level edit');
  const flagged = firstEditable(rom, [21], info => info.originalEdatId === 469);
  assert(flagged, 'Scenario key 21 row with retail EDAT 470 accepts a level edit');

  const plan = S.planNativeSquadLibrary(rom);
  assert.deepStrictEqual(plan.blocked, []);
  assert.strictEqual(plan.records.length, 2);
  const { candidate, result } = exportCandidate(rom, plan);
  const library = result.nativeSquadLibrary;
  assert(library && library.customCount === 2);

  assert.strictEqual(readU32(candidate.z64, LINK_WORD), library.key);
  const entry = library.key + KEY_BASE;
  assert(entry >= 0x0275E000 && entry < 0x02780000 && entry % 16 === 0);
  const stored = readU32(candidate.z64, entry);
  assert.strictEqual(candidate.z64[entry + 4 + stored - 1], 0, 'the archive keeps its terminator');
  const payload = decodeMember(candidate.z64, entry + 4);
  const retailLibrary = OB64.extractArchive(retail, rom.archives[647]);
  assert.strictEqual(payload.length, (RETAIL_RECORDS + 2) * 35);
  assert(equal(payload.subarray(0, retailLibrary.length), retailLibrary), 'retail records are unchanged');
  plan.records.forEach((record, index) => {
    const offset = (RETAIL_RECORDS + index) * 35;
    assert(equal(payload.subarray(offset, offset + 35), record));
  });

  const plainRow = exportedRow(candidate, result, plain.key, plain.row);
  const plainId = ((plainRow[1] << 8) | plainRow[2]) - 1;
  assert(plainId >= RETAIL_RECORDS, 'the edited row names an appended record');
  const plainInfo = S.squadLevelInfo(rom, plain.key, plain.row);
  const plainRecord = payload.subarray(plainId * 35, plainId * 35 + 35);
  assert.strictEqual(plainRecord[1], plainInfo.selectors.A.raw, 'the appended record carries the edited level');

  const flaggedRow = exportedRow(candidate, result, flagged.key, flagged.row);
  assert.strictEqual(flaggedRow[0], flagged.info.sourceId | 0x80, 'the retail deployment flag is preset');
  assert(((flaggedRow[1] << 8) | flaggedRow[2]) - 1 >= RETAIL_RECORDS);

  for (const [start, size, label] of [
    [0x00195584, 8, 'squad record-builder hook'],
    [0x000283C4, 0x6C, 'shared runtime bootstrap'],
    [0x02780000, 0x10000, 'shared runtime tail'],
  ]) {
    assert(equal(candidate.z64.subarray(start, start + size), retail.subarray(start, start + size)),
      label + ' stays retail');
  }
  assert(equal(rom.z64, retail), 'the loaded ROM is not modified');

  const owners = OB64.consumableEffects.standardPatchOwners(candidate, { scenario: true });
  const scenarioOwner = owners.find(owner => owner.id === 'scenario-archives');
  for (const region of library.regions) {
    assert(scenarioOwner.regions.some(owned => owned.start === region.start && owned.size === region.size),
      region.label + ' is owned by the Scenario archive composer');
  }
}

// The library composes with the authenticated Rebuild owners.
{
  const rom = freshRom();
  const keys = Object.keys(S.ensureState(rom).models).map(Number).filter(key => key !== 10 && key < 52);
  assert(firstEditable(rom, keys));
  const plan = S.planNativeSquadLibrary(rom);
  OB64.buildModes.set(rom, 'rebuild');
  rom.tools.desired['character-card-luck'] = true;
  const nativePlan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: false });
  const candidate = OB64.romExportCandidate.create(rom);
  candidate.z64 = nativePlan.baseZ64.slice();
  const result = S.exportScenarioArchives(candidate, { deferRedirect: true, nativeSquads: plan });
  assert.deepStrictEqual(result.blocked, []);
  assert.deepStrictEqual(result.redirectRequests, [], 'the edited ESET still fits its retail slot');
  const owners = nativePlan.owners.concat(OB64.consumableEffects.standardPatchOwners(candidate, { scenario: true }));
  assert.deepStrictEqual(OB64.consumableEffects.findRegionConflicts(owners), []);
  nativePlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(owners);
  const nativeResult = OB64.nativeRebuild.applyPlan(nativePlan, candidate, null);
  if (nativeResult.crc) OB64.recalcN64CRC(candidate.z64);
  OB64.nativeRebuild.validateApplied(candidate, nativePlan, null);
  assert(OB64.consumableEffects.verifyIndependentCrc(candidate.z64).ok);
  assert.strictEqual(readU32(candidate.z64, LINK_WORD), result.nativeSquadLibrary.key);
  const changes = OB64.consumableEffects.buildChangeRanges(rom.z64, candidate.z64,
    owners.concat(OB64.consumableEffects.standardPatchOwners(candidate, { tools: true })));
  const entry = result.nativeSquadLibrary.key + KEY_BASE;
  for (const address of [entry, LINK_WORD]) {
    const range = changes.find(candidateRange => candidateRange.start < address + 4 && address < candidateRange.end);
    assert(range && /^scenario[-.]arch/.test(range.ownerId),
      'z64 0x' + address.toString(16) + ' is claimed by the Scenario archive composer');
  }
}

// The finished-ROM validator re-parses every rebuilt ESET. It accepts rows that
// name appended records and the preset retail flag, as the Rebuild button needs.
{
  const rom = freshRom();
  const plain = firstEditable(rom, [1]);
  assert(plain, 'Scenario key 1 accepts a level edit');
  const flagged = firstEditable(rom, [21], info => info.originalEdatId === 469);
  assert(flagged, 'Scenario key 21 row with retail EDAT 470 accepts a level edit');
  const plan = S.planNativeSquadLibrary(rom);
  assert.deepStrictEqual(plan.blocked, []);
  OB64.buildModes.set(rom, 'rebuild');
  const nativePlan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: false });
  const candidate = OB64.romExportCandidate.create(rom);
  candidate.z64 = nativePlan.baseZ64.slice();
  const result = S.exportScenarioArchives(candidate, { deferRedirect: true, nativeSquads: plan });
  assert.deepStrictEqual(result.blocked, []);
  const owners = nativePlan.owners.concat(OB64.consumableEffects.standardPatchOwners(candidate, { scenario: true }));
  nativePlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(owners);
  const nativeResult = OB64.nativeRebuild.applyPlan(nativePlan, candidate, null);
  OB64.recalcN64CRC(candidate.z64);
  const flaggedTarget = result.validationTargets.find(target => target.runtimeKey === 21);
  assert.deepStrictEqual(flaggedTarget.esetValidation, {
    limits: { enemydatRecords: RETAIL_RECORDS + 2 },
    flaggedSourceRows: [flagged.row],
  });
  const report = OB64.romExportValidator.validate({
    buildMode: 'rebuild',
    buildModeLabel: 'Rebuild ROM',
    sourceRom: rom,
    candidateRom: candidate,
    dirty: { scenario: true },
    touched: result.touched,
    owners,
    scenarioResult: result,
    nativePlan,
    nativeResult,
  });
  assert(report.ok, JSON.stringify(report.errors));

  // Without the target's allowance, the same bytes fail both codec limits.
  const model = OB64.scenarioCodec.parseEset(decodeMember(candidate.z64, flaggedTarget.offset), {});
  const codes = OB64.scenarioCodec.validateEset(model).errors.map(error => error.code);
  assert(codes.includes('section1-edat-range') && codes.includes('source-id-cap'));
}

// The flag marks an allied squad deployment, so a template swap on a flagged
// row keeps it: the swapped squad still fights on the player's side.
{
  const rom = freshRom();
  const model = S.ensureState(rom).models[21];
  const row = model.section1.findIndex(candidate => ((candidate.bytes[1] << 8) | candidate.bytes[2]) === 470);
  assert(row >= 0);
  model.section1[row].bytes[1] = 0x01;
  model.section1[row].bytes[2] = 0x2C;
  OB64.scenarioCodec.refreshDecodedRows(model);
  const plan = S.planNativeSquadLibrary(rom);
  assert.deepStrictEqual(plan.blocked, []);
  const remap = Object.values(plan.rowRemap).map(rows => rows[row]).filter(Boolean);
  assert.deepStrictEqual(remap, [{ edatId: 299, setFlag: true }], 'the swapped row keeps the allied flag');
}

// Squads-tab overrides are keyed by scenario and retail template without a row
// repoint. A used template becomes a real record; an unused one blocks export.
{
  const rom = freshRom();
  const stock = id => Uint8Array.from(Buffer.from(OB64.SCENARIO_ESET_DATA.enemydat.records[id], 'hex'));
  const used = stock(298);
  used[0] = 21;
  rom.squadOverrides = { '1:298': used };
  let plan = S.planNativeSquadLibrary(rom);
  assert.deepStrictEqual(plan.blocked, []);
  assert.strictEqual(plan.records.length, 1);
  assert.deepStrictEqual(plan.rowRemap[Object.keys(plan.rowRemap)[0]][1], { edatId: RETAIL_RECORDS, setFlag: false });

  const unused = stock(0);
  unused[0] = 21;
  rom.squadOverrides['1:0'] = unused;
  plan = S.planNativeSquadLibrary(rom);
  assert(plan.blocked.some(message => /Scenario key 1, template 1, is not selected/.test(message)));
}

// Editing a template shared by two scenarios changes only the edited scenario.
{
  const rom = freshRom();
  const state = S.ensureState(rom);
  const internal = new Set([10, 22, 35, 62, 52, 53, 54]);
  const users = {};
  Object.keys(state.models).map(Number).filter(key => !internal.has(key)).forEach(key => {
    state.models[key].section1.forEach(row => {
      const edatId = ((row.bytes[1] << 8) | row.bytes[2]) - 1;
      (users[edatId] = users[edatId] || new Set()).add(key);
    });
  });
  const shared = Object.keys(users).map(Number).find(edatId => users[edatId].size >= 2);
  const [editedKey, otherKey] = [...users[shared]];
  const record = Uint8Array.from(Buffer.from(OB64.SCENARIO_ESET_DATA.enemydat.records[shared], 'hex'));
  record[0] = 21;
  rom.squadOverrides = { [editedKey + ':' + shared]: record };
  const plan = S.planNativeSquadLibrary(rom);
  assert.deepStrictEqual(plan.blocked, []);
  const { candidate, result } = exportCandidate(rom, plan);
  const idsIn = (key) => {
    const target = result.validationTargets.find(entry => entry.runtimeKey === key);
    const bytes = target ? decodeMember(candidate.z64, target.offset) : S.ensureState(rom).originalBytes[key];
    return OB64.scenarioCodec.parseEset(bytes, {}).section1.map(row => ((row.bytes[1] << 8) | row.bytes[2]) - 1);
  };
  assert(idsIn(editedKey).includes(RETAIL_RECORDS) && !idsIn(editedKey).includes(shared),
    'the edited scenario deploys its custom copy');
  assert(idsIn(otherKey).includes(shared) && !idsIn(otherKey).includes(RETAIL_RECORDS),
    'the other scenario still deploys the retail template');
  const library = decodeMember(candidate.z64, result.nativeSquadLibrary.key + KEY_BASE + 4);
  const retailLibrary = OB64.extractArchive(rom.z64, rom.archives[647]);
  assert(equal(library.subarray(shared * 35, shared * 35 + 35), retailLibrary.subarray(shared * 35, shared * 35 + 35)),
    'the shared retail template is unchanged');
}

// One physical ESET shared by several scenarios cannot hold a per-scenario squad.
{
  const rom = freshRom();
  assert(firstEditable(rom, [10]), 'a shared-resource squad accepts a level edit');
  const plan = S.planNativeSquadLibrary(rom);
  assert(plan.blocked.some(message => /share one deployment resource/.test(message)));
}

console.log('native squads rebuild: all checks passed');
