'use strict';

// Every Director scene shows where the game launches it from: the event-directory
// rows that request it and the scenario / mission / world-map node those rows
// belong to (parent repository trace docs/reviews/cutscene-launch-provenance-20260927).
// The generated data must match that trace, agree with the catalog's own launch
// rows, and make scenario names searchable.

const assert = require('assert');
const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const editor = path.resolve(__dirname, '..');
const { PARENT } = require('./project-paths.cjs');
const traceDir = path.join(PARENT, 'docs', 'reviews', 'cutscene-launch-provenance-20260927');
assert(fs.existsSync(path.join(traceDir, 'scene-provenance.json')), 'launch provenance trace missing: ' + traceDir);
const check = child_process.spawnSync(process.execPath,
  [path.join(editor, 'tools', 'generate-cutscene-provenance-data.js'), '--check', '--source', traceDir], { encoding: 'utf8' });
assert.strictEqual(check.status, 0, 'cutscene-provenance-data.js is stale:\n' + check.stdout + check.stderr);

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-provenance-data.js',
  'cutscene-model.js', 'cutscene-catalog.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}
const provenance = OB64.cutsceneProvenance;
assert.strictEqual(provenance.counts.rows, 115, 'all 115 event-directory rows are described');
assert.strictEqual(provenance.counts.scenes, 1472, 'every launched Director scene has provenance');

const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
// The rows the provenance names for a scene are exactly the rows its catalog launch data records.
let compared = 0;
let named = 0;
catalog.directorScenes.forEach(scene => {
  const launches = scene.launchProfile && scene.launchProfile.parentEventLaunches || [];
  const rows = new Set(launches.map(launch => launch.eventDirectoryRow));
  const entries = OB64.cutsceneCatalog.sceneProvenance(scene);
  if (!rows.size) { assert.strictEqual(entries.length, 0); return; }
  compared += 1;
  assert.deepStrictEqual(new Set(entries.map(entry => entry.row)), rows, scene.assetId + ' provenance rows match its launches');
  if (entries.some(entry => entry.missionOrScenario)) named += 1;
});
assert(compared >= 1400, 'launched scenes compared: ' + compared);
assert(named >= 1300, 'scenes with a named mission or scenario: ' + named);

// Known anchors from the trace.
const southern = OB64.cutsceneCatalog.sceneProvenance(catalog.getScene('rom-director:01F40650'));
assert.deepStrictEqual(southern.map(entry => [entry.row, entry.missionOrScenario, entry.worldMapNode, entry.confidence]),
  [[4, 'The Southern Reaches', 'Tenne Plains', 'High']]);
const office = OB64.cutsceneCatalog.sceneProvenance(catalog.getScene('rom-director:01F438FE'));
assert(office.some(entry => entry.row === 66 && /shared subroutine/.test(entry.role)), 'row 66 is the shared substitution program');

// Scenario and map-node names are searchable.
const byNode = catalog.searchScenes('Tenne Plains');
assert(byNode.some(scene => scene.assetId === 'rom-director:01F40650'), 'search by world-map node finds the scene');
const byMission = catalog.searchScenes('Southern Reaches');
assert(byMission.length >= 10 && byMission.every(scene =>
  OB64.cutsceneCatalog.sceneProvenance(scene).some(entry => /Southern Reaches/.test(entry.missionOrScenario || ''))),
  'search by mission returns only scenes launched from that mission');
console.log('cutscene provenance: ok (' + compared + ' launched scenes, ' + named + ' named, ' +
  byMission.length + ' Southern Reaches scenes)');
