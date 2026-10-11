'use strict';

// The whole-app wrappers around authored scenes: creating or renaming a scene
// notifies the app and counts as a Cutscene edit, the Project summary and the
// changelog include it, loading a Project of only new scenes reports them as
// applied, and a scene rediscovered unchanged from an exported ROM is not a
// change until it is edited.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-authoring.js', 'cutscene-dialogue-data.js',
  'cutscene-dialogue-lifecycle-data.js', 'cutscene-dialogue.js', 'cutscene-dialogue-lifecycle.js',
  'cutscene-resource-scheduler-data.js', 'cutscene-resource-scheduler.js',
  'cutscene-director-launch-data.js', 'cutscene-director-launch.js', 'cutscene-rom-start-data.js',
  'cutscene-rom-start.js', 'cutscene-preserved-stage-data.js', 'cutscene-preserved-stage.js',
  'cutscene-framebuffer-data.js', 'cutscene-framebuffer.js', 'cutscene-image-echo-data.js',
  'cutscene-image-echo.js', 'cutscene-shared-actor-data.js', 'cutscene-shared-actor.js',
  'cutscene-map-menu-data.js', 'cutscene-map-menu.js', 'cutscene-runtime.js', 'cutscene-preview.js',
  'cutscene-assets.js', 'cutscene-njpg.js', 'cutscene-sprites.js', 'cutscene-renderer.js',
  'cutscene-dialogue-draw.js', 'cutscene-project.js', 'cutscene-export.js', 'rom-compatibility.js',
  'cutscene-ui.js', 'patch.js', 'changelog.js'
]) vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });

// Other editor tables stay empty and unchanged; only Cutscene data differs.
function romFrom(z64) {
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64),
    shops: [], itemStats: [], classDefs: [], consumables: [] };
  OB64.patch.snapshotOriginal(rom);
  return rom;
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

  // 1. New scene and rename notify the app and count as one Cutscene edit.
  const rom = romFrom(z64.slice());
  const state = OB64.cutsceneUI.initialize(rom);
  await state.authoredReady;
  const notifications = [];
  state.callbacks.onChange = label => notifications.push(label);
  assert.strictEqual(OB64.cutsceneUI.editCount(state), 0);
  const scene = await OB64.cutsceneUI.createAuthoredScene(rom, state, 'App scene');
  OB64.cutsceneUI.renameAuthoredScene(rom, state, scene, 'Renamed app scene');
  assert.deepStrictEqual(notifications, ['Created App scene', 'Renamed scene to Renamed app scene']);
  assert.strictEqual(OB64.cutsceneUI.editCount(state), 1, 'an empty new scene is an edit the Export gate sees');

  // 2. The Project summary counts it (Save Project's empty gate) and the
  // changelog lists it with its selector row.
  const patch = OB64.patch.collectPatch(rom);
  assert.strictEqual(patch.summary.cutscenes_modified, 1);
  assert.strictEqual(patch.patches.cutscenes.schemaVersion, 2);
  const report = OB64.changelog.build(rom, patch, {});
  const section = report.sections.find(item => item.title === 'Cutscenes');
  assert(section, 'the changelog has a Cutscenes section: ' + report.text);
  const entry = section.entries.find(item => item.title === 'Renamed app scene');
  assert(entry && entry.lines[0] === 'New scene on Director selector row ' + scene.authored.selectorRow + '.',
    'the changelog names the new scene and its row: ' + JSON.stringify(section.entries));

  // 3. Loading a Project that holds only new scenes reports them as applied.
  const freshRom = romFrom(z64.slice());
  const fresh = OB64.cutsceneUI.initialize(freshRom);
  await fresh.authoredReady;
  const prepared = await OB64.cutsceneProject.prepareImport(freshRom, patch.patches.cutscenes, {});
  assert.strictEqual(OB64.cutsceneProject.applyPrepared(fresh, prepared), 1, 'the wrapper sets the Cutscene dirty flag');
  assert.strictEqual(OB64.cutsceneUI.editCount(fresh), 1, 'a Project scene absent from the ROM is a change');

  // 4. Export, then reopen: the rediscovered scene matches the ROM, so it is
  // not a change and the Project is empty, until it is renamed.
  const candidate = { z64: z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const reopenedRom = romFrom(candidate.z64.slice());
  const reopened = OB64.cutsceneUI.initialize(reopenedRom);
  await reopened.authoredReady;
  assert.deepStrictEqual(reopened.authoredDiscovery, { status: 'ok', count: 1 });
  assert.strictEqual(OB64.cutsceneUI.editCount(reopened), 0, 'an untouched rediscovered scene is not an edit');
  assert.strictEqual(OB64.patch.collectPatch(reopenedRom).summary.cutscenes_modified, 0);
  const rediscovered = reopened.catalog.authoredScenes()[0];
  OB64.cutsceneUI.renameAuthoredScene(reopenedRom, reopened, rediscovered, 'Renamed after reopen');
  assert.strictEqual(OB64.cutsceneUI.editCount(reopened), 1);
  assert.strictEqual(OB64.patch.collectPatch(reopenedRom).summary.cutscenes_modified, 1);

  // 5. Loading the original Project onto the exported ROM restores the same
  // identity over its ROM copy; an identical restore is not a new change.
  const againRom = romFrom(candidate.z64.slice());
  const again = OB64.cutsceneUI.initialize(againRom);
  await again.authoredReady;
  const preparedAgain = await OB64.cutsceneProject.prepareImport(againRom, patch.patches.cutscenes, {});
  OB64.cutsceneProject.applyPrepared(again, preparedAgain);
  assert.strictEqual(again.catalog.authoredScenes().length, 1, 'the restore replaces, never duplicates');
  assert.strictEqual(OB64.cutsceneUI.editCount(again), 0, 'restoring the scene the ROM already holds is not a change');

  console.log('PASS authored scenes reach the app: New scene and rename notify and count as edits, the Project summary and changelog include them, Project load reports them applied, and unchanged rediscovered scenes stay clean until edited.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
