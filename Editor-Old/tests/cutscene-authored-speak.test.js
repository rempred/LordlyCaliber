'use strict';

// Speak without a window template. A new scene has no native dialogue window
// to copy, so Speak uses the verified fixed-placement window recipe on
// presentation archive selector 5. The line compiles before the native exit,
// shows its text in the native runtime, exports into the shared archive at an
// entry reserved across the Project, and returns on ROM-only reopen.

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
  'cutscene-dialogue-draw.js', 'cutscene-project.js', 'cutscene-export.js', 'rom-compatibility.js', 'cutscene-ui.js'
]) vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });

const UI = OB64.cutsceneUI;
const wordsOf = bytes => Array.from({ length: bytes.length / 4 }, (_, index) =>
  ((bytes[index * 4] << 24) | (bytes[index * 4 + 1] << 16) | (bytes[index * 4 + 2] << 8) | bytes[index * 4 + 3]) >>> 0);
const textAppears = (run, text) => run.states.some(frame => frame.dialogue.some(row =>
  row.payload && row.payload.nativeDialogue && String(row.payload.nativeDialogue.text || '').includes(text)));

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = UI.initialize(rom);
  await state.authoredReady;
  const scene = await UI.createAuthoredScene(rom, state, 'Speaking scene');
  await UI.loadScene(rom, state, scene);
  await UI.addActor(rom, state);
  const actor = UI.selectedDocument(state).actors[0];
  state.views[scene.sceneId].selectedActorId = actor.id;
  const archiveBefore = OB64.cutsceneAuthoring.readDialogue(z64, 5);

  // 1. Speak in a scene with no dialogue window uses the verified recipe.
  await UI.addDialogue(rom, state);
  let line = UI.selectedDocument(state).tracks.flatMap(track => track.clips).find(clip => clip.kind === 'dialogue');
  assert(line && line.capability === 'native', 'Speak is native in a new scene');
  assert.deepStrictEqual([line.payload.nativeWindow, line.payload.windowId, line.payload.presentationArchiveSelector,
    line.payload.presentationEntrySelector, line.payload.ownerSlot],
    ['fixed-placement', 0, 5, archiveBefore.entries.length, actor.slot]);
  await UI.executeEdit(rom, state, 'Write the line', next => {
    next.tracks.flatMap(track => track.clips).find(clip => clip.id === line.id).payload.text = 'Hello from a new scene.';
  });
  line = UI.selectedDocument(state).tracks.flatMap(track => track.clips).find(clip => clip.id === line.id);

  // 2. It compiles to the recipe window with this line's window, entry and
  // owner, before the native exit.
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], UI.selectedDocument(state));
  const words = wordsOf(compiled.decodedBytes);
  const at = words.indexOf(0xBF);
  assert.deepStrictEqual(words.slice(at, at + 14), [0xBF, 0, 5, archiveBefore.entries.length, actor.slot, 7, 160, 192, 1, 0, 1, 0, 0, 1]);
  assert(at < words.lastIndexOf(0x7D), 'the line comes before the native exit');
  assert.deepStrictEqual(words.slice(-5), OB64.cutsceneAuthored.CLASS5_EXIT_TAIL);

  // 3. The native runtime shows the text.
  delete state.runtimeByAssetId[scene.assetId];
  await UI.loadScene(rom, state, scene);
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime, JSON.stringify(state.sourceErrors));
  assert(textAppears(runtime, 'Hello from a new scene.'), 'the box shows the new text in the native runtime');

  // 4. A second line takes the next window and the next reserved entry.
  await UI.addDialogue(rom, state);
  const lines = UI.selectedDocument(state).tracks.flatMap(track => track.clips).filter(clip => clip.kind === 'dialogue');
  assert.strictEqual(lines.length, 2);
  const second = lines.find(clip => clip.id !== line.id);
  assert.deepStrictEqual([second.payload.windowId, second.payload.presentationEntrySelector], [1, archiveBefore.entries.length + 1]);

  // 5. Export writes both lines into the archive; ROM-only reopen keeps them.
  const candidate = { z64: z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const archiveAfter = OB64.cutsceneAuthoring.readDialogue(candidate.z64, 5);
  assert.strictEqual(archiveAfter.entries.length, archiveBefore.entries.length + 2, 'two entries are appended');
  assert(JSON.stringify(archiveAfter.entries[archiveBefore.entries.length]).includes('Hello from a new scene.'),
    'the first appended entry holds the new line');
  const reopenRom = { z64: candidate.z64.slice(), layout: rom.layout, archives: OB64.findArchives(candidate.z64) };
  const reopened = UI.initialize(reopenRom);
  await reopened.authoredReady;
  const reopenedScene = reopened.catalog.getScene(scene.sceneId);
  reopened.selectedSceneId = reopenedScene.sceneId;
  await UI.loadScene(reopenRom, reopened, reopenedScene);
  assert(textAppears(reopened.runtimeByAssetId[reopenedScene.assetId], 'Hello from a new scene.'),
    'the reopened scene shows the line');

  // 6. After ROM-only reopen the line is an editable native dialogue card:
  // its text changes, exports, and shows on the next reopen. Loading an
  // unchanged retail scene while that shared text is edited still previews.
  const reopenedLine = reopened.histories[reopenedScene.storageId].present.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'dialogue' && clip.payload.nativeDialogueEditable);
  assert(reopenedLine, 'the reopened line is an editable dialogue card');
  assert(reopenedLine.payload.rawText.includes('Hello from a new scene.'));
  await UI.executeEdit(reopenRom, reopened, 'Edit reopened line', next => {
    const row = next.tracks.flatMap(track => track.clips).find(clip => clip.id === reopenedLine.id);
    row.payload.rawText = row.payload.rawText.replace('Hello from a new scene.', 'Edited after reopen.');
  });
  const retailScene = reopened.catalog.getScene(OB64.cutsceneAuthored.CLASS5_TEMPLATE);
  reopened.selectedSceneId = retailScene.sceneId;
  await UI.loadScene(reopenRom, reopened, retailScene);
  assert(reopened.runtimeByAssetId[retailScene.assetId], 'an unchanged scene previews while shared text is edited: ' +
    JSON.stringify(reopened.sourceErrors));
  reopened.selectedSceneId = reopenedScene.sceneId;
  const edited = { z64: reopenRom.z64.slice(), layout: rom.layout };
  const editedPlan = await OB64.cutsceneExport.prepare(reopenRom, edited);
  OB64.cutsceneExport.apply(edited, editedPlan);
  OB64.cutsceneExport.validateApplied(edited, editedPlan);
  const thirdRom = { z64: edited.z64.slice(), layout: rom.layout, archives: OB64.findArchives(edited.z64) };
  const third = UI.initialize(thirdRom);
  await third.authoredReady;
  const thirdScene = third.catalog.getScene(scene.sceneId);
  third.selectedSceneId = thirdScene.sceneId;
  await UI.loadScene(thirdRom, third, thirdScene);
  assert(textAppears(third.runtimeByAssetId[thirdScene.assetId], 'Edited after reopen.'), 'the edited text plays after the next reopen');

  console.log('PASS template-free Speak: a new scene speaks natively through the fixed-placement window recipe, the line compiles before the native exit, shows in the native runtime, takes unique windows and Project-reserved entries, exports into archive selector 5, returns on ROM-only reopen as an editable line whose text change exports, and does not disturb unchanged scenes.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
