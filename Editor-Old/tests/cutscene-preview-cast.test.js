'use strict';

// Story-cast scenes (event rows 66 and 67, e.g. 01FA8BF4) take up to five
// story characters from the player's army through launch translation entries
// 0..16. The preview supplies a declared sample cast that the user can change;
// an imported table and exact launch-context values take precedence, the
// launch-translated dialogue stays symbolic, placements stay the stream's own
// literal coordinates, and an untouched scene compiles byte for byte.

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
const romFrom = z64 => ({ z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) });
const STORY = 'scene:director:01fa8bf4';

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = romFrom(z64);
  const state = UI.initialize(rom);
  await state.authoredReady;

  // 1. The 18 row-66 story scenes use the cast; the row-98 scene with a
  // single entry of another meaning, and ordinary scenes, do not.
  const storyScenes = state.catalog.scenes.filter(scene => UI.isStoryCastScene(scene));
  assert.strictEqual(storyScenes.length, 18);
  assert(storyScenes.some(scene => scene.sceneId === STORY));
  assert(!UI.isStoryCastScene(state.catalog.getScene('scene:director:01fb91ec')));
  assert(!UI.isStoryCastScene(state.catalog.getScene('rom-custom-lz:01F3E836')));

  // 2. The default sample: Destin, Debonair, Gilbert, Aisha, Saradin, with
  // secondary classes equal to primary, their dialogue classifiers, and the
  // fixed Actor 5 variant and portrait.
  const scene = state.catalog.getScene(STORY);
  const expected = { 0: 2, 1: 0x58, 2: 0x59, 3: 0x5A, 4: 0x5B, 5: 0x5C, 6: 0x58, 7: 0x59, 8: 0x5A, 9: 0x5B, 10: 0x5C,
    11: 3, 12: 6, 13: 7, 14: 4, 15: 5, 16: 83 };
  assert.deepStrictEqual(UI.launchOperandTranslations(scene, null, state), expected);

  // 3. Precedence: an imported table, then exact launch-context values, then the sample.
  const context = { context: { launchTranslationTable: [null, null, 0x61] } };
  const mixed = UI.launchOperandTranslations(scene, context, state, { 1: 0x54 });
  assert.strictEqual(mixed[1], 0x54, 'imported entry wins');
  assert.strictEqual(mixed[2], 0x61, 'exact launch-context entry beats the sample');
  assert.strictEqual(mixed[3], 0x5A, 'the sample fills the rest');
  // A partly supplied character stays coherent: its secondary class and
  // dialogue classifier follow the supplied primary (Dio 0x54, classifier 1;
  // Ankiseth 0x61, classifier 2), whether it was imported or known at launch.
  assert.deepStrictEqual([mixed[6], mixed[11]], [0x54, 1], 'imported primary drives its companions');
  assert.deepStrictEqual([mixed[7], mixed[12]], [0x61, 2], 'launch-context primary drives its companions');
  assert.deepStrictEqual([mixed[8], mixed[13]], [0x5A, 7], 'sample characters are unchanged');
  // Explicit companion values still win on their own.
  const companions = UI.launchOperandTranslations(scene, { context: { launchTranslationTable: [null, null, 0x61, null, null, null,
    null, 0x62] } }, state, { 1: 0x54, 11: 9 });
  assert.deepStrictEqual([companions[1], companions[6], companions[11]], [0x54, 0x54, 9], 'explicit imported classifier wins');
  assert.deepStrictEqual([companions[2], companions[7], companions[12]], [0x61, 0x62, 2], 'explicit context secondary wins');
  assert.deepStrictEqual([companions[0], companions[16]], [2, 83], 'fixed entries keep their sample values');

  // 4. The cast can be changed, including the ordinary Fighter filler
  // (classifier 20); an unknown class is refused.
  state.selectedSceneId = scene.sceneId;
  await UI.loadScene(rom, state, scene);
  await UI.setPreviewCast(rom, state, scene, [0x54, 0x56, 0x61, 0x04, 0x02]);
  const castB = UI.launchOperandTranslations(scene, null, state);
  assert.deepStrictEqual([1, 2, 3, 4, 5].map(i => castB[i]), [0x54, 0x56, 0x61, 0x04, 0x02]);
  assert.deepStrictEqual([11, 12, 13, 14, 15].map(i => castB[i]), [1, 0, 2, 19, 20]);
  assert.throws(() => UI.setPreviewCast(rom, state, scene, [0x58, 0x59, 0x5A, 0x5B, 0x33]), /five story characters/);
  assert.throws(() => UI.setPreviewCast(rom, state, state.catalog.getScene('rom-custom-lz:01F3E836'), expected), /does not take its cast/);

  // 5. The launch-translated dialogue projects symbolically and the untouched
  // scene compiles to its own bytes, every 0x0888xxxx word included.
  const document = state.histories[scene.storageId].present;
  const symbolic = document.tracks.flatMap(track => track.clips)
    .filter(clip => clip.kind === 'dialogue' && clip.payload.sourceSystem === 'serifu-launch-translated');
  assert.strictEqual(symbolic.length, 5);
  assert(symbolic.every(clip => clip.payload.nativeDialogueEditable === false && Number.isInteger(clip.payload.launchTranslationIndex)));
  const source = state.sourceByAssetId[scene.assetId];
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, document);
  assert(OB64.cutsceneCodec.equalBytes(compiled.decodedBytes, source.decodedBytes), 'untouched compile is byte-identical');

  // 6. The preview runs past the launch loader for either cast, and every
  // Actor keeps the stream's literal placement.
  const placements = {};
  document.actors.forEach(actor => { placements[actor.slot] = [actor.initial.x, actor.initial.y, actor.initial.z]; });
  const positions = [];
  for (const cast of [[0x58, 0x59, 0x5A, 0x5B, 0x5C], [0x54, 0x56, 0x61, 0x04, 0x02]]) {
    await UI.setPreviewCast(rom, state, scene, cast);
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    const runtime = state.runtimeByAssetId[scene.assetId];
    assert(runtime && runtime.states.length > 1000, 'the preview runs with the sample cast');
    assert.deepStrictEqual(runtime.launchOperandTranslation.missingIndexes, []);
    const first = runtime.states[5].actors.filter(actor => placements[actor.slot]);
    first.forEach(actor => assert.deepStrictEqual([actor.baseX, actor.baseY, actor.baseZ].map(v => Math.round(v)),
      placements[actor.slot], 'slot ' + actor.slot + ' stands on its literal placement'));
    positions.push(JSON.stringify(first.map(actor => [actor.slot, actor.baseX, actor.baseY, actor.baseZ])));
  }
  assert.strictEqual(positions[0], positions[1], 'the cast changes identities, never positions');

  // 7. An imported Playback-inputs table is honoured and left unchanged.
  const program = OB64.cutsceneCodec.createIr(scene, source.decodedBytes).program;
  const imported = OB64.cutsceneRomStart.input(rom.z64, scene, program);
  imported.externalProducers.value.directorLaunch.operandTranslations = Object.assign({}, expected, { 1: 0x61 });
  state.nativeLaunchInputsByAssetId = { [scene.assetId]: imported };
  delete state.runtimeByAssetId[scene.assetId];
  await UI.loadScene(rom, state, scene);
  const importedRun = state.runtimeByAssetId[scene.assetId];
  assert(importedRun, 'the imported table runs');
  assert.strictEqual(importedRun.nativeLaunchInputs.externalProducers.value.directorLaunch.operandTranslations[1], 0x61);
  assert.strictEqual(imported.externalProducers.value.directorLaunch.operandTranslations[1], 0x61);
  assert.strictEqual(Object.keys(imported.externalProducers.value.directorLaunch.operandTranslations).length, 17,
    'the stored imported inputs are not rewritten');

  console.log('PASS Preview cast: 18 story-cast scenes; default sample and classifiers (Fighter filler included); imported, ' +
    'launch-context and sample precedence; symbolic launch dialogue and byte-identical compile; the preview runs for either ' +
    'cast with every Actor on its literal placement; imported tables are honoured and left unchanged.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
