'use strict';

// Interactive A/B choice in a new scene. A Choice card opens a native choice
// window (two options on archive selector 5); two control-byte queries close
// it and jump to labeled branches; actions set to run after an option play
// only for it, then both branches rejoin. Preview plays option 1 by default
// and option 2 on request (down, then confirm on a later pass).

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
const M = OB64.cutsceneModel;
const B = 0x80000002, H = 0x80000000, L = 0x80000005;
const wordsOf = bytes => Array.from({ length: bytes.length / 4 }, (_, index) =>
  ((bytes[index * 4] << 24) | (bytes[index * 4 + 1] << 16) | (bytes[index * 4 + 2] << 8) | bytes[index * 4 + 3]) >>> 0);

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = UI.initialize(rom);
  await state.authoredReady;
  const scene = await UI.createAuthoredScene(rom, state, 'Choice scene');
  await UI.loadScene(rom, state, scene);
  await UI.addActor(rom, state);
  const view = state.views[scene.sceneId];
  view.selectedActorId = UI.selectedDocument(state).actors[0].id;
  view.frame = 0;
  const archiveBefore = OB64.cutsceneAuthoring.readDialogue(z64, 5);

  // 1. Choice, two Moves and a Hold; the Moves run after options 1 and 2.
  await UI.addChoice(rom, state);
  await UI.addMove(rom, state);
  await UI.addMove(rom, state);
  await UI.addHold(rom, state);
  const clips = () => UI.selectedDocument(state).tracks.flatMap(track => track.clips).filter(clip => !clip.source.nodeId);
  const choice = clips().find(clip => clip.kind === 'dialogue');
  assert(choice && choice.payload.choice && choice.capability === 'native', 'Choice is a native card');
  const moves = clips().filter(clip => clip.kind === 'movement');
  await UI.executeEdit(rom, state, 'Set up branches', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => {
      if (clip.id === choice.id) clip.payload = Object.assign({}, clip.payload, { choice: { option1: 'Fight', option2: 'Flee' } });
      if (clip.id === moves[0].id) clip.payload = Object.assign({}, clip.payload, { to: Object.assign({}, clip.payload.to, { x: -54 }), choiceBranch: { choiceClipId: choice.id, option: 1 } });
      if (clip.id === moves[1].id) clip.payload = Object.assign({}, clip.payload, { to: Object.assign({}, clip.payload.to, { x: 16 }), choiceBranch: { choiceClipId: choice.id, option: 2 } });
    }));
  });
  const document = UI.selectedDocument(state);
  assert.strictEqual(document.exportRequirements.capability, 'native', JSON.stringify(document.exportRequirements.reasons));

  // 2. The compiled stream is the native choice pattern.
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], document);
  const words = wordsOf(compiled.decodedBytes);
  const at = words.indexOf(0xBF);
  const windowId = words[at + 1];
  const after = words.slice(at + 14, at + 14 + 23);
  const labelA = after[11], labelB = after[20];
  assert.deepStrictEqual(after, [0, B, H, 0x56, 0x25, 0, 1, 3, 0x05, windowId, 0x59, labelA, B,
    0x25, 0, 255, 3, 0x05, windowId, 0x59, labelB, B, H], 'queries close the window and jump to the branches');
  const branchA = words.indexOf(L, at + 37);
  assert.deepStrictEqual(words.slice(branchA, branchA + 6), [L, labelA, 0x10, 0, 1, windowId], 'branch 1 waits for the window release');
  const moveAChange = compiled.changes.find(change => change.clipId === moves[0].id);
  const moveBChange = compiled.changes.find(change => change.clipId === moves[1].id);
  assert(moveAChange.startWord > branchA && moveAChange.choiceBranch === 1);
  const branchB = words.lastIndexOf(labelB) - 1;
  assert(words[branchB] === L && moveBChange.startWord > branchB && moveBChange.choiceBranch === 2);
  const joinAt = words.lastIndexOf(L);
  assert(joinAt > moveBChange.startWord, 'the join label follows both branches');
  const joinLabel = words[joinAt + 1];
  assert.strictEqual(words.filter((word, index) => word === 0x59 && words[index + 1] === joinLabel).length, 2, 'both branches jump to the join');
  assert(new Set([labelA, labelB, joinLabel]).size === 3 && labelA >= 0x7000, 'three distinct labels');
  assert.deepStrictEqual(words.slice(-5), OB64.cutsceneAuthored.CLASS5_EXIT_TAIL);

  // 3. Preview plays option 1 by default and option 2 on request.
  async function finalX() {
    delete state.runtimeByAssetId[scene.assetId];
    await UI.loadScene(rom, state, scene);
    const runtime = state.runtimeByAssetId[scene.assetId];
    assert(runtime, JSON.stringify(state.sourceErrors));
    assert.strictEqual(runtime.states.at(-1).runtime.terminalReason, 'terminal-state-release');
    return runtime.states.at(-1).actors.find(actor => actor.slot === 0).baseX;
  }
  const x1 = await finalX();
  assert(Math.abs(x1 + 54) < 0.01, 'option 1 runs its Move: ' + x1);
  view.previewChoices = { [choice.id]: 2 };
  const x2 = await finalX();
  assert(Math.abs(x2 - 16) < 0.01, 'option 2 runs its Move: ' + x2);
  view.previewChoices = {};

  // 4. Export writes the choice text in the retail layout; reopen plays it.
  const candidate = { z64: z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const archiveAfter = OB64.cutsceneAuthoring.readDialogue(candidate.z64, 5);
  assert.strictEqual(archiveAfter.entries[archiveBefore.entries.length].rawText,
    '@S0@%0@n    1. {T05}Fight@n    2. {T05}Flee@1@w3001@c@n@2@w3255@c@n@#');
  const reopenRom = { z64: candidate.z64.slice(), layout: rom.layout, archives: OB64.findArchives(candidate.z64) };
  const reopened = UI.initialize(reopenRom);
  await reopened.authoredReady;
  const reopenedScene = reopened.catalog.getScene(scene.sceneId);
  reopened.selectedSceneId = reopenedScene.sceneId;
  await UI.loadScene(reopenRom, reopened, reopenedScene);
  const reopenedRuntime = reopened.runtimeByAssetId[reopenedScene.assetId];
  assert(Math.abs(reopenedRuntime.states.at(-1).actors.find(actor => actor.slot === 0).baseX + 54) < 0.01, 'the reopened choice plays option 1');

  // 4b. After ROM-only reopen the choice and its branches come back as a
  // choice: options from its text, branch cards tagged, both options play,
  // and option labels, branch actions and new branch cards can be edited.
  const reopenedDocument = () => reopened.histories[reopenedScene.storageId].present;
  const nativeChoice = reopenedDocument().tracks.flatMap(track => track.clips).find(clip => clip.payload.nativeChoice);
  assert(nativeChoice, 'the reopened window is a choice card');
  assert.deepStrictEqual(OB64.cutsceneAuthoring.choiceOptionsFromRawText(nativeChoice.payload.rawText), { option1: 'Fight', option2: 'Flee' });
  const branchMoves = reopenedDocument().tracks.flatMap(track => track.clips).filter(clip => clip.kind === 'movement' && clip.payload.nativeChoiceBranch);
  assert.deepStrictEqual(branchMoves.map(clip => clip.payload.nativeChoiceBranch.option).sort(), [1, 2], 'both branch Moves are tagged');
  const reopenedView = UI.viewFor(reopened, reopenedScene.sceneId);
  reopenedView.previewChoices = { [nativeChoice.id]: 2 };
  delete reopened.runtimeByAssetId[reopenedScene.assetId];
  await UI.loadScene(reopenRom, reopened, reopenedScene);
  assert(Math.abs(reopened.runtimeByAssetId[reopenedScene.assetId].states.at(-1).actors.find(actor => actor.slot === 0).baseX - 16) < 0.01,
    'the reopened choice plays option 2 on request');
  reopenedView.previewChoices = {};
  const moveA = branchMoves.find(clip => clip.payload.nativeChoiceBranch.option === 1);
  await UI.executeEdit(reopenRom, reopened, 'Edit reopened choice', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => {
      if (clip.id === nativeChoice.id) clip.payload.rawText = OB64.cutsceneAuthoring.choiceRawText({ option1: 'Charge', option2: 'Flee' });
      if (clip.id === moveA.id) clip.payload = Object.assign({}, clip.payload, { to: Object.assign({}, clip.payload.to, { x: -40 }) });
    }));
  });
  reopened.views[reopenedScene.sceneId].frame = 0;
  await UI.addHold(reopenRom, reopened);
  const addedHold = reopenedDocument().tracks.flatMap(track => track.clips).find(clip => clip.kind === 'wait' && !clip.source.nodeId);
  await UI.executeEdit(reopenRom, reopened, 'Hold in option 2', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => {
      if (clip.id === addedHold.id) clip.source.insertBeforeNodeId = nativeChoice.payload.nativeChoice.branchEndNodeIds[1];
    }));
  });
  assert.strictEqual(reopenedDocument().exportRequirements.capability, 'native', JSON.stringify(reopenedDocument().exportRequirements.reasons));
  const reopenedCompiled = OB64.cutsceneCodec.compileSceneDocument(reopenedScene, reopened.sourceByAssetId[reopenedScene.assetId], reopenedDocument());
  const holdChange = reopenedCompiled.changes.find(change => change.clipId === addedHold.id);
  const reopenedWords = wordsOf(reopenedCompiled.decodedBytes);
  assert(reopenedWords[holdChange.startWord + 8] === 0x59, 'the new Hold sits just before option 2 rejoins');
  const second = { z64: reopenRom.z64.slice(), layout: rom.layout };
  const secondPlan = await OB64.cutsceneExport.prepare(reopenRom, second);
  OB64.cutsceneExport.apply(second, secondPlan);
  OB64.cutsceneExport.validateApplied(second, secondPlan);
  assert.strictEqual(OB64.cutsceneAuthoring.readDialogue(second.z64, 5).entries[archiveBefore.entries.length].rawText,
    '@S0@%0@n    1. {T05}Charge@n    2. {T05}Flee@1@w3001@c@n@2@w3255@c@n@#', 'the option label edit exports');
  const thirdRom = { z64: second.z64.slice(), layout: rom.layout, archives: OB64.findArchives(second.z64) };
  const third = UI.initialize(thirdRom);
  await third.authoredReady;
  const thirdScene = third.catalog.getScene(scene.sceneId);
  third.selectedSceneId = thirdScene.sceneId;
  await UI.loadScene(thirdRom, third, thirdScene);
  assert(Math.abs(third.runtimeByAssetId[thirdScene.assetId].states.at(-1).actors.find(actor => actor.slot === 0).baseX + 40) < 0.01,
    'the edited branch action plays after the next reopen');

  // 4c. Two choices preview independently: each takes its own option in order.
  const twoScene = await UI.createAuthoredScene(rom, state, 'Two choices');
  await UI.loadScene(rom, state, twoScene);
  await UI.addActor(rom, state);
  const twoView = state.views[twoScene.sceneId];
  twoView.selectedActorId = UI.selectedDocument(state).actors[0].id;
  twoView.frame = 0;
  await UI.addChoice(rom, state); await UI.addMove(rom, state); await UI.addMove(rom, state);
  await UI.addChoice(rom, state); await UI.addMove(rom, state); await UI.addMove(rom, state);
  const twoClips = UI.selectedDocument(state).tracks.flatMap(track => track.clips).filter(clip => !clip.source.nodeId);
  const [choiceOne, choiceTwo] = twoClips.filter(clip => clip.kind === 'dialogue').sort((a, b) => a.source.sequence - b.source.sequence);
  const twoMoves = twoClips.filter(clip => clip.kind === 'movement').sort((a, b) => a.source.sequence - b.source.sequence);
  const targets = [[-54, choiceOne, 1], [16, choiceOne, 2], [40, choiceTwo, 1], [-30, choiceTwo, 2]];
  await UI.executeEdit(rom, state, 'Two choice branches', next => {
    next.tracks.forEach(track => track.clips.forEach(clip => {
      const index = twoMoves.findIndex(move => move.id === clip.id);
      if (index < 0) return;
      const [x, owner, option] = targets[index];
      // Default speeds: each Move waits for the Actor to arrive (native 0x11
      // query), however far it walks after the earlier choice.
      clip.payload = Object.assign({}, clip.payload, { to: Object.assign({}, clip.payload.to, { x }),
        choiceBranch: { choiceClipId: owner.id, option } });
    }));
  });
  async function run(options) {
    twoView.previewChoices = options;
    delete state.runtimeByAssetId[twoScene.assetId];
    await UI.loadScene(rom, state, twoScene);
    return state.runtimeByAssetId[twoScene.assetId].states.map(frame => frame.actors.find(actor => actor.slot === 0).baseX);
  }
  const firstThenSecond = await run({ [choiceOne.id]: 1, [choiceTwo.id]: 2 });
  assert(firstThenSecond.some(x => Math.abs(x + 54) < 0.01) && Math.abs(firstThenSecond.at(-1) + 30) < 0.01, 'option 1 then option 2');
  const secondThenFirst = await run({ [choiceOne.id]: 2, [choiceTwo.id]: 1 });
  assert(secondThenFirst.some(x => Math.abs(x - 16) < 0.01) && Math.abs(secondThenFirst.at(-1) - 40) < 0.01, 'option 2 then option 1');

  // 5. A branch action must follow its choice.
  const misplaced = M.cloneSceneDocument(document);
  misplaced.tracks.forEach(track => track.clips.forEach(clip => {
    if (clip.id === moves[0].id) clip.source = Object.assign({}, clip.source, { sequence: -1 });
  }));
  assert.throws(() => OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], misplaced), /must follow its choice/);

  console.log('PASS A/B choice: a native choice window with two control-byte queries and labeled branches, actions that run after one option only, both options in preview, the retail-layout choice text on export, playback after reopen with the choice, its options and branches restored and editable, independent preview options for several choices, and branch order enforced.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
