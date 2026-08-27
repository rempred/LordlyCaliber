'use strict';

// Regression for Scenario Section 1 rows whose accepted physical ESET did not
// match the older runtime squad atlas. Every deployed EDAT must remain editable
// through its canonical ENEMYDAT record.

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

function equalBytes(left, right) {
  if (!left || !right || left.length !== right.length) return false;
  for (let index = 0; index < left.length; index++) {
    if (left[index] !== right[index]) return false;
  }
  return true;
}

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'scenario-eset-codec.js',
  'scenario-eset-data.js',
  'squads-data.js',
  'squads.js',
  'scenario.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

function scenarioByKey(runtimeKey) {
  return OB64.SQUAD_DATA.scenarios.find(scenario => scenario.id === runtimeKey);
}

function esetByKey(runtimeKey) {
  return OB64.SCENARIO_ESET_DATA.scenarios.find(scenario => scenario.runtimeKey === runtimeKey);
}

const atlasOmissions = [];
const unresolved = [];
const mismatched = [];
for (const eset of OB64.SCENARIO_ESET_DATA.scenarios) {
  const model = OB64.scenarioCodec.parseEset(eset.rawHex);
  const squadScenario = scenarioByKey(eset.runtimeKey);
  const squadByEdat = new Map((squadScenario && squadScenario.squads || []).map(squad => [squad.e, squad]));
  const referencedEdats = new Set(model.section1.map(row => row.edatOneBased - 1));
  for (const edat of referencedEdats) {
    const squad = squadByEdat.get(edat);
    if (!squad) {
      atlasOmissions.push(eset.runtimeKey + ':' + edat);
    } else if (squad.rec !== OB64.SCENARIO_ESET_DATA.enemydat.records[edat]) {
      mismatched.push(eset.runtimeKey + ':' + edat);
    }
    if (!OB64._squadTest.vanillaRec(squadScenario, edat)) {
      unresolved.push(eset.runtimeKey + ':' + edat);
    }
  }
}

check('every accepted Scenario ESET row resolves to an editable stock record',
  unresolved.length === 0, unresolved.join(', '));
check('every generated squad record matches canonical ENEMYDAT',
  mismatched.length === 0, mismatched.join(', '));
check('only the known non-enemy Key 4 story row is absent from the enemy squad atlas',
  atlasOmissions.length === 1 && atlasOmissions[0] === '4:516', atlasOmissions.join(', '));

const key42Eset = esetByKey(42);
const key42Model = OB64.scenarioCodec.parseEset(key42Eset.rawHex);
const key42Source30 = key42Model.section1.find(row => row.sourceId === 30);
const key42 = scenarioByKey(42);
const amazeroth = key42.squads.find(squad => squad.e === 370);
const carthDormant = key42.squads.find(squad => squad.e === 371);

check('Key 42 uses accepted physical ESET 4_06a',
  key42Eset.resourcePath === 'ob64_all/e793/eset4_06a.bin', key42Eset.resourcePath);
check('Key 42 source 30 resolves EDAT 370 Amazeroth',
  key42Source30 && key42Source30.edatOneBased - 1 === 370 &&
    amazeroth && amazeroth.sources.length === 1 && amazeroth.sources[0] === 30 &&
    Buffer.from(amazeroth.rec, 'hex')[0] === 0x6A,
  JSON.stringify({ source30: key42Source30, squad: amazeroth }));
check('Key 42 keeps dormant EDAT 371 on source 46',
  carthDormant && carthDormant.sources.length === 1 && carthDormant.sources[0] === 46,
  JSON.stringify(carthDormant));
check('Key 42 and Key 57 expose distinct branch labels',
  key42.name === 'The Disillusioned - before Latium / Amazeroth' &&
    scenarioByKey(57).name === 'The Disillusioned - after Latium / Carth');

const key4 = scenarioByKey(4);
const key4Model = OB64.scenarioCodec.parseEset(esetByKey(4).rawHex);
const key4Source40 = key4Model.section1.find(row => row.sourceId === 40);
const storyEvent = OB64._squadTest.vanillaRec(key4, 516);
check('the similar Key 4 EDAT 516 story row resolves through canonical ENEMYDAT',
  key4Source40 && key4Source40.edatOneBased - 1 === 516 &&
    storyEvent && storyEvent[0] === 0x62,
  JSON.stringify(key4Source40));

const canonical370 = OB64._squadTest.canonicalEdatRec(370);
check('the squad editor falls back to canonical ENEMYDAT when atlas metadata is absent',
  equalBytes(OB64._squadTest.vanillaRec({ squads: [] }, 370), canonical370));

const effective370 = OB64.scenario._modelTest.effectiveRecordFor(
  { squadOverrides: {} },
  999,
  { edat: 370 }
);
check('scenario roster lookup falls back to canonical ENEMYDAT',
  equalBytes(effective370, canonical370));

OB64.classPortraitUrl = classId => 'portrait:' + classId;
check('scenario map leader icon uses the canonical fallback leader class',
  OB64.scenario._modelTest.liveLeaderIcon(
    { squadOverrides: {} },
    999,
    { edat: 370 }
  ) === 'portrait:106');

if (failures) {
  console.error('\n' + failures + ' scenario EDAT record test(s) failed.');
  process.exit(1);
}
console.log('\nAll Scenario EDAT record tests passed.');
