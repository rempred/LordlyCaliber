'use strict';

// Sequencing in a new scene. Cards run in order: Wait and Speak block; Move,
// Camera pan and Fade start and hand over at once unless "wait until it
// finishes" is on, which new scenes turn on by default. The native exit stops
// anything still running, so the Studio warns about actions it would cut
// short and offers one explicit Hold before the exit.

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

const M = OB64.cutsceneModel;
const UI = OB64.cutsceneUI;

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = UI.initialize(rom);
  await state.authoredReady;
  const scene = await UI.createAuthoredScene(rom, state, 'Sequencing');
  await UI.loadScene(rom, state, scene);
  const view = state.views[scene.sceneId];
  const present = () => state.histories[scene.storageId].present;
  const cards = () => present().tracks.flatMap(track => track.clips).filter(clip => clip.source.insertBeforeNodeId);
  async function replay() {
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    const runtime = state.runtimeByAssetId[scene.assetId];
    assert(runtime, JSON.stringify(state.sourceErrors));
    return runtime;
  }

  // 1. The probe's sequence: Actor, Wait 30, then Move, Pose, Camera pan and
  // Fade added at displayed frames 5, 10, 15 and 20.
  await UI.addActor(rom, state);
  await UI.addHold(rom, state);
  view.frame = 5; await UI.addMove(rom, state);
  view.frame = 10; await UI.addPose(rom, state);
  view.frame = 15; await UI.addCamera(rom, state);
  view.frame = 20; await UI.addFade(rom, state);
  const cameraCard = cards().find(clip => clip.kind === 'camera');
  const cameraStartX = cameraCard.payload.target.translateX;
  state.selectedSceneId = scene.sceneId;
  await UI.executeEdit(rom, state, 'Pan the camera', next => {
    const row = next.tracks.flatMap(track => track.clips).find(clip => clip.id === cameraCard.id);
    row.payload = Object.assign({}, row.payload, { target: Object.assign({}, row.payload.target, { translateX: cameraStartX + 40 }) });
  });
  const asyncCards = cards().filter(clip => ['movement', 'camera', 'fade'].includes(clip.kind));
  assert.strictEqual(asyncCards.length, 3);
  assert(asyncCards.every(clip => clip.payload.waitForCompletion === true), 'new-scene async cards wait by default');
  assert.strictEqual(cards().find(clip => clip.kind === 'pose').payload.waitForCompletion, undefined, 'Pose needs no completion');

  // 2. Every action gets its time before the native exit: the Actor walks the
  // whole way, the fade runs to its end, and the scene then releases.
  let runtime = await replay();
  assert.strictEqual(runtime.states.at(-1).runtime.terminalReason, 'terminal-state-release');
  const xs = runtime.states.flatMap(frame => frame.actors.filter(actor => actor.slot === 0).map(actor => actor.baseX));
  assert.strictEqual(Math.min(...xs), -19);
  assert(Math.max(...xs) >= 30, 'the Actor walks to its target: ' + Math.max(...xs));
  const move = cards().find(clip => clip.kind === 'movement');
  const fade = cards().find(clip => clip.kind === 'fade');
  const moveStart = UI.clipRuntimeTick(state, scene, move);
  const fadeStart = UI.clipRuntimeTick(state, scene, fade);
  assert(moveStart >= 30, 'the Move starts after the 30-update Wait: ' + moveStart);
  assert(fadeStart >= moveStart + move.durationFrames, 'the Fade waits for the Move to finish');
  assert(runtime.states.length - 1 >= fadeStart + fade.durationFrames, 'the Fade runs to its end before the exit');
  assert.strictEqual(UI.runningAtExit(state, scene, present()), null, 'nothing is cut short');
  const finalState = runtime.states.at(-1);
  assert(Math.abs(finalState.cameraState.translateX - (cameraStartX + 40)) < 1e-6,
    'the camera pan reaches its target: ' + finalState.cameraState.translateX);
  assert(runtime.states.some(frame => frame.cameraState.translateX !== cameraStartX), 'the camera visibly moves');
  const finalFade = finalState.overlays.find(overlay => overlay.native && overlay.targetAlpha === 255);
  assert(finalFade && finalFade.alpha === 255, 'the fade reaches its target alpha before release: ' + JSON.stringify(finalState.overlays));
  assert(/^Starts at \d+\.\d s · lasts 1\.0 s, then the next action once the Actor arrives$/.test(UI.cardTimingText(state, scene, move)),
    UI.cardTimingText(state, scene, move));

  // 3. The compiled stream follows each async command with a native wait of
  // its duration; the scene still ends with the native exit.
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], present());
  const moveChange = compiled.changes.find(change => change.clipId === move.id);
  const words = Array.from({ length: compiled.decodedBytes.length / 4 }, (_, index) =>
    ((compiled.decodedBytes[index * 4] << 24) | (compiled.decodedBytes[index * 4 + 1] << 16) |
      (compiled.decodedBytes[index * 4 + 2] << 8) | compiled.decodedBytes[index * 4 + 3]) >>> 0);
  assert.deepStrictEqual(words.slice(moveChange.startWord + 8, moveChange.startWord + 15), [0, 0x80000002, 0x80000000, 0x11, 0, 0, 0],
    'the Move is followed by the native wait for arrival (0x11 on its slot)');

  assert.deepStrictEqual(words.slice(-5), OB64.cutsceneAuthored.CLASS5_EXIT_TAIL);

  // 4. Turning "wait" off on the last Fade makes it run into the exit: the
  // Studio reports it, and Hold until finished adds one explicit Wait card
  // that lets it complete.
  const withoutWait = M.cloneSceneDocument(present());
  withoutWait.tracks.forEach(track => track.clips.forEach(clip => {
    if (clip.id === fade.id) clip.payload = Object.assign({}, clip.payload, { waitForCompletion: false });
  }));
  state.histories[scene.storageId] = M.createHistory(withoutWait);
  runtime = await replay();
  const running = UI.runningAtExit(state, scene, present());
  assert(running && running.late.length === 1 && running.late[0].row.clip.id === fade.id,
    'the Studio names the Fade the exit would cut short: ' + JSON.stringify(running && running.late.map(item => item.row.clip.id)));
  assert(/alongside the next action/.test(UI.cardTimingText(state, scene, cards().find(clip => clip.id === fade.id))));
  const waitsBefore = cards().filter(clip => clip.kind === 'wait').length;
  state.selectedSceneId = scene.sceneId;
  await UI.holdUntilFinished(rom, state);
  const holds = cards().filter(clip => clip.kind === 'wait');
  assert.strictEqual(holds.length, waitsBefore + 1, 'one explicit Wait card is added');
  assert.strictEqual(holds.at(-1).durationFrames, running.needed);
  runtime = await replay();
  assert.strictEqual(UI.runningAtExit(state, scene, present()), null, 'after the Hold nothing is cut short');
  assert.strictEqual(runtime.states.at(-1).runtime.terminalReason, 'terminal-state-release');

  // 4b. The warning is part of the saved scene, not session state: it returns
  // after a Project reload and after a ROM-only reopen of an export.
  const beforeHold = M.cloneSceneDocument(withoutWait);
  const heldDocument = M.cloneSceneDocument(present());
  state.histories[scene.storageId] = M.createHistory(beforeHold);
  const project = OB64.cutsceneProject.collect(state);
  const projectRom = { z64: z64.slice(), layout: rom.layout, archives: rom.archives };
  const projectState = UI.initialize(projectRom);
  await projectState.authoredReady;
  OB64.cutsceneProject.applyPrepared(projectState, await OB64.cutsceneProject.prepareImport(projectRom, project, {}));
  const projectScene = projectState.catalog.getScene(scene.sceneId);
  projectState.selectedSceneId = projectScene.sceneId;
  await UI.loadScene(projectRom, projectState, projectScene);
  assert(UI.runningAtExit(projectState, projectScene, projectState.histories[projectScene.storageId].present),
    'the warning returns after a Project reload');
  const cutCandidate = { z64: z64.slice(), layout: rom.layout };
  const cutPlan = await OB64.cutsceneExport.prepare(rom, cutCandidate);
  OB64.cutsceneExport.apply(cutCandidate, cutPlan);
  const reopenRom = { z64: cutCandidate.z64.slice(), layout: rom.layout, archives: OB64.findArchives(cutCandidate.z64) };
  const reopened = UI.initialize(reopenRom);
  await reopened.authoredReady;
  const reopenedScene = reopened.catalog.getScene(scene.sceneId);
  reopened.selectedSceneId = reopenedScene.sceneId;
  await UI.loadScene(reopenRom, reopened, reopenedScene);
  const reopenedRunning = UI.runningAtExit(reopened, reopenedScene, reopened.histories[reopenedScene.storageId].present);
  assert(reopenedRunning, 'the warning returns after a ROM-only reopen of the export');
  assert.strictEqual(reopenedRunning.needed, fade.durationFrames + 1, 'the reopened warning asks for the whole fade');
  // Repair the reopened scene with the offered Hold: the fade now reaches its
  // target alpha before the native exit.
  await UI.holdUntilFinished(reopenRom, reopened);
  delete reopened.runtimeByAssetId[reopenedScene.assetId];
  await UI.loadScene(reopenRom, reopened, reopenedScene);
  const repaired = reopened.runtimeByAssetId[reopenedScene.assetId];
  const repairedFade = repaired.states.at(-1).overlays.find(overlay => overlay.native && overlay.targetAlpha === 255);
  assert(repairedFade && repairedFade.alpha === 255 && repairedFade.remaining === 0,
    'the repaired reopened scene fades fully before release: ' + JSON.stringify(repaired.states.at(-1).overlays));
  assert.strictEqual(UI.runningAtExit(reopened, reopenedScene, reopened.histories[reopenedScene.storageId].present), null);
  // 4d. The reopened native fade is a real Fade card: its duration and
  // direction come back from the ROM, and editing or removing it compiles.
  const nativeFadeCard = reopened.histories[reopenedScene.storageId].present.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'fade' && clip.source.nodeId);
  assert(nativeFadeCard, 'the native 0x1B projects as a Fade card');
  assert.deepStrictEqual([nativeFadeCard.durationFrames, nativeFadeCard.payload.direction, nativeFadeCard.capability],
    [fade.durationFrames, 'to-black', 'native']);
  await UI.executeEdit(reopenRom, reopened, 'Shorter fade', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => { if (clip.id === nativeFadeCard.id) clip.durationFrames = 30; }));
  });
  const editedReopened = reopened.histories[reopenedScene.storageId].present;
  const refade = OB64.cutsceneCodec.compileSceneDocument(reopenedScene, reopened.sourceByAssetId[reopenedScene.assetId], editedReopened);
  const refadeWords = Array.from({ length: refade.decodedBytes.length / 4 }, (_, index) =>
    ((refade.decodedBytes[index * 4] << 24) | (refade.decodedBytes[index * 4 + 1] << 16) |
      (refade.decodedBytes[index * 4 + 2] << 8) | refade.decodedBytes[index * 4 + 3]) >>> 0);
  const fadeAt = refadeWords.indexOf(0x1B);
  assert.deepStrictEqual(refadeWords.slice(fadeAt, fadeAt + 7), [0x1B, 30, 0, 0, 0, 0xFFFFFFFF, 255], 'the edited native fade recompiles');
  assert.strictEqual(editedReopened.exportRequirements.capability, 'native', JSON.stringify(editedReopened.exportRequirements.reasons));
  const refadeCandidate = { z64: reopenRom.z64.slice(), layout: rom.layout };
  const refadePlan = await OB64.cutsceneExport.prepare(reopenRom, refadeCandidate);
  OB64.cutsceneExport.apply(refadeCandidate, refadePlan);
  OB64.cutsceneExport.validateApplied(refadeCandidate, refadePlan);
  await UI.executeEdit(reopenRom, reopened, 'Remove fade', next => {
    next.tracks.forEach(track => { track.clips = track.clips.filter(clip => clip.id !== nativeFadeCard.id); });
  });
  const unfaded = OB64.cutsceneCodec.compileSceneDocument(reopenedScene, reopened.sourceByAssetId[reopenedScene.assetId],
    reopened.histories[reopenedScene.storageId].present);
  const unfadedWords = Array.from({ length: unfaded.decodedBytes.length / 4 }, (_, index) =>
    ((unfaded.decodedBytes[index * 4] << 24) | (unfaded.decodedBytes[index * 4 + 1] << 16) |
      (unfaded.decodedBytes[index * 4 + 2] << 8) | unfaded.decodedBytes[index * 4 + 3]) >>> 0);
  assert(!unfadedWords.some((word, index) => word === 0x1B && unfadedWords[index + 1] === 30 &&
    unfadedWords[index + 5] === 0xFFFFFFFF && unfadedWords[index + 6] === 255), 'removing the card removes the native fade');
  assert.strictEqual(unfadedWords.length, refadeWords.length - 7, 'exactly the seven fade words are gone');
  state.histories[scene.storageId] = M.createHistory(heldDocument);
  runtime = await replay();

  // 4c. Cards added at one displayed frame keep their creation order in the
  // list and in the compiled stream: Hold, Move, Hold.
  const tie = await UI.createAuthoredScene(rom, state, 'Tie order');
  await UI.loadScene(rom, state, tie);
  await UI.addActor(rom, state);
  state.views[tie.sceneId].selectedActorId = state.histories[tie.storageId].present.actors[0].id;
  state.views[tie.sceneId].frame = 0;
  await UI.addHold(rom, state);
  await UI.addMove(rom, state);
  await UI.addHold(rom, state);
  const tieDocument = state.histories[tie.storageId].present;
  const tieCompiled = OB64.cutsceneCodec.compileSceneDocument(tie, state.sourceByAssetId[tie.assetId], tieDocument);
  const insertedKinds = tieCompiled.changes.filter(change => change.operation === 'insert' && change.clipId)
    .sort((left, right) => left.startWord - right.startWord).map(change => change.kind);
  assert.deepStrictEqual(insertedKinds, ['wait', 'movement', 'wait'], 'the compiled order is the creation order');
  delete state.runtimeByAssetId[tie.assetId];
  await UI.loadScene(rom, state, tie);
  const tieXs = state.runtimeByAssetId[tie.assetId].states.map(frame => (frame.actors.find(actor => actor.slot === 0) || {}).baseX);
  assert(tieXs.slice(0, 29).every(x => x === -19), 'the Actor stands still during the first Hold');
  state.selectedSceneId = scene.sceneId;

  // 5. Export accepts the sequence.
  const candidate = { z64: z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);

  // 6. Retail insertions keep their behavior: an added Move there does not wait.
  const retail = state.catalog.getScene(OB64.cutsceneAuthored.CLASS5_TEMPLATE);
  state.selectedSceneId = retail.sceneId;
  await UI.loadScene(rom, state, retail);
  const retailView = state.views[retail.sceneId];
  retailView.selectedActorId = state.histories[retail.storageId].present.actors[0].id;
  await UI.addMove(rom, state);
  const retailMove = state.histories[retail.storageId].present.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'movement' && clip.source.insertBeforeNodeId);
  assert(retailMove && retailMove.payload.waitForCompletion === undefined, 'retail insertions do not change');

  // 7. Pose after Move (Astra's flow at playhead 0): a plain Pose keeps the
  // Actor where it is, so the scene stays native, exports, and ends at the
  // Move's target without jumping back; coordinates copied from interpolated
  // preview positions are rounded to native precision.
  const poseScene = await UI.createAuthoredScene(rom, state, 'Pose after Move');
  await UI.loadScene(rom, state, poseScene);
  await UI.addActor(rom, state);
  const poseView = state.views[poseScene.sceneId];
  poseView.selectedActorId = state.histories[poseScene.storageId].present.actors[0].id;
  poseView.frame = 0;
  await UI.addMove(rom, state);
  await UI.executeEdit(rom, state, 'Move to -54', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => {
      if (clip.kind === 'movement' && !clip.source.nodeId) clip.payload = Object.assign({}, clip.payload, { to: Object.assign({}, clip.payload.to, { x: -54 }) });
    }));
  });
  await UI.addPose(rom, state);
  await UI.addHold(rom, state);
  const poseDocument = state.histories[poseScene.storageId].present;
  const plainPose = poseDocument.tracks.flatMap(track => track.clips).find(clip => clip.kind === 'pose' && !clip.source.nodeId);
  assert.deepStrictEqual([plainPose.payload.x, plainPose.payload.y, plainPose.payload.z], [null, null, null], 'a plain Pose keeps position');
  assert.strictEqual(poseDocument.exportRequirements.capability, 'native', JSON.stringify(poseDocument.exportRequirements.reasons));
  const poseCompiled = OB64.cutsceneCodec.compileSceneDocument(poseScene, state.sourceByAssetId[poseScene.assetId], poseDocument);
  const poseWords = Array.from({ length: poseCompiled.decodedBytes.length / 4 }, (_, index) =>
    ((poseCompiled.decodedBytes[index * 4] << 24) | (poseCompiled.decodedBytes[index * 4 + 1] << 16) |
      (poseCompiled.decodedBytes[index * 4 + 2] << 8) | poseCompiled.decodedBytes[index * 4 + 3]) >>> 0);
  const stateChange = poseCompiled.changes.find(change => change.clipId === plainPose.id);
  assert.deepStrictEqual(poseWords.slice(stateChange.startWord + 5, stateChange.startWord + 8), [-1000 >>> 0, -1000 >>> 0, -1000 >>> 0],
    'the Pose compiles with the native keep-coordinate value');
  delete state.runtimeByAssetId[poseScene.assetId];
  await UI.loadScene(rom, state, poseScene);
  const poseRuntime = state.runtimeByAssetId[poseScene.assetId];
  const finalX = poseRuntime.states.at(-1).actors.find(actor => actor.slot === 0).baseX;
  assert(Math.abs(finalX + 54) < 0.01, 'the Actor ends at the Move target, not back at its start: ' + finalX);
  const poseCandidate = { z64: z64.slice(), layout: rom.layout };
  const posePlan = await OB64.cutsceneExport.prepare(rom, poseCandidate);
  OB64.cutsceneExport.apply(poseCandidate, posePlan);
  OB64.cutsceneExport.validateApplied(poseCandidate, posePlan);
  // Duplicating the Actor mid-move rounds its copied position.
  poseView.frame = 3;
  const mover = state.histories[poseScene.storageId].present.actors[0];
  await UI.addActor(rom, state, mover);
  const copy = state.histories[poseScene.storageId].present.actors.find(actor => actor.id !== mover.id);
  assert.strictEqual(copy.initial.x, UI.nativeCoordinate(copy.initial.x));
  assert.strictEqual(Math.round(copy.initial.x * 1000) / 1000, copy.initial.x, 'the duplicate starts at a native coordinate');

  console.log('PASS new-scene sequencing: async cards wait until they finish by default, the Actor walks and the Fade completes before the native exit, a native wait follows each async command, runtime start times are shown, an action cut short by the exit is reported and fixed by one explicit Hold, export accepts it, retail insertions keep their behavior, the camera and fade visibly reach their targets, tied cards keep creation order, the cut-short warning survives Project reload and ROM-only reopen, a reopened native fade is an editable, removable Fade card, and Pose after Move keeps the Actor in place and exports.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
