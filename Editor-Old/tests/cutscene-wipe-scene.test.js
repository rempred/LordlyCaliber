'use strict';

// Wipe an original scene to its valid empty form and rebuild it. The wiped
// stream keeps the scene's own leading records, the camera pair for classes 4
// and 5, and its parsed native exit (Director release or presentation
// handoff). Actors, a Hold and a line can then be added; export writes the
// scene over its retail slot after checking the ROM, the manifest names it,
// ROM-only reopen restores it as wiped, and it can be edited again. Scenes
// whose ending has no defined empty form are refused with the reason.

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
const wordsOf = bytes => Array.from({ length: bytes.length / 4 }, (_, index) =>
  ((bytes[index * 4] << 24) | (bytes[index * 4 + 1] << 16) | (bytes[index * 4 + 2] << 8) | bytes[index * 4 + 3]) >>> 0);
const romFrom = z64 => ({ z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) });
const CONTENT_KINDS = ['movement', 'pose', 'dialogue', 'effect', 'enter', 'exit', 'opacity', 'camera', 'fade', 'audio'];

async function exportFrom(rom) {
  const candidate = { z64: rom.z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  return { candidate, plan };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

  const cases = [
    { sceneId: 'rom-custom-lz:01F3E836', label: 'class 5 with its camera pair', exitKind: 'director-release', cameras: true,
      recipe: ['cutscene-art-bank:53', 100, { transformChannel: 0, uniformScalePercent: 95 }], minSize: 30 },
    // The strict framebuffer capture refuses this scene's existing scene-prop
    // layers (a capture limit unrelated to Wipe), so it is not render-checked.
    { sceneId: 'rom-director:01F3EC88', label: 'class 8 presentation handoff', exitKind: 'presentation-handoff', cameras: false,
      minSize: 15, renderCheck: false },
    { sceneId: 'rom-director:01F450D0', label: 'inherited class 2', exitKind: 'director-release', cameras: false, minSize: 15 }
  ];
  let projectFromFirst = null;
  for (const item of cases) {
    const rom = romFrom(z64.slice());
    const state = UI.initialize(rom);
    await state.authoredReady;
    const scene = state.catalog.getScene(item.sceneId);
    assert(scene, item.sceneId);
    state.selectedSceneId = scene.sceneId;
    await UI.loadScene(rom, state, scene);
    const retailSource = state.sourceByAssetId[scene.assetId];
    const rows = scene.source.directorSelectorRows.slice();

    // 1. Wipe: the document is empty of content; the stream is the wipe rule.
    await UI.wipeScene(rom, state, scene);
    assert(scene.wiped && scene.wiped.pending, item.label + ': the scene is marked wiped');
    assert.strictEqual(scene.wiped.exitKind, item.exitKind);
    assert.strictEqual(scene.wiped.keptCameras, item.cameras);
    const expected = OB64.cutsceneAuthored.wipeStream(retailSource.decodedBytes, retailSource.nodeDefinitions);
    assert.deepStrictEqual(Array.from(state.sourceByAssetId[scene.assetId].decodedBytes), Array.from(expected.bytes));
    const empty = state.histories[scene.storageId].present;
    assert.strictEqual(empty.actors.length, 0, item.label + ': no Actors remain');
    // What remains is setup (camera poses) and the exit itself (7D or 39,
    // projected as a native Director effect).
    const leftovers = empty.tracks.flatMap(track => track.clips).filter(clip => CONTENT_KINDS.includes(clip.kind) &&
      clip.kind !== 'camera' && !(clip.kind === 'effect' && ['0x7D', '0x39'].includes(clip.payload.nativeOpcode)));
    assert.deepStrictEqual(leftovers.map(clip => clip.kind), [], item.label + ': no content actions remain');
    assert.strictEqual(UI.editCount(state), 1, 'a wipe is an edit');
    // The empty wiped scene previews (an explicitly wiped scene is not a
    // retail control resource, even though it has only its exit).
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    assert(state.runtimeByAssetId[scene.assetId], item.label + ': the empty wiped scene previews: ' + JSON.stringify(state.sourceErrors));

    // 2. Rebuild: Actor, Hold and a line go before the native exit.
    await UI.addActor(rom, state);
    const actor = state.histories[scene.storageId].present.actors[0];
    state.views[scene.sceneId].selectedActorId = actor.id;
    if (item.recipe) {
      assert.deepStrictEqual([actor.artSourceId, actor.initial.z, actor.source.presentation],
        item.recipe, item.label + ': the Actor copies the first Actor and presentation of this scene');
    }
    // It draws at a usable size on the Stage.
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    const actorDocument = state.histories[scene.storageId].present;
    if (item.renderCheck !== false) {
    const actorPreview = OB64.cutscenePreview.evaluateAtFrame(actorDocument, 5, {});
    const frame = await UI.capturePreviewFrame(rom, state, actorDocument,
      { preview: actorPreview, backgroundPolicy: 'require', pass: 5, nodeId: 'wipe-test' });
    const hit = frame.hitRegions.find(region => region.actorId === actor.id);
    // Hit regions are clipped to the viewport: touching an edge means the figure is cut off.
    assert(hit && hit.right - hit.left >= item.minSize && hit.left >= 1 && hit.right <= 319 && hit.top >= 1 && hit.bottom <= 239,
      item.label + ': the Actor is visible at a usable size: ' + JSON.stringify(hit));
    }
    // The Size control compiles to the native uniform scale.
    await UI.executeEdit(rom, state, 'Resize', next => {
      const row = next.actors.find(candidate => candidate.id === actor.id);
      row.source.presentation = Object.assign({}, row.source.presentation || {}, { uniformScalePercent: 60 });
    });
    const sized = wordsOf(OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId],
      state.histories[scene.storageId].present).decodedBytes);
    assert(sized.some((word, index) => word === 0x1D && sized[index + 1] === actor.slot && sized[index + 2] === 60), 'Size compiles to 0x1D');
    await UI.addHold(rom, state);
    await UI.addDialogue(rom, state);
    const rebuilt = state.histories[scene.storageId].present;
    const line = rebuilt.tracks.flatMap(track => track.clips).find(clip => clip.kind === 'dialogue');
    assert(line && line.capability === 'native', item.label + ': Speak is native in a wiped scene');
    assert.strictEqual(rebuilt.exportRequirements.capability, 'native', JSON.stringify(rebuilt.exportRequirements.reasons));
    const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], rebuilt);
    const words = wordsOf(compiled.decodedBytes);
    const exitOpcode = item.exitKind === 'director-release' ? 0x7D : 0x39;
    assert(words.indexOf(0x14) < words.lastIndexOf(exitOpcode), 'the Actor is created before the exit');
    assert(words.indexOf(0xBF) < words.lastIndexOf(exitOpcode), 'the line opens before the exit');
    assert.deepStrictEqual(words.slice(-2), wordsOf(retailSource.decodedBytes).slice(-2), 'the class trailer is the original');
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    assert(state.runtimeByAssetId[scene.assetId], item.label + ': ' + JSON.stringify(state.sourceErrors));

    // 3. Export over the retail slot; the manifest names the wiped scene.
    const first = await exportFrom(rom);
    const entry = first.plan.entries.find(row => row.scene && row.scene.sceneId === scene.sceneId);
    assert(entry && !entry.result.noOp, item.label + ': the wipe is written');
    const manifestRow = A.read(first.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST);
    assert.deepStrictEqual(A.readManifest(first.candidate.z64, manifestRow).wipedScenes.map(entry => [entry.sceneId, entry.exitKind]),
      [[scene.sceneId, item.exitKind]]);
    rows.forEach(row => assert(first.candidate.z64.slice(A.directorTableZ64 + row * 4, A.directorTableZ64 + row * 4 + 4).some(Boolean)));

    // 4. ROM-only reopen restores the wiped state with the rebuilt content,
    // and the scene can be edited and exported again.
    const reopenedRom = romFrom(first.candidate.z64.slice());
    const reopened = UI.initialize(reopenedRom);
    await reopened.authoredReady;
    assert.strictEqual(reopened.authoredDiscovery.status, 'ok', JSON.stringify(reopened.authoredDiscovery));
    const again = reopened.catalog.getScene(scene.sceneId);
    assert(again.wiped && again.wiped.pending === false && again.wiped.exitKind === item.exitKind,
      item.label + ': reopen restores the wiped state');
    assert.strictEqual(UI.editCount(reopened), 0, 'a reopened wipe is not an edit');
    reopened.selectedSceneId = again.sceneId;
    await UI.loadScene(reopenedRom, reopened, again);
    const reopenedDocument = reopened.histories[again.storageId].present;
    assert.strictEqual(reopenedDocument.actors.length, 1, 'the rebuilt Actor is back');
    const reopenedRuntime = reopened.runtimeByAssetId[again.storageId] || reopened.runtimeByAssetId[again.assetId];
    assert(reopenedRuntime && reopenedRuntime.states.some(frame => frame.dialogue.some(dialogueRow =>
      dialogueRow.payload && dialogueRow.payload.nativeDialogue && String(dialogueRow.payload.nativeDialogue.text || '').includes('New dialogue'))),
      item.label + ': the line plays again after reopen');
    await UI.addHold(reopenedRom, reopened);
    const second = await exportFrom(reopenedRom);
    assert(second.plan.entries.some(row => row.scene && row.scene.sceneId === scene.sceneId && !row.result.noOp));
    const secondManifest = A.readManifest(second.candidate.z64,
      A.read(second.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
    assert.deepStrictEqual(secondManifest.wipedScenes.map(entry => [entry.sceneId, entry.exitKind]), [[scene.sceneId, item.exitKind]]);

    if (!projectFromFirst) projectFromFirst = { project: OB64.cutsceneProject.collect(state), sceneId: scene.sceneId, document: rebuilt };
  }

  // 5. Project: a wipe is saved as a wiped row (schema 2) and replayed from the
  // loaded ROM on load, keeping the rebuilt document.
  const { project, sceneId, document } = projectFromFirst;
  const row = project.scenes.find(item => item.sceneId === sceneId);
  assert(row && row.wiped === true && project.schemaVersion === 2);
  const loadRom = romFrom(z64.slice());
  const loadState = UI.initialize(loadRom);
  await loadState.authoredReady;
  const prepared = await OB64.cutsceneProject.prepareImport(loadRom, project, {});
  OB64.cutsceneProject.applyPrepared(loadState, prepared);
  const loaded = loadState.catalog.getScene(sceneId);
  assert(loaded.wiped && loaded.wiped.pending, 'the Project wipe is replayed');
  assert.strictEqual(OB64.cutsceneModel.serializeSceneDocument(loadState.histories[loaded.storageId].present, 0),
    OB64.cutsceneModel.serializeSceneDocument(document, 0), 'the rebuilt document is restored');

  // 6. Restore original undoes a wipe made in this session.
  const restoreRom = romFrom(z64.slice());
  const restoreState = UI.initialize(restoreRom);
  await restoreState.authoredReady;
  const restoreScene = restoreState.catalog.getScene(cases[0].sceneId);
  restoreState.selectedSceneId = restoreScene.sceneId;
  await UI.loadScene(restoreRom, restoreState, restoreScene);
  const before = restoreState.sourceByAssetId[restoreScene.assetId].decodedSha256;
  await UI.wipeScene(restoreRom, restoreState, restoreScene);
  await UI.restoreWipedScene(restoreRom, restoreState, restoreScene);
  assert.strictEqual(restoreScene.wiped, undefined);
  assert.strictEqual(restoreState.sourceByAssetId[restoreScene.assetId].decodedSha256, before, 'the original stream is back');
  assert.strictEqual(UI.editCount(restoreState), 0);

  // 6b. An empty wipe exported and reopened keeps the Actor recipe of the
  // original cast (recorded in the manifest), so a new Actor still fits.
  const emptyRom = romFrom(z64.slice());
  const emptyState = UI.initialize(emptyRom);
  await emptyState.authoredReady;
  const emptyScene = emptyState.catalog.getScene(cases[0].sceneId);
  emptyState.selectedSceneId = emptyScene.sceneId;
  await UI.loadScene(emptyRom, emptyState, emptyScene);
  await UI.wipeScene(emptyRom, emptyState, emptyScene);
  const emptyExport = await exportFrom(emptyRom);
  const emptyManifest = A.readManifest(emptyExport.candidate.z64,
    A.read(emptyExport.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
  assert.deepStrictEqual([emptyManifest.wipedScenes[0].actorRecipe.bank, emptyManifest.wipedScenes[0].actorRecipe.uniformScalePercent], [53, 95]);
  const emptyReopenRom = romFrom(emptyExport.candidate.z64.slice());
  const emptyReopen = UI.initialize(emptyReopenRom);
  await emptyReopen.authoredReady;
  const emptyAgain = emptyReopen.catalog.getScene(cases[0].sceneId);
  emptyReopen.selectedSceneId = emptyAgain.sceneId;
  await UI.loadScene(emptyReopenRom, emptyReopen, emptyAgain);
  await UI.addActor(emptyReopenRom, emptyReopen);
  const emptyActor = emptyReopen.histories[emptyAgain.storageId].present.actors[0];
  assert.deepStrictEqual([emptyActor.artSourceId, emptyActor.initial.z, emptyActor.source.presentation],
    ['cutscene-art-bank:53', 100, { transformChannel: 0, uniformScalePercent: 95 }], 'the reopened empty wipe keeps its recipe');

  // 7. Refusals name the reason: Hugo's interactive loop, a relaunch ending.
  const refuseRom = romFrom(z64.slice());
  const refuseState = UI.initialize(refuseRom);
  await refuseState.authoredReady;
  for (const [refusedId, reason] of [['rom-director:01F3E500', /opcode 0x59/], ['rom-director:01F4005C', /relaunches the parent event/],
    ['rom-director:01F89860', /opcode 0xBB/]]) {
    const refused = refuseState.catalog.getScene(refusedId);
    refuseState.selectedSceneId = refused.sceneId;
    await assert.rejects(UI.wipeScene(refuseRom, refuseState, refused), reason);
    assert.strictEqual(refused.wiped, undefined);
  }

  console.log('PASS Wipe: class 5 with cameras, class 8 presentation handoff and inherited class 2 wipe to their own empty form, rebuild with an Actor, Hold and line before the native exit, export over the retail slot with the manifest naming them, reopen as wiped with the content, edit and export again; Project save and replay; Restore original; and refusals with reasons.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
