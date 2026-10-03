'use strict';

// Deleting a new scene. One made this session simply goes. One the loaded ROM
// holds is released on export: its arena record is dropped, its selector row
// returns to zero, its manifest entry goes, and launch sites assigned to it
// return to their retail scenes. A Project carries the deletion.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-provenance-data.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-event-rows-data.js', 'cutscene-events.js',
  'cutscene-authoring.js', 'cutscene-dialogue-data.js',
  'cutscene-dialogue-lifecycle-data.js', 'cutscene-dialogue.js', 'cutscene-dialogue-lifecycle.js',
  'cutscene-resource-scheduler-data.js', 'cutscene-resource-scheduler.js',
  'cutscene-director-launch-data.js', 'cutscene-director-launch.js', 'cutscene-rom-start-data.js',
  'cutscene-rom-start.js', 'cutscene-preserved-stage-data.js', 'cutscene-preserved-stage.js',
  'cutscene-framebuffer-data.js', 'cutscene-framebuffer.js', 'cutscene-image-echo-data.js',
  'cutscene-image-echo.js', 'cutscene-shared-actor-data.js', 'cutscene-shared-actor.js',
  'cutscene-map-menu-data.js', 'cutscene-map-menu.js', 'cutscene-runtime.js', 'cutscene-preview.js',
  'cutscene-assets.js', 'cutscene-njpg.js', 'cutscene-sprites.js', 'cutscene-renderer.js',
  'cutscene-dialogue-draw.js', 'cutscene-project.js', 'cutscene-export.js', 'rom-compatibility.js', 'cutscene-ui.js'
]) vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });

const UI = OB64.cutsceneUI;
const A = OB64.cutsceneExport.arena;
const E = OB64.cutsceneEvents;
const u32 = (z64, at) => ((z64[at] << 24) | (z64[at + 1] << 16) | (z64[at + 2] << 8) | z64[at + 3]) >>> 0;
const romFrom = z64 => ({ z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) });
const OPENING = 'event-director:1:b0862';

async function exportFrom(rom) {
  const candidate = { z64: rom.z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  return { candidate, plan };
}
async function open(z64) {
  const rom = romFrom(z64.slice());
  const state = UI.initialize(rom);
  await state.authoredReady;
  return { rom, state };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

  // 1. Two new scenes; the first is launched from the Opening Title site.
  const start = await open(z64);
  const kept = await UI.createAuthoredScene(start.rom, start.state, 'Kept scene');
  const doomed = await UI.createAuthoredScene(start.rom, start.state, 'Deleted scene');
  UI.assignLaunchSite(start.rom, start.state, doomed, OPENING);
  const first = await exportFrom(start.rom);
  const doomedRow = doomed.authored.selectorRow, keptRow = kept.authored.selectorRow;
  assert(u32(first.candidate.z64, A.directorTableZ64 + doomedRow * 4) !== 0);

  // 2. After ROM-only reopen, delete the bound scene: its binding goes, and
  // the deletion is an edit.
  const reopened = await open(first.candidate.z64);
  const reopenedDoomed = reopened.state.catalog.getScene(doomed.sceneId);
  assert.strictEqual(UI.deleteAuthoredScene(reopened.rom, reopened.state, reopenedDoomed), 1, 'one launch site is released');
  assert.strictEqual(reopened.state.catalog.getScene(doomed.sceneId), null);
  assert.deepStrictEqual(reopened.state.eventBindings, []);
  assert(UI.editCount(reopened.state) >= 2, 'the deletion and the binding change are edits');
  const project = OB64.cutsceneProject.collect(reopened.state);
  assert.deepStrictEqual(project.deletedAuthoredScenes, [doomed.authored.authoredId]);

  // 3. Export releases it: zero selector row, no record, no manifest entry,
  // the event row back on its retail key; the kept scene is untouched.
  const second = await exportFrom(reopened.rom);
  assert.strictEqual(u32(second.candidate.z64, A.directorTableZ64 + doomedRow * 4), 0, 'the selector row is free again');
  const arena = A.read(second.candidate.z64);
  assert(!arena.records.some(row => row[0] >>> 28 === A.KIND.AUTHORED && (row[0] & 0x0FFFFFFF) === doomedRow));
  assert(arena.records.some(row => row[0] >>> 28 === A.KIND.AUTHORED && (row[0] & 0x0FFFFFFF) === keptRow));
  const manifest = A.readManifest(second.candidate.z64, arena.records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
  assert.deepStrictEqual(manifest.authoredScenes.map(entry => entry.authoredId), [kept.authored.authoredId]);
  assert.strictEqual(manifest.eventBindings, undefined);
  assert.strictEqual(u32(second.candidate.z64, E.directoryWordZ64(1)), OB64.cutsceneEventRows.rows[1].key);
  const third = await open(second.candidate.z64);
  assert.deepStrictEqual(third.state.catalog.authoredScenes().map(scene => scene.authored.authoredId), [kept.authored.authoredId]);
  assert(OB64.cutsceneAuthored.freeSelectorRows(second.candidate.z64, third.state.catalog, []).includes(doomedRow), 'the row can be reused');

  // 4. The Project replays the deletion on the ROM that holds the scene.
  const replay = await open(first.candidate.z64);
  const prepared = await OB64.cutsceneProject.prepareImport(replay.rom, project, {});
  OB64.cutsceneProject.applyPrepared(replay.state, prepared);
  assert.strictEqual(replay.state.catalog.getScene(doomed.sceneId), null, 'the Project deletes the scene');
  const replayed = await exportFrom(replay.rom);
  assert.strictEqual(u32(replayed.candidate.z64, A.directorTableZ64 + doomedRow * 4), 0);
  const retail = await open(z64);
  await assert.rejects(OB64.cutsceneProject.prepareImport(retail.rom, project, {}), /does not hold/,
    'a deletion needs the ROM that holds the scene');

  // 4b. Deleting a scene bound to a site drops cached previews that depended
  // on it: Traveling Cutscene 3 (b08A8) follows the scene bound to b0890.
  // Loading a Project's bindings does the same.
  const cacheRun = await open(z64);
  const predecessor = await UI.createAuthoredScene(cacheRun.rom, cacheRun.state, 'Predecessor');
  UI.assignLaunchSite(cacheRun.rom, cacheRun.state, predecessor, 'event-director:1:b0890');
  const traveling3 = cacheRun.state.catalog.getScene(OB64.cutsceneAuthored.CLASS5_TEMPLATE);
  cacheRun.state.runtimeByAssetId[traveling3.assetId] = { marker: 'stale' };
  cacheRun.state.selectedSceneId = predecessor.sceneId;
  UI.deleteAuthoredScene(cacheRun.rom, cacheRun.state, predecessor);
  assert.strictEqual(cacheRun.state.runtimeByAssetId[traveling3.assetId], undefined, 'the dependent preview is dropped on delete');
  assert(UI.launchContextChoices(cacheRun.state, traveling3).filter(choice => choice.context.precedingDirectorLaunchId === 'event-director:1:b0890')
    .every(choice => choice.context.precedingDirectorSelector === 972), 'Traveling 3 follows its retail predecessor again');
  const projectRun = await open(z64);
  const boundProject = { format: project.format, schemaVersion: 2, sourceRevision: 'us-rev0', selectedSceneId: traveling3.sceneId,
    scenes: [], eventBindings: [{ launchSiteId: 'event-director:1:b0890',
      target: { kind: 'physical', sceneId: traveling3.sceneId, selectorRow: traveling3.source.directorSelectorRows[0] } }] };
  const projectTraveling = projectRun.state.catalog.getScene(OB64.cutsceneAuthored.CLASS5_TEMPLATE);
  projectRun.state.runtimeByAssetId[projectTraveling.assetId] = { marker: 'stale' };
  projectRun.state.selectedSceneId = projectRun.state.catalog.directorScenes[0].sceneId;
  OB64.cutsceneProject.applyPrepared(projectRun.state, await OB64.cutsceneProject.prepareImport(projectRun.rom, boundProject, {}));
  assert.strictEqual(projectRun.state.runtimeByAssetId[projectTraveling.assetId], undefined, 'loading bindings drops dependent previews');

  // 5. A scene made this session leaves no deletion record.
  const session = await open(z64);
  const temporary = await UI.createAuthoredScene(session.rom, session.state, 'Temporary');
  UI.deleteAuthoredScene(session.rom, session.state, temporary);
  assert.deepStrictEqual(session.state.deletedAuthored || [], []);
  assert.strictEqual(UI.editCount(session.state), 0, 'creating and deleting in one session leaves no edit');

  console.log('PASS delete scene: a ROM-held new scene is released on export (zero row, no record, no manifest entry, bindings back to retail), its row can be reused, the Project replays the deletion only on the ROM that holds it, and a session-only scene leaves no trace.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
